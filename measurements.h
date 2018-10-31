#pragma once
#ifndef MCL_MEASUREMENTS_APPEND
#define MCL_MEASUREMENTS_APPEND 0
#endif

#include <string>
#include <vector>
#include <map>
#include <valarray>
#include "types.h"
#include "dump.h"
#include "observable.h"
#include "evalable.h"

//! Basic measurements class
/*! The interface to add and access measurements and observables is controlled from this class. */
class measurements
{
	public :
		measurements() {chunks=0;}
		~measurements() {clear();}

		observable* get_observable(std::string name) {return obs_v[eo.at(name)];}
		observable* get_vectorobservable(std::string name) {return obs_v[eo.at(name)];}
		evalable* get_evaluable(std::string name) {return eva_v[eo.at(name)];}
		evalable* get_vectorevalable(std::string name) {return eva_v[eo.at(name)];} 
		void extract_observable(std::string name, int i, observable& os) {obs_v[eo.at(name)]->extract(i,os);}

		/*! Adds a new observable with specified name, specific bin length and a specified initial number of bins and resizing data*/
		void add_observable(std::string,luint=1,luint=1000,luint=2,luint=0);

		/*! Adds a new vector observable with specified name and specified vector length and specified bin length and specified initial number of bins and resizing data*/
		void add_vectorobservable(std::string,luint=1,luint=1,luint=1000,luint=2,luint=0);

		//! (Re)Set bin length for all observables
		void set_bin_length(luint bl) {for (uint i=0;i<obs_v.size();++i) obs_v[i]->set_bin_length(bl);}
		void set_bin_length(std::string name, luint bl) {obs_v[eo.at(name)]->set_bin_length(bl);}
		void set_vector_length(std::string name, luint nl) {obs_v[eo.at(name)]->set_vector_length(nl);}

 		void add_evalable(std::string, const std::vector<std::string>&, evalablefunc f); 

		void add_vectorevalable(std::string, const std::vector<std::string>&, vectorevalablefunc);

		//! Add measurment to observable
		template <class T> 
		void add(std::string name, const T& value) {obs_v[eo.at(name)]->add(value);}
		template <class T> 
		void add(std::string name, T* value) {obs_v[eo.at(name)]->add(value);}
		template <class T> 
		void add(std::string name, const vector<T>& value) {obs_v[eo.at(name)]->add(value);}
 		void add(std::string name, const std::valarray<double>& value) {obs_v[eo.at(name)]->add(value);}

 		void mean(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo.at(name)]->mean(v) : eva_v[eo.at(name)]->mean(v);}
 		void error(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo.at(name)]->error(v) : eva_v[eo.at(name)]->error(v);}
		void variance(string name, std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo.at(name)]->variance(v) : obs_v[eo.at(name)]->variance(v);}
 		void autocorrelationtime(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo.at(name)]->autocorrelationtime(v) : eva_v[eo.at(name)]->autocorrelationtime(v);}
		
		//! Mean
		double mean(std::string name,uint j=0)  
			{return (tag[name]) ? obs_v[eo.at(name)]->mean(j) : eva_v[eo.at(name)]->mean(j);}
		//! Error
		double error(std::string name,uint j=0) 
			{return (tag[name]) ? obs_v[eo.at(name)]->error(j) : eva_v[eo.at(name)]->error(j);}
		//! Variance
		double variance(std::string name,uint j=0) 
			{return (tag[name]) ? obs_v[eo.at(name)]->variance(j) : eva_v[eo.at(name)]->variance(j);}
		//! Autocorrelation time
		/*! Extract the integrated autocorrelation time obtained from a binning analysis. */
		double autocorrelationtime(std::string name,uint j=0)  
			{return (tag[name]) ? obs_v[eo.at(name)]->autocorrelationtime(j) : eva_v[eo.at(name)]->autocorrelationtime(j);}
		//! Vector length
                double vector_length(std::string name)
                        {return (tag[name]) ? obs_v[eo.at(name)]->vector_length() : eva_v[eo.at(name)]->vector_length();}
                //! Naive Error
                /*! Extract the naive error (assuming no correlations) of observable.*/
                double naiveerror(std::string name,uint j=0)
                        {return (tag[name]) ? obs_v[eo.at(name)]->naiveerror(j) : eva_v[eo.at(name)]->naiveerror(j);}
		//! Number of Bins
		int bins(std::string name)
			{return (tag[name]) ? obs_v[eo.at(name)]->bins() : eva_v[eo.at(name)]->bins();}


		void timeseries(std::string name, uint j, std::valarray<double>& v) {obs_v[eo.at(name)]->timeseries(j,v);} 
		void timeseries(std::string name, std::valarray<double>& v) {obs_v[eo.at(name)]->timeseries(0,v);}

		//! Get statistics
		/*! Pipe the statiscial information for one observable/evalable to a stream.*/
		void get_statistics(std::string, std::ostream&);	
		/*! Pipe the statiscial information for all observables/evalables to a stream.*/
		void get_statistics(std::ostream&);

		bool appendmode() {
			return (MCL_MEASUREMENTS_APPEND==1) ? true : false; 
		}


		void rebin(int new_bin_length) {for (uint i=0;i<obs_v.size();++i) obs_v[i]->rebin(new_bin_length);}

		void write(odump&);
		bool read(idump&);
		bool printdumpsummary(idump&);
		bool merge(idump&,int skipbins=0);

		void write_appendmode(odump&,odump&);
		bool read_appendmode(idump&);
		bool printdumpsummary_appendmode(idump&, idump& );
		bool merge_appendmode(idump&,idump&,int skipbins=0);

		void write(std::string);
		bool read(std::string);
		bool printdumpsummary(std::string);
		bool merge(std::string,int skipbins=0);

		void reset();
		void clear();

		std::vector<observable*> obs_v;	
		std::vector<evalable*> eva_v;	

	private	:
		luint chunks;
		std::map<std::string,luint> eo;
		std::map<std::string,int> tag; //1=obervable,0=evalable
};
