SUBDIRS = completion-providers

@CODE_COVERAGE_RULES@

AM_CPPFLAGS = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-DGTK_SOURCE_COMPILATION	\
	-I$(top_builddir)		\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(GTK_MAC_CFLAGS)		\
	$(DEP_CFLAGS)

BUILT_SOURCES = 			\
	gtksourceview-enumtypes.c	\
	gtksourceview-enumtypes.h	\
	gtksourceview-resources.c

libgtksourceview_public_headers =		\
	gtksource.h				\
	gtksourceautocleanups.h			\
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
	gtksourcemap.h				\
	gtksourcemark.h				\
	gtksourcemarkattributes.h		\
	gtksourceprintcompositor.h		\
	gtksourceregion.h			\
	gtksourcesearchcontext.h		\
	gtksourcesearchsettings.h		\
	gtksourcespacedrawer.h			\
	gtksourcestyle.h			\
	gtksourcestylescheme.h			\
	gtksourcestyleschemechooser.h		\
	gtksourcestyleschemechooserbutton.h	\
	gtksourcestyleschemechooserwidget.h	\
	gtksourcestyleschememanager.h		\
	gtksourcetag.h				\
	gtksourcetypes.h			\
	gtksourceundomanager.h			\
	gtksourceutils.h			\
	gtksourceview.h				\
	gtksourceview-typebuiltins.h

libgtksourceview_public_c_files =		\
	gtksourcebuffer.c 			\
	gtksourcecompletion.c			\
	gtksourcecompletioncontext.c		\
	gtksourcecompletioninfo.c		\
	gtksourcecompletionitem.c		\
	gtksourcecompletionproposal.c		\
	gtksourcecompletionprovider.c		\
	gtksourceencoding.c			\
	gtksourcefile.c				\
	gtksourcefileloader.c			\
	gtksourcefilesaver.c			\
	gtksourcegutter.c			\
	gtksourcegutterrenderer.c		\
	gtksourcegutterrendererpixbuf.c		\
	gtksourcegutterrenderertext.c		\
	gtksourcelanguage.c 			\
	gtksourcelanguagemanager.c 		\
	gtksourcemap.c				\
	gtksourcemark.c				\
	gtksourcemarkattributes.c		\
	gtksourceprintcompositor.c		\
	gtksourceregion.c			\
	gtksourcesearchcontext.c		\
	gtksourcesearchsettings.c		\
	gtksourcespacedrawer.c			\
	gtksourcestyle.c			\
	gtksourcestylescheme.c			\
	gtksourcestyleschemechooser.c		\
	gtksourcestyleschemechooserbutton.c	\
	gtksourcestyleschemechooserwidget.c	\
	gtksourcestyleschememanager.c		\
	gtksourcetag.c				\
	gtksourceundomanager.c 			\
	gtksourceutils.c			\
	gtksourceversion.c			\
	gtksourceview.c

libgtksourceview_private_headers =		\
	gconstructor.h				\
	gtksourcebuffer-private.h		\
	gtksourcebufferinputstream.h		\
	gtksourcebufferinternal.h		\
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
	gtksourceiter.h				\
	gtksourcelanguage-private.h		\
	gtksourcemarkssequence.h		\
	gtksourcepixbufhelper.h			\
	gtksourceregex.h			\
	gtksourcespacedrawer-private.h		\
	gtksourcestyle-private.h		\
	gtksourcetypes-private.h		\
	gtksourceundomanagerdefault.h		\
	gtksourceview-i18n.h			\
	gtksourceview-utils.h

libgtksourceview_private_c_files =	\
	gtksourcebufferinputstream.c	\
	gtksourcebufferinternal.c	\
	gtksourcebufferoutputstream.c	\
	gtksourcecompletioncontainer.c	\
	gtksourcecompletionmodel.c	\
	gtksourcecontextengine.c	\
	gtksourceengine.c		\
	gtksourcegutterrendererlines.c	\
	gtksourcegutterrenderermarks.c	\
	gtksourceiter.c			\
	gtksourcelanguage-parser-1.c	\
	gtksourcelanguage-parser-2.c	\
	gtksourcemarkssequence.c	\
	gtksourcepixbufhelper.c		\
	gtksourceregex.c		\
	gtksourceundomanagerdefault.c	\
	gtksourceview-i18n.c		\
	gtksourceview-init.c		\
	gtksourceview-utils.c

# Split in a helper library, so the private functions can be used in unit tests.
noinst_LTLIBRARIES = libgtksourceview-core.la

