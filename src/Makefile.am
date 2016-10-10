# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUBDIRS = wavelib

SUFFIXES = .cpp .h .yy .ll .cc .hh .hpp

bin_PROGRAMS = souffle souffle-wave

# dependenceis of wave

# ... which should no go to distribution
nodist_souffle_wave_SOURCES = $(BUILT_SOURCES)

souffle_wave_SOURCES = wave.cpp wave_trace_macro_expansion.hpp \
		wave_stop_watch.hpp

souffle_wave_CPPFLAGS = $(BOOST_CPPFLAGS) -DPACKAGE_VERSION="\"${PACKAGE_VERSION}\""
souffle_wave_LDFLAGS =
souffle_wave_LDADD = $(BOOST_LDFLAGS) \
             $(BOOST_FILESYSTEM_LIB) \
             wavelib/libwave.a \
             $(BOOST_SYSTEM_LIB) \
             $(BOOST_THREAD_LIB) \
             $(BOOST_DATE_TIME_LIB)

# ... which should no go to distribution
nodist_souffle_SOURCES = $(BUILT_SOURCES)

souffle_sources = \
              parser.cc             parser.hh           \
              scanner.cc            stack.hh            \
              ParserDriver.cpp      ParserDriver.h      \
              AstArgument.cpp       AstArgument.h       \
              AstAttribute.h                            \
              AstClause.cpp         AstClause.h         \
              AstComponent.h                            \
              AstLiteral.h                              \
              AstNode.h                                 \
              AstParserUtils.cpp    AstParserUtils.h    \
              AstProgram.cpp        AstProgram.h        \
              AstRelation.h                             \
              AstRelationIdentifier.h                   \
              AstSrcLocation.cpp    AstSrcLocation.h    \
              AstTypeAnalysis.cpp   AstTypeAnalysis.h   \
              AstTuner.cpp          AstTuner.h          \
              AstType.h                                 \
              AstUtils.cpp          AstUtils.h          \
              AstVisitor.h                              \
              AstAnalysis.h                             \
              AstTransformer.cpp    AstTransformer.h    \
              AstTranslationUnit.h  ErrorReport.h       \
              DebugReport.cpp       DebugReport.h       \
              ComponentModel.cpp    ComponentModel.h    \
              AstSemanticChecker.cpp                    \
              AstSemanticChecker.h                      \
              AstTransforms.cpp     AstTransforms.h     \
              BinaryOperator.h                          \
              Constraints.h                             \
              GraphUtils.h                              \
              RamValue.h                                \
              RamIndex.h                                \
              RamCondition.h                            \
              RamTranslator.cpp     RamTranslator.h     \
              RamExecutor.cpp       RamExecutor.h       \
              RamStatement.h                            \
              RamMaxMatching.cpp    RamMaxMatching.h    \
              RamNode.h                                 \
              RamData.h                                 \
              RamAutoIndex.cpp      RamAutoIndex.h      \
              RamOperation.cpp      RamOperation.h      \
              RamRecords.cpp        RamRecords.h        \
              RamRelation.cpp       RamRelation.h       \
              RamRelationStats.cpp  RamRelationStats.h  \
              RamVisitor.h                              \
              RuleScheduler.h                           \
              StringPool.h                              \
              TypeSystem.cpp        TypeSystem.h        \
              PrecedenceGraph.cpp   PrecedenceGraph.h

# -- build souffle as a library so it can be reused in testing
noinst_LIBRARIES = libsouffle.a
libsouffle_a_SOURCES = $(souffle_sources)
libsouffle_a_CXXFLAGS = $(souffle_CPPFLAGS)

souffle_SOURCES = main.cpp
souffle_LDADD = libsouffle.a

dist_bin_SCRIPTS = souffle-compile souffle-config

EXTRA_DIST = parser.yy scanner.ll  test/test.h

soufflepublicdir = $(includedir)/souffle

soufflepublic_HEADERS = CompiledRamOptions.h CompiledRamRelation.h CompiledRamTuple.h CompiledRamRecord.h CompiledSouffle.h \
				  SouffleInterface.h ParallelUtils.h RamTypes.h BTree.h Trie.h Table.h Util.h IterUtils.h SymbolTable.h RamLogger.h \
				  SqliteRelationWriter.h

souffle_CPPFLAGS = -DPACKAGE_VERSION="\"${PACKAGE_VERSION}\""

# files to clean
CLEANFILES = $(BUILT_SOURCES)  parser.cc scanner.cc parser.hh stack.hh

