#!/bin/sh

# check env CC
[ -z "$CC" ] && CC="cc"
echo "CC=$CC"

# compile
echo "Compiling..."
$CC sss.c -O2 -o sss

# strip
echo "Stripping..."
strip sss
