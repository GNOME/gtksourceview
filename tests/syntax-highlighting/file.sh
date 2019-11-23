#!/bin/bash
echo "Hi there!"

# Parameter Expansion

xxx${xxx}xxx # Parameter in braces
xxx${var/\"}xxx xxx${var/\}}xxx # Escaped characters
xxx${var\
/x/y} # Line continue

xxx$0000 # One digit parameter
xxx$-xxx xxx$$xxx xxx$@xxx # Special parameters
xxx${array[@]}xxx xxx${!array[@]}xxx xxx${array[-1]}xxx # Arrays

xxx${parameter:-word}xxx${parameter-word}xxx # Use Default Values
xxx${parameter:=word}xxx${parameter=word}xxx # Assign Default Values
xxx${parameter:?word}xxx${parameter?word}xxx # Indicate Error if Null or Unset
xxx${parameter:?}xxx${parameter?}xxx # Indicate Error if Null or Unset
xxx${parameter:+word}xxx${parameter+word}xxx # Use Alternative Value

xxx${#parameter}xxx # String Length
xxx${!parameter}xxx # Names matching prefix
xxx${parameter%word}xxx # Remove Smallest Suffix Pattern
xxx${parameter%%word}xxx # Remove Largest Suffix Pattern
xxx${parameter#word}xxx # Remove Smallest Prefix Pattern
xxx${parameter##word}xxx # Remove Largest Prefix Pattern

xxx${x:-$(ls ~/*)}xxx
xxx${posix:?}xxx
xxx${3:+posix}xxx
xxx${#HOME}xxx
xxx${x%.c}.oxxx
xxx${x#$HOME}xxx
xxx${x##*/}xxx
xxx${x#*}xxx
xxx${x#"*"}xxx

# Variable definitions
var1=val1; var2=val2 var3=val3
if var=$(cmd); then some; fi
test -f xxx && var=xxx || var=yyy
echo text | var=xxx cmd & var=yyy
declare -i '-r' "-x" var1=val1 var2=$val1 var3=`cmd1` \
  var4=$(cmd2) var5=xxx\ yyy var6=("${ar[@]}") var7 # Comment
var+=xxx; (var=yyy); { var=zzz; }
case $1 in
  item) var=xxx;;
  *)declare var=yyy;;
esac

# For statements
for word in hello world
do
  echo $word
done

for arg; do echo $arg; done
for \
arg; do echo $arg; done

# Generic command (e.g. echo)
echo for case grep $var ${var/x/y} $(cmd) `cmd` \
  'a' "b" \\ | grep 'pattern' > >(tee file)
echo echo; echo echo & echo echo
echo

# Redirections
> >> 1>&2 &> 3>&-
< 0<&3 3<&-
3<> 1>|

# Quoting
'no special characters'
"$var, ${var/x/y}, $(cmd), `cmd`, \
\$, \`, \", \\, \ "

# Arithmetic evaluation/expansion
var1=$((var2+2#101+$(cmd)+($var3+ \
  "$var4")/0x1f))
let var1='1'+010-23+`cmd`+var2 # Comment

# History expansion
"txt!cmd:^-$:r:gs/xxx/yyy/:ptxt"
^xxx^yyy^

# Test commands
test ! -f "$file"
[ "$var" = 'xxx' ]
[[ ! (-n $var1||$var2 =~ regex) && -f $file ]]
