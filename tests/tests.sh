#!/usr/bin/env bash

# ===========================================================================
# Usage: ./tests.sh [test_directory]
#
# Is a test_directory is given then the automated testings will be restricted
# to the given directory tree
# ===========================================================================

header1()
{
  printf "%-20s %-5s %s %s\n" Directory Tests States
}

header2()
{
  printf "%-5s %s %s\n" Tests States
}

[ -z "$1" ] && TESTDIR="." \
            || TESTDIR=$1

[ ! -d $TESTDIR ] && echo "$TESTDIR is not a directory" \
                  && exit 1

export DIR=${PWD}

# Remove the existing .log files
# """"""""""""""""""""""""""""""
[ "$TESTDIR" == "." ] && rm -f *.log || rm -f ${TESTDIR}.log

# Build the list of tests to be performed
# """""""""""""""""""""""""""""""""""""""
LIST=$(
        find $TESTDIR -type f -name 't[0-9][0-9][0-9][0-9].tst' \
        | sort
      )

# Perform the testings
# """"""""""""""""""""
for INDEX in $LIST; do
  INDEX=${INDEX#./}
  SUBDIR=${INDEX%/*}

  [ $SUBDIR != $INDEX ] && cd $SUBDIR

  TST=${INDEX##*/}

  $DIR/test.sh ${TST%.tst} || exit 1

  [ $SUBDIR != $INDEX ] && cd -
done

# Display the results
# """""""""""""""""""
echo
echo -n "Results"
[ "$TESTDIR" != "." ] && echo -n " for the tests in the '$TESTDIR' directory" \
                      || TESTDIR=""
echo :
clear

RESULTS_BAD=$(grep BAD       ${TESTDIR:=*}.log)
RESULTS_UNC=$(grep UNCHECKED ${TESTDIR:=*}.log)

if [ -z "$RESULTS_BAD" ]; then
  tput smso
  echo "All validated tests passed successfully !" 
  tput rmso
else
  tput smso
  echo "Some tests have failed !"
  tput rmso

  if [ -z "$TESTDIR" ]; then
    header1
    printf "%-20s %5s %s\n" $(echo "$RESULTS_BAD" | sed 's/.log:/ /')
  else
    header2
    printf "%5s %s\n" $(echo "$RESULTS_BAD" | sed 's/.log:/ /')
  fi

  echo
  echo "For each BAD, a diff between the corresponding .bad and .good"
  echo "files in the specified directory will provide more details on"
  echo "the failure."
fi

if [ -n "$RESULTS_UNC" ]; then
  echo
  echo "But some tests have not been validated, please provide a .good"
  echo "file for them. Here is the list:"
  echo

  if [ -z "$TESTDIR" ]; then
    header1
    printf "%-20s %5s %s\n" $(echo "$RESULTS_UNC" | sed 's/.log:/ /')
  else
    header2
    printf "%5s %s\n" $(echo "$RESULTS_UNC" | sed 's/.log:/ /')
  fi
fi

echo

exit 0
