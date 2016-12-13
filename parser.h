#ifndef MY_PARSER_H
#define MY_PARSER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <map>
#include "types.h"

class parser
{
	public:
		parser();
		parser(std::string);
		
		// one can add other symbols for comments, etc.
		void add_comment(std::string);
		void add_delim(std::string);
		
		// read a file (if it has not happend yet)
		bool read_file(std::string);
		
		// writes out all variables to the specified output
		void get_all(std::ostream&);

		// writes out all scalar variables and from the first array 
		// the secified entry to the specified output
		// usefull for output within a parallel tempering code
		void get_all_with_one_from_specified_array(std::string, int, std::ostream&);

		std::string return_value_of(std::string);
		std::string value_of(std::string name) {return return_value_of(name);}
		std::string operator[](std::string name) {return return_value_of(name);}

		bool defined(std::string);

		template <class Type>
		Type return_value_of(std::string name)
		{
			Type ret;
			std::stringstream buffer;
			std::map<std::string,std::string>::iterator it;
			it=vars.find(name);
			if(it != vars.end()) {	
				buffer << vars[name];
				buffer >> ret;
				readVals.push_back(name);
				return ret;
			} else {
				std::cerr<<"#PARSER: NO VARIABLE WITH NAME "<<name<<" FOUND!"<< std::endl;
				return Type();
			}
		};
		
		template <class Type>
		Type value_of(std::string name) {return return_value_of<Type>(name);}

		template <class Type>
		Type operator[](std::string name) {return return_value_of<Type>(name);}

		std::string value_or_default(std::string, std::string);
		
		template <class Type>
		Type value_or_default(std::string name, Type defa)
		{
			Type ret;
			std::stringstream buffer;
			std::map<std::string,std::string>::iterator it;
			it=vars.find(name);
			if(it != vars.end()) {	
				buffer << vars[name];
				buffer >> ret;
				readVals.push_back(name);
				return ret;
			} else return defa;
		};


		template <class Type>
		void set_value(std::string name, Type value)
		{
			std::stringstream b;
			b<<value;
			vars[name]=b.str();
		};

		template <class Type>
		std::vector< Type > return_vector(std::string name)
		{
			std::vector< Type > ret;
			std::map<std::string,std::vector<std::string> >::iterator it;
			it=arrs.find(name);
			if(it==arrs.end()) {
				std::cerr<<"#PARSER: NO ARRAY WITH NAME "<<name<<" FOUND!"<<std::endl; 
				return ret;
			}
			for(uint j=0;j<((*it).second).size();j++) {
				Type tmp;
				std::stringstream buffer;
				buffer<<((*it).second)[j];
				buffer>>tmp;
				ret.push_back(tmp);
			}
			readVals.push_back(name);
			return ret;
		};

		template <class Type>
		std::vector< Type > vector(std::string name) {return return_vector<Type>(name);}

		std::vector<std::string> unused_parameters();


	private:
		std::string notfine, comment, array;
		std::map<std::string,std::string> vars;
		std::map<std::string,std::vector<std::string> > arrs;
		std::vector<std::string> readVals;
};



#endif

