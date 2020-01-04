#!/usr/bin/env ruby
#
puts "\141" # Shows 'a'

str = 'lorem'
puts str[1..3] # 'ore'

# Ranges
11..11, 0x11..0x11, 01..07, ?a..?f, 1.1..2.2 # range incl. the last value
11...11, 0x11...0x11, 01...07, ?a...?f, 1.1...2.2 # range excl. the last value
11....111, 0x11......0x11, 1.1.....1.2 # 4 (and more) dots are not a valid range
