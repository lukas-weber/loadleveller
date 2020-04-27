import json
import os
import subprocess
import errno
from collections import defaultdict


'''Helpers for handling loadleveller jobfiles/scripts. For lack of a better idea, the job description files of loadleveller are actually executables that output a more verbose json parameter file to stdout. Use the taskmaker module to write the input scripts.'''

class JobFileGenError(Exception):
    pass

class JobFileOverwriteError(Exception):
    def __init__(self, difference):
        self.difference = difference
    def diff_summary(self):
        summary = []
        for taskname, taskdiff in self.difference.items():
            summary.append('{}:'.format(taskname))
            for param, diff in taskdiff.items():
                summary.append('    {}: {} -> {}'.format(param, diff[1], diff[0]))
        return '\n'.join(summary)
    def __str__(self):
        return 'Tried to overwrite existing job data with different parameters.\n{}'.format(self.diff_summary())

class JobFile:
    def __init__(self, filename):
        env = dict(os.environ)
        env['PATH'] += ':.'
        result = subprocess.run([filename], stdout=subprocess.PIPE, env=env)

        self.raw_jobfile = result.stdout.decode('utf-8')
        if result.returncode != 0:
            raise JobFileGenError('Generation script "{}" had a non-zero return code. Treating as error.'.format(filename))
 
        try:
            parsed_job = json.loads(self.raw_jobfile)
            self.__dict__.update(parsed_job)
        except Exception as e: 
            raise JobFileGenError('Could not parse job generation script output: {}'.format(e))

    def _compare_to_old(self, old_data):
        diff = defaultdict(dict)
        for taskname, task in self.tasks.items():
            if taskname in old_data['tasks']:
                old_task = old_data['tasks'][taskname]
                for param, value in task.items():
                    if param not in old_task:
                        diff[taskname][param] = (value, None)
                    elif value != old_task[param]:
                        diff[taskname][param] = (value, old_task[param])
        return diff

    def write_job_input_file(self, force_overwrite):
        datadir = self.jobname + '.data'
        try:
            os.makedirs(datadir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        job_input_filename = os.path.join(datadir, 'parameters.json')

        if os.path.isfile(job_input_filename):
            with open(job_input_filename, 'r') as f:
                old_data = json.load(f)
            diff = self._compare_to_old(old_data)
            if not force_overwrite and len(diff) > 0:
                raise JobFileOverwriteError(difference=diff)
                 
        try:
            with open(job_input_filename, 'w') as f:
                f.write(self.raw_jobfile)
        except JobFileGenError as e:
            raise JobFileGenError('Could not write parameters.json: {}'.format(e))

        return job_input_filename
