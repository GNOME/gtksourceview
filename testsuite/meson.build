testsuite_c_args = [
  '-DG_LOG_DOMAIN="GtkSourceView"',
  '-DTOP_SRCDIR="@0@"'.format(srcdir),
]

testsuite_sources = [
  ['test-buffer'],
  ['test-buffer-input-stream'],
  ['test-buffer-output-stream'],
  ['test-completion-model'],
  ['test-completion-words'],
  ['test-encoding'],
  ['test-file-loader'],
  ['test-file-saver'],
  ['test-iter'],
  ['test-language'],
  ['test-languagemanager'],
  ['test-mark'],
  ['test-printcompositor'],
  ['test-regex'],
  ['test-region'],
  ['test-search-context'],
  ['test-space-drawer'],
  ['test-stylescheme'],
  ['test-styleschememanager'],
  ['test-undo-manager'],
  ['test-utils'],
  ['test-view'],
]

foreach test: testsuite_sources
  test_name = test.get(0)
  test_sources = [
    '@0@.c'.format(test_name),
    test.get(1, [])
  ]

  test_exe = executable(test_name, test_sources,
          c_args: testsuite_c_args,
    dependencies: [gtksource_dep],
         install: get_option('install_tests'),
     install_dir: testexecdir
  )

  test(test_name, test_exe, suite: package_string)

  if get_option('install_tests')
    test_t = configuration_data()
    test_t.set('testexecdir', testexecdir)
    test_t.set('test', test_name)

    configure_file(
              input: 'gtksourceview.test.in',
             output: '@0@.test'.format(test_name),
      configuration: test_t,
            install: true,
        install_dir: testdatadir
    )
  endif
endforeach

if get_option('install_tests')
  install_subdir('language-specs', install_dir: testexecdir)
  install_subdir('styles', install_dir: testexecdir)
endif
