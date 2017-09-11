EXTRA_DIST =				\
	gtksourceview.sln			\
	gtksourceview-build-defines.props	\
	gtksourceview-gen-srcs.props		\
	gtksourceview-install.props		\
	gtksourceview-version-paths.props	\
	gtksourceview.vcxproj			\
	gtksourceview.vcxproj.filters		\
	gtksourceview-install.vcxproj		\
	gtksourceview-install.vcxproj.filters	\
	gtksourceview-introspect.vcxproj	\
	math.h

math.h: $(top_srcdir)/win32/vs9/math.h
	cp $< $@

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_BASE_VER = 10
MSVC_BASE_VER_LONG = 2010
MSVC_VER = 11
MSVC_VER_LONG = 2012

include $(top_srcdir)/win32/Makefile-newvs.am

-include $(top_srcdir)/git.mk
