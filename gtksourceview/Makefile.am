SUBDIRS = language-specs gnu-regex

if BUILD_GNU_REGEX
REGEX_LIBS = gnu-regex/libgnuregex.la
else
REGEX_LIBS = 
endif

INCLUDES = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-DG_DISABLE_DEPRECATED 		\
	-DGTK_DISABLE_DEPRECATED 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)

BUILT_SOURCES = 			\
	gtksourceview-marshal.c 	\
	gtksourceview-marshal.h		\
	gtksourceview-typebuiltins.c	\
	gtksourceview-typebuiltins.h

# libs
lib_LTLIBRARIES = libgtksourceview-1.0.la

libgtksourceview_headers =			\
	gtksourcebuffer.h			\
	gtksourceiter.h				\
	gtksourcetag.h				\
	gtksourceview.h				\
	gtksourcelanguage.h			\
	gtksourcelanguagesmanager.h		\
	gtksourcetagstyle.h			\
	gtksourcetagtable.h			\
	gtksourcestylescheme.h			\
	gtksourcemarker.h			\
	gtksourceprintjob.h

libgtksourceview_1_0_la_SOURCES = 		\
	gtksourcebuffer.c 		\
	gtksourceiter.c			\
	gtksourcetag.c 			\
	gtksourcetag-private.h 		\
	gtksourceview.c 		\
	gtksourceundomanager.h 		\
	gtksourceundomanager.c 		\
	gtktextregion.h 		\
	gtktextregion.c 		\
	gtksourceregex.h 		\
	gtksourceregex.c 		\
	gtksourcelanguage.c 		\
	gtksourcelanguage-private.h 	\
	gtksourcelanguagesmanager.c 	\
	gtksourceview-i18n.h 		\
	gtksourceview-i18n.c		\
	gtksourcetagtable.c		\
	gtksourcestylescheme.c		\
	gtksourcemarker.c		\
	gtksourceprintjob.c		\
	$(libgtksourceview_headers)	\
	$(BUILT_SOURCES)

libgtksourceview_1_0_la_LIBADD = $(DEP_LIBS) $(REGEX_LIBS)

libgtksourceview_1_0_includedir = $(includedir)/gtksourceview-1.0/gtksourceview

libgtksourceview_1_0_include_HEADERS =		\
	$(libgtksourceview_headers)		\
	gtksourceview-typebuiltins.h

gtksourceview-marshal.h: gtksourceview-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gtksourceview_marshal > $@

gtksourceview-marshal.c: gtksourceview-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"gtksourceview-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=gtksourceview_marshal >> $@

$(srcdir)/gtksourceview-typebuiltins.c: Makefile.am $(libgtksourceview_headers)
	cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"gtksourceview-typebuiltins.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libgtksourceview_headers) > xgen-gtbec \
	&& cp xgen-gtbec $(@F) \
	&& rm -f xgen-gtbec

$(srcdir)/gtksourceview-typebuiltins.h: Makefile.am $(libgtksourceview_headers)
	cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __GTKSOURCEVIEW_TYPEBUILTINS_H__\n" \
			--fhead "#define __GTKSOURCEVIEW_TYPEBUILTINS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __GTKSOURCEVIEW_TYPEBUILTINS_H__ */\n" \
			--fprod "#include \"@filename@\"\n" \
			--eprod "#define GTK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(libgtksourceview_headers) > xgen-gth \
	&& cp xgen-gth $(@F) \
	&& rm -f xgen-gth

EXTRA_DIST = 				\
	gtksourceview-marshal.list

CLEANFILES = $(BUILT_SOURCES)
