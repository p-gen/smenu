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

RES=$(
       $SMENU -2 ^Y -1 ^N -3 ^C -s /^N -x cur 10 \
              -m "Please confirm your choice:"   \
         <<< "YES NO CANCEL"
     )

[[ -z "$RES" ]] && echo "'q' was hit, exiting without selecting anything" \
                || echo "You selected: $RES"

exit 0
