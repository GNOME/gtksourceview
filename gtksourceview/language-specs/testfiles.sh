#!/bin/sh

# Running this file will create "testdir" directory with bunch
# of files inside. Open them all to see if something broke.
# Kind of smoke test.

# Langs covered here:
# changelog.lang c.lang cpp.lang desktop.lang diff.lang dtd.lang
# gap.lang gtkrc.lang html.lang ini.lang latex.lang m4.lang
# makefile.lang ms.lang perl.lang po.lang python.lang sh.lang
# texinfo.lang xml.lang yacc.lang libtool.lang pkgconfig.lang
# objc.lang chdr.lang testv1.lang

dir="testdir"
mkdir -p $dir/

cat > $dir/file.f <<EOFEOF
! gtk-source-lang: fortran
c comment
if .TRUE.
.FALSE.
endif
Numbers: 1234 o'10176' b'10101' z'23FF'
12. .12 12.12 1.23e+09 1.23D+09
EOFEOF

cat > $dir/file.frt <<EOFEOF
\ gtk-source-lang: forth
-- comment
(* comment!
  here still comment
*)
NEEDS something
IF 0 THEN ENDIF
Numbers: 1234 $233 #345325 %01000
EOFEOF

cat > $dir/file.testv1 <<EOFEOF
// gtk-source-lang: testv1
// comment
/* comment! */
"A string"
'And a string too'
bambom - keyword, Others
bumbam - keyword, Others2
kwkw - keyword, Keyword
Numbers: 0.1 1234 0233
EOFEOF

cat > $dir/file.cc <<EOFEOF
#include <iostream>

class A : B {
public:
    A();
private:
    foobar() const;
};
EOFEOF

cat > $dir/file.c <<EOFEOF
// Comment

#include <stdio.h>

int main (void)
{
    int a = 0x89;
    int b = 089;
    int c = 89.;
    int d = 'a';
    printf ("Hello %s!\n", "world");
    return 0;
}
EOFEOF

cat > $dir/file.m <<EOFEOF
#import <stdio.h>
#import <Object.h>
@interface Lalala : Object
- (BOOL) sayHello;
@end

@implementation Lalala : Object
- (BOOL) sayHello
{
  printf ("Hello there!\n");
  return YES;
}
@end

int main (void)
{
  Lalala *obj = [[Lalala alloc] init];
  [obj sayHello];
  [obj free];
  return 0;
}
EOFEOF

cat > $dir/file.h <<EOFEOF
/* A C header damn it */
#include <foo.h>
#import <Object.h>

@interface Lalala : Object
- (void) sayHello;
@end

class Boo {
  void hello ();
};
EOFEOF

cat > $dir/ChangeLog <<EOFEOF
= Release =

