dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)

m4_define(gtksourceview_major_version, 3)
m4_define(gtksourceview_minor_version, 22)
m4_define(gtksourceview_micro_version, 2)
m4_define(gtksourceview_version, gtksourceview_major_version.gtksourceview_minor_version.gtksourceview_micro_version)

AC_INIT([gtksourceview],
        [gtksourceview_version],
	[http://bugzilla.gnome.org/enter_bug.cgi?product=gtksourceview],
	[gtksourceview],
	[https://wiki.gnome.org/Projects/GtkSourceView])

AX_IS_RELEASE([git-directory])

GTK_SOURCE_MAJOR_VERSION=gtksourceview_major_version
GTK_SOURCE_MINOR_VERSION=gtksourceview_minor_version
GTK_SOURCE_MICRO_VERSION=gtksourceview_micro_version
AC_SUBST(GTK_SOURCE_MAJOR_VERSION)
AC_SUBST(GTK_SOURCE_MINOR_VERSION)
AC_SUBST(GTK_SOURCE_MICRO_VERSION)

# Libtool versioning
#
# For development releases, keep the same numbers.
#
# For stable releases, apply the following algorithm step by step:
# 1. If the library source code has changed at all since the last
#    update, then increment REVISION.
# 2. If any exported functions or data have been added, removed, or
#    changed since the last update, increment CURRENT and set REVISION
#    to 0.
# 3. If any exported functions or data have been added since the last
#    public release, increment AGE.
# 4. If any exported functions or data have been removed since the last
#    public release, set AGE to 0.

GSV_LT_CURRENT=8
GSV_LT_REVISION=0
GSV_LT_AGE=7
GSV_LT_VERSION="$GSV_LT_CURRENT:$GSV_LT_REVISION:$GSV_LT_AGE"
AC_SUBST(GSV_LT_VERSION)

# Dependencies
glib_req=2.48
gtk_req=3.20
libxml_req=2.6
gladeui_req=3.9

AC_CONFIG_SRCDIR([gtksourceview/gtksourcebuffer.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# gnu strictness to generate the INSTALL file
AM_INIT_AUTOMAKE([1.13 tar-ustar dist-xz no-dist-gzip -Wno-portability gnu])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL

AX_REQUIRE_DEFINED([PKG_PROG_PKG_CONFIG])
AX_REQUIRE_DEFINED([PKG_INSTALLDIR])
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Check for header files
AC_CHECK_HEADERS([unistd.h])

# Pull glib-mkenums & co.
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
AC_PATH_PROG(ITSTOOL, itstool)

# Compile warnings
AX_REQUIRE_DEFINED([AX_COMPILER_FLAGS])
AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

AC_ARG_ENABLE(deprecations,
              [AS_HELP_STRING([--disable-deprecations],
                              [remove deprecated symbols from the header files])],
              [enable_deprecations=$enableval],
              [enable_deprecations="yes"])

if test "$enable_deprecations" = "no"; then
	DISABLE_DEPRECATED_CFLAGS="\
-DG_DISABLE_DEPRECATED \
-DGDK_DISABLE_DEPRECATED \
-DGTK_DISABLE_DEPRECATED \
-DGDK_PIXBUF_DISABLE_DEPRECATED"

	AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

# Symbol visibility handling
HIDDEN_VISIBILITY_CFLAGS=""
case "$host" in
    *-*-mingw*)
        dnl on mingw32 we do -fvisibility=hidden and __declspec(dllexport)
        AC_DEFINE([_GTK_SOURCE_EXTERN], [__attribute__((visibility("default"))) __declspec(dllexport) extern],
                  [defines how to decorate public symbols while building])
        HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
        ;;
    *)
        dnl on other compilers, check if we can do -fvisibility=hidden
        SAVED_CFLAGS="${CFLAGS}"
        CFLAGS="-fvisibility=hidden"
        AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
        AC_TRY_COMPILE([], [int main (void) { return 0; }],
                       AC_MSG_RESULT(yes)
                       enable_fvisibility_hidden=yes,
                       AC_MSG_RESULT(no)
                       enable_fvisibility_hidden=no)
        CFLAGS="${SAVED_CFLAGS}"

        AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
            AC_DEFINE([_GTK_SOURCE_EXTERN], [__attribute__((visibility("default"))) extern],
                      [defines how to decorate public symbols while building])
            HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
        ])
        ;;
esac
AC_SUBST(HIDDEN_VISIBILITY_CFLAGS)

AX_REQUIRE_DEFINED([AX_PKG_CHECK_MODULES])
AX_PKG_CHECK_MODULES([DEP],
		     [glib-2.0 >= $glib_req  gio-2.0 >= $glib_req  gtk+-3.0 >= $gtk_req],
		     [libxml-2.0 >= $libxml_req])

dnl check for native osx
AC_MSG_CHECKING([for native Mac OS X])
AX_REQUIRE_DEFINED([GTK_CHECK_BACKEND])
GTK_CHECK_BACKEND([quartz], [3.0.0], [os_osx=yes], [os_osx=no])
AC_MSG_RESULT([$os_osx])
AM_CONDITIONAL([OS_OSX], [ test "$os_osx" = "yes" ])

if test "$os_osx" = "yes"; then
	AC_DEFINE([OS_OSX], [1], [Defined if os is Mac OSX])
fi

# Check for Glade3
AC_ARG_ENABLE([glade-catalog],
	[AS_HELP_STRING([--enable-glade-catalog],
		[Install a glade catalog file [default=no]])],
	[glade_catalog=$enableval],
	[glade_catalog=no])

AM_CONDITIONAL([GLADE_CATALOG], [ test "$glade_catalog" = "yes" ])

AS_IF([ test "$glade_catalog" = "yes" ],
	[PKG_CHECK_MODULES(GLADE, [gladeui-2.0 >= $gladeui_req])
	 AC_MSG_CHECKING([whether to install Glade catalog])
	 GLADE_CATALOG_DIR=`$PKG_CONFIG --variable=catalogdir gladeui-2.0 2>&AS_MESSAGE_LOG_FD`
	 AC_MSG_RESULT([$GLADE_CATALOG_DIR])
	 AC_SUBST(GLADE_CATALOG_DIR)])

# i18n
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.4])

