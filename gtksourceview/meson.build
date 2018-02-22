core_public_h = files([
  'gtksource.h',
  'gtksourceautocleanups.h',
  'gtksourcebuffer.h',
  'gtksourcecompletion.h',
  'gtksourcecompletioncontext.h',
  'gtksourcecompletioninfo.h',
  'gtksourcecompletionitem.h',
  'gtksourcecompletionproposal.h',
  'gtksourcecompletionprovider.h',
  'gtksourceencoding.h',
  'gtksourcefile.h',
  'gtksourcefileloader.h',
  'gtksourcefilesaver.h',
  'gtksourcegutter.h',
  'gtksourcegutterrenderer.h',
  'gtksourcegutterrendererpixbuf.h',
  'gtksourcegutterrenderertext.h',
  'gtksourceinit.h',
  'gtksourcelanguage.h',
  'gtksourcelanguagemanager.h',
  'gtksourcemap.h',
  'gtksourcemark.h',
  'gtksourcemarkattributes.h',
  'gtksourceprintcompositor.h',
  'gtksourceregion.h',
  'gtksourcesearchcontext.h',
  'gtksourcesearchsettings.h',
  'gtksourcespacedrawer.h',
  'gtksourcestyle.h',
  'gtksourcestylescheme.h',
  'gtksourcestyleschemechooser.h',
  'gtksourcestyleschemechooserbutton.h',
  'gtksourcestyleschemechooserwidget.h',
  'gtksourcestyleschememanager.h',
  'gtksourcetag.h',
  'gtksourcetypes.h',
  'gtksourceundomanager.h',
  'gtksourceutils.h',
  'gtksourceview.h',
])

core_public_c = files([
  'gtksourcebuffer.c',
  'gtksourcecompletion.c',
  'gtksourcecompletioncontext.c',
  'gtksourcecompletioninfo.c',
  'gtksourcecompletionitem.c',
  'gtksourcecompletionproposal.c',
  'gtksourcecompletionprovider.c',
  'gtksourceencoding.c',
  'gtksourcefile.c',
  'gtksourcefileloader.c',
  'gtksourcefilesaver.c',
  'gtksourcegutter.c',
  'gtksourcegutterrenderer.c',
  'gtksourcegutterrendererpixbuf.c',
  'gtksourcegutterrenderertext.c',
  'gtksourceinit.c',
  'gtksourcelanguage.c',
  'gtksourcelanguagemanager.c',
  'gtksourcemap.c',
  'gtksourcemark.c',
  'gtksourcemarkattributes.c',
  'gtksourceprintcompositor.c',
  'gtksourceregion.c',
  'gtksourcesearchcontext.c',
  'gtksourcesearchsettings.c',
  'gtksourcespacedrawer.c',
  'gtksourcestyle.c',
  'gtksourcestylescheme.c',
  'gtksourcestyleschemechooser.c',
  'gtksourcestyleschemechooserbutton.c',
  'gtksourcestyleschemechooserwidget.c',
  'gtksourcestyleschememanager.c',
  'gtksourcetag.c',
  'gtksourceundomanager.c',
  'gtksourceutils.c',
  'gtksourceversion.c',
  'gtksourceview.c',
])

core_private_c = files([
  'gtksourcebufferinputstream.c',
  'gtksourcebufferinternal.c',
  'gtksourcebufferoutputstream.c',
  'gtksourcecompletioncontainer.c',
  'gtksourcecompletionmodel.c',
  'gtksourcecontextengine.c',
  'gtksourceengine.c',
  'gtksourcegutterrendererlines.c',
  'gtksourcegutterrenderermarks.c',
  'gtksourceiter.c',
  'gtksourcelanguage-parser-2.c',
  'gtksourcemarkssequence.c',
  'gtksourcepixbufhelper.c',
  'gtksourceregex.c',
  'gtksourceundomanagerdefault.c',
])

core_c_args = [
  '-DHAVE_CONFIG_H',
  '-DGTK_SOURCE_COMPILATION',
  '-DG_LOG_DOMAIN="GtkSourceView"',
]

core_link_args = [
]

