# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

# TODO
# Add option to run tests from install directory
# Update README with cmake instructions
# bash completion
# clang-format, clang-tidy etc
# Packaging
# substitute changelog

# Future TODO/improvements:
# - Replace all the shell scripts (e.g. souffle-compile and all the testing helpers)
#   with Python so it's cross-platform.  @b-scholz has agreed it's ok to have Python
#   as a dependency
# - See if we can call back into cmake so that we have a better mechanism for building
#   consistenly rather than trying to thread compile flags etc into e.g. souffle-compile
# - Make it easier to include Souffle as a subproject using add_subdirectory
# - Currently, there a few unit tests in the src directory, we should
#   migrate them to test to that they can be excluded if needed

cmake_minimum_required(VERSION 3.15)

include(CMakeDependentOption)

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
option(SOUFFLE_DOMAIN_64BIT "Enable/Disable 64-bit number values in Datalog tuples" OFF)
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
        find_package(Python "3.7" REQUIRED)
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
find_package(LibFFI REQUIRED)

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
  if (OPENMP_FOUND)
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  endif()
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
    add_subdirectory(src/tests)
    add_subdirectory(tests)
endif()


# --------------------------------------------------
# Installing bash completion file
# --------------------------------------------------
find_package (bash-completion)
if (BASH_COMPLETION_FOUND)
    message(STATUS "Using bash completion dir ${BASH_COMPLETION_COMPLETIONSDIR}")
else()
    set (BASH_COMPLETION_COMPLETIONSDIR "/etc/bash_completion.d")
    message (STATUS "Using fallback bash completion dir ${BASH_COMPLETION_COMPLETIONSDIR}")
endif()

install(
    FILES "${CMAKE_SOURCE_DIR}/debian/souffle.bash-completion"
    DESTINATION ${BASH_COMPLETION_COMPLETIONSDIR}
    RENAME "souffle"
)

# --------------------------------------------------
# Utility function to help us distinguish between Linux distros when packaging
# --------------------------------------------------

function(get_linux_lsb_release_information)
    find_program(LSB_RELEASE_EXEC lsb_release)
    if(NOT LSB_RELEASE_EXEC)
        message(FATAL_ERROR "Could not detect lsb_release executable, can not gather required information")
    endif()

    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --id OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --release OUTPUT_VARIABLE LSB_RELEASE_VERSION_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${LSB_RELEASE_EXEC}" --short --codename OUTPUT_VARIABLE LSB_RELEASE_CODENAME_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(LSB_RELEASE_ID_SHORT "${LSB_RELEASE_ID_SHORT}" PARENT_SCOPE)
    set(LSB_RELEASE_VERSION_SHORT "${LSB_RELEASE_VERSION_SHORT}" PARENT_SCOPE)
    set(LSB_RELEASE_CODENAME_SHORT "${LSB_RELEASE_CODENAME_SHORT}" PARENT_SCOPE)
endfunction()

# --------------------------------------------------
# CPack configuration
# --------------------------------------------------

SET(CPACK_PACKAGE_CONTACT "Patrick H.")
SET(CPACK_PACKAGE_DESCRIPTION "Souffle - A Datalog Compiler")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Datalog Compiler")

# Use all available threads (primarily for compression of files)
SET(CPACK_THREADS 0)

# Make sure changelog, bash-completion and other important files in debian directory also packaged
SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/debian/changelog.in" "${CMAKE_SOURCE_DIR}/debian/souffle.bash-completion" "${CMAKE_SOURCE_DIR}/debian/copyright")

# --------------------------------------------------
# CPack configuration for Linux
# --------------------------------------------------
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	get_linux_lsb_release_information()
    if (LSB_RELEASE_ID_SHORT MATCHES "Ubuntu")
		# Generate just DEB
		SET(CPACK_GENERATOR "DEB")
		# --------------------------------------------------
		# Variables relevent to DEB packages
		# --------------------------------------------------

		# Specifying runtime dependencies
		set(CPACK_DEBIAN_PACKAGE_DEPENDS "g++ (>= 7), libffi-dev, libncurses5-dev, libsqlite3-dev, mcpp, zlib1g-dev")

		# Auto-generate any runtime dependencies that are required
		SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

		# Architectures are actually auto-detected so no need to set this variable
		# SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
	endif()

    if (LSB_RELEASE_ID_SHORT MATCHES "Fedora")
		# Generate both DEB and RPM packages
		SET(CPACK_GENERATOR "RPM")

		# --------------------------------------------------
		# Variables relevent to RPM packages
		# --------------------------------------------------

		# Specifying runtime dependencies
		set(CPACK_RPM_PACKAGE_REQUIRES "g++ >= 7, libffi, libffi-devel, ncurses-devel, libsqlite3x, mcpp, zlib-devel")

		# Don't auto-detect dependencies and provides
		SET(CPACK_RPM_PACKAGE_AUTOREQPROV "no")
	endif()
endif()


INCLUDE(CPack)
