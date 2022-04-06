# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

if (SOUFFLE_TEST_EVALUATION)
    add_subdirectory(evaluation)
endif()
if (SOUFFLE_TEST_EXAMPLES)
    add_subdirectory(example)
endif()
add_subdirectory(semantic)
add_subdirectory(swig)
add_subdirectory(syntactic)
add_subdirectory(interface)
add_subdirectory(provenance)
add_subdirectory(profile)
add_subdirectory(scheduler)
