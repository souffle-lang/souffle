#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
rm /usr/local/include/c++ || true
brew update

# Install gcc instead of gcc-x.x if a current version is preferred
brew install md5sha1sum bison libtool gcc@7 mcpp libffi
brew link bison --force
brew link libffi --force
export PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig/

# Using 'g++' will call the xcode link to clang
g++-7 --version

rm /Users/travis/Library/Logs/DiagnosticReports/* || true
