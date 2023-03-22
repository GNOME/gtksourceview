Title: Style Scheme Definition Reference

# Style Scheme Definition Reference

Reference to the GtkSourceView style scheme definition file format

## Overview

This is an overview of the Style Scheme Definition XML format, describing the
meaning and usage of every element and attribute.  The formal definition is
stored in the RelaxNG schema file `style.rng` which
should be installed on your system in the directory
`${PREFIX}/share/gtksourceview-5/` (where
`${PREFIX}` can be `/usr/` or
`/usr/local/` if you have installed from source).

The toplevel tag in a style scheme file is `<style-scheme>`.
It has the following attributes:

- `id` (mandatory)

  Identifier for the style scheme. This is must be unique among style schemes.

- `name` (mandatory)

  Name of the style scheme. This is the name of the scheme to display to user, e.g. in a preferences dialog.

- `_name`

  This is the same as `name` attribute, except it will be translated. `name` and `_name` may not be used simultaneously.

- `parent-scheme` (optional)

  Style schemes may have **parent** schemes: all styles but those specified
  in the scheme will be taken from the parent scheme. In this way a scheme may
  be customized without copying all its content.

- `version` (mandatory)

  Style scheme format identifier. At the moment it must be "1.0".

- `style-scheme` tag may contain the following tags:

  - `author` Name of the style scheme author.

  - `description` Description of the style scheme.
  
  - `_description` Same as `description` except it will be localized.
  
  - `color` tags

    These define color names to be used in `style` tags.
    It has two attributes: `name` and `value`.
    `value` is the hexadecimal color specification like
    "#000000" or named color understood by Gdk prefixed with "#",
    e.g. "#beige".
  
  - `style` tags

    See below for their format description.

Each `style` tag describes a single element of style scheme (it corresponds
to [class@Style] object). It has the following attributes:

- `name` (mandatory)

  Name of the style. It can be anything, syntax highlighting uses **lang-id:style-id**,
  and there are few special styles which are used to control general appearance
  of the text. Style scheme may contain other names to be used in an application. For instance,
  it may define color to highlight compilation errors in a build log or a color for
  bookmarks.

- `foreground`

  Foreground color. It may be name defined in one of `color` tags, or value in
  hexadecimal format, e.g. "#000000", or symbolic name understood
  by Gdk, prefixed with "#", e.g. "#magenta" or "#darkred".

- `background`

  Background color.

- `italic`: `true` or `false`

- `bold`: `true` or `false`

- `weight`

  The weight attribute was added in GtkSourceView 5.4.

  The weight of the matched text block. The default value is "normal" but
  you may use any of the [enum@Pango.Weight] nicks or optionally a numeric
  value equivalent to the weights of CSS.

  Setting this value will override any setting of "bold".

- `underline`

  Accepts the values supported by [enum@Pango.Underline] ("none", "single",
  "double", "low", "error"). GtkSourceView versions <= 3.16 only
  supported `true` or `false` and such value are still accepted
  for backward compatibility.

- `underline-color`

  Underline color.

- `strikethrough`: `true` or `false`

- `scale`

  Scale the matched text block. Possible values are a float number as factor
  (e.g. `"1.75"`) or one of the values `"xx-small"`, `"x-small"`, `"small"`, `"medium"`, `"large"`,`"x-large"`,`"xx-large"`.

The following are names of styles which control GtkSourceView appearance:

- `text`

  Default style of text.

- `selection`

  Style of selected text.

- `selection-unfocused`

  Style of selected text when the widget doesn't have input focus.

- `cursor`

  Text cursor style. Only the `foreground` attribute is used for this style.

- `secondary-cursor`

  Secondary cursor style (used in bidirectional text). Only the
  `foreground` attribute is used for this style. If this is not set
  while "cursor" is, then a color between text background and cursor colors is
  chosen, so it is enough to use "cursor" style only.

- `current-line`

  Current line style. Only the `background` attribute is used.

- `line-numbers`

  Text and background colors for the left margin, on which line
  numbers are drawn.

- `line-numbers-border`

  Background color for the border between the left gutter and
  textview.

- `current-line-number`

  Current line style for the left margin, on which the current
  line number is drawn. The `background`, `foreground`, and
  `bold` attributes may be used. `foreground` and `bold` were
  added in GtkSourceView 5.4. If `bold` is unset, it defaults
  to `true`.

- `bracket-match`

  Style to use for matching brackets.

- `bracket-mismatch`

  Style to use for mismatching brackets.

- `right-margin`

  Style to use for the right margin. The `foreground` attribute is used for
  drawing the vertical line. And the `background` attribute is used for the background on
  the right side of the margin. An alpha channel is applied to the two colors. For a light style
  scheme, a good color for both attributes is black. For a dark style scheme, white is a good
  choice.

- `draw-spaces`

  Style to use for drawing spaces (special symbols for a space, a tabulation, etc).
  Only the `foreground` attribute is used.

- `background-pattern`

  Style to use for drawing a background pattern, for example a
  grid. Only the `background` attribute is used.

## Default style schemes

The GtkSourceView team prefers to just keep a small number of style schemes
distributed with the library. To add a new style scheme in GtkSourceView
itself, the style scheme must be very popular, and ideally a
GtkSourceView-based application must use it by default.