libgtksourceview_core_la_SOURCES =		\
	$(libgtksourceview_public_c_files)	\
	$(libgtksourceview_public_headers)	\
	$(libgtksourceview_private_c_files)	\
	$(libgtksourceview_private_headers)

# do not distribute generated files
nodist_libgtksourceview_core_la_SOURCES = \
	$(BUILT_SOURCES)

libgtksourceview_core_la_CFLAGS = 	\
	$(CODE_COVERAGE_CFLAGS)

libgtksourceview_core_la_LDFLAGS =	\
	-no-undefined 			\
	$(WARN_LDFLAGS)			\
	$(CODE_COVERAGE_LDFLAGS)

if OS_OSX
libgtksourceview_core_la_CFLAGS += -xobjective-c
libgtksourceview_core_la_LDFLAGS += -framework Cocoa
endif

# The real library
lib_LTLIBRARIES = libgtksourceview-3.0.la

libgtksourceview_3_0_la_SOURCES =

libgtksourceview_3_0_la_LIBADD = 					\
	-lm								\
	libgtksourceview-core.la					\
	completion-providers/words/libgtksourcecompletionwords.la	\
	$(DEP_LIBS) $(GTK_MAC_LIBS)

libgtksourceview_3_0_la_CFLAGS = 	\
	$(HIDDEN_VISIBILITY_CFLAGS)	\
	$(CODE_COVERAGE_CFLAGS)

libgtksourceview_3_0_la_LDFLAGS =		\
	-version-info $(GSV_LT_VERSION)		\
	-no-undefined 				\
	-export-dynamic				\
	$(WARN_LDFLAGS)				\
	$(CODE_COVERAGE_LDFLAGS)

libgtksourceview_3_0_includedir = $(includedir)/gtksourceview-3.0/gtksourceview

libgtksourceview_3_0_include_HEADERS = $(libgtksourceview_public_headers)

nodist_libgtksourceview_3_0_include_HEADERS =	\
	gtksourceversion.h			\
	gtksourceview-enumtypes.h

gtksourceview-resources.c: gtksourceview.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gtksourceview.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/gtksourceview.gresource.xml

gtksourceview-enumtypes.c: stamp-gtksourceview-enumtypes.c
	@true
