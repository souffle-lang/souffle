# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

function(SOUFFLE_ADD_SCHEDULER_TEST TEST_NAME)
    set(INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}")
    set(FACTS_DIR "${INPUT_DIR}/facts")
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}")
    set(TEST_LABELS "scheduler;positive;integration")

    # Setup test dir
    set(QUALIFIED_TEST_NAME scheduler/${TEST_NAME})
    set(FIXTURE_NAME ${QUALIFIED_TEST_NAME}_fixture)
    souffle_setup_integration_test_dir(TEST_NAME ${TEST_NAME}
                                       QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                       DATA_CHECK_DIR ${INPUT_DIR}
                                       OUTPUT_DIR ${OUTPUT_DIR}
                                       FIXTURE_NAME ${FIXTURE_NAME}
                                       TEST_LABELS ${TEST_LABELS})

    set(QUALIFIED_TEST_NAME scheduler/${TEST_NAME}_stats_collection)
    # Run stats collection
    set(SOUFFLE_PARAMS "-p" "${OUTPUT_DIR}/${TEST_NAME}.prof" "--emit-statistics")
    add_test(NAME ${QUALIFIED_TEST_NAME}
      COMMAND
      ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/redirect.py
        --out ${TEST_NAME}.out
        --err ${TEST_NAME}.err
        ${STDIN_ARGS}
        $<TARGET_FILE:souffle>
        ${SOUFFLE_PARAMS}
        "${INPUT_DIR}/${TEST_NAME}.dl"
      COMMAND_EXPAND_LISTS)

    set_tests_properties(${QUALIFIED_TEST_NAME} PROPERTIES
      WORKING_DIRECTORY "${OUTPUT_DIR}"
      LABELS "${TEST_LABELS}"
      FIXTURES_SETUP ${FIXTURE_NAME}_stats_collection
      FIXTURES_REQUIRED ${FIXTURE_NAME}_setup)

    # Check output
    souffle_compare_std_outputs(TEST_NAME ${TEST_NAME}
                                 QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                 OUTPUT_DIR ${OUTPUT_DIR}
                                 RUN_AFTER_FIXTURE ${FIXTURE_NAME}_stats_collection
                                 TEST_LABELS ${TEST_LABELS})

    souffle_compare_csv(QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                        INPUT_DIR ${INPUT_DIR}
                        OUTPUT_DIR ${OUTPUT_DIR}
                        RUN_AFTER_FIXTURE ${FIXTURE_NAME}_stats_collection
                        NEGATIVE ${PARAM_NEGATIVE}
                        TEST_LABELS ${TEST_LABELS})

    # Run scheduler
    set(QUALIFIED_TEST_NAME scheduler/${TEST_NAME}_auto_scheduler)
    set(SOUFFLE_PARAMS "--auto-schedule" "${OUTPUT_DIR}/${TEST_NAME}.prof" "-c")
    add_test(NAME ${QUALIFIED_TEST_NAME}
      COMMAND
      ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/redirect.py
        --out ${TEST_NAME}.out
        --err ${TEST_NAME}.err
        ${STDIN_ARGS}
        $<TARGET_FILE:souffle>
        ${SOUFFLE_PARAMS}
        "${INPUT_DIR}/${TEST_NAME}.dl"
      COMMAND_EXPAND_LISTS)

    set_tests_properties(${QUALIFIED_TEST_NAME} PROPERTIES
      WORKING_DIRECTORY "${OUTPUT_DIR}"
      LABELS "${TEST_LABELS}"
      FIXTURES_SETUP ${FIXTURE_NAME}_auto_scheduler
      FIXTURES_REQUIRED ${FIXTURE_NAME}_stats_collection)

    # Check output
    souffle_compare_std_outputs(TEST_NAME ${TEST_NAME}
                                 QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                                 OUTPUT_DIR ${OUTPUT_DIR}
                                 RUN_AFTER_FIXTURE ${FIXTURE_NAME}_auto_scheduler
                                 TEST_LABELS ${TEST_LABELS})

    souffle_compare_csv(QUALIFIED_TEST_NAME ${QUALIFIED_TEST_NAME}
                        INPUT_DIR ${INPUT_DIR}
                        OUTPUT_DIR ${OUTPUT_DIR}
                        RUN_AFTER_FIXTURE ${FIXTURE_NAME}_auto_scheduler
                        NEGATIVE ${PARAM_NEGATIVE}
                        TEST_LABELS ${TEST_LABELS})
endfunction()

if (NOT MSVC)
    souffle_add_scheduler_test(functionality)
    souffle_add_scheduler_test(eqrel)
endif()
