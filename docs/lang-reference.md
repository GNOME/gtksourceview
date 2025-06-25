Title: Language Definition v2.0 Reference

# Language Definition v2.0 Reference

Reference to the GtkSourceView language definition file format

## Overview

This is an overview of the Language Definition XML format, describing the
meaning and usage of every element and attribute.  The formal definition is
stored in the RelaxNG schema file `language2.rng` which
should be installed on your system in the directory
`${PREFIX}/share/gtksourceview-${GSV_API_VERSION}/` (where
`${PREFIX}` can be `/usr/` or
`/usr/local/` if you have installed from source.

## Some advice

The easiest way to start a new language definition is to copy a preinstalled
language definition from a language that has similar constructs as the one you
want to write a specification for. You can copy and rename a file from the
systems directory to the local user one (create the directory if it doesn't
exist yet) and edit the file accordingly.

The important thing you need to change are the `id` and
`name` of the language and the metadata properties
`mimetypes` and `globs` in the language spec. These
should resemble your new language. It might be that your files do not have an
appropriate mimetype associated yet. You can either in that case leave it
empty, or add a new mimetype (see below).

If for some reason your language spec doesn't show up in an application like
gedit, it might be a good idea to start the application from a terminal window
and see if any errors/warnings appear for your language file. This usually
gives good clues what's wrong with the specification. Note that you need to
restart the application to take into account changes in a language definition
file.

## Conventions

It is better to follow the following conventions, especially if you want to
contribute upstream, and get your language definition file included in
GtkSourceView.

- Indentation: 2 spaces.
- Have the main context at the bottom.
- Use references to def.lang.
- Add an example in tests/syntax-highlighting/.
- LGPL v2.1+ license (copy/paste the license header from c.lang, for example).
- Add the e-mail address of the language definition file author(s).
- Refer to the CONTRIBUTING.md file for submitting your language definition
  file upstream. If the file is not included upstream, you can also
  add the language definition file to the GtkSourceView wiki, so
  users can easily find it.

## Tag `<language>`

The root element for Language Definition files.

Contained elements:

- `<metadata>` (optional)
- `<styles>` (optional)
- `<default-regex-options>` (optional)
- `<keyword-char-class>` (optional)
- `<definitions>` (mandatory)

### Attributes

- `id` (mandatory)

  Identifier for the description. This is used for
  external references and must be unique among language descriptions. It can
  contain a string of letters, digits, hyphens ("`-`") and
  underscores ("`_`").

- `name` or `_name` (mandatory)

  The name of the language presented to the user. With `_name`, the
  string is marked for translation (see the gettext documentation).

  The name should be marked for translation only if: (1) it contains a common
  English word, for example "C++ header" should be translated, but not "C++"
  or "XML". Or (2) if the name contains several words, because in some
  languages the word order should be different, for example "LLVM IR" or "RPM
  spec" should also be marked for translation.

- `version` (mandatory)

  The version of the XML format (currently "2.0").

- `section` or `_section` (optional)

  The category in which the language has to be grouped when presented to the user.
  With `_section` the string is marked for translation. Currently used
  categories in GtkSourceView are "Source", "Script", "Markup", "Scientific" and
  "Other", but it is possible to use arbitrary categories (while usually
  discouraged). The convention in GtkSourceView is to use `_section`.

- `hidden` (optional)

  It's a hint that the language should be "hidden" from user. For instance,
  def.lang has this flag, and a text editor should not present "default" as
  a syntax highlighting choice.

## Tag `<metadata>`

Contains optional metadata about the language definition.

Recognized elements are (all optional):

- `mimetypes`

  The semicolon-separated list of mimetypes associated to the language. See the
  [shared-mime-info](http://www.freedesktop.org/wiki/Specifications/shared-mime-info-spec/)
  freedesktop.org specification. A language definition file shipped by
  GtkSourceView needs to have a mimetype defined in the shared-mime-info
  database. If the language definition file is not shipped by GtkSourceView, you
  can also create the mimetype locally, usually in
  `~/.local/share/mime/packages/`.

- `globs`

  The semicolon-separated list of globs associated to the language.

- `line-comment-start`

  String used to create single-line comment in files of this type, e.g.
  "`#`" in shell scripts.
  It may be used in an editor to implement Comment/Uncomment functionality.

- `block-comment-start`

  String used to start block comment in files of this type, e.g. "`/*`" in C
  files.

- `block-comment-end`

  String used to end block comment in files of this type, e.g. "`*/`" in C
  files.

## Tag `<styles>`

Contains the definitions of every style used in the current language and
their association with predefined styles in GtkSourceView.

Contained elements: `<style>` (one or more).

## Tag `<style>`

Defines a style, associating its id with a user visible translatable
name and a default style.

Contained elements: none.

### Attributes

- `id` (mandatory)

  Identifier for the style. This is used in the current language
  to refer to this style and must be unique for the current document.
  It can contain a string of letters, digits,
  hyphens ("`-`") and underscores ("`_`").

- `name` or `_name` (mandatory)

  The user visible name for the style. With `_name` the string is
  marked for translation.

  The convention in GtkSourceView is to **never** mark those
  strings for translation because they are extremely rarely used in the UI of
  an application (for example it could be used by a GUI tool to edit or create
  style schemes, but it is not really useful beyond that), and because those
  strings would be hard to translate (as a result, if those strings are
  translated, in practice they would often be translated in a way that the
  user doesn't understand what it means; for example for the C language,
  "include" or "define" should not be translated, those are keywords of the
  programming language).

- `map-to` (optional)

  Used to map the style with a default style, to use colors and
  font properties defined for those default styles.
  The id of the default style has to be preceded by the id of the
  language where it is defined, separated with a colon "`:`".
  When omitted the style is not considered derived from any style and will
  not be highlighted until the user specifies a color scheme for this
  style.

## Tag `<keyword-char-class>`

Contains a regex character class used to redefine the customizable
word boundary delimiters `\%[` and `\%]`. This class is the set of character
that can be commonly found in a keyword.
If the element is omitted the two delimiters default to `\b`.

Contained elements: none.

## Tag `<default-regex-options>`

Used to set global options for how regular expressions are processed.

Contained elements: none.

### Attributes

- `case-sensitive` (optional)

  Set to `false` to make regular expressions ignore case.
  Defaults to `true`.

- `extended` (optional)

  Setting this to `true` makes the regular
  expression engine ignore spaces and comments. These comments start with
  "`#`" and continue to the end of the line.
  Defaults to `false`.

- `dupnames` (optional)

  Setting this to true allows one to repeat an identifier
  for capturing parentheses.  This is useful for some patterns that you
  know only one instance of a named subpattern can ever be matched.
  Defaults to `false`.

## Tag `<definitions>`

The element containing the real description of the syntax to be
highlighted. It contains one or more `<context>` element and an
arbitrary number of `<define-regex>` elements, interleaved.
It has no attributes.
Every contained element must have its `id` attribute set to an
identifier unique for the document. Exactly one of the contained
`<context>` element must have
the `id` attribute set to the `id` of the `<language>` root element,
representing the initial context for the highlighting, the one the engine
enters at the beginning of the highlighted file.

Contained elements:

- `<context>` (one or more)
- `<define-regex>` (zero or more)

## Tag `<define-regex>`

The syntax highlighting engine of GtkSourceView uses [struct@GLib.Regex],
which uses the PCRE library. See the
[Regular expression syntax](https://docs.gtk.org/glib/struct.Regex.html)
page in the GLib reference manual.

The `<define-regex>` tag defines a regular expression that can
be reused inside other regular expression, to avoid replicating common portions.
Those regular expressions are in the form `/regex/options`.  If there
are no options to be specified and you don't need to match the spaces at the
start and at the end of the regular expression, you can omit the slashes,
putting here only `regex`.  The possible options are those specified
above in the description of the `<default-regex-options>`
element. To disable a group of options, instead, you have to prepend an hyphen
`-` to them.  In GtkSourceView are also available some extensions to
the standard Perl style regular expressions:

- `\%[` and `\%]` are custom word boundaries, which can
  be redefined with the `<keyword-char-class>` (in contrast with `\b`);

- `\%{id}` will include the regular expression defined in another
  `<define-regex>` element with the specified id.

It is allowed to use any of the attributes from `<default-regex-options>` as attributes of this tag.

Contained elements: none.

### Attributes

- `id` (mandatory)

  Identifier for the regular expression. This is used
  for the inclusion of the defined regular expression and must be unique
  for the current document. It can contain a string of letters, digits,
  hyphens ("`-`") and underscores ("`_`").

## Tag `<context>`

This is the most important element when describing the syntax: the file to
be highlighted is partitioned in contexts representing the portions to be
colored differently. Contexts can also contain other contexts.
There are different kind of context elements: simple contexts, container
contexts, sub-pattern contexts, reference contexts and keyword contexts.

Context classes can be enabled or disabled for some contexts, with the
`class` and `class-disabled` attributes. You can create
your own context classes in custom language definition files. Here are the
default context classes:

- **comment**: the context delimits a comment;
- **no-spell-check**: the context's content should not be spell checked;
- **path**: the context delimits a path to a file;
- **string**: the context delimits a string.

## Simple contexts

They contain a mandatory `<match>` element and an optional
`<include>` element. The context will span over the strings
matched by the regular expression contained in the `<match>`
element. In the `<include>` element you
can only put sub-pattern contexts.

Contained elements:

- `<match>` (mandatory)
- `<include>` (optional)

### Attributes

- `id` (optional)

  A unique identifier for the context, used in references to the context. It
  can contain a string of letters, digits, hyphens ("`-`") and
  underscores ("`_`").

- `style-ref` (optional)

  Highlighting style for this context. Value of this attribute
  may be id of a style defined in current lang file, or id of a style
  defined in other files prefixed with corresponding language id,
  e.g. "def:comment".

- `extend-parent` (optional)

  A boolean value telling the engine whether the context has higher
  priority than the end of its parent. If not specified it defaults to
  `true`.

- `end-parent` (optional)

  A boolean value telling the engine whether the context terminates parent context.
  If not specified it defaults to `false`.

- `first-line-only` (optional)

  A boolean value telling the engine whether the context can occur only
  on the first line of buffer. If not specified it defaults to `false`.

- `once-only` (optional)

  A boolean value telling the engine whether the context can occur only
  once in its parent. If not specified it defaults to `false`.

- `class` (optional)

  A space-separated list of context classes to enable.

- `class-disabled` (optional)

  A space-separated list of context classes to disable.

## Container contexts

They contain a `<start>` element and an optional
`<end>`. They respectively contain the regular
expression that makes the engine enter in the context and the terminating one.
In the optional `<include>` element you can put contained
contexts of every type (simple, container, sub-pattern or reference).
If the `<start>` element is omitted, then the
`id` attribute and the `<include>` become
mandatory (the context can only be used as a container to include
its children).

Contained elements:

- `<start>` (optional)
- `<end>` (optional)
- `<include>` (optional)

### Attributes

- `id` (mandatory only if `<start>` not present)

  A unique identifier for the context, used in references to the context. It
  can contain a string of letters, digits, hyphens ("`-`") and
  underscores ("`_`").

- `style-ref` (optional)

  Highlighting style for this context. Value of this attribute
  may be id of a style defined in current lang file, or id of a style
  defined in other files prefixed with corresponding language id,
  e.g. "def:comment".

- `style-inside` (optional)

  If this attribute is `true`, then the highlighting style will
  be applied to the area between start and end matches; otherwise
  whole context will be highlighted.

- `extend-parent` (optional)

  A boolean value telling the engine whether the context has a higher
  priority than the end of its parent. If not specified it defaults to
  `true`.

- `end-at-line-end` (optional)

  A boolean value telling the engine whether the context must be forced
  to end at the end of the line. If not specified it defaults to
  `false`.

- `end-parent` (optional)

  A boolean value telling the engine whether the context terminates parent context
  when it ends.
  If not specified it defaults to `false`.

- `first-line-only` (optional)

  A boolean value telling the engine whether the context can start only
  on the first line of buffer. If not specified it defaults to `false`.

- `once-only` (optional)

  A boolean value telling the engine whether the context can occur only
  once in its parent. For a container context, it means that
  **each** included context can occur once.
  If not specified it defaults to `false`.

- `class` (optional)

  A space-separated list of context classes to enable.

- `class-disabled` (optional)

  A space-separated list of context classes to disable.

## Sub-pattern contexts

They refer to a group in a regular expression of the parent context, so it
is possible to highlight differently only a portion of the matched regular
expression.

Contained elements: none.

### Attributes

- `id` (optional)

  A unique identifier for the context. It can contain a string of letters,
  digits, hyphens ("`-`") and underscores ("`_`").

- `sub-pattern` (mandatory)

  The sub-pattern to which we refer. "0" means the whole expression, "1" the
  first group, "2" the second one, etc. If named sub-patterns are used you can
  also use the name.

- `where` (mandatory only in container contexts)

  Can be "`start`" or "`end`". It has to be used
  only if the parent is a container context to specify whether the
  sub-pattern is in the regular
  expression of the `<start>` or the `<end>`
  element. In simple contexts it must be omitted.

- `class` (optional)

  A space-separated list of context classes to enable.

- `class-disabled` (optional)

  A space-separated list of context classes to disable.

## Reference contexts

Used to include a previously defined context.

Contained elements: none.

### Attributes

- `ref` (mandatory)

  The id of the context to be included. A colon followed by an asterisk
  ("`:*`") at the end of the id means that the parent should include
  every child of the specified context, instead of the context itself.
  Prepending the id of another language to the id of the context (separated
  with a colon "`:`") is possible to include contexts defined inside such
  external language.

- `style-ref` (optional)

  Style in included context may be overridden by using this attribute.
  Its value is id of the style to be used instead of style specified
  in the referenced context.

- `ignore-style` (optional)

  If this attribute is `true` then the referenced context will not
  be highlighted. It does not affect child contexts and their styles.

- `original` (optional)

  If this attribute is `true`, it references the original context, if it
  has been replaced with the `<replace>` tag.

## Keyword contexts

They contain a list of `<keyword>` and matches every keyword
listed. You can also put a `<prefix>` and/or a
`<suffix>` common to every keyword.
Note that keywords are matched in the order they are listed, so if you
have both a keyword `foo` and a keyword `foobar`, you should always list
foobar before foo, or it will never be matched.

Contained elements:

- `<prefix>` (optional)
- `<suffix>` (optional)
- `<keyword>` (one or more)

The attributes are the same used in simple contexts. If the
`once-only` attribute is `true`, it means that
**each** keyword can occur once.

## Tag `<include>`

Contains the list of context contained in the current
`<context>`.

Contained elements:

- `<context>` (one or more)
- `<define-regex>` (zero or more)

## Tag `<match>`

Contains the regular expression for the current simple context.
The expression is in the same form used in
`<define-regex>` elements.
It is allowed to use any of the attributes from
`<default-regex-options>` as attributes of this tag.

Contained elements: none.

## Tag `<start>`

Contains the starting regular expression for the current container context.
The expression is in the same form used in `<define-regex>`
elements.
It is allowed to use any of the attributes from
`<default-regex-options>` as attributes of this tag.

Contained elements: none.

## Tag `<end>`

Contains the terminating regular expression for the current container
context. The expression is in the same form used in `<define-regex>`
elements, with an extension: `\%{sub-pattern@start}` will be
substituted with the string matched in the corresponding sub-pattern
(can be a number or a name if named sub-patterns are used) in the
preceding `<start>` element. For instance you could
implement shell-style here-documents with this code:

```xml
<context id="here-doc">
    <start>&lt;&lt;\s*(\S+)$</start>
    <end>^\%{1@start}$</end>
</context>
```

It is also possible to use any of the attributes from
`<default-regex-options>` as attributes of this tag.

Contained elements: none.

## Tag `<keyword>`

Contains a keyword to be matched by the current context. The keyword is a
regular expression in the form used in `<define-regex>`.

Contained elements: none.

## Tag `<prefix>`

Contains a prefix common to all of the following keywords in the current
context. The prefix is a regular expression in the form used in
`<define-regex>`. If not specified it defaults to `\%[`.

Contained elements: none.

## Tag `<suffix>`

Contains a suffix common to all of the following keywords in the current
context. The suffix is a regular expression in the form used in
`<define-regex>`. If not specified it defaults to `\%]`.

Contained elements: none.

## Tag `<replace>`

The replace tag allows you to change one context so it functions as
another context.  For example, in the `html.lang` definition,
there are a few references to a null context with id
"`embedded-lang-hook`".  In `php.lang`, that context is
replaced like this: `<replace id="html:embedded-lang-hook" ref="php-block">`,
so that php blocks are recognized within the
`html:html` context at the points where the
`embedded-lang-hook` context appears.

Contained elements: none.

### Attributes

- `id` (mandatory)

  The id of the context to replace. Ex: `id="html:embedded-lang-hook"`

- `ref` (mandatory)

  The id of the context to put in place of the context being replaced. Ex: `ref="php-block"`
