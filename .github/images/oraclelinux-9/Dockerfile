FROM oraclelinux:9

WORKDIR /souffle

# Install souffle build dependencies
RUN yum -y install dnf-plugins-core
RUN dnf config-manager --set-enabled ol9_codeready_builder
RUN dnf -y install \
    autoconf \
    automake \
    bash-completion \
    bison \
    clang \
    cmake \
    doxygen \
    flex \
    gcc-c++ \
    git \
    libffi-devel \
    libtool \
    make \
    ncurses-devel \
    pkg-config \
    python39 \
    rpm-build \
    sqlite-devel \
    zlib-devel

# Copy everything into souffle directory
COPY . .

ENTRYPOINT ["/bin/bash", "-l", "-c", ".github/images/entrypoint.sh"]
