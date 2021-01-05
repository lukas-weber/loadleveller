#!/usr/bin/env python3

import os
import json
import subprocess
import argparse
import sys
import shutil
import h5py
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('silly_mc')
parser.add_argument('test_param_file')
args = parser.parse_args()

jobdir = 'silly_job.data'
shutil.rmtree(jobdir, ignore_errors=True)
os.makedirs(jobdir)

param_file = jobdir + '/parameters.json'

shutil.copy(args.test_param_file, param_file)

proc = subprocess.run(['valgrind', args.silly_mc, 'single', param_file])
if proc.returncode != 0:
    sys.exit(1)

# inject some corrupt observables

measfile = h5py.File(jobdir + '/task0001/run0001.meas.h5', 'a')
corrupt = measfile.create_group('/Corrupt')
corrupt['samples'] = [1,3,3,7.1337]
corrupt['vector_length'] = [0]
corrupt = measfile.create_group('/Corrupt2')
corrupt['samples'] = [1,3,3,7.1337]
corrupt['vector_length'] = [5]
corrupt['bin_length'] = [5]
measfile.close()

proc = subprocess.run(['valgrind', args.silly_mc, 'merge', param_file])
if proc.returncode != 0:
    sys.exit(1)

# read results and compare

with open('silly_job.results.json', 'r') as f:
    results = json.load(f)
with open(param_file, 'r') as f:
    rebin_size = json.load(f)['jobconfig']['merge_rebin_length']

sweeps = results[0]['parameters']['sweeps']
therm = results[0]['parameters']['thermalization']
binsize = results[0]['parameters']['binsize']*rebin_size
nbin = sweeps//binsize

# based on analytical expressions for Σi, Σi²

mean = (binsize*nbin)*(binsize*nbin+2*therm-1)/2/(nbin*binsize)

binmean2 = 1/12 * (3-12*therm+12*therm**2-binsize**2-6*binsize*nbin+12*therm*binsize*nbin+4*binsize**2*nbin**2)
    
error = (1/(nbin-1)*(binmean2 - mean**2))**0.5

reference = {
    'MagicNumber': {
        'mean' : mean,
        'error' : error
    }
}

# TODO autocorrelation time and evalables

failcount = 0

for obsname, obs in reference.items():
    for entry, val in obs.items():
        mcval = results[0]['results'][obsname][entry][0]
        if not np.isclose(val, mcval):
            print('{}/{}: {} != {}'.format(obsname, entry, val, mcval))
            failcount += 1

# check if seed was saved
dump = h5py.File('silly_job.data/task0001/run0001.dump.h5','r')
seed = dump['/random_number_generator/seed'][0]
seed_param = results[0]['parameters']['seed']
if seed != seed_param:
    failcount += 1
    print('seed was not saved!')

sys.exit(failcount)

