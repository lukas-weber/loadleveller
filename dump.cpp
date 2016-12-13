#include "dump.h"

void odump :: write(std::string& a)
{
	luint asize=a.size();
	write(asize);
	binary_file.write(a.c_str(),(asize+1)*sizeof(char));
}

void idump :: read(std::string& a)
{
	luint asize;
	read(asize);
	char* ca=new char[asize+1];
	binary_file.read(ca,(asize+1)*sizeof(char));
	a=ca;
}

//hope, these work
void odump :: write(std::vector<bool>& v) {
        luint n=v.size();
        write(n);
        for (uint i=0;i<n;++i) {
                bool nel = v[i];
                write(nel);
        }
}
  
void idump :: read(std::vector<bool>& v) {  
        luint n;
        read(n);
        v.clear();
        for (uint i=0;i<n;++i) {
                bool nel;
                read(nel);
                v.push_back(nel);       
        }
}


