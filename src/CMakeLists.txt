# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

set(SOUFFLE_SOURCES
    FunctorOps.cpp
    Global.cpp
    ast/Aggregator.cpp
    ast/AlgebraicDataType.cpp
    ast/AliasType.cpp
    ast/Atom.cpp
    ast/Attribute.cpp
    ast/BinaryConstraint.cpp
    ast/BooleanConstraint.cpp
    ast/BranchType.cpp
    ast/BranchInit.cpp
    ast/Clause.cpp
    ast/SubsumptiveClause.cpp
    ast/Component.cpp
    ast/ComponentInit.cpp
    ast/ComponentType.cpp
    ast/Constant.cpp
    ast/Counter.cpp
    ast/Directive.cpp
    ast/ExecutionOrder.cpp
    ast/ExecutionPlan.cpp
    ast/FunctorDeclaration.cpp
    ast/FunctionalConstraint.cpp
    ast/IntrinsicFunctor.cpp
    ast/Negation.cpp
    ast/NilConstant.cpp
    ast/Node.cpp
    ast/NumericConstant.cpp
    ast/Pragma.cpp
    ast/Program.cpp
    ast/QualifiedName.cpp
    ast/RecordInit.cpp
    ast/RecordType.cpp
    ast/Relation.cpp
    ast/StringConstant.cpp
    ast/SubsetType.cpp
    ast/Term.cpp
    ast/TranslationUnit.cpp
    ast/Type.cpp
    ast/TypeCast.cpp
    ast/UnionType.cpp
    ast/UnnamedVariable.cpp
    ast/UserDefinedFunctor.cpp
    ast/Variable.cpp
    ast/analysis/Aggregate.cpp
    ast/analysis/ClauseNormalisation.cpp
    ast/analysis/ComponentLookup.cpp
    ast/analysis/Functor.cpp
    ast/analysis/Ground.cpp
    ast/analysis/IOType.cpp
    ast/analysis/PrecedenceGraph.cpp
    ast/analysis/ProfileUse.cpp
    ast/analysis/RecursiveClauses.cpp
    ast/analysis/RedundantRelations.cpp
    ast/analysis/RelationSchedule.cpp
    ast/analysis/SCCGraph.cpp
    ast/analysis/TopologicallySortedSCCGraph.cpp
    ast/analysis/typesystem/PolymorphicObjects.cpp
    ast/analysis/typesystem/SumTypeBranches.cpp
    ast/analysis/typesystem/Type.cpp
    ast/analysis/typesystem/TypeConstraints.cpp
    ast/analysis/typesystem/TypeConstrainsAnalysis.cpp
    ast/analysis/typesystem/TypeConstrainsAnalysis.h
    ast/analysis/typesystem/TypeSystem.cpp
    ast/analysis/typesystem/TypeEnvironment.cpp
    ast/transform/AddNullariesToAtomlessAggregates.cpp
    ast/transform/ComponentChecker.cpp
    ast/transform/ComponentInstantiation.cpp
    ast/transform/DebugReporter.cpp
    ast/transform/ExecutionPlanChecker.cpp
    ast/transform/ExpandEqrels.cpp
    ast/transform/FoldAnonymousRecords.cpp
    ast/transform/GroundedTermsChecker.cpp
    ast/transform/GroundWitnesses.cpp
    ast/transform/InlineRelations.cpp
    ast/transform/MagicSet.cpp
    ast/transform/MaterializeAggregationQueries.cpp
    ast/transform/MaterializeSingletonAggregation.cpp
    ast/transform/Meta.cpp
    ast/transform/MinimiseProgram.cpp
    ast/transform/NameUnnamedVariables.cpp
    ast/transform/NormaliseGenerators.cpp
    ast/transform/PartitionBodyLiterals.cpp
    ast/transform/PragmaChecker.cpp
    ast/transform/ReduceExistentials.cpp
    ast/transform/RemoveBooleanConstraints.cpp
    ast/transform/RemoveEmptyRelations.cpp
    ast/transform/RemoveRedundantRelations.cpp
    ast/transform/RemoveRedundantSums.cpp
    ast/transform/RemoveRelationCopies.cpp
    ast/transform/ReorderLiterals.cpp
    ast/transform/ReplaceSingletonVariables.cpp
    ast/transform/ResolveAliases.cpp
    ast/transform/ResolveAnonymousRecordAliases.cpp
    ast/transform/SemanticChecker.cpp
    ast/transform/SubsumptionQualifier.cpp
    ast/transform/SimplifyAggregateTargetExpression.cpp
    ast/transform/Transformer.cpp
    ast/transform/TypeChecker.cpp
    ast/transform/UniqueAggregationVariables.cpp
    ast/utility/BindingStore.cpp
    ast/utility/SipsMetric.cpp
    ast/utility/Utils.cpp
    ast2ram/incremental/TranslationStrategy.cpp
    ast2ram/incremental/UnitTranslator.cpp
    ast2ram/provenance/ClauseTranslator.cpp
    ast2ram/provenance/ConstraintTranslator.cpp
    ast2ram/provenance/SubproofGenerator.cpp
    ast2ram/provenance/TranslationStrategy.cpp
    ast2ram/provenance/UnitTranslator.cpp
    ast2ram/seminaive/ClauseTranslator.cpp
    ast2ram/seminaive/ConstraintTranslator.cpp
    ast2ram/seminaive/TranslationStrategy.cpp
    ast2ram/seminaive/UnitTranslator.cpp
    ast2ram/seminaive/ValueTranslator.cpp
    ast2ram/utility/Utils.cpp
    ast2ram/utility/TranslatorContext.cpp
    ast2ram/utility/ValueIndex.cpp
    interpreter/Engine.cpp
    interpreter/Generator.cpp
    interpreter/BrieIndex.cpp
    interpreter/BTreeIndex.cpp
    interpreter/BTreeDeleteIndex.cpp
    interpreter/EqrelIndex.cpp
    interpreter/ProvenanceIndex.cpp
    parser/ParserDriver.cpp
    parser/ParserUtils.cpp
    parser/SrcLocation.cpp
    ram/Node.cpp
    ram/TranslationUnit.cpp
    ram/analysis/Complexity.cpp
    ram/analysis/Index.cpp
    ram/analysis/Level.cpp
    ram/analysis/Relation.cpp
    ram/transform/IfExistsConversion.cpp
    ram/transform/CollapseFilters.cpp
    ram/transform/EliminateDuplicates.cpp
    ram/transform/ExpandFilter.cpp
    ram/transform/HoistAggregate.cpp
    ram/transform/HoistConditions.cpp
    ram/transform/IfConversion.cpp
    ram/transform/MakeIndex.cpp
    ram/transform/Parallel.cpp
    ram/transform/ReorderConditions.cpp
    ram/transform/ReorderFilterBreak.cpp
    ram/transform/Transformer.cpp
    ram/transform/TupleId.cpp
    ram/utility/NodeMapper.cpp
    reports/DebugReport.cpp
    synthesiser/Synthesiser.cpp
    synthesiser/Relation.cpp
)

