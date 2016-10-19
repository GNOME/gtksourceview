@CODE_COVERAGE_RULES@

AM_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_srcdir) -I$(srcdir) 	\
	-DTOP_SRCDIR=\""$(ABS_TOP_SRCDIR)"\"\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(DEP_CFLAGS)			\
	$(TESTS_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

LDADD = $(top_builddir)/gtksourceview/libgtksourceview-@GSV_API_VERSION@.la \
	-lm \
	$(DEP_LIBS) \
	$(TESTS_LIBS)

BUILT_SOURCES =				\
	test-completion-resources.c	\
	test-search-resources.c		\
	test-widget-resources.c

CLEANFILES = $(BUILT_SOURCES)

test-completion-resources.c: test-completion.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test-completion.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-completion.gresource.xml

test-search-resources.c: test-search.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test-search.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-search.gresource.xml

test-widget-resources.c: test-widget.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/test-widget.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-widget.gresource.xml

TEST_PROGS =

TEST_PROGS += test-completion
test_completion_SOURCES = test-completion.c
nodist_test_completion_SOURCES = test-completion-resources.c

TEST_PROGS += test-search
test_search_SOURCES = test-search.c
nodist_test_search_SOURCES = test-search-resources.c

TEST_PROGS += test-search-performances
test_search_performances_SOURCES = test-search-performances.c

TEST_PROGS += test-space-drawing
test_space_drawing_SOURCES = test-space-drawing.c

TEST_PROGS += test-undo-manager-performances
test_undo_manager_performances_SOURCES = test-undo-manager-performances.c

TEST_PROGS += test-widget
test_widget_SOURCES = test-widget.c
nodist_test_widget_SOURCES = test-widget-resources.c

noinst_PROGRAMS = $(TEST_PROGS)

python_tests =			\
	test-completion.py	\
	test-widget.py

EXTRA_DIST =				\
	syntax-highlighting		\
	test-completion.gresource.xml	\
	test-completion.ui		\
	test-search.gresource.xml	\
	test-search.ui			\
	test-widget.gresource.xml	\
	test-widget.ui			\
	$(python_tests)

-include $(top_srcdir)/git.mk
