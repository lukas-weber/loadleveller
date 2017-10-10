#include "measurements.h"
#include <sstream>

void measurements :: add_observable(std::string name) 
{
	observable *o = new observable(name,1);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_observable(std::string name, luint bin_length) 
{
	observable *o = new observable(name, 1,bin_length);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_observable(std::string name, luint bin_length, luint initial_length) 
{
	observable *o = new observable(name, 1, bin_length, initial_length);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_observable(std::string name, luint bin_length, luint initial_length, luint resizefactor, luint resizeoffset)
{
        observable *o = new observable(name, 1, bin_length, initial_length, resizefactor, resizeoffset);
        obs_v.push_back(o);
        eo[name]=obs_v.size()-1;
        tag[name]=1;
}



void measurements :: add_vectorobservable(std::string name) 
{
	observable *o = new observable(name);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_vectorobservable(std::string name, luint vector_length) 
{
	observable *o = new observable(name, vector_length);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_vectorobservable(std::string name, luint vector_length, luint bin_length) 
{
	observable *o = new observable(name, vector_length,bin_length);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_vectorobservable(std::string name, luint vector_length, luint bin_length, luint initial_length) 
{
	observable *o = new observable(name, vector_length, bin_length, initial_length);
	obs_v.push_back(o);
	eo[name]=obs_v.size()-1;
	tag[name]=1;
}

void measurements :: add_vectorobservable(std::string name, luint vector_length, luint bin_length, luint initial_length, luint resizefactor, luint resizeoffset)
{
        observable *o = new observable(name, vector_length, bin_length, initial_length, resizefactor, resizeoffset);
        obs_v.push_back(o);
        eo[name]=obs_v.size()-1;
        tag[name]=1;
}


void measurements :: add_evalable(std::string name, std::vector<std::string>& n, 
			void (*f) (double&, std::vector <std::valarray<double>* >&))
{
 	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) {
		if (eo.find(n[i]) == eo.end()) {
			stringstream s;
			s << "No observable with name " << n[i];
			throw s.str();
		}
		v.push_back(obs_v[eo[n[i]]]);
	}
 	e->jackknife(v,f);
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}

void measurements :: add_evalable(std::string name, std::string n1, 
			void (*f) (double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_evalable(name,n,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2,
			void (*f) (double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_evalable(name,n,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3,
			void (*f) (double&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_evalable(name,n,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3,std::string n4,
			void (*f) (double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_evalable(name,n,f);
}



void measurements :: add_evalable(std::string name, std::vector<std::string>& n, 
			void (*f) (double&, std::vector <std::valarray<double>* >&, double* p), double* p) 
{
  	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) v.push_back(obs_v[eo[n[i]]]);
 	e->jackknife(v,f,p);
 	//cout << e->toString() << endl;
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}

void measurements :: add_evalable(std::string name, std::string n1, 
			void (*f) (double&, std::vector<std::valarray<double>* >&,double*), double* p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_evalable(name,n,f,p);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2,
			void (*f) (double&, std::vector<std::valarray<double>* >&, double*), double* p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_evalable(name,n,f,p);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3,
			void (*f) (double&, std::vector<std::valarray<double>* >&, double*), double *p)  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_evalable(name,n,f,p);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3,std::string n4,
			void (*f) (double&, std::vector<std::valarray<double>* >&, double *), double* p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_evalable(name,n,f,p);
}



void measurements :: add_vectorevalable(std::string name, std::vector<std::string>& n, 
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&))
{
 	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) v.push_back(obs_v[eo[n[i]]]);
 	e->vectorjackknife(v,f);
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}
 
void measurements :: add_vectorevalable(std::string name, std::string n1, 
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_vectorevalable(name,n,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_vectorevalable(name,n,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_vectorevalable(name,n,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3,std::string n4,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_vectorevalable(name,n,f);
}

void measurements :: add_vectorevalable(std::string name, std::vector<std::string>& n, 
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&, double*), double* p) 
{
  	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) v.push_back(obs_v[eo[n[i]]]);
 	e->vectorjackknife(v,f,p);
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}

void measurements :: add_vectorevalable(std::string name, std::string n1, 
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&, double*),double* p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_vectorevalable(name,n,f,p);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&, double*),double* p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_vectorevalable(name,n,f,p);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&, double*),double *p)  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_vectorevalable(name,n,f,p);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3,std::string n4,
			void (*f) (std::valarray<double>&, std::vector<std::valarray<double>* >&, double*), double *p) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_vectorevalable(name,n,f,p);
}



void measurements :: add_evalable(std::string name, std::vector<std::string>& n, 
			void* obj, void (*f) (void*, double&, std::vector <std::valarray<double>* >&))
{
 	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) {
		if (eo.find(n[i]) == eo.end()) {
			stringstream s;
			s << "No observable with name " << n[i];
			throw s.str();
		}
		v.push_back(obs_v[eo[n[i]]]);
	}
 	e->jackknife(v,obj,f);
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}

void measurements :: add_evalable(std::string name, std::string n1, 
			void* obj, void (*f) (void*, double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_evalable(name,n,obj,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2,
			void* obj, void (*f) (void*, double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_evalable(name,n,obj,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3,
			void* obj, void (*f) (void*, double&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_evalable(name,n,obj,f);
}

void measurements :: add_evalable(std::string name, std::string n1, std::string n2, std::string n3, std::string n4,
			void* obj, void (*f) (void*, double&, std::vector<std::valarray<double>* >&)) 
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_evalable(name,n,obj,f);
}



void measurements :: add_vectorevalable(std::string name, std::vector<std::string>& n, 
			void* obj, void (*f) (void*, std::valarray<double>&, std::vector<std::valarray<double>* >&))
{
 	evalable *e = new evalable(name);
	std::vector<observable* > v;
	for (uint i=0;i<n.size();++i) v.push_back(obs_v[eo[n[i]]]);
 	e->vectorjackknife(v,obj,f);
 	eva_v.push_back(e);
 	eo[name]=eva_v.size()-1;
 	tag[name]=0;
}

void measurements :: add_vectorevalable(std::string name, std::string n1,
			void* obj, void (*f) (void*, std::valarray<double>&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	add_vectorevalable(name,n,obj,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, 
			void* obj, void (*f) (void*, std::valarray<double>&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	add_vectorevalable(name,n,obj,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3,
			void* obj, void (*f) (void*, std::valarray<double>&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	add_vectorevalable(name,n,obj,f);
}

void measurements :: add_vectorevalable(std::string name, std::string n1, std::string n2, std::string n3, std::string n4,
			void* obj, void (*f) (void*, std::valarray<double>&, std::vector<std::valarray<double>* >&))  
{
	std::vector<std::string> n;
	n.push_back(n1);
	n.push_back(n2);
	n.push_back(n3);
	n.push_back(n4);
	add_vectorevalable(name,n,obj,f);
}



void measurements :: reset() {
	for (uint i=0;i<obs_v.size();++i ) 
		obs_v[i]->reset();
	for (uint i=0;i<eva_v.size();++i ) 
		eva_v[i]->reset();
}

void measurements :: clear() {
	for (uint i=0;i<obs_v.size();++i) delete obs_v[i];
	for (uint i=0;i<eva_v.size();++i) delete eva_v[i];
	chunks=0;
	obs_v.clear();
	eva_v.clear();
	eo.clear();
	tag.clear();
}

void measurements :: get_statistics(std::string name, ostream& s) {
	if (tag[name]) obs_v[eo[name]]->get_statistics(s);
	else           eva_v[eo[name]]->get_statistics(s);
	
}

void measurements :: get_statistics(ostream& s) {
	for (uint i=0;i<obs_v.size();++i)
		obs_v[i]->get_statistics(s);
	for (uint i=0;i<eva_v.size();++i)
		eva_v[i]->get_statistics(s);
}



void measurements :: write(std::string path) {
        if (appendmode()) {
	        std::string lfn=path+"meas";
        	std::string lfnd=path+"data";
        	odump lbd(lfn.c_str());
        	odump lbdd(lfnd.c_str(),DUMP_APPEND);
		write_appendmode(lbd,lbdd);
        	lbd.close();
        	lbdd.close();
	}
	else {
        	std::string lfn=path+"meas";
	        odump lbd(lfn.c_str());
		write(lbd);
        	lbd.close();
	}
}


bool measurements :: read(std::string path) {
	if (appendmode()) {
	        std::string lfn=path+"meas";
		idump lbd(lfn.c_str());
		bool success=read_appendmode(lbd);
        	lbd.close();
		return success;
	}
	else {
	        std::string lfn=path+"meas";
        	idump lbd(lfn.c_str());
		bool success=read(lbd);
        	lbd.close();
		return success;
	}
}

bool measurements :: printdumpsummary(std::string path) {
	if (appendmode()) {	
		std::string lfn=path+"meas";
                std::string lfnd=path+"data";
		idump lbd(lfn.c_str());
                idump lbdd(lfnd.c_str());
		bool success=printdumpsummary_appendmode(lbd,lbdd);
        	lbd.close();
        	lbdd.close();
		return success;
	}
	else {
	        std::string lfn=path+"meas";
        	idump lbd(lfn.c_str());
		bool success=printdumpsummary(lbd);
        	lbd.close();
		return success;
	}
}


bool measurements :: merge(std::string path, int skipbins) {
	if (appendmode()) {
        	std::string lfn=path+"meas";
        	std::string lfnd=path+"data";
        	if((obs_v.size() == 0)) {//we have no information about observables and need to get this first
			idump bd(lfn.c_str());
                	read_appendmode(bd);
			bd.close();
        	}
	        idump lbd(lfn.c_str());
        	idump lbdd(lfnd.c_str());
		bool success=merge_appendmode(lbd,lbdd,skipbins);
        	lbd.close();
        	lbdd.close();
		return success;
	}
	else {
        	std::string lfn=path+"meas";
	        idump lbd(lfn.c_str());
		bool success=merge(lbd,skipbins);
        	lbd.close();
		return success;
	}
}


void measurements :: write(odump& bd) {
	uint number_of_observables=obs_v.size();
	bd.write(number_of_observables);
	for (uint i=0;i<number_of_observables;++i) 
		obs_v[i]->write(bd);
}


bool measurements :: read(idump& bd) {
        if (!bd)
                return false;
        else {
                bool madeit=true;
                uint number_of_observables;
                bd.read(number_of_observables);
                for (uint i=0;i<number_of_observables;++i) {
                        observable *o = new observable("",1,1,1);
                        if (o->read(bd)) {
                                obs_v.push_back(o);
                                eo[o->name()]=obs_v.size()-1;
                                tag[o->name()]=1;
                        }
                        else {
                                madeit=false;
                                break;
                        }
                }
                return madeit;
        }
}

bool measurements :: printdumpsummary(idump& bd) {
        if (!bd)
                return false;
        else {
                bool madeit=true;
                uint number_of_observables;
                bd.read(number_of_observables);
		cout << "    " << number_of_observables <<" observables"<< endl;
                for (uint i=0;i<number_of_observables;++i) {
                        observable *o = new observable("",1,1,1);
                        if (o->read(bd)) {
				cout <<"    "<< o->name();
				if (o->vector_length()>1) 
					cout << "[" << o->vector_length() <<"] : ";
				else
					cout << " : " ;
				cout << o->bins() << " bins (of length " << o->bin_length() << ")" << endl; 
                        }
                        else {
                                madeit=false;
                                break;
                        }
                }
                return madeit;
        }
}


bool measurements :: merge(idump& bd, int skipbins) {
	if (!bd) 
		return false;
	else {
		bool madeit=true;
		if((obs_v.size() == 0)) {
			uint number_of_observables;
                	bd.read(number_of_observables);
                	for (uint i=0;i<number_of_observables;++i) {
                        	observable *o = new observable("",1,1,1);
                        	if (o->read(bd,skipbins)) {
                                	obs_v.push_back(o);
                                	eo[o->name()]=obs_v.size()-1;
                                	tag[o->name()]=1;
                        	}
                        	else {
                                	madeit=false;
                                	break;
                        	}
			}
		}
		else {
			uint number_of_observables; 
			bd.read(number_of_observables);
			//std::cout <<"  "<< number_of_observables <<" observables" <<std::endl;
			for (uint i=0;i<number_of_observables;++i) { 
				if (!(obs_v[i]->merge(bd,skipbins))) {
					madeit=false;
					break;
				}
			}
		}
		return madeit;
	}
}


void measurements :: write_appendmode(odump& bd,odump& bdd) {
	chunks++;
	uint number_of_observables=obs_v.size();
	bd.write(number_of_observables);
        bd.write(chunks);
	for (uint i=0;i<number_of_observables;++i) 
		obs_v[i]->write_appendmode(bd,bdd);
}


bool measurements :: read_appendmode(idump& bd) {
        if (!bd) return false;
	uint number_of_observables;
	bd.read(number_of_observables);
	bd.read(chunks);
	bool madeit=true;
	for (uint i=0;i<number_of_observables;++i) {
		observable *o = new observable("",1,1,1);
		if (o->read_appendmode(bd)) {
			obs_v.push_back(o);
			eo[o->name()]=obs_v.size()-1;
			tag[o->name()]=1;
		}
		else {
			madeit=false;
			break;
		}
	}
	return madeit;
}

bool measurements :: printdumpsummary_appendmode(idump& bd, idump& bdd) {
	if (!bd) return false;
	uint number_of_observables;		
	bd.read(number_of_observables);
	bd.read(chunks);
	bool madeit=true;
	std::cout <<"    "<< chunks << " chunks with " << number_of_observables <<" observables" << std::endl;
	for (uint i=0;i<number_of_observables;++i) {
                observable *o = new observable("",1,1,1);
                if (o->read_appendmode(bd)) {
                        obs_v.push_back(o);
                        eo[o->name()]=obs_v.size()-1;
                        tag[o->name()]=1;
                }
                else {
                        madeit=false;
                        break;
                }
        }
	for (uint cn=1;cn<=chunks;++cn) { //now, we load all the data
                for (uint i=0;i<number_of_observables;++i) {
                        if (!(obs_v[i]->merge_appendmode(bdd))) {
                                madeit=false;
                                break;
                        }

                }
        }
	if (madeit) for (uint i=0;i<number_of_observables;++i) {
		cout <<"    "<< obs_v[i]->name();
                if (obs_v[i]->vector_length()>1)
                	cout << "[" << obs_v[i]->vector_length() <<"] : ";
               	else
                        cout << " : " ;
                cout << obs_v[i]->bins() << " bins (of length " << obs_v[i]->bin_length() << ")" << endl;
	}
	return madeit;
}


bool measurements :: merge_appendmode(idump& bd, idump& bdd, int skipbins) {
	if (!bd) return false;
	uint number_of_observables;
	bd.read(number_of_observables);
	bd.read(chunks);
	bool madeit=true;
        vector<int> skipbinsv(number_of_observables,skipbins);
	//std::cout << "  (" << chunks << " chunks, " << number_of_observables <<" observables)  ";
	for (uint cn=1;cn<=chunks;++cn) { //now, we load all the data
		for (uint i=0;i<number_of_observables;++i) { 
			if (!(obs_v[i]->merge_appendmode(bdd,skipbinsv[i]))) {//note, that skipbinsv gets adjusted after each chunk
				madeit=false;
				break;
			}
		}
	}
	return madeit;
}


