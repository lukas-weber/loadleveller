
t1 = executable('tests',
  ['duration_parser.cpp', 'monotone_interpolator.cpp', 'observable_names.cpp', 'jackknifing.cpp'],
  dependencies : loadleveller_dep,
  include_directories : include_directories('../src')
)

test('tests', t1)

bench_rng = executable('bench_rng', ['bench_rng.cpp'], dependencies : loadleveller_dep)
benchmark('rng', bench_rng)

test_rng = executable('test_rng', ['test_rng.cpp'], dependencies : loadleveller_dep)
test('mt19937', test_rng)

subdir('silly_mc') 
