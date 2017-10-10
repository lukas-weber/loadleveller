#include "evalable.h"
#include <sstream>

void evalable :: mean(valarray<double>& v) {
	v=mean_v[0];
}

void evalable :: error(valarray<double>& v) {
	v=error_v[error_v.size()-1];
}

void evalable :: variance(valarray<double>& v) {
	v=error_v[error_v.size()-1];
	v*=v;
	v*=2.;
}

void evalable :: autocorrelationtime(valarray<double>& v) {
	v=error_v[error_v.size()-1];
	v/=error_v[0];
	v*=v;
	v*=0.5;
}

double evalable :: mean(uint j) {
	return mean_v[0][j];
}

double evalable :: error(uint j) {
	return error_v[error_v.size()-1][j];
}

double evalable :: naiveerror(uint j) {
         return error_v[0][j];
}


double evalable :: variance(uint j) {
	return 2.*pow(error_v[error_v.size()-1][j],2.);
}

double evalable :: autocorrelationtime(uint j) {
	return 0.5*pow(error_v[error_v.size()-1][j]/error_v[0][j],2.);
}

void evalable :: get_statistics(ostream& os)
{
	streamsize origprec=os.precision();
	for (uint j=0;j<vector_length_;++j) {
 		os << "Name = " << name_;
		if (vector_length_>1) os<<"["<<j<<"]";
		os<<"\n";
 		os << "Bins = " << bins() << "\n";
 		os << "BinLength = " << bin_length() << "\n";
 		os.precision((int)(fabs((log(error_v[error_v.size()-1][j])/log(10.))))+3);
 		os << "Mean  = " << mean_v[0][j]<<"\n";
 		os.precision(3);
 		os << "Error = " << error_v[error_v.size()-1][j] <<"\n";
 		os << "Variance = " << pow(error_v[error_v.size()-1][j],2.)*bins() <<"\n";
// not done 	os << "Autocorrelationtime = " << 0.5*pow(error_v[error_v.size()-1][j]/error_v[0][j],2.) <<"\n";
// 		luint length=1;
// 		luint n=bins();
// 		for (uint i=0;i<error_v.size();++i) {
// 			os << "  ReBinLength = "<< length
// 			   << "  Bins = " << n;
// 			os.precision((int)(fabs((log(error_v[i][j])/log(10.))))+3);
// 			os << "  Mean = " << mean_v[i][j];
// 			os.precision(3);
// 			os << "  Error = "<< error_v[i][j] 
// 					<<"\n";
// 			length*=binning_base;
// 			n/=binning_base;
// 		}
	}	
 	os.precision(origprec);

}

void evalable :: jackknife(vector<observable*>& v, 
		void (*f) (double&, vector <valarray<double>* >&)) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me(0.,1);
	valarray<double> jm(0.,1);
	valarray<double> ali(0.,1);
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali[0],jack);
		if (k) me+=ali; else {me=ali;}
	}
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(ali[0],jack);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali[0],jack);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}
}

void evalable :: jackknife(vector<observable*>& v, 
		void (*f) (double&, vector <valarray<double>* >&, double*),double* p) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me(0.,1);
	valarray<double> jm(0.,1);
	valarray<double> ali(0.,1);
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali[0],jack,p);
		if (k) me+=ali; else {me=ali;}
	}
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(ali[0],jack,p);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali[0],jack,p);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}
}

void evalable :: vectorjackknife(vector<observable*>& v, 
		void (*f) (valarray<double>&, vector <valarray<double>* >&)) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me;
	valarray<double> jm;
	valarray<double> ali;
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali,jack);
		if (k) me+=ali; else {me.resize(ali.size());me=ali;}
	}
	jm.resize(me.size());
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(ali,jack);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali,jack);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}

// 
// 	luint max_exp=luint(log(double(n))/log(double(binning_base))-7);
// 	luint max_l=luint(pow(double(binning_base),double(max_exp)));
// 	double* binneddata1 = new double[n/binning_base];
// 	luint length=binning_base;
// 	n/=binning_base;
// 	nmo=n-1;	
// 	luint index=0;
// 	for (luint j=0;j<n;++j)	{
// 		double ali1=0;
// 		for (luint k=0;k<binning_base;++k) {
// 			ali1+=o1->samples[index];
// 			++index;
// 		}
// 		binneddata1[j]=ali1/binning_base;		
// 	}
// 	sum1=0;
// 	for (uint j=0;j<=nmo;++j){
// 		sum1+=binneddata1[j];
// 	}
// 	double meanb=0;
// 	for (uint j=0;j<=nmo;++j){
// 		double J1=(sum1-binneddata1[j])/nmo;
// 		meanb+=f(J1);
// 	}
// 	meanb=n*f(sum1/n)-nmo*meanb/n;
// 	double varb=0;
// 	for (uint j=0;j<=nmo;++j){
// 		double J1=(sum1-binneddata1[j])/nmo;
// 		varb+=pow(f(J1)-meanb,2.);
// 	}
// 	varb*=((double)nmo)/((double)n);
// 	mean_v.push_back(meanb);
// 	error_v.push_back(sqrt(varb));
// 	while (length<max_l) {		
// 		length*=binning_base;
// 		n/=binning_base;
// 		nmo=n-1;	
// 		index=0;
// 		for (luint j=0;j<n;++j)	{
// 			double ali1=0;
// 			for (luint k=0;k<binning_base;++k) {
// 				ali1+=binneddata1[index];
// 				++index;
// 			}
// 			binneddata1[j]=ali1/binning_base;		
// 		}
// 		sum1=0;
// 		for (uint j=0;j<=nmo;++j){
// 			sum1+=binneddata1[j];
// 		}
// 		meanb=0;
// 		for (uint j=0;j<=nmo;++j){
// 			double J1=(sum1-binneddata1[j])/nmo;
// 			meanb+=f(J1);
// 		}
// 		meanb=n*f(sum1/n)-nmo*meanb/n;
// 		varb=0;
// 		for (uint j=0;j<=nmo;++j){
// 			double J1=(sum1-binneddata1[j])/nmo;
// 			varb+=pow(f(J1)-meanb,2.);
// 		}
// 		varb*=((double)nmo)/((double)n);
// 		mean_v.push_back(meanb);
// 		error_v.push_back(sqrt(varb));
// 		
// 	}
// 	error_=error_v[error_v.size()-1];
// 	autocorrelationtime_=0.5*pow(error_/error_v[0],2.);
// 	delete [] binneddata1;
}

