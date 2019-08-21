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
        self.tasks = list(jobfile.tasks.keys())
        self.tasks.sort()
        self.restart = False

        self.progress = []
        for task in self.tasks:
            tp = TaskProgress()
            
            tp.target_sweeps = jobfile.tasks[task]['sweeps']
            tp.target_therm = jobfile.tasks[task]['thermalization']

            tp.sweeps = 0
            tp.therm_sweeps = 0

            tp.num_runs = 0
            
            for runfile in glob.iglob('{}.data/{}/run*.dump.h5'.format(self.jobfile.jobname,task)):
                tp.num_runs += 1

                with h5py.File(runfile, 'r') as f:
                    sweeps = f['/sweeps'][0]//jobfile.tasks[task].get('pt_sweeps_per_global_update',1)
                    
                    tp.therm_sweeps += min(sweeps,tp.target_therm)
                    tp.sweeps += max(0,sweeps - tp.target_therm)


            if tp.sweeps < tp.target_sweeps:
                self.restart = True

            self.progress.append(tp)

    def needs_restart(self):
        return self.restart
        
    def needs_merge(self):
        result_mtime = 0
        try:
            result_mtime = os.path.getmtime(self.jobfile.jobname+'.results.yml')
        except FileNotFoundError:
            return True

        for task in self.tasks:
            for measfile in glob.iglob('{}.data/{}/run*.meas.h5'.format(self.jobfile.jobname, task)):
                if os.path.getmtime(measfile) > result_mtime:
                    return True

        return False
        
def print_status(jobfile, args):
    """ This function is exported as the loadl status command """
    
    import argparse
    import sys

    parser = argparse.ArgumentParser(description='Prints the status and progress of a loadleveller Monte Carlo job.')
    parser.add_argument('--need-restart', action='store_true', help='Return 1 if the job is not completed yet and 0 otherwise.')
    parser.add_argument('--need-merge', action='store_true', help='Return 1 if the merged results are older than the raw data and 0 otherwise.')
    
    args = parser.parse_args(args)

    try:
        job_prog = JobProgress(jobfile)

        if args.need_restart and args.need_merge:
            print("Error: only one option of '--need-restart' and '--need-merge' can appear at once", file=sys.stderr)
            sys.exit(-1)

        if args.need_restart:
            if job_prog.needs_restart():
                print('Needs restart!')
                return 0
            print('Job completed.')
            return 1

        if args.need_merge:
            if job_prog.needs_merge():
                print('Needs merge!')
                return 0
            print('Job already merged.')
            return 1

        for task, tp in zip(job_prog.tasks, job_prog.progress):
            therm_per_run = tp.therm_sweeps/tp.num_runs if tp.num_runs > 0 else 0
            print('{t}: {tp.num_runs} runs, {tp.sweeps:8d}/{tp.target_sweeps} sweeps, {therm_per_run:8d}/{tp.target_therm} thermalization'.format(t=task,tp=tp,therm_per_run=int(round(therm_per_run))))
        
    except FileNotFoundError as e:
        print("Error: jobfile '{}' not found.".format(args.jobfile))