2006-12-10  Kristian Rietveld  <kris@gtk.org>

	* gtk/gtkcellrenderertext.c (gtk_cell_renderer_text_focus_out_event):
	cancel editing (ie. don't accept changes) when the entry loses
	focus. (Fixes #164494, reported by Chris Rouch).

2006-12-10  Matthias Clasen  <mclasen@redhat.com>

	* configure.in: Correct a misapplied patch.
EOFEOF

cat > $dir/file.g <<EOFEOF
for i in [1..10] do
  Print("blah blah blah\n");
od;
EOFEOF

cat > $dir/file.html <<EOFEOF
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<!-- Comment -->
<html>
  <head>
    <title>Hi there!</title>
    <meta http-equiv="Content-Type" content="text/html; charset=us-ascii">
    <style type="text/css">
      a.summary-letter {text-decoration: none}
      pre.display {font-family: serif}
      pre.format {font-family: serif}
    </style>
  </head>
  <body lang="en" bgcolor="#FFFFFF" text="#000000" link="#0000FF"
        vlink="#800080" alink="#FF0000">
    Hi there!
  </body>
</html>
EOFEOF

cat > $dir/file.tex <<EOFEOF
\documentclass{article}
\begin{document}
Hi there!
\end{document}
EOFEOF

cat > $dir/file.m4 <<EOFEOF
dnl an m4 file
AC_DEFINE([foo],[echo "Hi there!"])
AC_CHECK_FUNC([foo],[yes=yes],[yes=no])
foo()
EOFEOF

cat > $dir/file.sh <<EOFEOF
#!/bin/bash
echo "Hi there!"
EOFEOF

cat > $dir/Makefile <<EOFEOF
all: foo bar
foo: hello ; \$(MAKE) bar
bar:
	echo "Hello world!"
EOFEOF

cat > $dir/file.py <<EOFEOF
import sys
from sys import *
class Hello(object):
    def __init__(self):
        object.__init__(self)
    def hello(self):
        print >> sys.stderr, "Hi there!"
    None, True, False
Hello().hello()
EOFEOF

cat > $dir/file.xml <<EOFEOF
<?xml version="1.0" encoding="UTF-8"?>
<foolala version='8.987'>
  <bar>momomomo</bar><baz a="b"/>
</foolala>
EOFEOF

cat > $dir/file.y <<EOFEOF
%{
#include <stdio.h>
#define FOO_BAR(x,y) printf ("x, y")
%}

%name-prefix="foolala"
%error-verbose
%lex-param   {FooLaLa *lala}
%parse-param {FooLaLa *lala}
/* %expect 1 */

%union {
    int ival;
    const char *str;
}

%token <str> ATOKEN
%token <str> ATOKEN2

%type <ival> program stmt
%type <str> if_stmt

%token IF THEN ELSE ELIF FI
%token WHILE DO OD FOR IN
%token CONTINUE BREAK RETURN
%token EQ NEQ LE GE
%token AND OR NOT
%token UMINUS
%token TWODOTS

%left '-' '+'
%left '*' '/'
%left '%'
%left EQ NEQ '<' '>' GE LE
%left OR
%left AND
%left NOT
%left '#'
%left UMINUS

%%

script:   program           { _ms_parser_set_top_node (parser, \$1); }
;

program:  stmt_or_error             { \$\$ = node_list_add (parser, NULL, \$1); }
        | program stmt_or_error     { \$\$ = node_list_add (parser, MS_NODE_LIST (\$1), \$2); }
;

stmt_or_error:
          error ';'         { \$\$ = NULL; }
        | stmt ';'          { \$\$ = $1; }
;

variable: IDENTIFIER                        { \$\$ = node_var (parser, \$1); }
;

%%
EOFEOF

cat > $dir/file.desktop <<EOFEOF
[Desktop Entry]
Encoding=UTF-8
_Name=medit
_Comment=Text editor
Exec=medit %F
# blah blah blah
Terminal=false
Type=Application
StartupNotify=true
MimeType=text/plain;
Icon=medit.png
Categories=Application;Utility;TextEditor;
EOFEOF

cat > $dir/file.diff <<EOFEOF
diff -r 231ed68760a0 moo/moofileview/moofileview.c
--- a/moo/moofileview/moofileview.c     Wed Dec 20 21:08:14 2006 -0600
+++ b/moo/moofileview/moofileview.c     Wed Dec 20 20:33:06 2006 -0600
@@ -1407,7 +1413,7 @@ create_toolbar (MooFileView *fileview)

     gtk_toolbar_set_tooltips (toolbar, TRUE);
     gtk_toolbar_set_style (toolbar, GTK_TOOLBAR_ICONS);
-    gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_MENU);
+    gtk_toolbar_set_icon_size (toolbar, TOOLBAR_ICON_SIZE);

     _moo_file_view_setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoUp", "go-up");
     _moo_file_view_setup_button_drag_dest (fileview, "MooFileView/Toolbar/GoBack", "go-back");
EOFEOF

cat > $dir/gtkrc <<EOFEOF
# -- THEME AUTO-WRITTEN DO NOT EDIT
include "/usr/share/themes/Clearlooks/gtk-2.0/gtkrc"
style "user-font"
{
  font_name="Tahoma 11"
}
widget_class "*" style "user-font"
include "/home/muntyan/.gtkrc-2.0.mine"
# -- THEME AUTO-WRITTEN DO NOT EDIT
EOFEOF

cat > $dir/file.ini <<EOFEOF
[module]
type=Python
file=simple.py
version=1.0
[plugin]
id=APlugin
_name=A Plugin
_description=A plugin
author=Some Guy
; this is a plugin version, can be anything
version=3.1415926
EOFEOF

cat > $dir/file.pl <<EOFEOF
#!/usr/bin/perl -- # -*- Perl -*-
#
# $Id: collateindex.pl,v 1.10 2004/10/24 17:05:41 petere78 Exp $

print OUT "<title>$title</title>\n\n" if $title;

$last = {};     # the last indexterm we processed
$first = 1;     # this is the first one
$group = "";    # we're not in a group yet
$lastout = "";  # we've not put anything out yet
@seealsos = (); # See also stack.

# Termcount is > 0 iff some entries were skipped.
$quiet || print STDERR "$termcount entries ignored...\n";

&end_entry();

print OUT "</indexdiv>\n" if $lettergroups;
print OUT "</$indextag>\n";

close (OUT);

$quiet || print STDERR "Done.\n";

sub same {
    my($a) = shift;
    my($b) = shift;

    my($aP) = $a->{'psortas'} || $a->{'primary'};
    my($aS) = $a->{'ssortas'} || $a->{'secondary'};
    my($aT) = $a->{'tsortas'} || $a->{'tertiary'};

    my($bP) = $b->{'psortas'} || $b->{'primary'};
    my($bS) = $b->{'ssortas'} || $b->{'secondary'};
    my($bT) = $b->{'tsortas'} || $b->{'tertiary'};

    my($same);

    $aP =~ s/^\s*//; $aP =~ s/\s*$//; $aP = uc($aP);
    $aS =~ s/^\s*//; $aS =~ s/\s*$//; $aS = uc($aS);
    $aT =~ s/^\s*//; $aT =~ s/\s*$//; $aT = uc($aT);
    $bP =~ s/^\s*//; $bP =~ s/\s*$//; $bP = uc($bP);
    $bS =~ s/^\s*//; $bS =~ s/\s*$//; $bS = uc($bS);
    $bT =~ s/^\s*//; $bT =~ s/\s*$//; $bT = uc($bT);

#    print "[$aP]=[$bP]\n";
#    print "[$aS]=[$bS]\n";
#    print "[$aT]=[$bT]\n";

    # Two index terms are the same if:
    # 1. the primary, secondary, and tertiary entries are the same
    #    (or have the same SORTAS)
    # AND
    # 2. They occur in the same titled section
    # AND
    # 3. They point to the same place
    #
    # Notes: Scope is used to suppress some entries, but can't be used
    #          for comparing duplicates.
    #        Interpretation of "the same place" depends on whether or
    #          not $linkpoints is true.

    $same = (($aP eq $bP)
	     && ($aS eq $bS)
	     && ($aT eq $bT)
	     && ($a->{'title'} eq $b->{'title'})
	     && ($a->{'href'} eq $b->{'href'}));

    # If we're linking to points, they're only the same if they link
    # to exactly the same spot.
    $same = $same && ($a->{'hrefpoint'} eq $b->{'hrefpoint'})
	if $linkpoints;

    if ($same) {
       warn "$me: duplicated index entry found: $aP $aS $aT\n";
    }

    $same;
}

sub tsame {
    # Unlike same(), tsame only compares a single term
    my($a) = shift;
    my($b) = shift;
    my($term) = shift;
    my($sterm) = substr($term, 0, 1) . "sortas";
    my($A, $B);

    $A = $a->{$sterm} || $a->{$term};
    $B = $b->{$sterm} || $b->{$term};

    $A =~ s/^\s*//; $A =~ s/\s*$//; $A = uc($A);
    $B =~ s/^\s*//; $B =~ s/\s*$//; $B = uc($B);

    return $A eq $B;
}

=head1 EXAMPLE
B<collateindex.pl> B<-o> F<index.sgml> F<HTML.index>
=head1 EXIT STATUS
=over 5
=item B<0>
Success
=item B<1>
Failure
=back
=head1 AUTHOR
Norm Walsh E<lt>ndw@nwalsh.comE<gt>
Minor updates by Adam Di Carlo E<lt>adam@onshore.comE<gt> and Peter Eisentraut E<lt>peter_e@gmx.netE<gt>
=cut
EOFEOF

cat > $dir/file.po <<EOFEOF
# SOME DESCRIPTIVE TITLE.
# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER
# This file is distributed under the same license as the PACKAGE package.
# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.
#
msgid ""
msgstr ""
"Project-Id-Version: PACKAGE VERSION\n"
"Report-Msgid-Bugs-To: \n"
"POT-Creation-Date: 2006-12-17 09:49-0600\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
"Language-Team: LANGUAGE <LL@li.org>\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=CHARSET\n"
"Content-Transfer-Encoding: 8bit\n"

#: ../medit/medit.desktop.in.h:1
msgid "Text editor"
msgstr ""

#: ../medit/medit.desktop.in.h:2
msgid "medit"
msgstr ""
EOFEOF

cat > $dir/file.texi <<EOFEOF
\input texinfo
@setfilename manual
@settitle manual

@titlepage
@title manual

@c The following two commands start the copyright page.
@page
@vskip 0pt plus 1filll
@insertcopying
@end titlepage

@c Output the table of contents at the beginning.
@contents

@ifnottex
@node Top
@top manual
@insertcopying
@end ifnottex

@menu
* MooScript::    MooScript - builtin scripting language.
* Index::        Index.
@end menu

@node MooScript
@chapter MooScript

@cindex chapter, first

This is the first chapter.
@cindex index entry, another

Here is a numbered list.

@enumerate
@item
This is the first item.

@item
This is the second item.
@end enumerate

@node Index
@unnumbered Index

@printindex cp

@bye
EOFEOF

cat > $dir/file.dtd <<EOFEOF
<!-- FIXME: the "name" attribute can be "_name" to be marked for translation -->
<!ENTITY % itemattrs
 "name  CDATA #REQUIRED
  style CDATA #REQUIRED">
<!ELEMENT language (escape-char?,(line-comment|block-comment|string|syntax-item|pattern-item|keyword-list)+)>
<!-- FIXME: the "name" and "section" attributes can be prefixed with
     "_" to be marked for translation -->
<!ELEMENT keyword-list (keyword+)>
<!ATTLIST keyword-list
  %itemattrs;
  case-sensitive                  (true|false) "true"
  match-empty-string-at-beginning (true|false) "false"
  match-empty-string-at-end       (true|false) "false"
  beginning-regex                 CDATA        #IMPLIED
  end-regex                       CDATA        #IMPLIED>
EOFEOF

cat > $dir/file.la <<EOFEOF
# moo.la - a libtool library file
# Generated by ltmain.sh - GNU libtool 1.5.22 Debian 1.5.22-4 (1.1220.2.365 2005/12/18 22:14:06)
#
# Please DO NOT delete this file!
# It is necessary for linking the library.

# The name that we can dlopen(3).
dlname='moo.so'

# Names of this library.
library_names='moo.so moo.so moo.so'

# The name of the static archive.
old_library=''

# Libraries that this one depends upon.
dependency_libs=' -L/usr/local/gtk/lib /home/muntyan/projects/gtk/build/moo/moo/libmoo.la -lutil /usr/local/gtk/lib/libgtk-x11-2.0.la -lXext -lXinerama -lXrandr -lXcursor -lXfixes /usr/local/gtk/lib/libgdk-x11-2.0.la -latk-1.0 /usr/local/gtk/lib/libgdk_pixbuf-2.0.la /usr/local/gtk/lib/libpangocairo-1.0.la /usr/local/gtk/lib/libpangoft2-1.0.la /usr/local/gtk/lib/libpango-1.0.la /usr/local/gtk/lib/libcairo.la -lfreetype -lz -lfontconfig -lpng12 -lXrender -lX11 -lm /usr/local/gtk/lib/libgobject-2.0.la /usr/local/gtk/lib/libgmodule-2.0.la -ldl /usr/local/gtk/lib/libgthread-2.0.la -lpthread /usr/local/gtk/lib/libglib-2.0.la -lrt -lpcre /usr/lib/gcc/i486-linux-gnu/4.1.2/../../..//libfam.la -lrpcsvc /usr/lib/gcc/i486-linux-gnu/4.1.2/../../..//libxml2.la -L/usr/lib/python2.4 -lpython2.4  '

# Version information for moo.
current=0
age=0
revision=0

# Is this an already installed library?
installed=no

# Should we warn about portability when linking against -modules?
shouldnotlink=yes

# Files to dlopen/dlpreopen
dlopen=''
dlpreopen=''

# Directory that this library needs to be installed in:
libdir='/usr/local/gtk/lib/python2.4/site-packages'
relink_command="(cd /home/muntyan/projects/gtk/build/moo/moo; /bin/sh ../libtool  --tag=CC --mode=relink gcc -g -L/usr/local/gtk/lib -o moo.la -rpath /usr/local/gtk/lib/python2.4/site-packages -no-undefined -module -avoid-version -export-symbols-regex initmoo moopython/libmoomod.la libmoo.la -lutil -L/usr/local/gtk/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0 -pthread -L/usr/local/gtk/lib -lgthread-2.0 -lglib-2.0 -lpcre -lfam -lxml2 -L/usr/lib/python2.4 -lpython2.4 @inst_prefix_dir@)"
EOFEOF

cat > $dir/file.pc <<EOFEOF
# A comment
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: cairo
Description: Multi-platform 2D graphics library
Version: 1.4.10

Requires.private: freetype2 >= 8.0.2 fontconfig libpng12 xrender >= 0.6 x11
Libs: -L${libdir} -lcairo
Libs.private: -lz -lm
Cflags: -I${includedir}/cairo
EOFEOF

cat > $dir/file.spec <<EOFEOF
#
# gtksourceview.spec
#

%define api_version	1.0
%define lib_major 0
%define lib_name	%mklibname %{name}- %{api_version} %{lib_major}

Summary:	Source code viewing library
Name:		gtksourceview
Version: 	1.7.2
Release:	%mkrel 1
License:	GPL
Group:		Editors
URL:		http://people.ecsc.co.uk/~matt/downloads/rpms/gtksourceview/
Source0:	ftp://ftp.gnome.org/pub/GNOME/sources/%{name}/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}
BuildRequires:	libgtk+2-devel >= 2.3.0
BuildRequires:  libgnome-vfs2-devel >= 2.2.0
BuildRequires:  libgnomeprintui-devel >= 2.7.0
BuildRequires:  perl-XML-Parser
Conflicts:	gtksourceview-sharp <= 0.5-3mdk

%description
GtkSourceview is a library that adds syntax highlighting,
line numbers, and other programming-editor features.
GtkSourceView specializes these features for a code editor.

%package -n %{lib_name}
Summary:	Source code viewing library
Group:		Editors
Requires:	%{name} >= %{version}-%{release}
Provides:	lib%{name} = %{version}-%{release}
Provides:	libgtksourceview0 = %{version}-%{release}
Obsoletes:	libgtksourceview0
Provides:   libgtksourceview1.0 = %{version}-%{release}
Obsoletes:  libgtksourceview1.0

%description -n %{lib_name}
GtkSourceview is a library that adds syntax highlighting,
line numbers, and other programming-editor features.
GtkSourceView specializes these features for a code editor.

%package -n %{lib_name}-devel
Summary:        Libraries and include files for GtkSourceView
Group:          Development/GNOME and GTK+
Requires:       %{lib_name} = %{version}
Provides:	%{name}-devel = %{version}-%{release}
Provides:	lib%{name}-devel = %{version}-%{release}
Provides:	lib%{name}-%{api_version}-devel = %{version}-%{release}
Provides:	libgtksourceview0-devel = %{version}-%{release}
Obsoletes:	libgtksourceview0-devel
Provides:   libgtksourceview1.0-devel = %{version}-%{release}
Obsoletes:   libgtksourceview1.0-devel

%description -n %{lib_name}-devel
GtkSourceView development files


%prep
%setup -q

%build

%configure2_5x

%make

%install
rm -rf %{buildroot}

%makeinstall_std

%{find_lang} %{name}-%{api_version}

%post -n %{lib_name} -p /sbin/ldconfig

%postun -n %{lib_name} -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files -f %{name}-%{api_version}.lang
%defattr(-,root,root)
%doc AUTHORS ChangeLog NEWS README TODO
%{_datadir}/gtksourceview-%{api_version}

%files -n %{lib_name}
%defattr(-,root,root)
%{_libdir}/*.so.*

%files -n %{lib_name}-devel
%defattr(-,root,root)
%doc %{_datadir}/gtk-doc/html/gtksourceview
%{_libdir}/*.so
%attr(644,root,root) %{_libdir}/*.la
%{_includedir}/*
%{_libdir}/pkgconfig/*

%changelog
* Tue Aug 08 2006 GÃ¶tz Waschk <waschk@mandriva.org> 1.7.2-1mdv2007.0
- New release 1.7.2

* Tue Jul 25 2006 GÃ¶tz Waschk <waschk@mandriva.org> 1.7.1-1mdk
- New release 1.7.1
EOFEOF