# run Bison
$(builddir)/parser.hh $(builddir)/parser.cc $(builddir)/stack.hh: $(srcdir)/parser.yy
	$(BISON) -d -o parser.cc $(srcdir)/parser.yy

# and FLEX
$(builddir)/scanner.cc: $(srcdir)/scanner.ll
	$(FLEX) -o scanner.cc $(srcdir)/scanner.ll

# driver depends on the generated header
$(builddir)/ParserDriver.cpp: $(builddir)/parser.hh

########## Unit Tests

AM_COLOR_TESTS=always

# -------------------------

# init check programs
check_PROGRAMS =

# utils test
check_PROGRAMS += test/util_test
test_util_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_util_test_SOURCES = test/util_test.cpp
test_util_test_LDADD = libsouffle.a

# matching test
check_PROGRAMS += test/matching_test
test_matching_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_matching_test_SOURCES = test/matching_test.cpp
test_matching_test_LDADD = libsouffle.a

# table test
check_PROGRAMS += test/table_test
test_table_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_table_test_SOURCES = test/table_test.cpp
test_table_test_LDADD = libsouffle.a

# b-tree set test
check_PROGRAMS += test/btree_set_test
test_btree_set_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_btree_set_test_SOURCES = test/btree_set_test.cpp
test_btree_set_test_LDADD = libsouffle.a

# b-tree multi-set test
check_PROGRAMS += test/btree_multiset_test
test_btree_multiset_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_btree_multiset_test_SOURCES = test/btree_multiset_test.cpp
test_btree_multiset_test_LDADD = libsouffle.a

# compiled ram tuple test
check_PROGRAMS += test/compiled_ram_tuple_test
test_compiled_ram_tuple_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_compiled_ram_tuple_test_SOURCES = test/compiled_ram_tuple_test.cpp
test_compiled_ram_tuple_test_LDADD = libsouffle.a

# compiled ram relation test
check_PROGRAMS += test/compiled_ram_relation_test
test_compiled_ram_relation_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_compiled_ram_relation_test_SOURCES = test/compiled_ram_relation_test.cpp
test_compiled_ram_relation_test_LDADD = libsouffle.a

# type system test
check_PROGRAMS += test/type_system_test
test_type_system_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_type_system_test_SOURCES = test/type_system_test.cpp
test_type_system_test_LDADD = libsouffle.a

# constraints test
check_PROGRAMS += test/constraints_test
test_constraints_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_constraints_test_SOURCES = test/constraints_test.cpp
test_constraints_test_LDADD = libsouffle.a

# ast program test
check_PROGRAMS += test/ast_program_test
test_ast_program_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_program_test_SOURCES = test/ast_program_test.cpp
test_ast_program_test_LDADD = libsouffle.a

# ast utils test
check_PROGRAMS += test/ast_utils_test
test_ast_utils_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_utils_test_SOURCES = test/ast_utils_test.cpp
test_ast_utils_test_LDADD = libsouffle.a

# ast parser utils test
check_PROGRAMS += test/ast_parser_utils_test
test_ast_parser_utils_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_parser_utils_test_SOURCES = test/ast_parser_utils_test.cpp
test_ast_parser_utils_test_LDADD = libsouffle.a

# rule scheduler test
check_PROGRAMS += test/rule_scheduler_test
test_rule_scheduler_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_rule_scheduler_test_SOURCES = test/rule_scheduler_test.cpp
test_rule_scheduler_test_LDADD = libsouffle.a

# relation statistics
check_PROGRAMS += test/ram_relation_stats_test
test_ram_relation_stats_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_ram_relation_stats_test_SOURCES = test/ram_relation_stats_test.cpp
test_ram_relation_stats_test_LDADD = libsouffle.a

# symbol table
check_PROGRAMS += test/symbol_table_test
test_symbol_table_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_symbol_table_test_SOURCES = test/symbol_table_test.cpp
test_symbol_table_test_LDADD = libsouffle.a

# graph utils
check_PROGRAMS += test/graph_utils_test
test_graph_utils_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_graph_utils_test_SOURCES = test/graph_utils_test.cpp
test_graph_utils_test_LDADD = libsouffle.a

# trie implementation
check_PROGRAMS += test/trie_test
test_trie_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_trie_test_SOURCES = test/trie_test.cpp
test_trie_test_LDADD = libsouffle.a

# parallel utils implementation
check_PROGRAMS += test/parallel_utils_test
test_parallel_utils_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_parallel_utils_test_SOURCES = test/parallel_utils_test.cpp
test_parallel_utils_test_LDADD = libsouffle.a



# make all check-programs tests
TESTS = $(check_PROGRAMS)

