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
