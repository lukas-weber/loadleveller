loadleveller
============

loadleveller is a C++ framework for creating MPI distributed Monte Carlo simulations. It takes care of data storage and error handling and other functionalities common to all MC codes. The only thing you have to implement is the actual update and measurement code. 
It uses HDF5 and YAML as data storage formats and provides a python package containing helpers for launching jobs and accessing the results of simulations.

Getting Started
------------

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
