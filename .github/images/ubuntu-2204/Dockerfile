FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
WORKDIR /souffle

# Install souffle build dependencies
RUN apt-get update && \
apt-get install -y -q bash-completion bison build-essential clang debhelper default-jdk-headless devscripts doxygen fakeroot flex g++ gdb git graphviz libffi-dev libncurses5-dev libsqlite3-dev libtool make mcpp pkg-config python3-dev sqlite swig zlib1g-dev cmake ruby

# Copy everything into souffle directory
COPY CMakeLists.txt ./CMakeLists.txt
COPY cmake ./cmake
COPY debian ./debian
COPY man ./man
COPY sh ./sh
COPY src ./src
COPY tests ./tests
COPY utilities ./utilities
COPY .github/images/entrypoint.sh ./entrypoint.sh
COPY .git ./.git

ENTRYPOINT ["./entrypoint.sh"]
