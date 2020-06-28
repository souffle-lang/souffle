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
        AstAbstract.h                                      \
        AstAnalysis.h                                      \
        AstArgument.h                                      \
        AstAttribute.h                                     \
        AstClause.h                                        \
        AstComponent.h                                     \
        AstComponentChecker.cpp                            \
        AstComponentChecker.h                              \
        AstConstraintAnalysis.h                            \
        AstFunctorDeclaration.h                            \
        AstGroundAnalysis.cpp                              \
        AstGroundAnalysis.h                                \
        AstIO.h                                            \
        AstIOTypeAnalysis.cpp                              \
        AstIOTypeAnalysis.h                                \
        AstLiteral.h                                       \
        AstNode.h                                          \
        AstParserUtils.cpp                                 \
        AstParserUtils.h                                   \
        AstPragma.h                                        \
        AstPragmaChecker.cpp                               \
        AstPragmaChecker.h                                 \
        AstProfileUse.cpp                                  \
        AstProfileUse.h                                    \
        AstProgram.h                                       \
        AstQualifiedName.h                                 \
        AstRelation.h                                      \
        AstSemanticChecker.cpp                             \
        AstSemanticChecker.h                               \
        AstTransformer.cpp                                 \
        AstTransformer.h                                   \
        AstTransforms.cpp                                  \
        AstTransforms.h                                    \
        AstTranslationUnit.h                               \
        AstTranslator.cpp                                  \
        AstTranslator.h                                    \
        AstType.h                                          \
        AstTypeAnalysis.cpp                                \
        AstTypeAnalysis.h                                  \
        AstTypeEnvironmentAnalysis.cpp                     \
        AstTypeEnvironmentAnalysis.h                       \
        AstUtils.cpp                                       \
        AstUtils.h                                         \
        AstVisitor.h                                       \
        AuxArityAnalysis.cpp                               \
        AuxArityAnalysis.h                                 \
        BinaryConstraintOps.h                              \
        ComponentInstantiationTransformer.cpp              \
        ComponentInstantiationTransformer.h                \
        ComponentLookupAnalysis.cpp                        \
        ComponentLookupAnalysis.h                          \
        Constraints.h                                      \
        DebugReport.cpp                                    \
        DebugReport.h                                      \
        DebugReporter.cpp                                  \
        DebugReporter.h                                    \
        ErrorReport.h                                      \
        EventProcessor.h                                   \
        FunctorOps.cpp                                     \
        FunctorOps.h                                       \
        Global.cpp                                         \
        Global.h                                           \
        GraphUtils.h                                       \
        IOSystem.h                                         \
        InlineRelationsTransformer.cpp                     \
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
        MagicSet.cpp                                       \
        MagicSet.h                                         \
        MinimiseProgramTransformer.cpp                     \
        ParserDriver.cpp                                   \
        ParserDriver.h                                     \
        PrecedenceGraph.cpp                                \
        PrecedenceGraph.h                                  \
        ProfileEvent.h                                     \
        ProvenanceTransformer.cpp                          \
        RamAnalysis.h                                      \
        RamComplexityAnalysis.cpp                          \
        RamComplexityAnalysis.h                            \
        RamCondition.h                                     \
        RamExpression.h                                    \
        RamIndexAnalysis.cpp                               \
        RamIndexAnalysis.h                                 \
        RamLevelAnalysis.cpp                               \
        RamLevelAnalysis.h                                 \
        RamNode.h                                          \
        RamOperation.h                                     \
        RamProgram.h                                       \
        RamRelation.h                                      \
        RamStatement.h                                     \
        RamTransformer.cpp                                 \
        RamTransformer.h                                   \
        RamTransforms.cpp                                  \
        RamTransforms.h                                    \
        RamTranslationUnit.h                               \
        RamTypes.h                                         \
        RamUtils.h                                         \
        RamVisitor.h                                       \
        ReadStream.h                                       \
        ReadStreamCSV.h                                    \
        RecordTable.h                                      \
        RelationTag.h                                      \
        ReorderLiteralsTransformer.cpp                     \
        ResolveAliasesTransformer.cpp                      \
        SerialisationStream.h                              \
        SignalHandler.h                                    \
        SrcLocation.cpp                                    \
        SrcLocation.h                                      \
        Synthesiser.cpp                                    \
        Synthesiser.h                                      \
        SynthesiserRelation.cpp                            \
        SynthesiserRelation.h                              \
        TypeSystem.cpp                                     \
        TypeSystem.h                                       \
        Util.cpp                                           \
        WriteStream.h                                      \
        WriteStreamCSV.h                                   \
        parser.cc                                          \
        scanner.cc                                         \
        parser.hh                                          \
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
        IterUtils.h                                        \
        LambdaBTree.h                                      \
        Logger.h                                           \
        PiggyList.h                                        \
        ProfileDatabase.h                                  \
        ProfileEvent.h                                     \
        RamTypes.h                                         \
        ReadStream.h                                       \
        ReadStreamCSV.h                                    \
        RecordTable.h                                      \
        SerialisationStream.h                              \
        SignalHandler.h                                    \
        SouffleInterface.h                                 \
        SymbolTable.h                                      \
        Table.h                                            \
        UnionFind.h                                        \
        WriteStream.h                                      \
        WriteStreamCSV.h                                   \
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

