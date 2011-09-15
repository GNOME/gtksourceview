#!/bin/sh

# Running this file will create "testdir" directory with bunch
# of files inside. Open them all to see if something broke.
# Kind of smoke test.

# Langs covered here:
# asp.lang changelog.lang c.lang cpp.lang desktop.lang diff.lang dtd.lang
# fsharp.lang gap.lang glsl.lang go.lang gtkrc.lang html.lang ini.lang
# latex.lang m4.lang makefile.lang ms.lang perl.lang po.lang prolog.lang
# python.lang sh.lang texinfo.lang xml.lang yacc.lang libtool.lang
# pkgconfig.lang objc.lang chdr.lang testv1.lang t2t.lang fortran.lang
# forth.lang octave.lang automake.lang

dir="testdir"
mkdir -p $dir/

cat > $dir/file.impl << EOFEOF
-- Opal test
IMPLEMENTATION LeapYear

IMPORT Nat COMPLETELY

DEF leapYear(year) ==
  IF (year % ("400"!) = 0) THEN true
  IF (year % ("400"!) |= 0) and (year % 100 = 0) THEN false
  IF (year % 100 |= 0) and (year % 4 = 0) THEN true
  IF (year % 4 |= 0) THEN false
  FI
EOFEOF

cat > $dir/file.sci << EOFEOF
// A comment with whites and tabulations 		
// Scilab editor: http://www.scilab.org/

function [a, b] = myfunction(d, e, f)
	a = 2.71828 + %pi + f($, :);
	b = cos(a) + cosh(a);
	if d == e then
		b = 10 - e.field;
	else
		b = "		test     " + home
		return
	end
	myvar = 1.23e-45;
endfunction
EOFEOF

cat > $dir/file.cob << EOFEOF
       IDENTIFICATION DIVISION.
       PROGRAM-ID. Age.
       AUTHOR. Fernando Brito.

       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  Age               PIC 99   VALUE ZEROS.
       01  Had_Birthday      PIC X    VALUE SPACES.
       01  Current_Year      PIC 9999 VALUE 2010.
       01  Result            PIC 9999 VALUE ZEROS.

       PROCEDURE DIVISION.
          DISPLAY "==> How old are you?".
          ACCEPT Age
          DISPLAY "==> Had you already had birthday this year (y or n)?".
          ACCEPT Had_Birthday

          SUBTRACT Current_Year FROM Age GIVING Result

          IF Had_Birthday = "n" THEN
            SUBTRACT 1 FROM Result GIVING Result
          END-IF

          DISPLAY "Let me guess... "" You were born in ", Result
          STOP RUN.
EOFEOF

cat > $dir/file.svh << EOFEOF
class my_class extends some_class;

  // This is a comment.
  /* This is also a comment, but it containts keywords: bit string, etc */

  // Some types.
  string         my_string = "This is a string";
  bit [3:0]        my_bits = 4'b0z1x;
  integer       my_integer = 32'h0z2ab43x;
  real             my_real = 1.2124155e-123;
  shortreal   my_shortreal = -0.1111e1;
  int               my_int = 53152462;
 
  extern function bit
    my_function(
      int unsigned       something);

endclass : my_class

function bit
  my_class::my_function(
    int unsigned    something);

  /* Display a string.
   *
   *   This is a slightly awkward string as it has
   *   special characters and line continuations.
   */
  $display("Display a string that continues over \
            multiple lines and contains special \
            characters: \n \t \" \'");

  // Use a system task.
  my_int = $bits(my_bits);

  // $asserton();     // Commenting a system task.
  // my_function();   // Commenting a function.

endfunction my_function

program test();

  my_class c;

  c = new();
  c.my_function(3);

endprogram : test
EOFEOF

cat > $dir/file.prg << EOFEOF
import "mod_say"
import "mod_rand"
import "mod_screen"
import "mod_video"
import "mod_map"
import "mod_key"

// Bouncing box example
process main()
private
  int vx=5, vy=5;

begin
  set_mode(640, 480, 16, MODE_WINDOW);
  graph = map_new(20, 20, 16);
  map_clear(0, graph, rgb(255, 0, 0));
  x = 320; y = 240;
  while(! key(_esc))
    if(x > 630 || x < 10)
      vx = -vx;
    end;
    if(y > 470 || y < 10)
      vy = -vy;
    end;
    x += vx; y += vy; // Move the box
    frame;
  end;
end;
EOFEOF

cat > $dir/file.ooc << EOFEOF
import structs/[ArrayList, LinkedList], io/FileReader
include stdarg, memory, string
use sdl, gtk
pi := const 3.14
Int: cover from int {
    toString: func -> String { "%d" format() }
}
Dog: class extends Animal {
    barf: func { ("woof! " * 2) println() }
}
EOFEOF

