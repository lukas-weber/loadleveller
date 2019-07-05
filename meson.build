project('load-leveller', 'c', 'cpp',
  version : '2.0',
  meson_version : '>=0.50.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'cpp_std=c++17'
  ]
)

fmt_dep = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
yamlcpp_dep = dependency('yaml-cpp', version : '>=0.6.0', fallback : ['yaml-cpp', 'yamlcpp_dep'])
mpi_dep = dependency('mpi', language : 'cpp')

# HDF5 is a pain
cppc = meson.get_compiler('cpp')
hdf5_dep = cppc.find_library('hdf5', required : false)

# This should be a last resort because I hacked the meson.build for hdf5 together in an afternoon and am not sure if it works correctly ;)
if not hdf5_dep.found() or get_option('force_hdf5_fallback')
  hdf5_proj = subproject('hdf5')
  hdf5_dep = hdf5_proj.get_variable('hdf5_dep')
endif

should_install = not meson.is_subproject()

loadleveller_deps = [ fmt_dep, yamlcpp_dep, mpi_dep, hdf5_dep ]

subdir('src')

if not meson.is_subproject()
	subdir('test')
endif
