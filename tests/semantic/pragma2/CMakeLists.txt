# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

# FUTURE WORK:  Ideally we'd like to test/support libs with spaces in their file name
#               However, the mere idea of this is so god damn hertical on linux that
#               pretty much *everything* seems irredemably broken.
#               No amount of escaping/quoting tricks seem to work.
#
#   CMake w/ Makefile output: Doesn't quote the `-Wl,-soname,lib<name>.so` in its link scripts
#   GNU ld: Corrupts the soname of linked executables. ask for `-l'foo bar'`?
#           Ok, but the output bin will dylink `libfoo bar.so`
#
# Interpreter test works, even with the weirdest quoting & spaces.
# Synthesis is broke because of the bugs mentioned above.

function(SOUFFLE_TEST_SEMANTIC_PRAGMA2_SETUP FILE SO_NAME)
    add_library(${FILE} SHARED "${FILE}.cpp")
    target_include_directories(${FILE} PRIVATE "${CMAKE_SOURCE_DIR}/src/include")

    target_compile_features(${FILE}
                            PUBLIC cxx_std_17)
    set_target_properties(
        ${FILE}
        PROPERTIES
            OUTPUT_NAME "'${SO_NAME}'"
            CXX_EXTENSIONS OFF
            WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    if (NOT MSVC)
        target_compile_options(${FILE} PUBLIC "-Wall;-Wextra;-fwrapv")
      else ()
        target_compile_options(${FILE} PUBLIC /W3 /WX)
    endif()

    if (WIN32)
      # Prefix all shared libraries with 'lib'.
      set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

      # Prefix all static libraries with 'lib'.
      set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
    endif ()

    if (SOUFFLE_DOMAIN_64BIT)
        target_compile_definitions(${FILE} PUBLIC RAM_DOMAIN_SIZE=64)
    endif()
endfunction()

souffle_test_semantic_pragma2_setup(lib_foo "never gonna")
souffle_test_semantic_pragma2_setup(lib_bar "let you")
