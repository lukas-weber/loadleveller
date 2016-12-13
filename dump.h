#ifndef MCL_BINARY_DUMP_H
#define MCL_BINARY_DUMP_H

#ifndef MCL_DUMP_BUFFER
#define MCL_DUMP_BUFFER 0 
#endif

#if MCL_DUMP_BUFFER
#warning MCL_DUMP_BUFFER=1: dump uses buffered writeout
#else
#warning MCL_DUMP_BUFFER=0: no buffering in dump writeout
#endif

#define DUMP_APPEND 0

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "types.h"

class odump
{
	public:
		//changed the endianness of val
		void endian_convert(unsigned char * val, int size) 
		{
			int i = 0;
			int j = size-1;
			while (i<j) {
				std::swap(val[i], val[j]);
				i++;
				j--;
			}
		}

		odump() {
			amode = std::ios::out|std::ios::binary;
			bmode = MCL_DUMP_BUFFER;
			is_open=0;
		}
		odump(int app_flag) { 
			if(app_flag == DUMP_APPEND) amode = std::ios::out|std::ios::binary|std::ios::app; 
			else amode = std::ios::out|std::ios::binary;
			bmode = MCL_DUMP_BUFFER;
			is_open=0;
		}
		odump(int app_flag, int buffer_flag) { 
			if(app_flag == DUMP_APPEND) amode = std::ios::out|std::ios::binary|std::ios::app; 
			else amode = std::ios::out|std::ios::binary;
			bmode = buffer_flag;
			is_open=0;
		}
		odump(std::string fn) {
			amode = std::ios::out|std::ios::binary; 
			bmode = MCL_DUMP_BUFFER;
			is_open=0;
			open(fn);
		}
		odump(std::string fn, int app_flag) {
			if(app_flag == DUMP_APPEND) amode = std::ios::out|std::ios::binary|std::ios::app; 
			else amode = std::ios::out|std::ios::binary;
			bmode = MCL_DUMP_BUFFER;
			is_open=0;
			open(fn);
		}
		odump(std::string fn, int app_flag, int buffer_flag) {
			if(app_flag == DUMP_APPEND) amode = std::ios::out|std::ios::binary|std::ios::app; 
			else amode = std::ios::out|std::ios::binary;
			bmode = buffer_flag;
			is_open=0;
			open(fn);
		}
		~odump() { if (is_open) close();}
		void open(std::string fn) {
			if (is_open==0) {
				if (bmode) {
					filename=fn;
					fn+=".buf";
					if (amode & std::ios::app) {
						std::string coco="touch "+filename+"; cp -f "+filename+" "+fn;
						system(coco.c_str());
					}
					//std::cout << " open " << filename << " " << fn << std::endl;
				}
				binary_file.open(fn.c_str(),amode);
				is_open=1;
			}
		}  
		void close() {
			if (is_open==1) {
				//no clue, why the following 4 lines were there
				//std::size_t found;
				//std::string findstr ("info");
				//found=filename.find(findstr);
				//binary_file.close();
				if (bmode) {
					std::string str="mv -f "+filename+".buf "+filename;
					char * cstr;
					cstr = new char [str.size()+1];
  					strcpy(cstr, str.c_str());
					int dummy=1;
					int count=0;
					while (dummy && count <1000) {
						dummy=system(cstr);
						++count;
					}
					delete[] cstr; 
					//std::cout << " close " << str <<std::endl;
				}
				is_open=0;
			}	
		}
		template <class Type> void write(std::vector<Type>&);
		void write(std::vector<bool>&);
		template <class Type> void write(Type*, int);
		template <class Type> void write(Type&);
		void write(std::string&);

	private:
		std::string filename;
		std::ofstream binary_file;
		std::ios_base::openmode amode;
		int bmode;
		int is_open;
};

class idump
{
	public:
		
		//changed the endianness of val
		void endian_convert(unsigned char * val, int size) 
		{
			int i = 0;
			int j = size-1;
			while (i<j) {
				std::swap(val[i], val[j]);
				i++;
				j--;
			}
		}

		idump() {is_open=0;}
		idump(std::string filename) {is_open=0;open(filename);}
		~idump() {close();}
		bool operator!() { return !binary_file;}
		bool good() {return !(!binary_file);}
		void open(std::string filename) {if (!is_open) {binary_file.open(filename.c_str(),std::ios::in|std::ios::binary);is_open=1;}}
		template <class Type> void read(std::vector<Type>&);
		void read(std::vector<bool>&);
		template <class Type> void read(Type*, int);
		template <class Type> void read(Type&);
		// read data and swap endianness
//		template <class Type> void read_se(std::vector<Type>&);
		template <class Type> void read_se(Type*, int);
		template <class Type> void read_se(Type&);

		template <class Type> void ignore(int);

		void read(std::string&);
		void close() {if (is_open) {binary_file.close();is_open=0;}}
	private:
		std::ifstream binary_file;
		int is_open;
};

template<class Type>
void odump :: write(std::vector<Type>& v) {
	luint n=v.size();
	write(n);
	for (uint i=0;i<n;++i)
		write(v[i]);
}

template <class Type>
void odump :: write(Type *a, int size)
{
	binary_file.write((char*)a, size*sizeof(Type));
}

template <class Type>
void odump :: write(Type& a)
{
	binary_file.write((char*)(&a), sizeof(Type));
}

template<class Type>
void idump :: read(std::vector<Type>& v) {
	luint n;
	read(n);
	v.clear();
	for (uint i=0;i<n;++i) {
		Type nel;
		read(nel);
		v.push_back(nel);
	}
}
/*
template <class Type>
void idump :: read_se(std::vector<Type>& v) {
	luint n;
	read(&n);
	endian_convert((unsigned char*) n,sizeof(n));
	v.clear();
	for(uint i=0;i<n;++i) {
		Type nel;
		read(nel);
		endian_convert(&nel,sizeof(nel));
		v.push_back(nel);
	}
}*/

template <class Type>
void idump :: read(Type *a, int size)
{
 	binary_file.read((char*)a,size*sizeof(Type));
}

template <class Type>
void idump :: read_se(Type *a, int size)
{
	binary_file.read((char*)a,size*sizeof(Type));
	endian_convert(a,size*sizeof(Type));
}

template <class Type>
void idump :: read(Type& a)
{
 	binary_file.read((char*)(&a),sizeof(Type));
}

template <class Type>
void idump :: read_se(Type& a)
{
	binary_file.read((char*)(&a),sizeof(Type));
	endian_convert(&a,sizeof(Type));
}

template <class Type>
void idump :: ignore(int size)
{
	binary_file.ignore(size*sizeof(Type));
}

#endif
