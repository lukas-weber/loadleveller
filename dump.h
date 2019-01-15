#pragma once

#include <vector>
#include <hdf5.h>
#include <string>
#include <fmt/format.h>

struct iodump_exception : public std::exception {
	std::string message;
	iodump_exception(const std::string& msg) : message(msg) {}
	const char *what() { return (std::string("iodump: ")+message).c_str(); }
};


// This thing is a wrapper around an HDF5 file which can do both reading and
// writing depending on how you open it. If you write to a read only file,
// there will be an error probably.
class iodump {
public:
	// helper for comfortable h5 entry listings.
	class group_members {
	public:
		struct iterator {
			iterator(hid_t group, uint64_t idx);
			std::string operator*();
			iterator operator++();
			bool operator!=(const iterator& b);

			const hid_t group_;
			uint64_t idx_;
		};
			
		group_members(hid_t group);
		iterator begin();
		iterator end();
	private:
		const hid_t group_;
	};
	
	// delete what was there and create a new file for writing
	static iodump create(const std::string& filename);

	static iodump open_readonly(const std::string& filename);
	static iodump open_readwrite(const std::string& filename);

	// returns a range expression over the entries of the current group
	// which you can iterate over using a range-for loop.
	group_members list();

	~iodump();
	template <class T>
	void write(const std::string& name, const std::vector<T>& data);
	template <class T>
	void write(const std::string& name, T value); // this is meant for atomic datatypes like int and double

	// insert_back inserts data at the end of the dataset given by name, extending it if necessary.
	// This only works in read/write mode.
	template <class T>
	void insert_back(const std::string& name, const std::vector<T>& data);
	
	template <class T>
	void read(const std::string& name, std::vector<T>& data);
	template <class T>
	void read(const std::string& name, T& value);
	size_t get_extent(const std::string& name);

	void change_group(const std::string &path); // this works like the cd command
private:
	iodump(hid_t h5_file);

	template<typename T>
	static hid_t h5_datatype();
	template<typename T>
	static hid_t create_dataset(hid_t group, const std::string& name, hsize_t size, hsize_t chunk_size, H5Z_filter_t compression_filter, bool unlimited);
	
	const hid_t h5_file_;
	hid_t group_;

	// TODO: make these variable if necessary
	const H5Z_filter_t compression_filter_ = H5Z_FILTER_DEFLATE;
	const size_t chunk_size_ = 1000;
};


template<typename T>
hid_t iodump::h5_datatype() {
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

// chunk_size == 0 means contiguous storage
template<class T>
hid_t iodump::create_dataset(hid_t group, const std::string& name, hsize_t size, hsize_t chunk_size, H5Z_filter_t compression_filter, bool unlimited) {
	herr_t status;

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

	hsize_t new_size = size + data.size();
	status = H5Dset_extent(dataset, &new_size);
	if(status < 0)
		throw iodump_exception{"H5Pset_extent"};
	
	dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	// select the hyperslab of the extended area
	hsize_t pos = size;
	hsize_t extent = data.size();
	status = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, &pos, nullptr, &extent, nullptr);
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
