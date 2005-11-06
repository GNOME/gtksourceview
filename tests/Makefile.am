INCLUDES = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(DEPRECATED_FLAGS)		\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)                  \
	$(TESTS_CFLAGS)

# testing executables
noinst_PROGRAMS = test-fold test-widget testregion

test_fold_SOURCES = test-fold.c

test_fold_LDADD = \
	$(top_builddir)/gtksourceview/libgtksourceview-1.0.la \
	$(DEP_LIBS)

test_widget_SOURCES = test-widget.c

test_widget_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-1.0.la \
	$(DEP_LIBS)                     \
	$(TESTS_LIBS)

testregion_SOURCES = testregion.c

testregion_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-1.0.la \
	$(DEP_LIBS)
