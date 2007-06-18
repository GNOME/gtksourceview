#!/bin/sh
# "./check.sh somelang.lang" will validate somelang.lang file.
# "./check.sh" without arguments will validate lang and styles files
# specified here.

langs="c.lang cpp.lang changelog.lang def.lang
       html.lang javascript.lang latex.lang
       m4.lang makefile.lang xml.lang yacc.lang
       sh.lang python.lang perl.lang ada.lang
       csharp.lang css.lang desktop.lang fortran.lang
       gtkrc.lang haskell.lang idl.lang ini.lang
       java.lang octave.lang pascal.lang php.lang
       po.lang ruby.lang scheme.lang sql.lang tcl.lang
       texinfo.lang gtk-doc.lang dtd.lang dpatch.lang
       libtool.lang pkgconfig.lang"

styles="gvim.xml kate.xml testdark.xml"

if [ $1 ]; then
    langs=$*
    styles=
fi

for file in $langs; do
    if ! xmllint --relaxng language2.rng --noout $file ; then
	exit 1
    fi
done

for file in $styles; do
    if ! xmllint --relaxng styles.rng --noout $file ; then
	exit 1
    fi
done
