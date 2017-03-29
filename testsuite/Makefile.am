@CODE_COVERAGE_RULES@

@VALGRIND_CHECK_RULES@

# https://github.com/pwithnall/walbottle
# https://github.com/dtrebbien/GNOME.supp
VALGRIND_SUPPRESSIONS_FILES =						\
	$(top_srcdir)/../walbottle/libwalbottle/tests/walbottle.supp	\
	$(top_srcdir)/../GNOME.supp/build/base.supp			\
	$(top_srcdir)/../GNOME.supp/build/pango.supp			\
	$(top_srcdir)/../GNOME.supp/build/gdk.supp			\
	$(top_srcdir)/../GNOME.supp/build/gtk3.supp

AM_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	-DTOP_SRCDIR=\""$(ABS_TOP_SRCDIR)"\"\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)			\
	$(TESTS_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

LDADD = $(top_builddir)/gtksourceview/completion-providers/words/libgtksourcecompletionwords.la \
	$(top_builddir)/gtksourceview/libgtksourceview-core.la \
	-lm \
	$(DEP_LIBS) \
	$(TESTS_LIBS)

UNIT_TEST_PROGS =

UNIT_TEST_PROGS += test-buffer
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

UNIT_TEST_PROGS += test-iter
test_iter_SOURCES = test-iter.c

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

UNIT_TEST_PROGS += test-space-drawer
test_space_drawer_SOURCES = test-space-drawer.c

UNIT_TEST_PROGS += test-stylescheme
test_stylescheme_SOURCES = test-stylescheme.c

UNIT_TEST_PROGS += test-styleschememanager
test_styleschememanager_SOURCES = test-styleschememanager.c

UNIT_TEST_PROGS += test-undo-manager
test_undo_manager_SOURCES = test-undo-manager.c

UNIT_TEST_PROGS += test-utils
test_utils_SOURCES = test-utils.c

UNIT_TEST_PROGS += test-view
test_view_SOURCES = test-view.c

TESTS_ENVIRONMENT = G_TEST_SRCDIR="$(abs_srcdir)"
TESTS = $(UNIT_TEST_PROGS)
noinst_PROGRAMS = $(UNIT_TEST_PROGS)

EXTRA_DIST =				\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang	\
	setup-file-saver.sh		\
	styles/classic.xml		\
	styles/test.xml

if INSTALLED_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)-@GSV_API_VERSION@
insttest_PROGRAMS = $(UNIT_TEST_PROGS)

insttestlangdir = $(libexecdir)/installed-tests/$(PACKAGE)-@GSV_API_VERSION@/language-specs
insttestlang_DATA = 			\
	language-specs/test-empty.lang	\
	language-specs/test-full.lang

instteststyledir = $(libexecdir)/installed-tests/$(PACKAGE)-@GSV_API_VERSION@/styles
instteststyle_DATA = 			\
	styles/classic.xml		\
	styles/test.xml

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Description=GtkSourceView test' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$<' >> $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Output=TAP' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(UNIT_TEST_PROGS:=.test)

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)-@GSV_API_VERSION@
testmeta_DATA = $(test_files)

endif # INSTALLED_TESTS

-include $(top_srcdir)/git.mk
