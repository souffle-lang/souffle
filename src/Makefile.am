# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUBDIRS = . tests

SUFFIXES = .cpp .h .yy .ll .cc .hh .h

bin_PROGRAMS = souffle souffle-profile

DIR := ${CURDIR}

# ... which should no go to distribution
nodist_souffle_SOURCES = $(BUILT_SOURCES)

souffle_sources = \
        AggregateOp.h                                                          \
        AstToRamTranslator.cpp                                                 \
        AstToRamTranslator.h                                                   \
        Constraints.h                                                          \
        DebugReport.cpp                                                        \
        DebugReport.h                                                          \
        ErrorReport.h                                                          \
        FunctorOps.cpp                                                         \
        FunctorOps.h                                                           \
        Global.cpp                                                             \
        Global.h                                                               \
        GraphUtils.h                                                           \
        LogStatement.h                                                         \
        ParserDriver.cpp                                                       \
        ParserDriver.h                                                         \
        RelationTag.h                                                          \
        SrcLocation.cpp                                                        \
        SrcLocation.h                                                          \
        Util.cpp                                                               \
        ast/AstAbstract.h                                                      \
        ast/AstArgument.h                                                      \
        ast/AstAttribute.h                                                     \
        ast/AstClause.h                                                        \
        ast/AstComponent.h                                                     \
        ast/AstFunctorDeclaration.h                                            \
        ast/AstIO.h                                                            \
        ast/AstLiteral.h                                                       \
        ast/AstNode.h                                                          \
        ast/AstParserUtils.cpp                                                 \
        ast/AstParserUtils.h                                                   \
        ast/AstPragma.h                                                        \
        ast/AstProgram.h                                                       \
        ast/AstQualifiedName.h                                                 \
        ast/AstRelation.h                                                      \
        ast/AstTranslationUnit.h                                               \
        ast/AstType.h                                                          \
        ast/AstUtils.cpp                                                       \
        ast/AstUtils.h                                                         \
        ast/AstVisitor.h                                                       \
        ast/TypeSystem.cpp                                                     \
        ast/TypeSystem.h                                                       \
        ast/analysis/AstAnalysis.h                                             \
        ast/analysis/AstConstraintAnalysis.h                                   \
        ast/analysis/AstGroundAnalysis.cpp                                     \
        ast/analysis/AstGroundAnalysis.h                                       \
        ast/analysis/AstIOTypeAnalysis.cpp                                     \
        ast/analysis/AstIOTypeAnalysis.h                                       \
        ast/analysis/AstProfileUse.cpp                                         \
        ast/analysis/AstProfileUse.h                                           \
        ast/analysis/AstTypeAnalysis.cpp                                       \
        ast/analysis/AstTypeAnalysis.h                                         \
        ast/analysis/AstTypeEnvironmentAnalysis.cpp                            \
        ast/analysis/AstTypeEnvironmentAnalysis.h                              \
        ast/analysis/AuxArityAnalysis.cpp                                      \
        ast/analysis/AuxArityAnalysis.h                                        \
        ast/analysis/ComponentLookupAnalysis.cpp                               \
        ast/analysis/ComponentLookupAnalysis.h                                 \
        ast/analysis/PrecedenceGraph.cpp                                       \
        ast/analysis/PrecedenceGraph.h                                         \
        ast/transform/AstComponentChecker.cpp                                  \
        ast/transform/AstComponentChecker.h                                    \
        ast/transform/AstPragmaChecker.cpp                                     \
        ast/transform/AstPragmaChecker.h                                       \
        ast/transform/AstSemanticChecker.cpp                                   \
        ast/transform/AstSemanticChecker.h                                     \
        ast/transform/AstTransformer.cpp                                       \
        ast/transform/AstTransformer.h                                         \
        ast/transform/AstUserDefinedFunctors.cpp                               \
        ast/transform/AstUserDefinedFunctors.h                                 \
        ast/transform/ComponentInstantiation.cpp                               \
        ast/transform/ComponentInstantiation.h                                 \
        ast/transform/Conditional.h                                            \
        ast/transform/DebugReporter.cpp                                        \
        ast/transform/DebugReporter.h                                          \
        ast/transform/Fixpoint.h                                               \
        ast/transform/FoldAnonymousRecords.cpp                                 \
        ast/transform/FoldAnonymousRecords.h                                   \
        ast/transform/IOAttributes.h                                           \
        ast/transform/IODefaults.h                                             \
        ast/transform/InlineRelations.cpp                                      \
        ast/transform/InlineRelations.h                                        \
        ast/transform/MagicSet.cpp                                             \
        ast/transform/MagicSet.h                                               \
        ast/transform/MaterializeAggregationQueries.cpp                        \
        ast/transform/MaterializeAggregationQueries.h                          \
        ast/transform/MaterializeSingletonAggregation.cpp                      \
        ast/transform/MaterializeSingletonAggregation.h                        \
        ast/transform/MinimiseProgram.cpp                                      \
        ast/transform/MinimiseProgram.h                                        \
        ast/transform/NameUnnamedVariables.cpp                                 \
        ast/transform/NameUnnamedVariables.h                                   \
        ast/transform/NormaliseConstraints.cpp                                 \
        ast/transform/NormaliseConstraints.h                                   \
        ast/transform/PartitionBodyLiterals.cpp                                \
        ast/transform/PartitionBodyLiterals.h                                  \
        ast/transform/Pipeline.h                                               \
        ast/transform/PolymorphicObjects.cpp                                   \
        ast/transform/PolymorphicObjects.h                                     \
        ast/transform/Provenance.cpp                                           \
        ast/transform/Provenance.h                                             \
        ast/transform/ReduceExistentials.cpp                                   \
        ast/transform/ReduceExistentials.h                                     \
        ast/transform/RemoveBooleanConstraints.cpp                             \
        ast/transform/RemoveBooleanConstraints.h                               \
        ast/transform/RemoveEmptyRelations.cpp                                 \
        ast/transform/RemoveEmptyRelations.h                                   \
        ast/transform/RemoveRedundantRelations.cpp                             \
        ast/transform/RemoveRedundantRelations.h                               \
        ast/transform/RemoveRedundantSums.cpp                                  \
        ast/transform/RemoveRedundantSums.h                                    \
        ast/transform/RemoveRelationCopies.cpp                                 \
        ast/transform/RemoveRelationCopies.h                                   \
        ast/transform/RemoveTypecasts.cpp                                      \
        ast/transform/RemoveTypecasts.h                                        \
        ast/transform/ReorderLiterals.cpp                                      \
        ast/transform/ReorderLiterals.h                                        \
        ast/transform/ReplaceSingletonVariables.cpp                            \
        ast/transform/ReplaceSingletonVariables.h                              \
        ast/transform/ResolveAliases.cpp                                       \
        ast/transform/ResolveAliases.h                                         \
        ast/transform/ResolveAnonymousRecordsAliases.cpp                       \
        ast/transform/ResolveAnonymousRecordsAliases.h                         \
        ast/transform/UniqueAggregationVariables.cpp                           \
        ast/transform/UniqueAggregationVariables.h                             \
        ast/transform/While.h                                                  \
        interpreter/InterpreterContext.h                                       \
        interpreter/InterpreterEngine.cpp                                      \
        interpreter/InterpreterEngine.h                                        \
        interpreter/InterpreterGenerator.h                                     \
        interpreter/InterpreterIndex.cpp                                       \
        interpreter/InterpreterIndex.h                                         \
        interpreter/InterpreterNode.h                                          \
        interpreter/InterpreterPreamble.h                                      \
        interpreter/InterpreterProgInterface.h                                 \
        interpreter/InterpreterRelation.cpp                                    \
        interpreter/InterpreterRelation.h                                      \
        parser.cc                                                              \
        parser.hh                                                              \
        ram/RamCondition.h                                                     \
        ram/RamExpression.h                                                    \
        ram/RamNode.h                                                          \
        ram/RamOperation.h                                                     \
        ram/RamProgram.h                                                       \
        ram/RamRelation.h                                                      \
        ram/RamStatement.h                                                     \
        ram/RamTranslationUnit.h                                               \
        ram/RamUtils.h                                                         \
        ram/RamVisitor.h                                                       \
        ram/analysis/RamAnalysis.h                                             \
        ram/analysis/RamComplexityAnalysis.cpp                                 \
        ram/analysis/RamComplexityAnalysis.h                                   \
        ram/analysis/RamIndexAnalysis.cpp                                      \
        ram/analysis/RamIndexAnalysis.h                                        \
        ram/analysis/RamLevelAnalysis.cpp                                      \
        ram/analysis/RamLevelAnalysis.h                                        \
        ram/transform/ChoiceConversion.cpp                                     \
        ram/transform/ChoiceConversion.h                                       \
        ram/transform/CollapseFilters.cpp                                      \
        ram/transform/CollapseFilters.h                                        \
        ram/transform/EliminateDuplicates.cpp                                  \
        ram/transform/EliminateDuplicates.h                                    \
        ram/transform/ExpandFilter.cpp                                         \
        ram/transform/ExpandFilter.h                                           \
        ram/transform/HoistAggregate.cpp                                       \
        ram/transform/HoistAggregate.h                                         \
        ram/transform/HoistConditions.cpp                                      \
        ram/transform/HoistConditions.h                                        \
        ram/transform/IfConversion.cpp                                         \
        ram/transform/IfConversion.h                                           \
        ram/transform/IndexedInequality.cpp                                    \
        ram/transform/IndexedInequality.h                                      \
        ram/transform/MakeIndex.cpp                                            \
        ram/transform/MakeIndex.h                                              \
        ram/transform/Parallel.cpp                                             \
        ram/transform/Parallel.h                                               \
        ram/transform/RamTransformer.cpp                                       \
        ram/transform/RamTransformer.h                                         \
        ram/transform/ReorderConditions.cpp                                    \
        ram/transform/ReorderConditions.h                                      \
        ram/transform/ReorderFilterBreak.cpp                                   \
        ram/transform/ReorderFilterBreak.h                                     \
        ram/transform/ReportIndex.h                                            \
        ram/transform/TupleId.cpp                                              \
        ram/transform/TupleId.h                                                \
        scanner.cc                                                             \
        stack.hh                                                               \
        synthesiser/Synthesiser.cpp                                            \
        synthesiser/Synthesiser.h                                              \
        synthesiser/SynthesiserRelation.cpp                                    \
        synthesiser/SynthesiserRelation.h

# -- build souffle as a library so it can be reused in testing
noinst_LTLIBRARIES = libsouffle.la
libsouffle_la_SOURCES  = $(souffle_sources)
libsouffle_la_CXXFLAGS = $(souffle_CPPFLAGS) $(FFI_CFLAGS)
libsouffle_la_LDFLAGS = --static --dlopen --pic -ldl -lffi

souffle_SOURCES = main.cpp
souffle_LDADD = libsouffle.la
souffle_CPPFLAGS = -I$(top_srcdir)/include

souffle_profile_SOURCES = souffle_prof.cpp
souffle_profile_CPPFLAGS = -I$(top_srcdir)/include

dist_bin_SCRIPTS = souffle-compile souffle-config

EXTRA_DIST = parser.yy scanner.ll

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

