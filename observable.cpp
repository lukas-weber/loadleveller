#include "observable.h"
#include <fmt/format.h>

observable::observable(std::string name, size_t vector_length, size_t bin_length, size_t initial_length)
	: name_{name},
	bin_length_{bin_length},
	vector_length_{vector_length},
	initial_length_{initial_length},
	current_bin_{0},
	current_bin_filling_{0}
{
	samples_.reserve(vector_length_*initial_length_);
}


template <class T>
void observable::add(T val) {
	std::vector<T> v = {val};
	add(v);
}

const std::string& observable::name() const {
	return name_;
}

template <class T> 
void observable::add(const std::vector<T>& val) {
	// handle wrong vector length gracefully on first add
	if(current_bin_ == 0 and current_bin_filling_ == 0) {
		vector_length_=val.size();
		samples_.resize((current_bin_+1)*vector_length_,0.);
	} else if(vector_length_ != val.size()) {
		throw std::runtime_error{fmt::format("observable::add: added vector has different size ({}) than what was added before ({})", val.size(), vector_length_)};
	}
		
	for(size_t j=0; j < vector_length_; ++j) 
		samples_[j+current_bin_*vector_length_] += static_cast<double>(val[j]);
	current_bin_filling_++;
	
	if(current_bin_filling_ == bin_length_) { //need to start a new bin next time
		if (bin_length_>1)
			for(size_t j = 0; j < vector_length_; ++j)
				samples_[current_bin_*vector_length_+j] /= bin_length_;
		current_bin_++;
		samples_.resize((current_bin_+1)*vector_length_);
		current_bin_filling_=0;
	}
}

void observable::checkpoint_write(iodump& dump_file) const {
	// The plan is that before checkpointing, all complete bins are written to the measurement file.
	// Then only the incomplete bin remains and we write that into the dump to resume
	// the filling process next time.
	// So if current_bin_ is not 0 here, we have made a mistake.
	assert(current_bin_ != 0);
		
	dump_file.write("name", name_);
	dump_file.write("vector_length", vector_length_);
	dump_file.write("bin_length", bin_length_);	
	dump_file.write("initial_length", initial_length_);
	dump_file.write("current_bin_filling", current_bin_filling_);
	dump_file.write("samples", samples_);
}


void observable::measurement_write(iodump& meas_file) {
	std::vector<double> current_bin_value(samples_.end()-vector_length_, samples_.end());
	samples_.resize(current_bin_*vector_length_);

	meas_file.write("vector_length", vector_length_);
	meas_file.write("bin_length", bin_length_);
	meas_file.insert_back("samples", samples_);

	samples_ = current_bin_value;
	samples_.reserve(initial_length_*vector_length_);
	current_bin_ = 0;
}

void observable::checkpoint_read(iodump& d) {
	d.read("name", name_);
	d.read("vector_length", vector_length_);
	d.read("bin_length", bin_length_);	
	d.read("initial_length", initial_length_);
	d.read("current_bin_filling", current_bin_filling_);
	d.read("samples", samples_);
	current_bin_ = 0;
}

