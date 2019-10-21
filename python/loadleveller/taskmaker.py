import sys
import os
import yaml
import json
import numpy

try:
    from yaml import CSafeLoader as SafeLoader, CSafeDumper as SafeDumper
except ImportError:
    from yaml import SafeLoader, SafeDumper

def _expand_path(path):
    scriptdir = os.path.dirname(sys.argv[0])
    return os.path.abspath(os.path.join(scriptdir,os.path.expandvars(os.path.expanduser(path))))

def JobConfig(filename):
    with open(_expand_path(filename), 'r') as f:
        return yaml.load(f, Loader=SafeLoader)

class TaskMaker:
    def __init__(self, name, jobconfig):
        self._tm_name = name
        self._tm_tasks = []
        if type(jobconfig) == dict:
            self._tm_jobconfig = jobconfig
        elif type(jobconfig) == str:
            self._tm_jobconfig = JobConfig(jobconfig)

        self._tm_jobconfig['mc_binary'] = _expand_path(self._tm_jobconfig['mc_binary'])

    def task(self, **additional_parameters):
        self._tm_tasks.append({**self.__dict__, **additional_parameters})

    def write(self):
        filenames = []
        jobfile_dict = { 'jobname': self._tm_name, 'jobconfig': self._tm_jobconfig, 'tasks': {}}

        for i, t in enumerate(self._tm_tasks):
            task_name = f'task{i+1:04d}'
            task_dict = {}
            for k, v in t.items():
                if k.startswith('_tm_'):
                    continue

                if isinstance(v, numpy.generic): # cast numpy types to their native forms
                    v = v.item()

                task_dict[k] = v
            jobfile_dict['tasks'][task_name] = task_dict

        json.dump(jobfile_dict, sys.stdout, indent=1)
