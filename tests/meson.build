tests_c_args = [
  '-DG_LOG_DOMAIN="GtkSourceView"',
  '-DTOP_SRCDIR="@0@"'.format(srcdir),
]

tests_sources = {
  'completion': ['test-completion.c'],
  'search': ['test-search.c'],
  'search-performances': ['test-search-performances.c'],
  'space-drawing': ['test-space-drawing.c'],
  'undo-manager-performances': ['test-undo-manager-performances.c'],
  'widget': ['test-widget.c'],
}

tests_resources = {
  'completion': 'test-completion.gresource.xml',
  'search': 'test-search.gresource.xml',
  'widget': 'test-widget.gresource.xml',
}

foreach test_name, test_sources: tests_sources
  if tests_resources.has_key(test_name)
    test_sources += gnome.compile_resources(
      'test-@0@-resources'.format(test_name),
      tests_resources.get(test_name),
    )
  endif

  executable(
    'test-@0@'.format(test_name),
    test_sources,
    c_args: tests_c_args,
    dependencies: [gtksource_dep])
endforeach
