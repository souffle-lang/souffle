#! /usr/bin/env bash

# Souffle - A Datalog Compiler
# Copyright (c) 2013, 2015, 2016, The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

#=========================================================================#
#                            after_failure.sh                             #
#                                                                         #
# Author: Nic H.                                                          #
# Date: 2016-Oct-10                                                       #
#                                                                         #
# Cats the contents of the first failed test case to travis's output,     #
# allowing us to debug the problem.                                       #
#=========================================================================#

pwd
ls

for FI in */; do
    echo $FI
    ls $FI
done


TEST_ROOT=`find . -type d -name "testsuite.dir" | head -1`
RELEVANT_EXTENSIONS=".out .err .log"
MAXIMUM_LINES="200"

# prints the text of its $1 arguement in blue and underlines it
pretty_print () {
    echo -n $(tput setaf 4)
    echo $1
    echo -n $1 | tr '[:print:]' '-'
    echo $(tput sgr0)
}

# print some helpful stats
pretty_print "Showing git info"
git tag
git remote -v
git describe --tags --abbrev=0 --always
git describe --all --abbrev=0 --always
git describe --tags --abbrev=0

# Find the test case that we will be displaying
CANDIDATE=`ls $TEST_ROOT | head -1`
pretty_print "Displaying contents of failed test-case no. $CANDIDATE from $TEST_ROOT"
echo

# Show the contents of its directory
pretty_print "Directory contents: "
ls "$TEST_ROOT/$CANDIDATE"
echo

# Print any of the relevant files in the directory (up to a maximum, for readability's sake)
for EXTENSION in $RELEVANT_EXTENSIONS; do
    for FI in "$TEST_ROOT/$CANDIDATE/"*$EXTENSION; do
        pretty_print "File: $(basename $FI)"
        head -$MAXIMUM_LINES $FI
        echo
    done
done

