project('load-leveller', 'c', 'cpp',
  version : '2.0',
  meson_version : '>=0.50.0',
  default_options : [
    'warning_level=3',
    'cpp_std=c++17'
  ]
)

fmt_dep = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
yamlcpp_dep = dependency('yaml-cpp', version : '>=0.6.0', fallback : ['yaml-cpp', 'yamlcpp_dep'])
mpi_dep = dependency('mpi', language : 'cpp')

# HDF5 is a pain
cc = meson.get_compiler('cpp')
hdf5_lib = cc.find_library('hdf5')

should_install = not meson.is_subproject()

loadleveller_deps = [ fmt_dep, yamlcpp_dep, mpi_dep, hdf5_lib ]

subdir('src')
subdir('test')
