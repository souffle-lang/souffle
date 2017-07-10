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
  sudo apt-get -qq update # TODO comment this out if the apt repo is up-to-date
  sudo apt-get -y install build-essential g++ automake autoconf bison flex openjdk-8-jdk lsb-release libtool libncurses5-dev

  if [ "$TEST_FORMAT" == 1 ]
  then
    sudo apt-get -y install clang-format-3.8

    sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-3.8 100
    sudo rm /usr/local/clang-3.5.0/bin/clang-format
    sudo ln -s /usr/bin/clang-format  /usr/local/clang-3.5.0/bin/clang-format
  else
    if [ "$MAKEPACKAGE" == 1 ]
    then
      sudo apt-get -y install debhelper devscripts
    elif [[ "$CC" == "clang" ]]
    then
      sudo apt-get -y install clang-3.8

      # Travis ignores our installed clang so we symlink it in place of clang's preferred version.
      sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 100
      sudo rm /usr/local/clang-3.5.0/bin/clang
      sudo ln -s /usr/bin/clang  /usr/local/clang-3.5.0/bin/clang

      sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 100
      sudo rm /usr/local/clang-3.5.0/bin/clang++
      sudo ln -s /usr/bin/clang++  /usr/local/clang-3.5.0/bin/clang++

      # Install libomp
      sudo add-apt-repository -y "deb http://ftp.us.debian.org/debian unstable main contrib non-free"
      sudo apt-get -qq update
      sudo apt-get -y --force-yes install libomp-dev
    fi
  fi
fi

############
# MAC OS X #
############

# Install requirements of MAC OS X
if [ $TRAVIS_OS_NAME == osx ]
then
  brew update
  brew install md5sha1sum bison libtool
  brew link bison --force
fi
