INCLUDES = \
-DG_LOG_DOMAIN=\"GtkSourceView\" \
-I$(top_srcdir) \
-I$(top_srcdir)/src \
@DEP_CFLAGS@

# testing executable
noinst_PROGRAMS = test-widget

# libs
lib_LTLIBRARIES = libgtksourceview.la

libgtksourceview_la_SOURCES = \
regex.c \
regex.h \
gtksourcebuffer.c \
gtksourcebuffer.h \
gtksourcetag.c \
gtksourcetag.h \
gtksourceview.c \
gtksourceview.h

libgtksourceview_la_LIBADD = @DEP_LIBS@

libgtksourceview_includedir=$(includedir)/gtksourceview

libgtksourceview_include_HEADERS = \
gtksourcebuffer.h \
gtksourcetag.h \
gtksourceview.h

# The testing binary magic
test_widget_SOURCES = test-widget.c

test_widget_LDADD = \
libgtksourceview.la \
@DEP_LIBS@
