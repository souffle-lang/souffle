# Souffle - A Datalog Compiler
# Copyright (c) 2023 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

function(POSITIVE_TEST NAME)
  souffle_run_test_helper(TEST_NAME ${NAME} CATEGORY behavior ${ARGV})
  souffle_run_test_helper(TEST_NAME ${NAME} CATEGORY behavior ${ARGV} COMPILED)
endfunction()

positive_test(output_stdout OUTPUT_STDOUT)
