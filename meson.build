project('load-leveller', 'c', 'cpp',
  version : '2.0',
  meson_version : '>=0.50.0',
  default_options : [
    'warning_level=3',
  ]
)

fmt_dep = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
yamlcpp_dep = dependency('yaml-cpp', version : '>=0.6.0', fallback : ['yaml-cpp', 'yamlcpp_dep'])
mpi_dep = dependency('mpi')

# HDF5 is a pain
cc = meson.get_compiler('cpp')
hdf5_lib = cc.find_library('hdf5')

should_install = meson.is_subproject()

loadleveller_deps = [ fmt_dep, yamlcpp_dep, mpi_dep, hdf5_lib ]

subdir('src')
