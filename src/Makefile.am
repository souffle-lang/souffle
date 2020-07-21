# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUBDIRS = . tests

SUFFIXES = .cpp .h .yy .ll .cc .hh .h

bin_PROGRAMS = souffle souffle-profile

nodist_souffle_profile_SOURCES = $(BUILT_SOURCES)

souffle_profile_sources = \
        ProfileDatabase.h                                  \
        json11.h                                           \
        profile/Cell.h                                     \
        profile/CellInterface.h                            \
        profile/Cli.h                                      \
        profile/DataComparator.h                           \
        profile/HtmlGenerator.h                            \
        profile/Iteration.h                                \
        profile/OutputProcessor.h                          \
        profile/ProgramRun.h                               \
        profile/Reader.h                                   \
        profile/Relation.h                                 \
        profile/Row.h                                      \
        profile/Rule.h                                     \
        profile/StringUtils.h                              \
        profile/Table.h                                    \
        profile/Tui.h                                      \
        profile/UserInputReader.h                          \
        profile/htmlCssChartist.h                          \
        profile/htmlCssStyle.h                             \
        profile/htmlJsChartistMin.h                        \
        profile/htmlJsChartistPlugin.h                     \
        profile/htmlJsMain.h                               \
        profile/htmlJsTableSort.h                          \
        profile/htmlJsUtil.h                               \
        profile/htmlMain.h

souffle_swig_sources = \
        swig/SwigInterface.h                               \
        swig/SwigInterface.i

souffle_utility_sources = \
        utility/CacheUtil.h                                \
        utility/ContainerUtil.h                            \
        utility/FileUtil.h                                 \
        utility/EvaluatorUtil.h                            \
        utility/FunctionalUtil.h                           \
        utility/MiscUtil.h                                 \
        utility/ParallelUtil.h                             \
        utility/StreamUtil.h                               \
        utility/StringUtil.h                               \
        utility/tinyformat.h

DIR := ${CURDIR}

# ... which should no go to distribution
nodist_souffle_SOURCES = $(BUILT_SOURCES)

if LIBZ
libz_sources = gzfstream.h
endif

if SQLITE
sqlite_sources = ReadStreamSQLite.h WriteStreamSQLite.h
endif


