if ENABLE_PROVIDERS
SUBDIRS = completion-providers
endif

@GNOME_CODE_COVERAGE_RULES@

AM_CPPFLAGS = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-I$(top_builddir)		\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(GTK_MAC_CFLAGS)		\
	$(DEP_CFLAGS)

BUILT_SOURCES = 			\
	gtksourceview-resources.c	\
	gtksourceview-typebuiltins.c	\
	gtksourceview-typebuiltins.h

libgtksourceview_headers =			\
	gtksource.h				\
	gtksourcebuffer.h			\
	gtksourcecompletion.h			\
	gtksourcecompletioncontext.h		\
	gtksourcecompletioninfo.h		\
	gtksourcecompletionitem.h		\
	gtksourcecompletionproposal.h		\
	gtksourcecompletionprovider.h		\
	gtksourceencoding.h			\
	gtksourcefile.h				\
	gtksourcefileloader.h			\
	gtksourcefilesaver.h			\
	gtksourcegutter.h			\
	gtksourcegutterrenderer.h		\
	gtksourcegutterrendererpixbuf.h		\
	gtksourcegutterrenderertext.h		\
	gtksourcelanguage.h			\
	gtksourcelanguagemanager.h		\
	gtksourcemark.h				\
	gtksourcemarkattributes.h		\
	gtksourceprintcompositor.h		\
	gtksourcesearchcontext.h		\
	gtksourcesearchsettings.h		\
	gtksourcestyle.h			\
	gtksourcestylescheme.h			\
	gtksourcestyleschememanager.h		\
	gtksourcetypes.h			\
	gtksourceundomanager.h			\
	gtksourceutils.h			\
	gtksourceview.h

libgtksourceview_private_headers =		\
	gtksourcebuffer-private.h		\
	gtksourcebufferinputstream.h		\
	gtksourcebufferoutputstream.h		\
	gtksourcecompletioncontainer.h		\
	gtksourcecompletionmodel.h		\
	gtksourcecompletion-private.h		\
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
	gtksourceview-utils.h			\
	gtktextregion.h

libgtksourceview_private_c_files =	\
	gtksourcebufferinputstream.c	\
	gtksourcebufferoutputstream.c	\
	gtksourcecompletioncontainer.c	\
	gtksourcecompletionmodel.c	\
	gtksourcecontextengine.c	\
	gtksourceengine.c		\
	gtksourcegutterrendererlines.c	\
	gtksourcegutterrenderermarks.c	\
	gtksourcelanguage-parser-1.c	\
	gtksourcelanguage-parser-2.c	\
	gtksourcemarkssequence.c	\
	gtksourcepixbufhelper.c		\
	gtksourceregex.c		\
	gtksourceundomanagerdefault.c	\
	gtksourceview-i18n.c		\
	gtksourceview-utils.c 		\
	gtktextregion.c

libgtksourceview_c_files =		\
	gtksourcebuffer.c 		\
	gtksourcecompletion.c		\
	gtksourcecompletioncontext.c	\
	gtksourcecompletioninfo.c	\
	gtksourcecompletionitem.c	\
	gtksourcecompletionproposal.c	\
	gtksourcecompletionprovider.c	\
	gtksourceencoding.c		\
	gtksourcefile.c			\
	gtksourcefileloader.c		\
	gtksourcefilesaver.c		\
	gtksourcegutter.c		\
	gtksourcegutterrenderer.c	\
	gtksourcegutterrendererpixbuf.c	\
	gtksourcegutterrenderertext.c	\
	gtksourcelanguage.c 		\
	gtksourcelanguagemanager.c 	\
	gtksourcemark.c			\
	gtksourcemarkattributes.c	\
	gtksourceprintcompositor.c	\
	gtksourcesearchcontext.c	\
	gtksourcesearchsettings.c	\
	gtksourcestyle.c		\
	gtksourcestylescheme.c		\
	gtksourcestyleschememanager.c	\
	gtksourceundomanager.c 		\
	gtksourceutils.c		\
	gtksourceview.c

# Split in a helper library, so the private functions can be used in unit tests.
noinst_LTLIBRARIES = libgtksourceview-core.la

libgtksourceview_core_la_SOURCES =		\
	$(libgtksourceview_private_c_files)	\
	$(libgtksourceview_private_headers)	\
	$(libgtksourceview_c_files)		\
	$(libgtksourceview_headers)

# do not distribute generated files
nodist_libgtksourceview_core_la_SOURCES = \
	$(BUILT_SOURCES)

libgtksourceview_core_la_CFLAGS = 	\
	$(CODE_COVERAGE_CFLAGS)

libgtksourceview_core_la_LDFLAGS =	\
	-no-undefined 			\
	$(CODE_COVERAGE_LDFLAGS)

if OS_OSX
libgtksourceview_core_la_CFLAGS += -xobjective-c
libgtksourceview_core_la_LDFLAGS += -framework Cocoa
endif

# The real library
lib_LTLIBRARIES = libgtksourceview-3.0.la

libgtksourceview_3_0_la_SOURCES =

libgtksourceview_3_0_la_LIBADD = 					\
	libgtksourceview-core.la					\
	completion-providers/words/libgtksourcecompletionwords.la	\
	$(DEP_LIBS) $(GTK_MAC_LIBS)

libgtksourceview_3_0_la_CFLAGS = 	\
	$(CODE_COVERAGE_CFLAGS)

