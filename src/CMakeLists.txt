# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt


set(SOUFFLE_SOURCES
    FunctorOps.cpp
    Global.cpp
    MainDriver.cpp
    ast/Aggregator.cpp
    ast/IntrinsicAggregator.cpp
    ast/UserDefinedAggregator.cpp
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
    ast/IterationCounter.cpp
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
    ast/analysis/JoinSize.cpp
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
    ast/transform/DebugDeltaRelation.cpp
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
    ast/transform/ReplaceSingletonVariables.cpp
    ast/transform/ResolveAliases.cpp
    ast/transform/ResolveAnonymousRecordAliases.cpp
    ast/transform/SemanticChecker.cpp
    ast/transform/SimplifyConstantBinaryConstraints.cpp
    ast/transform/SubsumptionQualifier.cpp
    ast/transform/SimplifyAggregateTargetExpression.cpp
    ast/transform/Transformer.cpp
    ast/transform/TypeChecker.cpp
    ast/transform/UniqueAggregationVariables.cpp
    ast/utility/BindingStore.cpp
    ast2ram/utility/SipsMetric.cpp
    ast2ram/utility/SipGraph.cpp
    ast/utility/Utils.cpp
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
    parser/VirtualFileSystem.cpp
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
    reports/ErrorReport.cpp
    reports/DebugReport.cpp
    synthesiser/Synthesiser.cpp
    synthesiser/Relation.cpp
    synthesiser/Utils.cpp
    synthesiser/GenDb.cpp
)

# --------------------------------------------------
# Flex/Bison
# --------------------------------------------------
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/parser")

flex_target(scanner parser/scanner.ll ${CMAKE_CURRENT_BINARY_DIR}/parser/scanner.cc
  COMPILE_FLAGS "${SCANNER_COMPILE_FLAGS} -d")

bison_target(parser parser/parser.yy ${CMAKE_CURRENT_BINARY_DIR}/parser/parser.cc
             COMPILE_FLAGS "-Wall -Wno-error=deprecated -Wno-error=other -v -d")
add_flex_bison_dependency(scanner parser)


if (MSVC)
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/scanner.cc PROPERTIES
             COMPILE_FLAGS "/wd4005 /wd4996")
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/parser.cc PROPERTIES
             COMPILE_FLAGS "/wd4005 /wd26819")
elseif (EMSCRIPTEN)
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/parser.cc PROPERTIES
             COMPILE_FLAGS "-Wno-error=unused-but-set-variable")
else ()
  # OSX compiler doesn't recognise `(void)var;` ideom
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/scanner.cc PROPERTIES
             COMPILE_FLAGS "-Wno-error=unused-parameter")
endif ()

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

add_library(compiled STATIC dummy.cpp)

