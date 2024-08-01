Title: Overview

# Overview

GtkSourceView is a [GNOME](https://www.gnome.org/) library that extends
GtkTextView, the standard GTK widget for multiline text editing. GtkSourceView
adds support for syntax highlighting, file loading and saving, search and
replace, code completion, snippets, Vim emulation, printing, displaying line
numbers, and other features typical of a source code editor.

See the [GtkSourceView website](https://wiki.gnome.org/Projects/GtkSourceView).

GtkSourceView 5 depends on GTK 4.

## pkg-config name

For GtkSourceView 5, the pkg-config name is: `gtksourceview-5`

To compile a program that uses GtkSourceView 5, you
can for example use the following command:

```
$ gcc hello.c `pkg-config --cflags --libs gtksourceview-5` -o hello
```

## Library Initialization

Your application *MUST* initialize GtkSourceView from `main()`.
Call `gtk_source_init()` or its variant within your programming language.
A great place to do this is right after calling `gtk_init()`.

In language bindings, this function is often named similar to `GtkSource.init()`.