/*static size_t calcprec(streamsize origprec, double m, double e) {
	if (fabs(e)>1e-200) {
		if (fabs(m/e)>1e-200) 
			return (streamsize)(3. + 0.43429448*log(fabs(m/e)));
		else
			return 3;
	}
	return (origprec>16) ? origprec: 16;
}
void observable::get_statistics(ostream& os) {
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
	size_t origprec=os.precision();
 	if (!analysis_done) analyse();	
	for (uint j=0;j<vector_length_;++j) {
 		os << "Name = " << name_;
		if (vector_length_>1) os<<"["<<j<<"]";
		os<<"\n";
 		os << "Bins = " << bins() << "\n";
 		os << "BinLength = " << bin_length() << "\n";
 		//os.precision((int)(fabs((log(error_vector_[error_vector_.size()-1][j])/log(10.))))+3);
		os.precision(calcprec(origprec,mean_vector_[0][j],error_vector_[error_vector_.size()-1][j]));	
 		os << "Mean  = " << mean_vector_[0][j]<<"\n";
 		os.precision(3);
 		os << "Error = " << error_vector_[error_vector_.size()-1][j] <<"\n";
 		os << "Variance = " << pow(error_vector_[error_vector_.size()-1][j],2.)*bins() <<"\n";
 		os << "Autocorrelationtime = " << 0.5*pow(error_vector_[error_vector_.size()-1][j]/error_vector_[0][j],2.) <<"\n";
 		luint length=1;
 		luint n=bins();
 		for (uint i=0;i<error_vector_.size();++i) {
 			os << "  ReBinLength = "<< length
 			   << "  Bins = " << n;
 			//os.precision((int)(fabs((log(error_vector_[i][j])/log(10.))))+3);
			os.precision(calcprec(origprec,mean_vector_[0][j],error_vector_[error_vector_.size()-1][j]));	
 			os << "  Mean = " << mean_vector_[i][j];
 			os.precision(3);
 			os << "  Error = "<< error_vector_[i][j] 
 					<<"\n";
 			length*=binning_base();
 			n/=binning_base();
 		}
	}	
 	os.precision(origprec);

	}
}

void observable::mean(valarray<double>& v) {
	v.resize(vector_length_);
	if (!simple_analysis_done) simple_analyse(); 
	v=mean_vector_[0];
}

void observable::error(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_vector_[error_vector_.size()-1];
}

void observable::naiveerror(valarray<double>& v) {
	v.resize(vector_length_);
        if (!simple_analysis_done) simple_analyse();
        v=error_vector_[0];
}

void observable::variance(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_vector_[error_vector_.size()-1];
	v*=v;
	v*=2.;
}

void observable::autocorrelationtime(valarray<double>& v) {
	v.resize(vector_length_);
	if (!analysis_done) analyse();
	v=error_vector_[error_vector_.size()-1];
	v/=error_vector_[0];
	v*=v;
	v*=0.5;
}

void observable::timeseries(uint j,valarray<double>& v) {
	v.resize(current_bin);
	v=samples[slice(j,current_bin,vector_length_)];
}

double observable::mean(uint j) {
	if (!simple_analysis_done) simple_analyse(); 
	return mean_vector_[0][j];
}

double observable::error(uint j) {
	if (!analysis_done) analyse();
	return error_vector_[error_vector_.size()-1][j];
}

double observable::naiveerror(uint j) {
         if (!simple_analysis_done) simple_analyse();
         return error_vector_[0][j];
}

double observable::variance(uint j) {
	if (!analysis_done) analyse();
	return 2.*pow(error_vector_[error_vector_.size()-1][j],2.);
}

double observable::autocorrelationtime(uint j) {
	if (!analysis_done) analyse();
	return 0.5*pow(error_vector_[error_vector_.size()-1][j]/error_vector_[0][j],2.);
}

void observable::simple_analyse(){
        mean_vector_.clear();
        error_vector_.clear();
        valarray<double> meanb(0.,vector_length_);
        luint n=bins();
        for (luint i=0;i<n;++i)
                meanb+=samples[slice(i*vector_length_,vector_length_,1)];
        meanb/=n;
        mean_vector_.push_back(meanb);
        valarray<double> errb(0.,vector_length_);
        for (luint i=0;i<n;++i)
                for (uint j=0;j<vector_length_;++j) {
                        double diff=samples[j+i*vector_length_]-meanb[j];
                        errb[j]+=diff*diff;
                }
        errb/=n-1;
        errb/=n;
        errb=sqrt(errb);
        error_vector_.push_back(errb);

        simple_analysis_done_ = true;
}


void observable::analyse(){
	mean_vector_.clear();
	error_vector_.clear();
	valarray<double> meanb(0.,vector_length_);
	luint length=1;
	luint n=bins();
	for (luint i=0;i<n;++i)
		meanb+=samples[slice(i*vector_length_,vector_length_,1)];
  	meanb/=n;
	mean_vector_.push_back(meanb);
	valarray<double> errb(0.,vector_length_);
 	for (luint i=0;i<n;++i)
		for (uint j=0;j<vector_length_;++j) {
                        double diff=samples[j+i*vector_length_]-meanb[j];
                        errb[j]+=diff*diff;
                }
 	errb/=n-1;
 	errb/=n;
	errb=sqrt(errb);
 	error_vector_.push_back(errb);
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
		mean_vector_.push_back(meanb);
		errb=0.;
 		for (luint i=0;i<n;++i)
			for (uint j=0;j<vector_length_;++j) {
                	        double diff=binneddata[j+i*vector_length_]-meanb[j];
                        	errb[j]+=diff*diff;
                	}
 		errb/=n-1;
 		errb/=n;
		errb=sqrt(errb);
 		error_vector_.push_back(errb);
		is_first_rebinning=0;
	}	

	analysis_done_ = true;
	simple_analysis_done = true;
}

bool observable::merge(idump& d int check_bin_length)
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
*/
