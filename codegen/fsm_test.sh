jsonnet fsm_test.jsonnet > fsm_test.json &&  j2 fsm_msm_euml.cc fsm_test.json > fsm_test.cc && g++ -o fsm_test fsm_test.cc 
