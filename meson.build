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
json_dep = dependency('nlohmann_json', fallback : ['nlohmann_json', 'nlohmann_json_dep'])
mpi_dep = dependency('mpi', language : 'cpp')

# HDF5 is a pain
cppc = meson.get_compiler('cpp')
hdf5_dep = cppc.find_library('hdf5', required : false)

# This should be a last resort because I hacked the meson.build for hdf5 together in an afternoon and am not sure if it works correctly ;)
if not hdf5_dep.found() or get_option('force_hdf5_fallback')
  hdf5_proj = subproject('hdf5')
  hdf5_dep = hdf5_proj.get_variable('hdf5_dep')
endif

loadleveller_deps = [ fmt_dep, json_dep, mpi_dep, hdf5_dep ]

# intel mkl
mkl_dep = dependency('mkl-dynamic-ilp64-seq', required : false)
if not mkl_dep.found()
  mkl_dep = dependency('mkl-static-ilp64-seq', required : (get_option('rng_backend') == 'intel_mkl'))
endif

if mkl_dep.found()
	loadleveller_deps += mkl_dep
endif


should_install = not meson.is_subproject()

subdir('src')

if not meson.is_subproject()
	subdir('test')
endif
