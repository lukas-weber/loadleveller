#include "dump.h"
#include <typeinfo>
#include <unistd.h>
#include <sstream>
#include <iostream>

static bool filter_available(H5Z_filter_t filter) {
	htri_t avail = H5Zfilter_avail(filter);
	if(not avail) {
		return false;
        }

	unsigned int filter_info;
	herr_t status = H5Zget_filter_info(filter, &filter_info);
	if(status < 0 or not (filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) or not (filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED)) {
		return false;
	}
	
	return true;
}

static herr_t H5Ewalk_cb(unsigned int n, const H5E_error2_t *err_desc, void *client_data) {
	std::stringstream& s = *reinterpret_cast<std::stringstream *>(client_data);

	char *min_str = H5Eget_minor(err_desc->min_num);
	char *maj_str = H5Eget_major(err_desc->maj_num);
	s << fmt::format("#{}: {}:{} in {}(): {}\n", n, err_desc->file_name, err_desc->line, err_desc->func_name, err_desc->desc);
	s << fmt::format("    {}: {}\n", err_desc->maj_num, maj_str);
	s << fmt::format("    {}: {}\n\n", err_desc->min_num, min_str);

	free(min_str);
	free(maj_str);

	return 0;
}

iodump_exception::iodump_exception(const std::string& message)  {
	std::stringstream s;
	H5Ewalk(H5E_DEFAULT, H5E_WALK_DOWNWARD, H5Ewalk_cb, &s);

	s << "Error triggered: " << message;
	message_ = s.str();
}

const char *iodump_exception::what() const noexcept {
	return message_.c_str();
}

iodump::group::group(hid_t parent, const std::string& path) {
	group_ = H5Gopen(parent, path.c_str(), H5P_DEFAULT);
	if(group_ < 0) {
		group_ = H5Gcreate2(parent, path.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if(group_ < 0) {
			throw iodump_exception{"H5Gcreate"};
		}
	}
}

iodump::group::~group() {
	if(group_ < 0)
		return;
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
	H5G_info_t info;
	herr_t status = H5Gget_info(group_, &info);
	if(status < 0)
		throw iodump_exception{"H5Gget_info"};
	
	return iodump::group::iterator{group_, info.nlinks};
}

iodump::group::iterator::iterator(hid_t group, uint64_t idx)
	: group_(group), idx_(idx) {
}

std::string iodump::group::iterator::operator*() {
	ssize_t name_size = H5Lget_name_by_idx(group_, "/", H5_INDEX_NAME, H5_ITER_INC, idx_, nullptr, 0, H5P_DEFAULT);
	if(name_size < 0)
		throw iodump_exception{"H5Lget_name_by_idx"};

	std::vector<char> buf(name_size+1);
	name_size = H5Lget_name_by_idx(group_, "/", H5_INDEX_NAME, H5_ITER_INC, idx_, buf.data(), buf.size(), H5P_DEFAULT);
	if(name_size < 0)
		throw iodump_exception{"H5Lget_name_by_idx"};

	return std::string(buf.data());
}

iodump::group::iterator iodump::group::iterator::operator++() {
	idx_++;
	return *this;
}
	
bool iodump::group::iterator::operator!=(const iterator& b) {
	return idx_ != b.idx_;
}

iodump iodump::create(const std::string& filename) {
	H5Eset_auto(H5E_DEFAULT, NULL, NULL);
	hid_t file = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

	if(file < 0)
		throw iodump_exception{"H5Fcreate"};

	return iodump{file};
}

iodump iodump::open_readonly(const std::string& filename) {
	H5Eset_auto(H5E_DEFAULT, NULL, NULL);
	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if(file < 0)
		throw iodump_exception{"H5Fopen"};
	return iodump{file};
}

iodump iodump::open_readwrite(const std::string& filename) {
	H5Eset_auto(H5E_DEFAULT, NULL, NULL);
	if(access(filename.c_str(), R_OK) != F_OK) {
		create(filename);
	}

	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
	if(file < 0)
		throw iodump_exception{"H5Fopen"};
	return iodump{file};
}

iodump::iodump(hid_t h5_file) : h5_file_{h5_file} {
	if(compression_filter_ != 0 and not filter_available(compression_filter_))
		throw iodump_exception{"H5Filter not available."};
}

iodump::~iodump() {
	H5Fclose(h5_file_);
}

iodump::group iodump::get_root() {
	return group{h5_file_, "/"};
}

hid_t iodump::group::create_dataset(const std::string& name, hid_t datatype, hsize_t size, hsize_t chunk_size, H5Z_filter_t compression_filter, bool unlimited) const {
	herr_t status;

	hid_t dataset;
	if(exists(name)) {
		dataset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
		if(dataset < 0)
			throw iodump_exception{"H5Dopen2"};
		hid_t dataspace = H5Dget_space(dataset);
		if(dataspace < 0)
			throw iodump_exception{"H5Dget_space"};

		hsize_t oldsize = H5Sget_simple_extent_npoints(dataspace);
		if(oldsize < 0)
			throw iodump_exception{"H5Sget_simple_extent_npoints"};
		if(oldsize != size)
			throw std::runtime_error{"iodump: tried to write into an existing dataset with different dimensions!"};
	} else {
		hid_t dataspace;
		if(not unlimited) {
			dataspace = H5Screate_simple(1, &size, nullptr);
		} else {
			hsize_t maxdim = H5S_UNLIMITED;
			dataspace = H5Screate_simple(1, &size, &maxdim);
		}

		hid_t plist = H5Pcreate(H5P_DATASET_CREATE);

		if(chunk_size > 1) { // do not use compression on small datasets
			status = H5Pset_chunk(plist, 1, &chunk_size);
			if(status < 0)
				throw iodump_exception{"H5Pset_chunk"};
			
			if(compression_filter == H5Z_FILTER_DEFLATE) {
				status = H5Pset_deflate(plist, 6);
				if(status < 0)
					throw iodump_exception{"H5Pset_deflate"};
			}
		}

		dataset = H5Dcreate2(group_, name.c_str(), datatype, dataspace, H5P_DEFAULT, plist, H5P_DEFAULT);
		if(dataset < 0)
			throw iodump_exception{"H5Dcreate2"};
		status = H5Pclose(plist);
		if(status < 0)
			throw iodump_exception{"H5Pclose"};
		status = H5Sclose(dataspace);
		if(status < 0)
			throw iodump_exception{"H5Sclose"};
	}
	

	return dataset;
}

iodump::group iodump::group::open_group(const std::string& path) const {
	return group{group_, path};
}

size_t iodump::group::get_extent(const std::string& name) const {
	hid_t dataset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
	if(dataset < 0)
		throw iodump_exception{"H5Dopen2"};

	hid_t dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	int size = H5Sget_simple_extent_npoints(dataspace); // rank > 1 will get flattened when loaded.
	if(size < 0)
		throw iodump_exception{"H5Sget_simple_extent_npoints"};

	return size;
}

bool iodump::group::exists(const std::string &path) const {
	htri_t exists = H5Lexists(group_, path.c_str(), H5P_DEFAULT);
	if(exists == 0) {
		return false;
	} else if(exists < 0) {
		throw iodump_exception{"H5Lexists"};
	}

	return true;
}
