# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUFFIXES = .h

souffle_io_sources = \
        souffle/io/gzfstream.h                             \
        souffle/IOSystem.h                                 \
        souffle/io/ReadStream.h                            \
        souffle/io/ReadStreamCSV.h                         \
        souffle/io/ReadStreamJSON.h                        \
        souffle/io/ReadStreamSQLite.h                      \
        souffle/io/SerialisationStream.h                   \
        souffle/io/WriteStream.h                           \
        souffle/io/WriteStreamCSV.h                        \
        souffle/io/WriteStreamJSON.h                       \
		souffle/io/WriteStreamSQLite.h

souffle_profile_sources = \
        souffle/ProfileDatabase.h                          \
        souffle/json11.h                                   \
        souffle/profile/Cell.h                             \
        souffle/profile/CellInterface.h                    \
        souffle/profile/Cli.h                              \
        souffle/profile/DataComparator.h                   \
        souffle/profile/HtmlGenerator.h                    \
        souffle/profile/Iteration.h                        \
        souffle/profile/OutputProcessor.h                  \
        souffle/profile/ProgramRun.h                       \
        souffle/profile/Reader.h                           \
        souffle/profile/Relation.h                         \
        souffle/profile/Row.h                              \
        souffle/profile/Rule.h                             \
        souffle/profile/StringUtils.h                      \
        souffle/profile/Table.h                            \
        souffle/profile/Tui.h                              \
        souffle/profile/UserInputReader.h                  \
        souffle/profile/htmlCssChartist.h                  \
        souffle/profile/htmlCssStyle.h                     \
        souffle/profile/htmlJsChartistMin.h                \
        souffle/profile/htmlJsChartistPlugin.h             \
        souffle/profile/htmlJsMain.h                       \
        souffle/profile/htmlJsTableSort.h                  \
        souffle/profile/htmlJsUtil.h                       \
        souffle/profile/htmlMain.h

souffle_swig_sources = \
        souffle/swig/SwigInterface.h                       \
        souffle/swig/SwigInterface.i

souffle_utility_sources = \
        souffle/utility/CacheUtil.h                        \
        souffle/utility/ContainerUtil.h                    \
        souffle/utility/FileUtil.h                         \
        souffle/utility/EvaluatorUtil.h                    \
        souffle/utility/FunctionalUtil.h                   \
        souffle/utility/MiscUtil.h                         \
        souffle/utility/ParallelUtil.h                     \
        souffle/utility/StreamUtil.h                       \
        souffle/utility/StringUtil.h                       \
        souffle/utility/tinyformat.h

soufflepublic_HEADERS = \
        souffle/BTree.h                                    \
        souffle/BinaryConstraintOps.h                      \
        souffle/Brie.h                                     \
        souffle/CompiledOptions.h                          \
        souffle/CompiledSouffle.h                          \
        souffle/CompiledTuple.h                            \
        souffle/EquivalenceRelation.h                      \
        souffle/EventProcessor.h                           \
        souffle/Explain.h                                  \
        souffle/ExplainProvenance.h                        \
        souffle/ExplainProvenanceImpl.h                    \
        souffle/ExplainTree.h                              \
        souffle/LambdaBTree.h                              \
        souffle/Logger.h                                   \
        souffle/PiggyList.h                                \
        souffle/ProfileDatabase.h                          \
        souffle/ProfileEvent.h                             \
        souffle/RamTypes.h                                 \
        souffle/RecordTable.h                              \
        souffle/SignalHandler.h                            \
        souffle/SouffleInterface.h                         \
        souffle/SymbolTable.h                              \
        souffle/Table.h                                    \
        souffle/UnionFind.h                                \
        souffle/json11.h

souffleiodir = $(soufflepublicdir)/io
souffleio_HEADERS = $(souffle_io_sources)

souffleprofiledir = $(soufflepublicdir)/profile
souffleprofile_HEADERS = $(souffle_profile_sources)

souffleswigdir = $(soufflepublicdir)/swig
souffleswig_HEADERS = $(souffle_swig_sources)

souffleutilitydir = $(soufflepublicdir)/utility
souffleutility_HEADERS = $(souffle_utility_sources)

