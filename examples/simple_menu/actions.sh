#!/usr/bin/env bash

(( $# != 1 )) && exit 0

ACTION=$1

case $ACTION in
  *) echo "Tag $ACTION selected."
     sleep 1
     ;;
esac

exit 0
