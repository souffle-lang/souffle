# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)

include(CMakeDependentOption)

option(SOUFFLE_GIT "Enable/Disable git completion" ON)

if (SOUFFLE_GIT) 
    find_package(Git REQUIRED)

    # PACKAGE_VERSION is the full tag with git hash
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    RESULT_VARIABLE GIT_RESULT
                    OUTPUT_VARIABLE GIT_PACKAGE_VERSION)
                    # FIXME: Use in cmake 3.19 or later
                    # COMMAND_ERROR_IS_FATAL ANY)

    # Figure out the version number, depends on whether building from the git repo
    if (NOT GIT_RESULT EQUAL 0)
        # Not building from a git clone
        message(WARNING "Unable to find git repository: version number will be incomplete")
        set(PACKAGE_VERSION "UNKNOWN")
        set(SOUFFLE_VERSION "")
    else()
        string(REGEX REPLACE "\n$" "" PACKAGE_VERSION "${GIT_PACKAGE_VERSION}")
        message(STATUS "Building souffle version ${PACKAGE_VERSION}")
    
        # SOUFFLE_VERSION only includes the major/minor triplet
        string(REGEX REPLACE "-.*$" "" SOUFFLE_VERSION "${PACKAGE_VERSION}")

        # If building from a shallow clone where tag is not available.
        if (NOT ${SOUFFLE_VERSION} MATCHES "^[0-9.]+$")
            message(WARNING "Cannot find a valid tag: cmake project version will be incomplete")
            set (SOUFFLE_VERSION "")
        endif()
    endif()
endif()

project(souffle VERSION "${SOUFFLE_VERSION}"
                DESCRIPTION "A datalog compiler"
                LANGUAGES CXX)
include(CTest)

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build" FORCE)
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Tells us whether we're building souffle as a main project
# or a subprobject
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SOUFFLE_MASTER_PROJECT On)
else()
    set(SOUFFLE_MASTER_PROJECT Off)
endif()

if (SOUFFLE_MASTER_PROJECT AND BUILD_TESTING)
    set(SOUFFLE_ENABLE_TESTING_DEFAULT ON)
else()
    set(SOUFFLE_ENABLE_TESTING_DEFAULT OFF)
endif()

# --------------------------------------------------
# User options available from the command line/cache
# --------------------------------------------------
option(SOUFFLE_DOMAIN_64BIT "Enable/Disable 64-bit number values in Datalog tuples" ON)
option(SOUFFLE_USE_CURSES "Enable/Disable ncurses-based provenance display" ON)
option(SOUFFLE_SWIG "Enable/Disable all SWIG builds" OFF)
option(SOUFFLE_SWIG_PYTHON "Enable/Disable Python SWIG" OFF)
option(SOUFFLE_SWIG_JAVA "Enable/Disable Java SWIG" OFF)
option(SOUFFLE_USE_ZLIB "Enable/Disable use of libz file compression" ON)
option(SOUFFLE_USE_SQLITE "Enable/Disable use sqlite IO" ON)
option(SOUFFLE_USE_OPENMP "Enable/Disable use of openmp if available" ON)
option(SOUFFLE_SANITISE_MEMORY "Enable/Disable memory sanitiser" OFF)
option(SOUFFLE_SANITISE_THREAD "Enable/Disable thread sanitiser" OFF)
# SOUFFLE_NDEBUG = ON means -DNDEBUG on the compiler command line = no cassert
# Therefor SOUFFLE_NDEBUG = OFF means keep asserts
option(SOUFFLE_NDEBUG "Enable/Disable runtime checks even in release mode" OFF)
# By default, the "test/example" directory is not part of testing
# this flag enables the tests to run
option(SOUFFLE_TEST_EXAMPLES "Enable/Disable testing of code examples in tests/examples" OFF)
option(SOUFFLE_TEST_EVALUATION "Enable/Disable testing of evaluation examples in tests/examples" ON)
option(SOUFFLE_ENABLE_TESTING "Enable/Disable testing" ${SOUFFLE_ENABLE_TESTING_DEFAULT})
option(SOUFFLE_GENERATE_DOXYGEN "Generate Doxygen files (html;htmlhelp;man;rtf;xml;latex)" "")
option(SOUFFLE_CODE_COVERAGE "Enable coverage reporting" OFF)
option(SOUFFLE_BASH_COMPLETION "Enable/Disable bash completion" OFF)
option(SOUFFLE_USE_LIBFFI "Enable/Disable use of libffi" ON)
option(SOUFFLE_CUSTOM_GETOPTLONG "Enable/Disable custom getopt_long implementation" OFF)

