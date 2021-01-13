# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUBDIRS = . tests ast/tests ram/tests interpreter/tests

SUFFIXES = .cpp .h .yy .ll .cc .hh .h

bin_PROGRAMS = souffle souffle-profile

souffle_sources = \
        AggregateOp.h                                      \
        FunctorOps.cpp                                     \
        FunctorOps.h                                       \
        Global.cpp                                         \
        Global.h                                           \
        GraphUtils.h                                       \
        LogStatement.h                                     \
        RelationTag.h                                      \
        ast/Aggregator.h                                   \
        ast/AlgebraicDataType.h                            \
        ast/Argument.h                                     \
        ast/Atom.h                                         \
        ast/Attribute.h                                    \
        ast/BinaryConstraint.h                             \
        ast/BooleanConstraint.h                            \
        ast/BranchDeclaration.h                            \
        ast/BranchInit.h                                   \
        ast/Clause.h                                       \
        ast/Component.h                                    \
        ast/ComponentInit.h                                \
        ast/ComponentType.h                                \
        ast/Constant.h                                     \
        ast/Constraint.h                                   \
        ast/Counter.h                                      \
        ast/ExecutionOrder.h                               \
        ast/ExecutionPlan.h                                \
        ast/Functor.h                                      \
        ast/FunctorDeclaration.h                           \
        ast/FunctionalConstraint.h                         \
        ast/Directive.h                                    \
        ast/IntrinsicFunctor.h                             \
        ast/Literal.h                                      \
        ast/Negation.h                                     \
        ast/NilConstant.h                                  \
        ast/Node.h                                         \
        ast/NumericConstant.h                              \
        ast/Pragma.h                                       \
        ast/Program.cpp                                    \
        ast/Program.h                                      \
        ast/QualifiedName.h                                \
        ast/RecordInit.h                                   \
        ast/RecordType.h                                   \
        ast/Relation.h                                     \
        ast/StringConstant.h                               \
        ast/SubsetType.h                                   \
        ast/Term.h                                         \
        ast/TranslationUnit.h                              \
        ast/Type.h                                         \
        ast/TypeCast.h                                     \
        ast/UnionType.h                                    \
        ast/UnnamedVariable.h                              \
        ast/UserDefinedFunctor.h                           \
        ast/Variable.h                                     \
        ast/analysis/Aggregate.cpp                         \
        ast/analysis/Aggregate.h                           \
        ast/analysis/Analysis.h                            \
        ast/analysis/AuxArity.cpp                          \
        ast/analysis/AuxArity.h                            \
        ast/analysis/ClauseNormalisation.h                 \
        ast/analysis/ClauseNormalisation.cpp               \
        ast/analysis/ComponentLookup.cpp                   \
        ast/analysis/ComponentLookup.h                     \
        ast/analysis/Constraint.h                          \
        ast/analysis/ConstraintSystem.h                    \
        ast/analysis/Functor.h                             \
        ast/analysis/Functor.cpp                           \
        ast/analysis/Ground.cpp                            \
        ast/analysis/Ground.h                              \
        ast/analysis/IOType.cpp                            \
        ast/analysis/IOType.h                              \
        ast/analysis/PolymorphicObjects.cpp                \
        ast/analysis/PolymorphicObjects.h                  \
        ast/analysis/PrecedenceGraph.cpp                   \
        ast/analysis/PrecedenceGraph.h                     \
        ast/analysis/ProfileUse.cpp                        \
        ast/analysis/ProfileUse.h                          \
        ast/analysis/RecursiveClauses.cpp                  \
        ast/analysis/RecursiveClauses.h                    \
        ast/analysis/RedundantRelations.cpp                \
        ast/analysis/RedundantRelations.h                  \
        ast/analysis/RelationDetailCache.cpp               \
        ast/analysis/RelationDetailCache.h                 \
        ast/analysis/RelationSchedule.cpp                  \
        ast/analysis/RelationSchedule.h                    \
        ast/analysis/SCCGraph.cpp                          \
        ast/analysis/SCCGraph.h                            \
        ast/analysis/SumTypeBranches.cpp                   \
        ast/analysis/SumTypeBranches.h                     \
        ast/analysis/TopologicallySortedSCCGraph.cpp       \
        ast/analysis/TopologicallySortedSCCGraph.h         \
        ast/analysis/Type.cpp                              \
        ast/analysis/Type.h                                \
        ast/analysis/TypeConstraints.h                     \
        ast/analysis/TypeConstraints.cpp                   \
        ast/analysis/TypeSystem.cpp                        \
        ast/analysis/TypeSystem.h                          \
        ast/analysis/TypeEnvironment.cpp                   \
        ast/analysis/TypeEnvironment.h                     \
        ast/transform/AddNullariesToAtomlessAggregates.cpp \
        ast/transform/AddNullariesToAtomlessAggregates.h   \
        ast/transform/ComponentChecker.cpp                 \
        ast/transform/ComponentChecker.h                   \
        ast/transform/ComponentInstantiation.cpp           \
        ast/transform/ComponentInstantiation.h             \
        ast/transform/Conditional.h                        \
        ast/transform/DebugReporter.cpp                    \
        ast/transform/DebugReporter.h                      \
        ast/transform/ExecutionPlanChecker.cpp             \
        ast/transform/ExecutionPlanChecker.h               \
        ast/transform/Fixpoint.h                           \
        ast/transform/FoldAnonymousRecords.cpp             \
        ast/transform/FoldAnonymousRecords.h               \
        ast/transform/GroundedTermsChecker.cpp             \
        ast/transform/GroundedTermsChecker.h               \
        ast/transform/GroundWitnesses.cpp                  \
        ast/transform/GroundWitnesses.h                    \
        ast/transform/IOAttributes.h                       \
        ast/transform/IODefaults.h                         \
        ast/transform/InlineRelations.cpp                  \
        ast/transform/InlineRelations.h                    \
        ast/transform/MagicSet.cpp                         \
        ast/transform/MagicSet.h                           \
        ast/transform/MaterializeAggregationQueries.cpp    \
        ast/transform/MaterializeAggregationQueries.h      \
        ast/transform/MaterializeSingletonAggregation.cpp  \
        ast/transform/MaterializeSingletonAggregation.h    \
        ast/transform/Meta.cpp                             \
        ast/transform/Meta.h                               \
        ast/transform/MinimiseProgram.cpp                  \
        ast/transform/MinimiseProgram.h                    \
        ast/transform/NameUnnamedVariables.cpp             \
        ast/transform/NameUnnamedVariables.h               \
        ast/transform/NormaliseGenerators.cpp              \
        ast/transform/NormaliseGenerators.h                \
        ast/transform/Null.h                               \
        ast/transform/PartitionBodyLiterals.cpp            \
        ast/transform/PartitionBodyLiterals.h              \
        ast/transform/Pipeline.h                           \
        ast/transform/PragmaChecker.cpp                    \
        ast/transform/PragmaChecker.h                      \
        ast/transform/Provenance.cpp                       \
        ast/transform/Provenance.h                         \
        ast/transform/ReduceExistentials.cpp               \
        ast/transform/ReduceExistentials.h                 \
        ast/transform/RemoveBooleanConstraints.cpp         \
        ast/transform/RemoveBooleanConstraints.h           \
        ast/transform/RemoveEmptyRelations.cpp             \
        ast/transform/RemoveEmptyRelations.h               \
        ast/transform/RemoveRedundantRelations.cpp         \
        ast/transform/RemoveRedundantRelations.h           \
        ast/transform/RemoveRedundantSums.cpp              \
        ast/transform/RemoveRedundantSums.h                \
        ast/transform/RemoveRelationCopies.cpp             \
        ast/transform/RemoveRelationCopies.h               \
        ast/transform/ReorderLiterals.cpp                  \
        ast/transform/ReorderLiterals.h                    \
        ast/transform/ReplaceSingletonVariables.cpp        \
        ast/transform/ReplaceSingletonVariables.h          \
        ast/transform/ResolveAliases.cpp                   \
        ast/transform/ResolveAliases.h                     \
        ast/transform/ResolveAnonymousRecordAliases.cpp    \
        ast/transform/ResolveAnonymousRecordAliases.h      \
        ast/transform/SemanticChecker.cpp                  \
        ast/transform/SemanticChecker.h                    \
        ast/transform/SimplifyAggregateTargetExpression.cpp\
        ast/transform/SimplifyAggregateTargetExpression.h  \
        ast/transform/Transformer.cpp                      \
        ast/transform/Transformer.h                        \
        ast/transform/TypeChecker.cpp                      \
        ast/transform/TypeChecker.h                        \
        ast/transform/UniqueAggregationVariables.cpp       \
        ast/transform/UniqueAggregationVariables.h         \
        ast/transform/While.h                              \
        ast/utility/BindingStore.h                         \
        ast/utility/BindingStore.cpp                       \
        ast/utility/LambdaNodeMapper.h                     \
        ast/utility/NodeMapper.h                           \
        ast/utility/SipsMetric.h                           \
        ast/utility/SipsMetric.cpp                         \
        ast/utility/Utils.cpp                              \
        ast/utility/Utils.h                                \
        ast/utility/Visitor.h                              \
        ast2ram/AstToRamTranslator.cpp                     \
        ast2ram/AstToRamTranslator.h                       \
        ast2ram/ClauseTranslator.cpp                       \
        ast2ram/ClauseTranslator.h                         \
        ast2ram/ConstraintTranslator.cpp                   \
        ast2ram/ConstraintTranslator.h                     \
        ast2ram/ProvenanceClauseTranslator.cpp             \
        ast2ram/ProvenanceClauseTranslator.h               \
        ast2ram/ProvenanceSubproofGenerator.cpp            \
        ast2ram/ProvenanceSubproofGenerator.h              \
        ast2ram/ProvenanceTranslator.cpp                   \
        ast2ram/ProvenanceTranslator.h                     \
        ast2ram/utility/Location.h                         \
        ast2ram/utility/Utils.cpp                          \
        ast2ram/utility/Utils.h                            \
        ast2ram/utility/TranslatorContext.cpp              \
        ast2ram/utility/TranslatorContext.h                \
        ast2ram/utility/ValueIndex.cpp                     \
        ast2ram/utility/ValueIndex.h                       \
        ast2ram/ValueTranslator.cpp                        \
        ast2ram/ValueTranslator.h                          \
        interpreter/Context.h                              \
        interpreter/Engine.cpp                             \
        interpreter/Engine.h                               \
        interpreter/Generator.h                            \
        interpreter/Generator.cpp                          \
        interpreter/BrieIndex.cpp                          \
        interpreter/BTreeIndex.cpp                         \
        interpreter/EqrelIndex.cpp                         \
        interpreter/ProvenanceIndex.cpp                    \
        interpreter/Index.h                                \
        interpreter/Node.h                                 \
        interpreter/ViewContext.h                          \
        interpreter/ProgInterface.h                        \
        interpreter/Relation.h                             \
        interpreter/Util.h                                 \
        parser/ParserDriver.cpp                            \
        parser/ParserDriver.h                              \
        parser/ParserUtils.cpp                             \
        parser/ParserUtils.h                               \
        parser/SrcLocation.cpp                             \
        parser/SrcLocation.h                               \
        parser/parser.cc                                   \
        parser/parser.hh                                   \
        parser/scanner.cc                                  \
        parser/stack.hh                                    \
        ram/AbstractAggregate.h                            \
        ram/AbstractChoice.h                               \
        ram/AbstractConditional.h                          \
        ram/AbstractExistenceCheck.h                       \
        ram/AbstractLog.h                                  \
        ram/AbstractOperator.h                             \
        ram/AbstractParallel.h                             \
        ram/Aggregate.h                                    \
        ram/AutoIncrement.h                                \
        ram/BinRelationStatement.h                         \
        ram/Break.h                                        \
        ram/Call.h                                         \
        ram/Choice.h                                       \
        ram/Clear.h                                        \
        ram/Condition.h                                    \
        ram/Conjunction.h                                  \
        ram/Constant.h                                     \
        ram/Constraint.h                                   \
        ram/DebugInfo.h                                    \
        ram/EmptinessCheck.h                               \
        ram/ExistenceCheck.h                               \
        ram/Exit.h                                         \
        ram/Expression.h                                   \
        ram/Extend.h                                       \
        ram/False.h                                        \
        ram/Filter.h                                       \
        ram/FloatConstant.h                                \
        ram/GuardedProject.h                               \
        ram/IO.h                                           \
        ram/IndexAggregate.h                               \
        ram/IndexChoice.h                                  \
        ram/IndexOperation.h                               \
        ram/IndexScan.h                                    \
        ram/IntrinsicOperator.h                            \
        ram/ListStatement.h                                \
        ram/LogRelationTimer.h                             \
        ram/LogSize.h                                      \
        ram/LogTimer.h                                     \
        ram/Loop.h                                         \
        ram/Negation.h                                     \
        ram/NestedIntrinsicOperator.h                      \
        ram/NestedOperation.h                              \
        ram/Node.h                                         \
        ram/Operation.h                                    \
        ram/PackRecord.h                                   \
        ram/Parallel.h                                     \
        ram/ParallelAggregate.h                            \
        ram/ParallelChoice.h                               \
        ram/ParallelIndexAggregate.h                       \
        ram/ParallelIndexChoice.h                          \
        ram/ParallelIndexScan.h                            \
        ram/Program.h                                      \
        ram/Project.h                                      \
        ram/ProvenanceExistenceCheck.h                     \
        ram/Query.h                                        \
        ram/Relation.h                                     \
        ram/RelationOperation.h                            \
        ram/RelationStatement.h                            \
        ram/RelationSize.h                                 \
        ram/ParallelScan.h                                 \
        ram/Scan.h                                         \
        ram/Sequence.h                                     \
        ram/SignedConstant.h                               \
        ram/Statement.h                                    \
        ram/Statement.h                                    \
        ram/SubroutineArgument.h                           \
        ram/SubroutineReturn.h                             \
        ram/Swap.h                                         \
        ram/TranslationUnit.h                              \
        ram/True.h                                         \
        ram/TupleElement.h                                 \
        ram/TupleOperation.h                               \
        ram/UndefValue.h                                   \
        ram/UnpackRecord.h                                 \
        ram/UnsignedConstant.h                             \
        ram/UserDefinedOperator.h                          \
        ram/analysis/Analysis.h                            \
        ram/analysis/Complexity.cpp                        \
        ram/analysis/Complexity.h                          \
        ram/analysis/Index.cpp                             \
        ram/analysis/Index.h                               \
        ram/analysis/Level.cpp                             \
        ram/analysis/Level.h                               \
        ram/analysis/Relation.cpp                          \
        ram/analysis/Relation.h                            \
        ram/transform/ChoiceConversion.cpp                 \
        ram/transform/ChoiceConversion.h                   \
        ram/transform/CollapseFilters.cpp                  \
        ram/transform/CollapseFilters.h                    \
        ram/transform/Conditional.h                        \
        ram/transform/EliminateDuplicates.cpp              \
        ram/transform/EliminateDuplicates.h                \
        ram/transform/ExpandFilter.cpp                     \
        ram/transform/ExpandFilter.h                       \
        ram/transform/HoistAggregate.cpp                   \
        ram/transform/HoistAggregate.h                     \
        ram/transform/HoistConditions.cpp                  \
        ram/transform/HoistConditions.h                    \
        ram/transform/IfConversion.cpp                     \
        ram/transform/IfConversion.h                       \
        ram/transform/Loop.h                               \
        ram/transform/MakeIndex.cpp                        \
        ram/transform/MakeIndex.h                          \
        ram/transform/Meta.h                               \
        ram/transform/Parallel.cpp                         \
        ram/transform/Parallel.h                           \
        ram/transform/ReorderConditions.cpp                \
        ram/transform/ReorderConditions.h                  \
        ram/transform/ReorderFilterBreak.cpp               \
        ram/transform/ReorderFilterBreak.h                 \
        ram/transform/ReportIndex.h                        \
        ram/transform/Sequence.h                           \
        ram/transform/Transformer.cpp                      \
        ram/transform/Transformer.h                        \
        ram/transform/TupleId.cpp                          \
        ram/transform/TupleId.h                            \
        ram/utility/LambdaNodeMapper.h                     \
        ram/utility/NodeMapper.h                           \
        ram/utility/Utils.h                                \
        ram/utility/Visitor.h                              \
        reports/DebugReport.cpp                            \
        reports/DebugReport.h                              \
        reports/ErrorReport.h                              \
        synthesiser/Synthesiser.cpp                        \
        synthesiser/Synthesiser.h                          \
        synthesiser/Relation.cpp                           \
        synthesiser/Relation.h                             \
        $(souffle_utility_sources)                         \
        $(souffle_profile_sources)

