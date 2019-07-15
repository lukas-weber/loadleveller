#include "dump.h"
#include <iostream>
#include <sstream>
#include <sys/file.h>
#include <typeinfo>
#include <unistd.h>

namespace loadl {

static bool filter_available(H5Z_filter_t filter) {
	htri_t avail = H5Zfilter_avail(filter);
	if(avail == 0) {
		return false;
	}

	unsigned int filter_info;
	herr_t status = H5Zget_filter_info(filter, &filter_info);
	return status >= 0 && (filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) != 0 &&
	       (filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED) != 0;
}

static herr_t H5Ewalk_cb(unsigned int n, const H5E_error2_t *err_desc, void *client_data) {
	std::stringstream &s = *reinterpret_cast<std::stringstream *>(client_data);

	char *min_str = H5Eget_minor(err_desc->min_num);
	char *maj_str = H5Eget_major(err_desc->maj_num);
	s << fmt::format("#{}: {}:{} in {}(): {}\n", n, err_desc->file_name, err_desc->line,
	                 err_desc->func_name, err_desc->desc);
	s << fmt::format("    {}: {}\n", static_cast<int32_t>(err_desc->maj_num), maj_str);
	s << fmt::format("    {}: {}\n\n", static_cast<int32_t>(err_desc->min_num), min_str);

	free(min_str);
	free(maj_str);

	return 0;
}

iodump_exception::iodump_exception(const std::string &message) {
	std::stringstream s;
	H5Ewalk(H5E_DEFAULT, H5E_WALK_DOWNWARD, H5Ewalk_cb, &s);

	s << "Error triggered: " << message;
	message_ = s.str();
}

const char *iodump_exception::what() const noexcept {
	return message_.c_str();
}

iodump::group::group(hid_t parent, const std::string &path) {
	group_ = H5Gopen(parent, path.c_str(), H5P_DEFAULT);
	if(group_ < 0) {
		group_ = H5Gcreate2(parent, path.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if(group_ < 0) {
			throw iodump_exception{"H5Gcreate"};
		}
	}
}

iodump::group::~group() {
	if(group_ < 0) {
		return;
	}
	herr_t status = H5Gclose(group_);
	if(status < 0) {
		std::cerr << iodump_exception{"H5Gclose"}.what();
		std::cerr << group_ << " …something went wrong in the destructor.\n";
		assert(false);
	}
}

iodump::group::iterator iodump::group::begin() const {
	return iodump::group::iterator{group_, 0};
}

iodump::group::iterator iodump::group::end() const {
	H5G_info_t info{};
	herr_t status = H5Gget_info(group_, &info);
	if(status < 0) {
		throw iodump_exception{"H5Gget_info"};
	}

	return iodump::group::iterator{group_, info.nlinks};
}

iodump::group::iterator::iterator(hid_t group, uint64_t idx) : group_(group), idx_(idx) {}

std::string iodump::group::iterator::operator*() {
	ssize_t name_size =
	    H5Lget_name_by_idx(group_, ".", H5_INDEX_NAME, H5_ITER_INC, idx_, nullptr, 0, H5P_DEFAULT);
	if(name_size < 0) {
		throw iodump_exception{"H5Lget_name_by_idx"};
	}

	std::vector<char> buf(name_size + 1);
	name_size = H5Lget_name_by_idx(group_, ".", H5_INDEX_NAME, H5_ITER_INC, idx_, buf.data(),
	                               buf.size(), H5P_DEFAULT);
	if(name_size < 0) {
		throw iodump_exception{"H5Lget_name_by_idx"};
	}

	return std::string(buf.data());
}

iodump::group::iterator iodump::group::iterator::operator++() {
	idx_++;
	return *this;
}

bool iodump::group::iterator::operator!=(const iterator &b) {
	return idx_ != b.idx_;
}

iodump iodump::create(const std::string &filename) {
	H5Eset_auto(H5E_DEFAULT, nullptr, nullptr);
	hid_t file = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

	if(file < 0) {
		throw iodump_exception{"H5Fcreate"};
	}

	return iodump{filename, file};
}

iodump iodump::open_readonly(const std::string &filename) {
	H5Eset_auto(H5E_DEFAULT, nullptr, nullptr);
	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if(file < 0) {
		throw iodump_exception{"H5Fopen"};
	}
	return iodump{filename, file};
}

iodump iodump::open_readwrite(const std::string &filename) {
	H5Eset_auto(H5E_DEFAULT, nullptr, nullptr);
	if(access(filename.c_str(), R_OK) != F_OK) {
		create(filename);
	}

	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
	if(file < 0) {
		throw iodump_exception{"H5Fopen"};
	}
	return iodump{filename, file};
}

iodump::iodump(std::string filename, hid_t h5_file)
    : filename_{std::move(filename)}, h5_file_{h5_file} {
	if(compression_filter_ != 0 && !filter_available(compression_filter_)) {
		throw iodump_exception{"H5Filter not available."};
	}
}

iodump::~iodump() {
	H5Fclose(h5_file_);
}

iodump::group iodump::get_root() {
	return group{h5_file_, "/"};
}

iodump::h5_handle iodump::group::create_dataset(const std::string &name, hid_t datatype,
                                                hsize_t size, hsize_t chunk_size,
                                                H5Z_filter_t compression_filter,
                                                bool unlimited) const {
	herr_t status;

	if(exists(name)) {
		h5_handle dataset{H5Dopen2(group_, name.c_str(), H5P_DEFAULT), H5Dclose};
		h5_handle dataspace{H5Dget_space(*dataset), H5Sclose};

		hssize_t oldsize = H5Sget_simple_extent_npoints(*dataspace);

		if(oldsize < 0) {
			throw iodump_exception{"H5Sget_simple_extent_npoints"};
		}
		if(static_cast<hsize_t>(oldsize) != size) {
			throw std::runtime_error{
			    "iodump: tried to write into an existing dataset with different dimensions!"};
		}

		return dataset;
	} else {
		hid_t dataspace_h;
		if(!unlimited) {
			dataspace_h = H5Screate_simple(1, &size, nullptr);
		} else {
			hsize_t maxdim = H5S_UNLIMITED;
			dataspace_h = H5Screate_simple(1, &size, &maxdim);
		}
		h5_handle dataspace{dataspace_h, H5Sclose};

		h5_handle plist{H5Pcreate(H5P_DATASET_CREATE), H5Pclose};

		if(chunk_size > 1) { // do not use compression on small datasets
			status = H5Pset_chunk(*plist, 1, &chunk_size);
			if(status < 0) {
				throw iodump_exception{"H5Pset_chunk"};
			}

			if(compression_filter == H5Z_FILTER_DEFLATE) {
				status = H5Pset_deflate(*plist, 6);
				if(status < 0) {
					throw iodump_exception{"H5Pset_deflate"};
				}
			}
		}

		return h5_handle{H5Dcreate2(group_, name.c_str(), datatype, *dataspace, H5P_DEFAULT, *plist,
		                            H5P_DEFAULT),
		                 H5Dclose};
	}
}

iodump::group iodump::group::open_group(const std::string &path) const {
	return group{group_, path};
}

size_t iodump::group::get_extent(const std::string &name) const {
	h5_handle dataset{H5Dopen2(group_, name.c_str(), H5P_DEFAULT), H5Dclose};
	h5_handle dataspace{H5Dget_space(*dataset), H5Sclose};

	int size = H5Sget_simple_extent_npoints(*dataspace); // rank > 1 will get flattened when loaded.
	if(size < 0) {
		throw iodump_exception{"H5Sget_simple_extent_npoints"};
	}

	return size;
}

bool iodump::group::exists(const std::string &path) const {
	htri_t exists = H5Lexists(group_, path.c_str(), H5P_DEFAULT);
	if(exists == 0) {
		return false;
	}

	if(exists < 0) {
		throw iodump_exception{"H5Lexists"};
	}

	return true;
}

iodump::h5_handle::h5_handle(hid_t handle, herr_t (*closer)(hid_t))
    : closer_{closer}, handle_{handle} {
	if(handle < 0) {
		throw iodump_exception{"h5_handle"};
	}
}

iodump::h5_handle::h5_handle(h5_handle &&h) noexcept : closer_{h.closer_}, handle_{h.handle_} {
	h.handle_ = -1;
}

iodump::h5_handle::~h5_handle() {
	if(handle_ < 0) {
		return;
	}
	herr_t status = closer_(handle_);
	if(status < 0) {
		std::cerr << iodump_exception{"~h5_handle"}.what() << "\n";
		std::abort();
	}
}

hid_t iodump::h5_handle::operator*() {
	return handle_;
}

bool file_exists(const std::string &path) {
	struct stat buf;
	return stat(path.c_str(), &buf) == 0;
}
}
