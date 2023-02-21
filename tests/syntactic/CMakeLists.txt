# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

function(POSITIVE_TEST NAME)
    souffle_positive_test(${NAME} syntactic ${ARGN})
endfunction()

function(NEGATIVE_TEST NAME)
    souffle_negative_test(${NAME} syntactic)
endfunction()

positive_test(binary COMPILED_SPLITTED)
positive_test(comment)
positive_test(comment2)
if (FALSE)
# Fails because warning/error reporting has incorrect column when the line contains an inline comment.
positive_test(comment3)
endif ()
positive_test(comment_rule)
positive_test(cpp_keywords)
positive_test(duplicates)
positive_test(empty)
negative_test(execution_plan)
negative_test(input)
positive_test(input_directive3 COMPILED_SPLITTED)
positive_test(input_directive4)
negative_test(input_directive_quotes2)
positive_test(input_directive_quotes4)
negative_test(input_directive_quotes)
positive_test(liberal_identifiers)
positive_test(multiple_heads)
positive_test(name_clash)
positive_test(ordered_constraints)
negative_test(output)
positive_test(output_directive1 COMPILED_SPLITTED)
positive_test(output_directive_quotes2)
positive_test(output_directive_quotes3)
negative_test(printsize)
negative_test(printsize_directive2)
negative_test(printsize_directive)
negative_test(qualifiers)
negative_test(qualifiers2)
positive_test(qualifiers3)
negative_test(qualifiers4)
positive_test(relation)
negative_test(rule)
positive_test(string)
negative_test(syntax1)
negative_test(syntax2)
negative_test(syntax3)
negative_test(syntax4)
negative_test(syntax5)
negative_test(syntax6)
negative_test(syntax7)
negative_test(syntax8)
negative_test(syntax9)
negative_test(syntax10)
positive_test(type)
positive_test(union_comp_type)
positive_test(dot_identifiers)
positive_test(input_adt_names1)
positive_test(input_adt_names2)
positive_test(input_directive_rfc4180)
positive_test(equivalent_types_in_negation)
if (NOT MSVC)
# does not pass with Visual Studio pre-processor because it preserves all whitespaces
positive_test(whitespaces)
endif ()
souffle_run_test(
  TEST_NAME include_directive1
  CATEGORY syntactic
  NO_PREPROCESSOR
  )
souffle_run_test(
  TEST_NAME include_directive2
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME include_directive3
  CATEGORY syntactic
  NO_PREPROCESSOR
  )
souffle_run_test(
  TEST_NAME include_directive4
  CATEGORY syntactic
  INCLUDE_DIRS q1 q2
  NO_PREPROCESSOR
  )
souffle_run_test(
  TEST_NAME sourceloc1
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME sourceloc2
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME sourceloc3
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME sourceloc4
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME sourceloc5
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
souffle_run_test(
  TEST_NAME unterminated_comment
  CATEGORY syntactic
  NO_PREPROCESSOR
  NEGATIVE
  )
