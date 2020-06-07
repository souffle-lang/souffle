#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# create deployment directory
mkdir deploy

# Ensure we have the tags before attempting to use them
# Avoids issues with shallow clones not finding tags.
git fetch --tags --unshallow || true
echo -n "Version: "
git describe --tags --abbrev=0 --always

./bootstrap
./configure --enable-host-packaging

if [ -n "$1" ];
then
  JOBS=$1
else
  JOBS=$(nproc)
fi

# In case nproc doesn't exist, set a default
if [ -z "$JOBS" ];
then
  JOBS=2
fi

export AM_MAKEFLAGS=-j$JOBS

make -j$JOBS package

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
