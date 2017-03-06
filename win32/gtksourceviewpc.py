#!/usr/bin/python
#
# Utility script to generate .pc files for GLib
# for Visual Studio builds, to be used for
# building introspection files

# Author: Fan, Chun-wei
# Date: March 10, 2016

import os
import sys

from replace import replace_multi
from pc_base import BasePCItems

def main(argv):
    base_pc = BasePCItems()

    base_pc.setup(argv)

    glib_req_ver = '2.48'
    gtk_req_ver = '3.20'
    libxml2_req_ver = '2.6'
    API_VERSION = '3.0'

    pkg_replace_items = {'@PACKAGE_NAME@': 'gtksourceview',
                         '@AX_PACKAGE_REQUIRES@': 'glib-2.0 >= ' + glib_req_ver + ' ' +
	                                              'gio-2.0 >= ' + glib_req_ver + ' ' +
                                                  'gtk+-3.0 >= ' + gtk_req_ver,
                         '@AX_PACKAGE_REQUIRES_PRIVATE@': 'libxml-2.0 >= ' + libxml2_req_ver,
                         '@GSV_API_VERSION@': API_VERSION,
                         '@PACKAGE_VERSION@': base_pc.version}

    pkg_replace_items.update(base_pc.base_replace_items)

    # Generate gtksourceview-3.0.pc
    replace_multi(base_pc.top_srcdir + '/gtksourceview.pc.in',
                  base_pc.srcdir + '/gtksourceview-' + API_VERSION + '.pc',
                  pkg_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