core_deps = [
  libm_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
  gtk_dep,
  libxml_dep,
]

if config_h.has('OS_OSX')
  core_c_args += '-xobjective-c'
  core_link_args += '-framework Cocoa'
endif

core_enums = gnome.mkenums(
  'gtksource-enumtypes',
  sources: core_public_h,
  identifier_prefix: 'GtkSource',
  symbol_prefix: 'gtk_source',
  h_template: 'gtksource-enumtypes.h.template',
  c_template: 'gtksource-enumtypes.c.template',
  install_header: true,
  install_dir: join_paths(
    pkgincludedir,
    'gtksourceview',
  )
)

version_h = configuration_data()
version_h.set('GTK_SOURCE_MAJOR_VERSION', version_major)
version_h.set('GTK_SOURCE_MINOR_VERSION', version_minor)
version_h.set('GTK_SOURCE_MICRO_VERSION', version_micro)

gtksourceversion_h = configure_file(
  input: 'gtksourceversion.h.in',
  output: 'gtksourceversion.h',
  configuration: version_h,
  install: true,
  install_dir: join_paths(
    pkgincludedir,
    'gtksourceview',
  )
)

core_sources = [
  core_public_c,
  core_private_c,
  core_enums,
  gtksourceversion_h,
]

install_headers(
  core_public_h,
  install_dir: join_paths(
    pkgincludedir,
    'gtksourceview',
  )
)

core_lib = static_library(
  package_string + 'core', core_sources,
  include_directories: rootdir,
  dependencies: core_deps,
  c_args: core_c_args,
  link_args: core_link_args,
  install: false
)

gtksource_objs = [
  core_lib.extract_all_objects(),
]

core_enums_h = core_enums.get(1)

core_dep = declare_dependency(
  link_with: core_lib,
  include_directories: rootdir,
  dependencies: core_deps,
  sources: [core_enums_h]
)

gtksource_deps = [
  core_dep,
]

extra_public_sources = []

subdir('completion-providers')

gtksource_res = gnome.compile_resources(
  'gtksourceview-gresources',
  'gtksourceview.gresource.xml'
)

gtksource_lib = shared_library(
  package_string,
  gtksource_res,
  version: lib_version,
  include_directories: rootdir,
  dependencies: gtksource_deps,
  objects: gtksource_objs,
  c_args: core_c_args,
  link_args: core_link_args,
  install: true,
  install_dir: libdir
)

gtksource_dep_sources = [
  gtksourceversion_h,
  core_enums_h,
]

if generate_gir == true
  gtksource_gir_sources = [
    core_public_c,
    core_public_h,
    core_enums_h,
    extra_public_sources,
  ]

  gtksource_gir = gnome.generate_gir(
    gtksource_lib,
    sources: gtksource_gir_sources,
    nsversion: api_version,
    namespace: 'GtkSource',
    symbol_prefix: 'gtk_source',
    identifier_prefix: 'GtkSource',
    export_packages: [package_string],
    link_with: gtksource_lib,
    includes: [ 'Gdk-3.0', 'Gtk-3.0' ],
    install: true,
    install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
    extra_args: [
      '--c-include=gtksourceview/gtksource.h',
      '--warn-all',
    ],
  )

  gtksource_dep_sources += [
    gtksource_gir,
  ]

  if generate_vapi == true
    configure_file(
      input: 'GtkSource.metadata.in',
      output: 'GtkSource-@0@.metadata'.format(api_version),
      copy: true,
      install: false,
    )

    gtksource_vapi = gnome.generate_vapi(
      package_string,
      sources: gtksource_gir.get(0),
      metadata_dirs: join_paths(builddir, 'gtksourceview'),
      install: true,
      install_dir: vapidir,
      packages: [
        'atk',
        'gdk-3.0',
        'gdk-pixbuf-2.0',
        'gio-2.0',
        'gtk+-3.0',
        'pango',
        'cairo',
      ],
    )
  endif
endif

gtksource_dep = declare_dependency(
  link_with: gtksource_lib,
  include_directories: rootdir,
  dependencies: gtksource_deps,
  sources: gtksource_dep_sources,
)
