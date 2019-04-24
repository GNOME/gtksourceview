SUBDIRS = po data gtksourceview msvc tests testsuite docs

@CODE_COVERAGE_RULES@

AM_DISTCHECK_CONFIGURE_FLAGS =		\
	--enable-gtk-doc		\
	--enable-introspection		\
	--enable-vala			\
	--enable-glade-catalog

pkgconfig_DATA = gtksourceview-@GSV_API_VERSION@.pc

EXTRA_DIST = \
	gtksourceview.doap \
	gtksourceview.pc.in \
	HACKING

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	gtk-doc.make \
	INSTALL

-include $(top_srcdir)/git.mk
