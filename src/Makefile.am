SUBDIRS = language-specs

INCLUDES = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-DG_DISABLE_DEPRECATED 		\
	-DGTK_DISABLE_DEPRECATED 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED	\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)

BUILT_SOURCES = 			\
	gtksourceview-marshal.c 	\
	gtksourceview-marshal.h

# libs
lib_LTLIBRARIES = libgtksourceview-1.0.la

libgtksourceview_1_0_la_SOURCES = 		\
	gtksourcebuffer.c 		\
	gtksourcebuffer.h 		\
	gtksourcetag.c 			\
	gtksourcetag.h 			\
	gtksourcetag-private.h 		\
	gtksourceview.c 		\
	gtksourceview.h 		\
	gtksourceundomanager.h 		\
	gtksourceundomanager.c 		\
	gtktextregion.h 		\
	gtktextregion.c 		\
	gtksourceregex.h 		\
	gtksourceregex.c 		\
	gtksourcelanguage.c 		\
	gtksourcelanguage.h 		\
	gtksourcelanguage-private.h 	\
	gtksourcelanguagesmanager.h 	\
	gtksourcelanguagesmanager.c 	\
	gtksourcetagstyle.h		\
	gtksourceview-i18n.h 		\
	gtksourceview-i18n.c		\
	gtksourcetagtable.h		\
	gtksourcetagtable.c		\
	gtksourcestylescheme.h		\
	gtksourcestylescheme.c		\
	gtksourcemarker.h		\
	gtksourcemarker.c		\
	$(BUILT_SOURCES)

libgtksourceview_1_0_la_LIBADD = $(DEP_LIBS)

libgtksourceview_1_0_includedir = $(includedir)/gtksourceview-1.0

libgtksourceview_1_0_include_HEADERS = 	\
	gtksourcebuffer.h 		\
	gtksourcetag.h 			\
	gtksourceview.h 		\
	gtksourcelanguage.h 		\
	gtksourcelanguagesmanager.h 	\
	gtksourcetagstyle.h 		\
	gtksourcetagtable.h		\
	gtksourcestylescheme.h		\
	gtksourcemarker.h

gtksourceview-marshal.h: gtksourceview-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=gtksourceview_marshal > $@

gtksourceview-marshal.c: gtksourceview-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"gtksourceview-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=gtksourceview_marshal >> $@

EXTRA_DIST = 				\
	gtksourceview-marshal.list

CLEANFILES = $(BUILT_SOURCES) 
