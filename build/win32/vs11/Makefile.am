EXTRA_DIST =				\
	gtksourceview.sln			\
	gtksourceview-build-defines.props	\
	gtksourceview-gen-srcs.props		\
	gtksourceview-install.props		\
	gtksourceview-version-paths.props	\
	gtksourceview.vcxproj			\
	gtksourceview.vcxproj.filters		\
	gtksourceview-install.vcxproj		\
	math.h

math.h: $(top_srcdir)/build/win32/vs9/math.h
	cp $< $@

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 11
MSVC_FORMAT_VER = 12
MSVC_VER_LONG = 2012

include $(top_srcdir)/build/Makefile-newvs.am

-include $(top_srcdir)/git.mk
