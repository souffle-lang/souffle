# Souffle - A Datalog Compiler
# Copyright (c) 2022 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

import os
import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser(description="Redirect standard streams")
parser.add_argument('--in', dest='in_file')
parser.add_argument('--out', dest='out_file')
parser.add_argument('--err', dest='err_file')
parser.add_argument('command', type=lambda p: pathlib.Path(p).absolute())
parser.add_argument('arguments', nargs=argparse.REMAINDER)

args = parser.parse_args()

if args.in_file:
    stdin = open(args.in_file)
else:
    stdin = None

if args.out_file:
    stdout = open(args.out_file, "w")
else:
    stdout = None

if args.err_file:
    stderr = open(args.err_file, "w")
else:
    stderr = None

status = subprocess.run([args.command] + args.arguments, stdin=stdin, stdout=stdout, stderr=stderr)

if stdout:
    stdout.close()

if stderr:
    stderr.close()

os.sys.exit(status.returncode)
