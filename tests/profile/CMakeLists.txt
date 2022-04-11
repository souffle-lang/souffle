# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

function(SOUFFLE_RUN_PROF_TEST)
    cmake_parse_arguments(
        PARAM
        ""
        "PARAM_TEST_NAME;QUALIFIED_TEST_NAME;INPUT_DIR;OUTPUT_DIR;FIXTURE_NAME;TEST_LABELS;FACTS_DIR"
        ""
        ${ARGV}
    )
    # Run souffle (through the shell, so we can easily redirect)


    SET(CMD_LIST "rel;rel R2;rul;rul C2.1;rul id N2.1;graph R2 copy_t;graph C2.1 tot_t;graph ver C2.1 tuples;top;help")
    foreach(CMD IN LISTS CMD_LIST)
       SET(CMD_NAME "${PARAM_QUALIFIED_TEST_NAME}_run_prof_${CMD}")

       string(REPLACE " " "_" CMD_NAME ${CMD_NAME})

       SET(CMD_EXEC "set -e$<SEMICOLON> '$<TARGET_FILE:souffleprof>' '${OUTPUT_DIR}/${TEST_NAME}.prof' -c '${CMD}' >'${CMD}.out'")

       add_test(NAME "${CMD_NAME}" COMMAND sh -c "${CMD_EXEC}")

       set_tests_properties("${CMD_NAME}" PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP "${CMD_NAME}"
                         FIXTURES_REQUIRED ${PARAM_FIXTURE_NAME}_run_souffle
                        )

       SET(CMD_NAME_CMP "${PARAM_QUALIFIED_TEST_NAME}_cmp_prof_${CMD}")

       string(REPLACE " " "_" CMD_NAME_CMP ${CMD_NAME_CMP})

       SET(CMD_EXEC "[[ `cat '${CMD}.out'|wc -l` == `cat '${INPUT_DIR}/out/${CMD}.out'|wc -l` ]]")

       add_test(NAME "${CMD_NAME_CMP}" COMMAND bash -c "${CMD_EXEC}")

       set_tests_properties("${CMD_NAME_CMP}" PROPERTIES
                         WORKING_DIRECTORY "${PARAM_OUTPUT_DIR}"
                         LABELS "${PARAM_TEST_LABELS}"
                         FIXTURES_SETUP "${CMD_NAME_CMP}"
                         FIXTURES_REQUIRED ${CMD_NAME}
                        )

    endforeach()
endfunction()



function(SOUFFLE_RUN_PROF_TEST_HELPER)
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
    set(QUALIFIED_TEST_NAME profile/${PARAM_TEST_NAME})
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
                                 SOUFFLE_PARAMS "-p" "${OUTPUT_DIR}/${TEST_NAME}.prof")

    souffle_run_prof_test(TEST_NAME ${PARAM_TEST_NAME}
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
                                 RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_prof
                                 TEST_LABELS ${TEST_LABELS})

    souffle_compare_csv(QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                        INPUT_DIR ${INPUT_DIR}
                        OUTPUT_DIR ${OUTPUT_DIR}
                        RUN_AFTER_FIXTURE ${FIXTURE_NAME}_run_prof
                        NEGATIVE ${PARAM_NEGATIVE}
                        TEST_LABELS ${TEST_LABELS})

endfunction()

# prof test which will compile Souffle programs externally
function(SOUFFLE_POSITIVE_PROF_TEST TEST_NAME)
    souffle_run_prof_test_helper(TEST_NAME ${TEST_NAME} ${ARGN})
endfunction()

if (NOT MSVC)
souffle_positive_prof_test(lrg_attr_id)
souffle_positive_prof_test(recursive)
endif ()
