#!/bin/sh
# "./check.sh somelang.lang" will validate somelang.lang file.
# "./check.sh" without arguments will validate lang and styles files
# specified here.

langs="ada.lang changelog.lang chdr.lang c.lang cpp.lang csharp.lang
       css.lang def.lang desktop.lang diff.lang dpatch.lang dtd.lang
       fortran.lang gap.lang gtk-doc.lang gtkrc.lang haskell.lang
       html.lang idl.lang ini.lang java.lang javascript.lang latex.lang
       libtool.lang m4.lang makefile.lang objc.lang ocaml.lang
       octave.lang pascal.lang perl.lang php.lang pkgconfig.lang po.lang
       python.lang ruby.lang scheme.lang sh.lang sql.lang tcl.lang
       texinfo.lang verilog.lang xml.lang yacc.lang"

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
