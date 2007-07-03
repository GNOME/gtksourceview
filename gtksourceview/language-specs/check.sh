#!/bin/sh
# "./check.sh files..." will validate files given on command line.
# "./check.sh" without arguments will validate lang and styles files
# specified here.

langs="ada.lang awk.lang changelog.lang chdr.lang c.lang cpp.lang csharp.lang
       css.lang def.lang desktop.lang diff.lang dpatch.lang dtd.lang
       fortran.lang gap.lang gtk-doc.lang gtkrc.lang haddock.lang
       haskell.lang haskell-literate.lang html.lang idl.lang ini.lang
       java.lang javascript.lang latex.lang libtool.lang lua.lang
       m4.lang makefile.lang objc.lang ocaml.lang ocl.lang octave.lang
       pascal.lang perl.lang php.lang pkgconfig.lang po.lang
       python.lang ruby.lang scheme.lang sh.lang sql.lang tcl.lang
       texinfo.lang verilog.lang xml.lang yacc.lang"

styles="gvim.xml kate.xml tango.xml"

check_file() {
  case $1 in
  *.xml)
    xmllint --relaxng styles.rng --noout $file || exit 1
    ;;
  *)
    xmllint --relaxng language2.rng --noout $file || exit 1
    ;;
  esac
}

if [ $1 ]; then
  for file in $@; do
    check_file $file
  done
  exit 0
fi

if [ "$srcdir" ]; then
  cd $srcdir
fi

for file in $langs $styles; do
  check_file $file
done