# --------------------------------------------------
# Flex/Bison
# --------------------------------------------------
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/parser")
flex_target(scanner parser/scanner.ll ${CMAKE_CURRENT_BINARY_DIR}/parser/scanner.cc)
bison_target(parser parser/parser.yy ${CMAKE_CURRENT_BINARY_DIR}/parser/parser.cc
             COMPILE_FLAGS "-Wall -Werror -Wno-error=deprecated -Wno-error=other -v -d")
add_flex_bison_dependency(scanner parser)

# OSX compiler doesn't recognise `(void)var;` ideom
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/scanner.cc PROPERTIES
             COMPILE_FLAGS "-Wno-error=unused-parameter")

# --------------------------------------------------
# Souffle library
# --------------------------------------------------
add_library(libsouffle STATIC
            ${SOUFFLE_SOURCES}
            ${FLEX_scanner_OUTPUTS}
            ${BISON_parser_OUTPUTS}
            )

# Make most of the attributes public.  Note that that's strictly not "correct" but
# this library is not going to be installed and it makes it easier to build
# the other targets in this project.  It encapsulates things nicely.
target_include_directories(libsouffle
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                                  "${CMAKE_CURRENT_SOURCE_DIR}"
                                  "${CMAKE_CURRENT_BINARY_DIR}"
                           )

# install souffle directory
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/souffle DESTINATION include)

# Set C++ standard to C++17
target_compile_features(libsouffle
                        PUBLIC cxx_std_17)
set_target_properties(libsouffle PROPERTIES CXX_EXTENSIONS OFF)

target_compile_options(libsouffle
                       PUBLIC "-Wall;-Wextra;-Werror;-fwrapv")

target_link_libraries(libsouffle PRIVATE Threads::Threads)

