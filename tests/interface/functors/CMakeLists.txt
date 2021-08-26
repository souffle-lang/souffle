# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

add_library(functors SHARED functors.cpp)
target_include_directories(functors PRIVATE "${CMAKE_SOURCE_DIR}/src/include")

target_compile_features(functors
                        PUBLIC cxx_std_17)
set_target_properties(functors PROPERTIES CXX_EXTENSIONS OFF)

target_compile_options(functors
                       PUBLIC "-Wall;-Wextra;-Werror;-fwrapv")

if (SOUFFLE_DOMAIN_64BIT)
    target_compile_definitions(functors
                               PUBLIC RAM_DOMAIN_SIZE=64)
endif()
