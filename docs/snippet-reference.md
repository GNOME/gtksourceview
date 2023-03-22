Title: Snippet Definition Reference

# Snippet Definition Reference

Reference to the GtkSourceView snippet definition file format

## Overview

This is an overview of the Snippet Definition XML format, describing the
meaning and usage of every element and attribute.  The formal definition is
stored in the RelaxNG schema file `snippets.rng` which
should be installed on your system in the directory `${PREFIX}/share/gtksourceview-5/` (where
`${PREFIX}` can be `/usr/` or `/usr/local/` if you have installed from source).

The toplevel tag in a snippet file is `<snippets>`.
It has the following attributes:

- `_group`

  The group for the snippet. This is purely used to group snippets together
  in user-interfaces.

  Within the snippets tag is one or more `<snippets>` elements.
  It has the following attributes:

- `_name`

  The name of the snippet. This may be displayed in the user interface such as
  in a snippet editor or completion providers. An attempt will be made to translate
  it by GtkSourceView.

- `trigger`

  The word trigger of the snippet. If the user types this word and hits Tab,
  the snippet will be inserted.

- `_description`

  The description of the snippet. This may be displayed in the user interface
  such as in a snippet editor or completion providers. An attempt will be made
  to translate it by GtkSourceView.

  Within the snippet tag is one or more `<text>` elements.
  It has the following attributes:

- `languages`

  A semicolon separated list of GtkSourceView language identifiers for which this
  text should be used when inserting the snippet. Defining this on the
  `<text>` tag allows a snippet to have multiple variants based
  on the programming language.

- `CDATA` Within the `<text>` tag should be a single

  `<![CDATA[]]>` tag containing the text for the snippet
  between the []. You do not need to use CDATA if the text does not have any
  embedded characters that will conflict with XML.

## Snippet Text Format

GtkSourceView's snippet text format is largely based upon other snippet
implementations that have gained popularity. Since there are so many, it likely
differs in some small ways from what you may have used previously.

## Focus Positions

Focus positions allow the user to move through chunks within the snippet
that are meant to be edited by the user. Each subsequent "Tab" by the user will
advance them to the next focus position until all have been exhausted.

To set a focus position in your snippet, use a dollar sign followed by
curly braces with the focus position number inside like `${1}` or
`${2}`. The user can also use Shift+Tab to move to a previous tab
stop.

The special `$0` tab stop is used to place the cursor after
all focus positions have been exhausted. If no focus position was provided, the
cursor will be placed there automatically.

You can also set a default value for a focus position by appending a colon
and the initial value such as `${2:default_value}`. You can even
reference other chunks such as `${3:$2_$1}` to join the contents of
`$2`, an underbar `_`, and `$1`.

## Variable Expansion

When a snippet is expanded into the #GtkSourceView, chunks may reference
a number of built-in or application provided variables. Applications may
add additional variables with either [method@SnippetContext.set_constant]
(for things that do not change) or [method@SnippetContext.set_variable]
for values that may change.

Snippet chunks can reference a variable anywhere by using a dollar sign
followed by the variable name such as `$variable`.

You can also reference another focus position's value by using their focus
position number as the variable such as `$1`.

To post-process a variables value, enclose the variable in curly-brackets
and use the pipe operator to denote the post-processing function such as
`${$1|capitalize}`.

## Predefined Variables

A number of predefined variables are provided by GtkSourceView which can be extended by applications.

- `$CURRENT_YEAR` The current 4-digit year
- `$CURRENT_YEAR_SHORT` The last 2 digits of the current year
- `$CURRENT_MONTH` The current month as a number from 01-12
- `$CURRENT_MONTH_NAME` The full month name such as "August" in the current locale
- `$CURRENT_MONTH_NAME_SHORT` The short month name such as "Aug" in the current locale
- `$CURRENT_DATE` The current day of the month from 1-31
- `$CURRENT_DAY_NAME` The current day such as "Friday" in the current locale
- `$CURRENT_DAY_NAME_SHORT` The current day using the shortened name such as "Fri" in the current locale
- `$CURRENT_HOUR` The current hour in 24-hour format
- `$CURRENT_MINUTE` The current minute within the hour
- `$CURRENT_SECOND` The current second within the minute
- `$CURRENT_SECONDS_UNIX` The number of seconds since the UNIX epoch
- `$NAME_SHORT` The users login user name (See [func@GLib.get_user_name])
- `$NAME` The users full name, if known (See `g_get_full_name()`)
- `$TM_CURRENT_LINE` The contents of the current line
- `$TM_LINE_INDEX` The zero-index based line number
- `$TM_LINE_NUMBER` The one-index based line number

# Post-Processing

By appending a pipe to a variable within curly braces, you can post-process
the variable. A number of built-in functions are available for processing.
For example `${$1|stripsuffix|functify}`.

- `lower`
- `upper`
- `captialize`
- `uncapitalize`
- `html`
- `camelize`
- `functify`
- `namespace`
- `class`
- `instance`
- `space`
- `stripsuffix`
- `slash_to_dots`
- `descend_path`

## Default snippets

The GtkSourceView team prefers to just keep a small number of snippets
distributed with the library. To add a new snippet in GtkSourceView itself,
the snippet must be very popular, and ideally a GtkSourceView-based application
must use it by default.
