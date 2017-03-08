EXTRA_DIST =							\
	gtksourceview-@GSV_API_VERSION@.sln			\
	gtksourceview@GSV_API_VERSION@-build-defines.props	\
	gtksourceview@GSV_API_VERSION@-gen-srcs.props		\
	gtksourceview@GSV_API_VERSION@-install.props		\
	gtksourceview@GSV_API_VERSION@-version-paths.props	\
	gtksourceview-@GSV_API_VERSION@.vcxproj			\
	gtksourceview-@GSV_API_VERSION@.vcxproj.filters		\
	gtksourceview@GSV_API_VERSION@-install.vcxproj		\
	gtksourceview@GSV_API_VERSION@-install.vcxproj.filters	\
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
