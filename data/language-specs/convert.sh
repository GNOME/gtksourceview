#!/bin/sh

files="ada changelog csharp css desktop diff fortran gtkrc \
       haskell idl ini java lua makefile msil nemerle octave pascal perl \
       php po python R ruby scheme sh sql tcl texinfo vbnet verilog vhdl"

for file in $files; do
    if ! (./convert.py $file.lang > $file.new.lang) ; then
        echo "*** Error: " $file.lang
	exit 1
    fi
    if ! (./check.sh $file.new.lang) ; then
        echo "*** Error: " $file.lang
	exit 1
    fi
#    mv $file.new.lang $file.lang
done
