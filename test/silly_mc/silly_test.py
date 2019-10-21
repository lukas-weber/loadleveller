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

shutil.rmtree('silly_job.data', ignore_errors=True)
proc = subprocess.run(['valgrind', args.silly_mc, 'single', args.test_param_file])
if proc.returncode != 0:
    sys.exit(1)

with open('silly_job.results.json', 'r') as f:
    results = json.load(f)
with open(args.test_param_file, 'r') as f:
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

sys.exit(failcount)
