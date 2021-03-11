core_marshallers = gnome.genmarshal('gtksource-marshal',
             sources: ['gtksourcemarshalers.list'],
              prefix: '_gtk_source_marshal',
         skip_source: true,
  valist_marshallers: true,
)

core_public_h = files([
  'gtksource.h',
  'gtksourcebuffer.h',
  'gtksourcecompletion.h',
  'gtksourcecompletioncell.h',
  'gtksourcecompletioncontext.h',
  'gtksourcecompletionprovider.h',
  'gtksourcecompletionproposal.h',
  'gtksourceencoding.h',
  'gtksourcefile.h',
  'gtksourcefileloader.h',
  'gtksourcefilesaver.h',
  'gtksourcegutter.h',
  'gtksourcegutterrenderer.h',
  'gtksourcegutterrendererpixbuf.h',
  'gtksourcegutterrenderertext.h',
  'gtksourcehover.h',
  'gtksourcehovercontext.h',
  'gtksourcehoverprovider.h',
  'gtksourcehoverdisplay.h',
  'gtksourceindenter.h',
  'gtksourceinit.h',
  'gtksourcelanguage.h',
  'gtksourcelanguagemanager.h',
  'gtksourcegutterlines.h',
  'gtksourcemap.h',
  'gtksourcemark.h',
  'gtksourcemarkattributes.h',
  'gtksourceprintcompositor.h',
  'gtksourceregion.h',
  'gtksourcesearchcontext.h',
  'gtksourcesearchsettings.h',
  'gtksourcesnippet.h',
  'gtksourcesnippetchunk.h',
  'gtksourcesnippetcontext.h',
  'gtksourcesnippetmanager.h',
  'gtksourcespacedrawer.h',
  'gtksourcestyle.h',
  'gtksourcestylescheme.h',
  'gtksourcestyleschemechooser.h',
  'gtksourcestyleschemechooserbutton.h',
  'gtksourcestyleschemechooserwidget.h',
  'gtksourcestyleschememanager.h',
  'gtksourcetag.h',
  'gtksourcetypes.h',
  'gtksourceutils.h',
  'gtksourceview.h',
])

core_public_c = files([
  'gtksourcebuffer.c',
  'gtksourcecompletion.c',
  'gtksourcecompletioncell.c',
  'gtksourcecompletioncontext.c',
  'gtksourcecompletionprovider.c',
  'gtksourcecompletionproposal.c',
  'gtksourceencoding.c',
  'gtksourcefile.c',
  'gtksourcefileloader.c',
  'gtksourcefilesaver.c',
  'gtksourcegutter.c',
  'gtksourcegutterrenderer.c',
  'gtksourcegutterrendererpixbuf.c',
  'gtksourcegutterrenderertext.c',
  'gtksourcehover.c',
  'gtksourcehovercontext.c',
  'gtksourcehoverdisplay.c',
  'gtksourcehoverprovider.c',
  'gtksourceindenter.c',
  'gtksourceinit.c',
  'gtksourcelanguage.c',
  'gtksourcelanguagemanager.c',
  'gtksourcegutterlines.c',
  'gtksourcemap.c',
  'gtksourcemark.c',
  'gtksourcemarkattributes.c',
  'gtksourceprintcompositor.c',
  'gtksourceregion.c',
  'gtksourcesearchcontext.c',
  'gtksourcesearchsettings.c',
  'gtksourcesnippet.c',
  'gtksourcesnippetchunk.c',
  'gtksourcesnippetcontext.c',
  'gtksourcesnippetmanager.c',
  'gtksourcespacedrawer.c',
  'gtksourcestyle.c',
  'gtksourcestylescheme.c',
  'gtksourcestyleschemechooser.c',
  'gtksourcestyleschemechooserbutton.c',
  'gtksourcestyleschemechooserwidget.c',
  'gtksourcestyleschememanager.c',
  'gtksourcetag.c',
  'gtksourceutils.c',
  'gtksourceversion.c',
  'gtksourceview.c',
])

core_private_c = files([
  'gtksourceassistant.c',
  'gtksourceassistantchild.c',
  'gtksourcebindinggroup.c',
  'gtksourcebufferinputstream.c',
  'gtksourcebufferinternal.c',
  'gtksourcebufferoutputstream.c',
  'gtksourcecompletioninfo.c',
  'gtksourcecompletionlist.c',
  'gtksourcecompletionlistbox.c',
  'gtksourcecompletionlistboxrow.c',
  'gtksourcecontextengine.c',
  'gtksourceengine.c',
  'gtksourcegutterrendererlines.c',
  'gtksourcegutterrenderermarks.c',
  'gtksourcehoverassistant.c',
  'gtksourceinformative.c',
  'gtksourceiter.c',
  'gtksourcelanguage-parser-2.c',
  'gtksourcemarkssequence.c',
  'gtksourcepixbufhelper.c',
  'gtksourceregex.c',
  'gtksourcesignalgroup.c',
  'gtksourceview-assistants.c',
  'gtksourcesnippetbundle.c',
  'gtksourcesnippetbundle-parser.c',
  'gtksourceview-snippets.c',
  'implregex.c',
])

core_c_args = [
  '-DGTK_SOURCE_COMPILATION',
  '-DG_LOG_DOMAIN="GtkSourceView"',
]

