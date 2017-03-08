GENERATED_ITEMS = 		\
	gtksourceview-@GSV_API_VERSION@.vcproj	\
	gtksourceview@GSV_API_VERSION@-install.vsprops

MSVC_HEADERS_LISTS =	\
	gtksourceview-@GSV_API_VERSION@.headers

EXTRA_DIST = 							\
	gtksourceview-@GSV_API_VERSION@.sln			\
	gtksourceview@GSV_API_VERSION@-build-defines.vsprops	\
	gtksourceview@GSV_API_VERSION@-gen-srcs.vsprops		\
	gtksourceview@GSV_API_VERSION@-install.vspropsin	\
	gtksourceview@GSV_API_VERSION@-version-paths.vsprops	\
	gtksourceview-@GSV_API_VERSION@.vcprojin		\
	gtksourceview@GSV_API_VERSION@-install.vcproj		\
	$(GENERATED_ITEMS)					\
	math.h

gtksourceview@GSV_API_VERSION@-install.vsprops: $(top_srcdir)/win32/vs9/gtksourceview@GSV_API_VERSION@-install.vspropsin $(MSVC_HEADERS_LISTS)
	$(CPP) -P - <$(top_srcdir)/win32/vs9/gtksourceview@GSV_API_VERSION@-install.vspropsin >$@
	$(RM) $(MSVC_HEADERS_LISTS)

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
