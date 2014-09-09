@GNOME_CODE_COVERAGE_RULES@

AM_CPPFLAGS = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	-DTOP_SRCDIR=\""$(ABS_TOP_SRCDIR)"\"\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)			\
	$(TESTS_CFLAGS)

LDADD = $(top_builddir)/gtksourceview/completion-providers/words/libgtksourcecompletionwords.la \
	$(top_builddir)/gtksourceview/libgtksourceview-core.la \
	$(DEP_LIBS) \
	$(TESTS_LIBS)

noinst_PROGRAMS = $(TEST_PROGS) $(UNIT_TEST_PROGS)

BUILT_SOURCES =				\
	test-completion-resources.c	\
	test-search-resources.c		\
	test-widget-resources.c

CLEANFILES = $(BUILT_SOURCES)

test-completion-resources.c: test-completion.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(srcdir)/test-completion.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-completion.gresource.xml

test-search-resources.c: test-search.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(srcdir)/test-search.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-search.gresource.xml

test-widget-resources.c: test-widget.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(srcdir)/test-widget.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-widget.gresource.xml

TEST_PROGS = test-completion
test_completion_SOURCES = test-completion.c
nodist_test_completion_SOURCES = test-completion-resources.c
test_completion_LDADD =						\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

TEST_PROGS += test-search
test_search_SOURCES = test-search.c
nodist_test_search_SOURCES = test-search-resources.c
test_search_LDADD =						\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

TEST_PROGS += test-search-performances
test_search_performances_SOURCES = test-search-performances.c

TEST_PROGS += test-undo-manager-performances
test_undo_manager_performances_SOURCES = test-undo-manager-performances.c

TEST_PROGS += test-widget
test_widget_SOURCES = test-widget.c
nodist_test_widget_SOURCES = test-widget-resources.c
test_widget_LDADD =						\
	$(top_builddir)/gtksourceview/libgtksourceview-3.0.la	\
	$(DEP_LIBS)						\
	$(TESTS_LIBS)

UNIT_TEST_PROGS = test-buffer
test_buffer_SOURCES = test-buffer.c

UNIT_TEST_PROGS += test-buffer-input-stream
test_buffer_input_stream_SOURCES = test-buffer-input-stream.c

UNIT_TEST_PROGS += test-buffer-output-stream
test_buffer_output_stream_SOURCES = test-buffer-output-stream.c

UNIT_TEST_PROGS += test-completion-model
test_completion_model_SOURCES =	test-completion-model.c

UNIT_TEST_PROGS += test-completion-words
test_completion_words_SOURCES = test-completion-words.c

UNIT_TEST_PROGS += test-encoding
test_encoding_SOURCES = test-encoding.c

UNIT_TEST_PROGS += test-file-loader
test_file_loader_SOURCES = test-file-loader.c

UNIT_TEST_PROGS += test-file-saver
test_file_saver_SOURCES = test-file-saver.c

UNIT_TEST_PROGS += test-language
test_language_SOURCES = test-language.c

UNIT_TEST_PROGS += test-languagemanager
test_languagemanager_SOURCES = test-languagemanager.c

UNIT_TEST_PROGS += test-mark
test_mark_SOURCES = test-mark.c

UNIT_TEST_PROGS += test-printcompositor
test_printcompositor_SOURCES = test-printcompositor.c

UNIT_TEST_PROGS += test-regex
test_regex_SOURCES = test-regex.c

UNIT_TEST_PROGS += test-region
test_region_SOURCES = test-region.c

UNIT_TEST_PROGS += test-search-context
test_search_context_SOURCES = test-search-context.c

UNIT_TEST_PROGS += test-styleschememanager
test_styleschememanager_SOURCES = test-styleschememanager.c

UNIT_TEST_PROGS += test-undo-manager
test_undo_manager_SOURCES = test-undo-manager.c

UNIT_TEST_PROGS += test-utils
test_utils_SOURCES = test-utils.c

python_tests =			\
	test-completion.py	\
	test-widget.py

TESTS_ENVIRONMENT = G_TEST_SRCDIR="$(abs_srcdir)"
TESTS = $(UNIT_TEST_PROGS)

EXTRA_DIST =				\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang	\
	setup-file-saver.sh		\
	styles/classic.xml		\
	syntax-highlighting		\
	test-completion.gresource.xml	\
	test-completion.ui		\
	test-search.gresource.xml	\
	test-search.ui			\
	test-widget.gresource.xml	\
	test-widget.ui			\
	$(python_tests)

if INSTALLED_TESTS
insttestdir = ${libexecdir}/installed-tests/$(PACKAGE)
insttest_PROGRAMS = $(UNIT_TEST_PROGS)

instteslangdir = ${libexecdir}/installed-tests/$(PACKAGE)/language-specs
instteslang_DATA = 			\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang

insttesstyledir = ${libexecdir}/installed-tests/$(PACKAGE)/styles
insttesstyle_DATA = 			\
	styles/classic.xml

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(UNIT_TEST_PROGS:=.test)

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
