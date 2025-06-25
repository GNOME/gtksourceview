How to contribute to GtkSourceView
==================================

For general guidance, you can refer to the
[Development section of the GNOME handbook](https://handbook.gnome.org/development.html).

In short, the
[GtkSourceView repository](https://gitlab.gnome.org/GNOME/gtksourceview) is
hosted on the GNOME GitLab instance. You can fork it and then open a merge
request. Please follow the
[conventions for Git commit messages](https://handbook.gnome.org/development/commit-messages.html).

If you have write access to the Git repository, please don't push your commits
directly unless you have been given the green light to commit freely to
GtkSourceView. When in doubt assume you haven't ;-).

People who can commit freely to GtkSourceView:
* GtkSourceView maintainers (or sub-maintainers in their area of expertise),
  with of course discussion before doing important changes.
* GNOME "build sheriffs", to fix build issues.

C code conventions
------------------

You may encounter old code that doesn't follow all the following code
conventions, but for new code it is better to follow them, for consistency.

- Avoid trailing whitespace.

- Indent the C code with tabulations with a width of eight characters. However,
  alignment after matching the current scope should be done with spaces.

- All blocks should be surrounded by curly braces, even one-line blocks. The
  curly braces must be placed on their own lines. Like this:

```
if (foo)
{
	call_foo ();
}
else
{
	call_bar ();
}
```

Rationale: it spaces out the code, to have a better readability. And when
modifying a block of code, if it switches between one and two lines, we don't
need to add/remove the curly braces all the time.

- Follow the C99 standard but without `//`-style comments and with all
  declarations at the top, before statements. Some restrictions apply but
  relatively should match GTK and GLib.

- The files should not have modelines included. A `.editorconfig` file is
  provided for configuring indentation and many editors have support for using
  them.

- Do not be cheap about blank lines, spacing the code vertically helps
  readability. However never use two consecutive blank lines, there is really no
  need.

- As a general rule of thumb, follow the same coding style as the surrounding
  code.

See also:
- https://developer.gnome.org/programming-guidelines/stable/
- https://wiki.gnome.org/Projects/GTK%2B/BestPractices
- https://wiki.gnome.org/Projects/GLib/CompilerRequirements

Programming best-practices
--------------------------

GtkSourceView is a sizeable piece of software, developed over the years by
different people and GNOME technologies. Some parts of the code may be a little
old. So when editing the code, we should try to make it better, not worse.

Here are some general advices:

- Simplicity: the simpler code the better. Any trick that seem smart when you
  write it is going to bite you later when reading the code. In fact, the code
  is read far more often than it is written: for fixing a bug, adding a feature,
  or simply see how it is implemented. Making the code harder to read is a net
  loss.

- Avoid code duplication, make an effort to refactor common code into utility
  functions.

- Write self-documented code when possible: instead of writing comments, it is
  often possible to make the code self-documented by choosing good names for the
  variables, functions and types.

  Please avoid lots of one-letter variable names, it makes the code hard to
  understand. Don't be afraid to write long variable names. Also, a variable
  should be used for only one purpose.

  A good function name is one that explain clearly all what its code really
  does. There shouldn't be hidden features. If you can not find easily a good
  function name, you should probably split the function in smaller pieces. A
  function should do only one thing, but do it well.

- About comments:

  Do not write comments to state the obvious, for example avoid:
```
i = 0; /* assign 0 to i */
```

  Of course, write GTK-Doc comments to document the public API, especially the
  class descriptions. The class descriptions gives a nice overview when someone
  discovers the library.

  For a private class, it is useful to write a comment at the top describing in
  a few lines what the class does.

  Document well the data structures: the invariants (what is or should be
  always true about certain data fields); for a list, what is the element type;
  for a hash table, what are the key and value types; etc. It is more important
  to document the data structures than the functions, because when understanding
  well the data structures, the functions implementation should be for the most
  part obvious.

  When it isn't obvious, it is more important to explain *why* something is
  implemented in this way, not the *how*. You can deduce the *how* from the
  code, but not the *why*.

  If a non-trivial feature was previously implemented in a different way,
  it's useful to write a comment to describe in a few lines the previous
  implementation(s), and why it has been changed (for example to fix some
  problems). It permits to avoid repeating history, otherwise a new developer
  might wonder why a certain feature is implemented in "this complicated way"
  and not in "that simpler obvious way". For such things, a comment in the
  code has more chances to be read than an old commit message (especially if
  the code has been copied from one repository to another).

- Contribute below on the stack. Fix a problem at the right place, instead of
  writing hacks or heuristics to work around a bug or a lack of feature in an
  underlying library.

- Public API should have precondition guards using `g_return_if_fail()` or
  `g_return_val_if_fail()`. Optionally, you may do this before returning values
  from the function to help catch bugs earlier in the development cycle.

  Private functions (such as those with static) should use `g_assert()` to
  validate invariants. These are used in debug builds but can be compiled out of
  production/release builds.

- When transfering ownership of an object or struct, use `g_steal_pointer()` to
  make it clear when reading that ownership was transfered.

Language Specifications
-----------------------

The GtkSourceView project attempts to "upstream" a number of language
specifications as part of GtkSourceView itself. This is primarily to ease the
burden of community maintainership but also so that we can be relatively
certain that changes to the internals of GtkSourceView do not break your
favoriate language.

But there are many (175 at the time of writing) and we cannot possibly maintain
correctness for every language specification. Maintenance of these are at a
best-effort and rely on community members to contribute improvements to
languages they care about.

Additionally, there is a user burden to the files being bundled with
GtkSourceView as GtkSourceView must load them when loading the
GtkSourceLanguageManager. That has memory and CPU overhead parsing them when
the process loads. We must balance inclusion of every language against the
overhead placed on users.

To strike this balance, we ask that there are multiple contributors available
for the language specification you would like to add and that you'll be around
to help maintain it. Adding a language specification and then not returning to
maintain it does nobody any favors and simply transfers burden onto overworked
maintainers.

It must also match the license of GtkSourceView itself (LGPLv2.1+).
