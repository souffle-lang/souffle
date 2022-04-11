# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

import glob
import os
import shutil
from common import *

args = os.sys.argv
args.pop(0)

if len(args) == 0:
    raise RuntimeError("Missing INPUT_DIR")
input_dir = args.pop(0)

if len(args) == 0:
    raise RuntimeError("Missing OUTPUT_DIR")
output_dir = args.pop(0)

if len(args) == 0:
    raise RuntimeError("Missing TEST_NAME")
test_name = args.pop(0)

if len(args) > 0:
    extra_data = args.pop(0)
else:
    extra_data = None

if len(args) != 0:
    raise RuntimeError("Unexpected argument")

if os.path.exists(output_dir) and (not os.path.isdir(output_dir)):
    raise RuntimeError("Output path exists but is not a directory")

# clean output directory
if os.path.isdir(output_dir):
    for file in os.listdir(output_dir):
        path = os.path.join(output_dir, file)
        if os.path.isdir(path):
            shutil.rmtree(path) # only remove directories
        else:
            os.remove(path) # only remove files
else:
    os.makedirs(output_dir, exist_ok=True)

if extra_data == "json":
    for file in [os.path.basename(p) for p in glob.glob(os.path.join(input_dir,"*.json"))]:
        shutil.copyfile(
                os.path.join(input_dir, file),
                os.path.join(output_dir, "{}.expected".format(file)))
elif extra_data == "python" or extra_data == "java":
    shutil.copyfile(
            os.path.join(input_dir, "{}-{}.out".format(test_name, extra_data)),
            os.path.join(output_dir, "{}-{}.out.expected".format(test_name, extra_data)))
elif extra_data == "provenance":
    shutil.copyfile(
            os.path.join(input_dir, "{}.in".format(test_name)),
            os.path.join(output_dir, "{}.in".format(test_name)))

shutil.copyfile(
        os.path.join(input_dir, "{}.out".format(test_name)),
        os.path.join(output_dir, "{}.out.expected".format(test_name)))

shutil.copyfile(
        os.path.join(input_dir, "{}.err".format(test_name)),
        os.path.join(output_dir, "{}.err.expected".format(test_name)))

csvs = [os.path.basename(p) for p in glob.glob(os.path.join(input_dir,"*.csv"))]

with open(os.path.join(output_dir, "num.expected"), "w") as num_file:
    num_file.write("{}\n".format(len(csvs)))

for file in csvs:
    shutil.copyfile(
            os.path.join(input_dir, file),
            os.path.join(output_dir, "{}.expected".format(file)))

owd = os.getcwd()
try:
    os.chdir(output_dir)
    for file in glob.glob("*.expected"):
        sort_file(file)
finally:
    os.chdir(owd)

