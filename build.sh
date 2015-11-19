#!/bin/sh
./configure "$@"

[ -d .git ] && V=`git log -1 --pretty=format:-%h` || V=""

sed "/VERSION/s/\$/ \"$V\"/" config.h > /tmp/config.h$$
mv /tmp/config.h$$ config.h

make

exit 0