souffle_sources = \
        AggregateOp.h                                      \
        AstTranslator.cpp                                  \
        AstTranslator.h                                    \
        BinaryConstraintOps.h                              \
        Constraints.h                                      \
        DebugReport.cpp                                    \
        DebugReport.h                                      \
        ErrorReport.h                                      \
        EventProcessor.h                                   \
        FunctorOps.cpp                                     \
        FunctorOps.h                                       \
        Global.cpp                                         \
        Global.h                                           \
        GraphUtils.h                                       \
        IOSystem.h                                         \
        InterpreterContext.h                               \
        InterpreterEngine.cpp                              \
        InterpreterEngine.h                                \
        InterpreterGenerator.h                             \
        InterpreterIndex.cpp                               \
        InterpreterIndex.h                                 \
        InterpreterNode.h                                  \
        InterpreterPreamble.h                              \
        InterpreterProgInterface.h                         \
        InterpreterRelation.cpp                            \
        InterpreterRelation.h                              \
        LogStatement.h                                     \
        ParserDriver.cpp                                   \
        ParserDriver.h                                     \
        ProfileEvent.h                                     \
        RamTypes.h                                         \
        ReadStream.h                                       \
        ReadStreamCSV.h                                    \
        ReadStreamJSON.h                                   \
        RecordTable.h                                      \
        RelationTag.h                                      \
        SerialisationStream.h                              \
        SignalHandler.h                                    \
        SrcLocation.cpp                                    \
        SrcLocation.h                                      \
        Synthesiser.cpp                                    \
        Synthesiser.h                                      \
        SynthesiserRelation.cpp                            \
        SynthesiserRelation.h                              \
        Util.cpp                                           \
        WriteStream.h                                      \
        WriteStreamCSV.h                                   \
        WriteStreamJSON.h                                  \
        ast/AstAbstract.h                                  \
        ast/AstArgument.h                                  \
        ast/AstAttribute.h                                 \
        ast/AstClause.h                                    \
        ast/AstComponent.h                                 \
        ast/AstFunctorDeclaration.h                        \
        ast/AstIO.h                                        \
        ast/AstLiteral.h                                   \
        ast/AstNode.h                                      \
        ast/AstParserUtils.cpp                             \
        ast/AstParserUtils.h                               \
        ast/AstPragma.h                                    \
        ast/AstProgram.h                                   \
        ast/AstQualifiedName.h                             \
        ast/AstRelation.h                                  \
        ast/AstTranslationUnit.h                           \
        ast/AstType.h                                      \
        ast/AstUtils.cpp                                   \
        ast/AstUtils.h                                     \
        ast/AstVisitor.h                                   \
        ast/TypeSystem.cpp                                 \
        ast/TypeSystem.h                                   \
        ast/analysis/AstAnalysis.h                         \
        ast/analysis/AstConstraintAnalysis.h               \
        ast/analysis/AstGroundAnalysis.cpp                 \
        ast/analysis/AstGroundAnalysis.h                   \
        ast/analysis/AstIOTypeAnalysis.cpp                 \
        ast/analysis/AstIOTypeAnalysis.h                   \
        ast/analysis/AstProfileUse.cpp                     \
        ast/analysis/AstProfileUse.h                       \
        ast/analysis/AstTypeAnalysis.cpp                   \
        ast/analysis/AstTypeAnalysis.h                     \
        ast/analysis/AstTypeEnvironmentAnalysis.cpp        \
        ast/analysis/AstTypeEnvironmentAnalysis.h          \
        ast/analysis/AuxArityAnalysis.cpp                  \
        ast/analysis/AuxArityAnalysis.h                    \
        ast/analysis/ComponentLookupAnalysis.cpp           \
        ast/analysis/ComponentLookupAnalysis.h             \
        ast/analysis/PrecedenceGraph.cpp                   \
        ast/analysis/PrecedenceGraph.h                     \
        ast/transform/AstComponentChecker.cpp              \
        ast/transform/AstComponentChecker.h                \
        ast/transform/AstPragmaChecker.cpp                 \
        ast/transform/AstPragmaChecker.h                   \
        ast/transform/AstSemanticChecker.cpp               \
        ast/transform/AstSemanticChecker.h                 \
        ast/transform/AstTransformer.cpp                   \
        ast/transform/AstTransformer.h                     \
        ast/transform/AstTransforms.cpp                    \
        ast/transform/AstTransforms.h                      \
        ast/transform/ComponentInstantiationTransformer.cpp  \
        ast/transform/ComponentInstantiationTransformer.h  \
        ast/transform/DebugReporter.cpp                    \
        ast/transform/DebugReporter.h                      \
        ast/transform/IOAttributesTransformer.h            \
        ast/transform/IODefaultsTransformer.h              \
        ast/transform/InlineRelationsTransformer.cpp       \
        ast/transform/MagicSet.cpp                         \
        ast/transform/MagicSet.h                           \
        ast/transform/MinimiseProgramTransformer.cpp       \
        ast/transform/ProvenanceTransformer.cpp            \
        ast/transform/ReorderLiteralsTransformer.cpp       \
        ast/transform/ResolveAliasesTransformer.cpp        \
        parser.cc                                          \
        parser.hh                                          \
        ram/RamCondition.h                                 \
        ram/RamExpression.h                                \
        ram/RamNode.h                                      \
        ram/RamOperation.h                                 \
        ram/RamProgram.h                                   \
        ram/RamRelation.h                                  \
        ram/RamStatement.h                                 \
        ram/RamTranslationUnit.h                           \
        ram/RamUtils.h                                     \
        ram/RamVisitor.h                                   \
        ram/analysis/RamAnalysis.h                         \
        ram/analysis/RamComplexityAnalysis.cpp             \
        ram/analysis/RamComplexityAnalysis.h               \
        ram/analysis/RamIndexAnalysis.cpp                  \
        ram/analysis/RamIndexAnalysis.h                    \
        ram/analysis/RamLevelAnalysis.cpp                  \
        ram/analysis/RamLevelAnalysis.h                    \
        ram/transform/RamTransformer.cpp                   \
        ram/transform/RamTransformer.h                     \
        ram/transform/RamTransforms.cpp                    \
        ram/transform/RamTransforms.h                      \
        scanner.cc                                         \
        stack.hh                                           \
        $(sqlite_sources)                                  \
        $(libz_sources)                                    \
        $(souffle_utility_sources)                         \
        $(souffle_profile_sources)