# -- build souffle as a library so it can be reused in testing
noinst_LTLIBRARIES = libsouffle.la libsouffle_py.la
libsouffle_la_SOURCES  = $(souffle_sources)
libsouffle_la_CXXFLAGS = $(souffle_CPPFLAGS) $(FFI_CFLAGS)
libsouffle_la_LDFLAGS = --static --dlopen --pic -ldl -lffi

souffle_SOURCES = main.cpp
souffle_LDADD = libsouffle.la
souffle_CPPFLAGS = -I$(srcdir)/include

souffle_profile_SOURCES = souffle_prof.cpp
souffle_profile_CPPFLAGS = $(souffle_CPPFLAGS)

PYTHON_EXT = $(shell python3-config --extension-suffix)
libsouffle_py_la_SOURCES =                                  \
        pybind/cpp/souffle_py.cpp                           \
        $(shell find $(top_srcdir)/src/pybind/python -name "*.py") \
        $(top_srcdir)/setup.py

libsouffle_py_la_CXXFLAGS = $(souffle_CPPFLAGS) $(FFI_CFLAGS)
libsouffle_py_la_CPPFLAGS =                                 \
        -I$(srcdir)/include                                 \
        -I$(top_srcdir)/extern/pybind11/include             \
        $(shell python3-config --includes)
