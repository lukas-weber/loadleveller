#!/bin/bash

mc=$1
testparams=$2

rm -rf silly_job_long.data

mpirun -np 3 $mc $testparams
if [ $? -ne 0 ]; then
	exit 1
fi

mpirun -np 3 $mc $testparams
if [ $? -ne 0 ]; then
	exit 1
fi

echo "removing observable from run0002..."
# this simulates progress differences in runs of the same task that may or may not yet know about different observables.
python3 - <<'END'
import h5py

file = h5py.File('silly_job_long.data/task0001/run0002.meas.h5', 'a')
del file['/MagicNumber']
file.close()
END

echo "trying to merge..."
$mc merge $testparams
if [ $? -ne 0 ]; then
	exit 1
fi

seed1=$(h5dump -d '/random_number_generator/seed' -O /dev/null silly_job_long.data/task0001/run0001.dump.h5)
seed2=$(h5dump -d '/random_number_generator/seed' -O /dev/null silly_job_long.data/task0001/run0002.dump.h5)

if [ "$seed1" == "$seed2" ]; then
	echo "Error: saved seeds are the same between different runs!"
	exit 1
fi
