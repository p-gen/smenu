#!/usr/bin/env bash

# ==================== #
# Fatal error function #
# ==================== #
function error
{
  echo $* >&2
  exit 1
}

# Check for the presence of smenu
# """""""""""""""""""""""""""""""
SMENU=$(which smenu 2>/dev/null)
[[ -x "$SMENU" ]] || error "smenu is not in the PATH or not executable."

MENU+="\
'Create' 'Delete' 'Extend' 'Shrink'
'VGC VG' 'VGD VG' 'VGE VG' 'VGS VG'
'LVC LV' 'LVD LV' 'LVE LV' 'LVS LV'
'PVC PV' 'PVD PV'
#
'L List'
#
'Quit Exit menu'"

trap "exit 1" INT

while true
do
   clear
   echo "Executing ${0##*/}"
   echo

   MESSAGE="LVM management"
   read REP <<< $(echo "$MENU" | ./menu.sh -p $SMENU -i "$MESSAGE" "-Re1")

   case $REP in
     ABORT) exit 1         ;;
     QUIT)  exit 0         ;;

     L)     echo List      ;;

     VGC)   echo VG create ;;
     VGD)   echo VG delete ;;
     VGE)   echo VG expand ;;
     VGS)   echo VG shrink ;;

     LVC)   echo LV create ;;
     LVD)   echo LV delete ;;
     LVE)   echo LV expand ;;
     LVS)   echo LV shrink ;;

     PVC)   echo PV create ;;
     PVD)   echo PV delete ;;
   esac

   echo
   echo -n "Press <Enter> to continue"
   read
done

exit 0
