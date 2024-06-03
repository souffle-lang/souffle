#!/bin/bash

# Install MACOS dependencies on a Github Action VM

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
brew install libtool mcpp swig bison libffi
#brew install gcc@13
#brew link gcc@13

echo "/usr/local/opt/bison/bin:$PATH" >> $GITHUB_PATH
echo 'PKG_CONFIG_PATH="/usr/local/opt/libffi/lib/pkgconfig/"' >> $GITHUB_ENV
#echo 'CC=gcc-13' >> $GITHUB_ENV
#echo 'CXX=g++-13' >> $GITHUB_ENV
echo "SDKROOT=$(xcrun --sdk macosx --show-sdk-path)" >> $GITHUB_ENV

set +e
set +x
