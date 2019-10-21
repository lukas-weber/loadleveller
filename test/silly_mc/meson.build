silly_mc = executable('silly_mc_test',
	['main.cpp', 'silly_mc.cpp'],
	dependencies : loadleveller_dep,
)

testscript = find_program('silly_test.py')

test('algebraic series mc', testscript, args : [silly_mc, files('test_parameters.json')]) 
