dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)

# Initialization and versioning
AC_INIT(gtksourceview, 1.90.0, http://bugzilla.gnome.org/enter_bug.cgi?product=gtksourceview)
AC_CONFIG_SRCDIR([gtksourceview/gtksourcebuffer.h])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS")

AM_MAINTAINER_MODE

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_PROG_INTLTOOL([0.31])

# Check for headers and functions
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])
AC_CHECK_FUNCS([memset memmove strerror])

# Dependencies
GTK_REQUIRED_VERSION=2.8.0
LIBXML_REQUIRED_VERSION=2.5.0
GNOME_PRINT_REQUIRED_VERSION=2.8.0

AC_SUBST(GTK_REQUIRED_VERSION)
AC_SUBST(LIBXML_REQUIRED_VERSION)
AC_SUBST(GNOME_PRINT_REQUIRED_VERSION)

# Pull glib-genmarshal & co.
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

# Environment
GNOME_COMMON_INIT
GNOME_COMPILE_WARNINGS(maximum)
if test x$MAINT = x; then
    DEPRECATED_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
else
    DEPRECATED_FLAGS=""
fi
AC_SUBST(DEPRECATED_FLAGS)

PKG_CHECK_MODULES(DEP,
	gtk+-2.0 >= $GTK_REQUIRED_VERSION
	libxml-2.0 >= $LIBXML_REQUIRED_VERSION)
AC_SUBST(DEP_CFLAGS)
AC_SUBST(DEP_LIBS)

# If the regex system is included in glib we should check that
BUILD_EGG_REGEX=true
AM_CONDITIONAL(BUILD_EGG_REGEX,$BUILD_EGG_REGEX)
# this macro is defined in acinclude.m4
GTK_SOURCE_VIEW_AC_PCRE

# Check for gnomeprint support
AC_ARG_ENABLE(gnomeprint, AC_HELP_STRING([--disable-gnomeprint],
		 [disable gnomeprint support. Do this only if you are using gtksourceview in a custom application.]),
[case "${enableval}" in
  yes) enable_gnomeprint=true ;;
  no)  enable_gnomeprint=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-gnomeprint) ;;
esac],
[enable_gnomeprint=true]) dnl default
# it doesn't work now
enable_gnomeprint=false

msg_gnomeprint=no
AM_CONDITIONAL(HAVE_GNOMEPRINT, false)

if test "$enable_gnomeprint" = "true"; then
	PKG_CHECK_MODULES(GNOMEPRINT,
			libgnomeprint-2.2 >= $GNOME_PRINT_REQUIRED_VERSION,
			[AM_CONDITIONAL(HAVE_GNOMEPRINT, true)
			 AC_DEFINE(HAVE_GNOMEPRINT, [], [using gnomeprint])
			 msg_gnomeprint=yes],
			[AM_CONDITIONAL(HAVE_GNOMEPRINT, false)])
	AC_SUBST(GNOMEPRINT_CFLAGS)
	AC_SUBST(GNOMEPRINT_LIBS)
fi

# Do we build optional tests?
AC_ARG_ENABLE(build_tests,AC_HELP_STRING([--disable-build-tests],
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
ALL_LINGUAS="ar az be bg bn bs ca cs cy da de el en_CA en_GB es et eu fa fi fr ga gl gu he hi hr hu id it ja ko lt mk ml mn ms nb ne nl no pa pl pt pt_BR ro ru rw sk sq sr sr@Latn sv ta th tr uk vi xh zh_CN zh_TW"
GETTEXT_PACKAGE=gtksourceview-2.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

# gtk-doc
GTK_DOC_CHECK([1.0])

# Output files
AC_CONFIG_FILES([
gtksourceview-2.0.pc
Makefile
gtksourceview-zip
po/Makefile.in
gtksourceview/Makefile
gtksourceview/libegg/Makefile
gtksourceview/libegg/regex/Makefile
gtksourceview/libegg/regex/pcre/Makefile
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
	gnome-print support:	$msg_gnomeprint
	build tests:		$msg_tests
        use system pcre:        $USE_SYSTEM_PCRE
"
