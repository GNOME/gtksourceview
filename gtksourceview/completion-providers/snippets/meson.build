completionsnippets_c_args = [
  '-DGTK_SOURCE_COMPILATION',
  '-DG_LOG_DOMAIN="GtkSourceView"',
]

completionsnippets_public_h = [
  'gtksourcecompletionsnippets.h',
]

completionsnippets_sources = [
  'gtksourcecompletionsnippets.c',
  'gtksourcecompletionsnippetsproposal.c',
]

install_headers(
  completionsnippets_public_h,
  install_dir: join_paths(
    pkgincludedir,
    'gtksourceview',
    'completion-providers',
    'snippets'
  )
)

completionsnippets_lib = static_library(
  package_string + 'completionsnippets',
  completionsnippets_sources,
  include_directories: rootdir,
  dependencies: core_dep,
  c_args: completionsnippets_c_args,
  install: false,
)

gtksource_libs += [
  completionsnippets_lib
]

completionsnippets_dep = declare_dependency(
  link_with: completionsnippets_lib,
  include_directories: rootdir,
  dependencies: core_dep,
)

gtksource_deps += completionsnippets_dep

extra_public_sources += files([
  'gtksourcecompletionsnippets.c',
  'gtksourcecompletionsnippets.h',
])
