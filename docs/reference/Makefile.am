## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE = gtksourceview-@GSV_API_VERSION@

# The top-level XML file (SGML in the past).
DOC_MAIN_SGML_FILE = gtksourceview-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS =								\
	--deprecated-guards='GTKSOURCEVIEW_DISABLE_DEPRECATED'		\
	--rebuild-types							\
	--ignore-decorators='GTK_SOURCE_DEPRECATED\w*\s*\([^)]*\)|GTK_SOURCE_DEPRECATED\w*|GTK_SOURCE_AVAILABLE\w*'

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = $(top_srcdir)/gtksourceview

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/gtksourceview/*.h
CFILE_GLOB = $(top_srcdir)/gtksourceview/*.c

# Extra header to include when scanning, which are not under DOC_SOURCE_DIR
# e.g. EXTRA_HFILES = $(top_srcdir}/contrib/extra.h
EXTRA_HFILES =								\
	$(top_builddir)/gtksourceview/gtksourceversion.h		\
	$(top_builddir)/gtksourceview/gtksourceview-enumtypes.h

# Header files or dirs to ignore when scanning. Use base file/dir names
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h private_code
IGNORE_HFILES =					\
	config.h				\
	gconstructor.h				\
	gtksource.h				\
	gtksourcebuffer-private.h		\
	gtksourcebufferinputstream.h		\
	gtksourcebufferinternal.h		\
	gtksourcebufferoutputstream.h		\
	gtksourcecompletioncontainer.h		\
	gtksourcecompletionmodel.h		\
	gtksourcecompletion-private.h		\
	gtksourcecompletionwordsbuffer.h	\
	gtksourcecompletionwordslibrary.h	\
	gtksourcecompletionwordsproposal.h	\
	gtksourcecompletionwordsutils.h		\
	gtksourcecontextengine.h		\
	gtksourceencoding-private.h		\
	gtksourceengine.h			\
	gtksourcegutter-private.h		\
	gtksourcegutterrendererlines.h		\
	gtksourcegutterrenderermarks.h		\
	gtksourcegutterrenderer-private.h	\
	gtksourceiter.h				\
	gtksourcelanguage-private.h		\
	gtksourcemarkssequence.h		\
	gtksourcepixbufhelper.h			\
	gtksourceregex.h			\
	gtksourcestyle-private.h		\
	gtksourcetypes-private.h		\
	gtksourceundomanagerdefault.h		\
	gtksourceview-i18n.h			\
	gtksourceview-utils.h

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --xml-mode --output-format=xml

# Images to copy into HTML directory
HTML_IMAGES =

# Extra XML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =			\
	intro.xml		\
	lang-tutorial.xml	\
	lang-reference.xml	\
	style-reference.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
GTKDOC_CFLAGS = -I$(top_srcdir) $(DEP_CFLAGS)
GTKDOC_LIBS =									\
	$(top_builddir)/gtksourceview/libgtksourceview-@GSV_API_VERSION@.la	\
	$(DEP_LIBS)

MAINTAINERCLEANFILES =			\
	$(DOC_MODULE)-overrides.txt	\
	$(DOC_MODULE).types

# include common portion ...
include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=				\
	gtksourceview-docs.xml.in	\
	intro.xml.in			\
	lang-reference.xml.in		\
	style-reference.xml.in

DISTCLEANFILES =			\
	gtksourceview-docs.xml		\
	intro.xml			\
	lang-reference.xml		\
	style-reference.xml

-include $(top_srcdir)/git.mk
