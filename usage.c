/* **************** */
/* Usage functions. */
/* **************** */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "usage.h"
#include "ctxopt.h"

static void
common_help(void);

/* ======================= */
/* Usage display and exit. */
/* ======================= */
static void
common_help(void)
{
  printf("-h|-help\n");
  printf("  displays this help.\n");
  printf("-u|-usage\n");
  printf("  displays the synopsis of the current context.\n");
  printf("-i|-in|-inc|-incl|-include\n");
  printf("  sets the regex input filter to match the selectable words.\n");
  printf("-e|-ex|-exc|-excl|-exclude\n");
  printf("  sets the regex input filter to match the non-selectable "
         "words.\n");
  printf("-m|-msg|-message|-title\n");
  printf("  displays a one-line message above the window.\n");
  printf("-!|-int|-int_string\n");
  printf("  outputs an optional string when ^C is typed.\n");
  printf("-a|-attr|-attributes\n");
  printf("  sets the attributes for the various displayed elements.\n");
  printf("-1|-l1|-level1,-2|-l2|-level2,...,-5|-l5|-level5\n");
  printf("  gives specific colors to up to 5 classes of "
         "selectable words.\n");
  printf("-z|-zap|-zap_glyphs bytes\n");
  printf("  defines a set of glyphs to ignore as input.\n");
  printf("-n|-lines|-height\n");
  printf("  sets the number of lines in the selection window.\n");
  printf("-b|-blank\n");
  printf("  displays non printable characters as space.\n");
  printf("-.|-dot|-invalid\n");
  printf("  defines the substitution character for a non-printable "
         "character.\n");
  printf("-M|-middle|-center\n");
  printf("  centers the display if possible.\n");
  printf("-d|-restore|-delete|-clean|-delete_window|-clean_window\n");
  printf("  clears the lines used by the selection window on exit.\n");
  printf("-k|-ks|-keep_spaces\n");
  printf("  does not trim spaces surrounding the output string if any.\n");
  printf("-W|-ws|-wd|-word_delimiters|-word_separators\n");
  printf("  sets the input words separators.\n");
  printf("-L|-ls|-ld|-line-delimiters|-line_separators\n");
  printf("  sets the input lines separators.\n");
  printf("-q|-no_bar|-no-scroll_bar\n");
  printf("  prevents the display of the scroll bar.\n");
  printf("-S|-subst\n");
  printf("  sets the post-processing action to apply to all words.\n");
  printf("-I|-si|-subst_included\n");
  printf("  sets the post-processing action to apply to selectable "
         "words only.\n");
  printf("-E|-se|-subst_excluded\n");
  printf("  sets the post-processing action to apply to non-selectable "
         "words only.\n");
  printf("-/|-search_method\n");
  printf("  changes the affectation of the / key (default fuzzy search).\n");
  printf("-s|-sp|-start|-start_pattern\n");
  printf("  sets the initial cursor position (read the manual for "
         "more details).\n");
  printf("-x|-tmout|-timeout/-X|-htmout|-hidden_timeout\n");
  printf("  sets a timeout and specifies what to do when it expires.\n");
  printf("-r|-auto_validate\n");
  printf("  enables ENTER to validate the selection even in search mode.\n");
  printf("-v|-vb|-visual_bell\n");
  printf("  makes the bell visual (fuzzy search with error).\n");
  printf("-Q|-ignore_quotes\n");
  printf("  treats single and double quotes as normal characters.\n");
}

