#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
brew update
brew install md5sha1sum bison libtool
brew link bison --force

rm /Users/travis/Library/Logs/DiagnosticReports/*
