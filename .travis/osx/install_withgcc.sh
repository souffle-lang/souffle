#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
rm /usr/local/include/c++
brew update
brew install md5sha1sum bison libtool gcc
brew link bison --force
g++-7 --version

rm /Users/travis/Library/Logs/DiagnosticReports/*
