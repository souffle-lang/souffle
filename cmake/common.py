# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

import difflib
import re
import os

## sort file `file_name` to file `file_name.sorted`
def sort_file(file_name):
    if not os.path.isfile(file_name):
        raise RuntimeError("Missing file '{}'".format(file_name))

    output_file = "{}.sorted".format(file_name)

    with open(file_name) as f:
        digits = re.compile(r'^\d+$')
        def prepare_row(row):
            return list(map(
                # natural sort, transfor integers to string representation with leading zeroes
                lambda field: (("{:0>20d}".format(int(field))) if digits.match(field) else field),
                row.split("\t")))

        rows = sorted([row.rstrip() for row in f], key=lambda row: prepare_row(row))

    with open(output_file, "w") as f:
        f.write("\n".join(rows))

    return None

## Compare two given files. Exit with error status if files do not match.
def compare_files(expected_file, actual_file):
    if not os.path.isfile(expected_file):
        raise RuntimeError("Missing file '{}'".format(expected_file))
    if not os.path.isfile(actual_file):
        raise RuntimeError("Missing file '{}'".format(actual_file))

    with open(expected_file) as f:
        expected_lines = [line.rstrip() for line in f]

    with open(actual_file) as f:
        actual_lines = [line.rstrip() for line in f]

    if actual_lines != expected_lines:
        os.sys.stdout.writelines(difflib.unified_diff(open(expected_file).readlines(), open(actual_file).readlines(), fromfile=expected_file, tofile=actual_file))
        os.sys.exit("Found output difference, expected file:'{}', actual file:'{}".format(expected_file, actual_file))

    return True

## Compare `file_name` with `file_name.expected`. Exit with error status if files do not match.
def compare_file(file_name):
    actual_file = file_name
    expected_file = "{}.expected".format(file_name)

    return compare_files(expected_file, actual_file)

## Compare `file_name.sorted` with `file_name.expected.sorted`. Exit with error status if files do not match.
def compare_sorted_file(file_name):
    actual_file = "{}.sorted".format(file_name)
    expected_file = "{}.expected.sorted".format(file_name)

    return compare_files(expected_file, actual_file)

