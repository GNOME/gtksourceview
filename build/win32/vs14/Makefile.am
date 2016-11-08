EXTRA_DIST =							\
	gtksourceview-@GSV_API_VERSION@.sln			\
	gtksourceview@GSV_API_VERSION@-build-defines.props	\
	gtksourceview@GSV_API_VERSION@-gen-srcs.props		\
	gtksourceview@GSV_API_VERSION@-install.props		\
	gtksourceview@GSV_API_VERSION@-version-paths.props	\
	gtksourceview-@GSV_API_VERSION@.vcxproj			\
	gtksourceview-@GSV_API_VERSION@.vcxproj.filters		\
	gtksourceview@GSV_API_VERSION@-install.vcxproj

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 14
MSVC_FORMAT_VER = 12
MSVC_VER_LONG = 14

include $(top_srcdir)/build/Makefile-newvs.am

-include $(top_srcdir)/git.mk
