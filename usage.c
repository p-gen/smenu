/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* **************** */
/* Usage functions. */
/* **************** */

#include <stdio.h>
#include <stdlib.h>

#include "usage.h"
#include "ctxopt.h"

static void
common_help(void);

#define pusage(args...) fprintf(stderr, args)

/* ======================= */
/* Usage display and exit. */
/* ======================= */
static void
common_help(void)
{
  pusage("-h|-help\n");
  pusage("  displays this help.\n");
  pusage("-u|-usage\n");
  pusage("  displays the synopsis of the current context.\n");
  pusage("-i|-in|-inc|-incl|-include\n");
  pusage("  sets the regex input filter to match the selectable words.\n");
  pusage("-e|-ex|-exc|-excl|-exclude\n");
  pusage("  sets the regex input filter to match the non-selectable "
         "words.\n");
  pusage("-m|-msg|-message|-title\n");
  pusage("  displays a one-line message/title above the window.\n");
  pusage("-!|-int|-int_string\n");
  pusage("  outputs an optional string when ^C is typed.\n");
  pusage("-a|-attr|-attributes\n");
  pusage("  sets the attributes for the various displayed elements.\n");
  pusage("-1|-l1|-level1,-2|-l2|-level2,...,-9|-l9|-level9\n");
  pusage("  gives specific colors to up to 9 classes of "
         "selectable words.\n");
  pusage("-z|-zap|-zap_glyphs bytes\n");
  pusage("  defines a set of glyphs that should be ignored in the input "
         "stream.\n");
  pusage("-n|-lines|-height\n");
  pusage("  sets the maximum number of lines in the selection window.\n");
  pusage("  This number is autodetected when no number is given.\n");
  pusage("-b|-blank\n");
  pusage("  displays non printable characters as space.\n");
  pusage("-.|-dot|-invalid\n");
  pusage("  defines the substitution character for a non-printable "
         "character.\n");
  pusage("-M|-middle|-center\n");
  pusage("  centers the display if possible.\n");
  pusage("-d|-restore|-delete|-clean|-delete_window|-clean_window\n");
  pusage("  clears the lines used by the selection window on exit.\n");
  pusage("-k|-ks|-keep_spaces\n");
  pusage("  does not trim spaces surrounding the output string if any.\n");
  pusage("-W|-ws|-wd|-word_delimiters|-word_separators\n");
  pusage("  defines word separators in the input stream.\n");
  pusage("-L|-ls|-ld|-line-delimiters|-line_separators\n");
  pusage("  defines line separators in the input stream.\n");
  pusage("-q|-no_bar|-no_scroll_bar\n");
  pusage("  prevents the scroll bar from being displayed.\n");
  pusage("-no_hbar|-no_hor_scroll_bar\n");
  pusage("  prevents the horizontal scroll bar from being displayed.\n");
  pusage("-hbar|-hor_scroll_bar\n");
  pusage("  always displays the horizontal scroll bar when certain lines are "
         "truncated.\n");
  pusage("-S|-subst\n");
  pusage("  defines the post-substitution action to apply to all words.\n");
  pusage("-I|-si|-subst_included\n");
  pusage("  defines the post-substitution action to apply to selectable "
         "words only.\n");
  pusage("-E|-se|-subst_excluded\n");
  pusage("  defines the post-substitution action to apply to non-selectable "
         "words only.\n");
  pusage("-ES|-early_subst\n");
  pusage("  defines the early substitution action to apply to all words.\n");
  pusage("-/|-search_method\n");
  pusage("  changes the affectation of the / key (default: fuzzy search).\n");
  pusage("-s|-sp|-start|-start_pattern\n");
  pusage("  sets the initial cursor position (refer to the manual for "
         "more details).\n");
  pusage("-x|-tmout|-timeout/-X|-htmout|-hidden_timeout\n");
  pusage("  defines a timeout and specifies what to do when it expires.\n");
  pusage("-r|-auto_validate\n");
  pusage("  enables ENTER to validate the selection even in search mode.\n");
  pusage("-is|-incremental_search\n");
  pusage("  prevents the search buffer from being reset when starting a new "
         "search\n");
  pusage("  session.\n");
  pusage("-v|-vb|-visual_bell\n");
  pusage("  makes the bell visual (fuzzy search with error).\n");
  pusage("-Q|-ignore_quotes\n");
  pusage("  treats single and double quotes as normal characters.\n");
  pusage("-lim|-limits\n");
  pusage("  overload the words number/max. word length/max columns limits.\n");
  pusage("-al|-align\n");
  pusage("  sets alignments for words selected by regular expressions.\n");
  pusage("-f|-forgotten_timeout|-global_timeout\n");
  pusage("  defines a global inactivity timeout, defaults to 15 min.\n");
  pusage("-nm|-no_mouse\n");
  pusage("  disable a possibly auto-detected mouse tracking support.\n");
  pusage("-br|-buttons|-button_remapping\n");
  pusage("  Remaps the left and right mouse buttons, default is 1 and 3.\n");
  pusage("-dc|-dcd|-double_click|-double_click_delay\n");
  pusage("  Change the mouse double-click delay which is 150 ms by default.\n");
  pusage("-sb|-sbw|-show_blank_words\n");
  pusage("  Make blank words visible and usable even in non column mode.\n");
}

