dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)

m4_define(gtksourceview_major_version, 2)
m4_define(gtksourceview_minor_version, 10)
m4_define(gtksourceview_micro_version, 5)
m4_define(gtksourceview_version, gtksourceview_major_version.gtksourceview_minor_version.gtksourceview_micro_version)

AC_INIT([gtksourceview],[gtksourceview_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=gtksourceview],[gtksourceview],[http://projects.gnome.org/gtksourceview/])

AC_CONFIG_SRCDIR([gtksourceview/gtksourcebuffer.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10.3 dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL

#Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Check for header files
AC_CHECK_HEADERS([unistd.h])

# Dependencies
GTK_REQUIRED_VERSION=2.12.0
LIBXML_REQUIRED_VERSION=2.5.0
GLADE_UI_REQUIRED=3.2

AC_SUBST(GTK_REQUIRED_VERSION)
AC_SUBST(LIBXML_REQUIRED_VERSION)

# Pull glib-genmarshal & co.
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

# Compile warnings
GNOME_COMPILE_WARNINGS([maximum])
#GNOME_MAINTAINER_MODE_DEFINES

PKG_CHECK_MODULES(DEP, [
	gio-2.0
	gtk+-2.0 >= $GTK_REQUIRED_VERSION
	libxml-2.0 >= $LIBXML_REQUIRED_VERSION
])


dnl Check to enable completion providers
AC_ARG_ENABLE(providers,
	    AS_HELP_STRING([--enable-completion-providers],
	    		   [Enable building completion providers (words)]),,
	    enable_providers=yes)

AM_CONDITIONAL(ENABLE_PROVIDERS, test "x$enable_providers" = "xyes")

dnl check for native osx
AC_MSG_CHECKING([for native Mac OS X])

gdk_windowing=`$PKG_CONFIG --variable=target gdk-2.0`

if test "$gdk_windowing" = "quartz"; then
	os_osx=yes
else
	os_osx=no
fi
AC_MSG_RESULT([$os_osx])
AM_CONDITIONAL(OS_OSX, test "$os_osx" = "yes")

if test "$os_osx" = "yes"; then
	AC_DEFINE([OS_OSX],[1],[Defined if os is Mac OSX])

	PKG_CHECK_MODULES(IGE_MAC, ige-mac-integration)
fi

# Check for Glade3
AC_ARG_ENABLE([glade-catalog],
	[AS_HELP_STRING([--enable-glade-catalog],
		[Install a glade catalog file [default=no]])],
	[glade_catalog=$enableval],
	[glade_catalog=no])

AM_CONDITIONAL([GLADE_CATALOG], [test "x$glade_catalog" = xyes])

AS_IF([test "x$glade_catalog" = xyes],
	[PKG_CHECK_EXISTS(GLADE, [gladeui-1.0 >= $GLADE_UI_REQUIRED])
	 AC_MSG_CHECKING([for Glade catalog dir])
	 GLADE_CATALOGDIR=`$PKG_CONFIG --variable=catalogdir gladeui-1.0 2>&AS_MESSAGE_LOG_FD`
         AC_MSG_RESULT([$GLADE_CATALOGDIR])
	 AC_SUBST(GLADE_CATALOGDIR)])

# i18N stuff
IT_PROG_INTLTOOL([0.40])

AC_SUBST([GETTEXT_PACKAGE], [gtksourceview-2.0])
AM_GNU_GETTEXT_VERSION([0.17])
AM_GLIB_GNU_GETTEXT
AC_DEFINE([GETTEXT_PACKAGE], [PACKAGE_TARNAME], [Define to the gettext package name.])

# Documentation
GTK_DOC_CHECK([1.11],[--flavour=no-tmpl])

# test-widget uses this to find lang files and gtksourcebuffer.c 	 
ABS_TOP_SRCDIR=`cd $srcdir && pwd` 	 
AC_SUBST(ABS_TOP_SRCDIR)

# Output files
AC_CONFIG_FILES([
gtksourceview-2.0.pc
Makefile
gtksourceview-zip
po/Makefile.in
gtksourceview/Makefile
gtksourceview/completion-providers/Makefile
gtksourceview/completion-providers/words/Makefile
gtksourceview/language-specs/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/version.xml
glade/Makefile
tests/Makefile
])

AC_OUTPUT


echo "

Configuration:

	Source code location:	${srcdir}
	Compiler:		${CC}
	Completion Providers:	${enable_providers}
	Glade Catalog:		${glade_catalog}
"
