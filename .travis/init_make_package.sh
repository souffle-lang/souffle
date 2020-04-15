#!/bin/bash


# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# configure project

# Ensure we have the tags before attempting to use them
# Avoids issues with shallow clones not finding tags.
git fetch --tags --unshallow || true
echo -n "Version: "
git describe --tags --abbrev=0 --always

# create configure files
./bootstrap
./configure --enable-host-packaging

set +e
set +x
