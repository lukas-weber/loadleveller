import yaml
import os
import subprocess
import errno

'''Helpers for handling loadleveller jobfiles/scripts. For lack of a better idea, the job description files of loadleveller are actually executables that output a more verbose yaml parameter file to stdout. Use the taskmaker module to write the input scripts.'''

class JobFileGenError(Exception):
    pass

class JobFile:
    def __init__(self, filename):
        env = dict(os.environ)
        env['PATH'] += ':.'
        result = subprocess.run([filename], stdout=subprocess.PIPE, env=env)

        self.raw_jobfile = result.stdout.decode('utf-8')
        if result.returncode != 0:
            raise JobFileGenError('Generation script "{}" had a non-zero return code. Treating as error.'.format(filename))
 
        try:
            parsed_job = yaml.load(self.raw_jobfile, Loader=yaml.CSafeLoader)
            self.__dict__.update(parsed_job)
        except Exception as e: 
            raise JobFileGenError('Could not parse job generation script output: {}'.format(e))

    def write_job_input_file(self):
        try:
            datadir = self.jobname + '.data'
            try:
                os.makedirs(datadir)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise
            job_input_filename = os.path.join(datadir, 'parameters.yml')
            with open(job_input_filename, 'w') as f:
                f.write(self.raw_jobfile)
        except Exception as e:
            raise JobFileGenError('Could not write parameters.yml: {}'.format(e))

        return job_input_filename
