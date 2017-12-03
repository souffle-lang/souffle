# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt


SUFFIXES = .cpp .h .yy .ll .cc .hh .hpp

bin_PROGRAMS = souffle souffle-mcpp souffle-profile

nodist_souffle_profile_SOURCES = $(BUILT_SOURCES)

souffle_profile_SOURCES = souffle_prof.cpp profilerlib/Cell.hpp \
                          profilerlib/CellInterface.hpp         \
                          profilerlib/Cli.cpp                   \
                          profilerlib/Cli.hpp                   \
                          profilerlib/DataComparator.hpp        \
                          profilerlib/Iteration.cpp             \
                          profilerlib/Iteration.hpp             \
                          profilerlib/OutputProcessor.cpp       \
                          profilerlib/OutputProcessor.hpp       \
                          profilerlib/ProgramRun.cpp            \
                          profilerlib/ProgramRun.hpp            \
                          profilerlib/Reader.cpp                \
                          profilerlib/Reader.hpp                \
                          profilerlib/Relation.cpp              \
                          profilerlib/Relation.hpp              \
                          profilerlib/Row.hpp                   \
                          profilerlib/Rule.cpp                  \
                          profilerlib/Rule.hpp                  \
                          profilerlib/StringUtils.cpp           \
                          profilerlib/StringUtils.hpp           \
                          profilerlib/Table.cpp                 \
                          profilerlib/Table.hpp                 \
                          profilerlib/Tui.cpp                   \
                          profilerlib/Tui.hpp                   \
                          profilerlib/html_string.hpp           \
                          profilerlib/UserInputReader.cpp       \
                          profilerlib/UserInputReader.hpp

DIR := ${CURDIR}

souffle_profile_CXXFLAGS = $(souffle_CPPFLAGS) -DMAKEDIR='"$(DIR)"'

nodist_souffle_mcpp_SOURCES = $(BUILT_SOURCES)

souffle_mcpp_SOURCES = mcpp-system.H mcpp-configed.H mcpp-internal.H mcpp-main.c mcpp-directive.c mcpp-eval.c \
                        mcpp-expand.c mcpp-mbchar.c mcpp-support.c mcpp-system.c mcpp_out.h

mcpp-main.o mcpp-directive.o mcpp-eval.o mcpp-expand.o mcpp-mbchar.o mcpp-support.o mcpp-system.o    :    \
     mcpp-system.H mcpp-internal.H mcpp_out.h
mcpp-system.H    : mcpp-configed.H


# ... which should no go to distribution
nodist_souffle_SOURCES = $(BUILT_SOURCES)

if LIBZ
libz_sources = gzfstream.h
endif

if SQLITE
sqlite_sources = ReadStreamSQLite.h WriteStreamSQLite.h
endif

souffle_sources = \
              parser.cc             parser.hh           \
              scanner.cc            stack.hh            \
              ParserDriver.cpp      ParserDriver.h      \
              AstArgument.cpp       AstArgument.h       \
              AstAttribute.h                            \
              AstClause.cpp         AstClause.h         \
              AstComponent.h                            \
              AstPragma.cpp         AstPragma.h         \
              AstIODirective.h                          \
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
              BddbddbBackend.h      BddbddbBackend.cpp  \
              DebugReport.cpp       DebugReport.h       \
              ComponentModel.cpp    ComponentModel.h    \
              AstSemanticChecker.cpp                    \
              AstSemanticChecker.h                      \
              AstTransforms.cpp     AstTransforms.h     \
              UnaryFunctorOps.h                         \
              BinaryFunctorOps.h                        \
              TernaryFunctorOps.h                       \
              BinaryConstraintOps.h                     \
              Constraints.h                             \
              GraphUtils.h                              \
              RamValue.h                                \
              RamIndex.h                                \
              RamCondition.h                            \
              AstTranslator.cpp     AstTranslator.h     \
              RamExecutor.h                             \
              RamInterpreter.cpp    RamInterpreter.h    \
              RamSynthesiser.cpp    RamSynthesiser.h    \
              RamStatement.h                            \
              RamMaxMatching.cpp    RamMaxMatching.h    \
              RamNode.h                                 \
              RamData.h                                 \
              RamAutoIndex.cpp      RamAutoIndex.h      \
              RamOperation.cpp      RamOperation.h      \
              RamProgram.h                              \
              RamRecords.cpp        RamRecords.h        \
              RamRelation.h                             \
              RamRelationStats.cpp  RamRelationStats.h  \
              RamVisitor.h                              \
              RuleScheduler.h                           \
              SignalHandler.h                           \
              StringPool.h                              \
              TypeSystem.cpp        TypeSystem.h        \
              PrecedenceGraph.cpp   PrecedenceGraph.h   \
              MagicSet.cpp          MagicSet.h          \
              InlineRelationsTransformer.cpp            \
              $(sqlite_sources)     $(libz_sources)     \
              ReadStream.h                              \
              ReadStreamCSV.h                           \
              WriteStream.h                             \
              IODirectives.h                            \
              WriteStreamCSV.h                          \
              IOSystem.h                                \
              RamInterface.cpp      RamInterface.h      \
              ProvenanceTransformer.cpp                 \
              ProvenanceRecordTransformer.cpp ProvenanceRecordTransformer.h \
              Global.cpp            Global.h

