project('load-leveller', 'c', 'cpp',
  version : '2.0',
  meson_version : '>=0.56.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
  ]
)

fmt_dep = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
json_dep = dependency('nlohmann_json', fallback : ['nlohmann_json', 'nlohmann_json_dep'])
mpi_dep = dependency('mpi', language : 'cpp')
hdf5_dep = dependency('hdf5', language : 'c', required : true)

loadleveller_deps = [ fmt_dep, json_dep, mpi_dep, hdf5_dep ]

# intel mkl
mkl_dep = dependency('mkl-dynamic-ilp64-seq', required : false)
if not mkl_dep.found()
  mkl_dep = dependency('mkl-static-ilp64-seq', required : (get_option('rng_backend') == 'intel_mkl'))
endif

if mkl_dep.found()
	loadleveller_deps += mkl_dep
endif

should_install = not meson.is_subproject() or get_option('default_library') == 'static'

subdir('src')

if not meson.is_subproject()
	subdir('test')
endif
