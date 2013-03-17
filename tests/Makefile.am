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

UNIT_TEST_PROGS += test-styleschememanager
test_styleschememanager_SOURCES =		\
	test-styleschememanager.c
test_styleschememanager_LDADD = 		\
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

UNIT_TEST_PROGS += test-mark
test_mark_SOURCES = test-mark.c
test_mark_LDADD = 		\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-regex
test_regex_SOURCES = test-regex.c
test_regex_LDADD = 						\
	$(top_builddir)/gtksourceview/libgtksourceview-private.la	\
	$(DEP_LIBS)			\
	$(GTK_MAC_LIBS)		\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-completion-model
test_completion_model_SOURCES =	test-completion-model.c
test_completion_model_LDADD =					\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la \
	$(top_builddir)/gtksourceview/libgtksourceview-private.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-region
test_region_SOURCES = test-region.c
test_region_LDADD = 						\
	$(top_builddir)/gtksourceview/libgtksourceview-private.la	\
	$(DEP_LIBS)			\
	$(TESTS_LIBS)

UNIT_TEST_PROGS += test-undo-manager
test_undo_manager_SOURCES = test-undo-manager.c
test_undo_manager_LDADD =					\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

python_tests =			\
	test-completion.py	\
	test-widget.py

EXTRA_DIST =				\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang	\
	styles/classic.xml		\
	$(python_tests)

-include $(top_srcdir)/git.mk
