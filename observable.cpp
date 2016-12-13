#include "observable.h"
#include <sstream>

observable :: observable()
{
	init("",1,1,1000,2,0);	
}

observable :: observable(std::string _name)
{
	init(_name,1,1,1000,2,0);
}

observable :: observable(std::string _name, luint _vector_length)
{
	init(_name,_vector_length,1,1000,2,0);
}

observable :: observable(std::string _name, luint _vector_length, luint _bin_length)
{
	init(_name,_vector_length,_bin_length,1000,2,0);
}

observable :: observable(std::string _name, luint _vector_length, luint _bin_length, luint _initial_length)
{
	init(_name,_vector_length,_bin_length,_initial_length,2,0);
}

observable :: observable(std::string _name, luint _vector_length, luint _bin_length, luint _initial_length, luint _resizefactor, luint _resizeoffset)
{
	init(_name,_vector_length,_bin_length,_initial_length,_resizefactor,_resizeoffset);
}

void observable :: init(std::string _name, luint _vector_length, luint _bin_length, luint _initial_length, luint _resizefactor, luint _resizeoffset)
{
	name_=_name;
	vector_length_ = _vector_length;
	bin_length_ = _bin_length;
	initial_length = _initial_length;
	length = initial_length;
	samples.resize(length*vector_length_,0.);
	resizefactor=_resizefactor;
	resizeoffset=_resizeoffset;
	current_bin= 0;
	current_bin_filling=0;
	analysis_done=0;
	simple_analysis_done=0;
}

void observable :: extract(int j, observable& obs) 
{
	stringstream obsname;obsname<<name_<<"["<<j<<"]";  
	obs.name_= obsname.str();
	obs.bin_length_ = bin_length_;
	obs.initial_length = initial_length;
	obs.length=length;
	obs.current_bin=current_bin;
	obs.current_bin_filling=current_bin_filling;
	obs.analysis_done=0;
	obs.simple_analysis_done=0;
	obs.samples.resize(length);
	obs.samples=samples[slice(j,current_bin,vector_length_)];

}

void observable :: add(const valarray<double>& val)
{
	if ((current_bin==0) && (val.size()>vector_length_)) {
			vector_length_=val.size();
			samples.resize(length*vector_length_,0.);
	}
	samples[slice(current_bin*vector_length_,vector_length_,1)]+=val;
	++current_bin_filling;
	if (current_bin_filling >= bin_length_) //need to start a new bin next time
	{
		if (bin_length_>1) for (uint j=0;j<vector_length_;++j)
			samples[current_bin*vector_length_+j]/=bin_length_;
		if( current_bin == (length - 1)) //need to allocate more memory for a new chunk of bins
		{
			valarray<double> old=samples;
			samples.resize(resizefactor*old.size()+resizeoffset,0.);
			samples[slice(0,old.size(),1)]=old;
			length =resizefactor*length+resizeoffset;	
		}
		++current_bin;
		current_bin_filling=0;
	}
}


void observable :: rebin(int new_bin_length) {
	int n_group=new_bin_length/bin_length_; //number of previous bins that make up a new bin
	int new_number_of_full_bins=(current_bin-1)/n_group;
	double rescale=double(n_group)/double(bin_length_);
	for (int i=0;i<new_number_of_full_bins;++i) {
		if (i) samples[slice(i*vector_length_,vector_length_,1)]=samples[slice(i*n_group*vector_length_,vector_length_,1)];
		for (int j=1;j<n_group;++j)
		 	samples[slice(i*vector_length_,vector_length_,1)]+=samples[slice((i*n_group+j)*vector_length_,vector_length_,1)];
		for (uint k=i*vector_length_;k<(i+1)*vector_length_;++k) 
			samples[k]/=rescale;
	}			
	samples[slice(new_number_of_full_bins*vector_length_,vector_length_,1)]=samples[slice(current_bin*vector_length_,vector_length_,1)];
	for (uint i=new_number_of_full_bins*n_group;i<current_bin;++i) {
		for (uint k=i*vector_length_;k<(i+1)*vector_length_;++k)
			samples[k]*=bin_length_;
		samples[slice(new_number_of_full_bins*vector_length_,vector_length_,1)]+=samples[slice(i*vector_length_,vector_length_,1)];
		current_bin_filling+=bin_length_;
	}
	current_bin=new_number_of_full_bins;
	bin_length_=new_bin_length;
}
			


