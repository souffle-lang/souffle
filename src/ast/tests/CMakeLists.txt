# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

include(SouffleTests)

souffle_add_binary_test(ast_print_test ast)
souffle_add_binary_test(ast_program_test ast)
souffle_add_binary_test(ast_transformers_test ast)
souffle_add_binary_test(ast_utils_test ast)
souffle_add_binary_test(type_system_test ast)
souffle_add_binary_test(constraints_test ast)
souffle_add_binary_test(ast_recursive_clauses_test ast)
