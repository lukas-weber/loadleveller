#pragma once

#include <fmt/format.h>
#include <hdf5.h>
#include <string>
#include <cassert>
#include <vector>

namespace loadl {

class iodump_exception : public std::exception {
private:
	std::string message_;

public:
	iodump_exception(const std::string &filename, const std::string &msg);

	const char *what() const noexcept override;
};

// This thing is a wrapper around an HDF5 file which can do both reading and
// writing depending on how you open it. If you write to a read only file,
// there will be an error probably.
class iodump {
private:
	// helper class to make sure those pesky HDF5 hid_t handles are always closed
	class h5_handle {
	public:
		h5_handle(hid_t handle, herr_t (*closer)(hid_t));
		~h5_handle();
		h5_handle(h5_handle &) = delete;
		h5_handle(h5_handle &&) noexcept;
		hid_t operator*();

	private:
		herr_t (*closer_)(hid_t);
		hid_t handle_;
	};

	iodump(std::string filename, hid_t h5_file);

	const std::string filename_;
	const hid_t h5_file_;

	// TODO: make these variable if necessary
	static const H5Z_filter_t compression_filter_ = H5Z_FILTER_DEFLATE;
	static const size_t chunk_size_ = 1000;

	template<typename T>
	constexpr static hid_t h5_datatype();

public:
	// Wrapper around the concept of a HDF5 group.
	// You can list over the group elements by using the iterators.
	class group {
	public:
		struct iterator {
			std::string operator*();
			iterator operator++();
			bool operator!=(const iterator &b);

			const hid_t group_;
			const std::string &filename_;
			uint64_t idx_;
		};

		group(hid_t parent, const std::string &filename, const std::string &path);
		group(const group &) =
		    delete; // did you know this is a thing? Very handy in preventing errors.
		~group();
		iterator begin() const;
		iterator end() const;

		template<class T>
		void write(const std::string &name, const std::vector<T> &data) const;
		template<class T>
		void write(const std::string &name,
		           const T &value) const; // this is meant for atomic datatypes like int and double

		// insert_back inserts data at the end of the dataset given by name, extending it if
		// necessary. This only works in read/write mode.
		template<class T>
		void insert_back(const std::string &name, const std::vector<T> &data) const;

		template<class T>
		void read(const std::string &name, std::vector<T> &data) const;
		template<class T>
		void read(const std::string &name, T &value) const;

		size_t get_extent(const std::string &name) const;

		group open_group(const std::string &path) const; // this works like the cd command

		bool exists(
		    const std::string &path) const; // checks whether an object in the dump file exists
	private:
		hid_t group_;
		const std::string &filename_;

		// chunk_size == 0 means contiguous storage
		// if the dataset already exists, we try to overwrite it. However it must have the same
		// extent for that to work.
		iodump::h5_handle create_dataset(const std::string &name, hid_t datatype, hsize_t size,
		                                 hsize_t chunk_size, H5Z_filter_t compression_filter,
		                                 bool unlimited) const;
	};

	// delete what was there and create a new file for writing
	static iodump create(const std::string &filename);

	static iodump open_readonly(const std::string &filename);
	static iodump open_readwrite(const std::string &filename);

	group get_root();

	// TODO: once the intel compiler can do guaranteed copy elision,
	// please uncomment this line! and be careful about bugs!
	// iodump(iodump &) = delete;
	~iodump();

