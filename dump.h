#pragma once

#include <vector>
#include <hdf5.h>
#include <string>

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
	
	const hid_t h5_file_;
	hid_t group_;

	// TODO: make these variable if necessary
	const H5Z_filter_t compression_filter_ = H5Z_FILTER_DEFLATE;
	const size_t chunk_size_ = 1000;
};