cat > $dir/file.bib << EOFEOF
%A .bib file might contain the following entry, which describes a mathematical handbook
@Book{abramowitz+stegun,
 author    = "Milton {Abramowitz} and Irene A. {Stegun}",
 title     = "Handbook of Mathematical Functions with
              Formulas, Graphs, and Mathematical Tables",
 publisher = "Dover",
 year      =  1964,
 address   = "New York",
 edition   = "ninth Dover printing, tenth GPO printing"
}
EOFEOF

cat > $dir/file.rq << EOFEOF
# Positive test: product of type promotion within the xsd:decimal type tree.

PREFIX t: <http://www.w3.org/2001/sw/DataAccess/tests/data/TypePromotion/tP-0#>
PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>
PREFIX xsd: <http://www.w3.org/2001/XMLSchema#>
ASK
 WHERE { t:double1 rdf:value ?l .
         t:float1 rdf:value ?r .
         FILTER ( datatype(?l + ?r) = xsd:double ) }
EOFEOF

cat > $dir/file.fcl << EOFEOF
FUNCTION_BLOCK explore

VAR_INPUT
    front : REAL;
    left : REAL;
    right : REAL;
END_VAR

VAR_OUTPUT
    velocity : REAL;
    angular_velocity : REAL;
END_VAR
EOFEOF

cat > $dir/file.fs << EOFEOF
(* Simple F# sample *)
let rec map func lst =
    match lst with
       | [] -> []
       | head :: tail -> func head :: map func tail
 
let myList = [1;3;5]
let newList = map (fun x -> x + 1) myList
EOFEOF

cat > $dir/file.glslf << EOFEOF
#version 140
varying vec4 color_out;
/*
  A color shader
*/
void main(void)
{
  // edit the color of the fragment
  vec4 new_color = color_out + vec4(0.4, 0.4, 0.1, 1.0);
  gl_FragColor = clamp(new_color, 0.0, 1.0);
}
EOFEOF

cat > $dir/file.go << EOFEOF
// gtk-source-lang: Go
/* comment! */
var s string := "A string\n"
import ("fmt")
func main() { fmt.Printf(s); }
type my_struct struct { I int; o string }
package foo
bar( a int32, b string )(c float32 ){ c = 1.3 + float32(a - int32(len(b)) }
EOFEOF

cat > $dir/file.cg << EOFEOF
struct OurOutputType
{
	float4 position : POSITION;
	float4 color : COLOR;
};

OurOutputType
main(float4           position : POSITION,
     uniform float4x4 modelViewProj)
{
	OurOutputType OUT;

	OUT.position = mul(modelViewProj, position);
	OUT.color = position;

	return OUT;
}
EOFEOF

cat > $dir/file.cu << EOFEOF
#include "cuMatrix.h"

__global__ void make_BlackWhite(int *image, int N){
	unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;

	image[y*N + x] = image[y*N + x] > 128 ? 255 : 0;
}

void convertToArray(int **matrix, int *array, int N){
	for(unsigned int i=0; i< N; i++)
		for(unsigned int j=0; j< N; j++)
			array[i*N+ j] = matrix[i][j];
}
EOFEOF

cat > $dir/file.prolog <<EOFEOF
conc([],X,X).
conc([Car|Cdr], X, [Car|ConcatCdr]):-
  conc(Cdr, X, ConcatCdr).
EOFEOF

cat > $dir/file.asp <<EOFEOF
<html>
<body>

<%
dim d
set d=Server.CreateObject("Scripting.Dictionary")
d.Add "o", "Orange"
d.Add "a", "Apple"
if d.Exists("o")= true then
    Response.Write("Key exists.")
else
    Response.Write("Key does not exist.")
end if
set d=nothing
%>

</body>
</html>
EOFEOF

cat > $dir/file.octave <<EOFEOF
% gtk-source-lang: octave
% -*- octave -*-
No idea what syntax is
EOFEOF

cat > $dir/file.t2t <<\EOFEOF
! gtk-source-lang: t2t

%!include ``something``
% Comment
``verbatim`` //italic//
```
verbatim block
```
EOFEOF

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

cat > $dir/file.ijm <<EOFEOF
// line comment
var variable = "string\n with \t escaped\"characters";
macro "new macro" {
  NotGlobalVar = 5 +6;
  result = getPixel(0, 0);
  run("8-bit");
}
function NewFunction() {
  /*
  multiline comment*/
}
EOFEOF

cat > $dir/Makefile.am <<EOFEOF
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST += README.W32

if HAVE_DOXYGEN
  DOXYDIR = docs
endif

doc_DATA = AUTHORS ChangeLog COPYING INSTALL NEWS README

bin_PROGRAMS = jupiter
jupiter_SOURCES = main.c
jupiter_CPPFLAGS = -I\$(top_srcdir)/common
jupiter_LDADD = ../common/libjupcommon.a

check_SCRIPTS = greptest.sh
TESTS = \$(check_SCRIPTS)

greptest.sh:
	echo './jupiter | grep "Hello from .*jupiter!"' > greptest.sh
	chmod +x greptest.sh

CLEANFILES = greptest.sh
EOFEOF
