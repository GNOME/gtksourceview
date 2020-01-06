#!/usr/bin/env ruby
#
puts "\141" # Shows 'a'

str = 'lorem'
puts str[1..3] # 'ore'

# Number methods
puts -11.to_s + ' ' + 0x11.to_s  + ' ' + 1.1.to_s + ' ' + ?a.to_s + ' ' + 1.x
11.1aa # not a valid method name

# Ranges
11..11, 0x11..0x11, 01..07, ?a..?f, 1.1..2.2 # range incl. the last value
11...11, 0x11...0x11, 01...07, ?x40...?\101, 1.1...2.2 # range excl. the last value
11....111, 0x11......0x11, 1.1.....1.2 # 4 (and more) dots are not a valid range

# String literals
str = ?\x41 + ?\101 # == 'AA'
puts ?\M-\C-x
