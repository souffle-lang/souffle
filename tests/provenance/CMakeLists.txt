# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

# provenance test 
function(SOUFFLE_PROVENANCE_TEST TEST_NAME)
    souffle_run_test_helper(TEST_NAME ${TEST_NAME} CATEGORY provenance)
    souffle_run_test_helper(TEST_NAME ${TEST_NAME} COMPILED CATEGORY provenance)
endfunction()

if (NOT MSVC)
souffle_provenance_test(components)
souffle_provenance_test(constraints)
souffle_provenance_test(cprog1)
souffle_provenance_test(eqrel_tests3)
souffle_provenance_test(explain_float_unsigned)
souffle_provenance_test(high_arity)
souffle_provenance_test(negation)
souffle_provenance_test(path)
souffle_provenance_test(path_explain_negation)
souffle_provenance_test(path_explain_output)
souffle_provenance_test(query_1)
souffle_provenance_test(query_2)
souffle_provenance_test(query_3)
souffle_provenance_test(query_4)
souffle_provenance_test(query_float_unsigned)
souffle_provenance_test(same_gen)
endif ()
