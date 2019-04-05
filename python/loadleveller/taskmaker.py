import sys
import os
import yaml
import numpy

class TaskMaker:
    def __init__(self, name, jobconfig):
        self._tm_name = os.path.splitext(name)[0]
        self._tm_tasks = []
        self._tm_jobconfig = jobconfig
    def task(self, **additional_parameters):
        self._tm_tasks.append({**self.__dict__, **additional_parameters})

    def write(self):
        filenames = []
        jobfile_dict = { 'jobconfig': self._tm_jobconfig, 'tasks': {}}
        
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

        with open(self._tm_name, 'w') as f:
            f.write(yaml.dump(jobfile_dict))
