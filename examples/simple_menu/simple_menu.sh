#!/usr/bin/env bash

# Variables
# """""""""
PROG=${0#*/}

typeset -a MENU_STACK # A stack of MENUS to store the previously visited menus

# Array of menu characteristics for caching purpose
# '''''''''''''''''''''''''''''''''''''''''''''''''
typeset -A MENU_ARRAY
typeset -A COL_ARRAY
typeset -A CENTERING_ARRAY
typeset -A TITLE_ARRAY
typeset -A ERASE_ARRAY

SEL=   # The selection
SMENU= # The smenu path will be stored here

# ============================ #
# Usage function, always fails #
# ============================ #
function usage
{
  echo "Usage: $PROG menu_file[.mnu] user_program," >&2
  echo "       read the README for an example"      >&2
  exit 1
}

# ==================== #
# Fatal error function #
# ==================== #
function error
{
  echo $* >&2
  exit 1
}

# The script expects exactly one argument (the filename of the root menu).
# """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
(( $# != 2 )) && usage

USER_PROGRAM=$2

# ======================================================================= #
# Parse a level in the menu hierarchy and call process with the selection #
# (possibly empty).                                                       #
# ======================================================================= #
function process_menu
{
  TITLE=$'\n'"[ENTER: select, q: abort]" # Untitled by default
  CENTERING="-M" # Is the menu centered in the screen ?, defaults to yes
  ERASE="-d"  # Destroy the selection window after use. Defaults to yes

  MENU_FILE=$1
  MENU= # Make sure the working area is empty

  # If the menu has already been seen, read its characteristics from the cache
  # else construct them.
  # """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
  if [[ -z ${MENU_ARRAY[$MENU_FILE]} ]]; then
    COL=1

    # Parse the directives embedded in the menu file
    # """"""""""""""""""""""""""""""""""""""""""""""
    MENU_DIRECIVES=$(grep '^\.' $MENU_FILE)

    while read DIRECTIVE VALUE; do
      case $DIRECTIVE in
        .columns) # Number of columns of the menu
           COL=$VALUE
           (( COL < 1 || COL > 15 )) && error "$DIRECTIVE, bad value"
           ;;

        .centered) # Is the menu centered?
           [[ $VALUE == no ]] && CENTERING=""
           ;;

        .eraseafter) # Will the space used by the menu be reclaimed?
           [[ $VALUE == no ]] && ERASE=""
           ;;

        .title) # The menu title
           TITLE="$VALUE"$'\n'$TITLE
           ;;

        *)
           error "bad directive $DIRECTIVE"
           ;;
      esac
    done <<< "$MENU_DIRECIVES"

    # Build the menu entries in the working area
    # """"""""""""""""""""""""""""""""""""""""""
    MENU_LINES=$(grep -v -e '^\.' -e '^#' -e '^[ \t]*$' $MENU_FILE)

    # The special tag "---" creates an empty entry (a hole in a column)
    # The special tag "===" create an empty line
    # The special tag "EXIT" permit to exit the menu
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

    while read TAG VALUE; do
      ITEMS_TO_ADD=1 # By default, only one iteration of the tag is taken
                     # into account

      (( ${#TAG} > 30 )) \
        && error "Menu tag \"${TAG}\" too long (max 30 characters)."

      [[ $TAG == --- ]] && VALUE=@@@
      [[ $TAG == === ]] && VALUE=@@@ && ITEMS_TO_ADD=COL

      [[ -z $VALUE ]] && error "Empty menu entry for $TAG"
      [[ $TAG == EXIT ]] && TAG="@EXIT@xxxxxxxxxxxxxxxxxxxxxxxx00"
      [[ $TAG == "<"* ]] && TAG="<xxxxxxxxxxxxxxxxxxxxxxxxxxxxx00"

      # Protect quotes in VALUE
      # """""""""""""""""""""""
      FINAL_VALUE=$(echo "$VALUE" | sed -e 's/"/\\"/' -e "s/'/\\\\'/")

      while (( ITEMS_TO_ADD-- > 0 )); do
        MENU+="'$TAG $FINAL_VALUE'"$'\n'
      done
    done <<< "$MENU_LINES"

    # Feed the cache
    # """"""""""""""
    MENU_ARRAY[$MENU_FILE]="$MENU"
    COL_ARRAY[$MENU_FILE]=$COL
    CENTERING_ARRAY[$MENU_FILE]=$CENTERING
    TITLE_ARRAY[$MENU_FILE]=$TITLE
    ERASE_ARRAY[$MENU_FILE]=$ERASE
  else
    # Read from the cache
    # """""""""""""""""""
    MENU="${MENU_ARRAY[$MENU_FILE]}"
    COL=${COL_ARRAY[$MENU_FILE]}
    CENTERING=${CENTERING_ARRAY[$MENU_FILE]}
    TITLE=${TITLE_ARRAY[$MENU_FILE]}
    ERASE=${ERASE_ARRAY[$MENU_FILE]}
  fi

  # Display the menu and get the selection
  # """"""""""""""""""""""""""""""""""""""
  SEL=$(echo "$MENU" | $SMENU               \
                         $CENTERING         \
                         $ERASE             \
                         -N                 \
                         -F -D o:30 i:0 n:2 \
                         -n                 \
                         -a m:0/6           \
                         -m "$TITLE"        \
                         -t $COL            \
                         -S'/@@@/ /'        \
                         -S'/^[^ ]+ //v'    \
                         -e @@@)
  SEL=${SEL%% *}
}

# Check for the presence of smenu
# """""""""""""""""""""""""""""""
SMENU=$(which smenu 2>/dev/null)
[[ -x "$SMENU" ]] || error "smenu is not in the PATH or not executable."

# Initialize the menu stack with the argument
# """""""""""""""""""""""""""""""""""""""""""
MENU_STACK+=(${1%.mnu}.mnu)

# And process the main menu
# """""""""""""""""""""""""
process_menu ${1%.mnu}.mnu

# According to the selection, navigate in the submenus or return
# the tag associated with the selected menu entry.
# """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
while true; do
  if [[ $SEL == "<"* ]]; then
    # Back to the previous menu
    # '''''''''''''''''''''''''
    if (( ${#MENU_STACK[*]} == 1 )); then
      process_menu ${MENU_STACK[${#MENU_STACK[*]}-1]} 
    else
      # Unstack the newly found submenu
      # '''''''''''''''''''''''''''''''
      unset ${MENU_STACK[${#MENU_STACK[*]}-1]}

      # And generate the previous menu
      # ''''''''''''''''''''''''''''''
      process_menu ${MENU_STACK[${#MENU_STACK[*]}-1]}
    fi

  elif [[ $SEL == ">"* ]]; then
    # Enter the selected submenu
    # ''''''''''''''''''''''''''
    SMENU_FILE=${SEL#>}
    SMENU_FILE=${SMENU_FILE%.mnu}.mnu
    [[ -f $SMENU_FILE ]] || error "The file $SMENU_FILE was not found/readable."

    # Stack the newly found submenu
    # '''''''''''''''''''''''''''''
    MENU_STACK+=($SMENU_FILE)

    # And generate the submenu
    # ''''''''''''''''''''''''
    process_menu $SMENU_FILE
  else
    # An empty selection means than q or ^C has been hit
    # ''''''''''''''''''''''''''''''''''''''''''''''''''
    [[ -z $SEL ]] && exit 0

    # Output the selected menu tag or exit the menu without outputting
    # anything.
    # ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
    [[ $SEL == @EXIT@* ]] && exit 0

    # Lauch the user action which has the responsibility to act according
    # to the tag passed as argument.
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
    $USER_PROGRAM $SEL

    # And re-generate the current menu
    # ''''''''''''''''''''''''''''''''
    process_menu ${MENU_STACK[-1]}
  fi
done
