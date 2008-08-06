INCLUDES = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	-DTOP_SRCDIR=\""$(ABS_TOP_SRCDIR)"\"\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)			\
	$(TESTS_CFLAGS)

# testing executables
EXTRA_PROGRAMS = test-widget testregion
# testregion need gtk_text_region_ api which is normally private
# modify gtksourceview/Makefile.am to export gtk_text_* symbols
noinst_PROGRAMS = test-widget

test_widget_SOURCES = test-widget.c

test_widget_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-2.0.la \
	$(DEP_LIBS)                     \
	$(TESTS_LIBS)

testregion_SOURCES = testregion.c

testregion_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-2.0.la \
	$(DEP_LIBS)

EXTRA_DIST =				\
	apple-red.png			\
	gnome-gmush.png
