/* *************** */
/* Usage functions */
/* *************** */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "usage.h"

/* =================== */
/* Short usage display */
/* =================== */
void
short_usage(int must_exit)
{
  printf("\nUsage: smenu [-h|-?] [-f config_file] [-n [lines]] ");
  printf("[-t [cols]] [-k] [-v]       \\\n");
  printf("       [-s pattern] [-m message] [-w] [-d] [-M] [-c] [-l] ");
  printf("[-r] [-b]            \\\n");
  printf("       [-a prefix:attr [prefix:attr]...] ");
  printf("[-i regex] [-e regex]                 \\\n");
  printf("       [-C [i|e]<col selectors>] ");
  printf("[-R [i|e]<row selectors>]                     \\\n");
  printf("       [-S /regex/repl/[g][v][s][i]] ");
  printf("[-I /regex/repl/[g][v][s][i]]             \\\n");
  printf("       [-E /regex/repl/[g][v][s][i]] ");
  printf("[-A regex] [-Z regex]                     \\\n");
  printf("       [-N [regex]] [-U [regex]] [-F] [-D arg...] ");
  printf("                             \\\n");
  printf("       [-1 regex [attr]] [-2 regex [attr]]... ");
  printf("[-5 regex [attr]] [-g [string]]  \\\n");
  printf("       [-q] [-W bytes] [-L bytes] [-T [separator]] ");
  printf("[-P [separator]] [-p]       \\\n");
  printf("       [-V] [-x|-X current|quit|word [<word>] ");
  printf("<seconds>]                       \\\n");
  printf("       [-/ prefix|substring|fuzzy] [--] [input_file]\n\n");
  printf("       <col selectors> ::= col1[-col2]...|<RE>...\n");
  printf("       <row selectors> ::= row1[-row2]...|<RE>...\n");
  printf("       <prefix>        ::= i|e|c|b|s|m|t|ct|sf|st|mf|mt|");
  printf("sfe|ste|mfe|mte|da\n");
  printf("       <arg>           ::= [l|r:<char>]|[a:l|r]|[p:i|a]|");
  printf("[w:<size>]|\n");
  printf("                           [f:y|n]|[o:<num>[+]]|[n:<num>]|");
  printf("[i:<num>]|[d:<char>]|\n");
  printf("                           [s:<num>]|[h:t|c|k]\n");
  printf("         Ex: l:'(' a:l\n");
  printf("       <attr>          ::= [fg][/bg][,style] \n");
  printf("         Ex: 7/4,bu\n");
  printf("       <RE>            ::= <char>regex<char> \n");
  printf("         Ex: /regex/ or :regex:\n\n");
  printf("       <col/row selectors> and <RE> can be freely mixed ");
  printf("when used\n");
  printf("       with -C and -R (ex: 2,/x/,4-5).\n\n");

  if (must_exit)
    exit(EXIT_FAILURE);
}

