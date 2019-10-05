#!/usr/bin/env bash

# ################################################################ #
# Frontend to smenu to display a nice menu                         #
#                                                                  #
# FLAGS: -p    The path of the smenu program                       #
#        -i    (optional) add some help in the title               #
#                                                                  #
# ARGS:  $1    (required) must contain a string which will become  #
#                         the menu title                           #
#        $2... (optional) adds more options passed to smenu        #
#                                                                  #
# RC:    0 OK                                                      #
#        1 KO                                                      #
# ################################################################ #

# ============== #
# usage function #
# ============== #
function usage
{
  echo "menu.sh [-i] title [smenu options]"
  exit 1
}

declare integer INFO=0

# Option processing
# """""""""""""""""
while getopts p:ih OPT 2>/dev/null
do
  case ${OPT} in
    p)   SMENU=${OPTARG}
         ;;

    i)   INFO=1
         ;;

    h|*) usage
         exit 1
         ;;
  esac
done
shift $((${OPTIND} - 1))

# Menu title generation
# """""""""""""""""""""
if [[ $1 ]]; then
  MESSAGE="$1"
  (( INFO )) && MESSAGE+=$'\n'"[ q=quit arrows=move ?=help ]"
  TITLE="-m"
  TITLE+=$MESSAGE$'\n'
  shift
else
  TITLE="-m "
fi

# Menu display
# """"""""""""
REP=$($SMENU "$TITLE"                      \
            -s /Exit                       \
            -q -d -M -n 30                 \
            -U "Exit menu"                 \
            -a da:3,b                      \
            -e '^#' -E '/(^#)+(.*)$/ \2/v' \
            -1 'Exit' 2,r                  \
            -I '/([^ ]+) (.*)/\2/v' -c -w  \
            $1                             \
      || echo Abort)

read KEY DUMMY <<< $REP

# Action processing
# """""""""""""""""
case $KEY in
  Abort)
    echo ABORT
    ;;

  ""|Quit)
    echo QUIT
    ;;

  *)
    echo $KEY
    ;;
esac

# Normal exit
# """""""""""
exit 0