# -- build souffle as a library so it can be reused in testing
noinst_LTLIBRARIES = libsouffle.la
libsouffle_la_SOURCES  = $(souffle_sources)
libsouffle_la_CXXFLAGS = $(souffle_CPPFLAGS) $(FFI_CFLAGS)
libsouffle_la_LDFLAGS = --static --dlopen --pic -ldl -lffi

souffle_SOURCES = main.cpp
souffle_LDADD = libsouffle.la

souffle_profile_SOURCES = souffle_prof.cpp

dist_bin_SCRIPTS = souffle-compile souffle-config

EXTRA_DIST = parser.yy scanner.ll

soufflepublicdir = $(includedir)/souffle

soufflepublic_HEADERS = \
        BTree.h                                            \
        BinaryConstraintOps.h                              \
        Brie.h                                             \
        CompiledOptions.h                                  \
        CompiledSouffle.h                                  \
        CompiledTuple.h                                    \
        EquivalenceRelation.h                              \
        EventProcessor.h                                   \
        Explain.h                                          \
        ExplainProvenance.h                                \
        ExplainProvenanceImpl.h                            \
        ExplainTree.h                                      \
        IOSystem.h                                         \
        LambdaBTree.h                                      \
        Logger.h                                           \
        PiggyList.h                                        \
        ProfileDatabase.h                                  \
        ProfileEvent.h                                     \
        RamTypes.h                                         \
        ReadStream.h                                       \
        ReadStreamCSV.h                                    \
        ReadStreamJSON.h                                   \
        RecordTable.h                                      \
        SerialisationStream.h                              \
        SignalHandler.h                                    \
        SouffleInterface.h                                 \
        SymbolTable.h                                      \
        Table.h                                            \
        UnionFind.h                                        \
        WriteStream.h                                      \
        WriteStreamCSV.h                                   \
        WriteStreamJSON.h                                  \
        json11.h                                           \
        $(souffle_utility_sources)                         \
        $(libz_sources)                                    \
        $(sqlite_sources)

souffleprofiledir = $(soufflepublicdir)/profile

souffleprofile_HEADERS = $(souffle_profile_sources)

souffleswigdir = $(soufflepublicdir)/swig

souffleswig_HEADERS = $(souffle_swig_sources)

souffleutilitydir = $(soufflepublicdir)/utility

souffleutility_HEADERS = $(souffle_utility_sources)

# files to clean
CLEANFILES = $(BUILT_SOURCES)  parser.cc scanner.cc parser.hh stack.hh

# run Bison
$(builddir)/parser.hh: $(srcdir)/parser.yy
	$(BISON) -Wall -Werror -Wno-error=deprecated -Wno-error=other -v -d -o parser.cc $(srcdir)/parser.yy

# and FLEX
$(builddir)/scanner.cc: $(srcdir)/scanner.ll
	$(FLEX) -o scanner.cc $(srcdir)/scanner.ll

# driver depends on the generated header
$(builddir)/parser.cc $(builddir)/stack.hh $(builddir)/scanner.cc \
    $(builddir)/main.cpp $(builddir)/ParserDriver.cpp: $(builddir)/parser.hh

