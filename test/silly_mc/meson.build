silly_mc = executable('silly_mc_test',
	['main.cpp', 'silly_mc.cpp'],
	dependencies : loadleveller_dep,
)

algebraic_test = find_program('silly_test.py')
checkpoint_test = find_program('checkpoint_test.sh')

test('algebraic series mc', algebraic_test, args : [silly_mc, files('test_parameters.json')]) 
test('checkpoint test', checkpoint_test, args : [silly_mc, files('test_parameters_long.json')])