cmake_dependent_option(SOUFFLE_USE_LIBCPP "Link to libc++ instead of libstdc++" ON
    "CMAKE_CXX_COMPILER_ID STREQUAL Clang" OFF)

# Using Clang? Likely want to use `lld` too.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
endif()

# Add aditional modules to CMake
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if (SOUFFLE_USE_LIBCPP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -lc++abi")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi")
endif()

if (WIN32)
  # Search libraries with and without 'lib' prefix.
  set(CMAKE_FIND_LIBRARY_PREFIXES ";lib")

  # Prefix all shared libraries with 'lib'.
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

  # Prefix all static libraries with 'lib'.
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")

  SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
  SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
  SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
endif ()


if(EMSCRIPTEN)
  set(SOUFFLE_USE_SQLITE off)
  set(SOUFFLE_USE_OPENMP off)
  set(SOUFFLE_USE_ZLIB off)
  set(SOUFFLE_USE_LIBFFI off)
  set(SOUFFLE_USE_CURSES off)
  set(SOUFFLE_ENABLE_TESTING off)
endif ()
# --------------------------------------------------
# curses libraries for Provenance information
# --------------------------------------------------
if (SOUFFLE_USE_CURSES)
    find_package(Curses REQUIRED)
    if(NOT TARGET Curses::NCurses)
        add_library(Curses::NCurses UNKNOWN IMPORTED)
        set_target_properties(Curses::NCurses PROPERTIES
            IMPORTED_LOCATION "${CURSES_NCURSES_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIR}"
        )
    endif()
endif()

# --------------------------------------------------
# swig support
# --------------------------------------------------
if (SOUFFLE_SWIG)
    # Enable both
    set(SOUFFLE_SWIG_PYTHON "ON" CACHE STRING "" FORCE)
    set(SOUFFLE_SWIG_JAVA "ON" CACHE STRING "" FORCE)
endif()

if (SOUFFLE_SWIG_PYTHON OR SOUFFLE_SWIG_JAVA)
    find_package(SWIG REQUIRED)

    if (SOUFFLE_SWIG_PYTHON)
        find_package(Python3 3.7 REQUIRED)
    endif()

    if (SOUFFLE_SWIG_JAVA)
        find_package(Java REQUIRED
                     COMPONENTS Development)
        find_package(JNI REQUIRED)
        list(APPEND SOUFFLE_JAVA_INCLUDE_PATH ${JAVA_INCLUDE_PATH})
        list(APPEND SOUFFLE_JAVA_INCLUDE_PATH ${JAVA_INCLUDE_PATH2})
    endif()
endif()

# --------------------------------------------------
# flex and bison
# --------------------------------------------------
# See issue #2143.
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
    execute_process(
            COMMAND brew --prefix bison
            RESULT_VARIABLE BREW_BISON
            OUTPUT_VARIABLE BREW_BISON_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_BISON EQUAL 0 AND EXISTS "${BREW_BISON_PREFIX}")
        message(STATUS "Found Bison keg installed by Homebrew at ${BREW_BISON_PREFIX}")
        set(BISON_EXECUTABLE "${BREW_BISON_PREFIX}/bin/bison")
    endif()

    execute_process(
            COMMAND brew --prefix flex
            RESULT_VARIABLE BREW_FLEX
            OUTPUT_VARIABLE BREW_FLEX_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_FLEX EQUAL 0 AND EXISTS "${BREW_FLEX_PREFIX}")
        message(STATUS "Found Flex keg installed by Homebrew at ${BREW_FLEX_PREFIX}")
        set(FLEX_EXECUTABLE "${BREW_FLEX_PREFIX}/bin/flex")
    endif()
