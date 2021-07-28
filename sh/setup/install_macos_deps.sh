#!/bin/bash

# Install MACOS dependencies on a Github Action VM

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
brew install bison libtool mcpp libffi swig
brew link --force bison
brew link --force libffi
brew install gcc@10
brew link gcc@10

echo "/usr/local/opt/bison/bin:$PATH" >> $GITHUB_PATH
echo 'PKG_CONFIG_PATH="/usr/local/opt/libffi/lib/pkgconfig/"' >> $GITHUB_ENV 

set +e
set +x
