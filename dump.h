#pragma once

#include <vector>
#include <hdf5.h>
#include <string>
#include <fmt/format.h>

class iodump_exception : public std::exception {
private:
	std::string message_;
public:
	iodump_exception(const std::string& msg);

	const char *what() const noexcept;
};


// This thing is a wrapper around an HDF5 file which can do both reading and
// writing depending on how you open it. If you write to a read only file,
// there will be an error probably.
class iodump {
public:
	// Wrapper around the concept of a HDF5 group.
	// You can list over the group elements by using the iterators.
	class group {
	public:
		struct iterator {
			iterator(hid_t group, uint64_t idx);
			std::string operator*();
			iterator operator++();
			bool operator!=(const iterator& b);

			const hid_t group_;
			uint64_t idx_;
		};
			
		group(hid_t parent, const std::string& path);
		group(const group&) = delete; // did you know this is a thing? Very handy in preventing errors.
		~group();
		iterator begin() const;
		iterator end() const;
		
		template <class T>
		void write(const std::string& name, const std::vector<T>& data) const;
		template <class T>
		void write(const std::string& name, const T& value) const; // this is meant for atomic datatypes like int and double

		// insert_back inserts data at the end of the dataset given by name, extending it if necessary.
		// This only works in read/write mode.
		template <class T>
		void insert_back(const std::string& name, const std::vector<T>& data) const;
		
		template <class T>
		void read(const std::string& name, std::vector<T>& data) const;
		template <class T>
		void read(const std::string& name, T& value) const;
		
		size_t get_extent(const std::string& name) const;

		group open_group(const std::string &path) const; // this works like the cd command

		bool exists(const std::string &path) const; // checks whether an object in the dump file exists
	private:
		hid_t group_;

		// chunk_size == 0 means contiguous storage
		// if the dataset already exists, we try to overwrite it. However it must have the same extent for that to work.
		hid_t create_dataset(const std::string& name, hid_t datatype, hsize_t size, hsize_t chunk_size, H5Z_filter_t compression_filter, bool unlimited) const;
	};
	
	// delete what was there and create a new file for writing
	static iodump create(const std::string& filename);

	static iodump open_readonly(const std::string& filename);
	static iodump open_readwrite(const std::string& filename);

	group get_root();

	iodump(iodump&) = delete;
	~iodump();
private:
	iodump(hid_t h5_file);
	
	const hid_t h5_file_;

	// TODO: make these variable if necessary
	static const H5Z_filter_t compression_filter_ = H5Z_FILTER_DEFLATE;
	static const size_t chunk_size_ = 1000;
		
	template<typename T>
	constexpr static hid_t h5_datatype();
};

template<typename T>
constexpr hid_t iodump::h5_datatype() {
	if(typeid(T) == typeid(char))
		return H5T_NATIVE_CHAR;
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
		

	throw std::runtime_error{fmt::format("unsupported datatype: {}",typeid(T).name())};
	// If you run into this error, you probably tried to write a non-primitive datatype
	// to a dump file. see runner_task for a minimalistic example of what to do.
	// ... or it is a native datatype I forgot to add. Then add it.
}


	

template<class T>
void iodump::group::write(const std::string& name, const std::vector<T>& data) const {
	int chunk_size = 0;
	H5Z_filter_t compression_filter = 0;
	
	// no compression and chunking unless dataset is big enough
	if(data.size() >= chunk_size_) { 
		chunk_size = iodump::chunk_size_;
		compression_filter= iodump::compression_filter_;
	}

	hid_t dataset = create_dataset(name, h5_datatype<T>(), data.size(), chunk_size, compression_filter, false);
	herr_t status = H5Dwrite(dataset, h5_datatype<T>(), H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{"H5Dwrite"};
	status = H5Dclose(dataset);
	if(status < 0)
		throw iodump_exception{"H5Dclose"};
}

template<class T>
void iodump::group::write(const std::string& name, const T& value) const {
	// I hope nobody copies a lot of small values...
	write(name, std::vector<T>{value});
}

template<>
inline void iodump::group::write(const std::string& name, const std::string& value) const {
	write(name, std::vector<char>{value.begin(), value.end()});
}


template <class T>
void iodump::group::insert_back(const std::string& name, const std::vector<T>& data) const {
	// If the dataset does not exist, we create a new unlimited one with 0 extent.
	if(!exists(name)) {
		create_dataset(name, h5_datatype<T>(), 0, chunk_size_, compression_filter_, true);
	}

	hid_t dataset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
	if(dataset < 0)
		throw iodump_exception{"H5Dopen2"};
		
	hid_t dataspace = H5Dget_space(dataset);
	if(dataspace < 0)
		throw iodump_exception{"H5Dget_space"};

	hsize_t mem_size = data.size();
	hid_t memspace = H5Screate_simple(1, &mem_size, nullptr);

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

	status = H5Dwrite(dataset, h5_datatype<T>(), memspace, dataspace, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{"H5Dwrite"};
	status = H5Sclose(dataspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};
	status = H5Sclose(memspace);
	if(status < 0)
		throw iodump_exception{"H5Sclose"};
	status = H5Dclose(dataset);
	if(status < 0)
		throw iodump_exception{"H5Dclose"};
}

template<class T>
void iodump::group::read(const std::string& name, std::vector<T>& data) const {
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

template<>
inline void iodump::group::read(const std::string& name, std::string& value) const {
	std::vector<char> buf;
	read(name, buf);
	value = std::string{buf.begin(), buf.end()};
}

template<class T>
void iodump::group::read(const std::string& name, T& value) const {
	std::vector<T> buf;
	read(name, buf);
	value = buf.at(0);
}