void
main_help(void)
{
  ctxopt_ctx_disp_usage("Main", continue_after);

  pusage("\n----------------------------------------");
  pusage("----------------------------------------\n");

  pusage("\nThis is a filter that gets words from stdin or from a file and ");
  pusage("outputs\n");
  pusage("the selected words (or nothing) on stdout in a nice selection ");
  pusage("window\n\n");
  pusage("The selection window appears on /dev/tty ");
  pusage("just below the current line\n");
  pusage("(no clear screen!).\n\n");

  pusage("Short description of allowed parameters:\n\n");
  common_help();

  pusage("-V|-version\n");
  pusage("  displays the current version and quits.\n");
  pusage("-H|-long_help\n");
  pusage("  displays a full help and the options available in all "
         "contexts.\n");
  pusage("-N|-number/-U|-unnumber\n");
  pusage("  creates or deletes direct access entries for words matching "
         "(or not) a\n");
  pusage("  specific regex.\n");
  pusage("-F|-en|-embedded_number\n");
  pusage("  creates direct access entries by extracting the numbers from the "
         "input words.\n");
  pusage("-c|-col|-col_mode|-column\n");
  pusage("  is like -tab without argument but respects end of lines.\n");
  pusage("-l|-line|-line_mode\n");
  pusage("  is like -col without column alignments.\n");
  pusage("-t|-tab|-tab_mode|-tabulate_mode\n");
  pusage("  tabulates the items. The number of columns can be limited "
         "with\n");
  pusage("  an optional number.\n");
  pusage("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  pusage("  enables the tagging mode (multi-selections). ");
  pusage("An optional parameter\n");
  pusage("  defines the separator string between the selected words ");
  pusage("on the output.\n");
  pusage("  A single space is the default separator.\n");

  pusage("\nNavigation keys are:\n");
  pusage("  - Left/Down/Up/Right arrows or h/j/k/l, H/J/K/L.\n");
  pusage("  - Home/End, SHIFT|CTRL+Home/End CTRK+J/CTRL+K.\n");
  pusage("  - Numbers if some words are numbered (-N/-U/-F).\n");
  pusage("  - SPACE to search for the next match of a previously\n");
  pusage("          entered search prefix if any, see below.\n\n");
  pusage("Other useful keys are:\n");
  pusage("  - Help key (temporary display of a short help line): "
         "?\n");
  pusage("  - Exit key without output (do nothing)             : "
         "q\n");
  pusage("  - Tagging keys: Select/Deselect/Toggle             : "
         "INS/DEL/t\n");
  pusage("  - Selection key                                    : "
         "ENTER\n");
  pusage("  - Cancel key                                       : "
         "ESC\n");
  pusage("  - Search key                                       : "
         "/ or CTRL-F\n\n");
  pusage("The search key activates a timed search mode in which\n");
  pusage("you can enter the first letters of the searched word.\n");
  pusage("When entering this mode you have 10s to start typing\n");
  pusage("and each entered letter restarts this timer.\n");
  pusage("After this time, the normal navigation mode is restored.\n\n");
  pusage("Notes:\n");
  pusage("- the search timer can be cancelled by pressing ESC.\n");
  pusage("- a bad search letter can be removed with ");
  pusage("CTRL-H or Backspace.\n\n");
  pusage("(C) Pierre Gentile.\n\n");

  exit(EXIT_FAILURE);
}

void
columns_help(void)
{
  ctxopt_ctx_disp_usage("Columns", continue_after);

  pusage("\n----------------------------------------");
  pusage("----------------------------------------\n");

  pusage("Short description of allowed parameters:\n\n");
  common_help();

  pusage("-C|-cs|-cols|-cols_select\n");
  pusage("  sets alignments and columns restrictions for selections.\n");
  pusage("-R|-rs|-rows|-rows_select\n");
  pusage("  sets rows restrictions for selections.\n");
  pusage("-w|-wide|-wide_mode\n");
  pusage("  uses all the terminal width for the columns if their numbers "
         "is given.\n");
  pusage("-g|-gutter\n");
  pusage("  separates columns with a character in column or tabulate "
         "mode.\n");
  pusage("-N|-number/-U|-unnumber\n");
  pusage("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  pusage("  (or not) a specific regex.\n");
  pusage("-F|-en|-embedded_number\n");
  pusage("  numbers and provides a direct access to words by extracting the "
         "number\n");
  pusage("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  pusage("  enables the tagging (multi-selections) mode. ");
  pusage("An optional parameter\n");
  pusage("  sets the separator string between the selected words ");
  pusage("on the output.\n");
  pusage("  A single space is the default separator.\n");
  pusage("-A|-fc|-first_column\n");
  pusage("  forces the specified word pattern to start a line.\n");
  pusage("-Z|-lc|-last_column\n");
  pusage("  forces the specified word pattern to end a line.\n");
}

void
lines_help(void)
{
  ctxopt_ctx_disp_usage("Lines", continue_after);

  pusage("\n----------------------------------------");
  pusage("----------------------------------------\n");

  pusage("Short description of allowed parameters:\n\n");
  common_help();

  pusage("-R|-rs|-rows|-row_select\n");
  pusage("  sets alignments and rows restrictions for selections.\n");
  pusage("-N|-number/-U|-unnumber\n");
  pusage("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  pusage("  (or not) a specific regex.\n");
  pusage("-F|-en|-embedded_number\n");
  pusage("  numbers and provides a direct access to words by extracting the "
         "number\n");
  pusage("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  pusage("  enables the tagging (multi-selections) mode. ");
  pusage("An optional parameter\n");
  pusage("  sets the separator string between the selected words ");
  pusage("on the output.\n");
  pusage("  A single space is the default separator.\n");
  pusage("-A|-fc|-first_column\n");
  pusage("  forces the specified word pattern to start a line.\n");
  pusage("-Z|-lc|-last_column\n");
  pusage("  forces the specified word pattern to end a line.\n");
}

void
tabulations_help(void)
{
  ctxopt_ctx_disp_usage("Tabulations", continue_after);

  pusage("\n----------------------------------------");
  pusage("----------------------------------------\n");

  pusage("Short description of allowed parameters:\n\n");
  common_help();

  pusage("-w|-wide|-wide_mode\n");
  pusage("  uses all the terminal width for the columns if their numbers "
         "is given.\n");
  pusage("-g|-gutter\n");
  pusage("  separates columns with a character in column or tabulate "
         "mode.\n");
  pusage("-N|-number/-U|-unnumber\n");
  pusage("  numbers/un-numbers and provides a direct access to words "
         "matching\n");
  pusage("  (or not) a specific regex.\n");
  pusage("-F|-en|-embedded_number\n");
  pusage("  numbers and provides a direct access to words by extracting the "
         "number\n");
  pusage("-T|-tm|-tag|-tag_mode/-P|-pm|-pin|-pin_mode\n");
  pusage("  enables the tagging (multi-selections) mode. ");
  pusage("An optional parameter\n");
  pusage("  sets the separator string between the selected words ");
  pusage("on the output.\n");
  pusage("  A single space is the default separator.\n");
  pusage("-A|-fc|-first_column\n");
  pusage("  forces the specified word pattern to start a line.\n");
  pusage("-Z|-lc|-last_column\n");
  pusage("  forces the specified word pattern to end a line.\n");
}

void
tagging_help(void)
{
  ctxopt_ctx_disp_usage("Tagging", continue_after);

  pusage("\n----------------------------------------");
  pusage("----------------------------------------\n");

  pusage("The following parameters are available in this context:\n\n");
  common_help();

  pusage("-p|-at|-auto_tag\n");
  pusage("  activates the auto-tagging.\n");
  pusage("-0|-noat|-no_auto_tag\n");
  pusage("  do not auto-tag the word under the cursor when in tagged mode\n");
  pusage("  and no other word is selected.\n");

  exit(EXIT_FAILURE);
}
