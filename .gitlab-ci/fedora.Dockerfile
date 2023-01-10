FROM fedora:37

RUN dnf update -y
RUN dnf -y install --setopt=install_weak_deps=False \
    clang \
    git \
    gi-docgen \
    meson \
    ninja-build \
    pkgconf \
    vala \
    expat-devel \
    gtk4-devel \
    gobject-introspection-devel \
    libjpeg-turbo-devel \
    libpng-devel \
    libvala-devel \
    sysprof-devel \
    vulkan-headers \
    wayland-devel \
    wayland-protocols-devel

RUN dnf clean all

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
