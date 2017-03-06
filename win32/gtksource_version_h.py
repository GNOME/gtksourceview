#!/usr/bin/python
#
# Simple utility script to generate gtksourceversion.h

import os
import sys
import argparse

from replace import replace_multi

def gen_version_h(argv):
    top_srcdir = os.path.dirname(__file__) + "\\.."
    parser = argparse.ArgumentParser(description='Generate gtksourceversion.h')
    parser.add_argument('--version', help='Version of the package',
                        required=True)
    args = parser.parse_args()
    version_info = args.version.split('.')

    version_h_replace_items = {'@GTK_SOURCE_MAJOR_VERSION@': version_info[0],
                               '@GTK_SOURCE_MINOR_VERSION@': version_info[1],
                               '@GTK_SOURCE_MICRO_VERSION@': version_info[2]}

    # Generate gtksourceversion.h
    replace_multi(top_srcdir + '/gtksourceview/gtksourceversion.h.in',
                  top_srcdir + '/gtksourceview/gtksourceversion.h',
                  version_h_replace_items)

if __name__ == '__main__':
    sys.exit(gen_version_h(sys.argv))