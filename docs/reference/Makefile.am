GTKDOC_CFLAGS = -I$(top_srcdir) $(DEP_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/gtksourceview/libgtksourceview-3.0.la $(DEP_LIBS)

## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.7

# The name of the module.
DOC_MODULE = gtksourceview-3.0

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gtksourceview-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --deprecated-guards='GTKSOURCEVIEW_DISABLE_DEPRECATED'

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = ../..

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/gtksourceview/*.h
CFILE_GLOB = $(top_srcdir)/gtksourceview/*.c

# Header files or dirs to ignore when scanning. Use base file/dir names
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h private_code
IGNORE_HFILES =					\
	config.h				\
	gtksourceview-marshal.h			\
	gtksourceview-typebuiltins.h		\
	gtksourcecompletionmodel.h		\
	gtksourcecompletion-private.h		\
	gtksourcecompletionutils.h		\
	gtksourcecontextengine.h		\
	gtksourceengine.h			\
	gtksourcegutter-private.h		\
	gtksourcelanguage-private.h		\
	gtksourceregex.h			\
	gtksourcestyle-private.h		\
	gtksourceundomanagerdefault.h		\
	gtksourceview-i18n.h			\
	gtksourceview-utils.h			\
	gtktextregion.h				\
	gtksourcepixbufhelper.h			\
	gtksourcegutterrendererlines.h		\
	gtksourcegutterrenderermarks.h		\
	gtksourcegutterrenderer-private.h	\
	gtksourcecompletionwordsbuffer.h	\
	gtksourcecompletionwordslibrary.h	\
	gtksourcecompletionwordsproposal.h	\
	gtksourcecompletionwordsutils.h

update-sections: $(HFILE_GLOB)
	@echo 'gtk-doc: Scanning header files'
	cp $(DOC_MODULE)-sections.txt $(DOC_MODULE)-sections.txt.old && \
	(cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" --rebuild-sections $(SCAN_OPTIONS) $(EXTRA_HFILES)) && \
	cp $(DOC_MODULE)-sections.txt $(DOC_MODULE)-sections.txt.new && \
	mv $(DOC_MODULE)-sections.txt.old $(DOC_MODULE)-sections.txt

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml --ignore-files=trio

# Images to copy into HTML directory
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =			\
	version.xml		\
	lang-tutorial.sgml	\
	lang-reference.sgml	\
	style-reference.sgml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# include common portion ...
include $(top_srcdir)/gtk-doc.make

MAINTAINERCLEANFILES = gtksourceview-3.0-overrides.txt

-include $(top_srcdir)/git.mk
