glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

reference_private_h = [
  'config.h',
  'gtksource.h',
  'gtksourceassistant-private.h',
  'gtksourceassistantchild-private.h',
  'gtksourcebindinggroup-private.h',
  'gtksourcebuffer-private.h',
  'gtksourcebufferinputstream-private.h',
  'gtksourcebufferinternal-private.h',
  'gtksourcebufferoutputstream-private.h',
  'gtksourcecompletion-private.h',
  'gtksourcecompletioncell-private.h',
  'gtksourcecompletioncontext-private.h',
  'gtksourcecompletioninfo-private.h',
  'gtksourcecompletionlist-private.h',
  'gtksourcecompletionlistbox-private.h',
  'gtksourcecompletionlistboxrow-private.h',
  'gtksourcecontextengine-private.h',
  'gtksourceencoding-private.h',
  'gtksourceengine-private.h',
  'gtksourcefile-private.h',
  'gtksourcegutter-private.h',
  'gtksourcegutterlines-private.h',
  'gtksourcegutterrenderer-private.h',
  'gtksourcegutterrendererlines-private.h',
  'gtksourcegutterrenderermarks-private.h',
  'gtksourceinformative-private.h',
  'gtksourceiter-private.h',
  'gtksourcelanguage-private.h',
  'gtksourcelanguagemanager-private.h',
  'gtksourcemarkssequence-private.h',
  'gtksourcepixbufhelper-private.h',
  'gtksourceregex-private.h',
  'gtksourcesearchcontext-private.h',
  'gtksourcesignalgroup-private.h',
  'gtksourcesnippet-private.h',
  'gtksourcesnippetbundle-private.h',
  'gtksourcesnippetchunk-private.h',
  'gtksourcesnippetcontext-private.h',
  'gtksourcesnippetmanager-private.h',
  'gtksourcespacedrawer-private.h',
  'gtksourcestyle-private.h',
  'gtksourcestylescheme-private.h',
  'gtksourcestyleschememanager-private.h',
  'gtksourcetrace.h',
  'gtksourcetypes-private.h',
  'gtksourceutils-private.h',
  'gtksourceview-private.h',
  'gtksourcecompletionwordsbuffer-private.h',
  'gtksourcecompletionwordslibrary-private.h',
  'gtksourcecompletionwordsproposal-private.h',
  'gtksourcecompletionwordsutils-private.h',
  'gtksourcecompletionsnippetsproposal-private.h',
  'implregex-private.h',
  'quarkset-inline.h',
]

reference_content_files = files([
  'lang-tutorial.xml',
  'porting-guide-4-to-5.xml',
])

reference_sources = [
  join_paths(srcdir, 'gtksourceview'),
  join_paths(builddir, 'gtksourceview'),
]

into_xml = configure_file(
         input: 'intro.xml.in',
         output: 'intro.xml',
  configuration: config_h
)

lang_reference_xml = configure_file(
          input: 'lang-reference.xml.in',
         output: 'lang-reference.xml',
  configuration: config_h
)

snippet_reference_xml = configure_file(
          input: 'snippet-reference.xml.in',
         output: 'snippet-reference.xml',
  configuration: config_h
)

style_reference_xml = configure_file(
          input: 'style-reference.xml.in',
         output: 'style-reference.xml',
  configuration: config_h
)

reference_content_files += [
  into_xml,
  lang_reference_xml,
  style_reference_xml,
]

gtksourceview_docs_xml = configure_file(
          input: 'gtksourceview-docs.xml.in',
         output: 'gtksourceview-docs.xml',
  configuration: config_h
)

reference_scan_args = [
  '--rebuild-types',
  '--ignore-decorators=GTK_SOURCE_DEPRECATED\w*\s*\([^)]*\)|GTK_SOURCE_DEPRECATED\w*|GTK_SOURCE_AVAILABLE\w*|_GTK_SOURCE_EXTERN',
  '--deprecated-guards=GTKSOURCEVIEW_DISABLE_DEPRECATED',
  '--ignore-headers=' + ' '.join(reference_private_h),
]

reference_fixxref_args = [
  '--html-dir=@0@'.format(docpath),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
  '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gtk')),
]

gnome.gtkdoc('@0@.0'.format(package_string),
           main_xml: join_paths(builddir, '@0@'.format(gtksourceview_docs_xml)),
  gobject_typesfile: 'gtksourceview-5.0.types',
            src_dir: reference_sources,
       dependencies: gtksource_dep,
      content_files: reference_content_files,
          scan_args: reference_scan_args,
       fixxref_args: reference_fixxref_args,
            install: true
)
