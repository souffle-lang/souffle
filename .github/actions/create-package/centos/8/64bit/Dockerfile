FROM centos:8

# Create a souffle directory
WORKDIR /souffle

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
    redhat-lsb-core \
    rpm-build \
    sqlite-devel \
    zlib-devel

# Install dependencies for packagecloud CLI as well as package_cloud CLI
RUN gpg --keyserver keyserver.ubuntu.com --recv-keys 409B6B1796C275462A1703113804BB82D39DC0E3 7D2BAF1CF37B13E2069D6956105BD0E739499BDB && \
	curl -sSL https://get.rvm.io | bash -s stable
RUN /bin/bash -l -c ". /etc/profile.d/rvm.sh && rvm install 2.7.4 && gem install package_cloud"

# Copy everything into souffle directory
COPY . .

ENV DOMAIN_SIZE "64bit"
ENV PKG_EXTENSION ".rpm"
ENV PKG_CLOUD_OS_NAME "centos/8"

ENTRYPOINT ["/bin/bash", "-l", "-c", ".github/actions/create-package/entrypoint.sh"]
