# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

function(SOUFFLE_RUN_PYTHON_SWIG_TEST)
    cmake_parse_arguments(
        PARAM
        ""
        "PARAM_TEST_NAME;QUALIFIED_TEST_NAME;INPUT_DIR;OUTPUT_DIR;FIXTURE_NAME;TEST_LABELS;FACTS_DIR"
        ""
        ${ARGV}
    )

    add_test(NAME ${PARAM_QUALIFIED_TEST_NAME}_run_swig
             COMMAND sh -c "set -e; PYTHONPATH=. '${Python3_EXECUTABLE}' '${PARAM_INPUT_DIR}/driver.py' '${PARAM_FACTS_DIR}' \\
                                            1> '${PARAM_TEST_NAME}-python.out' \\
                                            2> '${PARAM_TEST_NAME}-python.err'")
    set_tests_properties(${PARAM_QUALIFIED_TEST_NAME}_run_swig PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP ${PARAM_FIXTURE_NAME}_run_swig
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_run_souffle
                        )
endfunction()

function(SOUFFLE_RUN_JAVA_SWIG_TEST)
    cmake_parse_arguments(
        PARAM
        ""
        "PARAM_TEST_NAME;QUALIFIED_TEST_NAME;INPUT_DIR;OUTPUT_DIR;FIXTURE_NAME;TEST_LABELS;FACTS_DIR"
        ""
        ${ARGV}
    )

    add_test(NAME ${PARAM_QUALIFIED_TEST_NAME}_compile_java
             COMMAND sh -c "set -e; '${Java_JAVAC_EXECUTABLE}' *.java '${PARAM_INPUT_DIR}'/*.java -d '${PARAM_OUTPUT_DIR}'")
    set_tests_properties(${PARAM_QUALIFIED_TEST_NAME}_compile_java PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP ${PARAM_FIXTURE_NAME}_compile_java
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_run_souffle
                        )

    add_test(NAME ${PARAM_QUALIFIED_TEST_NAME}_run_swig
            COMMAND sh -c "set -e; '${Java_JAVA_EXECUTABLE}' -Djava.library.path='${PARAM_OUTPUT_DIR}' driver \\
                                            ${PARAM_FACTS_DIR} \\
                                            1> '${PARAM_TEST_NAME}-java.out' \\
                                            2> '${PARAM_TEST_NAME}-java.err'")
    set_tests_properties(${PARAM_QUALIFIED_TEST_NAME}_run_swig PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP ${PARAM_FIXTURE_NAME}_run_swig
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_compile_java
                        )
endfunction()

function(SOUFFLE_RUN_SWIG_TEST_HELPER)
    # PARAM_TEST_NAME - the name of the test, the short directory name under tests/<category>/<test_name>
    cmake_parse_arguments(
        PARAM
        "COMPARE_STDOUT"
        "TEST_NAME;LANGUAGE" #Single valued options
        ""
        ${ARGV}
    )

    set(INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PARAM_LANGUAGE}/${PARAM_TEST_NAME}")
    set(FACTS_DIR "${INPUT_DIR}/facts")
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PARAM_LANGUAGE}/${PARAM_TEST_NAME}")
    # Give the test a name which has good info about it when running
    # People can then search for the test by the name, or the labels we create
    set(QUALIFIED_TEST_NAME swig/${PARAM_LANGUAGE}/${PARAM_TEST_NAME})
    set(FIXTURE_NAME ${QUALIFIED_TEST_NAME}_fixture)
    set(TEST_LABELS "swig;${PARAM_LANGUAGE};positive;integration")

    if (PARAM_COMPARE_STDOUT) # This flag will enable additional checking against <testname>-<language>.out
        # Otherwise, it will be unset and the script won't do anything additional
        set(EXTRA ${PARAM_LANGUAGE})
    endif()

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
                                 SOUFFLE_PARAMS "-s" "${PARAM_LANGUAGE}")

    if (PARAM_LANGUAGE STREQUAL "python")
        souffle_run_python_swig_test(TEST_NAME ${PARAM_TEST_NAME}
                                     QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                     INPUT_DIR ${INPUT_DIR}
                                     OUTPUT_DIR ${OUTPUT_DIR}
                                     FIXTURE_NAME ${FIXTURE_NAME}
                                     FACTS_DIR "${FACTS_DIR}"
                                     TEST_LABELS ${TEST_LABELS})

    elseif (PARAM_LANGUAGE STREQUAL "java")
        souffle_run_java_swig_test(TEST_NAME ${PARAM_TEST_NAME}
                                    QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                    INPUT_DIR ${INPUT_DIR}
                                    OUTPUT_DIR ${OUTPUT_DIR}
                                    FIXTURE_NAME ${FIXTURE_NAME}
                                    FACTS_DIR "${FACTS_DIR}"
                                    TEST_LABELS ${TEST_LABELS})
    else()
        message(FATAL_ERROR "Unknown swig language ${PARAM_LANGUAGE}")
    endif()

    souffle_compare_std_outputs(TEST_NAME ${PARAM_TEST_NAME}
                                 QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                 OUTPUT_DIR ${OUTPUT_DIR}
                                 EXTRA_DATA ${EXTRA}
                                 RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_swig
                                 TEST_LABELS ${TEST_LABELS})

    souffle_compare_csv(QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                        INPUT_DIR ${INPUT_DIR}
                        OUTPUT_DIR ${OUTPUT_DIR}
                        RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_swig
                        NEGATIVE ${PARAM_NEGATIVE}
                        TEST_LABELS ${TEST_LABELS})




endfunction()

# swig test which will run python, java or both
function(SOUFFLE_POSITIVE_SWIG_TEST TEST_NAME)
    if (SOUFFLE_SWIG_PYTHON)
        souffle_run_swig_test_helper(TEST_NAME ${TEST_NAME} LANGUAGE python ${ARGN})
    endif()

    if (SOUFFLE_SWIG_JAVA)
        souffle_run_swig_test_helper(TEST_NAME ${TEST_NAME} LANGUAGE java ${ARGN})
    endif()
endfunction()

souffle_positive_swig_test(dump_output COMPARE_STDOUT)
souffle_positive_swig_test(family COMPARE_STDOUT)
souffle_positive_swig_test(flights)
souffle_positive_swig_test(insert_for)
souffle_positive_swig_test(movies)
souffle_positive_swig_test(paths)
