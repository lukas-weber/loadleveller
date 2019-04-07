loadleveller
============

loadleveller is a C++ framework for creating MPI distributed Monte Carlo simulations. It takes care of data storage and error handling and other functionalities common to all MC codes. The only thing you have to implement is the actual update and measurement code. 
It uses HDF5 and YAML as data storage formats and provides a python package containing helpers for launching jobs and accessing the results of simulations.

Getting Started
---------------

loadleveller uses the `Meson <https://mesonbuild.com/>`_ build system. It is not as wide spread as CMake but has some advantages, especially on systems with old software. It can be installed via pip making it just as available as other build systems.

Dependencies
^^^^^^^^^^^^

- MPI
- HDF5 >=1.10.1
- yaml-cpp >= 0.6 (fallback provided)
- fmt (fallback provided)

Building loadleveller
^^^^^^^^^^^^^^^^^^^^^

If you don’t have meson installed install it from your distributions package manager or
::

    pip3 install --user meson ninja

Then from the source directory you do
::

    meson . build
    cd build
    ninja

You can install it using ``ninja install``. The recommended way to use loadleveller is to use meson for your Monte Carlo code, too. Then you can use loadleveller as a `wrap dependency <https://mesonbuild.com/Wrap-dependency-system-manual.html>`_ and not worry about this section at all.

Python package
^^^^^^^^^^^^^^

To use loadleveller effectively you should install the python package from the ``python/`` subdirectory
::

    cd python
    python3 setup.py install --user

How it works
------------

For details on how to implement and use your MC simulation with loadleveller see the `example project <TODO>`_. After you build it and get an executable. You need to create a job file containing the set of parameters you want to calculate. This is conveniently done with the ``loadleveller.taskmaker`` module.

You can start it (either on a cluster batch system or locally) using the ``yrun JOBFILE`` command from the python package. It will calculate all the tasks defined in the jobfile in parallel. Measurements and checkpoints will be saved in the ``JOBFILE.data`` directory. After everything is done, measurements will be averaged and merged into the human-readable ``JOBFILE.results.yml`` file. You may use the ``loadleveller.mcextract`` module to conveniently extract results from it for further processing.

Use the ``ydelete`` tool to delete all job data for a fresh start after you changed something. ``ystatus`` gives you information about the job progress. You do not have to wait until completion to get the result file. ``yrun -m JOBFILE`` merges whatever results are already done.

Hidden features
---------------

Some handy features that are not immediately obvious.

Autocorrelation times
^^^^^^^^^^^^^^^^^^^^^

For every observable (except evalables, which are calculated from other observables in postprocessing) loadleveller will estimate the autocorrelation times. You can see them in the results file and also extract them with python. 
These times are in terms of “bins”. So if you set a bin size for your observable (visible as ``internal_bin_size`` in the results) this is not the auto correlation time in MC sweeps.

Primitive profiling
^^^^^^^^^^^^^^^^^^^

You may notice observables starting with ``_ll_`` in your result files. These are profiling data loadleveller collects on any run. They are all given in seconds.

It may be useful to plot them to check the performance of your method in different parameter regiemes or the ratio of time spent on sweeps and measurements. Be careful about environmental effects, especially when you are running it next to other programs.
