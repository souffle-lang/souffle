#!/bin/bash

# Install MACOS dependencies on a Github Action VM

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
brew install libtool mcpp swig bison libffi gcc@15

echo "$(brew --prefix bison)/bin" >> "$GITHUB_PATH"
echo "PKG_CONFIG_PATH=$(brew --prefix libffi)/lib/pkgconfig" >> "$GITHUB_ENV"
echo "CC=$(brew --prefix gcc@15)/bin/gcc-15" >> "$GITHUB_ENV"
echo "CXX=$(brew --prefix gcc@15)/bin/g++-15" >> "$GITHUB_ENV"
echo "SDKROOT=$(xcrun --sdk macosx --show-sdk-path)" >> "$GITHUB_ENV"

set +e
set +x