# This is stupid.  Automake/libtool won't build a shared
# noist_ library unless -rpath is present...
libsouffle_py_la_LDFLAGS = -lffi -shared -avoid-version     \
			   -shrext $(PYTHON_EXT) -rpath /

noinst_HEADERS = $(shell find $(top_srcdir)/extern/pybind11/include)

dist_bin_SCRIPTS = souffle-compile souffle-config
noinst_DATA = python_paths.py

EXTRA_DIST = parser/parser.yy parser/scanner.ll

soufflepublicdir = $(includedir)/souffle

soufflepublic_HEADERS = \
        include/souffle/BinaryConstraintOps.h              \
        include/souffle/CompiledOptions.h                  \
        include/souffle/CompiledSouffle.h                  \
        include/souffle/RamTypes.h                         \
        include/souffle/RecordTable.h                      \
        include/souffle/SignalHandler.h                    \
        include/souffle/SouffleInterface.h                 \
        include/souffle/SymbolTable.h                      \
        include/souffle/TypeAttribute.h

souffledatastructuredir = $(soufflepublicdir)/datastructure

souffledatastructure_HEADERS = \
        include/souffle/datastructure/BTree.h              \
        include/souffle/datastructure/Brie.h               \
        include/souffle/datastructure/EquivalenceRelation.h\
        include/souffle/datastructure/LambdaBTree.h        \
        include/souffle/datastructure/PiggyList.h          \
        include/souffle/datastructure/Table.h              \
        include/souffle/datastructure/UnionFind.h

