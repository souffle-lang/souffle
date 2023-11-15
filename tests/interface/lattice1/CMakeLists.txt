# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

add_library(lattice1functors SHARED functors.cpp)
target_include_directories(lattice1functors PRIVATE "${CMAKE_SOURCE_DIR}/src/include")

target_compile_features(lattice1functors
                        PUBLIC cxx_std_17)

set_target_properties(lattice1functors PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(lattice1functors PROPERTIES OUTPUT_NAME "functors")
set_target_properties(lattice1functors PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

if (OPENMP_FOUND)
    target_link_libraries(lattice1functors PUBLIC OpenMP::OpenMP_CXX)
endif()

if (Threads_FOUND)
  target_link_libraries(lattice1functors PUBLIC Threads::Threads)
endif ()

if (NOT MSVC)
target_compile_options(lattice1functors
                       PUBLIC "-Wall;-Wextra;-Werror;-fwrapv")
else ()
  target_compile_options(lattice1functors PUBLIC /W3 /WX)
endif()

if (WIN32)
  # Prefix all shared libraries with 'lib'.
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

  # Prefix all static libraries with 'lib'.
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif ()

if (SOUFFLE_DOMAIN_64BIT)
    target_compile_definitions(lattice1functors
                               PUBLIC RAM_DOMAIN_SIZE=64)
endif()
