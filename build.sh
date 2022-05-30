#!/bin/sh

# ###################################################################
# Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# ###################################################################

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
