#!/usr/bin/env python3

import gi

gi.require_version('Gtk', '4.0')

from gi.repository import GLib, GObject, Gtk

my_fstring = f'this is a fstring {something("here")}'
my_fstring = f"this is a fstring {other('stuff'}"

class MyClass(GObject.Object):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwarg)

