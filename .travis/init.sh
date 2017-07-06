#!/bin/bash

if [ "$TEST_FORMAT" != 1 ]
then
  # Enable debugging and logging of shell operations
  # that are executed.
  set -e
  set -x

  # configure project

  if [ "$MAKEPACKAGE" == 1 ]
  then
    # Ensure we have the tags before attempting to use them
    # Avoids issues with shallow clones not finding tags.
    git fetch --tags --unshallow
    echo -n "Version: "
    git describe --tags --abbrev=0 --always

    # create configure files
    ./bootstrap
    ./configure --enable-host-packaging

    # create deployment directory
    mkdir deploy
  else
    # create configure files
    ./bootstrap
    ./configure
    make -j2
  fi
fi
