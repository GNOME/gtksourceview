INCLUDES = \
	-DG_LOG_DOMAIN=\"GtkSourceView\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@DEP_CFLAGS@

# testing executable
noinst_PROGRAMS = test-widget

# libs
lib_LTLIBRARIES = libgtksourceview.la

BUILT_SOURCES = \
	gtksourceview-marshal.c \
	gtksourceview-marshal.h

libgtksourceview_la_SOURCES = \
	regex.c \
	regex.h \
	gtksourcebuffer.c \
	gtksourcebuffer.h \
	gtktextsearch.c \
	gtktextsearch.h \
	gtksourcetag.c \
	gtksourcetag.h \
	gtksourceview.c \
	gtksourceview.h \
	gtkundomanager.h \
	gtkundomanager.c \
	gtksourceview-marshal.h \
	gtksourceview-marshal.c

libgtksourceview_la_LIBADD = @DEP_LIBS@

libgtksourceview_includedir=$(includedir)/gtksourceview

libgtksourceview_include_HEADERS = \
	gtksourcebuffer.h \
	gtksourcetag.h \
	gtksourceview.h

gtksourceview-marshal.h: gtksourceview-marshal.list
	@GLIB_GENMARSHAL@ \
		--prefix=gtksourceview_marshal $(srcdir)/gtksourceview-marshal.list --header > xgen-gmc \
	&& cp xgen-gmc $(@F) \
	&& rm -f xgen-gmc

gtksourceview-marshal.c: gtksourceview-marshal.h
	echo "#include \"gtksourceview-marshal.h\"" > xgen-gmc \
	&& @GLIB_GENMARSHAL@ \
		--prefix=gtksourceview_marshal $(srcdir)/gtksourceview-marshal.list --body >> xgen-gmc \
	&& cp xgen-gmc $(@F) \
	&& rm -f xgen-gmc

# The testing binary magic
test_widget_SOURCES = test-widget.c

test_widget_LDADD = \
	libgtksourceview.la \
	@DEP_LIBS@
