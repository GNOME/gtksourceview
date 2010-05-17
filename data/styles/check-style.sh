#!/bin/sh
# "./check.sh files..." will validate files given on command line.
# "./check.sh" without arguments will validate all style files
# in the source directory

check_file() {
  xmllint --relaxng styles.rng --noout $1 || exit 1
}

files=""

if [ $1 ]; then
  files=$@
else
  files=*.xml
fi

for file in $files; do
  check_file $file
done