libgtksourceview_3_0_la_LDFLAGS =		\
	-version-info $(GSV_LT_VERSION)		\
	-no-undefined 				\
	-export-symbols-regex "^gtk_source_.*"	\
	$(CODE_COVERAGE_LDFLAGS)

libgtksourceview_3_0_includedir = $(includedir)/gtksourceview-3.0/gtksourceview

libgtksourceview_3_0_include_HEADERS = $(libgtksourceview_headers)
nodist_libgtksourceview_3_0_include_HEADERS = gtksourceview-typebuiltins.h

gtksourceview-resources.c: gtksourceview.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gtksourceview.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/gtksourceview.gresource.xml

gtksourceview-typebuiltins.c: stamp-gtksourceview-typebuiltins.c
	@true
stamp-gtksourceview-typebuiltins.c: Makefile.am $(libgtksourceview_headers)
	$(AM_V_GEN) (cd $(srcdir) \
	 && $(GLIB_MKENUMS) \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"gtksourceview-typebuiltins.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libgtksourceview_headers) ) > xgen-gtbec \
	&& (cmp -s xgen-gtbec gtksourceview-typebuiltins.c || cp xgen-gtbec gtksourceview-typebuiltins.c) \
	&& rm -f xgen-gtbec && echo stamp > $(@F)

gtksourceview-typebuiltins.h: stamp-gtksourceview-typebuiltins.h
	@true

# FIXME: we have to sed the file because glib-mkenums does
#        not recognize GTK_SOURCE as the prefix but instead GTK
stamp-gtksourceview-typebuiltins.h: Makefile.am $(libgtksourceview_headers)
	$(AM_V_GEN) (cd $(srcdir) \
	 && $(GLIB_MKENUMS) \
			--fhead "#ifndef __GTKSOURCEVIEW_TYPEBUILTINS_H__\n" \
			--fhead "#define __GTKSOURCEVIEW_TYPEBUILTINS_H__\n\n" \
			--ftail "#endif /* __GTKSOURCEVIEW_TYPEBUILTINS_H__ */\n" \
			--fprod "#include <gtksourceview/@filename@>\n" \
			--eprod "G_BEGIN_DECLS\n" \
			--eprod "#define GTK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
			--eprod "G_END_DECLS\n\n" \
		$(libgtksourceview_headers) ) > xgen-gth \
	&& $(SED) -e 's/GTK_TYPE_SOURCE_/GTK_SOURCE_TYPE_/' <xgen-gth >xgen-gth.temp && mv xgen-gth.temp xgen-gth \
	&& (cmp -s xgen-gth gtksourceview-typebuiltins.h || cp xgen-gth gtksourceview-typebuiltins.h) \
	&& rm -f xgen-gth && echo stamp > $(@F)

EXTRA_DIST = 				\
	GtkSource-3.0.metadata		\
	gtksourcecompletion.ui		\
	gtksourceview.gresource.xml

CLEANFILES = 					\
	$(BUILT_SOURCES)			\
	stamp-gtksourceview-typebuiltins.h	\
	stamp-gtksourceview-typebuiltins.c

libgtksourcecompletionwords_files =	\
	./completion-providers/words/gtksourcecompletionwords.h	\
	./completion-providers/words/gtksourcecompletionwords.c

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = GtkSource-3.0.gir

INTROSPECTION_SCANNER_ENV = CC="$(CC)"

GtkSource-3.0.gir: libgtksourceview-3.0.la $(BUILT_SOURCES)
INTROSPECTION_SCANNER_ARGS =			\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	--c-include=gtksourceview/gtksource.h	\
	--warn-all
GtkSource_3_0_gir_NAMESPACE = GtkSource
GtkSource_3_0_gir_VERSION = 3.0
GtkSource_3_0_gir_CFLAGS = $(DEP_CFLAGS)
GtkSource_3_0_gir_LIBS = libgtksourceview-3.0.la
GtkSource_3_0_gir_FILES =			\
	$(libgtksourcecompletionwords_files)	\
	$(libgtksourceview_headers)		\
	$(libgtksourceview_c_files)		\
	$(BUILT_SOURCES)

GtkSource_3_0_gir_INCLUDES = Gtk-3.0 Gdk-3.0
GtkSource_3_0_gir_PACKAGES = gtk+-3.0 gdk-3.0
GtkSource_3_0_gir_EXPORT_PACKAGES = gtksourceview-3.0

girdir = $(datadir)/gir-1.0
gir_DATA = GtkSource-3.0.gir

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = GtkSource-3.0.typelib

CLEANFILES +=		\
	$(gir_DATA)	\
	$(typelib_DATA)

endif # HAVE_INTROSPECTION

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

gtksourceview-3.0.vapi: GtkSource-3.0.gir

VAPIGEN_VAPIS = gtksourceview-3.0.vapi

gtksourceview_3_0_vapi_DEPS = atk \
	gdk-3.0 \
	gdk-pixbuf-2.0 \
	gio-2.0 \
	gtk+-3.0 \
	pango \
	cairo

gtksourceview-3.0.deps:
	$(AM_V_GEN) for pkg in $(gtksourceview_3_0_vapi_DEPS); do \
		echo $$pkg >> $@; \
	done

gtksourceview_3_0_vapi_METADATADIRS = $(srcdir)
gtksourceview_3_0_vapi_FILES = GtkSource-3.0.gir

vapidir = $(datadir)/vala/vapi
vapi_DATA =			\
	gtksourceview-3.0.vapi	\
	gtksourceview-3.0.deps

CLEANFILES += $(vapi_DATA)

endif # ENABLE_VAPIGEN

-include $(top_srcdir)/git.mk