void
main_help(void)
{
  ctxopt_ctx_disp_usage("Main", continue_after);

  printf("\n----------------------------------------");
  printf("----------------------------------------\n");

  printf("\nThis is a filter that gets words from stdin or from a file and ");
  printf("outputs\n");
  printf("the selected words (or nothing) on stdout in a nice selection ");
  printf("window\n\n");
  printf("The selection window appears on /dev/tty ");
  printf("just below the current line\n");
  printf("(no clear screen!).\n\n");

  printf("Short description of allowed parameters:\n\n");
  common_help();

  printf("-V|-version\n");
  printf("  displays the current version and quits.\n");
  printf("-H|-long_help\n");
  printf("  displays a full help and the options available in all "
         "contexts.\n");
  printf("-N|-number/-U|-unnumber\n");
  printf("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  printf("  (or not) a specific regex.\n");
  printf("-F|-en|-embedded_number\n");
  printf("  numbers and provides a direct access to words by extracting the "
         "number\n");
  printf("-c|-col|-col_mode|-column\n");
  printf("  is like|-t without argument but respects end of lines.\n");
  printf("-l|-line|-line_mode\n");
  printf("  is like|-c without column alignments.\n");
  printf("-t|-tab|-tab_mode|-tabulate_mode\n");
  printf("  tabulates the items. The number of columns can be limited "
         "with\n");
  printf("  an optional number.\n");
  printf("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  printf("  enables the tagging (multi-selections) mode. ");
  printf("An optional parameter\n");
  printf("  sets the separator string between the selected words ");
  printf("on the output.\n");
  printf("  A single space is the default separator.\n");

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
  printf("(C) Pierre Gentile.\n\n");

  exit(EXIT_FAILURE);
}

void
columns_help(void)
{
  ctxopt_ctx_disp_usage("Columns", continue_after);

  printf("\n----------------------------------------");
  printf("----------------------------------------\n");

  printf("Short description of allowed parameters:\n\n");
  common_help();

  printf("-C|-cs|-cols|-cols_select\n");
  printf("  sets columns restrictions for selections.\n");
  printf("-R|-rs|-rows|-rows_select\n");
  printf("  sets rows restrictions for selections.\n");
  printf("-w|-wide|-wide_mode\n");
  printf("  uses all the terminal width for the columns if their numbers "
         "is given.\n");
  printf("-g|-gutter\n");
  printf("  separates columns with a character in column or tabulate "
         "mode.\n");
  printf("-N|-number/-U|-unnumber\n");
  printf("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  printf("  (or not) a specific regex.\n");
  printf("-F|-en|-embedded_number\n");
  printf("  numbers and provides a direct access to words by extracting the "
         "number\n");
  printf("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  printf("  enables the tagging (multi-selections) mode. ");
  printf("An optional parameter\n");
  printf("  sets the separator string between the selected words ");
  printf("on the output.\n");
  printf("  A single space is the default separator.\n");
  printf("-A|-fc|-first_column\n");
  printf("  forces the specified word pattern to start a line.\n");
  printf("-Z|-lc|-last_column\n");
  printf("  forces the specified word pattern to end a line.\n");
}

void
lines_help(void)
{
  ctxopt_ctx_disp_usage("Lines", continue_after);

  printf("\n----------------------------------------");
  printf("----------------------------------------\n");

  printf("Short description of allowed parameters:\n\n");
  common_help();

  printf("-R|-rs|-rows|-row_select\n");
  printf("  sets rows restrictions for selections.\n");
  printf("-N|-number/-U|-unnumber\n");
  printf("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  printf("  (or not) a specific regex.\n");
  printf("-F|-en|-embedded_number\n");
  printf("  numbers and provides a direct access to words by extracting the "
         "number\n");
  printf("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  printf("  enables the tagging (multi-selections) mode. ");
  printf("An optional parameter\n");
  printf("  sets the separator string between the selected words ");
  printf("on the output.\n");
  printf("  A single space is the default separator.\n");
  printf("-A|-fc|-first_column\n");
  printf("  forces the specified word pattern to start a line.\n");
  printf("-Z|-lc|-last_column\n");
  printf("  forces the specified word pattern to end a line.\n");
}

void
tabulations_help(void)
{
  ctxopt_ctx_disp_usage("Tabulations", continue_after);

  printf("\n----------------------------------------");
  printf("----------------------------------------\n");

  printf("Short description of allowed parameters:\n\n");
  common_help();

  printf("-w|-wide|-wide_mode\n");
  printf("  uses all the terminal width for the columns if their numbers "
         "is given.\n");
  printf("-g|-gutter\n");
  printf("  separates columns with a character in column or tabulate "
         "mode.\n");
  printf("-N|-number/-U|-unnumber\n");
  printf("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  printf("  (or not) a specific regex.\n");
  printf("-F|-en|-embedded_number\n");
  printf("  numbers and provides a direct access to words by extracting the "
         "number\n");
  printf("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  printf("  enables the tagging (multi-selections) mode. ");
  printf("An optional parameter\n");
  printf("  sets the separator string between the selected words ");
  printf("on the output.\n");
  printf("  A single space is the default separator.\n");
  printf("-A|-fc|-first_column\n");
  printf("  forces the specified word pattern to start a line.\n");
  printf("-Z|-lc|-last_column\n");
  printf("  forces the specified word pattern to end a line.\n");
}

void
tagging_help(void)
{
  ctxopt_ctx_disp_usage("Tagging", continue_after);

  printf("\n----------------------------------------");
  printf("----------------------------------------\n");

  printf("The following parameters are available in this context:\n\n");
  common_help();

  printf("-p|-at|-auto_tag\n");
  printf("  activates the auto-tagging.\n");

  exit(EXIT_FAILURE);
}
