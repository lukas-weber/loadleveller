#include "dump.h"
#include <typeinfo>

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

iodump::group_members::group_members(hid_t group)
	: group_{group} {
}

iodump::group_members::iterator iodump::group_members::begin() {
	return iodump::group_members::iterator{group_, 0};
}

iodump::group_members::iterator iodump::group_members::end() {
	H5G_info_t info;
	herr_t status = H5Gget_info(group_, &info);
	if(status < 0)
		throw iodump_exception{"H5Gget_info"};
	
	return iodump::group_members::iterator{group_, info.nlinks};
}

iodump::group_members::iterator::iterator(hid_t group, uint64_t idx)
	: group_(group), idx_(idx) {
}

std::string iodump::group_members::iterator::operator*() {
	ssize_t name_size = H5Lget_name_by_idx(group_, "/", H5_INDEX_NAME, H5_ITER_INC, idx_, nullptr, 0, H5P_DEFAULT);
	if(name_size < 0)
		throw iodump_exception{"H5Lget_name_by_idx"};

	std::vector<char> buf(name_size);
	name_size = H5Lget_name_by_idx(group_, "/", H5_INDEX_NAME, H5_ITER_INC, idx_, buf.data(), buf.size(), H5P_DEFAULT);
	if(name_size < 0)
		throw iodump_exception{"H5Lget_name_by_idx"};

	return std::string(buf.data());
}

iodump::group_members::iterator iodump::group_members::iterator::operator++() {
	idx_++;
	return *this;
}
	
bool iodump::group_members::iterator::operator!=(const iterator& b) {
	return idx_ != b.idx_;
}

iodump::group_members iodump::list() {
	return group_members{group_};
}
	
iodump iodump::create(const std::string& filename) {
	hid_t file = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

	if(file < 0)
		throw iodump_exception{"H5Fcreate"};

	return iodump{file};
}

iodump iodump::open_readonly(const std::string& filename) {
	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if(file < 0)
		throw iodump_exception{"H5Fopen"};
	return iodump{file};
}

iodump iodump::open_readwrite(const std::string& filename) {
	hid_t file = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
	if(file < 0)
		throw iodump_exception{"H5Fopen"};
	return iodump{file};
}

iodump::iodump(hid_t h5_file) : h5_file_{h5_file} {
	if(not filter_available(compression_filter_))
		throw iodump_exception{"H5Filter not available."};

	group_ = H5Gopen(h5_file_, "/", H5P_DEFAULT);
	if(group_ < 0)
		throw iodump_exception{"H5Gopen"};
}

iodump::~iodump() {
	H5Gclose(group_);
	H5Fclose(h5_file_);
}

void iodump::change_group(const std::string& path) {
	hid_t tmp = H5Gopen(group_, path.c_str(), H5P_DEFAULT);
	if(tmp < 0)
		throw iodump_exception{"H5Gopen"};
	herr_t status = H5Gclose(group_);
	if(status < 0)
		throw iodump_exception{"H5Gclose"};
	group_ = tmp;
}



size_t iodump::get_extent(const std::string& name) {
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