souffleiodir = $(soufflepublicdir)/io

souffleio_HEADERS = \
        include/souffle/io/IOSystem.h                      \
        include/souffle/io/gzfstream.h                     \
        include/souffle/io/ReadStream.h                    \
        include/souffle/io/ReadStreamCSV.h                 \
        include/souffle/io/ReadStreamJSON.h                \
        include/souffle/io/ReadStreamSQLite.h              \
        include/souffle/io/SerialisationStream.h           \
        include/souffle/io/WriteStreamSQLite.h             \
        include/souffle/io/WriteStream.h                   \
        include/souffle/io/WriteStreamCSV.h                \
        include/souffle/io/WriteStreamJSON.h

souffleprofiledir = $(soufflepublicdir)/profile

souffleprofile_HEADERS = \
        include/souffle/profile/Cell.h                     \
        include/souffle/profile/CellInterface.h            \
        include/souffle/profile/Cli.h                      \
        include/souffle/profile/DataComparator.h           \
        include/souffle/profile/EventProcessor.h           \
        include/souffle/profile/HtmlGenerator.h            \
        include/souffle/profile/Iteration.h                \
        include/souffle/profile/Logger.h                   \
        include/souffle/profile/OutputProcessor.h          \
        include/souffle/profile/ProfileDatabase.h          \
        include/souffle/profile/ProfileEvent.h             \
        include/souffle/profile/ProgramRun.h               \
        include/souffle/profile/Reader.h                   \
        include/souffle/profile/Relation.h                 \
        include/souffle/profile/Row.h                      \
        include/souffle/profile/Rule.h                     \
        include/souffle/profile/StringUtils.h              \
        include/souffle/profile/Table.h                    \
        include/souffle/profile/Tui.h                      \
        include/souffle/profile/UserInputReader.h          \
        include/souffle/profile/htmlCssChartist.h          \
        include/souffle/profile/htmlCssStyle.h             \
        include/souffle/profile/htmlJsChartistMin.h        \
        include/souffle/profile/htmlJsChartistPlugin.h     \
        include/souffle/profile/htmlJsMain.h               \
        include/souffle/profile/htmlJsTableSort.h          \
        include/souffle/profile/htmlJsUtil.h               \
        include/souffle/profile/htmlMain.h

