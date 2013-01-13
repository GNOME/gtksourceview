@GNOME_CODE_COVERAGE_RULES@

INCLUDES = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	-DTOP_SRCDIR=\""$(ABS_TOP_SRCDIR)"\"\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)			\
	$(TESTS_CFLAGS)

noinst_PROGRAMS = $(TEST_PROGS) $(UNIT_TEST_PROGS)
TESTS = $(UNIT_TEST_PROGS)

TEST_PROGS = test-widget
test_widget_SOURCES = test-widget.c
test_widget_LDADD = 			\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

TEST_PROGS += test-completion
test_completion_SOURCES = test-completion.c
test_completion_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS = test-languagemanager
test_languagemanager_SOURCES =		\
	test-languagemanager.c
test_languagemanager_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-language
test_language_SOURCES =		\
	test-language.c
test_language_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-printcompositor
test_printcompositor_SOURCES =		\
	test-printcompositor.c
test_printcompositor_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-buffer
test_buffer_SOURCES = test-buffer.c
test_buffer_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-completion-model
test_completion_model_SOURCES =					\
	test-completion-model.c					\
	$(top_srcdir)/gtksourceview/gtksourcecompletionmodel.c
test_completion_model_LDADD =					\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

# Since gtksourcecompletionmodel.c is recompiled, add code coverage support.
test_completion_model_CFLAGS = $(CODE_COVERAGE_CFLAGS)
test_completion_model_LIBS = $(CODE_COVERAGE_LDFLAGS)

python_tests =			\
	test-completion.py	\
	test-widget.py

# testregion need gtk_text_region_ api which is normally private
# modify gtksourceview/Makefile.am to export gtk_text_* symbols
# TEST_PROGS += testregion
#testregion_SOURCES = testregion.c
#testregion_LDADD = 			\
#	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
#	$(DEP_LIBS)

EXTRA_DIST =				\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang	\
	$(python_tests)

# Generated files for the code coverage
CLEANFILES =							\
	test_completion_model-gtksourcecompletionmodel.gcda	\
	test_completion_model-gtksourcecompletionmodel.gcno	\
	test_completion_model-test-completion-model.gcda	\
	test_completion_model-test-completion-model.gcno

-include $(top_srcdir)/git.mk
