#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

#########
# Linux #
#########

# Install requirements of Linux
if [ $TRAVIS_OS_NAME == linux ]
then
    # sudo apt-get -qq update # commenting out because i assume the apt-repository on travis is up to date
    sudo apt-get -y install debhelper devscripts build-essential g++ automake autoconf bison flex gradle openjdk-7-jdk libboost-all-dev lsb-release libtool
fi

############
# MAC OS X #
############

# Install requirements of MAC OS X
if [ $TRAVIS_OS_NAME == osx ]
then
   brew update
   brew install md5sha1sum bison libtool gradle
   brew link bison --force
fi