# -- build souffle as a library so it can be reused in testing
noinst_LTLIBRARIES = libsouffle.la
libsouffle_la_SOURCES  = $(souffle_sources)
libsouffle_la_CXXFLAGS = $(souffle_CPPFLAGS)
libsouffle_la_LDFLAGS = --static --dlopen --pic

souffle_SOURCES = main.cpp
souffle_LDADD = libsouffle.la

dist_bin_SCRIPTS = souffle-compile souffle-config

EXTRA_DIST = parser.yy scanner.ll  test/test.h

soufflepublicdir = $(includedir)/souffle

soufflepublic_HEADERS = CompiledRamOptions.h    \
                        CompiledRamIndexUtils.h \
                        CompiledRamRelation.h   \
                        CompiledRamTuple.h      \
                        CompiledRamRecord.h     \
                        CompiledSouffle.h       \
                        SignalHandler.h         \
                        SouffleInterface.h      \
                        ParallelUtils.h         \
                        BTree.h                 \
                        Trie.h                  \
                        UnionFind.h             \
                        BinaryRelation.h        \
                        BlockList.h             \
                        Table.h                 \
                        Util.h                  \
                        Macro.h                 \
                        AstTypes.h              \
                        RamTypes.h              \
                        IterUtils.h             \
                        SymbolTable.h           \
                        RamLogger.h             \
                        $(sqlite_sources)       \
                        $(libz_sources)         \
                        IODirectives.h          \
                        IOSystem.h              \
                        ReadStream.h            \
                        ReadStreamCSV.h         \
                        SymbolMask.h            \
                        WriteStream.h           \
                        WriteStreamCSV.h        \
                        Explain.h               \
                        ExplainTree.h           \
                        ExplainProvenance.h     \
                        ExplainProvenanceSLD.h  \
                        ExplainProvenanceRecords.h

souffle_CPPFLAGS = -DPACKAGE_VERSION="\"${PACKAGE_VERSION}\""

# files to clean
CLEANFILES = $(BUILT_SOURCES)  parser.cc scanner.cc parser.hh stack.hh

# run Bison
$(builddir)/parser.hh $(builddir)/stack.hh: $(srcdir)/parser.yy
	$(BISON) -Wall -Werror -v -d -o parser.cc $(srcdir)/parser.yy

# and FLEX
$(builddir)/scanner.cc: $(srcdir)/scanner.ll
	$(FLEX) -o scanner.cc $(srcdir)/scanner.ll

# driver depends on the generated header
$(builddir)/parser.cc $(builddir)/scanner.cc $(builddir)/main.cpp $(builddir)/ParserDriver.cpp: \
	$(builddir)/parser.hh

########## Unit Tests

AM_COLOR_TESTS=always

# -------------------------

# init check programs
check_PROGRAMS =

# utils test
check_PROGRAMS += test/util_test
test_util_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_util_test_SOURCES = test/util_test.cpp
test_util_test_LDADD = libsouffle.la

# matching test
check_PROGRAMS += test/matching_test
test_matching_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_matching_test_SOURCES = test/matching_test.cpp
test_matching_test_LDADD = libsouffle.la

# table test
check_PROGRAMS += test/table_test
test_table_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_table_test_SOURCES = test/table_test.cpp
test_table_test_LDADD = libsouffle.la

