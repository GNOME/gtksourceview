EXTRA_DIST =				\
	gtksourceview.sln			\
	gtksourceview-build-defines.props	\
	gtksourceview-gen-srcs.props		\
	gtksourceview-install.props		\
	gtksourceview-version-paths.props	\
	gtksourceview.vcxproj			\
	gtksourceview.vcxproj.filters		\
	gtksourceview-install.vcxproj

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 12
MSVC_FORMAT_VER = 12
MSVC_VER_LONG = 2013

include $(top_srcdir)/build/Makefile-newvs.am

-include $(top_srcdir)/git.mk
