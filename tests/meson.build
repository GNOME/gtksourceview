tests_c_args = [
  '-DG_LOG_DOMAIN="GtkSourceView"',
  '-DTOP_SRCDIR="@0@"'.format(srcdir),
]

tests_sources = [
  ['test-completion', 'test-completion.gresource.xml'],
  ['test-search', 'test-search.gresource.xml'],
  ['test-search-performances'],
  ['test-space-drawing'],
  ['test-undo-manager-performances'],
  ['test-widget', 'test-widget.gresource.xml']
]

foreach test: tests_sources
  test_name = test.get(0)
  test_sources = ['@0@.c'.format(test_name)]

  if test.length() > 1
    test_sources += gnome.compile_resources(
      test_name + '-resources',
      test.get(1)
    )
  endif

  executable(
    test_name,
    test_sources,
    c_args: tests_c_args,
    dependencies: [gtksource_dep])
endforeach
