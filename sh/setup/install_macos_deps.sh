#!/bin/bash

# Install MACOS dependencies on a Github Action VM

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
brew install bison libtool mcpp libffi swig
brew install gcc@12
brew link gcc@12

echo "/usr/local/opt/bison/bin:$PATH" >> $GITHUB_PATH
echo 'PKG_CONFIG_PATH="/usr/local/opt/libffi/lib/pkgconfig/"' >> $GITHUB_ENV 
echo 'CC=gcc-12' >> $GITHUB_ENV
echo 'CXX=g++-12' >> $GITHUB_ENV

set +e
set +x