if(SOUFFLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(libsouffle PUBLIC coverage_config)
endif(SOUFFLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

# FIXME: Better to have a library list
if (OPENMP_FOUND)
    target_link_libraries(libsouffle
                      PUBLIC OpenMP::OpenMP_CXX
                             $<$<BOOL:${SOUFFLE_USE_ZLIB}>:ZLIB::ZLIB>
                             $<$<BOOL:${SOUFFLE_USE_SQLITE}>:SQLite::SQLite3>
                             $<$<BOOL:${SOUFFLE_USE_CURSES}>:Curses::NCurses>
                             $<$<BOOL:${SOUFFLE_USE_LIBFFI}>:LibFFI::LibFFI>
                             "${CMAKE_DL_LIBS}")
else()
    target_link_libraries(libsouffle
                      PUBLIC $<$<BOOL:${SOUFFLE_USE_ZLIB}>:ZLIB::ZLIB>
                             $<$<BOOL:${SOUFFLE_USE_SQLITE}>:SQLite::SQLite3>
                             $<$<BOOL:${SOUFFLE_USE_CURSES}>:Curses::NCurses>
                             $<$<BOOL:${SOUFFLE_USE_LIBFFI}>:LibFFI::LibFFI>
                             "${CMAKE_DL_LIBS}")
endif()


if (SOUFFLE_DOMAIN_64BIT)
    target_compile_definitions(libsouffle
                               PUBLIC RAM_DOMAIN_SIZE=64)
endif()

if (SOUFFLE_USE_CURSES)
    target_compile_definitions(libsouffle
                               PUBLIC USE_NCURSES)
endif()

if (SOUFFLE_USE_ZLIB)
    target_compile_definitions(libsouffle
                               PUBLIC USE_LIBZ)
endif()

if (SOUFFLE_USE_SQLITE)
    target_compile_definitions(libsouffle
                               PUBLIC USE_SQLITE)
endif()

if (SOUFFLE_USE_LIBFFI)
    target_compile_definitions(libsouffle
      PUBLIC USE_LIBFFI)
endif()

# The target names "souffle" for the library and "souffle" for the binary
# clash in cmake.  I could just rename the library (since it's private)
# but to keep things "familiar", I renamed the target to "libsouffle"
# but have to adjust the output name, otherwise the library would be
# called "liblibsouffle"
set_target_properties(libsouffle PROPERTIES OUTPUT_NAME "souffle")

# --------------------------------------------------
# Souffle binary
# --------------------------------------------------
add_executable(souffle
               main.cpp)
target_link_libraries(souffle libsouffle)
install(TARGETS souffle DESTINATION bin)

# --------------------------------------------------
# Souffle's profiler binary
# --------------------------------------------------


add_executable(souffleprof
               souffle_prof.cpp)
target_include_directories(souffleprof PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
install(TARGETS souffleprof DESTINATION bin)

# Set C++ standard to C++17
target_compile_features(souffleprof
                        PUBLIC cxx_std_17)

# --------------------------------------------------
# Substitutions for souffle-compile
# --------------------------------------------------
# FIXME: This is a bit fragile but there is actually no better way
# to do this, AFAIK
# Maybe we could make a cmake script instead of using shell?
function(AUTOMAKE_COMPATIBLE_SUBST F_IN F_OUT)
    # enclose in scope so we can create the
    # values to substitute

    set(CXX ${CMAKE_CXX_COMPILER})

    set(CMAKE_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")

    set(CPPFLAGS "")

    # Compile definitions
    get_target_property(COMP_DEFS libsouffle COMPILE_DEFINITIONS)
    set(SUBST_DEFS "")
    foreach(DEF ${COMP_DEFS})
        string(APPEND SUBST_DEFS "-D${DEF} ")
    endforeach()

    # Compile flags
    string(TOUPPER ${CMAKE_BUILD_TYPE} BT_UPPER)
    # Remove -Werror from the default flag set for compilation using `souffle-compile`.
    # *) `-d` option in `souffle-compile` adds this flag back
    # *) The compiler used by `souffle-compile` isn't neccesarily the same as the one used by `cmake`.
    #    Code that's free of warnings w/ the cmake compiler might not be w/ `souffle-compile`'s compiler.
    #    e.g. we might use clang-10, user might use gcc or some newer version of clang.
    # *) Synth'd code isn't warning free for now anyways.
    #    TODO: Require/verify that test case synthesised code is warning free as part of test suite.
    string(REGEX REPLACE "-Werror" ""  SOUFFLE_CXXFLAGS "${CMAKE_CXX_FLAGS}")
    set(SOUFFLE_CXXFLAGS "${SOUFFLE_CXXFLAGS} ${CMAKE_CXX_FLAGS_${BT_UPPER}} ${SUBST_DEFS}")
    if (OPENMP_FOUND)
       set(SOUFFLE_CXXFLAGS "${SOUFFLE_CXXFLAGS} -fopenmp")
    endif()

    # Libraries
    set(LIBS "-lpthread")

    if (CMAKE_DL_LIBS)
       string(APPEND LIBS " -l${CMAKE_DL_LIBS}")
    endif()

    if (SOUFFLE_USE_SQLITE)
        string(APPEND LIBS " ${SQLite3_LIBRARY}")
    endif()

    if (SOUFFLE_USE_ZLIB)
        string(APPEND LIBS " ${ZLIB_LIBRARY_RELEASE}")
    endif()

    if (SOUFFLE_USE_CURSES)
        string(APPEND LIBS " ${CURSES_NCURSES_LIBRARY}")
    endif()

    configure_file("${F_IN}" "${F_OUT}" @ONLY)
endfunction()

automake_compatible_subst("${CMAKE_CURRENT_SOURCE_DIR}/souffle-compile.in"
                          "${CMAKE_CURRENT_BINARY_DIR}/souffle-compile")

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/souffle-compile DESTINATION bin)

# FIXME: Ideally, eventually we will move these out to the "tests" subdirectory
# now that we have a sane(er?) build system
if (SOUFFLE_ENABLE_TESTING)
    add_subdirectory(ast/tests)
    add_subdirectory(ram/tests)
    add_subdirectory(interpreter/tests)
endif()