void observable :: get_statistics(ostream& os) {
#ifdef GET_STATISTICS_COUT
        cout <<" "<< name_ <<" with "<< bins() <<" bins "<<endl;
#endif	
	if (bins()==0) {//there is no full bin, so we just return the mean of the data collected thus far.
		for (uint j=0;j<vector_length_;++j) {
                	os << "Name = " << name_;
                	if (vector_length_>1) os<<"["<<j<<"]";
                	os<<"\n";
                	os << "Bins = " << 1 << "\n";
                	os << "BinLength = " << current_bin_filling << "\n";
                	os << "Mean  = " << samples[j]/current_bin_filling<<"\n";
			double dummy=0.;
                	os << "Error = " << 0./dummy <<"\n";
                	os << "Variance = " << 0./dummy <<"\n";
                	os << "Autocorrelationtime = " << 0./dummy <<"\n";
		}
	}
	else {
	streamsize origprec=os.precision();
 	if (!analysis_done) analyse();	
	for (uint j=0;j<vector_length_;++j) {
 		os << "Name = " << name_;
		if (vector_length_>1) os<<"["<<j<<"]";
		os<<"\n";
 		os << "Bins = " << bins() << "\n";
 		os << "BinLength = " << bin_length() << "\n";
 		//os.precision((int)(fabs((log(error_v[error_v.size()-1][j])/log(10.))))+3);
		os.precision(calcprec(origprec,mean_v[0][j],error_v[error_v.size()-1][j]));	
 		os << "Mean  = " << mean_v[0][j]<<"\n";
 		os.precision(3);
 		os << "Error = " << error_v[error_v.size()-1][j] <<"\n";
 		os << "Variance = " << pow(error_v[error_v.size()-1][j],2.)*bins() <<"\n";
 		os << "Autocorrelationtime = " << 0.5*pow(error_v[error_v.size()-1][j]/error_v[0][j],2.) <<"\n";
 		luint length=1;
 		luint n=bins();
 		for (uint i=0;i<error_v.size();++i) {
 			os << "  ReBinLength = "<< length
 			   << "  Bins = " << n;
 			//os.precision((int)(fabs((log(error_v[i][j])/log(10.))))+3);
			os.precision(calcprec(origprec,mean_v[0][j],error_v[error_v.size()-1][j]));	
 			os << "  Mean = " << mean_v[i][j];
 			os.precision(3);
 			os << "  Error = "<< error_v[i][j] 
 					<<"\n";
 			length*=binning_base();
 			n/=binning_base();
 		}
	}	
 	os.precision(origprec);

	}
}

void observable :: mean(valarray<double>& v) {
	v.resize(vector_length_);
	if (!simple_analysis_done) simple_analyse(); 
	v=mean_v[0];
}

void observable :: error(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_v[error_v.size()-1];
}

void observable :: naiveerror(valarray<double>& v) {
	v.resize(vector_length_);
        if (!simple_analysis_done) simple_analyse();
        v=error_v[0];
}

void observable :: variance(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_v[error_v.size()-1];
	v*=v;
	v*=2.;
}

void observable :: autocorrelationtime(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_v[error_v.size()-1];
	v/=error_v[0];
	v*=v;
	v*=0.5;
}

void observable :: timeseries(uint j,valarray<double>& v) {
	v.resize(current_bin);
	v=samples[slice(j,current_bin,vector_length_)];
}

double observable :: mean(uint j) {
	if (!simple_analysis_done) simple_analyse(); 
	return mean_v[0][j];
}

double observable :: error(uint j) {
	if (!analysis_done) analyse();
	return error_v[error_v.size()-1][j];
}

double observable :: naiveerror(uint j) {
         if (!simple_analysis_done) simple_analyse();
         return error_v[0][j];
}

double observable :: variance(uint j) {
	if (!analysis_done) analyse();
	return 2.*pow(error_v[error_v.size()-1][j],2.);
}

double observable :: autocorrelationtime(uint j) {
	if (!analysis_done) analyse();
	return 0.5*pow(error_v[error_v.size()-1][j]/error_v[0][j],2.);
}

void observable :: simple(){
        mean_v.clear();
        error_v.clear();
        valarray<double> meanb(0.,vector_length_);
        luint n=bins();
        for (luint i=0;i<n;++i)
                meanb+=samples[slice(i*vector_length_,vector_length_,1)];
        meanb/=n;
        mean_v.push_back(meanb);
        valarray<double> errb(0.,vector_length_);
        for (luint i=0;i<n;++i)
                for (uint j=0;j<vector_length_;++j) {
                        double diff=samples[j+i*vector_length_]-meanb[j];
                        errb[j]+=diff*diff;
                }
        errb/=n-1;
        errb/=n;
        errb=sqrt(errb);
        error_v.push_back(errb);
}


