#!/bin/bash
echo "Hi there!"

# Parameter Expansion

xxx${xxx}xxx # parameter in braces
xxx${"{"}xxx xxx${"}"}xxx # brace in quoted string

xxx$0000 # one digit parameter

xxx${parameter:-word}xxx${parameter-word}xxx # Use Default Values
xxx${parameter:=word}xxx${parameter=word}xxx # Assign Default Values
xxx${parameter:?word}xxx${parameter?word}xxx # Indicate Error if Null or Unset
xxx${parameter:?}xxx${parameter?}xxx # Indicate Error if Null or Unset
xxx${parameter:+word}xxx${parameter+word}xxx # Use Alternative Value

xxx${#parameter}xxx # String Length
xxx${parameter%word}xxx # Remove Smallest Suffix Pattern
xxx${parameter%%word}xxx # Remove Largest Suffix Pattern
xxx${parameter#word}xxx # Remove Smallest Prefix Pattern
xxx${parameter##word}xxx # Remove Largest Prefix Pattern

xxx${x:-$(ls)}xxx
xxx${posix:?}xxx
xxx${3:+posix}xxx
xxx${#HOME}xxx
xxx${x%.c}.oxxx
xxx${x#$HOME}xxx
xxx${x##*/}xxx
xxx${x#*}xxx
xxx${x#"*"}xxx

# Variable definitions
var1=val1; var2=val2
if var=$(cmd); then some; fi
test -f xxx && var=xxx || var=yyy
echo text | var=xxx cmd & var=yyy
declare -i '-r' "-x" var1=val1 var2=$val1 var3=`cmd1` \
  var4=$(cmd2) var5=xxx\ yyy var6 #comment
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

echo Look for file
echo Look for; echo Look for
echo next line
