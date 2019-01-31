#include <functional>
#include <string>
#include "merger.h"
#include "mc.h"
#include "evalable.h"
#include "measurements.h"
#include "dump.h"

#include <vector>
#include <fmt/format.h>

void merger::add_evalable(const std::string& name, const std::vector<std::string>& used_observables, evalable::func func) {
	// evalable names also should be valid HDF5 paths
	if(not measurements::observable_name_is_legal(name)) {
		throw std::runtime_error{fmt::format("illegal evalable name '{}': must not contain . or /", name)};
	}

	evalables_.emplace_back(name, used_observables, func);
}

results merger::merge(const std::vector<std::string>& filenames, size_t rebinning_bin_count) {
	results res;

	// This thing reads the complete time series of an observable which will
	// probably make it the biggest memory user of load leveller. But since
	// it’s only one observable at a time, it is maybe still okay.

	// If not, research a custom solution using fancy HDF5 virtual datasets or something.

	// In the first pass we gather the metadata to decide on the rebinning_bin_length.
	for(auto& filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(const auto& obs_name : g) {
			res.observables.try_emplace(obs_name);
			auto& obs = res.observables[obs_name];
			obs.name = obs_name;

			size_t vector_length;
			std::vector<double> samples;
			std::cerr << obs_name << "\n";

			auto obs_group = g.open_group(obs_name);
			obs_group.read("bin_length", obs.internal_bin_length);
			obs_group.read("vector_length", vector_length);
			int sample_size = obs_group.get_extent("samples");
			obs_group.read("samples", samples);

			if(sample_size % vector_length != 0) {
				throw std::runtime_error{"merge: sample count is not an integer multiple of the vector length. Corrupt file?"};
			}

			obs.total_sample_count += sample_size/vector_length;
			obs.mean.resize(vector_length);
			obs.error.resize(vector_length);
			obs.autocorrelation_time.resize(vector_length);

		}
	}
	
	for(auto& entry : res.observables) {
		auto& obs = entry.second;
		obs.rebinning_means.reserve(rebinning_bin_count*obs.mean.size());
		obs.rebinning_bin_length = obs.total_sample_count/rebinning_bin_count;
		obs.rebinning_bin_count = rebinning_bin_count;
	}

	size_t samples_processed = 0;
	for(auto& filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(const auto& obs_name : g) {
			std::vector<double> samples;
			auto& obs = res.observables[obs_name];
			obs.name = obs_name;
			
			g.read(fmt::format("{}/samples", obs_name), samples);

			// rebinning_bin_count*rebinning_bin_length may be smaller than
			// total_sample_count. In that case, we throw away the leftover samples.
			// 
			for(size_t i = 0; samples_processed < obs.rebinning_bin_count*obs.rebinning_bin_length and i < samples.size(); i++) {
				size_t vector_length = obs.mean.size();
				size_t vector_idx = i%vector_length;

				obs.mean[vector_idx] += samples[i];
				samples_processed++;
			}
		}
	}

	struct obs_rebinning_metadata {
		size_t current_rebin = 0;
		size_t current_rebin_filling = 0;
	};

	std::map<std::string, obs_rebinning_metadata> metadata;
	for(auto& entry : res.observables) {
		auto& obs = entry.second;
		for(size_t i = 0; i < obs.mean.size(); i++) {
			obs.mean[i] /= obs.total_sample_count;
		}

		metadata.emplace(obs.name, obs_rebinning_metadata{});
	}
	
	// now handle the error and autocorrelation time which are calculated by rebinning.
	for(auto& filename : filenames) {
		iodump meas_file = iodump::open_readonly(filename);
		auto g = meas_file.get_root();
		for(const auto& obs_name : g) {
			std::vector<double> samples;
			auto& obs = res.observables.at(obs_name);
			auto& obs_meta = metadata.at(obs_name);

			size_t vector_length = obs.mean.size();
			
			g.read(fmt::format("{}/samples", obs_name), samples);

			for(size_t i = 0; obs_meta.current_rebin < rebinning_bin_count and i < samples.size(); i++) {
				size_t vector_idx = i%vector_length;
				size_t rebin_idx = obs_meta.current_rebin*vector_length + vector_idx;

				obs.rebinning_means[rebin_idx] += samples[vector_idx];
				
				obs_meta.current_rebin_filling++;

				// I used autocorrelation_time as a buffer here to hold the naive no-rebinning error (sorry)
				obs.autocorrelation_time[vector_idx] += (samples[i]-obs.mean[vector_idx])*(samples[i]-obs.mean[vector_idx]);

				if(obs_meta.current_rebin_filling >= obs.rebinning_bin_length) {
					obs.rebinning_means[rebin_idx] /= obs.rebinning_bin_length;
					
					double diff = obs.rebinning_means[rebin_idx] - obs.mean[vector_idx];
					obs.error[vector_idx] += diff*diff;

					obs_meta.current_rebin++;
					obs.rebinning_means.resize(obs_meta.current_rebin*obs.mean.size(), 0);
					obs_meta.current_rebin_filling = 0;

				}
			}
		}
	}
	
	for(auto& entry : res.observables) {
		auto& obs = entry.second;
		for(size_t i = 0; i < obs.error.size(); i++) {
			double no_rebinning_error = sqrt(obs.autocorrelation_time[i]/(obs.total_sample_count-1)/obs.total_sample_count);

			obs.error[i] = sqrt(obs.error[i]/(obs.rebinning_bin_count-1)/(obs.total_sample_count/static_cast<double>(obs.rebinning_bin_length)));

			obs.autocorrelation_time[i] = 0.5*pow(no_rebinning_error/obs.error[i],2);
		}
	}

	evaluate_evalables(res);

	return res;
}

