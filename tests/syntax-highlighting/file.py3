#!/usr/bin/env python3

import gi

gi.require_version('Gtk', '4.0')

from gi.repository import GLib, GObject, Gtk

my_fstring = f'this is a fstring {something("here")}'
my_fstring = f"this is a fstring {other('stuff'}"

class MyClass(GObject.Object):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwarg)

# Valid numbers, should be highlighted
valid_numbers = (1000 + 1_000 + 1_000_000 +
                 0x1000 + 0x_10 + 0x10_00 +
                 0b1000 + 0b_10 + 0b1_0 +
                 0o1000 + 0o_10 + 0o10_00)

# Invalid numbers (raise SyntaxError), must not be highlighted
invalid_numbers = (1000l + 1000L + 0x10L + 0b10L + 0o10L +
                   0100 + 1__000 + _100 + 100_ +
                   0x10_ + 0b10_ + + 0o10_)

# Soft keywords
match 1:
    case 1:
        case = match()
    case _:
        re.match(r"*", case)