core_deps = [
  libm_dep,
  fribidi_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
  gtk_dep,
  libxml_dep,
  pangoft2_dep,
  fontconfig_dep,
  pcre2_dep,
]

if profiler_enabled
  core_deps += [libsysprof_capture_dep]
endif

if config_h.has('OS_OSX')
  core_c_args += '-xobjective-c'
  core_deps += [
    cocoa_dep,
  ]
endif

if cc.get_id() == 'msvc' and cc.version().version_compare('<18')
  # include our math.h to implement round() for pre-2013 Visual Studio
  gtksourceview_extra_include_dirs = include_directories('../msvc')
else
  gtksourceview_extra_include_dirs = []
endif

gtksourceview_include_dirs = [rootdir, gtksourceview_extra_include_dirs]

core_enums_header = '''

#if defined (GTK_SOURCE_COMPILATION)
# include "config.h"
#endif

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
# error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtksourceview/gtksourceversion.h>
'''

core_enums = gnome.mkenums_simple('gtksource-enumtypes',
            sources: core_public_h,
  identifier_prefix: 'GtkSource',
      symbol_prefix: 'gtk_source',
      header_prefix: core_enums_header,
          decorator: 'GTK_SOURCE_ENUM_EXTERN',
     install_header: true,
        install_dir: join_paths(pkgincludedir, 'gtksourceview'),
)

version_h = configuration_data()
version_h.set('GTK_SOURCE_MAJOR_VERSION', version_major)
version_h.set('GTK_SOURCE_MINOR_VERSION', version_minor)
version_h.set('GTK_SOURCE_MICRO_VERSION', version_micro)

gtksourceversion_h = configure_file(
          input: 'gtksourceversion.h.in',
         output: 'gtksourceversion.h',
  configuration: version_h,
    install_dir: join_paths(pkgincludedir, 'gtksourceview'),
)

gtksource_res = gnome.compile_resources(
  'gtksourceview-gresources',
  'gtksourceview.gresource.xml'
)

core_sources = [
  core_public_c,
  core_private_c,
  core_enums,
  gtksourceversion_h,
  core_marshallers,
  gtksource_res,
]

install_headers(
  core_public_h,
  install_dir: join_paths(pkgincludedir, 'gtksourceview'),
)

core_enums_h = core_enums.get(1)

core_lib = static_library(package_string + 'core', core_sources,
  include_directories: gtksourceview_include_dirs,
         dependencies: core_deps,
               c_args: core_c_args,
              install: false
)

core_dep = declare_dependency(
            link_with: core_lib,
  include_directories: rootdir,
         dependencies: core_deps,
              sources: [core_enums_h]
)

gtksource_deps = [
  core_dep,
]

gtksource_libs = [core_lib]

extra_public_sources = []

subdir('completion-providers')


gtksource_lib = library(package_string,
                version: lib_version,
        darwin_versions: lib_osx_version,
    include_directories: gtksourceview_include_dirs,
           dependencies: gtksource_deps,
             link_whole: gtksource_libs,
                 c_args: core_c_args,
              link_args: release_link_args,
                install: true,
  gnu_symbol_visibility: 'hidden',
)

gtksource_dep_sources = [
  core_enums_h,
]

if generate_gir
  gtksource_gir_sources = [
    core_public_c,
    core_public_h,
    core_enums_h,
    extra_public_sources,
  ]

  gtksource_gir = gnome.generate_gir(gtksource_lib,
                sources: gtksource_gir_sources,
              nsversion: api_version,
              namespace: 'GtkSource',
          symbol_prefix: 'gtk_source',
      identifier_prefix: 'GtkSource',
        export_packages: [package_string],
              link_with: gtksource_lib,
           dependencies: gtksource_deps,
               includes: [ 'Gtk-4.0' ],
                install: true,
        install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
             extra_args: [ '--c-include=gtksourceview/gtksource.h',
                           '--warn-all' ],
  )

  gtksource_dep_sources += [
    gtksource_gir,
  ]

  if generate_vapi
    configure_file(
        input: 'GtkSource.metadata.in',
       output: 'GtkSource-@0@.metadata'.format(api_version),
         copy: true,
    )

    gtksource_vapi = gnome.generate_vapi(package_string,
            sources: gtksource_gir.get(0),
      metadata_dirs: join_paths(builddir, 'gtksourceview'),
            install: true,
        install_dir: vapidir,
           packages: [ 'gdk-pixbuf-2.0',
                       'gio-2.0',
                       'gtk4',
                       'pango',
                       'cairo' ],
    )
  endif
endif

gtksource_dep = declare_dependency(
            link_with: gtksource_lib,
  include_directories: rootdir,
         dependencies: gtksource_deps,
              sources: gtksource_dep_sources,
)

gtksource_pc_reqs = [
  'glib-2.0 @0@'.format(glib_req),
  'gobject-2.0 @0@'.format(glib_req),
  'gio-2.0 @0@'.format(glib_req),
  'gtk4 @0@'.format(gtk_req),
]

gtksource_pc = pkg.generate(gtksource_lib,
              name: package_string,
       description: 'Source code editing Gtk+ widget.',
          requires: gtksource_pc_reqs,
           version: version,
           subdirs: package_string,
       install_dir: pkgconfigdir,
)
