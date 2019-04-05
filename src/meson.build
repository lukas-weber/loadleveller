loadleveller_sources = files([
  'dump.cpp',
  'evalable.cpp',
  'mc.cpp',
  'measurements.cpp',
  'merger.cpp',
  'observable.cpp',
  'parser.cpp',
  'random.cpp',
  'results.cpp',
  'runner.cpp',
  'runner_single.cpp',
  'runner_task.cpp',
])

loadleveller_headers = files([
  'dump.h',
  'evalable.h',
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
  'runner_single.h',
  'runner_task.h',
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
    description : 'Framework for distributed (Quantum) Monte Carlo codes',
  )
  
  install_headers(loadleveller_headers, subdir : 'loadleveller')
endif

loadleveller_dep = declare_dependency(
  include_directories : include_directories('../include/'),
  link_with : libloadleveller,
  dependencies : loadleveller_deps,
  version : meson.project_version(),
)