stamp-gtksourceview-enumtypes.c: Makefile.am $(libgtksourceview_public_headers)
	$(AM_V_GEN) (cd $(srcdir) \
	 && $(GLIB_MKENUMS) \
			--fhead "#ifdef HAVE_CONFIG_H\n" \
			--fhead "#include \"config.h\"\n" \
			--fhead "#endif\n\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"gtksourceversion.h\"\n" \
			--fhead "#include \"gtksourceview-enumtypes.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libgtksourceview_public_headers) ) > xgen-gtbec \
	&& (cmp -s xgen-gtbec gtksourceview-enumtypes.c || cp xgen-gtbec gtksourceview-enumtypes.c) \
	&& rm -f xgen-gtbec && echo stamp > $(@F)

gtksourceview-enumtypes.h: stamp-gtksourceview-enumtypes.h
	@true

# FIXME: we have to sed the file because glib-mkenums does
#        not recognize GTK_SOURCE as the prefix but instead GTK
stamp-gtksourceview-enumtypes.h: Makefile.am $(libgtksourceview_public_headers)
	$(AM_V_GEN) (cd $(srcdir) \
	 && $(GLIB_MKENUMS) \
			--fhead "#ifndef GTKSOURCEVIEW_ENUMTYPES_H\n" \
			--fhead "#define GTKSOURCEVIEW_ENUMTYPES_H\n\n" \
			--fhead "#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)\n" \
			--fhead "#  if defined (__GNUC__)\n" \
			--fhead "#    warning \"Only <gtksourceview/gtksource.h> can be included directly.\"\n" \
			--fhead "#  elif defined (G_OS_WIN32)\n" \
			--fhead "#    pragma message(\"Only <gtksourceview/gtksource.h> can be included directly.\")\n" \
			--fhead "#  endif\n" \
			--fhead "#endif\n\n" \
			--ftail "#endif /* GTKSOURCEVIEW_ENUMTYPES_H */\n" \
			--fprod "#include <gtksourceview/@filename@>\n" \
			--eprod "G_BEGIN_DECLS\n" \
			--eprod "#define GTK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GTK_SOURCE_ENUM_EXTERN\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
			--eprod "G_END_DECLS\n\n" \
		$(libgtksourceview_public_headers) ) > xgen-gth \
	&& $(SED) -e 's/GTK_TYPE_SOURCE_/GTK_SOURCE_TYPE_/' <xgen-gth >xgen-gth.temp && mv xgen-gth.temp xgen-gth \
	&& (cmp -s xgen-gth gtksourceview-enumtypes.h || cp xgen-gth gtksourceview-enumtypes.h) \
	&& rm -f xgen-gth && echo stamp > $(@F)

EXTRA_DIST = 					\
	GtkSource.metadata.in			\
	gtksourcecompletion.ui			\
	gtksourceview.gresource.xml

CLEANFILES = 					\
	$(BUILT_SOURCES)			\
	stamp-gtksourceview-enumtypes.c		\
	stamp-gtksourceview-enumtypes.h

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
GtkSource_3_0_gir_SCANNERFLAGS = $(WARN_SCANNERFLAGS)
GtkSource_3_0_gir_LIBS = libgtksourceview-3.0.la

# Ensure that gtksource.h is the first one.
GtkSource_3_0_gir_FILES =			\
	gtksource.h				\
	$(libgtksourcecompletionwords_files)	\
	$(libgtksourceview_public_headers)	\
	$(libgtksourceview_public_c_files)	\
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

# MSVC items

# Pull in the libgtksourcecompletionwords sources since on MSVC we want to
# build everything in the same .dll with no intermediate libs
gtksource_win32_more_sources =						\
	completion-providers/words/gtksourcecompletionwords.c		\
	completion-providers/words/gtksourcecompletionwordsbuffer.c	\
	completion-providers/words/gtksourcecompletionwordslibrary.c	\
	completion-providers/words/gtksourcecompletionwordsproposal.c	\
	completion-providers/words/gtksourcecompletionwordsutils.c

MSVCPROJS = gtksourceview

gtksourceview_FILES = 					\
	$(libgtksourceview_public_c_files)		\
	$(libgtksourceview_private_c_files)		\
	$(nodist_libgtksourceview_core_la_SOURCES)	\
	$(gtksource_win32_more_sources)

gtksourceview_EXCLUDES = dummy
gtksourceview_HEADERS_DIR = $(libgtksourceview_3_0_includedir)
gtksourceview_HEADERS_INST = \
	$(libgtksourceview_public_headers)	\
	$(nodist_libgtksourceview_3_0_include_HEADERS)

gtksourceview_HEADERS_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

$(top_builddir)/win32/enums.headers: $(libgtksourceview_public_headers)
	-$(RM) $(top_builddir)/win32/enum.headers
	for F in $(libgtksourceview_public_headers); do \
		case $$F in \
		*.h) \
			echo $$F' ^'>>$(top_builddir)/win32/enum.headers \
			;; \
		esac \
	done

# NMake/MSVC introspection
if HAVE_INTROSPECTION
NMAKE_INTROSPECTION_FILES = \
	$(top_builddir)/win32/GtkSource-3.0.gir.msvc.introspect	\
	$(top_builddir)/win32/GtkSource_3_0_gir_list

MSVC_INTROSPECT_GIRS = GtkSource-3.0.gir

# Ensure that gtksource.h is the first one.
GtkSource_3_0_gir_MSVC_FILES = \
	gtksource.h				\
	$(libgtksourcecompletionwords_files)	\
	$(libgtksourceview_public_headers)	\
	$(libgtksourceview_public_c_files)	\
	$(BUILT_SOURCES)

GtkSource_3_0_gir_MSVC_INCLUDE_GIRS = Gtk-3.0 Gdk-3.0
GtkSource_3_0_gir_MSVC_PACKAGES = gtk+-3.0 gdk-3.0
GtkSource_3_0_gir_MSVC_EXPORT_PACKAGES = gtksourceview-3.0
GtkSource_3_0_gir_MSVC_LIBS = gtksourceview-3.0
GtkSource_3_0_gir_MSVC_CFLAGS = -I.. -DGTK_SOURCE_COMPILATION

GtkSource_3_0_gir_MSVC_SCANNERFLAGS = \
	--c-include=gtksourceview/gtksource.h	\
	--warn-all	\
	--filelist=GtkSource_3_0_gir_list

include $(top_srcdir)/win32/Makefile.msvc-introspection
else
NMAKE_INTROSPECTION_FILES =
endif

dist-hook: \
	$(top_builddir)/win32/enums.headers		\
	$(top_builddir)/win32/vs9/gtksourceview.vcproj	\
	$(top_builddir)/win32/vs9/gtksourceview.headers	\
	$(NMAKE_INTROSPECTION_FILES)

-include $(top_srcdir)/git.mk
