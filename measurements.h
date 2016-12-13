#ifndef MCL_MEASUREMENTS_H
#define MCL_MEASUREMENTS_H

#ifndef MCL_MEASUREMENTS_APPEND
#define MCL_MEASUREMENTS_APPEND 0
#endif

#if MCL_MEASUREMENTS_APPEND 
#warning MCL_MEASUREMENTS_APPEND=1: measurements will be written in append mode 
#else 
#warning MCL_MEASUREMENTS_APPEND=0: observable I/O involve full data sets 
#endif

#include <iostream>
#include <fstream>
#include <cmath>
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

		observable* get_observable(std::string name) {return obs_v[eo[name]];}
		observable* get_vectorobservable(std::string name) {return obs_v[eo[name]];}
		evalable* get_evaluable(std::string name) {return eva_v[eo[name]];}
		evalable* get_vectorevalable(std::string name) {return eva_v[eo[name]];} 
		void extract_observable(std::string name, int i, observable& os) {obs_v[eo[name]]->extract(i,os);}

		//! Adds scalar observable
		/*! Adds a new observable with specified name and default bin length*/
		void add_observable(std::string);
		/*! Adds a new observable with specified name and specific bin length. */
		void add_observable(std::string,luint);
		/*! Adds a new observable with specified name, specific bin length and a specified initial number of bins*/
		void add_observable(std::string,luint,luint);
		/*! Adds a new observable with specified name, specific bin length and a specified initial number of bins and resizing data*/
		void add_observable(std::string,luint,luint,luint,luint);

		//! Adds vector observable
		/*! Adds a new vector observable with specified name and default vector length and default bin length*/
		void add_vectorobservable(std::string);
		/*! Adds a new vector observable with specified name and specified vector length and default bin length*/
		void add_vectorobservable(std::string,luint);
		/*! Adds a new vector observable with specified name and specified vector length and specified bin length*/
		void add_vectorobservable(std::string,luint,luint);
		/*! Adds a new vector observable with specified name and specified vector length and specified bin length and specified initial number of bins*/
		void add_vectorobservable(std::string,luint,luint,luint);
		/*! Adds a new vector observable with specified name and specified vector length and specified bin length and specified initial number of bins and resizing data*/
		void add_vectorobservable(std::string,luint,luint,luint,luint,luint);

		//! (Re)Set bin length for all observables
		void set_bin_length(luint bl) {for (uint i=0;i<obs_v.size();++i) obs_v[i]->set_bin_length(bl);}
		void set_bin_length(std::string name, luint bl) {obs_v[eo[name]]->set_bin_length(bl);}
		void set_vector_length(std::string name, luint nl) {obs_v[eo[name]]->set_vector_length(nl);}

 		void add_evalable(std::string, std::vector<std::string>&, 
			void (*f) (double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,std::string,std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&)); 

 		void add_evalable(std::string, std::vector<std::string>&, 
			void (*f) (double&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_evalable(std::string, std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_evalable(std::string, std::string, std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_evalable(std::string, std::string, std::string, std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_evalable(std::string, std::string, std::string, std::string, std::string,
			void (*f) (double&, std::vector <std::valarray<double>* >&, double*), double*);

		void add_vectorevalable(std::string, std::vector<std::string>&, 
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&));
		void add_vectorevalable(std::string, std::string, 
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string, std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string, std::string, std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&)); 

 		void add_vectorevalable(std::string, std::vector<std::string>&, 
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_vectorevalable(std::string,std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_vectorevalable(std::string,std::string,std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_vectorevalable(std::string,std::string,std::string,std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&, double*), double*);
 		void add_vectorevalable(std::string,std::string,std::string,std::string,std::string,
			void (*f) (std::valarray<double>&, std::vector <std::valarray<double>* >&, double*), double*);

 		void add_evalable(std::string, std::vector<std::string>&, 
			void*, void (*f) (void*, double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,
			void*, void (*f) (void*, double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,
			void*, void (*f) (void*, double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,std::string,
			void*, void (*f) (void*, double&, std::vector <std::valarray<double>* >&)); 
 		void add_evalable(std::string,  std::string,std::string,std::string,std::string,
			void*, void (*f) (void*, double&, std::vector <std::valarray<double>* >&)); 

		void add_vectorevalable(std::string, std::vector<std::string>&, 
			void*, void (*f) (void*, std::valarray<double>&, std::vector <std::valarray<double>* >&));
		void add_vectorevalable(std::string, std::string, 
			void*, void (*f) (void*, std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string, 
			void*, void (*f) (void*, std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string, std::string,
			void*, void (*f) (void*, std::valarray<double>&, std::vector <std::valarray<double>* >&)); 
		void add_vectorevalable(std::string, std::string, std::string, std::string, std::string,
			void*, void (*f) (void*, std::valarray<double>&, std::vector <std::valarray<double>* >&)); 


		//! Add measurment to observable
		template <class T> 
		void add(std::string name, const T& value) {obs_v[eo[name]]->add(value);}
		template <class T> 
		void add(std::string name, T* value) {obs_v[eo[name]]->add(value);}
		template <class T> 
		void add(std::string name, const vector<T>& value) {obs_v[eo[name]]->add(value);}
 		void add(std::string name, const std::valarray<double>& value) {obs_v[eo[name]]->add(value);}

 		void mean(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo[name]]->mean(v) : eva_v[eo[name]]->mean(v);}
 		void error(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo[name]]->error(v) : eva_v[eo[name]]->error(v);}
		void variance(string name, std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo[name]]->variance(v) : obs_v[eo[name]]->variance(v);}
 		void autocorrelationtime(std::string name,std::valarray<double>& v) 
			{(tag[name]) ? obs_v[eo[name]]->autocorrelationtime(v) : eva_v[eo[name]]->autocorrelationtime(v);}
		
		//! Mean
		double mean(std::string name,uint j=0)  
			{return (tag[name]) ? obs_v[eo[name]]->mean(j) : eva_v[eo[name]]->mean(j);}
		//! Error
		double error(std::string name,uint j=0) 
			{return (tag[name]) ? obs_v[eo[name]]->error(j) : eva_v[eo[name]]->error(j);}
		//! Variance
		double variance(std::string name,uint j=0) 
			{return (tag[name]) ? obs_v[eo[name]]->variance(j) : eva_v[eo[name]]->variance(j);}
		//! Autocorrelation time
		/*! Extract the integrated autocorrelation time obtained from a binning analysis. */
		double autocorrelationtime(std::string name,uint j=0)  
			{return (tag[name]) ? obs_v[eo[name]]->autocorrelationtime(j) : eva_v[eo[name]]->autocorrelationtime(j);}
		//! Vector length
                double vector_length(std::string name)
                        {return (tag[name]) ? obs_v[eo[name]]->vector_length() : eva_v[eo[name]]->vector_length();}
                //! Naive Error
                /*! Extract the naive error (assuming no correlations) of observable.*/
                double naiveerror(std::string name,uint j=0)
                        {return (tag[name]) ? obs_v[eo[name]]->naiveerror(j) : eva_v[eo[name]]->naiveerror(j);}
		//! Number of Bins
		int bins(std::string name,uint j=0)
			{return (tag[name]) ? obs_v[eo[name]]->bins() : eva_v[eo[name]]->bins();}


		void timeseries(std::string name, uint j, std::valarray<double>& v) {obs_v[eo[name]]->timeseries(j,v);} 
		void timeseries(std::string name, std::valarray<double>& v) {obs_v[eo[name]]->timeseries(0,v);}

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
	
#endif
