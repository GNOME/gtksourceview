GtkSourceView
=============

GtkSourceView is a GNOME library that extends GtkTextView, the standard GTK
widget for multiline text editing. GtkSourceView adds support for syntax
highlighting, file loading and saving, search and replace, code completion,
snippets, Vim emulation, printing, displaying line numbers, and other features
typical of a source code editor.

The GtkSourceView library is free software and is released under the terms of
the GNU Lesser General Public License, see the 'COPYING' file for more details.
The official web site is https://wiki.gnome.org/Projects/GtkSourceView.

Documentation
-------------

Nightly documentation can be [found here](https://gnome.pages.gitlab.gnome.org/gtksourceview/gtksourceview5/).

Dependencies
------------

* GLib >= 2.72
* GTK >= 4.17
* libxml2 >= 2.6
* fribidi >= 0.19.7
* libpcre2-8 >= 10.21

Installation
------------

Simple install procedure from a tarball:
```
  $ meson setup build .
  $ cd build
  $ ninja install
```

To run the test suite:
```
  $ ninja test
```

To build the latest version of GtkSourceView plus its dependencies from Git,
[Jhbuild](https://wiki.gnome.org/Projects/Jhbuild) is recommended.


How to contribute
-----------------

See the 'HACKING' file.
