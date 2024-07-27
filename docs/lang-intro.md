Title: Language Definition v2.0 Introduction
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2024 Sébastien Wilmet

# Language Definition v2.0 Introduction

Introduction to the GtkSourceView language definition files.

## Purpose

The primary purpose of a language definition file (a `*.lang` file) is to add
support for syntax highlighting for a specific language.

## About the versions

We need to distinguish:

- The GtkSourceView library versions.
- The language definition file format versions.

The file format is at version 2 and is supported by the GtkSourceView library
versions 2 to 5.

## Directories

To find `*.lang` files, GtkSourceView looks by default at several directories,
relying on the
[XDG Base Directory Specification](https://www.freedesktop.org/wiki/Specifications/basedir-spec/).

On Linux it is usually:

- `~/.local/share/gtksourceview-5/language-specs/` for custom `*.lang` files.
- `/usr/share/gtksourceview-5/language-specs/` for the `*.lang` files installed
  by GtkSourceView 5.

More precisely:

- `${XDG_DATA_HOME}/gtksourceview-${GSV_API_VERSION}/language-specs/` used in
  priority.
- `${data_dir}/gtksourceview-${GSV_API_VERSION}/language-specs/` for each
  `data_dir` of `${XDG_DATA_DIRS}`.

Replace `${GSV_API_VERSION}` by `5` for GtkSourceView 5.

## List of `*.lang` files shipped by GtkSourceView

To see if a language is supported by GtkSourceView, see the
[list of `*.lang` files](https://gitlab.gnome.org/GNOME/gtksourceview/-/tree/master/data/language-specs)
in the Git repository.
