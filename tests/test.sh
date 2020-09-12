#!/usr/bin/env bash

# Find backwards for the directory where test.sh is
# """""""""""""""""""""""""""""""""""""""""""""""""
upsearch () {
  slashes=$(echo $PWD | tr -cd \/)
  directory="$PWD"
  n=${#slashes}
  while [ $n -gt 0 ]; do
    test -e "$directory/$1" && echo "$directory" && return 
    directory="$directory/.."
    n=$(expr $n - 1)
  done
}

[ -z "$DIR" ] && DIR=$(upsearch test.sh) && export DIR

if [ ! -f $1.tst ]; then
  echo "Unknown test $1"
  exit 1
fi

if [ ! -f $1.in ]; then
  echo "Unknown input file $1"
  exit 1
fi

LOG=${PWD}.log
PTYLIE=${PTYLIE_PATH:+${PTYLIE_PATH}/}ptylie
HVLT=${HLVT_PATH:+${HLVT_PATH}/}hlvt
BL=$(uname -s).bl

# Force a clean locale environment
# """"""""""""""""""""""""""""""""
export LANG=en_US.UTF-8
export LC_CTYPE=
export LC_NUMERIC=
export LC_TIME=
export LC_COLLATE=
export LC_MONETARY=
export LC_MESSAGES=
export LC_PAPER=
export LC_NAME=
export LC_ADDRESS=
export LC_TELEPHONE=
export LC_MEASUREMENT=
export LC_IDENTIFICATION=
export LC_ALL=

# Check for the presence of required programs
# """""""""""""""""""""""""""""""""""""""""""
if ! which smenu >/dev/null 2>&1; then
  echo "smenu was not found, please install it. Aborting."
  exit 1
fi

for PROG in $PTYLIE $HLVT; do
  if ! which $PROG >/dev/null 2>&1; then
    echo "The required $PROG program was not found, aborting."
    exit 1
  fi
done

# Ignore blacklisted tests for this OS.
# These tests probably need a fixed version of ptylie to work
# """""""""""""""""""""""""""""""""""""""""""""""""""""""""""
if [ -f "$BL" ]; then
  grep $1 $BL >/dev/null 2>&1 && exit 0
fi

# Launch the test
# """""""""""""""
clear
PS1='$ ' $PTYLIE -i $1.tst -l $1.log -w 80 -h 24 sh
$HVLT < $1.log | sed '1,/exit 0/!d' > $1.out
rm -f $1.log $1.bad
[ -f $LOG ] && touch $LOG

# Add the new log entry,
# If the .good file exists and the .out and the .good files are identical
#   then remove the .out file and tag the test as GOOD
# else if the .good file does not exist then keep the .out file and tag
#   the test as UNCHECKED in the log
# else the test is a failure and tag it as BAD
# """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

if [ -e $1.good ]; then
  if diff -u $1.out $1.good >/dev/null 2>&1; then
    ATTR="GOOD      "
    rm $1.out
  else
    ATTR="BAD       "
    mv $1.out $1.bad
  fi
else
  ATTR="UNCHECKED "
fi

if grep "^$1 " $LOG >/dev/null 2>&1; then
  RES=$(awk -vt=$1 -vattr="$ATTR" \
          '$1 == t {$2 = attr; print t,attr}' $LOG)

  NEWLOG=$(sed "s/^$1 .*/$RES/" $LOG)

  [ -n "$NEWLOG" ] && echo "$NEWLOG" > $LOG
else
  echo "$1 $ATTR" >> $LOG
fi

exit 0
