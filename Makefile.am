if BUILD_TESTS
MAYBE_TESTS = tests
else
MAYBE_TESTS =
endif

SUBDIRS = gtksourceview po $(MAYBE_TESTS) docs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtksourceview-2.0.pc

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	gtksourceview-2.0.pc.in	\
	gtksourceview-zip.in \
	COPYING.lib \
	HACKING \
	MAINTAINERS \
	TODO

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
