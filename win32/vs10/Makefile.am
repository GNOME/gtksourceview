GENERATED_ITEMS =					\
	gtksourceview-@GSV_API_VERSION@.vcxproj		\
	gtksourceview-@GSV_API_VERSION@.vcxproj.filters	\
	gtksourceview@GSV_API_VERSION@-install.props	\
	math.h

MSVC10_HEADERS_LISTS =	\
	gtksourceview-@GSV_API_VERSION@.vs10.headers

EXTRA_DIST =							\
	gtksourceview-@GSV_API_VERSION@.sln			\
	gtksourceview@GSV_API_VERSION@-build-defines.props	\
	gtksourceview@GSV_API_VERSION@-gen-srcs.props		\
	gtksourceview@GSV_API_VERSION@-install.propsin		\
	gtksourceview@GSV_API_VERSION@-version-paths.props	\
	gtksourceview-@GSV_API_VERSION@.vcxprojin		\
	gtksourceview-@GSV_API_VERSION@.vcxproj.filtersin	\
	gtksourceview@GSV_API_VERSION@-install.vcxproj		\
	gtksourceview@GSV_API_VERSION@-install.vcxproj.filters	\
	$(GENERATED_ITEMS)

gtksourceview@GSV_API_VERSION@-install.props: $(top_srcdir)/win32/vs10/gtksourceview@GSV_API_VERSION@-install.propsin $(MSVC10_HEADERS_LISTS)
	-$(RM) $(top_builddir)/win32/vs11/gtksourceview@GSV_API_VERSION@-install.props
	-$(RM) $(top_builddir)/win32/vs12/gtksourceview@GSV_API_VERSION@-install.props
	-$(RM) $(top_builddir)/win32/vs14/gtksourceview@GSV_API_VERSION@-install.props
	-$(RM) $(top_builddir)/win32/vs15/gtksourceview@GSV_API_VERSION@-install.props
	$(CPP) -P - <$(top_srcdir)/win32/vs10/gtksourceview@GSV_API_VERSION@-install.propsin >$@
	$(RM) $(MSVC10_HEADERS_LISTS)

math.h: $(top_srcdir)/win32/vs9/math.h
	cp $< $@

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
