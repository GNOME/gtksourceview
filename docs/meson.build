if build_documentation and not get_option('introspection').allowed()
  error('API reference requires introspection.')
endif

toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen', required: build_documentation)

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

docs_dir = datadir / 'doc'

if build_documentation
  source_toml = configure_file(
    input: 'GtkSource.toml.in',
    output: 'GtkSource.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'gtksourceview5',
  )

  custom_target('gtksourceview-doc',
    input: [ source_toml, gtksource_gir[0] ],
    output: 'gtksourceview5',
    command: [
      gidocgen,
      'generate',
      gidocgen_common_args,
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )
endif
