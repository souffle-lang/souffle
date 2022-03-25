# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

import os
import glob
import subprocess
from common import *

args = os.sys.argv
args.pop(0)

if len(args) == 0:
    raise RuntimeError("Missing INPUT_DIR")

input_dir = args.pop(0)
extra_data = None
binary = None

if len(args) > 0:
    arg = args.pop(0)
    if arg == "":
        extra_data = None
    elif arg == "sqlite3" or arg == "gzip":
        extra_data = arg
        if len(args) == 0:
            raise RuntimeError("Missing BINARY")
        binary = args.pop(0)
    elif arg == "json":
        extra_data = arg
        if len(args) > 0:
            args.pop(0) # unused last argument
    else:
        raise RuntimeError("Unknown processing type {}".format(arg))

if len(args) != 0:
    raise RuntimeError("Unexpected argument")

if extra_data == "gzip":
    extra_file_pattern = "*.gz.output"
elif extra_data == "sqlite3":
    extra_file_pattern = "*.sqlite.output"
elif extra_data == "json":
    extra_file_pattern = "*.json"
else:
    extra_file_pattern = None

if extra_file_pattern:
    extra_files = glob.glob(extra_file_pattern)

    for file in extra_files:
        if extra_data == "gzip":
            generated_file = os.path.basename(file).rstrip(".gz.output")
            with open(generated_file, "w") as generated:
                subprocess.run([binary, "-d", "-c", file], check=True, stdout=generated)
        elif extra_data == "sqlite3":
            generated_file = "{}.csv".format(os.path.basename(file).rstrip(".sqlite.output"))
            script = os.path.join(input_dir, "{}.script".format(file))
            with open(generated_file, "w") as generated:
                subprocess.run([binary,"-batch","-init",script,file,""], check=True, stdout=generated)
        elif extra_data == "json":
            generated_file = file

        sort_file(generated_file)
        compare_sorted_file(generated_file)

generated_csvs = glob.glob("*.csv")

with open("num.generated", "w") as num_file:
    num_file.write("{}\n".format(len(generated_csvs)))

compare_files("num.expected", "num.generated")

csvs = list(map(
        lambda file: os.path.basename(file).rstrip(".expected.sorted"),
        glob.glob("*.csv.expected.sorted")))

for csv_file in csvs:
    sort_file(csv_file)
    compare_sorted_file(csv_file)

