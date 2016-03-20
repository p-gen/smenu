#!/bin/bash

RES=$(
       ../../smenu -2 ^Y -1 ^N -3 ^C -s /^N -m "Please confirm your choice:" \
         <<< "YES NO CANCEL"
     )

[[ -z "$RES" ]] && echo "'q' was hit, exiting without selecting anything" \
                || echo "You selected: $RES"

exit 0
