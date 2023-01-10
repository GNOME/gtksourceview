FROM fedora:36

RUN dnf update -y
RUN dnf -y install \
    meson \
    vala \
    libvala-devel \
    gtk4-devel \
    ninja-build \
    pkgconf \
    clang \
    gi-docgen \
    gobject-introspection-devel \
    vulkan-headers \
    wayland-devel \
    wayland-protocols-devel \
    sysprof-devel

RUN dnf clean all

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