void evalable :: vectorjackknife(vector<observable*>& v, 
		void (*f) (valarray<double>&, vector <valarray<double>* >&, double*),double* p) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me;
	valarray<double> jm;
	valarray<double> ali;
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali,jack,p);
		if (k) me+=ali; else {me.resize(ali.size());me=ali;}
	}
	jm.resize(me.size());
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(ali,jack,p);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(ali,jack,p);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}
}


void evalable :: jackknife(vector<observable*>& v, void* obj,
		void (*f) (void* ,double&, vector <valarray<double>* >&)) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me(0.,1);
	valarray<double> jm(0.,1);
	valarray<double> ali(0.,1);
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(obj,ali[0],jack);
		if (k) me+=ali; else {me=ali;}
	}
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(obj,ali[0],jack);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(obj,ali[0],jack);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}
}

void evalable :: vectorjackknife(vector<observable*>& v, void* obj,
		void (*f) (void*, valarray<double>&, vector <valarray<double>* >&)) 
{
 	mean_v.clear();
 	error_v.clear();
	bins_=v[0]->bins();
	for (uint i=0;i<v.size();++i) if (v[i]->bins()<bins_) bins_=v[i]->bins();
	bin_length_=v[0]->bin_length();
	binning_base=v[0]->binning_base();
	luint n=bins_;
	luint nmo=n-1;
	vector<valarray<double>* > sum;
	vector<valarray<double>* > jack;
	for (uint i=0;i<v.size();++i) {
		valarray<double>* nsump=new valarray<double>(0.,v[i]->vector_length());
		valarray<double>* njackp=new valarray<double>(0.,v[i]->vector_length());	
		sum.push_back(nsump);
		jack.push_back(njackp);
	}
	for (uint i=0;i<v.size();++i)
		for (uint k=0;k<n;++k) {
			*sum[i]+=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
		}
	valarray<double> me;
	valarray<double> jm;
	valarray<double> ali;
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(obj,ali,jack);
		if (k) me+=ali; else {me.resize(ali.size());me=ali;}
	}
	jm.resize(me.size());
	jm=me;
	jm/=static_cast<double>(n);
	for (uint i=0;i<v.size();++i) {
		*jack[i]=*sum[i];
		*jack[i]/=static_cast<double>(n);
	}
	f(obj,ali,jack);
	for (uint j=0;j<me.size();++j)
		me[j]=n*ali[j]-nmo*me[j]/n;
	mean_v.push_back(me);
	vector_length_=me.size();
	for (uint k=0;k<n;++k) {
		for (uint i=0;i<v.size();++i) {
			*jack[i]=*sum[i];
			*jack[i]-=v[i]->samples[slice(k*(v[i]->vector_length()),v[i]->vector_length(),1)];
			*jack[i]/=static_cast<double>(nmo);
		}
		f(obj,ali,jack);
		ali-=jm;
		ali*=ali;
		if (k) me+=ali; else me=ali;
	}	
	me*=static_cast<double>(nmo)/static_cast<double>(bins_);
	me=sqrt(me);
	error_v.push_back(me);
	for (uint i=0;i<v.size();++i) {
		delete sum[i];
		delete jack[i];
	}
}

string evalable::toString() {
	std::stringstream ss;

	get_statistics(ss);
/*	ss << "name: " << name_ << "\n";
	ss << "bins: " << bins_ << "\n";
	ss << "vector length: " << vector_length_ << "\n";
	ss << "bin length: " << bin_length_ << "\n";
	ss << "binning base: " << binning_base << "\n";
	ss << "--------------------\n";
	for (size_t i = 0; i < mean_v.size(); ++i) {
		valarray<double> *v = &mean_v[i],
				*e = &error_v[i];
		for (size_t j = 0; j < v->size(); ++j)
			ss << (*v)[j] << "\t" << (*e)[j] << "\n";
	}*/
	return ss.str();
}

