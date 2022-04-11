# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

import os
from common import *

args = os.sys.argv
args.pop(0)

if len(args) == 0:
    raise RuntimeError("Missing TEST_NAME")

test_name = args.pop(0)

if len(args) > 0:
    extra_data = args.pop(0)
else:
    extra_data = None

if len(args) != 0:
    raise RuntimeError("Unexpected argument")

sort_file("{}.out".format(test_name))
compare_sorted_file("{}.out".format(test_name))

compare_file("{}.err".format(test_name))

if extra_data == "python" or extra_data == "java":
    file_name = "{}-{}.out".format(test_name, extra_data)
    sort_file(file_name)
    compare_sorted_file(file_name)