/* ====================== */
/* Usage display and exit */
/* ====================== */
void
usage(void)
{
  short_usage(0);

  printf("This is a filter that gets words from stdin ");
  printf("and outputs the selected\n");
  printf("words (or nothing) on stdout in a nice selection ");
  printf("window\n\n");
  printf("The selection window appears on /dev/tty ");
  printf("immediately after the current line\n");
  printf("(no clear screen!).\n\n");
  printf("The following options are available:\n\n");
  printf("-h displays this help.\n");
  printf("-f selects an alternative configuration file.\n");
  printf("-n sets the number of lines in the selection window.\n");
  printf("-t tabulates the items. The number of columns can be limited "
         "with\n");
  printf("   an optional number.\n");
  printf("-k does not trim spaces surrounding the output string if any.\n");
  printf("-v makes the bell visual (fuzzy search with error).\n");
  printf("-s sets the initial cursor position (read the manual for "
         "more details).\n");
  printf("-m displays a one-line message above the window.\n");
  printf("-w uses all the terminal width for the columns if their numbers "
         "is given.\n");
  printf("-d deletes the selection window on exit.\n");
  printf("-M centers the display if possible.\n");
  printf("-c is like -t without argument but respects end of lines.\n");
  printf("-l is like -c without column alignments.\n");
  printf("-r enables ENTER to validate the selection even in "
         "search mode.\n");
  printf("-b displays the non printable characters as space.\n");
  printf("-a sets the attributes for the various displayed ");
  printf("elements.\n");
  printf("-i sets the regex input filter to match the selectable words.\n");
  printf("-e sets the regex input filter to match the non-selectable "
         "words.\n");
  printf("-C sets columns restrictions for selections.\n");
  printf("-R sets rows restrictions for selections.\n");
  printf("-S sets the post-processing action to apply to all words.\n");
  printf("-I sets the post-processing action to apply to selectable "
         "words only.\n");
  printf("-E sets the post-processing action to apply to non-selectable "
         "words only.\n");
  printf("-A forces a class of words to be the first of the line they "
         "appear in.\n");
  printf("-Z forces a class of words to be the latest of the line they "
         "appear in.\n");
  printf("-N/-U numbers/un-numbers and provides a direct access to words "
         "matching\n");
  printf("   (or not) a specific regex.\n");
  printf("-F numbers and provides a direct access to words by extracting the "
         "number\n");
  printf("   from the words.\n");
  printf("-D sets sub-options to modify the behaviour of -N, -U and -F.\n");
  printf("-1,-2,...,-5 gives specific colors to up to 5 classes of "
         "selectable words.\n");
  printf("-g separates columns with a character in column or tabulate "
         "mode.\n");
  printf("-q prevents the display of the scroll bar.\n");
  printf("-W sets the input words separators.\n");
  printf("-L sets the input lines separators.\n");
  printf("-T/-P enables the tagging (multi-selections) mode. ");
  printf("An optional parameter\n");
  printf("   sets the separator string between the selected words ");
  printf("on the output.\n");
  printf("   A single space is the default separator.\n");
  printf("-p activates the auto-tagging when using -T or -P.\n");
  printf("-V displays the current version and quits.\n");
  printf("-x|-X sets a timeout and specifies what to do when it expires.\n");
  printf("-/ changes the affectation of the / key (default fuzzy search).\n");
  printf("\nNavigation keys are:\n");
  printf("  - Left/Down/Up/Right arrows or h/j/k/l, H/J/K/L.\n");
  printf("  - Home/End, SHIFT|CTRL+Home/End CTRK+J/CTRL+K.\n");
  printf("  - Numbers if some words are numbered (-N/-U/-F).\n");
  printf("  - SPACE to search for the next match of a previously\n");
  printf("          entered search prefix if any, see below.\n\n");
  printf("Other useful keys are:\n");
  printf("  - Help key (temporary display of a short help line): "
         "?\n");
  printf("  - Exit key without output (do nothing)             : "
         "q\n");
  printf("  - Tagging keys: Select/Deselect/Toggle             : "
         "INS/DEL/t\n");
  printf("  - Selection key                                    : "
         "ENTER\n");
  printf("  - Cancel key                                       : "
         "ESC\n");
  printf("  - Search key                                       : "
         "/ or CTRL-F\n\n");
  printf("The search key activates a timed search mode in which\n");
  printf("you can enter the first letters of the searched word.\n");
  printf("When entering this mode you have 7s to start typing\n");
  printf("and each entered letter gives you 5 more seconds before\n");
  printf("the timeout. After that the search mode is ended.\n\n");
  printf("Notes:\n");
  printf("- the timer can be cancelled by pressing ESC.\n");
  printf("- a bad search letter can be removed with ");
  printf("CTRL-H or Backspace.\n\n");
  printf("(C) Pierre Gentile (2015).\n\n");

  exit(EXIT_FAILURE);
}
