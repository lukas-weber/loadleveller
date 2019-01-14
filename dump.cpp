#include "dump.h"
#include <typeinfo>
#include <fmt/format.h>

// Maybe I should just use a C++ wrapper for HDF5 to avoid all these terrible error checking ifs but somehow I didn’t.

template<typename T>
static hid_t h5_datatype() {
	if(typeid(T) == typeid(int))
		return H5T_NATIVE_INT;
	if(typeid(T) == typeid(short))
		return H5T_NATIVE_SHORT;
	if(typeid(T) == typeid(long))
		return H5T_NATIVE_LONG;
	if(typeid(T) == typeid(long long))
		return H5T_NATIVE_LLONG;
	if(typeid(T) == typeid(unsigned int))
		return H5T_NATIVE_UINT;
	if(typeid(T) == typeid(unsigned short))
		return H5T_NATIVE_USHORT;
	if(typeid(T) == typeid(unsigned long))
		return H5T_NATIVE_ULONG;
	if(typeid(T) == typeid(unsigned long long))
		return H5T_NATIVE_ULLONG;
	if(typeid(T) == typeid(float))
		return H5T_NATIVE_FLOAT;
	if(typeid(T) == typeid(double))
		return H5T_NATIVE_DOUBLE;

	throw iodump_exception{fmt::format("unsupported datatype: {}",typeid(T).name())};
}

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

// chunk_size == 0 means contiguous storage
template<class T>
static hid_t create_dataset(hid_t group, const std::string& name, int size, int chunk_size, H5Z_filter_t compression_filter, bool unlimited) {
	herr_t status;

	hid_t dataspace;
	if(not unlimited)
		dataspace = H5Screate_simple(1, {size}, nullptr);
	else
		dataspace = H5Screate_simple(1, {size}, {H5S_UNLIMITED});

	hid_t plist = H5Pcreate(H5P_DATASET_CREATE);

	if(chunk_size > 1) { // do not use compression on small datasets
		status = H5Pset_chunk(plist, 1, {chunk_size});
		if(status < 0)
			throw iodump_exception{"H5Pset_chunk"};
		
		if(compression_filter== H5Z_FILTER_DEFLATE) {
			status = H5Pset_deflate(plist, 6);
			if(status < 0)
				throw iodump_exception{"H5Pset_deflate"};
		}
	}

	hid_t dataset = H5Dcreate2(group, name.c_str(), h5_datatype<T>(), dataspace, H5P_DEFAULT, plist, H5P_DEFAULT);
	if(dataset < 0)
		throw iodump_exception{"H5Dcreate2"};

	status = H5Pclose(plist);
	if(status < 0)
		throw iodump_exception{"H5Pclose"};
	status = H5Sclose(dataspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};
	return dataset;
}
	

template<class T>
void iodump::write(const std::string& name, const std::vector<T>& data) {
	int chunk_size = 0;
	H5Z_filter_t compression_filter= 0;
	
	// no compression and chunking unless dataset is big enough
	if(data.size() >= chunk_size_) { 
		chunk_size = chunk_size_;
		compression_filter= compression_filter_;
	}

	hid_t dataset = create_dataset<T>(group_, name, data.size(), chunk_size, compression_filter, false);
	herr_t status = H5Dwrite(dataset, h5_datatype<T>(), H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{"H5Dwrite"};
	status = H5Dclose(dataset);
	if(status < 0)
		throw iodump_exception{"H5Dclose"};
}

template<class T>
void iodump::write(const std::string& name, T value) {
	// I hope nobody copies a lot of small values...
	write(name, std::vector<T>{value});
}

template <class T>
void iodump::insert_back(const std::string& name, const std::vector<T>& data) {
	// If the dataset does not exist, we create a new unlimited one with 0 extent.
	htri_t exists = H5Lexists(group_, name.c_str(), H5P_DEFAULT);
	if(exists == 0) {
		create_dataset<T>(group_, name, 0, chunk_size_, compression_filter_, true);
	} else if(exists < 0) {
		throw iodump_exception{"H5Lexists"};
	}

	hid_t dataset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
	if(dataset < 0)
		throw iodump_exception{"H5Dopen2"};
		
	hid_t dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	int size = H5Sget_simple_extent_npoints(dataspace);
	if(size < 0)
		throw iodump_exception{"H5Sget_simple_extent_npoints"};

	herr_t status = H5Sclose(dataspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};

	status = H5Dset_extent(dataset, size + data.size());
	if(status < 0)
		throw iodump_exception{"H5Pset_extent"};
	
	dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	// select the hyperslab of the extended area
	status = H5Sselect_hyperslap(dataspace, H5S_SELECT_SET, {size}, nullptr, {data.size()}, nullptr);
	if(status < 0)
		throw iodump_exception{"H5Sselect_hyperslap"};

	status = H5Dwrite(dataset, h5_datatype<T>(), H5S_ALL, dataspace, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{"H5Dwrite"};
	status = H5Sclose(dataspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};
	status = H5Dclose(dataset);
	if(status < 0)
		throw iodump_exception{"H5Dclose"};
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

template<class T>
void iodump::read(const std::string& name, std::vector<T>& data) {
	hid_t dataset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
	if(dataset < 0)
		throw iodump_exception{"H5Dopen2"};

	hid_t dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	int size = H5Sget_simple_extent_npoints(dataspace); // rank > 1 will get flattened when loaded.
	if(size < 0)
		throw iodump_exception{"H5Sget_simple_extent_npoints"};
	data.resize(size);
	
	herr_t status = H5Dread(dataset, h5_datatype<T>(), H5S_ALL, H5P_DEFAULT, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{"H5Dread"};
	status = H5Sclose(dataspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};
	status = H5Dclose(dataset);
	if(status < 0)
		throw iodump_exception{"H5Dclose"};
}

template<class T>
void iodump::read(const std::string& name, T& value) {
	std::vector<T> buf;
	read(name, buf);
	value = buf.at(0);
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