AS_IF([test "$USE_NLS" = "yes"],
      [AC_DEFINE([ENABLE_NLS], [1], [Defined if native language support should be enabled.])])

AC_SUBST([GETTEXT_PACKAGE], [gtksourceview-3.0])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Define to the gettext package name.])

# Documentation
AX_REQUIRE_DEFINED([GTK_DOC_CHECK])
GTK_DOC_CHECK([1.25], [--flavour=no-tmpl])

# Installed Tests for gnome continuous integration
AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])

AM_CONDITIONAL(INSTALLED_TESTS, test "x$enable_installed_tests" = "xyes")

# Introspection
AX_REQUIRE_DEFINED([GOBJECT_INTROSPECTION_CHECK])
GOBJECT_INTROSPECTION_CHECK([1.42.0])

# Code coverage
AX_REQUIRE_DEFINED([AX_CODE_COVERAGE])
AX_CODE_COVERAGE()

# Valgrind check
AX_REQUIRE_DEFINED([AX_VALGRIND_CHECK])
AX_VALGRIND_CHECK

# Vala
AX_REQUIRE_DEFINED([VAPIGEN_CHECK])
VAPIGEN_CHECK

# test-widget uses this to find lang files and gtksourcebuffer.c
ABS_TOP_SRCDIR=`cd $srcdir && pwd`
AC_SUBST(ABS_TOP_SRCDIR)

# Output files
AC_CONFIG_FILES([
gtksourceview-3.0.pc
Makefile
config.h.win32
build/Makefile
build/win32/Makefile
build/win32/vs9/Makefile
build/win32/vs9/gtksourceview-gen-srcs.vsprops
build/win32/vs10/Makefile
build/win32/vs10/gtksourceview-gen-srcs.props
build/win32/vs11/Makefile
build/win32/vs12/Makefile
build/win32/vs14/Makefile
data/Makefile
data/glade/Makefile
data/language-specs/Makefile
data/styles/Makefile
docs/Makefile
docs/reference/Makefile
gtksourceview/Makefile
gtksourceview/gtksourceversion.h
gtksourceview/completion-providers/Makefile
gtksourceview/completion-providers/words/Makefile
po/Makefile.in
tests/Makefile
testsuite/Makefile
])

AC_OUTPUT


echo "

Configuration:

	Source code location:	${srcdir}
	Compiler:		${CC}
	Glade Catalog:		${glade_catalog}
	Documentation:		${enable_gtk_doc}
	GObject introspection:	${found_introspection}
	Vala:			${enable_vala}
	Code coverage:		${enable_code_coverage}
	Valgrind check:		${enable_valgrind}
	Installed tests:	${enable_installed_tests}
"
