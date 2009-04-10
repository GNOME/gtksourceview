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
EXTRA_PROGRAMS = test-widget testregion test-completion
# testregion need gtk_text_region_ api which is normally private
# modify gtksourceview/Makefile.am to export gtk_text_* symbols
noinst_PROGRAMS = test-widget test-completion

test_widget_SOURCES = test-widget.c

test_widget_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-2.0.la \
	$(DEP_LIBS)                     \
	$(TESTS_LIBS)

test_completion_SOURCES =		\
        completion-simple.c		\
        gsc-provider-test.h		\
        gsc-provider-test.c		\
        gsc-provider-devhelp.h		\
        gsc-provider-devhelp.c		\
	gsc-utils-test.c		\
	gsc-utils-test.h

test_completion_LDADD = 			\
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