void merger::evaluate_evalables(results& res) {
	std::vector<observable_result> evalable_results;
	for(auto &eval : evalables_) {
		evalable_results.emplace_back();
		
		eval.jackknife(res,evalable_results.back());
	}

	for(auto &eval : evalable_results) {
		res.observables.emplace(eval.name, eval);
	}
}
/*
int merge(function<abstract_mc* (string&)> mccreator, int argc, char* argv[]) {
	MPI_Init(&argc,&argv);
        if (argc==1) {
                cout << " usage: "<< argv[0] <<" jobfilename [-s number_of_bins_to_be_skipped] [-t min_task max_task] [-r min_run max_run]" << endl;
                exit(1);
        }
        std::string jobfile(argv[1]);
        int task_min=1;
        int task_max=-1;
        int run_min=1;
        int run_max=-1;
        int skipbins=0;
        int rarg=2;
        while (argc>rarg) {
		if (argv[rarg][0]=='s' || argv[rarg][1]=='s') {
			rarg++;
			skipbins=atoi(argv[rarg]);
			rarg++;
		}
		else if (argv[rarg][0]=='t' || argv[rarg][1]=='t') {
			rarg++;
			task_min=atoi(argv[rarg]);
			rarg++;
			task_max=atoi(argv[rarg]);
			rarg++;
		}
		else if (argv[rarg][0]=='r' || argv[rarg][1]=='r') {
			rarg++;
			run_min=atoi(argv[rarg]);
			rarg++;
			run_max=atoi(argv[rarg]);
			rarg++;
		}
		else {
			cout<<"unknown option "<<argv[rarg]<<endl;
			exit(1);
		}
	};
	cout <<"Merging "<<jobfile;
	if (task_max==-1) {
		cout << " all task";
	}
        else {
                cout <<" task " <<task_min << " to " << task_max;
        }
        if (run_max==-1) {
                cout << " all runs";
        }
        else {
                cout <<" run " <<run_min << " to " << run_max;
        }
        if (skipbins) cout << " skipping the first " << skipbins <<" bins";
        cout << endl;

        parser parsedfile(jobfile+".alltasks");
        std::vector<string> taskfiles;
        taskfiles= parsedfile.return_vector< string >("@taskfiles");
        std::string masterfile = parsedfile.value_or_default<string>("masterfile",jobfile+".master");
        if (task_max==-1) task_max=taskfiles.size();
        for (int i=task_min-1;i<task_max;++i) {
                std::string taskfile = taskfiles[i];
                parser cfg(taskfile);
                std::string taskdir = cfg.value_of("taskdir");
                std::stringstream rb; rb << taskdir << "/run" << run_min << ".";
                std::string rundir = rb.str();
                abstract_mc* sys = mccreator(taskfile);
                if ((*sys)._read(rundir)) {
                        if ((*sys).measure.merge(rundir,skipbins)) cout << rundir <<endl;
                        if (1) {
                                int run_counter=run_min+1;
                                bool success=true;
                                while (success && ((run_max==-1) || (run_counter<=run_max))) {
                                        stringstream b;b<<taskdir<<"/run"<<run_counter<<".";
                                        success=(*sys).measure.merge(b.str(),skipbins);
                                        if (success) cout << b.str() <<endl;
                                        ++run_counter;
                                }
                                std::stringstream mfb;
                                if (run_max==-1) {
                                         mfb << taskdir << ".";
                                }
                                else {
                                        if (run_min==run_max) {
                                                mfb << taskdir <<"."<<run_min<<".";
                                        }
                                        else {
                                                mfb << taskdir <<"."<<run_min<<"."<<run_max<<".";
                                        }
                                }
                                if (skipbins) mfb <<"s"<<skipbins<<".";
                                mfb <<"out";
                                (*sys)._write_output(mfb.str());
                        }
                }
                delete sys;
        }
        return 0;
}
*/
