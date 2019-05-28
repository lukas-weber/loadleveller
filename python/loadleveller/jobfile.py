import yaml
import os

'''Helpers for handling loadleveller jobfiles.'''

class JobFile:
    def __init__(self, filename):
        with open(filename, 'r') as f:
            jobfile = yaml.safe_load(f)
        self.__dict__ = jobfile
      