void observable :: binning(){
	mean_v.clear();
	error_v.clear();
	valarray<double> meanb(0.,vector_length_);
	luint length=1;
	luint n=bins();
	for (luint i=0;i<n;++i)
		meanb+=samples[slice(i*vector_length_,vector_length_,1)];
  	meanb/=n;
	mean_v.push_back(meanb);
	valarray<double> errb(0.,vector_length_);
 	for (luint i=0;i<n;++i)
		for (uint j=0;j<vector_length_;++j) {
                        double diff=samples[j+i*vector_length_]-meanb[j];
                        errb[j]+=diff*diff;
                }
 	errb/=n-1;
 	errb/=n;
	errb=sqrt(errb);
 	error_v.push_back(errb);
	// now comes the rebinning
	valarray<double> binneddata(0.,vector_length_*bins()/binning_base());
	valarray<double> ali(0.,vector_length_);
	int is_first_rebinning=1;
	while (n>=min_number_of_bins()*binning_base()) {
 		length*=binning_base();
 		n/=binning_base();
		luint index=0;	
 		meanb=0.;
  		for (luint i=0;i<n;++i)	{
  			ali=0.;
  			for (luint k=0;k<binning_base();++k) {
 	 			if (is_first_rebinning) ali+=samples[slice(index*vector_length_,vector_length_,1)];
				else ali+=binneddata[slice(index*vector_length_,vector_length_,1)];
  				++index;
  			}
 			ali/=binning_base();
  			binneddata[slice(i*vector_length_,vector_length_,1)]=ali;
  			meanb+=ali;
  		}
  		meanb/=n;
		mean_v.push_back(meanb);
		errb=0.;
 		for (luint i=0;i<n;++i)
			for (uint j=0;j<vector_length_;++j) {
                	        double diff=binneddata[j+i*vector_length_]-meanb[j];
                        	errb[j]+=diff*diff;
                	}
 		errb/=n-1;
 		errb/=n;
		errb=sqrt(errb);
 		error_v.push_back(errb);
		is_first_rebinning=0;
	}	
}


void observable :: reset()
{
	length = initial_length;
	samples.resize(length*vector_length_,0);
	current_bin= 0;
	current_bin_filling=0;
	mean_v.clear();
	error_v.clear();
	analysis_done=0;	
	simple_analysis_done=0;
}

void observable :: write(odump& d)
{
	d.write(name_);
	d.write(vector_length_);
	d.write(bin_length_);	
	d.write(length);
	d.write(initial_length);
	d.write(current_bin);
	d.write(current_bin_filling);
	d.write(&samples[0],(current_bin+1)*vector_length_);	
}

bool observable :: read(idump& d, int skipbins)
{
	if (!d) 
		return false;
	else {
		d.read(name_);
		d.read(vector_length_);
		d.read(bin_length_);	
		d.read(length);
		d.read(initial_length);
		d.read(current_bin);
		d.read(current_bin_filling);
		if (skipbins) {
			if (skipbins>current_bin) skipbins=current_bin;
			d.ignore<double>(skipbins*vector_length_);
			samples.resize(length*vector_length_,0);
			d.read(&samples[0],(current_bin+1-skipbins)*vector_length_);
			current_bin-=skipbins;
		}
		else {	
			samples.resize(length*vector_length_,0);
			d.read(&samples[0],(current_bin+1)*vector_length_);	
		}
		analysis_done=0;
		simple_analysis_done=0;
		return true;
	}
}

