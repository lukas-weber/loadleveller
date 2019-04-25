#!/usr/bin/env python3

import glob
import h5py
import os
import yaml

"""
This module extracts progress information from job data.
"""

class TaskProgress:
    pass

class JobProgress:
    def __init__(self, jobfile):
        self.jobfile = jobfile
        with open(jobfile, 'r') as f:
            jobfile = yaml.safe_load(f)

        self.tasks = jobfile['tasks'].keys()
        self.restart = False

        self.progress = []
        for task in self.tasks:
            tp = TaskProgress()
            
            tp.target_sweeps = jobfile['tasks'][task]['sweeps']
            tp.target_therm = jobfile['tasks'][task]['thermalization']

            tp.sweeps = 0
            tp.therm_sweeps = 0

            tp.num_runs = 0
            
            for runfile in glob.iglob('{}.data/{}/run*.dump.h5'.format(self.jobfile,task)):
                tp.num_runs += 1

                with h5py.File(runfile, 'r') as f:
                    tp.sweeps += f['/sweeps'][0]
                    tp.therm_sweeps += f['/thermalization_sweeps'][0]


            if tp.sweeps < tp.target_sweeps + tp.target_therm:
                self.restart = True

            self.progress.append(tp)

    def needs_restart(self):
        return self.restart
        
    def needs_merge(self):
        result_mtime = 0
        try:
            result_mtime = os.path.getmtime(self.jobfile+'.results.yml')
        except FileNotFoundError:
            return True

        for task in self.tasks:
            for measfile in glob.iglob('{}.data/{}/run*.meas.h5'.format(self.jobfile,task)):
                if os.path.getmtime(measfile) > result_mtime:
                    return True

        return False
        
def ystatus():
    """ This function is exported as the ystatus command """
    
    import argparse
    import sys

    parser = argparse.ArgumentParser(description='Prints the status and progress of a loadleveller Monte Carlo job.')
    parser.add_argument('jobfile', metavar='JOBFILE', help='Configuration file containing all the job information.')
    parser.add_argument('--need-restart', action='store_true', help='Return 1 if the job is not completed yet and 0 otherwise.')
    parser.add_argument('--need-merge', action='store_true', help='Return 1 if the merged results are older than the raw data and 0 otherwise.')
    
    args = parser.parse_args()

    try:
        job_prog = JobProgress(args.jobfile)

        if args.need_restart and args.need_merge:
            print("Error: only one option of '--need-restart' and '--need-merge' can appear at once", file=sys.stderr)
            sys.exit(-1)

        if args.need_restart:
            if job_prog.needs_restart():
                print('Needs restart!')
                return True
            print('Job completed.')
            return False

        if args.need_merge:
            if job_prog.needs_merge():
                print('Needs merge!')
                return True
            print('Job already merged.')
            return False

        for task, tp in zip(job_prog.tasks, job_prog.progress):
            print('{t}: {tp.num_runs} runs, {tp.sweeps}/{tp.target_sweeps} sweeps, {tp.therm_sweeps/tp.num_runs}/{tp.target_therm} thermalization'.format(t=task,tp=tp))
        
    except FileNotFoundError as e:
        print("Error: jobfile '{}' not found.".format(args.jobfile))
