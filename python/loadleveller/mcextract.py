import yaml
import numpy as np
import itertools

'''This module can be used to easily extract Monte Carlo results from the *.results.yml file produced by the loadleveller library.'''

class Observable:
    def __init__(self, num_tasks):
        self.rebinning_bin_length = np.zeros(num_tasks)
        self.rebinning_bin_count = np.zeros(num_tasks)
        self.autocorrelation_time = np.zeros(num_tasks)+np.nan

        # have to be set later because dimension is unknown
        self.mean = None
        self.error = None        

class MCArchive:
    def __init__(self, filename):
        with open(filename, 'r') as f:
            doc = yaml.load(f)

        param_names = set(sum([list(task['parameters'].keys()) for task in doc], []))
        observable_names = set(sum([list(task['results'].keys()) for task in doc], []))
        self.num_tasks = len(doc)
        
        self.parameters = dict(zip(param_names, [[None for _ in range(self.num_tasks)] for _ in param_names]))
        self.observables = dict(zip(observable_names, [Observable(self.num_tasks) for _ in observable_names]))

        for i, task in enumerate(doc):
            for param, value in task['parameters'].items():
                self.parameters[param][i] = value

            for obs, value in task['results'].items():
                o = self.observables[obs]
                o.rebinning_bin_length[i] = value['rebinning_bin_length']
                o.rebinning_bin_count[i] = value['rebinning_bin_count']
                o.autocorrelation_time[i] = value['autocorrelation_time']

                # fill in dimensions on first occurence of the observable
                if np.all(o.mean == None):
                    o.mean = np.zeros([self.num_tasks, len(value['mean'])]) + np.nan
                    o.error = np.zeros([self.num_tasks, len(value['error'])]) + np.nan

                    if o.mean.shape != o.error.shape:
                        raise 'observable "{}": dimension mismatch between mean and error'.format(obs)
                else:
                    if len(value['mean']) != o.mean.shape[1] or len(value['error']) != o.error.shape[1]:
                        raise 'observable "{}": dimension mismatch between different tasks'.format(obs)

                o.mean[i,:] = value['mean']
                o.error[i,:] = value['error']

    def condition_mask(self, conditions):
        if not conditions:
            return [True for _ in range(self.num_tasks)]

        return [all(self.parameters[key][i] == val for key, val in conditions.items()) for i in range(self.num_tasks)]

    def get_parameter(self, name, unique=False, conditions={}):
        selection = list(itertools.compress(self.parameters[name], self.condition_mask(conditions)))
        
        if unique:
            return list(sorted(set(selection)))
        return selection

    def get_observable(self, name, conditions={}):
        orig = self.observables[name]
        
        selection = Observable(0)
        mask = self.condition_mask(conditions)
        selection.rebinning_bin_count = orig.rebinning_bin_count[mask]
        selection.rebinning_bin_length = orig.rebinning_bin_length[mask]
        selection.autocorrelation_time = orig.autocorrelation_time[mask]
        selection.mean = orig.mean[mask,:]
        selection.error = orig.error[mask,:]

        return selection
