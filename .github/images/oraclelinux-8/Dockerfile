FROM oraclelinux:8

WORKDIR /souffle

# https://pagure.io/epel/issue/89
RUN yum -y install dnf-plugins-core
RUN dnf config-manager --set-enabled ol8_codeready_builder
# Install souffle build dependencies
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
    mcpp \
    ncurses-devel \
    pkg-config \
    python39 \
    rpm-build \
    sqlite-devel \
    zlib-devel

# Copy everything into souffle directory
COPY . .

ENTRYPOINT ["/bin/bash", "-l", "-c", ".github/images/entrypoint.sh"]
