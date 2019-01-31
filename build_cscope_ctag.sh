#!/bin/sh

find  . -name "*.h" -o -name "*.c" -o -name "*.s" -o -name "*.S" > cscope.files
cscope  -bkq -i cscope.files
ctags  -R --exclude=.git
