#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# create configure files
./bootstrap

# configure project
./configure --enable-host-packaging

# create deployment directory
mkdir deploy

###############
# Linux Build #
###############
if [ $TRAVIS_OS_NAME == linux ]
then
  # if c++ compiler is g++, build Debian package
  # otherwise; just check whether latest check-in works with
  # clang.
  if [ $CXX == g++ ]
  then
    TESTSUITEFLAGS="-j3 1466-1468" make package
    # compute md5 for package &
    # copy files to deploy directory
    for f in packaging/*.deb
    do
      pkg=`basename $f .deb`
      src="packaging/$pkg.deb"
      dest="deploy/$pkg.deb"
      cp $src $dest
      md5sum <$src >deploy/$pkg.md5
    done
    # show contents of deployment
    ls deploy/*
  else
    make
    TESTSUITEFLAGS="-j3 1466-1468" make check
  fi
fi

############
# MAC OS X #
############

if [ $TRAVIS_OS_NAME == osx ]
then
  TESTSUITEFLAGS="-j3 1466-1468" make package
  # compute md5 for package &
  # copy files to deploy directory
  for f in *.pkg
  do
    pkg=`basename $f .pkg`
    src="$pkg.pkg"
    dest="deploy/$pkg.pkg"
    cp $src $dest
    md5sum <$src >deploy/$pkg.md5
  done
  # show contents of deployment
  ls deploy/*
fi