target_include_directories(compiled PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Set C++ standard to C++17
target_compile_features(libsouffle PUBLIC cxx_std_17)
target_compile_features(compiled PUBLIC cxx_std_17)

set_target_properties(libsouffle PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(libsouffle PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(compiled PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(compiled PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (NOT MSVC)
  target_compile_options(libsouffle PUBLIC -Wall -Wextra -fwrapv)
else ()
  target_compile_options(libsouffle PUBLIC /W3 /WX)
endif ()

target_compile_options(compiled PUBLIC "")

if (Threads_FOUND)
  target_link_libraries(libsouffle PUBLIC Threads::Threads)
  target_link_libraries(compiled PUBLIC Threads::Threads)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(libsouffle PUBLIC stdc++fs)
    target_link_libraries(compiled PUBLIC stdc++fs)
  endif ()
endif ()


if(SOUFFLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(libsouffle PUBLIC coverage_config)
endif ()


target_link_libraries(libsouffle PUBLIC "${CMAKE_DL_LIBS}")
target_link_libraries(compiled PUBLIC "${CMAKE_DL_LIBS}")


if (SOUFFLE_DOMAIN_64BIT)
    target_compile_definitions(libsouffle PUBLIC RAM_DOMAIN_SIZE=64)
    target_compile_definitions(compiled PUBLIC RAM_DOMAIN_SIZE=64)
endif()

if (SOUFFLE_USE_LIBFFI)
if (libffi_FOUND)
  target_link_libraries(libsouffle PUBLIC libffi)
else ()
  target_link_libraries(libsouffle PUBLIC LibFFI::LibFFI)
endif ()
endif ()

if (OPENMP_FOUND)
    target_link_libraries(libsouffle PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(compiled PUBLIC OpenMP::OpenMP_CXX)
endif()

if (SOUFFLE_CUSTOM_GETOPTLONG)
    target_compile_definitions(libsouffle
                               PUBLIC USE_CUSTOM_GETOPTLONG)
    target_compile_definitions(compiled
                               PUBLIC USE_CUSTOM_GETOPTLONG)
endif()

if (SOUFFLE_USE_CURSES)
    target_compile_definitions(libsouffle PUBLIC USE_NCURSES)
    target_compile_definitions(compiled PUBLIC USE_NCURSES)
    target_link_libraries(libsouffle PUBLIC Curses::NCurses)
    target_link_libraries(compiled PUBLIC Curses::NCurses)
endif()

if (SOUFFLE_USE_ZLIB)
    target_compile_definitions(libsouffle PUBLIC USE_LIBZ)
    target_compile_definitions(compiled PUBLIC USE_LIBZ)
    target_link_libraries(libsouffle PUBLIC ZLIB::ZLIB)
    target_link_libraries(compiled PUBLIC ZLIB::ZLIB)
endif()

if (SOUFFLE_USE_SQLITE)
    target_compile_definitions(libsouffle PUBLIC USE_SQLITE)
    target_compile_definitions(compiled PUBLIC USE_SQLITE)
    target_link_libraries(libsouffle PUBLIC SQLite::SQLite3)
    target_link_libraries(compiled PUBLIC SQLite::SQLite3)
    target_include_directories(compiled PUBLIC ${SQLite3_INCLUDE_DIRS})
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
               souffle.cpp)
target_link_libraries(souffle libsouffle)
install(TARGETS souffle DESTINATION bin)
# add_executable(ram2se
#                ram2se.cpp)
# target_link_libraries(ram2se libsouffle)

if (EMSCRIPTEN)
target_link_libraries(souffle -sMODULARIZE=1
                      -s'EXPORTED_RUNTIME_METHODS=["FS"]' -sEXPORT_NAME="SOUFFLE")
endif()

# Copy the dlls in the same directory as Souffle so that they will
# be immediately found by the operating system.

if (SOUFFLE_USE_SQLITE)
  add_custom_command(TARGET souffle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SQLite3_LIBRARY}
    $<TARGET_FILE_DIR:souffle>)
  if (WIN32)
    cmake_path(GET SQLite3_LIBRARY PARENT_PATH SQLite3_LIBRARY_DIR)
    add_custom_command(TARGET souffle POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${SQLite3_LIBRARY_DIR}/../bin/sqlite3.dll
      $<TARGET_FILE_DIR:souffle>)
  endif ()
endif ()

if (SOUFFLE_USE_ZLIB)
add_custom_command(TARGET souffle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${ZLIB_LIBRARY_RELEASE}
  $<TARGET_FILE_DIR:souffle>)
endif ()

if (SOUFFLE_USE_CURSES)
add_custom_command(TARGET souffle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${CURSES_NCURSES_LIBRARY}
  $<TARGET_FILE_DIR:souffle>)
endif ()

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

if (MSVC)
  target_compile_options(libsouffle PUBLIC /bigobj)
  target_compile_options(compiled PUBLIC /bigobj)

  target_compile_options(libsouffle PUBLIC /wd5105)
  target_compile_options(compiled PUBLIC /wd5105)

  target_compile_options(libsouffle PUBLIC /wd6326)
  target_compile_options(compiled PUBLIC /wd6326)

  target_compile_options(libsouffle PUBLIC /wd26110)
  target_compile_options(libsouffle PUBLIC /wd4065 /wd4200)

  # to prevent old versions of Flex from redeclaring int types
  target_compile_definitions(libsouffle PRIVATE __STDC_LIMIT_MACROS)

  target_compile_definitions(libsouffle PUBLIC _CRT_SECURE_NO_WARNINGS)

  if (NOT (MSVC_VERSION LESS 1910))
    target_compile_options(libsouffle PUBLIC /permissive-)
    target_compile_options(compiled PUBLIC /permissive-)
  endif()

  target_compile_options(libsouffle PUBLIC /Zc:preprocessor)
  target_compile_options(compiled PUBLIC /Zc:preprocessor)
  target_compile_options(souffleprof PUBLIC /Zc:preprocessor)

  target_compile_options(libsouffle PUBLIC /EHsc)
  target_compile_options(compiled PUBLIC /EHsc)
  target_compile_options(souffleprof PUBLIC /EHsc)

  target_compile_definitions(libsouffle PUBLIC USE_CUSTOM_GETOPTLONG)
  target_compile_definitions(compiled PUBLIC USE_CUSTOM_GETOPTLONG)
  target_compile_definitions(souffleprof PUBLIC USE_CUSTOM_GETOPTLONG)
endif (MSVC)



# --------------------------------------------------
# Substitutions for souffle-compile.py
# --------------------------------------------------
set(SOUFFLE_COMPILED_CXX_COMPILER ${CMAKE_CXX_COMPILER})
set(SOUFFLE_COMPILED_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID})
set(SOUFFLE_COMPILED_CXX_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
set(SOUFFLE_COMPILED_CXX_STANDARD ${CMAKE_CXX17_STANDARD_COMPILE_OPTION})
set(SOUFFLE_COMPILED_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(SOUFFLE_COMPILED_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS})
set(SOUFFLE_COMPILED_RELEASE_CXX_FLAGS ${CMAKE_CXX_FLAGS_RELEASE})
set(SOUFFLE_COMPILED_DEBUG_CXX_FLAGS ${CMAKE_CXX_FLAGS_DEBUG})
get_target_property(SOUFFLE_COMPILED_DEFS compiled COMPILE_DEFINITIONS)
get_target_property(SOUFFLE_COMPILED_OPTS compiled COMPILE_OPTIONS)
get_target_property(SOUFFLE_COMPILED_INCS compiled INCLUDE_DIRECTORIES)

set(SOUFFLE_COMPILED_LIBS "")
set(SOUFFLE_COMPILED_RPATHS "")

if (THREADS_FOUND)
  list(APPEND SOUFFLE_COMPILED_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

if (OPENMP_FOUND)
  string(APPEND SOUFFLE_COMPILED_CXX_FLAGS " ${OpenMP_CXX_FLAGS}")
  list(APPEND SOUFFLE_COMPILED_INCS ${OpenMP_CXX_INCLUDE_DIRS})
endif()

if (CMAKE_DL_LIBS)
  list(APPEND SOUFFLE_COMPILED_LIBS -l${CMAKE_DL_LIBS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    list(APPEND SOUFFLE_COMPILED_LIBS -lstdc++fs)
  endif ()
endif()

if (SOUFFLE_USE_SQLITE)
  list(APPEND SOUFFLE_COMPILED_LIBS ${SQLite3_LIBRARY})
  if (COMMAND cmake_path)
    cmake_path(GET SQLite3_LIBRARY PARENT_PATH SQLite3_RPATH)
  else ()
    get_filename_component(SQLite3_RPATH ${SQLite3_LIBRARY} DIRECTORY)
  endif ()
  list(APPEND SOUFFLE_COMPILED_RPATHS ${SQLite3_RPATH})
endif()

if (SOUFFLE_USE_ZLIB)
  list(APPEND SOUFFLE_COMPILED_LIBS ${ZLIB_LIBRARY_RELEASE})
  if (COMMAND cmake_path)
    cmake_path(GET ZLIB_LIBRARY_RELEASE PARENT_PATH ZLIB_RPATH)
  else ()
    get_filename_component(ZLIB_RPATH ${ZLIB_LIBRARY_RELEASE} DIRECTORY)
  endif ()
  list(APPEND SOUFFLE_COMPILED_RPATHS ${ZLIB_RPATH})
endif()

if (SOUFFLE_USE_CURSES)
  list(APPEND SOUFFLE_COMPILED_LIBS ${CURSES_NCURSES_LIBRARY})
  if (COMMAND cmake_path)
    cmake_path(GET CURSES_NCURSES_LIBRARY PARENT_PATH NCURSES_RPATH)
  else ()
    get_filename_component(NCURSES_RPATH ${CURSES_NCURSES_LIBRARY} DIRECTORY)
  endif ()
  list(APPEND SOUFFLE_COMPILED_RPATHS ${NCURSES_RPATH})
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # using Python3 PEP 3101 Format String:
  set(OUTNAME_FMT "-o {}")
  set(LIBDIR_FMT "-L{}")
  set(LIBNAME_FMT "-l{}")
  set(RPATH_FMT "-Wl,-rpath,{}")
  set(EXE_EXTENSION "")
  set(OS_PATH_DELIMITER ":")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # using Python3 PEP 3101 Format String:
  set(OUTNAME_FMT "/Fe:{}")
  set(LIBDIR_FMT "/libpath:{}")
  set(LIBNAME_FMT "{}.lib")
  set(RPATH_FMT "")
  set(EXE_EXTENSION ".exe")
  set(OS_PATH_DELIMITER ";")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  ## Is is very important that the Runtime Library match between
  ## the Souffle compiled program and any dynamically loaded functor library.
  if (CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "^MultiThreaded(Debug)?$")
    # static multi-threaded library
    string(APPEND SOUFFLE_COMPILED_DEBUG_CXX_FLAGS " /MTd")
    string(APPEND SOUFFLE_COMPILED_RELEASE_CXX_FLAGS " /MT")
  else ()
    # dynamic multi-threaded library
    string(APPEND SOUFFLE_COMPILED_DEBUG_CXX_FLAGS " /MDd")
    string(APPEND SOUFFLE_COMPILED_RELEASE_CXX_FLAGS " /MD")
  endif ()
endif ()

list(JOIN SOUFFLE_COMPILED_OPTS " " SOUFFLE_COMPILED_CXX_OPTIONS)

list(JOIN SOUFFLE_COMPILED_LIBS " " SOUFFLE_COMPILED_LINK_OPTIONS)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(SOUFFLE_COMPILED_LINK_OPTIONS "/link ${SOUFFLE_COMPILED_LINK_OPTIONS}")
endif ()

list(JOIN SOUFFLE_COMPILED_RPATHS "${OS_PATH_DELIMITER}" SOUFFLE_COMPILED_RPATH_LIST)

list(TRANSFORM SOUFFLE_COMPILED_INCS PREPEND "-I")
list(JOIN SOUFFLE_COMPILED_INCS " " SOUFFLE_COMPILED_INCLUDES)

list(TRANSFORM SOUFFLE_COMPILED_DEFS PREPEND "-D")
list(JOIN SOUFFLE_COMPILED_DEFS " " SOUFFLE_COMPILED_DEFINITIONS)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/souffle-compile.template.py TEMPLATE)

# json parameters injected at the begining of souffle-compile.py
set(SOUFFLE_COMPILE_PY
"#!/usr/bin/env python3
JSON_DATA_TEXT = \"\"\"{
  \"compiler\": \"${SOUFFLE_COMPILED_CXX_COMPILER}\",
  \"compiler_id\": \"${SOUFFLE_COMPILED_CXX_COMPILER_ID}\",
  \"compiler_version\": \"${SOUFFLE_COMPILED_CXX_COMPILER_VERSION}\",
  \"msvc_version\": \"${MSVC_VERSION}\",
  \"includes\": \"${SOUFFLE_COMPILED_INCLUDES}\",
  \"std_flag\": \"${SOUFFLE_COMPILED_CXX_STANDARD}\",
  \"cxx_flags\": \"${SOUFFLE_COMPILED_CXX_FLAGS}\",
  \"cxx_link_flags\": \"${SOUFFLE_COMPILED_CXX_LINK_FLAGS}\",
  \"release_cxx_flags\": \"${SOUFFLE_COMPILED_RELEASE_CXX_FLAGS}\",
  \"debug_cxx_flags\": \"${SOUFFLE_COMPILED_DEBUG_CXX_FLAGS}\",
  \"definitions\": \"${SOUFFLE_COMPILED_DEFINITIONS}\",
  \"compile_options\": \"${SOUFFLE_COMPILED_CXX_OPTIONS}\",
  \"link_options\": \"${SOUFFLE_COMPILED_LINK_OPTIONS}\",
  \"rpaths\": \"${SOUFFLE_COMPILED_RPATH_LIST}\",
  \"outname_fmt\": \"${OUTNAME_FMT}\",
  \"libdir_fmt\": \"${LIBDIR_FMT}\",
  \"libname_fmt\": \"${LIBNAME_FMT}\",
  \"rpath_fmt\": \"${RPATH_FMT}\",
  \"path_delimiter\": \"${OS_PATH_DELIMITER}\",
  \"exe_extension\": \"${EXE_EXTENSION}\",
  \"source_include_dir\": \"${CMAKE_CURRENT_SOURCE_DIR}/include\",
  \"jni_includes\": \"${JAVA_INCLUDE_PATH}${OS_PATH_DELIMITER}${JAVA_INCLUDE_PATH2}\"
}\"\"\"
${TEMPLATE}
")

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/souffle-compile.py" CONTENT "${SOUFFLE_COMPILE_PY}")
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/souffle-compile.py DESTINATION bin)

# ---------------------------------------

# FIXME: Ideally, eventually we will move these out to the "tests" subdirectory
# now that we have a sane(er?) build system
if (SOUFFLE_ENABLE_TESTING)
    add_subdirectory(ast/tests)
    add_subdirectory(ram/tests)
    add_subdirectory(interpreter/tests)
endif()