endif()
find_package(FLEX REQUIRED)
find_package(BISON "3.0.4" REQUIRED)

# --------------------------------------------------
# mcpp
# --------------------------------------------------
find_program(MCPP mcpp)

# --------------------------------------------------
# libz
# --------------------------------------------------
if (SOUFFLE_USE_ZLIB)
    find_package(ZLIB REQUIRED)
    if (SOUFFLE_ENABLE_TESTING)
        find_program(GZIP_BINARY gzip REQUIRED)
    endif()
endif()

# --------------------------------------------------
# sqlite
# --------------------------------------------------
if (SOUFFLE_USE_SQLITE)
    find_package(SQLite3 REQUIRED)
    if (SOUFFLE_ENABLE_TESTING)
        find_program(SQLITE3_BINARY sqlite3 REQUIRED)
    endif()
endif()

# --------------------------------------------------
# libffi
# --------------------------------------------------
if (SOUFFLE_USE_LIBFFI)
  find_package(libffi CONFIG QUIET)
  if (NOT libffi_FOUND)
    find_package(LibFFI REQUIRED)
  endif()
endif()

# --------------------------------------------------
# pthreads
# --------------------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# --------------------------------------------------
# OpenMP
# --------------------------------------------------
if (SOUFFLE_USE_OPENMP)
  find_package(OpenMP)
endif()

# --------------------------------------------------
# Memory Sanitiser
# --------------------------------------------------
if (SOUFFLE_SANITISE_MEMORY)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,leak,undefined")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak,undefined")
    else()
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,leak")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak")
    endif()
endif()

# --------------------------------------------------
# Thread Sanitiser
# --------------------------------------------------
if (SOUFFLE_SANITISE_THREAD)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

# --------------------------------------------------
# Doxygen
# --------------------------------------------------
if (SOUFFLE_GENERATE_DOXYGEN)
    find_package(Doxygen REQUIRED dot)
    set(DOXYGEN_IN "${PROJECT_SOURCE_DIR}/doxygen.in")
    set(DOXYGEN_CFG "${PROJECT_SOURCE_DIR}/Doxyfile")
    set(DOXYGEN_DIR "${PROJECT_SOURCE_DIR}/doc")

    if ("htmlhelp" IN_LIST SOUFFLE_GENERATE_DOXYGEN)
        set(DOXYGEN_GENERATE_HTMLHELP "YES")
    endif()
    if ("man" IN_LIST SOUFFLE_GENERATE_DOXYGEN)
        set(DOXYGEN_GENERATE_MAN "YES")
    endif()
    if ("rtf" IN_LIST SOUFFLE_GENERATE_DOXYGEN)
        set(DOXYGEN_GENERATE_RTF "YES")
    endif()
    if ("xml" IN_LIST SOUFFLE_GENERATE_DOXYGEN)
        set(DOXYGEN_GENERATE_XML "YES")
    endif()
    if ("latex" IN_LIST SOUFFLE_GENERATE_DOXYGEN)
        set(DOXYGEN_GENERATE_LATEX "YES")
    endif()

    configure_file(${DOXYGEN_IN} ${DOXYGEN_CFG})

    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CFG}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen")
endif()

# --------------------------------------------------
# Generate the config file
# --------------------------------------------------
configure_file("${PROJECT_SOURCE_DIR}/cmake/config.h.in"
               "${PROJECT_BINARY_DIR}/src/config.h")

# --------------------------------------------------
# Change compile flags
# --------------------------------------------------
if (NOT SOUFFLE_NDEBUG)
    foreach(FLAG_VAR
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELWITHDEBINFO)

            # Remove/keep NDEBUG in Release builds
            string(REGEX REPLACE "-DNDEBUG" "" ${FLAG_VAR} "${${FLAG_VAR}}")
    endforeach()
endif()

# --------------------------------------------------
# Code Coverage Mode
# --------------------------------------------------