souffleprovenancedir = $(soufflepublicdir)/provenance

souffleprovenance_HEADERS = \
        include/souffle/provenance/Explain.h               \
        include/souffle/provenance/ExplainProvenance.h     \
        include/souffle/provenance/ExplainProvenanceImpl.h \
        include/souffle/provenance/ExplainTree.h

souffleswigdir = $(soufflepublicdir)/swig

souffleswig_HEADERS = \
        include/souffle/swig/SwigInterface.h               \
        include/souffle/swig/SwigInterface.i

souffleutilitydir = $(soufflepublicdir)/utility

souffleutility_HEADERS = \
        include/souffle/utility/CacheUtil.h                \
        include/souffle/utility/ContainerUtil.h            \
        include/souffle/utility/FileUtil.h                 \
        include/souffle/utility/EvaluatorUtil.h            \
        include/souffle/utility/FunctionalUtil.h           \
        include/souffle/utility/MiscUtil.h                 \
        include/souffle/utility/ParallelUtil.h             \
        include/souffle/utility/StreamUtil.h               \
        include/souffle/utility/StringUtil.h               \
        include/souffle/utility/json11.h                   \
        include/souffle/utility/span.h                     \
        include/souffle/utility/tinyformat.h

# files to clean
CLEANFILES = $(BUILT_SOURCES)                              \
                parser/parser.cc                           \
                parser/scanner.cc                          \
                parser/parser.hh                           \
                parser/stack.hh

