#!/bin/sh

# Manage --help option
# """"""""""""""""""""
if echo "$@" | grep -- "--help"; then
  ./configure --help | sed s/configure/build.sh/g
  exit 1
fi

# Ensure that aclocal wont' be called
# """""""""""""""""""""""""""""""""""
touch aclocal.m4
touch Makefile.in configure config.h.in

# Create the Makefile
# """""""""""""""""""
./configure "$@"

# Add the git version if this is a git clone
# """"""""""""""""""""""""""""""""""""""""""
[ -d .git ] && V=`git log -1 --pretty=format:-%h` || V=""

sed "/VERSION/s/\$/ \"$V\"/" config.h > /tmp/config.h$$
mv /tmp/config.h$$ config.h

# Create the executable
# """""""""""""""""""""
make

exit 0
