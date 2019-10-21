
t1 = executable('tests',
  ['duration_parser.cpp', 'monotone_interpolator.cpp'],
  dependencies : loadleveller_dep,
  include_directories : include_directories('../src')
)

test('tests', t1)

subdir('silly_mc')        
