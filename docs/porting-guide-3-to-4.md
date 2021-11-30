Title: GtkSourceView 3 -> 4 Migration Guide

# GtkSourceView 3 -> 4 Migration Guide

## Dependencies

Both GtkSourceView 3 and GtkSourceView 4 depend on GTK 3.

## Preparation in GtkSourceView 3

GtkSourceView 3.24 is the latest stable GtkSourceView 3 version. Before
continuing this porting guide, you should use the 3.24 version without
using any deprecated API.

## New pkg-config name

For GtkSourceView 4, the pkg-config name is: `gtksourceview-4`
To compile a program that uses GtkSourceView 4, you
can for example use the following command:

```
$ gcc hello.c "pkg-config --cflags --libs gtksourceview-4" -o hello
```

## GtkSourceView 3.99.1

- All the deprecated APIs have been removed.
- Only `<gtksourceview/gtksource.h>` can be included directly. There were already warnings about it in GtkSourceView 3. The warnings have been changed to errors.

- Only the version 2 of the [GtkSourceView language definition file format](./lang-reference.html) is supported (for *.lang files, used for syntax highlighting). The support for the version 1 has been
dropped.

## GtkSourceView 3.99.2

- `gtk_source_completion_item_new2()` has been renamed to `gtk_source_completion_item_new()`
- `gtk_source_search_context_forward2()` has been renamed to [method@SearchContext.forward].
- `gtk_source_search_context_forward_finish2()` has been renamed to [method@SearchContext.forward_finish].
- `gtk_source_search_context_backward2()` has been renamed to [method@SearchContext.backward].
- `gtk_source_search_context_backward_finish2()` has been renamed to [method@SearchContext.backward_finish].
- `gtk_source_search_context_replace2()` has been renamed to [method@SearchContext.replace].
- The [property@SearchContext:settings] property is now construct-only.

## GtkSourceView 3.99.3

No API changes.

## GtkSourceView 3.99.4

The API of the [signal@View::move-lines] keybinding signal has been simplified:

- The `copy` parameter was deprecated and has been removed
- The `count` parameter has been replaced by the `down` boolean.

## GtkSourceView 3.99.5 and 3.99.6

No API changes.

## GtkSourceView 3.99.7

In order to have a better *.gir file and have less metadata to generate the `*.vapi` for Vala, the following change has been made:

- `gtk_source_completion_show()` has been renamed to `gtk_source_completion_start()`. The function conflicted with the [signal@Completion::show] signal.
Note that in Vala this doesn't require code changes because the method was already renamed to `start()` in GtkSourceView 3.

- For i18n initialization, the [func@init] function now needs to be called. There is also the [func@finalize]
function which is convenient when using memory debugging tools.