bool observable :: merge(idump& d, int skipbins, int check_bin_length)
{
	if (!d)
		return false;
	else {
 		std::string namem;
 		d.read(namem);
		luint vector_lengthm;
		d.read(vector_lengthm);
 		luint bin_lengthm;
 		d.read(bin_lengthm);
 		if (namem==name_ && (!check_bin_length || (bin_lengthm==bin_length_)) ) {
			if (!current_bin && vector_lengthm>vector_length_) {
                        	vector_length_=vector_lengthm;
                        	samples.resize(length*vector_length_,0.);
        		}
 			luint lengthm;	
 			d.read(lengthm);
 			luint initial_lengthm;
 			d.read(initial_lengthm);	
 			luint current_binm;
 			d.read(current_binm);
 			luint current_bin_fillingm;
 			d.read(current_bin_fillingm);
		        valarray<double> old=samples;
			if (skipbins) {
				if (skipbins>current_binm) skipbins=current_binm;
				d.ignore<double>(skipbins*vector_length_);
				length+=current_binm-skipbins;
				samples.resize(length*vector_length_,0);
				d.read(&samples[0],(current_binm-skipbins)*vector_length_);
				samples[slice((current_binm-skipbins)*vector_length_,old.size(),1)]=old;
				current_bin+=current_binm-skipbins;
			}
			else {
			 	length+=current_binm;
				samples.resize(length*vector_length_,0);
				if (current_binm)
					d.read(&samples[0],current_binm*vector_length_);
				samples[slice(current_binm*vector_length_,old.size(),1)]=old;
	 			current_bin+=current_binm;
			}
			d.ignore<double>(vector_lengthm);//the last bin from file 
 			analysis_done=0;
			simple_analysis_done=0;
 		}
 		else {
 			cerr << "observable merge not possible: "<< namem << " to " << name_<< endl;
			return false;
		}
		return true;
	}	
}



void observable :: write_appendmode(odump& d,odump& dd)
{
	d.write(name_);
	d.write(vector_length_);
	d.write(bin_length_);	
	d.write(initial_length);
	d.write(current_bin_filling);
	d.write(&samples[current_bin*vector_length_],vector_length_);
	dd.write(bin_length_);
	dd.write(vector_length_);
        dd.write(current_bin);
        if (current_bin) {//we save all the full bins to the file dd in append mode 
		dd.write(&samples[0],(current_bin)*vector_length_);
		for (uint i=0;i<vector_length_;++i) 
			samples[i]=samples[current_bin*vector_length_+i];
		for (uint i=vector_length_;i<(current_bin+1)*vector_length_;++i) 
			samples[i]=0;
		current_bin=0;	
	}
}


bool observable :: read_appendmode(idump& d)
{       //here, we read in only the current bin, not all previous bins
        //to access the full data, use merge, which can be used to read in all previous bins from all chunks
	if (!d) return false;
	d.read(name_);
	d.read(vector_length_);
	d.read(bin_length_);
	d.read(initial_length);
	d.read(current_bin_filling);
	length=initial_length;
	samples.resize(length*vector_length_,0);
	d.read(&samples[0],vector_length_);	
	current_bin=0;
	analysis_done=0;
	simple_analysis_done=0;
	return true;
}

bool observable :: merge_appendmode(idump& dd, int& skipbins, int check_bin_length)
{
	if (!dd)
		return false;
	else {
 		luint bin_lengthm;
 		dd.read(bin_lengthm);
		luint vector_lengthm;
		dd.read(vector_lengthm);
 		if (!check_bin_length || (bin_lengthm==bin_length_) ) {
			if (!current_bin && vector_lengthm>vector_length_) {
                        	vector_length_=vector_lengthm;
                        	samples.resize(length*vector_length_,0.);
        		}
 			luint current_binm;
 			dd.read(current_binm);
			if (skipbins) {
				if (skipbins>=current_binm) {
					dd.ignore<double>(current_binm*vector_length_);
					skipbins-=current_binm;					
				}
				else {
					dd.ignore<double>(skipbins*vector_length_);
					valarray<double> old=samples;
					length+=current_binm-skipbins;
                                	samples.resize(length*vector_length_,0);
					if (current_bin)
						samples[slice(0,current_bin*vector_length_,1)]=old[slice(0,current_bin*vector_length_,1)];
                                	dd.read(&samples[current_bin*vector_length_],(current_binm-skipbins)*vector_length_);
					samples[slice((current_binm-skipbins)*vector_length_,vector_length_,1)]=old[slice(current_bin*vector_length_,vector_length_,1)];
                                	current_bin+=current_binm-skipbins;
					skipbins=0;
				}
			}
			else {	
 				length+=current_binm;
		        	valarray<double> old=samples;
				samples.resize(length*vector_length_);
				if (current_bin) 
					samples[slice(0,current_bin*vector_length_,1)]=old[slice(0,current_bin*vector_length_,1)];
				dd.read(&samples[current_bin*vector_length_],current_binm*vector_length_);
				samples[slice((current_bin+current_binm)*vector_length_,vector_length_,1)]=old[slice(current_bin*vector_length_,vector_length_,1)];
				current_bin+=current_binm;
			}
 			analysis_done=0;
			simple_analysis_done=0;
 		}
 		else {
 			cerr << "observable merge not possible: "<< name_<< endl;
			return false;
		}
		return true;
	}	
}






