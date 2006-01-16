GTKDOC_CFLAGS = -I$(top_srcdir) $(DEP_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/gtksourceview/libgtksourceview-1.0.la

## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.7

# The name of the module.
DOC_MODULE = gtksourceview

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gtksourceview-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS =

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = ../..

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/gtksourceview/*.h
CFILE_GLOB = $(top_srcdir)/gtksourceview/*.c

# Headers to ignore
IGNORE_HFILES =					\
	config.h				\
	gtksourcelanguage-private.h		\
	gtksourcetag-private.h			\
	gtksourceundomanager.h			\
	gtksourceview-i18n.h			\
	gtksourceview-marshal.h			\
	gtksourceview-typebuiltins.h		\
	gtktextregion.h				\
	gtksourcecontextengine.h		\
	regex.h					\
	regex_internal.h

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml --ignore-files=trio

# Images to copy into HTML directory
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =			\
	version.xml		\
	lang-tutorial.sgml	\
	lang-reference.sgml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

# include common portion ...
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST +=
