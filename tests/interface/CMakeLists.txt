# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

# Make sure that the functor library is built
add_subdirectory(functors)
add_subdirectory(graph_coloring)

# swig test which will run python, java or both
function(SOUFFLE_POSITIVE_FUNCTOR_TEST TEST_NAME)
        souffle_run_test_helper(TEST_NAME ${TEST_NAME} FUNCTORS ${ARGN})
        souffle_run_test_helper(TEST_NAME ${TEST_NAME} COMPILED FUNCTORS ${ARGN})
endfunction()

function(SOUFFLE_RUN_CPP_TEST)
    cmake_parse_arguments(
        PARAM
        ""
        "PARAM_TEST_NAME;QUALIFIED_TEST_NAME;INPUT_DIR;OUTPUT_DIR;FIXTURE_NAME;TEST_LABELS;FACTS_DIR"
        ""
        ${ARGV}
    )


    add_test(NAME ${PARAM_QUALIFIED_TEST_NAME}_compile_cpp
             COMMAND sh -c "set -e; '${CMAKE_CXX_COMPILER}' --std=c++17 -D__EMBEDDED_SOUFFLE__  '-I${CMAKE_SOURCE_DIR}/src/include' '${TEST_NAME}.cpp' '${PARAM_INPUT_DIR}/driver.cpp' -o '${TEST_NAME}'")
    set_tests_properties(${PARAM_QUALIFIED_TEST_NAME}_compile_cpp PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP ${PARAM_FIXTURE_NAME}_compile_cpp
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_run_souffle
                        )

    add_test(NAME ${PARAM_QUALIFIED_TEST_NAME}_run_cpp
            COMMAND sh -c "set -e; './${TEST_NAME}' '${FACTS_DIR}' > '${TEST_NAME}.out' 2> '${TEST_NAME}.err'")
    set_tests_properties(${PARAM_QUALIFIED_TEST_NAME}_run_cpp PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP ${PARAM_FIXTURE_NAME}_run_cpp
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_compile_cpp
                        )
endfunction()

function(SOUFFLE_RUN_CPP_TEST_HELPER)
    # PARAM_TEST_NAME - the name of the test, the short directory name under tests/<category>/<test_name>
    cmake_parse_arguments(
        PARAM
        "COMPARE_STDOUT"
        "TEST_NAME" #Single valued options
        ""
        ${ARGV}
    )

    set(INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PARAM_TEST_NAME}")
    set(FACTS_DIR "${INPUT_DIR}/facts")
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PARAM_TEST_NAME}")
    # Give the test a name which has good info about it when running
    # People can then search for the test by the name, or the labels we create
    set(QUALIFIED_TEST_NAME interface/${PARAM_TEST_NAME})
    set(FIXTURE_NAME ${QUALIFIED_TEST_NAME}_fixture)
    set(TEST_LABELS "positive;integration")

    souffle_setup_integration_test_dir(TEST_NAME ${PARAM_TEST_NAME}
                                       QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                       DATA_CHECK_DIR ${INPUT_DIR}
                                       OUTPUT_DIR ${OUTPUT_DIR}
                                       EXTRA_DATA ${EXTRA}
                                       FIXTURE_NAME ${FIXTURE_NAME}
                                       TEST_LABELS ${TEST_LABELS})

    souffle_run_integration_test(TEST_NAME ${PARAM_TEST_NAME}
                                 QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                 INPUT_DIR ${INPUT_DIR}
                                 OUTPUT_DIR ${OUTPUT_DIR}
                                 FIXTURE_NAME ${FIXTURE_NAME}
                                 TEST_LABELS "${TEST_LABELS}"
                                 SOUFFLE_PARAMS "-g '${OUTPUT_DIR}/${TEST_NAME}.cpp'")

    souffle_run_cpp_test(TEST_NAME ${PARAM_TEST_NAME}
                         QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                         INPUT_DIR ${INPUT_DIR}
                         OUTPUT_DIR ${OUTPUT_DIR}
                         FIXTURE_NAME ${FIXTURE_NAME}
                         FACTS_DIR "${FACTS_DIR}"
                         TEST_LABELS ${TEST_LABELS})

    souffle_compare_std_outputs(TEST_NAME ${PARAM_TEST_NAME}
                                 QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                 OUTPUT_DIR ${OUTPUT_DIR}
                                 EXTRA_DATA ${EXTRA}
                                 RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_cpp
                                 TEST_LABELS ${TEST_LABELS})

    souffle_compare_csv(QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                        INPUT_DIR ${INPUT_DIR}
                        OUTPUT_DIR ${OUTPUT_DIR}
                        RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_cpp
                        NEGATIVE ${PARAM_NEGATIVE}
                        TEST_LABELS ${TEST_LABELS})

endfunction()

# cpp test which will compile Souffle programs externally
function(SOUFFLE_POSITIVE_CPP_TEST TEST_NAME)
    souffle_run_cpp_test_helper(TEST_NAME ${TEST_NAME} ${ARGN})
endfunction()

souffle_positive_functor_test(functors CATEGORY interface)
souffle_positive_functor_test(graph_coloring CATEGORY interface)
souffle_positive_cpp_test(contain_insert)
souffle_positive_cpp_test(get_symboltabletype)
souffle_positive_cpp_test(insert_for)
souffle_positive_cpp_test(insert_print)
souffle_positive_cpp_test(load_print)
souffle_positive_cpp_test(signal_error)
souffle_positive_cpp_test(tuple_insertion_diff_element_type)
souffle_positive_cpp_test(tuple_insertion_diff_relation)

# The following test fails because we use -g (instead -o)
# TODO: (This neeads to be investigated)
# souffle_positive_cpp_test(repeat_analysis)
