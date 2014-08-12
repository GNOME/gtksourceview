## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE = gtksourceview-3.0

# The top-level XML file (SGML in the past).
DOC_MAIN_SGML_FILE = gtksourceview-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --deprecated-guards='GTKSOURCEVIEW_DISABLE_DEPRECATED' --rebuild-types

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = $(top_srcdir)/gtksourceview

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/gtksourceview/*.h
CFILE_GLOB = $(top_srcdir)/gtksourceview/*.c

# Header files or dirs to ignore when scanning. Use base file/dir names
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h private_code
IGNORE_HFILES =					\
	config.h				\
	gtksourcebuffer-private.h		\
	gtksourcebufferinputstream.h		\
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
	gtksourcelanguage-private.h		\
	gtksourcemarkssequence.h		\
	gtksourcepixbufhelper.h			\
	gtksourceregex.h			\
	gtksourcestyle-private.h		\
	gtksourcetypes-private.h		\
	gtksourceundomanagerdefault.h		\
	gtksourceview-i18n.h			\
	gtksourceview-typebuiltins.h		\
	gtksourceview-utils.h			\
	gtktextregion.h

update-sections: $(HFILE_GLOB)
	@echo 'gtk-doc: Scanning header files'
	cp $(DOC_MODULE)-sections.txt $(DOC_MODULE)-sections.txt.old && \
	(cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" --rebuild-sections $(SCAN_OPTIONS) $(EXTRA_HFILES)) && \
	cp $(DOC_MODULE)-sections.txt $(DOC_MODULE)-sections.txt.new && \
	mv $(DOC_MODULE)-sections.txt.old $(DOC_MODULE)-sections.txt

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --xml-mode --output-format=xml

# Images to copy into HTML directory
HTML_IMAGES =

# Extra XML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =			\
	version.xml		\
	lang-tutorial.xml	\
	lang-reference.xml	\
	style-reference.xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =							\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject		\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib		\
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio		\
	--extra-dir=$(GTK_PREFIX)/share/gtk-doc/html/gtk3		\
	--extra-dir=$(GTK_PREFIX)/share/gtk-doc/html/gdk3		\
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango		\
	--extra-dir=$(GDK_PIXBUF_PREFIX)/share/gtk-doc/html/gdk-pixbuf

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
GTKDOC_CFLAGS = -I$(top_srcdir) $(DEP_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/gtksourceview/libgtksourceview-3.0.la $(DEP_LIBS)

MAINTAINERCLEANFILES =			\
	gtksourceview-3.0-overrides.txt	\
	gtksourceview-3.0.types

# include common portion ...
include $(top_srcdir)/gtk-doc.make

-include $(top_srcdir)/git.mk
