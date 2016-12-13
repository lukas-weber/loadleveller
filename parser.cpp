#include "parser.h"

parser :: parser()
{
	notfine = " =\t";
	comment = "#";	
	array = "@";
}

parser :: parser(std::string input)
{
	notfine = " =\t";
	comment = "#";
	array = "@";
	read_file(input);
}

bool parser :: read_file(std::string input)
{
	std::string  buffer;
	std::fstream file(input.c_str(), std::ios::in);
	if (!file.good()) {
		std::cerr << "File not found: " << input << std::endl; 
		exit(1);
		return false;
	}
	while(getline(file,buffer))
	{
		std::string::size_type where_is_comment = buffer.find_first_of(comment);
		if(where_is_comment != buffer.npos)
		{
			buffer.erase(where_is_comment, buffer.size());
		}
		
		std::string::size_type is_array = buffer.find_first_of(array);
		if(is_array != buffer.npos)
		{	
			// we have to read an array
			//std::string::size_type start = buffer.find_first_not_of(array); not used?
			std::string::size_type end = buffer.find_first_of(array);
			buffer.erase(0, end);
			std::vector< std::string > dumpy;
			std::string line2;
			while(getline(file, line2) && line2.find(array) == line2.npos)
			{
				dumpy.push_back(line2);
			}
			arrs[buffer]=dumpy;
		} else
		{	// normal variable
			std::string _name, value;
			std::string::size_type start = buffer.find_first_not_of(notfine);
			std::string::size_type end = buffer.find_first_of(notfine);
			if(end != buffer.npos)
			{
				_name.assign(buffer, start, end);
				buffer.erase(start, end);
				end = buffer.find_first_not_of(notfine);
				buffer.erase(0, end);
				vars[_name]=buffer;
			}
		}
	}
	file.close();
	readVals.clear();
	return true;
}

std::string parser::return_value_of(std::string name)
{
	std::map<std::string,std::string>::iterator it;
	it=vars.find(name);
	if(it!=vars.end())
	{
		readVals.push_back(name);
		return (*it).second;
	}
	else {
		std::cerr << "#PARSER: NO VARIABLE WITH NAME " << name << " FOUND!" << std::endl;
		return "";
	}
}

std::string parser::value_or_default(std::string name, std::string defa)
{
	std::map<std::string,std::string>::iterator it;
	it=vars.find(name);
	if(it!=vars.end()) {
		readVals.push_back(name);
		return (*it).second;
	}
	else return defa;
}

bool parser::defined(std::string name)
{
	std::map<std::string,std::string>::iterator it;
	it=vars.find(name);
	std::map<std::string,std::vector<std::string> >::iterator arrit = arrs.find(name);
	return it != vars.end() || arrit != arrs.end();
//	if(it==vars.end()) return false;
//	else return true;
}	

void parser :: add_comment(std::string new_comm)
{
	comment += new_comm;
}

void parser :: add_delim(std::string new_del)
{
	notfine += new_del;
}

void parser :: get_all(std::ostream& os)
{
	std::map<std::string,std::string>::iterator i;
	std::map<std::string,std::vector<std::string> >::iterator i2;
	for(i=vars.begin(); i!=vars.end(); i++)
		os<<(*i).first<<" = "<<(*i).second<< std::endl;

	for(i2=arrs.begin(); i2!=arrs.end();i2++) {
		os<<(*i2).first<<"\n";
		int size=((*i2).second).size();
		for(int j=0;j<size;j++)
			os<< ((*i2).second)[j] <<"\n";
		os<<(*i2).first<< std::endl;
	}
}	

void parser :: get_all_with_one_from_specified_array(std::string sfai, int fai, std::ostream& os)
{
	std::map<std::string,std::string>::iterator i;
	std::map<std::string,std::vector<std::string> >::iterator i2;
	
	for(i=vars.begin(); i!=vars.end(); i++)
		os<<(*i).first<<" = "<<(*i).second<<"\n";

	for(i2=arrs.begin(); i2!=arrs.end();i2++) {
		if((*i2).first==sfai)
			os << (*i2).first.substr(1) << " = " << ((*i2).second)[fai] << "\n";
		else {
			os<<(*i2).first<<"\n";
			int size=((*i2).second).size();
			for(int j=0;j<size;j++)
				os<< ((*i2).second)[j] <<"\n";
			os<<(*i2).first<<"\n";

		}
	}
}

std::vector<std::string> parser::unused_parameters() {
	std::vector<std::string> unused;
	std::vector<std::string>::iterator re;
	bool in;
	for (std::map<std::string, std::string>::iterator va = vars.begin(); va != vars.end(); ++va) {
		in = false;
		for (re = readVals.begin(); re != readVals.end(); ++re) {
			if (va->first == (*re)) {
				in = true;
				break;
			}
		}
		if (!in)
			unused.push_back(va->first);
	}
	for (std::map<std::string, std::vector<std::string> >::iterator va = arrs.begin(); va != arrs.end(); ++va) {
		in = false;
		for (re = readVals.begin(); re != readVals.end(); ++re) {
			if (va->first == (*re)) {
				in = true;
				break;
			}
		}
		if (!in)
			unused.push_back(va->first);
	}
	return unused;
}

