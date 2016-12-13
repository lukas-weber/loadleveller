#ifndef MCL_VECTOREVALABLE_H
#define MCL_VECTOREVALABLE_H

#include <iostream>
#include <sstream>
#include <cmath>
#include <string>
#include <vector>
#include <valarray>
#include "types.h"
#include "observable.h"

class evalable
{
	public:
		evalable() {name_="";}
		evalable(string newname) {name_=newname;} 
		
		~evalable() {}

		string name() {return name_;}
		void set_name(string newname) {name_=newname;}
		luint  bins(){return bins_;}
		luint  vector_length() {return vector_length_;}
		luint  bin_length() {return bin_length_;}
 
                void mean(valarray<double>&); 
                void error(valarray<double>&);
		void variance(valarray<double>&);
		void autocorrelationtime(valarray<double>&); //need to implement jackknife binning still
		double mean(uint);
		double error(uint);
		double naiveerror(uint);
		double variance(uint);
		double autocorrelationtime(uint); //need to implement jackknife binning still

		void jackknife(vector<observable*>& , void (*f) (double&, vector <valarray<double>* >&));
		void jackknife(vector<observable*>& , void (*f) (double&, vector <valarray<double>* >&,double*), double*);
		void vectorjackknife(vector<observable*>& , void (*f) (valarray<double>&, vector <valarray<double>* >&));
		void vectorjackknife(vector<observable*>& , void (*f) (valarray<double>&, vector <valarray<double>* >&,double*), double*);

		void jackknife(vector<observable*>& , void* , void (*f) (void*, double&, vector <valarray<double>* >&));
		void vectorjackknife(vector<observable*>& , void*, void (*f) (void*, valarray<double>&, vector <valarray<double>* >&));


 		void reset() {mean_v.clear();error_v.clear();}
 		void get_statistics(ostream&);

 		string toString();

		vector<valarray<double> > mean_v;
		vector<valarray<double> > error_v;

	private:
                string name_;
		luint bins_;
		luint vector_length_;
		luint bin_length_;
		luint binning_base;	
};

#endif
