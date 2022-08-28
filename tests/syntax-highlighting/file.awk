#!/usr/bin/awk -f
# This is a comment line.

# Special patterns:
BEGIN
END

# Field variables:
xxx$0xxx
xx$NFxxx

# String and numeric contansts:
string = "Hello, World!"
number = 42

# Patterns:
/\.'regexpr#"/ { print $0 }
NR == 1, NR == 5 { printf("%s\n", $0) }

# Statements:
print string, number
