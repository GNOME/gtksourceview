#!/bin/sh
# "./check.sh files..." will validate files given on command line.
# "./check.sh" without arguments will validate all language files
# in the source directory

check_file() {
  case $1 in
  testv1.lang) ;; # skip test file for old format
  *)
    xmllint --relaxng language2.rng --noout $1 || exit 1
    ;;
  esac
}

files=""

if [ $1 ]; then
  files=$@
else
  if [ "$srcdir" ]; then
    cd $srcdir
  fi

  for lang in *.lang; do
    case $lang in
      msil.lang) ;;
      *)
        files="$files $lang"
        ;;
    esac
  done
fi

for file in $files; do
  check_file $file
done
