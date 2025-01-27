FROM fedora:41

WORKDIR /souffle

# Install souffle build dependencies
RUN dnf -y install \
    autoconf \
    automake \
    bash-completion \
    bison \
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