# b-tree set test
check_PROGRAMS += test/btree_set_test
test_btree_set_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_btree_set_test_SOURCES = test/btree_set_test.cpp
test_btree_set_test_LDADD = libsouffle.la

# b-tree multi-set test
check_PROGRAMS += test/btree_multiset_test
test_btree_multiset_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_btree_multiset_test_SOURCES = test/btree_multiset_test.cpp
test_btree_multiset_test_LDADD = libsouffle.la

# binary relation tests
check_PROGRAMS += test/binary_relation_test
test_binary_relation_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_binary_relation_test_SOURCES = test/binary_relation_test.cpp
test_binary_relation_test_LDADD = libsouffle.la

# compiled ram tuple test
check_PROGRAMS += test/compiled_ram_tuple_test
test_compiled_ram_tuple_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_compiled_ram_tuple_test_SOURCES = test/compiled_ram_tuple_test.cpp
test_compiled_ram_tuple_test_LDADD = libsouffle.la

# compiled ram index utils test
check_PROGRAMS += test/compiled_ram_index_utils_test
test_compiled_ram_index_utils_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_compiled_ram_index_utils_test_SOURCES = test/compiled_ram_index_utils_test.cpp
test_compiled_ram_index_utils_test_LDADD = libsouffle.la

# compiled ram relation test
check_PROGRAMS += test/compiled_ram_relation_test
test_compiled_ram_relation_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_compiled_ram_relation_test_SOURCES = test/compiled_ram_relation_test.cpp
test_compiled_ram_relation_test_LDADD = libsouffle.la

# type system test
check_PROGRAMS += test/type_system_test
test_type_system_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_type_system_test_SOURCES = test/type_system_test.cpp
test_type_system_test_LDADD = libsouffle.la

# constraints test
check_PROGRAMS += test/constraints_test
test_constraints_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_constraints_test_SOURCES = test/constraints_test.cpp
test_constraints_test_LDADD = libsouffle.la

# ast program test
check_PROGRAMS += test/ast_program_test
test_ast_program_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_program_test_SOURCES = test/ast_program_test.cpp
test_ast_program_test_LDADD = libsouffle.la

# ast utils test
check_PROGRAMS += test/ast_utils_test
test_ast_utils_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_utils_test_SOURCES = test/ast_utils_test.cpp
test_ast_utils_test_LDADD = libsouffle.la

# ast parser utils test
check_PROGRAMS += test/ast_parser_utils_test
test_ast_parser_utils_test_CXXFLAGS = $(souffle_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_ast_parser_utils_test_SOURCES = test/ast_parser_utils_test.cpp
test_ast_parser_utils_test_LDADD = libsouffle.la

# rule scheduler test
check_PROGRAMS += test/rule_scheduler_test
test_rule_scheduler_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test
test_rule_scheduler_test_SOURCES = test/rule_scheduler_test.cpp
test_rule_scheduler_test_LDADD = libsouffle.la

# relation statistics
check_PROGRAMS += test/ram_relation_stats_test
test_ram_relation_stats_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_ram_relation_stats_test_SOURCES = test/ram_relation_stats_test.cpp
test_ram_relation_stats_test_LDADD = libsouffle.la

# symbol table
check_PROGRAMS += test/symbol_table_test
test_symbol_table_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_symbol_table_test_SOURCES = test/symbol_table_test.cpp
test_symbol_table_test_LDADD = libsouffle.la

# graph utils
check_PROGRAMS += test/graph_utils_test
test_graph_utils_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_graph_utils_test_SOURCES = test/graph_utils_test.cpp
test_graph_utils_test_LDADD = libsouffle.la

# trie implementation
check_PROGRAMS += test/trie_test
test_trie_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_trie_test_SOURCES = test/trie_test.cpp
test_trie_test_LDADD = libsouffle.la

# parallel utils implementation
check_PROGRAMS += test/parallel_utils_test
test_parallel_utils_test_CXXFLAGS = $(souffle_bin_CPPFLAGS) -I @abs_top_srcdir@/src/test -DBUILDDIR='"@abs_top_builddir@/src/"'
test_parallel_utils_test_SOURCES = test/parallel_utils_test.cpp
test_parallel_utils_test_LDADD = libsouffle.la

# make all check-programs tests
TESTS = $(check_PROGRAMS)
