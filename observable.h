#ifndef MCL_OBSERVABLE_H
#define MCL_OBSERVABLE_H

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <vector>
#include <valarray>
#include <map>
#include "types.h"
#include "dump.h"

using namespace std;
		 
class observable
{
	public:	

		uint binning_base() { return 2; }	
		uint min_number_of_bins() { return 32; }

		observable();
		observable(string);
		observable(string,luint);
		observable(string,luint,luint);
		observable(string,luint,luint,luint);
		observable(string,luint,luint,luint,luint,luint);

		~observable() {}		
	
		string name() {return name_;}
		void set_name(string newname) {name_=newname;}
	
		luint bin_length() {return bin_length_;}
		void set_bin_length(luint bl) {bin_length_=bl;}

		luint vector_length() {return vector_length_;}
		void set_vector_length(luint vl) {init(name_, vl, bin_length_, initial_length, resizefactor, resizeoffset);}

		luint bins() {return current_bin;}

		void extract(int, observable&);

		void mean(valarray<double>&);
		void error(valarray<double>&);
		void naiveerror(valarray<double>&);
		void variance(valarray<double>&);
		void autocorrelationtime(valarray<double>&);
		
		double mean(uint);
		double error(uint);
		double naiveerror(uint);
		double variance(uint);
		double autocorrelationtime(uint);
		
		void timeseries(uint, valarray<double>&);

 		template <class T> void add(T);
		template <class T> void add(T*);
		template <class T> void add(const vector<T>&);
		void add(const valarray<double>&);

		void rebin(int);
		
		void reset();

		void write(odump&);
		bool read(idump&, int skipbins=0);
		bool merge(idump& id) {return merge(id,0,0);}
		bool merge(idump& id, int skipbins) {return merge(id,skipbins,0);} 
		bool merge(idump& id, int skipbins, int check_bin_length);

		void write_appendmode(odump&,odump&); //separates data/structural info on observable; data output is appended
		bool read_appendmode(idump&);
		bool merge_appendmode(idump& id) {int tmpi=0;return  merge_appendmode(id,tmpi,0);}
		bool merge_appendmode(idump& id, int& skipbins) {return merge_appendmode(id,skipbins,0);}
		bool merge_appendmode(idump& id, int& skipbins, int check_bin_length);

		void analyse() {binning();simple_analysis_done=true;analysis_done=true;}
		void simple_analyse() {simple();simple_analysis_done=true;}

		void get_statistics(ostream&);

		valarray<double> samples;
		vector<valarray<double> > mean_v;
		vector<valarray<double> > error_v;
		
	private:
		string name_;
		luint bin_length_;
		luint vector_length_;
		luint length;
		luint initial_length;
		luint current_bin;
		luint current_bin_filling;
		luint chunks;

		luint resizefactor;
		luint resizeoffset;

		void init(string,luint,luint,luint,luint,luint);
		void binning();
		void simple();

		bool analysis_done;
		bool simple_analysis_done;

		streamsize calcprec(streamsize origprec, double m, double e) {
			if (fabs(e)>1e-200) {
				if (fabs(m/e)>1e-200) 
					return (streamsize)(3. + 0.43429448*log(fabs(m/e)));
				else
					return 3;
			}
			return (origprec>16) ? origprec: 16;
		}
};

template <class T>
void observable :: add(T val)
{
	for (uint j=0;j<vector_length_;++j) 
		samples[j+current_bin*vector_length_]+=static_cast<double>(val);
	++current_bin_filling;
	if (current_bin_filling == bin_length_) //need to start a new bin next time
	{
		if (bin_length_>1) for (uint j=0;j<vector_length_;++j)
			samples[current_bin*vector_length_+j]/=bin_length_;
		if( current_bin == (length - 1)) //need to allocate more memory for a new chunk of bins
		{
			valarray<double> old=samples;
			samples.resize(2*old.size(),0.);
			samples[slice(0,old.size(),1)]=old;
			length += length;
		}
		++current_bin;
		current_bin_filling=0;
	}
}

template <class T>
void observable :: add(T* val)
{
	for (uint j=0;j<vector_length_;++j) 
		samples[j+current_bin*vector_length_]+=static_cast<double>(val[j]);
	++current_bin_filling;
	if (current_bin_filling == bin_length_) //need to start a new bin next time
	{
		if (bin_length_>1) for (uint j=0;j<vector_length_;++j)
			samples[current_bin*vector_length_+j]/=bin_length_;
		if( current_bin == (length - 1)) //need to allocate more memory for a new chunk of bins
		{
			valarray<double> old=samples;
			samples.resize(2*old.size(),0.);
			samples[slice(0,old.size(),1)]=old;
			length += length;
		}
		++current_bin;
		current_bin_filling=0;
	}
}

template <class T> 
void observable :: add(const vector<T>& val)
{
	if (!current_bin && val.size()>vector_length_) {
		vector_length_=val.size();
		samples.resize(length*vector_length_,0.);
	}
	for (uint j=0;j<vector_length_;++j) 
		samples[j+current_bin*vector_length_]+=static_cast<double>(val[j]);
	++current_bin_filling;
	if (current_bin_filling == bin_length_) //need to start a new bin next time
	{
		if (bin_length_>1) for (uint j=0;j<vector_length_;++j)
			samples[current_bin*vector_length_+j]/=bin_length_;
		if( current_bin == (length - 1)) //need to allocate more memory for a new chunk of bins
		{
			valarray<double> old=samples;
			samples.resize(2*old.size(),0.);
			samples[slice(0,old.size(),1)]=old;
			length += length;
		}
		++current_bin;
		current_bin_filling=0;
	}
}

#endif

