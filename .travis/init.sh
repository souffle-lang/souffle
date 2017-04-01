#!/bin/bash

if [ "$TEST_FORMAT" != 1 ]
then
  # Enable debugging and logging of shell operations
  # that are executed.
  set -e
  set -x

  echo -n "Version: "
  git describe --tags --abbrev=0 --always

  # create configure files
  ./bootstrap

  # configure project
  if [ "$MAKEPACKAGE" == 1 ]
  then
    ./configure --enable-host-packaging
  else
    ./configure
  fi

  # create deployment directory
  mkdir deploy

  if [ "$MAKEPACKAGE" != 1 ]
  then
    make -j2
  fi
fi
