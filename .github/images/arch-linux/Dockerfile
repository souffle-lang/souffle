FROM archlinux:latest

RUN pacman -Syu --noconfirm

RUN pacman -Sy --noconfirm \
    python \
    git \
    base-devel \
    cmake \
    openmp \
    bison \
    flex \
    ncurses \
    zlib \
    mcpp \
    gcc \
    openssh \
    sudo


# makepkg does not allow build from root
RUN useradd --system --create-home builduser 
RUN echo "builduser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/builduser
USER builduser

WORKDIR /home/builduser
RUN git clone https://aur.archlinux.org/yay.git \
  && cd yay \
  && makepkg -sri --needed --noconfirm

COPY PKGBUILD.in /home/builduser/PKGBUILD.in
COPY entrypoint.sh /home/builduser/entrypoint.sh

ENTRYPOINT ["./entrypoint.sh"]
