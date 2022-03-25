#!/bin/sh

# Install Ubuntu dependencies on a Github Action VM

apt-get update -q
apt-get install -y -q bash-completion bison build-essential clang debhelper default-jdk-headless devscripts doxygen fakeroot flex g++ gdb git graphviz libffi-dev libncurses5-dev libsqlite3-dev libtool make mcpp pkg-config python3-dev sqlite swig zlib1g-dev cmake ruby
