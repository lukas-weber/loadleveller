rng_names = {
  'stl_mt19937' : 'rng_stl<std::mt19937>',
  'stl_mt19937_64' : 'rng_stl<std::mt19937_64>',
  'internal_mersenne' : 'rng_internal_mersenne',
}

conf_data = configuration_data()
conf_data.set('rng_backend', rng_names[get_option('rng_backend')])
configure_file(input : 'config.h.in',
  output : 'config.h',
  install : should_install,
  install_dir : 'include/loadleveller',
  configuration : conf_data)
config_inc = include_directories('.')

loadleveller_sources = files([
  'evalable.cpp',
  'iodump.cpp',
  'jobinfo.cpp',
  'mc.cpp',
  'measurements.cpp',
  'merger.cpp',
  'observable.cpp',
  'parser.cpp',
  'random.cpp',
  'results.cpp',
  'runner.cpp',
  'runner_pt.cpp',
  'runner_single.cpp',
  'runner_task.cpp',
  'util.cpp',
])

loadleveller_headers = files([
  'evalable.h',
  'iodump.h',
  'jobinfo.h',
  'loadleveller.h',
  'mc.h',
  'measurements.h',
  'merger.h',
  'MersenneTwister.h',
  'observable.h',
  'parser.h',
  'random.h',
  'results.h',
  'runner.h',
  'runner_pt.h',
  'runner_single.h',
  'runner_task.h',
  'util.h',
])

libloadleveller = library('loadleveller',
  loadleveller_sources,
  dependencies : loadleveller_deps,
  version : meson.project_version(),
  install : should_install,
  override_options : ['c_std=none']
)

if should_install
  pkg = import('pkgconfig')
  pkg.generate(libloadleveller,
    description : 'Framework for distributed (quantum) Monte Carlo codes',
  )
  
  install_headers(loadleveller_headers, subdir : 'loadleveller')
endif

loadleveller_dep = declare_dependency(
  include_directories : [ include_directories('../include/'), config_inc ],
  link_with : libloadleveller,
  dependencies : loadleveller_deps,
  version : meson.project_version(),
)
