import yaml
import os

'''Helpers for handling loadleveller jobfiles.'''

class JobFile:
    def __init__(self, filename):
        with open(filename, 'r') as f:
            jobfile = yaml.safe_load(f)
        self.tasks = jobfile['tasks']

        cwd = os.getcwd()
        try:
            jobpath = os.path.dirname(filename)
            if jobpath != '':
                os.chdir(os.path.dirname(filename))
            jobconfig_path = os.path.expandvars(os.path.expanduser(jobfile['jobconfig']))
            with open(jobconfig_path) as f:
                self.jobconfig = yaml.safe_load(f)

            self.mc_binary = os.path.abspath(os.path.expandvars(os.path.expanduser(self.jobconfig['mc_binary'])))
        finally:
            os.chdir(cwd)
        