if(SOUFFLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Code Coverage Configuration
    add_library(coverage_config INTERFACE)
    # Add required flags (GCC & LLVM/Clang)
    target_compile_options(coverage_config INTERFACE
        -O0        # no optimization
        -g         # generate debug info
        --coverage # sets all required flags
    )
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(coverage_config INTERFACE --coverage)
    else()
        target_link_libraries(coverage_config INTERFACE --coverage)
    endif()
endif(SOUFFLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

# There are a few tests in src, so add it *after*
# including CTest
add_subdirectory(src)

if (SOUFFLE_ENABLE_TESTING)
    find_package(Python3 3.7 REQUIRED)
    add_subdirectory(src/tests)
    add_subdirectory(tests)
endif()


# --------------------------------------------------
# Installing bash completion file
# --------------------------------------------------
IF (SOUFFLE_BASH_COMPLETION) 
    if(NOT DEFINED BASH_COMPLETION_COMPLETIONSDIR)
        find_package (bash-completion)
        if (BASH_COMPLETION_FOUND)
            message(STATUS "Using bash completion dir ${BASH_COMPLETION_COMPLETIONSDIR}")
        else()
            set (BASH_COMPLETION_COMPLETIONSDIR "/etc/bash_completion.d")
            message (STATUS "Using fallback bash completion dir ${BASH_COMPLETION_COMPLETIONSDIR}")
        endif()
    else()
        message(STATUS "Using user-provided bash completion dir ${BASH_COMPLETION_COMPLETIONSDIR}")
    endif()

    install(
        FILES "${CMAKE_SOURCE_DIR}/debian/souffle.bash-completion"
        DESTINATION ${BASH_COMPLETION_COMPLETIONSDIR}
        RENAME "souffle"
    )
endif()


if (NOT WIN32)
# --------------------------------------------------
# CPack configuration
# --------------------------------------------------
execute_process(COMMAND bash "${CMAKE_SOURCE_DIR}/sh/check_os.sh"
                RESULT_VARIABLE CHECK_OS_RESULT
                OUTPUT_VARIABLE CHECK_OS_OUTPUT)

SET(CPACK_PACKAGE_CONTACT "Bernhard Scholz <bernhard.scholz@sydney.edu.au>")
SET(CPACK_PACKAGE_DESCRIPTION "Souffle - A Datalog Compiler")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Datalog Compiler")

# Use all available threads (primarily for compression of files)
SET(CPACK_THREADS 0)

# Make sure changelog, bash-completion and other important files in debian directory also packaged
SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/debian/changelog" "${CMAKE_SOURCE_DIR}/debian/souffle.bash-completion" "${CMAKE_SOURCE_DIR}/debian/copyright")

# --------------------------------------------------
# CPack configuration 
# --------------------------------------------------
if (CHECK_OS_RESULT EQUAL 0)
    if (CHECK_OS_OUTPUT MATCHES "DEBIAN")
        # Generate DEB packages
        SET(CPACK_GENERATOR "DEB")
        # --------------------------------------------------
        # Variables relevent to DEB packages
        # --------------------------------------------------

        # Specifying runtime dependencies
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "g++ (>= 8), libffi-dev, libncurses5-dev, libsqlite3-dev, mcpp, zlib1g-dev, python3")

        # Auto-generate any runtime dependencies that are required
        SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

        # Architectures are actually auto-detected so no need to set this variable
        # SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
        INCLUDE(CPack)
    endif()

    if (CHECK_OS_OUTPUT MATCHES "FEDORA")
      SET(CPACK_RPM_PACKAGE_GROUP "Unspecified")
      SET(CPACK_RPM_PACKAGE_LICENSE "UPL-1.0 License")
      SET(CPACK_RPM_PACKAGE_VENDOR "Souffle-lang")
      SET(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")

      # Generate RPM packages
      SET(CPACK_GENERATOR "RPM")

      # --------------------------------------------------
      # Variables relevent to RPM packages
      # --------------------------------------------------

      # Specifying runtime dependencies
      set(CPACK_RPM_PACKAGE_REQUIRES "gcc-c++ >= 8, libffi, libffi-devel, ncurses-devel, sqlite-devel, mcpp, zlib-devel, python3")

      # Note: By default automatic dependency detection is enabled by rpm generator.
      # SET(CPACK_RPM_PACKAGE_AUTOREQPROV "no")
      INCLUDE(CPack)
    endif()
endif()
endif()
