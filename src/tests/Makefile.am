# Souffle - A Datalog Compiler
# Copyright (c) 2020, The Souffle Developers. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt


SUFFIXES = .cpp .h .yy .ll .cc .hh .h

CXXFLAGS += -I${top_srcdir}/src

########## Unit Tests

AM_COLOR_TESTS=always

# -------------------------

# init check programs
check_PROGRAMS =

# test description
# check_PROGRAMS += testname
# test_testname_SOURCES = cppfile test.h
# if needed
# test_testname_LDADD = ../libsouffle.la
# test_testname_CXXFLAGS = test-specific flags

# profile utilities test
check_PROGRAMS += profile_util_test
profile_util_test_SOURCES = profile_util_test.cpp test.h

# utils test
check_PROGRAMS += util_test
util_test_SOURCES = util_test.cpp test.h

# matching test
check_PROGRAMS += matching_test
matching_test_SOURCES = matching_test.cpp test.h
matching_test_LDADD = ../libsouffle.la

# table test
check_PROGRAMS += table_test
table_test_SOURCES = table_test.cpp test.h

# b-tree set test
check_PROGRAMS += btree_set_test
btree_set_test_SOURCES = btree_set_test.cpp test.h

# b-tree multi-set test
check_PROGRAMS += btree_multiset_test
btree_multiset_test_SOURCES = btree_multiset_test.cpp test.h

# binary relation tests
check_PROGRAMS += binary_relation_test
binary_relation_test_SOURCES = binary_relation_test.cpp test.h

# pnappa's good ol fashioned data structure tests (auxilliary structures to binrel, and potentially useful outside)
check_PROGRAMS += eqrel_datastructure_test
eqrel_datastructure_test_SOURCES = eqrel_datastructure_test.cpp test.h

# compiled ram tuple test
check_PROGRAMS += compiled_tuple_test
compiled_tuple_test_SOURCES = compiled_tuple_test.cpp test.h

# interpreter relation test
check_PROGRAMS += interpreter_relation_test
interpreter_relation_test_SOURCES = interpreter_relation_test.cpp test.h
interpreter_relation_test_LDADD = ../libsouffle.la

# type system test
check_PROGRAMS += type_system_test
type_system_test_SOURCES = type_system_test.cpp test.h
type_system_test_LDADD = ../libsouffle.la

# constraints test
check_PROGRAMS += constraints_test
constraints_test_SOURCES = constraints_test.cpp test.h
constraints_test_LDADD = ../libsouffle.la

# ast print test
check_PROGRAMS += ast_print_test
ast_print_test_SOURCES = ast_print_test.cpp test.h
ast_print_test_LDADD = ../libsouffle.la

# ast program test
check_PROGRAMS += ast_program_test
ast_program_test_SOURCES = ast_program_test.cpp test.h
ast_program_test_LDADD = ../libsouffle.la

# ast transformers test
check_PROGRAMS += ast_transformers_test
ast_transformers_test_SOURCES = ast_transformers_test.cpp test.h
ast_transformers_test_LDADD = ../libsouffle.la

# ast utils test
check_PROGRAMS += ast_utils_test
ast_utils_test_SOURCES = ast_utils_test.cpp test.h
ast_utils_test_LDADD = ../libsouffle.la

# ast parser utils test
check_PROGRAMS += ast_parser_utils_test
ast_parser_utils_test_SOURCES = ast_parser_utils_test.cpp test.h
ast_parser_utils_test_LDADD = ../libsouffle.la

# symbol table
check_PROGRAMS += symbol_table_test
symbol_table_test_SOURCES = symbol_table_test.cpp test.h

# graph utils
check_PROGRAMS += graph_utils_test
graph_utils_test_SOURCES = graph_utils_test.cpp test.h

# trie implementation
check_PROGRAMS += brie_test
brie_test_SOURCES = brie_test.cpp test.h

# parallel utils implementation
check_PROGRAMS += parallel_utils_test
parallel_utils_test_SOURCES = parallel_utils_test.cpp test.h

# interpreter relation test
check_PROGRAMS += ram_condition_equal_clone_test
ram_condition_equal_clone_test_SOURCES = ram_condition_equal_clone_test.cpp test.h
ram_condition_equal_clone_test_LDADD = ../libsouffle.la

check_PROGRAMS += ram_statement_equal_clone_test
ram_statement_equal_clone_test_SOURCES = ram_statement_equal_clone_test.cpp test.h
ram_statement_equal_clone_test_LDADD = ../libsouffle.la

check_PROGRAMS += ram_expression_equal_clone_test
ram_expression_equal_clone_test_SOURCES = ram_expression_equal_clone_test.cpp test.h
ram_expression_equal_clone_test_LDADD = ../libsouffle.la

check_PROGRAMS += ram_operation_equal_clone_test
ram_operation_equal_clone_test_SOURCES = ram_operation_equal_clone_test.cpp test.h
ram_operation_equal_clone_test_LDADD = ../libsouffle.la

check_PROGRAMS += ram_relation_equal_clone_test
ram_relation_equal_clone_test_SOURCES = ram_relation_equal_clone_test.cpp test.h
ram_relation_equal_clone_test_LDADD = ../libsouffle.la

# relation test
check_PROGRAMS += ram_relation_test
ram_relation_test_SOURCES = ram_relation_test.cpp test.h
ram_relation_test_LDADD = ../libsouffle.la

# arithmetic test
check_PROGRAMS += ram_arithmetic_test
ram_arithmetic_test_SOURCES = ram_arithmetic_test.cpp test.h
ram_arithmetic_test_LDADD = ../libsouffle.la

check_PROGRAMS += ram_type_conversion_test
ram_type_conversion_test_SOURCES = ram_type_conversion_test.cpp test.h
ram_type_conversion_test_LDADD = ../libsouffle.la

check_PROGRAMS += record_table_test
record_table_test_SOURCES = record_table_test.cpp test.h

# make all check-programs tests
TESTS = $(check_PROGRAMS)