	friend class group;
};

template<typename T>
constexpr hid_t iodump::h5_datatype() {
	if(typeid(T) == typeid(char))
		return H5T_NATIVE_CHAR;
	if(typeid(T) == typeid(signed char))
		return H5T_NATIVE_SCHAR;
	if(typeid(T) == typeid(int))
		return H5T_NATIVE_INT;
	if(typeid(T) == typeid(short))
		return H5T_NATIVE_SHORT;
	if(typeid(T) == typeid(long))
		return H5T_NATIVE_LONG;
	if(typeid(T) == typeid(long long))
		return H5T_NATIVE_LLONG;
	if(typeid(T) == typeid(unsigned char))
		return H5T_NATIVE_UCHAR;
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

	throw std::runtime_error{fmt::format("unsupported datatype: {}", typeid(T).name())};
	// If you run into this error, you probably tried to write a non-primitive datatype
	// to a dump file. See the other classes’s checkpointing functions for an example of
	// what to do.
	// ... or it is a native datatype I forgot to add. Then add it.
}

template<class T>
void iodump::group::write(const std::string &name, const std::vector<T> &data) const {
	int chunk_size = 0;
	H5Z_filter_t compression_filter = 0;

	// no compression and chunking unless dataset is big enough
	if(data.size() >= chunk_size_) {
		chunk_size = iodump::chunk_size_;
		compression_filter = iodump::compression_filter_;
	}

	h5_handle dataset{
	    create_dataset(name, h5_datatype<T>(), data.size(), chunk_size, compression_filter, false)};
	herr_t status =
	    H5Dwrite(*dataset, h5_datatype<T>(), H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{filename_, "H5Dwrite"};
}

template<class T>
void iodump::group::write(const std::string &name, const T &value) const {
	// I hope nobody copies a lot of small values...
	write(name, std::vector<T>{value});
}

template<>
inline void iodump::group::write(const std::string &name, const std::string &value) const {
	write(name, std::vector<char>{value.begin(), value.end()});
}

template<class T>
void iodump::group::insert_back(const std::string &name, const std::vector<T> &data) const {
	// If the dataset does not exist, we create a new unlimited one with 0 extent.
	if(!exists(name)) {
		create_dataset(name, h5_datatype<T>(), 0, chunk_size_, compression_filter_, true);
	}

	h5_handle dataset{H5Dopen2(group_, name.c_str(), H5P_DEFAULT), H5Dclose};

	hsize_t mem_size = data.size();
	h5_handle memspace{H5Screate_simple(1, &mem_size, nullptr), H5Sclose};
	int size;
	herr_t status;

	{ // limit the scope of the dataspace handle
		h5_handle dataspace{H5Dget_space(*dataset), H5Sclose};

		size = H5Sget_simple_extent_npoints(*dataspace);
		if(size < 0) {
			throw iodump_exception{filename_, "H5Sget_simple_extent_npoints"};
		}

		if(data.size() > 0) {
			hsize_t new_size = size + data.size();
			status = H5Dset_extent(*dataset, &new_size);
			if(status < 0) {
				throw iodump_exception{filename_, "H5Pset_extent"};
			}
		}
	} // because it will be reopened after this

	h5_handle dataspace{H5Dget_space(*dataset), H5Sclose};

	// select the hyperslab of the extended area
	hsize_t pos = size;
	hsize_t extent = data.size();
	status = H5Sselect_hyperslab(*dataspace, H5S_SELECT_SET, &pos, nullptr, &extent, nullptr);
	if(status < 0)
		throw iodump_exception{filename_, "H5Sselect_hyperslap"};

	status = H5Dwrite(*dataset, h5_datatype<T>(), *memspace, *dataspace, H5P_DEFAULT, data.data());
	if(status < 0)
		throw iodump_exception{filename_, "H5Dwrite"};
}

template<class T>
void iodump::group::read(const std::string &name, std::vector<T> &data) const {
	hid_t dset = H5Dopen2(group_, name.c_str(), H5P_DEFAULT);
	if(dset < 0)
		throw iodump_exception{filename_, fmt::format("H5Dopen2({})", name)};

	h5_handle dataset{dset, H5Dclose};
	h5_handle dataspace{H5Dget_space(*dataset), H5Sclose};

	int size = H5Sget_simple_extent_npoints(*dataspace); // rank > 1 will get flattened when loaded.
	if(size < 0)
		throw iodump_exception{filename_, "H5Sget_simple_extent_npoints"};
	data.resize(size);

	if(size == 0) { // handle empty dataset correctly
		return;
	}

	herr_t status =
	    H5Dread(*dataset, h5_datatype<T>(), H5S_ALL, H5P_DEFAULT, H5P_DEFAULT, data.data());
	if(status < 0) {
		throw iodump_exception{filename_, fmt::format("H5Dread({})", name)};
	}
}

template<>
inline void iodump::group::read(const std::string &name, std::string &value) const {
	std::vector<char> buf;
	read(name, buf);
	value = std::string{buf.begin(), buf.end()};
}

template<class T>
void iodump::group::read(const std::string &name, T &value) const {
	std::vector<T> buf;
	read(name, buf);
	assert(buf.size() == 1);
	value = buf.at(0);
}

}
