dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

m4_define(gtksourceview_major_version,  1)
m4_define(gtksourceview_minor_version, 90)
m4_define(gtksourceview_micro_version,  0)
m4_define(gtksourceview_version, gtksourceview_major_version.gtksourceview_minor_version.gtksourceview_micro_version)

AC_INIT(gtksourceview, gtksourceview_version, http://bugzilla.gnome.org/enter_bug.cgi?product=gtksourceview)

AC_CONFIG_SRCDIR([gtksourceview/gtksourcebuffer.h])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([1.8 dist-bzip2 no-dist-gzip])

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS")

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_INSTALL
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])

GTK_DOC_CHECK([1.0])

# Check for headers and functions
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])
AC_CHECK_FUNCS([memset memmove strerror])

# Dependencies
GTK_REQUIRED_VERSION=2.8.0
LIBXML_REQUIRED_VERSION=2.5.0

AC_SUBST(GTK_REQUIRED_VERSION)
AC_SUBST(LIBXML_REQUIRED_VERSION)

# Pull glib-genmarshal & co.
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

GNOME_COMPILE_WARNINGS(yes)

AC_ARG_ENABLE(deprecations,
              [AC_HELP_STRING([--enable-deprecations],
                              [warn about deprecated usages [default=no]])],,
              [enable_deprecations=no])

if test "x$enable_deprecations" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS="\
-DG_DISABLE_DEPRECATED \
-DGDK_DISABLE_DEPRECATED \
-DGTK_DISABLE_DEPRECATED \
-DGDK_PIXBUF_DISABLE_DEPRECATED"
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

PKG_CHECK_MODULES(DEP, [
	gtk+-2.0 >= $GTK_REQUIRED_VERSION
	libxml-2.0 >= $LIBXML_REQUIRED_VERSION
])
AC_SUBST(DEP_CFLAGS)
AC_SUBST(DEP_LIBS)

# If the regex system is included in glib we should check that
BUILD_EGG_REGEX=true
AM_CONDITIONAL(BUILD_EGG_REGEX,$BUILD_EGG_REGEX)

# this macro is defined in acinclude.m4
GTK_SOURCE_VIEW_AC_REGEX

# Do we build optional tests?
AC_ARG_ENABLE(build_tests,
              AC_HELP_STRING([--disable-build-tests],
                             [disable building of the test programs]),
[case "${enableval}" in
    yes) build_tests=true ;;
    no)  build_tests=false ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --disable-build-tests) ;;
esac],
[build_tests=true])

msg_tests=no
AM_CONDITIONAL(BUILD_TESTS, false)
AM_CONDITIONAL(USE_GNOME_VFS, false)

if test x$build_tests = xtrue; then
	AM_CONDITIONAL(BUILD_TESTS, true)
	AC_DEFINE(BUILD_TESTS, [], [build test suite])
	msg_tests=yes
	PKG_CHECK_MODULES(TESTS, gnome-vfs-2.0,
			  AM_CONDITIONAL(USE_GNOME_VFS, true),
			  [PKG_CHECK_MODULES(TESTS, gthread-2.0)])
        # test-widget uses this to find lang files and gtksourcebuffer.c
        ABS_TOP_SRCDIR=`cd $srcdir && pwd`
        AC_SUBST(ABS_TOP_SRCDIR)
fi

# I18N stuff
GETTEXT_PACKAGE=gtksourceview-2.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

# Output files
AC_CONFIG_FILES([
gtksourceview-2.0.pc
Makefile
gtksourceview-zip
po/Makefile.in
gtksourceview/Makefile
gtksourceview/regex/Makefile
gtksourceview/regex/pcre/Makefile
gtksourceview/regex/glib/Makefile
gtksourceview/language-specs/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/version.xml
tests/Makefile
])

AC_OUTPUT


echo "

Configuration:

	Source code location:	${srcdir}
	Compiler:		${CC}
	build tests:		$msg_tests
        use system pcre:        $USE_SYSTEM_PCRE
"
