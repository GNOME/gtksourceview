GENERATED_ITEMS = 		\
	gtksourceview.vcproj	\
	gtksourceview-install.vsprops

MSVC_HEADERS_LISTS =	\
	gtksourceview.headers

EXTRA_DIST = 					\
	gtksourceview.sln			\
	gtksourceview-build-defines.vsprops	\
	gtksourceview-gen-srcs.vsprops		\
	gtksourceview-install.vspropsin		\
	gtksourceview-version-paths.vsprops	\
	gtksourceview.vcprojin			\
	gtksourceview-install.vcproj		\
	gtksourceview-introspect.vcproj		\
	$(GENERATED_ITEMS)			\
	math.h

gtksourceview-install.vsprops: $(top_srcdir)/win32/vs9/gtksourceview-install.vspropsin $(MSVC_HEADERS_LISTS)
	$(CPP) -P - <$(top_srcdir)/win32/vs9/gtksourceview-install.vspropsin >$@
	$(RM) $(MSVC_HEADERS_LISTS)

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