# run Bison
$(builddir)/parser/parser.hh: $(srcdir)/parser/parser.yy
	$(BISON) -Wall -Werror -Wno-error=deprecated -Wno-error=other -v -d -o parser/parser.cc $(srcdir)/parser/parser.yy

# and FLEX
$(builddir)/parser/scanner.cc: $(srcdir)/parser/scanner.ll
	$(FLEX) -o parser/scanner.cc $(srcdir)/parser/scanner.ll

# driver depends on the generated header
$(builddir)/parser/parser.cc $(builddir)/parser/stack.hh $(builddir)/parser/scanner.cc \
    $(builddir)/main.cpp $(builddir)/parser/ParserDriver.cpp: $(builddir)/parser/parser.hh

# There may be a cleaner way to do this but setup.py needs the library
# in the source tree
buildreal = $(shell realpath $(builddir))
all-local: $(LTLIBRARIES)
	chmod u+w $(top_srcdir)/src/pybind/python/souffle
	$(INSTALL) $(builddir)/.libs/libsouffle_py$(PYTHON_EXT) $(top_srcdir)/src/pybind/python/souffle

clean-local:
	rm -f $(top_srcdir)/src/pybind/python/souffle/libsouffle_py$(PYTHON_EXT)
	rm -f $(top_srcdir)/src/python_paths.py
