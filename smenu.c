/* ################################################################### */
/* Copyright 2015 - Pierre Gentile                                     */
/*                                                                     */
/* This Software is licensed under the GPL licensed Version 2,         */
/* please read http://www.gnu.org/copyleft/gpl.html                    */
/*                                                                     */
/* you can redistribute it and/or modify it under the terms of the GNU */
/* General Public License as published by the Free Software            */
/* Foundation; either version 2 of the License.                        */
/*                                                                     */
/* This software is distributed in the hope that it will be useful,    */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of      */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU   */
/* General Public License for more details.                            */
/* ################################################################### */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#if defined(__sun) && defined(__SVR4)
#include <curses.h>
#endif
#include <term.h>
#include <termios.h>
#include <regex.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <wchar.h>

#include "xmalloc.h"
#include "ptrlist.h"
#include "index.h"
#include "utf8.h"
#include "smenu.h"

/* **************** */
/* Extern variables */
/* **************** */

extern ll_t * tst_search_list;

/* **************** */
/* Global variables */
/* **************** */

word_t * word_a;       /* Array containing words data (size: count)         */
long     dummy_rc;     /* temporary variable to silence the compiler        */
long     count = 0;    /* number of words read from stdin                   */
long     current;      /* index the current selection under the cursor)     */
long     new_current;  /* final current position, (used in search function) */
long     prev_current; /* previous position stored when using direct access */

long * line_nb_of_word_a;    /* array containing the line number *
                              * (from 0) of each word read       */
long * first_word_in_line_a; /* array containing the index of    *
                              * the first word of each lines     */
search_mode_t search_mode     = NONE;
search_mode_t old_search_mode = NONE;

int help_mode = 0; /* 1 if help is display else 0      */

char * word_buffer;

/* UTF-8 useful symbols */
/* """""""""""""""""""" */
char * left_arrow      = "\xe2\x86\x90";
char * up_arrow        = "\xe2\x86\x91";
char * right_arrow     = "\xe2\x86\x92";
char * down_arrow      = "\xe2\x86\x93";
char * vertical_bar    = "\xe2\x94\x82"; /* box drawings light vertical      */
char * shift_left_sym  = "\xe2\x86\x90"; /* leftwards_arrow                  */
char * shift_right_sym = "\xe2\x86\x92"; /* rightwards_arrow                 */
char * sbar_line       = "\xe2\x94\x82"; /* box_drawings_light_vertical      */
char * sbar_top        = "\xe2\x94\x90"; /* box_drawings_light_down_and_left */
char * sbar_down       = "\xe2\x94\x98"; /* box_drawings_light_up_and_left   */
char * sbar_curs       = "\xe2\x95\x91"; /* box_drawings_double_vertical     */
char * sbar_arr_up     = "\xe2\x96\xb2"; /* black_up_pointing_triangle       */
char * sbar_arr_down   = "\xe2\x96\xbc"; /* black_down_pointing_triangle     */

daccess_t daccess;

/* Variables used to manage the direct access entries */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
char * daccess_stack;
int    daccess_stack_head;

/* Variables used for fuzzy and substring searching */
/* """""""""""""""""""""""""""""""""""""""""""""""" */
long * matching_words_a;
long   matching_words_a_size;
long   matches_count;
long * best_matching_words_a;
long   best_matching_words_a_size;
long   best_matches_count;
long * alt_matching_words_a = NULL;
long   alt_matches_count;

/* Variables used in signal handlers */
/* """"""""""""""""""""""""""""""""" */
volatile sig_atomic_t got_winch        = 0;
volatile sig_atomic_t got_winch_alrm   = 0;
volatile sig_atomic_t got_help_alrm    = 0;
volatile sig_atomic_t got_daccess_alrm = 0;
volatile sig_atomic_t got_timeout_tick = 0;

/* Variables used when a timeout is set (option -x) */
/* """""""""""""""""""""""""""""""""""""""""""""""" */
timeout_t timeout;
char *    timeout_word;    /* printed word when the timeout type is WORD.   */
char *    timeout_seconds; /* string containing the number of remaining     *
                            * seconds.                                      */
int quiet_timeout = 0;     /* 1 when we want no message to be displayed.    */

/* ************************************************************************ */
/* Custom fgetc/ungetc implementation able to unget more than one character */
/* ************************************************************************ */

enum
{
  GETC_BUFF_SIZE = 16
};

static char getc_buffer[GETC_BUFF_SIZE] = { '\0' };

static long next_buffer_pos = 0; /* next free position in the getc buffer */

/* ====================================== */
/* Get a (possibly pushed-back) character */
/* ====================================== */
int
my_fgetc(FILE * input)
{
  return (next_buffer_pos > 0) ? getc_buffer[--next_buffer_pos] : fgetc(input);
}

/* ============================ */
/* Push character back on input */
/* ============================ */
void
my_ungetc(int c)
{
  if (next_buffer_pos >= GETC_BUFF_SIZE)
    fprintf(stderr, "Error: cannot push back more than %d characters\n",
            GETC_BUFF_SIZE);
  else
    getc_buffer[next_buffer_pos++] = c;
}

/* ************** */
/* Help functions */
/* ************** */

/* =================== */
/* Short usage display */
/* =================== */
void
short_usage(void)
{
  fprintf(stderr, "Usage: smenu [-h|-?] [-f config_file] [-n lines] ");
  fprintf(stderr, "[-t [cols]] [-k] [-v]         \\\n");
  fprintf(stderr, "       [-s pattern] [-m message] [-w] [-d] [-M] [-c] [-l] ");
  fprintf(stderr, "[-r] [-b]            \\\n");
  fprintf(stderr, "       [-a prefix:attr [prefix:attr]...] ");
  fprintf(stderr, "[-i regex] [-e regex]                 \\\n");
  fprintf(stderr, "       [-C [i|e]<col selectors>] ");
  fprintf(stderr, "[-R [i|e]<row selectors>]                     \\\n");
  fprintf(stderr, "       [-S /regex/repl/[g][v][s][i]] ");
  fprintf(stderr, "[-I /regex/repl/[g][v][s][i]]             \\\n");
  fprintf(stderr, "       [-E /regex/repl/[g][v][s][i]] ");
  fprintf(stderr, "[-A regex] [-Z regex]                     \\\n");
  fprintf(stderr, "       [-N [regex]] [-U [regex]] [-F] [-D arg...] ");
  fprintf(stderr, "                             \\\n");
  fprintf(stderr, "       [-1 regex [attr]] [-2 regex [attr]]... ");
  fprintf(stderr, "[-5 regex [attr]] [-g [string]]  \\\n");
  fprintf(stderr, "       [-q] [-W bytes] [-L bytes] [-T [separator]] ");
  fprintf(stderr, "[-P [separator]] [-p]       \\\n");
  fprintf(stderr, "       [-V] [-x|-X current|quit|word [<word>] ");
  fprintf(stderr, "<seconds>]                       \\\n");
  fprintf(stderr, "       [-/ prefix|substring|fuzzy] [--] [input_file]\n\n");
  fprintf(stderr, "       <col selectors> ::= col1[-col2]...|<RE>...\n");
  fprintf(stderr, "       <row selectors> ::= row1[-row2]...|<RE>...\n");
  fprintf(stderr, "       <prefix>        ::= i|e|c|b|s|t|ct|sf|st|mf|mt|");
  fprintf(stderr, "sfe|ste|mfe|mte|da\n");
  fprintf(stderr, "       <arg>           ::= [l|r:<char>]|[a:l|r]|[p:i|a]|");
  fprintf(stderr, "[w:<size>]|\n");
  fprintf(stderr, "                           [f:y|n]|[o:<num>]|[n:<num>]|");
  fprintf(stderr, "[i:<num>]|[d:<char>]|\n");
  fprintf(stderr, "                           [s:<num>]|[h:t|c|k]\n");
  fprintf(stderr, "         Ex: l:'(' a:l\n");
  fprintf(stderr, "       <attr>          ::= [fg][/bg][,style] \n");
  fprintf(stderr, "         Ex: 7/4,bu\n");
  fprintf(stderr, "       <RE>            ::= <char>regex<char> \n");
  fprintf(stderr, "         Ex: /regex/ or :regex:\n\n");
  fprintf(stderr, "       <col/row selectors> and <RE> can be freely mixed ");
  fprintf(stderr, "when used\n");
  fprintf(stderr, "       with -C and -R (ex: 2,/x/,4-5).\n");
}

/* ====================== */
/* Usage display and exit */
/* ====================== */
void
usage(void)
{
  short_usage();

  fprintf(stderr, "\nThis is a filter that gets words from stdin ");
  fprintf(stderr, "and outputs the selected\n");
  fprintf(stderr, "words (or nothing) on stdout in a nice selection ");
  fprintf(stderr, "window\n\n");
  fprintf(stderr, "The selection window appears on /dev/tty ");
  fprintf(stderr, "immediately after the current line\n");
  fprintf(stderr, "(no clear screen!).\n\n");
  fprintf(stderr, "The following options are available:\n\n");
  fprintf(stderr, "-h displays this help.\n");
  fprintf(stderr, "-f selects an alternative configuration file.\n");
  fprintf(stderr, "-n sets the number of lines in the selection window.\n");
  fprintf(stderr,
          "-t tabulates the items. The number of columns can be limited "
          "with\n");
  fprintf(stderr, "   an optional number.\n");
  fprintf(stderr,
          "-k does not trim spaces surrounding the output string if any.\n");
  fprintf(stderr, "-v makes the bell visual (fuzzy search with error).\n");
  fprintf(stderr,
          "-s sets the initial cursor position (read the manual for "
          "more details).\n");
  fprintf(stderr, "-m displays a one-line message above the window.\n");
  fprintf(stderr,
          "-w uses all the terminal width for the columns if their numbers "
          "is given.\n");
  fprintf(stderr, "-d deletes the selection window on exit.\n");
  fprintf(stderr, "-M centers the display if possible.\n");
  fprintf(stderr,
          "-c is like -t without argument but respects end of lines.\n");
  fprintf(stderr, "-l is like -c without column alignments.\n");
  fprintf(stderr,
          "-r enables ENTER to validate the selection even in "
          "search mode.\n");
  fprintf(stderr, "-b displays the non printable characters as space.\n");
  fprintf(stderr, "-a sets the attributes for the various displayed ");
  fprintf(stderr, "elements.\n");
  fprintf(stderr,
          "-i sets the regex input filter to match the selectable words.\n");
  fprintf(stderr,
          "-e sets the regex input filter to match the non-selectable "
          "words.\n");
  fprintf(stderr, "-C sets columns restrictions for selections.\n");
  fprintf(stderr, "-R sets rows restrictions for selections.\n");
  fprintf(stderr,
          "-S sets the post-processing action to apply to all words.\n");
  fprintf(stderr,
          "-I sets the post-processing action to apply to selectable "
          "words only.\n");
  fprintf(stderr,
          "-E sets the post-processing action to apply to non-selectable "
          "words only.\n");
  fprintf(stderr,
          "-A forces a class of words to be the first of the line they "
          "appear in.\n");
  fprintf(stderr,
          "-Z forces a class of words to be the latest of the line they "
          "appear in.\n");
  fprintf(stderr,
          "-N/-U numbers/un-numbers and provides a direct access to words "
          "matching\n");
  fprintf(stderr, "   (or not) a specific regex.\n");
  fprintf(stderr,
          "-F numbers and provides a direct access to words by extracting the "
          "number\n");
  fprintf(stderr, "   from the words.\n");
  fprintf(stderr,
          "-D sets sub-options to modify the behaviour of -N, -U and -F.\n");
  fprintf(stderr,
          "-1,-2,...,-5 gives specific colors to up to 5 classes of "
          "selectable words.\n");
  fprintf(stderr,
          "-g separates columns with a character in column or tabulate "
          "mode.\n");
  fprintf(stderr, "-q prevents the display of the scroll bar.\n");
  fprintf(stderr, "-W sets the input words separators.\n");
  fprintf(stderr, "-L sets the input lines separators.\n");
  fprintf(stderr, "-T/-P enables the tagging (multi-selections) mode. ");
  fprintf(stderr, "An optional parameter\n");
  fprintf(stderr, "   sets the separator string between the selected words ");
  fprintf(stderr, "on the output.\n");
  fprintf(stderr, "   A single space is the default separator.\n");
  fprintf(stderr, "-p activates the auto-tagging when using -T or -P.\n");
  fprintf(stderr, "-V displays the current version and quits.\n");
  fprintf(stderr,
          "-x|-X sets a timeout and specifies what to do when it expires.\n");
  fprintf(stderr,
          "-/ changes the affectation of the / key (default fuzzy search).\n");
  fprintf(stderr, "\nNavigation keys are:\n");
  fprintf(stderr, "  - Left/Down/Up/Right arrows or h/j/k/l, J/K.\n");
  fprintf(stderr, "  - Home/End, SHIFT|CTRL+Home/End.\n");
  fprintf(stderr, "  - Numbers if some words are numbered (-N/-U/-F).\n");
  fprintf(stderr, "  - SPACE to search for the next match of a previously\n");
  fprintf(stderr, "          entered search prefix if any, see below.\n\n");
  fprintf(stderr, "Other useful keys are:\n");
  fprintf(stderr,
          "  - Help key (temporary display of a short help line): "
          "?\n");
  fprintf(stderr,
          "  - Exit key without output (do nothing)             : "
          "q\n");
  fprintf(stderr,
          "  - Tagging keys: Select/Deselect/Toggle             : "
          "INS/DEL/t\n");
  fprintf(stderr,
          "  - Selection key                                    : "
          "ENTER\n");
  fprintf(stderr,
          "  - Cancel key                                       : "
          "ESC\n");
  fprintf(stderr,
          "  - Search key                                       : "
          "/ or CTRL-F\n\n");
  fprintf(stderr, "The search key activates a timed search mode in which\n");
  fprintf(stderr, "you can enter the first letters of the searched word.\n");
  fprintf(stderr, "When entering this mode you have 7s to start typing\n");
  fprintf(stderr, "and each entered letter gives you 5 more seconds before\n");
  fprintf(stderr, "the timeout. After that the search mode is ended.\n\n");
  fprintf(stderr, "Notes:\n");
  fprintf(stderr, "- the timer can be cancelled by pressing ESC.\n");
  fprintf(stderr, "- a bad search letter can be removed with ");
  fprintf(stderr, "CTRL-H or Backspace.\n\n");
  fprintf(stderr, "(C) Pierre Gentile (2015).\n");

  exit(EXIT_FAILURE);
}

/* ==================== */
/* Help message display */
/* ==================== */
void
help(win_t * win, term_t * term, long last_line, toggle_t * toggle)
{
  int index;      /* used to identify the objects long the help line */
  int line   = 0; /* number of windows lines used by the help line  */
  int len    = 0; /* length of the help line                        */
  int offset = 0; /* offset from the first column of the terminal   *
                   * to the start of the help line                  */
  int entries_nb; /* number of help entries to display              */
  int help_len;   /* total length of the help line                  */

  struct entry_s
  {
    char   attr; /* r=reverse, n=normal, b=bold                           */
    char * str;  /* string to be displayed for an object in the help line */
    int    len;  /* length of one of these objects                        */
  };

  char * arrows = concat(left_arrow, up_arrow, right_arrow, down_arrow, NULL);
  struct entry_s entries[] =
    { { 'n', "Move:", 5 }, { 'b', arrows, 4 },   { 'n', "|", 1 },
      { 'b', "h", 1 },     { 'b', "j", 1 },      { 'b', "k", 1 },
      { 'b', "l", 1 },     { 'n', ",", 1 },      { 'b', "PgUp", 4 },
      { 'n', "/", 1 },     { 'b', "PgDn", 4 },   { 'n', "/", 1 },
      { 'b', "Home", 4 },  { 'n', "/", 1 },      { 'b', "End", 3 },
      { 'n', " ", 1 },     { 'n', "Abort:", 6 }, { 'b', "q", 1 },
      { 'n', " ", 1 },     { 'n', "Find:", 5 },  { 'b', "/", 1 },
      { 'n', "|", 1 },     { 'b', "\"\'", 2 },   { 'n', "|", 1 },
      { 'b', "~*", 2 },    { 'n', "|", 1 },      { 'b', "=^", 2 },
      { 'n', ",", 1 },     { 'b', "SP", 2 },     { 'n', "|", 1 },
      { 'b', "nN", 2 },    { 'n', " ", 1 },      { 'n', "Select:", 7 },
      { 'b', "CR", 2 },    { 'n', "|", 1 },      { 'b', "tTU", 3 } };

  entries_nb = sizeof(entries) / sizeof(struct entry_s);

  /* Remove the last two entries if tagging is not enabled */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!toggle->taggable)
    entries_nb -= 2;

  /* Get the total length of the help line */
  /* """"""""""""""""""""""""""""""""""""" */
  help_len = 0;
  for (index = 0; index < entries_nb; index++)
    help_len += entries[index].len;

  /* Save the position of the terminal cursor so that it can be */
  /* put back there after printing of the help line             */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tputs(TPARM1(save_cursor), 1, outch);

  /* Center the help line if the -M (Middle option is set. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->offset > 0)
    if ((offset = win->offset + win->max_width / 2 - help_len / 2) > 0)
    {
      int i;

      for (i = 0; i < offset; i++)
        fputc(' ', stdout);
    }

  /* Print the different objects forming the help line */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  for (index = 0; index < entries_nb; index++)
  {
    if ((len += entries[index].len) >= term->ncolumns - 1)
    {
      line++;

      if (line > last_line || line == win->max_lines)
        break;

      len = entries[index].len;
      fputc('\n', stdout);
    }

    switch (entries[index].attr)
    {
      case 'b':
        if (term->has_bold)
          tputs(TPARM1(enter_bold_mode), 1, outch);
        break;
      case 'r':
        if (term->has_reverse)
          tputs(TPARM1(enter_reverse_mode), 1, outch);
        else if (term->has_standout)
          tputs(TPARM1(enter_standout_mode), 1, outch);
        break;
      case 'n':
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        break;
    }
    fputs(entries[index].str, stdout);
  }

  tputs(TPARM1(exit_attribute_mode), 1, outch);
  tputs(TPARM1(clr_eol), 1, outch);

  /* Relocate the cursor to its saved position */
  /* """"""""""""""""""""""""""""""""""""""""" */
  tputs(TPARM1(restore_cursor), 1, outch);
}

/* *************************** */
/* Memory allocation functions */
/* *************************** */

/* Created by Kevin Locke (from numerous canonical examples)         */
/*                                                                   */
/* I hereby place this file in the public domain.  It may be freely  */
/* reproduced, distributed, used, modified, built upon, or otherwise */
/* employed by anyone for any purpose without restriction.           */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* ********************************** */
/* Attributes string parsing function */
/* ********************************** */

/* ================================ */
/* Decode attributes toggles if any */
/* b -> bold                        */
/* d -> dim                         */
/* r -> reverse                     */
/* s -> standout                    */
/* u -> underline                   */
/* i -> italic                      */
/*                                  */
/* Returns 0 if some unexpected     */
/* toggle is found else 0           */
/* ================================ */
int
decode_attr_toggles(char * s, attr_t * attr)
{
  int rc = 1;

  attr->bold = attr->dim = attr->reverse = 0;
  attr->standout = attr->underline = attr->italic = 0;

  while (*s != '\0')
  {
    switch (*s)
    {
      case 'b':
        attr->bold   = 1;
        attr->is_set = SET;
        break;
      case 'd':
        attr->dim    = 1;
        attr->is_set = SET;
        break;
      case 'r':
        attr->reverse = 1;
        attr->is_set  = SET;
        break;
      case 's':
        attr->standout = 1;
        attr->is_set   = SET;
        break;
      case 'u':
        attr->underline = 1;
        attr->is_set    = SET;
        break;
      case 'i':
        attr->italic = 1;
        attr->is_set = SET;
        break;
      default:
        rc = 0;
        break;
    }
    s++;
  }
  return rc;
}

/* =========================================================*/
/* Parse attributes in str in the form [fg][/bg][,toggles]  */
/* where:                                                   */
/* fg and bg are short representing a color value           */
/* toggles is an array of toggles (see decode_attr_toggles) */
/* Returns 1 on success else 0                              */
/* attr will be filled by the function                      */
/* =========================================================*/
int
parse_attr(char * str, attr_t * attr, short max_color)
{
  int    n;
  char * pos;
  char   s1[12] = { 0 };
  char   s2[7]  = { 0 };
  short  d1 = -1, d2 = -1;
  int    rc = 1;

  n = sscanf(str, "%11[^,],%6s", s1, s2);

  if (n == 0)
    return 0;

  if ((pos = strchr(s1, '/')))
  {
    if (pos == s1) /* s1 starts with a / */
    {
      d1 = -1;
      if (sscanf(s1 + 1, "%hd", &d2) == 0)
      {
        d2 = -1;
        if (n == 1)
          return 0;
      }
    }
    else if (sscanf(s1, "%hd/%hd", &d1, &d2) < 2)
    {
      d1 = d2 = -1;
      if (n == 1)
        return 0;
    }
  }
  else /* no / in the first string */
  {
    d2 = -1;
    if (sscanf(s1, "%hd", &d1) == 0)
    {
      d1 = -1;
      if (n == 2 || decode_attr_toggles(s1, attr) == 0)
        return 0;
    }
  }

  if (d1 < -1 || d2 < -1)
    return 0;

  if (max_color == 0)
  {
    attr->fg = -1;
    attr->bg = -1;
  }
  else
  {
    attr->fg = d1;
    attr->bg = d2;
  }

  if (n == 2)
    rc = decode_attr_toggles(s2, attr);

  return rc;
}

/* ============================================= */
/* Set the terminal attributes according to attr */
/* ============================================= */
void
apply_attr(term_t * term, attr_t attr)
{
  if (attr.fg >= 0)
    set_foreground_color(term, attr.fg);

  if (attr.bg >= 0)
    set_background_color(term, attr.bg);

  if (attr.bold > 0)
    tputs(TPARM1(enter_bold_mode), 1, outch);

  if (attr.dim > 0)
    tputs(TPARM1(enter_dim_mode), 1, outch);

  if (attr.reverse > 0)
    tputs(TPARM1(enter_reverse_mode), 1, outch);

  if (attr.standout > 0)
    tputs(TPARM1(enter_standout_mode), 1, outch);

  if (attr.underline > 0)
    tputs(TPARM1(enter_underline_mode), 1, outch);

  if (attr.italic > 0)
    tputs(TPARM1(enter_italics_mode), 1, outch);
}

/* ******************** */
/* ini parsing function */
/* ******************** */

/* ===================================================== */
/* Callback function called when parsing each non-header */
/* line of the ini file.                                 */
/* Returns 0 if OK, 1 if not.                            */
/* ===================================================== */
int
ini_cb(win_t * win, term_t * term, limits_t * limits, timers_t * timers,
       misc_t * misc, const char * section, const char * name, char * value)
{
  int error      = 0;
  int has_colors = (term->colors > 7);

  if (strcmp(section, "colors") == 0)
  {
    attr_t v = { UNSET, -1, -1, -1, -1, -1, -1, -1, -1 };

#define CHECK_ATTR(x)                             \
  else if (strcmp(name, #x) == 0)                 \
  {                                               \
    error = !parse_attr(value, &v, term->colors); \
    if (error)                                    \
      goto out;                                   \
    else                                          \
    {                                             \
      if (win->x##_attr.is_set != FORCED)         \
      {                                           \
        win->x##_attr.is_set = SET;               \
        if (v.fg >= 0)                            \
          win->x##_attr.fg = v.fg;                \
        if (v.bg >= 0)                            \
          win->x##_attr.bg = v.bg;                \
        if (v.bold >= 0)                          \
          win->x##_attr.bold = v.bold;            \
        if (v.dim >= 0)                           \
          win->x##_attr.dim = v.dim;              \
        if (v.reverse >= 0)                       \
          win->x##_attr.reverse = v.reverse;      \
        if (v.standout >= 0)                      \
          win->x##_attr.standout = v.standout;    \
        if (v.underline >= 0)                     \
          win->x##_attr.underline = v.underline;  \
        if (v.italic >= 0)                        \
          win->x##_attr.italic = v.italic;        \
      }                                           \
    }                                             \
  }

#define CHECK_ATT_ATTR(x, y)                            \
  else if (strcmp(name, #x #y) == 0)                    \
  {                                                     \
    error = !parse_attr(value, &v, term->colors);       \
    if (error)                                          \
      goto out;                                         \
    else                                                \
    {                                                   \
      if (win->x##_attr[y - 1].is_set != FORCED)        \
      {                                                 \
        win->x##_attr[y - 1].is_set = SET;              \
        if (v.fg >= 0)                                  \
          win->x##_attr[y - 1].fg = v.fg;               \
        if (v.bg >= 0)                                  \
          win->x##_attr[y - 1].bg = v.bg;               \
        if (v.bold >= 0)                                \
          win->x##_attr[y - 1].bold = v.bold;           \
        if (v.dim >= 0)                                 \
          win->x##_attr[y - 1].dim = v.dim;             \
        if (v.reverse >= 0)                             \
          win->x##_attr[y - 1].reverse = v.reverse;     \
        if (v.standout >= 0)                            \
          win->x##_attr[y - 1].standout = v.standout;   \
        if (v.underline >= 0)                           \
          win->x##_attr[y - 1].underline = v.underline; \
        if (v.italic >= 0)                              \
          win->x##_attr[y - 1].italic = v.italic;       \
      }                                                 \
    }                                                   \
  }

    /* [colors] section */
    /* """""""""""""""" */
    if (has_colors)
    {
      if (strcmp(name, "method") == 0)
      {
        if (strcmp(value, "classic") == 0)
          term->color_method = 0;
        else if (strcmp(value, "ansi") == 0)
          term->color_method = 1;
        else
        {
          error = 1;
          goto out;
        }
      }

      /* clang-format off */
      CHECK_ATTR(cursor)
      CHECK_ATTR(bar)
      CHECK_ATTR(shift)
      CHECK_ATTR(search_field)
      CHECK_ATTR(search_text)
      CHECK_ATTR(match_field)
      CHECK_ATTR(match_text)
      CHECK_ATTR(match_err_field)
      CHECK_ATTR(match_err_text)
      CHECK_ATTR(include)
      CHECK_ATTR(exclude)
      CHECK_ATTR(tag)
      CHECK_ATTR(cursor_on_tag)
      CHECK_ATTR(daccess)
      CHECK_ATT_ATTR(special, 1)
      CHECK_ATT_ATTR(special, 2)
      CHECK_ATT_ATTR(special, 3)
      CHECK_ATT_ATTR(special, 4)
      CHECK_ATT_ATTR(special, 5)
      /* clang-format on */
    }
  }
  else if (strcmp(section, "window") == 0)
  {
    int v;

    /* [window] section */
    /* """""""""""""""" */
    if (strcmp(name, "lines") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v >= 0)))
        goto out;
      else
        win->asked_max_lines = v;
    }
  }
  else if (strcmp(section, "limits") == 0)
  {
    int v;

    /* [limits] section */
    /* """""""""""""""" */
    if (strcmp(name, "word_length") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        limits->word_length = v;
    }
    else if (strcmp(name, "words") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        limits->words = v;
    }
    else if (strcmp(name, "columns") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        limits->cols = v;
    }
  }
  else if (strcmp(section, "timers") == 0)
  {
    int v;

    /* [timers] section */
    /* """""""""""""""" */
    if (strcmp(name, "help") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->help = v;
    }
    else if (strcmp(name, "window") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->winch = v;
    }
    else if (strcmp(name, "direct_access") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->direct_access = v;
    }
  }
  else if (strcmp(section, "misc") == 0)
  {
    /* [misc] section */
    /* """""""""""""""" */
    if (strcmp(name, "default_search_method") == 0)
      if (misc->default_search_method == NONE)
      {
        if (strcmp(value, "prefix") == 0)
          misc->default_search_method = PREFIX;
        else if (strcmp(value, "fuzzy") == 0)
          misc->default_search_method = FUZZY;
        else if (strcmp(value, "substring") == 0)
          misc->default_search_method = SUBSTRING;
      }
  }

out:

  return error;
}

/* ========================================================================= */
/* Load an .ini format file                                                  */
/* filename - path to a file                                                 */
/* report - callback can return non-zero to stop, the callback error code is */
/*     returned from this function.                                          */
/* return - return 0 on success                                              */
/*                                                                           */
/* This function is public domain. No copyright is claimed.                  */
/* Jon Mayo April 2011                                                       */
/* ========================================================================= */
int
ini_load(const char * filename, win_t * win, term_t * term, limits_t * limits,
         timers_t * timers, misc_t * misc,
         int (*report)(win_t * win, term_t * term, limits_t * limits,
                       timers_t * timers, misc_t * misc, const char * section,
                       const char * name, char * value))
{
  char   name[64]     = "";
  char   value[256]   = "";
  char   section[128] = "";
  char * s;
  FILE * f;
  int    cnt;
  int    error;

  /* If the filename is empty we skip this phase and use the default values */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (filename == NULL)
    return 1;

  /* We do that if the file is not readable as well */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  f = fopen(filename, "r");
  if (!f)
    return 0; /* Returns success as the presence of this file is optional */

  error = 0;

  /* Skip blank lines */
  /* """""""""""""""" */
  while (fscanf(f, "%*[\n]") == 1)
  {
  }

  while (!feof(f))
  {
    if (fscanf(f, "[%127[^];\n]]", section) == 1)
    {
      /* Do nothing */
      /* """""""""" */
    }
    else if ((cnt = fscanf(f, " %63[^=;\n] = %255[^;\n]", name, value)))
    {
      if (cnt == 1)
        *value = 0;

      for (s = name + strlen(name) - 1; s > name && isspace(*s); s--)
        *s = 0;

      for (s = value + strlen(value) - 1; s > value && isspace(*s); s--)
        *s = 0;

      /* Callback function calling */
      /* """"""""""""""""""""""""" */
      error = report(win, term, limits, timers, misc, section, name, value);

      if (error)
        goto out;
    }
    if (fscanf(f, " ;%*[^\n]"))
    {
      /* To silence the compiler about unused results */
    }

    /* Skip blank lines */
    /* """""""""""""""" */
    while (fscanf(f, "%*[\n]") == 1)
    {
      /* Do nothing */
      /* """""""""" */
    }
  }

out:
  fclose(f);

  if (error)
    fprintf(stderr, "Invalid entry: %s=%s in %s, exiting.\n", name, value,
            filename);

  return error;
}

/* ======================================================= */
/* Return the full path on the configuration file supposed */
/* to be in the home directory of the user.                */
/* NULL is returned if the built path is too large.        */
/* ======================================================= */
char *
make_ini_path(char * name, char * base)
{
  char * path;
  char * home;
  long   path_max;
  long   len;
  char * conf;

  home = getenv(base);

  if (home == NULL)
    home = "";

  path_max = pathconf(".", _PC_PATH_MAX);
  len      = strlen(home) + strlen(name) + 3;

  if (path_max < 0)
    path_max = 4096; /* POSIX minimal value */

  if (len <= path_max)
  {
    path = xmalloc(len);
    conf = strrchr(name, '/');
    if (conf != NULL)
      conf++;
    else
      conf = name;

    snprintf(path, 4096, "%s/.%s", home, conf);
  }
  else
    path = NULL;

  return path;
}

/* ****************** */
/* Interval functions */
/* ****************** */

/* ===================== */
/* Create a new interval */
/* ===================== */
interval_t *
interval_new(void)
{
  interval_t * ret = xmalloc(sizeof(interval_t));

  return ret;
}

/* ====================================== */
/* Compare 2 intervals as integer couples */
/* same return values as for strcmp       */
/* ====================================== */
int
interval_comp(void * a, void * b)
{
  interval_t * ia = (interval_t *)a;
  interval_t * ib = (interval_t *)b;

  if (ia->low < ib->low)
    return -1;
  if (ia->low > ib->low)
    return 1;
  if (ia->high < ib->high)
    return -1;
  if (ia->high > ib->high)
    return 1;

  return 0;
}

/* ================================ */
/* Swap the values of two intervals */
/* ================================ */
void
interval_swap(void * a, void * b)
{
  interval_t * ia = (interval_t *)a;
  interval_t * ib = (interval_t *)b;
  long         tmp;

  tmp     = ia->low;
  ia->low = ib->low;
  ib->low = tmp;

  tmp      = ia->high;
  ia->high = ib->high;
  ib->high = tmp;
}

/* =========================================================== */
/* Compare the pin order of two pinned word in the output list */
/* =========================================================== */
int
tag_comp(void * a, void * b)
{
  output_t * oa = (output_t *)a;
  output_t * ob = (output_t *)b;

  if (oa->order == ob->order)
    return 0;

  return (oa->order < ob->order) ? -1 : 1;
}

/* ======================================================== */
/* Swap the values of two selected words in the output list */
/* ======================================================== */
void
tag_swap(void * a, void * b)
{
  output_t * oa = (output_t *)a;
  output_t * ob = (output_t *)b;
  char *     tmp_str;
  long       tmp_order;

  tmp_str        = oa->output_str;
  oa->output_str = ob->output_str;
  ob->output_str = tmp_str;

  tmp_order = oa->order;
  oa->order = ob->order;
  ob->order = tmp_order;
}

/* ***************** */
/* Utility functions */
/* ***************** */

/* =================================================================== */
/* Create a new element to be added to the tst_search_list used by the */
/* search mechanism.                                                   */
/* =================================================================== */
sub_tst_t *
sub_tst_new(void)
{
  sub_tst_t * elem = xmalloc(sizeof(sub_tst_t));

  elem->size  = 64;
  elem->count = 0;
  elem->array = xmalloc(elem->size * sizeof(tst_node_t));

  return elem;
}

/* =============================== */
/* 7 bits aware version of isprint */
/* =============================== */
int
isprint7(int i)
{
  return (i >= 0x20 && i <= 0x7e);
}

/* =============================== */
/* 8 bits aware version of isprint */
/* =============================== */
int
isprint8(int i)
{
  unsigned char c = i & (unsigned char)0xff;

  return (c >= 0x20 && c < 0x7f) || (c >= (unsigned char)0xa0);
}

void
beep(toggle_t * toggle)
{
  struct timespec ts, rem;
  int             rc;

  if (!toggle->visual_bell)
    fputc('\a', stdout);
  else
  {
    tputs(TPARM1(cursor_visible), 1, outch);

    ts.tv_sec  = 0;
    ts.tv_nsec = 200000000;

    errno = 0;
    rc    = nanosleep(&ts, &rem);

    while (rc < 0 && errno == EINTR)
    {
      errno = 0;
      rc    = nanosleep(&rem, &rem);
    }

    tputs(TPARM1(cursor_invisible), 1, outch);
  }
}

/* ======================================================================== */
/* Update the bitmap associated with a word. This bitmap indicates the      */
/* positions of the searched mb characters previously entered in the search */
/* buffer.                                                                  */
/* The disp_word function will use it to display these special characters.  */
/* ======================================================================== */
void
update_bitmaps(search_mode_t mode, search_data_t * data,
               bitmap_affinity_t affinity)
{
  long   i, j, n;
  long   fc, bc;
  long   mb;
  char * start;
  char * p;
  char * bm;
  char * str;
  char * str_orig;
  char * first_mb;

  char * sb_orig = data->buf;
  char * sb;
  long * o    = data->utf8_off_a;
  long * l    = data->utf8_len_a;
  long   last = data->utf8_len - 1;

  long badness;

  best_matches_count = 0;

  if (mode == FUZZY || mode == SUBSTRING)
  {
    first_mb = xmalloc(5);

    if (mode == FUZZY)
    {
      sb = xstrdup(sb_orig);
      utf8_strtolower(sb, sb_orig);
    }
    else
      sb = sb_orig;

    for (i = 0; i < matches_count; i++)
    {
      n = matching_words_a[i];

      str_orig = xstrdup(word_a[n].str + daccess.flength);

      /* We need to remove the trailing spaces to use the     */
      /* following algorithm                                  */
      /* .len holds the original length in bytes of the word  */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      str_orig[word_a[n].len] = '\0';

      mb = (word_a[n].mb - daccess.flength) / CHAR_BIT + 1;
      bm = word_a[n].bitmap;

      if (mode == FUZZY)
      {
        str = xstrdup(str_orig);
        utf8_strtolower(str, str_orig);
      }
      else
        str = str_orig;

      start = str;
      fc    = 0;

      while (start - str < word_a[n].len - daccess.flength)
      {
        /* Reset the bitmap */
        /* """""""""""""""" */
        memset(bm, '\0', mb);

        if (memcmp(start, sb + o[last], l[last]) == 0)
        {
          /* As there is only on glyph in the search buffer, we can */
          /* stop here.                                             */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (last == 0)
          {
            BIT_ON(bm, fc);
            if (affinity != END_AFFINITY)
              break;
          }

          /* If the search buffer contains more than one glyph, we need  */
          /* to search the first combination which match the buffer in   */
          /* the word.                                                   */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          p = start;
          j = last;

          /* Proceed backwards from the position of last glyph of the search */
          /* to check if all the previous glyphs ca be fond before in the    */
          /* word. If not try to find the next position of this last glyph   */
          /* in the word.                                                    */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          bc = fc;
          while (j > 0 && (p = utf8_prev(str, p)) != NULL)
          {
            if (memcmp(p, sb + o[j - 1], l[j - 1]) == 0)
            {
              BIT_ON(bm, bc - 1);
              j--;
            }
            else if (mode == SUBSTRING)
              break;

            bc--;
          }

          /* All the glyphs have been found */
          /* """""""""""""""""""""""""""""" */
          if (j == 0)
          {
            BIT_ON(bm, fc);
            if (affinity != END_AFFINITY)
              break;
          }
        }

        fc++;
        start = utf8_next(start);
      }
      if (mode == FUZZY)
      {
        size_t utf8_index;

        free(str);

        /* We know that the first glyph is part of the pattern, so        */
        /* highlight it if it is not and un-highlight the next occurrence */
        /* that must be here because this word has already been filtered  */
        /* by select_staring_pattern()                                    */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (affinity == START_AFFINITY)
        {
          char *ptr1, *ptr2;
          long  i = 1;
          long  utf8_len;

          first_mb = utf8_strprefix(first_mb, word_a[n].str, 1, &utf8_len);

          if (!BIT_ISSET(word_a[n].bitmap, 0))
          {
            BIT_ON(word_a[n].bitmap, 0);
            ptr1 = word_a[n].str;
            while ((ptr2 = utf8_next(ptr1)) != NULL)
            {
              if (memcmp(ptr2, first_mb, utf8_len) == 0)
              {
                if (BIT_ISSET(word_a[n].bitmap, i))
                {
                  BIT_OFF(word_a[n].bitmap, i);
                  break;
                }
                else
                  ptr1 = ptr2;
              }
              else
                ptr1 = ptr2;

              i++;
            }
          }
        }

        /* Compute the number of 'holes' in the bitmap to determine the  */
        /* badness of a match. Th goal is to put the cursor on the word  */
        /* with the smallest badness.                                    */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        utf8_index = 0;
        j          = 0;
        badness    = 0;

        while (utf8_index < word_a[n].mb
               && !BIT_ISSET(word_a[n].bitmap, utf8_index))
          utf8_index++;

        while (utf8_index < word_a[n].mb)
        {
          if (!BIT_ISSET(word_a[n].bitmap, utf8_index))
            badness++;
          else
            j++;

          /* Stop here if all the possible bits has been checked as they  */
          /* cannot be more numerous than the number of UTF-8 glyphs in   */
          /* the search buffer.                                           */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (j == data->utf8_len)
            break;

          utf8_index++;
        }
      }
      free(str_orig);

      if (search_mode == FUZZY)
      {
        /* When the badness is zero (best match), add the word position. */
        /* at the end of a special array which will be used to move the. */
        /* cursor among this category of words.                          */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (badness == 0)
        {
          if (best_matches_count == best_matching_words_a_size)
          {
            best_matching_words_a = xrealloc(best_matching_words_a,
                                             (best_matching_words_a_size + 16)
                                               * sizeof(long));
            best_matching_words_a_size += 16;
          }

          best_matching_words_a[best_matches_count] = n;

          best_matches_count++;
        }
      }
    }
    if (mode == FUZZY)
      free(sb);

    free(first_mb);
  }
  else if (mode == PREFIX)
  {
    for (i = 0; i < matches_count; i++)
    {
      n  = matching_words_a[i];
      bm = word_a[n].bitmap;
      mb = (word_a[n].mb - daccess.flength) / CHAR_BIT + 1;

      memset(bm, '\0', mb);

      for (j = 0; j <= last; j++)
        BIT_ON(bm, j);
    }
  }
}

/* ====================================================== */
/* Find the next word index in the list of matching words */
/* returns -1 of not found.                               */
/* ====================================================== */
long
find_next_matching_word(long * array, long nb, long value, long * index)
{
  long left = 0, right = nb, middle;

  /* Use the cached value when possible */
  /* """""""""""""""""""""""""""""""""" */
  if (*index >= 0 && *index < nb - 1 && array[*index] == value)
    return (array[++(*index)]);

  if (nb > 0)
  {
    /* bisection search */
    /* """""""""""""""" */
    while (left < right)
    {
      middle = (left + right) / 2;

      if (value < array[middle])
        right = middle;
      else
        left = middle + 1;
    }
    if (left < nb - 1)
    {
      *index = left;
      return array[*index];
    }
    else
    {
      if (value > array[nb - 1])
      {
        *index = -1;
        return *index;
      }
      else
      {
        *index = nb - 1;
        return array[*index];
      }
    }
  }
  else
  {
    *index = -1;
    return *index;
  }
}

/* ========================================================== */
/* Find the previous word index in the list of matching words */
/* returns -1 of not found.                                   */
/* ========================================================== */
long
find_prev_matching_word(long * array, long nb, long value, long * index)
{
  long left = 0, right = nb, middle;

  /* Use the cached value when possible */
  /* """""""""""""""""""""""""""""""""" */
  if (*index > 0 && array[*index] == value)
    return (array[--(*index)]);

  if (nb > 0)
  {
    /* bisection search */
    /* """""""""""""""" */
    while (left < right)
    {
      middle = (left + right) / 2;

      if (array[middle] == value)
      {
        if (middle > 0)
        {
          *index = middle - 1;
          return array[*index];
        }
        else
        {
          *index = -1;
          return *index;
        }
      }

      if (value < array[middle])
        right = middle;
      else
        left = middle + 1;
    }
    if (left > 0)
    {
      *index = left - 1;
      return array[*index];
    }
    else
    {
      *index = -1;
      return *index;
    }
  }
  else
  {
    *index = -1;
    return *index;
  }
}

/* ================================================================ */
/* Remove all traces of the matched words and redisplay the windows */
/* ================================================================ */
void
clean_matches(search_data_t * search_data, long size)
{
  sub_tst_t * sub_tst_data;
  ll_node_t * fuzzy_node;
  long        i;

  /* Clean the list of lists data-structure containing the search levels */
  /* Note that the first element of the list is never deleted.           */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (tst_search_list)
  {
    fuzzy_node = tst_search_list->tail;
    while (tst_search_list->len > 1)
    {
      sub_tst_data = (sub_tst_t *)(fuzzy_node->data);

      free(sub_tst_data->array);
      free(sub_tst_data);

      ll_delete(tst_search_list, tst_search_list->tail);
      fuzzy_node = tst_search_list->tail;
    }
    sub_tst_data        = (sub_tst_t *)(fuzzy_node->data);
    sub_tst_data->count = 0;
  }

  search_data->fuzzy_err = 0;

  /* Clean the search buffer */
  /* """"""""""""""""""""""" */
  memset(search_data->buf, '\0', size - daccess.flength);
  search_data->len           = 0;
  search_data->utf8_len      = 0;
  search_data->only_ending   = 0;
  search_data->only_starting = 0;

  /* Clean the match flags and bitmaps */
  /* """"""""""""""""""""""""""""""""" */
  for (i = 0; i < matches_count; i++)
  {
    long n = matching_words_a[i];

    word_a[n].is_matching = 0;

    memset(word_a[n].bitmap, '\0',
           (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
  }

  matches_count = 0;
}

/* ************************** */
/* Terminal utility functions */
/* ************************** */

/* ===================================================================== */
/* outch is a function version of putchar that can be passed to tputs as */
/* a routine to call.                                                    */
/* ===================================================================== */
int
#ifdef __sun
outch(char c)
#else
outch(int c)
#endif
{
  putchar(c);
  return 1;
}

/* =============================================== */
/* Set the terminal in non echo/non canonical mode */
/* wait for at least one byte, no timeout.         */
/* =============================================== */
void
setup_term(int const fd)
{
  /* Save the terminal parameters and configure it in row mode */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tcgetattr(fd, &old_in_attrs);

  new_in_attrs.c_iflag = 0;
  new_in_attrs.c_oflag = old_in_attrs.c_oflag;
  new_in_attrs.c_cflag = old_in_attrs.c_cflag;
  new_in_attrs.c_lflag = old_in_attrs.c_lflag & ~(ICANON | ECHO | ISIG);

  new_in_attrs.c_cc[VMIN]  = 1; /* wait for at least 1 byte */
  new_in_attrs.c_cc[VTIME] = 0; /* no timeout               */

  tcsetattr(fd, TCSANOW, &new_in_attrs);
}

/* ===================================== */
/* Set the terminal in its previous mode */
/* ===================================== */
void
restore_term(int const fd)
{
  tcsetattr(fd, TCSANOW, &old_in_attrs);
}

/* ============================================== */
/* Get the terminal numbers of lines and columns  */
/* Assume that the TIOCGWINSZ, ioctl is available */
/* Defaults to 80x24                              */
/* ============================================== */
void
get_terminal_size(int * const r, int * const c)
{
  struct winsize ws;

  if (ioctl(0, TIOCGWINSZ, &ws) == 0)
  {
    *r = ws.ws_row;
    *c = ws.ws_col;
  }
  else
  {
    *r = tigetnum("lines");
    *c = tigetnum("cols");

    if (*r < 0 || *c < 0)
    {
      *r = 80;
      *c = 24;
    }
  }
}

/* ====================================================== */
/* Get cursor position the terminal                       */
/* Assume that the Escape sequence ESC [ 6 n is available */
/* ====================================================== */
int
get_cursor_position(int * const r, int * const c)
{
  char   buf[32];
  size_t i = 0;

  /* Report cursor location */
  /* """""""""""""""""""""" */
  if (write(1, "\x1b[6n", 4) != 4)
    return -1;

  /* Read the response: ESC [ rows ; cols R */
  /* """""""""""""""""""""""""""""""""""""" */
  while (i < sizeof(buf) - 1)
  {
    if (read(0, buf + i, 1) != 1)
      break;

    if (buf[i] == 'R')
      break;

    i++;
  }
  buf[i] = '\0';

  /* Parse it. */
  /* """"""""" */
  if (buf[0] != 0x1b || buf[1] != '[')
    return -1;

  if (sscanf(buf + 2, "%d;%d", r, c) != 2)
    return -1;

  return 1;
}

/* ***************************** */
/* Strings and utility functions */
/* ***************************** */

/* ======================= */
/* Trim leading characters */
/* ======================= */
void
ltrim(char * str, const char * trim_str)
{
  size_t len   = strlen(str);
  size_t begin = strspn(str, trim_str);
  size_t i;

  if (begin > 0)
    for (i = begin; i <= len; ++i)
      str[i - begin] = str[i];
}

/* ================================================= */
/* Trim trailing characters                          */
/* The resulting string will have at least min bytes */
/* even if trailing spaces remain.                   */
/* ================================================= */
void
rtrim(char * str, const char * trim_str, size_t min)
{
  size_t len = strlen(str);
  while (len > min && strchr(trim_str, str[len - 1]))
    str[--len] = '\0';
}

/* ===================================================== */
/* Returns 1 if a string is empty or only made of spaces */
/* ===================================================== */
int
isempty(const char * s)
{
  while (*s != '\0')
  {
    if (my_isprint(*s) && *s != ' ' && *s != '\t')
      return 0;
    s++;
  }
  return 1;
}

/* ========================================= */
/* Case insensitive strcmp                   */
/* from http://c.snippets.org/code/stricmp.c */
/* ========================================= */
int
my_strcasecmp(const char * str1, const char * str2)
{
#ifdef HAVE_STRCASECMP
  return strcasecmp(str1, str2);
#else
  int retval = 0;

  while (1)
  {
    retval = tolower(*str1++) - tolower(*str2++);

    if (retval)
      break;

    if (*str1 && *str2)
      continue;
    else
      break;
  }
  return retval;
#endif
}

/* =========================================== */
/* memmove based strcpy (tolerates overlaping) */
/* =========================================== */
char *
my_strcpy(char * str1, char * str2)
{
  if (str1 == NULL || str2 == NULL)
    return NULL;

  memmove(str1, str2, strlen(str2) + 1);

  return str1;
}

/* ======================================================================== */
/* Parse a regular expression based selector.                               */
/* The string to parse is bounded by a delimiter so we must parse something */
/* like: <delim><regex string><delim> as in /a.*b/ by example.              */
/*                                                                          */
/* str            (in)  delimited string to parse                           */
/* filter         (in)  INCLUDE_FILTER or EXCLUDE_FILTER                    */
/* inc_regex_list (out) INCLUDE_FILTER or EXCLUDE_FILTER                    */
/*                      regular expression if the filter is INCLUDE_FILTER  */
/* exc_regex_list (out) INCLUDE_FILTER or EXCLUDE_FILTER                    */
/*                      regular expression if the filter is EXCLUDE_FILTER  */
/* ======================================================================== */
void
parse_regex_selector_part(char * str, filters_t filter, ll_t ** inc_regex_list,
                          ll_t ** exc_regex_list)
{
  regex_t * regex;

  str[strlen(str) - 1] = '\0';

  regex = xmalloc(sizeof(regex_t));
  if (regcomp(regex, str + 1, REG_EXTENDED | REG_NOSUB) == 0)
  {
    if (filter == INCLUDE_FILTER)
    {
      if (*inc_regex_list == NULL)
        *inc_regex_list = ll_new();

      ll_append(*inc_regex_list, regex);
    }
    else
    {
      if (*exc_regex_list == NULL)
        *exc_regex_list = ll_new();

      ll_append(*exc_regex_list, regex);
    }
  }
}

/* ===================================================================== */
/* Parse a description string                                            */
/* <letter><range1>,<range2>,...                                         */
/* <range> is n1-n2 | n1 where n1 starts with 1                          */
/*                                                                       */
/* <letter> is a|A|s|S|r|R|u|U where                                     */
/* a|A is for Add                                                        */
/* s|S is for Select (same as Add)                                       */
/* r|R is for Remove                                                     */
/* d|D is for Deselect (same as Remove)                                  */
/*                                                                       */
/* str               (in)  string to parse                               */
/* filter            (out) is INCLUDE_FILTER or EXCLUDE_FILTER according */
/*                         to <letter>                                   */
/* unparsed          (out) is empty on success and contains the unparsed */
/*                         part on failure                               */
/* inc_interval_list (out) is a list of the interval of elements to      */
/*                         be included.                                  */
/* inc_regex_list    (out) is a list of extended regular expressions of  */
/*                         elements to be included.                      */
/* exc_interval_list (out) is a list of the interval of elements to be   */
/*                         excluded.                                     */
/* exc_regex_list    (out) is a list of extended interval of elements to */
/*                         be excluded.                                  */
/* ===================================================================== */
void
parse_selectors(char * str, filters_t * filter, char * unparsed,
                ll_t ** inc_interval_list, ll_t ** inc_regex_list,
                ll_t ** exc_interval_list, ll_t ** exc_regex_list,
                langinfo_t * langinfo)
{
  char         mark; /* Value to set */
  char         c;
  size_t       start = 1;     /* column string offset in the parsed string */
  size_t       first, second; /* range starting and ending values          */
  char *       ptr;           /* pointer to the remaining string to parse  */
  interval_t * interval;

  /* Replace the UTF-8 ascii representation in the selector by */
  /* their binary values.                                      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(str, langinfo);

  /* Get the first character to see if this is */
  /* an additive or restrictive operation.     */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (sscanf(str, "%c", &c) == 0)
    return;

  switch (c)
  {
    case 'i':
    case 'I':
      *filter = INCLUDE_FILTER;
      mark    = INCLUDE_MARK;
      break;

    case 'e':
    case 'E':
      *filter = EXCLUDE_FILTER;
      mark    = EXCLUDE_MARK;
      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      *filter = INCLUDE_FILTER;
      mark    = INCLUDE_MARK;
      start   = 0;
      break;

    default:
      if (!isgraph(c))
        return;

      *filter = INCLUDE_FILTER;
      mark    = INCLUDE_MARK;
      start   = 0;
      break;
  }

  /* Set ptr to the start of the interval list to parse */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  ptr = str + start;

  first = second = -1;

  /* Scan the comma separated ranges */
  /* """"""""""""""""""""""""""""""" */
  while (*ptr)
  {
    size_t swap;
    int    is_range = 0;
    char   delim1, delim2 = '\0';
    char * oldptr;

    oldptr = ptr;
    while (*ptr && *ptr != ',')
    {
      if (*ptr == '-')
        is_range = 1;
      ptr++;
    }

    /* Forbid the trailing comma (ex: xxx,) */
    /* """""""""""""""""""""""""""""""""""" */
    if (*ptr == ',' && (*(ptr + 1) == '\0' || *(ptr + 1) == '-'))
    {
      my_strcpy(unparsed, ptr);
      return;
    }

    /* Forbid the empty patterns (ex: xxx,,yyy) */
    /* """""""""""""""""""""""""""""""""""""""" */
    if (oldptr == ptr)
    {
      my_strcpy(unparsed, ptr);
      return;
    }

    /* Mark the end of the interval found */
    /* """""""""""""""""""""""""""""""""" */
    if (*ptr)
      *ptr++ = '\0';

    /* If the regex contains at least three characters then delim1 */
    /* and delim2 point to the first and last delimiter of the     */
    /* regular expression. Ex /abc/                                */
    /*                        ^   ^                                */
    /*                        |   |                                */
    /*                   delim1   delim2                           */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    delim1 = *(str + start);
    if (*ptr == '\0')
      delim2 = *(ptr - 1);
    else if (ptr > str + start + 2)
      delim2 = *(ptr - 2);

    /* Check is we have found a well describes regular expression */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (ptr > str + start + 2 && delim1 == delim2 && isgraph(delim1)
        && isgraph(delim2) && !isdigit(delim1) && !isdigit(delim2))
    {
      /* Process the regex */
      /* """"""""""""""""" */
      parse_regex_selector_part(str + start, *filter, inc_regex_list,
                                exc_regex_list);

      /* Adjust the start of the new interval to read in the initial string */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      start = ptr - str;

      continue;
    }
    else if (delim2 != '\0' && (!isdigit(delim1) || !isdigit(delim2)))
    {
      /* Both delimiter must be numeric if delim2 exist */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      my_strcpy(unparsed, str + start);
      return;
    }

    if (is_range)
    {
      int rc;
      int pos;

      rc = sscanf(str + start, "%zu-%zu%n", &first, &second, &pos);
      if (rc != 2 || *(str + start + pos) != '\0')
      {
        my_strcpy(unparsed, str + start);
        return;
      }

      if (first < 1 || second < 1)
      {
        /* Both interval boundaries must be strictly positive */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        my_strcpy(unparsed, str + start);
        return;
      }

      /* Ensure that the low bound of the interval is lower or equal */
      /* to the high one. Swap them if needed.                       */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      interval = interval_new();

      if (first > second)
      {
        swap   = first;
        first  = second;
        second = swap;
      }

      interval->low  = first - 1;
      interval->high = second - 1;
    }
    else
    {
      /* Read the number given */
      /* """"""""""""""""""""" */
      if (sscanf(str + start, "%zu", &first) != 1)
      {
        my_strcpy(unparsed, str + start);
        return;
      }

      interval      = interval_new();
      interval->low = interval->high = first - 1;
    }

    /* Adjust the start of the new interval to read in the initial string */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    start = ptr - str;

    /* Add the new interval to the correct list */
    /* """""""""""""""""""""""""""""""""""""""" */
    if (mark == EXCLUDE_MARK)
    {
      if (*exc_interval_list == NULL)
        *exc_interval_list = ll_new();

      ll_append(*exc_interval_list, interval);
    }
    else
    {
      if (*inc_interval_list == NULL)
        *inc_interval_list = ll_new();

      ll_append(*inc_interval_list, interval);
    }
  }

  /* If we are here, then all the intervals have be successfully parsed */
  /* Ensure that the unparsed string is empty.                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  *unparsed = '\0';
}

/* ===================================================================== */
/* Merge the intervals from an interval list in order to get the minimum */
/* number of intervals to consider.                                      */
/* ===================================================================== */
void
merge_intervals(ll_t * list)
{
  ll_node_t * node1, *node2;
  interval_t *data1, *data2;

  if (!list || list->len < 2)
    return;
  else
  {
    /* Step 1: sort the intervals list */
    /* """"""""""""""""""""""""""""""" */
    ll_sort(list, interval_comp, interval_swap);

    /* Step 2: merge the list by merging the consecutive intervals */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    node1 = list->head;
    node2 = node1->next;

    while (node2)
    {
      data1 = (interval_t *)(node1->data);
      data2 = (interval_t *)(node2->data);

      if (data1->high >= data2->low - 1)
      {
        /* Interval 1 overlaps interval 2 */
        /* '''''''''''''''''''''''''''''' */
        if (data2->high >= data1->high)
          data1->high = data2->high;
        ll_delete(list, node2);
        free(data2);
        node2 = node1->next;
      }
      else
      {
        /* No overlap */
        /* '''''''''' */
        node1 = node2;
        node2 = node2->next;
      }
    }
  }
}

/* ======================================================== */
/* Parse the sed like string passed as argument to -S/-I/-E */
/* ======================================================== */
int
parse_sed_like_string(sed_t * sed)
{
  char   sep;
  char * first_sep_pos;
  char * last_sep_pos;
  char * buf;
  long   index;
  int    icase;
  char   c;

  if (strlen(sed->pattern) < 4)
    return 0;

  /* Get the separator (the 1st character) */
  /* """"""""""""""""""""""""""""""""""""" */
  buf = xstrdup(sed->pattern);
  sep = buf[0];

  /* Space like separators are not permitted */
  /* """"""""""""""""""""""""""""""""""""""" */
  if (isspace(sep))
    goto err;

  /* Get the extended regular expression */
  /* """"""""""""""""""""""""""""""""""" */
  if ((first_sep_pos = strchr(buf + 1, sep)) == NULL)
    goto err;

  *first_sep_pos = '\0';

  /* Get the substitution string */
  /* """"""""""""""""""""""""""" */
  if ((last_sep_pos = strchr(first_sep_pos + 1, sep)) == NULL)
    goto err;

  *last_sep_pos = '\0';

  sed->substitution = xstrdup(first_sep_pos + 1);

  /* Get the global indicator (trailing g) */
  /* and the visual indicator (trailing v) */
  /* and the stop indicator (trailing s)   */
  /* """"""""""""""""""""""""""""""""""""" */
  sed->global = sed->visual = icase = 0;

  index = 1;
  while ((c = *(last_sep_pos + index)) != '\0')
  {
    if (c == 'g')
      sed->global = 1;
    else if (c == 'v')
      sed->visual = 1;
    else if (c == 's')
      sed->stop = 1;
    else if (c == 'i')
      icase = 1;
    else
      goto err;

    index++;
  }

  /* Empty regular expression ? */
  /* """""""""""""""""""""""""" */
  if (*(buf + 1) == '\0')
    goto err;

  /* Compile the regular expression and abort on failure */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (regcomp(&(sed->re), buf + 1,
              !icase ? REG_EXTENDED : (REG_EXTENDED | REG_ICASE))
      != 0)
    goto err;

  free(buf);

  return 1;

err:
  free(buf);

  return 0;
}

/* ===================================================================== */
/* Utility function used by replace to expand the replacement string     */
/* IN:                                                                   */
/* to_match:        matching part of the original string to be replaced  */
/* repl:            string containing the replacement directives         */
/* subs_a:          array of ranges containing the start and end offset  */
/*                  of the remembered parts of the strings referenced by */
/*                  the sequence \n where n is in [1,10]                 */
/* match_start/end: offset in to_match for the current matched string    */
/* subs_nb:         number of elements containing significant values in  */
/*                  the array described above                            */
/* match:           current match number in the original string          */
/*                                                                       */
/* OUT:                                                                  */
/* The modified string according to the content of repl                  */
/* ===================================================================== */
char *
build_repl_string(char * to_match, char * repl, long match_start,
                  long match_end, struct range_s * subs_a, long subs_nb,
                  long match)
{
  size_t rsize     = 0;
  size_t allocated = 16;
  char * str       = xmalloc(allocated);
  int    special   = 0;
  long   offset    = match * subs_nb; /* offset of the 1st sub      *
                                       * corresponding to the match */

  if (*repl == '\0')
    str = xstrdup("");
  else
    while (*repl)
    {
      switch (*repl)
      {
        case '\\':
          if (special)
          {
            if (allocated == rsize)
              str = xrealloc(str, allocated += 16);
            str[rsize] = '\\';
            rsize++;
            str[rsize] = '\0';
            special    = 0;
          }
          else
            special = 1;
          break;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (special)
          {
            if ((*repl) - '0' <= subs_nb)
            {
              long index = (*repl) - '0' - 1 + offset;
              long delta = subs_a[index].end - subs_a[index].start;

              if (allocated <= rsize + delta)
                str = xrealloc(str, allocated += (delta + 16));

              memcpy(str + rsize, to_match + subs_a[index].start, delta);

              rsize += delta;
              str[rsize] = '\0';
            }

            special = 0;
          }
          else
          {
            if (allocated == rsize)
              str = xrealloc(str, allocated += 16);
            str[rsize] = *repl;
            rsize++;
            str[rsize] = '\0';
          }
          break;

        case '&':
          if (!special)
          {
            long delta = match_end - match_start;

            if (allocated <= rsize + delta)
              str = xrealloc(str, allocated += (delta + 16));

            memcpy(str + rsize, to_match + match_start, delta);

            rsize += delta;
            str[rsize] = '\0';

            break;
          }

          /* No break here, '&' must be treated as a normal */
          /* character when protected.                      */
          /* '''''''''''''''''''''''''''''''''''''''''''''' */

        default:
          special = 0;
          if (allocated == rsize)
            str = xrealloc(str, allocated += 16);
          str[rsize] = *repl;
          rsize++;
          str[rsize] = '\0';
      }
      repl++;
    }
  return str;
}

/* ====================================================================== */
/* Replace the part of a string matched by an extender regular expression */
/* by the substitution string                                             */
/* The regex used must have been previously compiled                      */
/*                                                                        */
/* to_match: original string                                              */
/* sed:      composite variable containing the regular expression, a      */
/*           substitution string and various other informations.          */
/* output:   destination buffer                                           */
/*                                                                        */
/* RC:                                                                    */
/* return 1 if the replacement has been successful else 0                 */
/*                                                                        */
/* NOTE:                                                                  */
/* uses the global variable word_buffer                                   */
/* ====================================================================== */
int
replace(char * to_match, sed_t * sed)
{
  size_t match_nb = 0;   /* number of matches in the original string */
  int    sub_nb   = 0;   /* number of remembered matches in the      *
                          * original sting                           */
  size_t target_len = 0; /* length of the resulting string           */
  size_t subs_max   = 0;

  if (*to_match == '\0')
    return 1;

  struct range_s * matches_a = xmalloc(strlen(to_match)
                                       * sizeof(struct range_s));

  struct range_s * subs_a = xmalloc(10 * sizeof(struct range_s));

  const char * p = to_match; /* points to the end of the previous match. */
  regmatch_t   m[10];        /* array containing the start/end offsets   *
                              * of the matches found.                    */

  while (1)
  {
    size_t i = 0;
    size_t match;       /* current match index                        */
    size_t index   = 0; /* current char index in the original string  */
    int    nomatch = 0; /* equals to 1 when there is no more matching */
    char * exp_repl;    /* expanded replacement string                */

    if (*p == '\0')
      nomatch = 1;
    else
      nomatch = regexec(&sed->re, p, 10, m, 0);

    if (nomatch)
    {
      if (match_nb > 0)
      {
        for (index = 0; index < matches_a[0].start; index++)
          word_buffer[target_len++] = to_match[index];

        for (match = 0; match < match_nb; match++)
        {
          size_t len;
          size_t end;

          exp_repl = build_repl_string(to_match, sed->substitution,
                                       matches_a[match].start,
                                       matches_a[match].end, subs_a, subs_max,
                                       match);

          len = strlen(exp_repl);

          my_strcpy(word_buffer + target_len, exp_repl);
          target_len += len;
          free(exp_repl);

          index += matches_a[match].end - matches_a[match].start;

          if (match < match_nb - 1 && sed->global)
            end = matches_a[match + 1].start;
          else
            end = strlen(to_match);

          while (index < end)
            word_buffer[target_len++] = to_match[index++];

          word_buffer[target_len] = '\0';

          if (!sed->global)
            break;
        }
      }
      else
      {
        my_strcpy(word_buffer, to_match);
        return 0;
      }

      return nomatch;
    }

    subs_max = 0;
    for (i = 0; i < 10; i++)
    {
      size_t start;
      size_t finish;

      if (m[i].rm_so == -1)
        break;

      start  = m[i].rm_so + (p - to_match);
      finish = m[i].rm_eo + (p - to_match);

      if (i == 0)
      {
        matches_a[match_nb].start = start;
        matches_a[match_nb].end   = finish;
        match_nb++;
        if (match_nb > utf8_strlen(to_match))
          goto fail;
      }
      else
      {
        subs_a[sub_nb].start = start;
        subs_a[sub_nb].end   = finish;
        sub_nb++;
        subs_max++;
      }
    }
    if (m[0].rm_eo > 0)
      p += m[0].rm_eo;
    else
      p++; /* Empty match */
  }

fail:
  return 0;
}

/* ============================================================ */
/* Remove all ANSI color codes from s and puts the result in d. */
/* Memory space for d must have been allocated before.          */
/* ============================================================ */
void
strip_ansi_color(char * s, toggle_t * toggle)
{
  char * p   = s;
  long   len = strlen(s);

  while (*s != '\0')
  {
    /* Remove a sequence of \x1b[...m from s */
    /* """"""""""""""""""""""""""""""""""""" */
    if ((*s == 0x1b) && (*(s + 1) == '['))
    {
      while ((*s != '\0') && (*s++ != 'm'))
      {
        /* Do nothing */
      }
    }
    /* Convert a single \x1b in '.' */
    /* """""""""""""""""""""""""""" */
    else if (*s == 0x1b)
    {
      if (toggle->blank_nonprintable && len > 1)
        *s++ = ' ';
      else
        *s++ = '.';
      p++;
    }
    /* No ESC char, we can move on */
    /* """"""""""""""""""""""""""" */
    else
      *p++ = *s++;
  }

  *p = '\0';
}

/* ========================================================================= */
/* Allocate memory and safely concatenate strings. Stolen from a public      */
/* domain implementation which can be found here:                            */
/* http://openwall.info/wiki/people/solar/software/public-domain-source-code */
/* ========================================================================= */
char *
concat(const char * s1, ...)
{
  va_list      args;
  const char * s;
  char *       p, *result;
  size_t       l, m, n;

  m = n = strlen(s1);
  va_start(args, s1);
  while ((s = va_arg(args, char *)))
  {
    l = strlen(s);
    if ((m += l) < l)
      break;
  }
  va_end(args);
  if (s || m >= INT_MAX)
    return NULL;

  result = (char *)xmalloc(m + 1);

  memcpy(p = result, s1, n);
  p += n;
  va_start(args, s1);
  while ((s = va_arg(args, char *)))
  {
    l = strlen(s);
    if ((n += l) < l || n > m)
      break;
    memcpy(p, s, l);
    p += l;
  }
  va_end(args);
  if (s || m != n || p != result + n)
  {
    free(result);
    return NULL;
  }

  *p = 0;
  return result;
}

/* =============================================== */
/* Is the string str2 a prefix of the string str1? */
/* =============================================== */
int
strprefix(char * str1, char * str2)
{
  while (*str1 != '\0' && *str1 == *str2)
  {
    str1++;
    str2++;
  }

  return *str2 == '\0';
}

/* ================================================================ */
/* Callback to add a word index in the sorted list of matched words */
/* ================================================================ */
int
set_matching_flag(void * elem)
{
  ll_t * list = (ll_t *)elem;

  ll_node_t * node = list->head;

  while (node)
  {
    size_t pos;

    pos = *(size_t *)(node->data);
    if (word_a[pos].is_selectable)
      word_a[pos].is_matching = 1;

    insert_sorted_index(&matching_words_a, &matching_words_a_size,
                        &matches_count, pos);

    node = node->next;
  }
  return 1;
}

/* ======================================================================= */
/* Callback function used by tst_traverse                                  */
/* Iterate the linked list attached to the string containing the index of  */
/* the words in the input flow. Each page number is then added in a sorted */
/* array avoiding duplications keeping the array sorted.                   */
/* Mark the identified words as a matching word.                           */
/* ======================================================================= */
int
tst_cb(void * elem)
{
  /* The data attached to the string in the tst is a linked list of position */
  /* of the string in the input flow, This list is naturally sorted          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ll_t * list = (ll_t *)elem;

  ll_node_t * node = list->head;

  while (node)
  {
    size_t pos;

    pos = *(long *)(node->data);

    word_a[pos].is_matching = 1;
    insert_sorted_index(&matching_words_a, &matching_words_a_size,
                        &matches_count, pos);

    node = node->next;
  }
  return 1;
}

/* ======================================================================= */
/* Callback function used by tst_traverse                                  */
/* Iterate the linked list attached to the string containing the index of  */
/* the words in the input flow. Each page number is then used to determine */
/* the lower page greater than the cursor position                         */
/* ----------------------------------------------------------------------- */
/* This is a special version of tst_cb which permit to find the first word */
/* ----------------------------------------------------------------------- */
/* Require new_current to be set to count - 1 at start                     */
/* Update new_current to the smallest greater position than current        */
/* ======================================================================= */
int
tst_cb_cli(void * elem)
{
  long n  = 0;
  int  rc = 0;

  /* The data attached to the string in the tst is a linked list of position */
  /* of the string in the input flow, This list is naturally sorted          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ll_t * list = (ll_t *)elem;

  ll_node_t * node = list->head;

  while (n++ < list->len)
  {
    long pos;

    pos = *(long *)(node->data);

    word_a[pos].is_matching = 1;
    insert_sorted_index(&matching_words_a, &matching_words_a_size,
                        &matches_count, pos);

    /* We already are at the last word, report the finding */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos == count - 1)
      return 1;

    /* Only consider the indexes above the current cursor position */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos >= current) /* Enable the search of the current word */
    {
      /* As the future new current index has been set to the highest possible */
      /* value, each new possible position can only improve the estimation    */
      /* we set rc to 1 to mark that                                          */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (pos < new_current)
      {
        new_current = pos;
        rc          = 1;
      }
    }

    node = node->next;
  }
  return rc;
}

/* *************** */
/* Input functions */
/* *************** */

/* =============================================== */
/* Non delay reading of a scancode.                */
/* Update a scancodes buffer and return its length */
/* in bytes.                                       */
/* =============================================== */
int
get_scancode(unsigned char * s, size_t max)
{
  int            c;
  size_t         i = 1;
  struct termios original_ts, nowait_ts;

  if ((c = my_fgetc(stdin)) == EOF)
    return 0;

  /* Initialize the string with the first byte */
  /* """"""""""""""""""""""""""""""""""""""""" */
  memset(s, '\0', max);
  s[0] = c;

  /* 0x1b (ESC) has been found, proceed to check if additional codes */
  /* are available                                                   */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (c == 0x1b || c > 0x80)
  {
    /* Save the terminal parameters and configure getchar() */
    /* to return immediately                                */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    tcgetattr(0, &original_ts);
    nowait_ts = original_ts;
    nowait_ts.c_lflag &= ~ISIG;
    nowait_ts.c_cc[VMIN]  = 0;
    nowait_ts.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &nowait_ts);

    /* Check if additional code is available after 0x1b */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if ((c = my_fgetc(stdin)) != EOF)
    {
      s[1] = c;

      i = 2;
      while (i < max && (c = my_fgetc(stdin)) != EOF)
        s[i++] = c;
    }
    else
    {
      /* There isn't a new code, this mean 0x1b came from ESC key */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    }

    /* Restore the save terminal parameters */
    /* """""""""""""""""""""""""""""""""""" */
    tcsetattr(0, TCSADRAIN, &original_ts);

    /* Ignore EOF when a scancode contains an escape sequence. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
    clearerr(stdin);
  }

  return i;
}

/* ===================================================================== */
/* Get bytes from stdin. If the first byte is the leading character of a */
/* UTF-8 glyph, the following ones are also read.                        */
/* The utf8_get_length function is used to get the number of bytes of    */
/* the character.                                                        */
/* ===================================================================== */
int
get_bytes(FILE * input, char * utf8_buffer, langinfo_t * langinfo)
{
  int byte;
  int last = 0;
  int n;

  /* Read the first byte */
  /* """"""""""""""""""" */
  byte = my_fgetc(input);

  if (byte == EOF)
    return EOF;

  utf8_buffer[last++] = byte;

  /* Check if we need to read more bytes to form a sequence */
  /* and put the number of bytes of the sequence in last.   */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo->utf8 && ((n = utf8_get_length(byte)) > 1))
  {
    while (last < n && (byte = my_fgetc(input)) != EOF && (byte & 0xc0) == 0x80)
      utf8_buffer[last++] = byte;

    if (byte == EOF)
      return EOF;
  }

  utf8_buffer[last] = '\0';

  /* Replace an invalid UTF-8 byte sequence by a single dot.  */
  /* In this case the original sequence is lost (unsupported  */
  /* encoding).                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo->utf8 && !utf8_validate(utf8_buffer, last))
  {
    byte = utf8_buffer[0] = '.';
    utf8_buffer[1]        = '\0';
  }

  return byte;
}

/* ==========================================================================*/
/* Expand the string str by replacing all its embedded special characters by */
/* their corresponding escape sequence                                       */
/* dest must be long enough to contain the expanded string                   */
/* ========================================================================= */
size_t
expand(char * src, char * dest, langinfo_t * langinfo, toggle_t * toggle)
{
  char   c;
  int    n;
  int    all_spaces = 1;
  char * ptr        = dest;
  size_t len        = 0;

  while ((c = *(src++)))
  {
    /* UTF-8 codepoints take more than on character */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = utf8_get_length(c)) > 1)
    {
      all_spaces = 0;

      if (langinfo->utf8)
        /* If the locale is UTF-8 aware, copy src into ptr. */
        /* """""""""""""""""""""""""""""""""""""""""""""""" */
        do
        {
          *(ptr++) = c;
          len++;
        } while (--n && (c = *(src++)));
      else
      {
        /* If not, ignore the bytes composing the UTF-8 */
        /* glyph and replace them with a single '.'.    */
        /* """""""""""""""""""""""""""""""""""""""""""" */
        do
        {
          /* Skip this byte. */
          /* ''''''''''''''' */
        } while (--n && ('\0' != *(src++)));

        *(ptr++) = '.';
        len++;
      }
    }
    else
      /* This is not an UTF-8 glyph */
      /* """""""""""""""""""""""""" */
      switch (c)
      {
        case '\a':
          *(ptr++) = '\\';
          *(ptr++) = 'a';
          len += 2;
          all_spaces = 0;
          break;
        case '\b':
          *(ptr++) = '\\';
          *(ptr++) = 'b';
          len += 2;
          all_spaces = 0;
          break;
        case '\t':
          *(ptr++) = '\\';
          *(ptr++) = 't';
          len += 2;
          all_spaces = 0;
          break;
        case '\n':
          *(ptr++) = '\\';
          *(ptr++) = 'n';
          len += 2;
          all_spaces = 0;
          break;
        case '\v':
          *(ptr++) = '\\';
          *(ptr++) = 'v';
          len += 2;
          all_spaces = 0;
          break;
        case '\f':
          *(ptr++) = '\\';
          *(ptr++) = 'f';
          len += 2;
          all_spaces = 0;
          break;
        case '\r':
          *(ptr++) = '\\';
          *(ptr++) = 'r';
          len += 2;
          all_spaces = 0;
          break;
        case '\\':
          *(ptr++) = '\\';
          *(ptr++) = '\\';
          len += 2;
          all_spaces = 0;
          break;
        default:
          if (my_isprint(c))
          {
            *(ptr++)   = c;
            all_spaces = 0;
          }
          else
          {
            if (toggle->blank_nonprintable)
              *(ptr++) = ' ';
            else
            {
              *(ptr++)   = '.';
              all_spaces = 0;
            }
          }
          len++;
      }
  }

  /* If the word contains only spaces, replace them */
  /* by underscores so that it can be seen          */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (all_spaces)
    memset(dest, ' ', len);

  *ptr = '\0'; /* Ensure that dest has a nul terminator */

  return len;
}

/* ===================================================================== */
/* get_word(input): return a char pointer to the next word (as a string) */
/* Accept: a FILE * for the input stream                                 */
/* Return: a char *                                                      */
/*    On Success: the return value will point to a nul-terminated string */
/*    On Failure: the return value will be set to NULL                   */
/* ===================================================================== */
char *
get_word(FILE * input, ll_t * word_delims_list, ll_t * record_delims_list,
         char * utf8_buffer, unsigned char * is_last, toggle_t * toggle,
         langinfo_t * langinfo, win_t * win, limits_t * limits)
{
  char * temp = NULL;
  int    byte;
  long   utf8_count = 0; /* count chars used in current allocation */
  long   wordsize;       /* size of current allocation in chars    */
  int    is_dquote;      /* double quote presence indicator        */
  int    is_squote;      /* single quote presence indicator        */
  int    is_special;     /* a character is special after a \       */

  /* Skip leading delimiters */
  /* """"""""""""""""""""""" */
  byte = get_bytes(input, utf8_buffer, langinfo);

  while (byte == EOF
         || ll_find(word_delims_list, utf8_buffer, delims_cmp) != NULL)
  {
    if (byte == EOF)
      return NULL;

    byte = get_bytes(input, utf8_buffer, langinfo);
  }

  /* Allocate initial word storage space */
  /* """"""""""""""""""""""""""""""""""" */
  temp = xmalloc(wordsize = CHARSCHUNK);

  /* Start stashing bytes. Stop when we meet a non delimiter or EOF */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_count = 0;
  is_dquote  = 0;
  is_squote  = 0;
  is_special = 0;

  while (byte != EOF)
  {
    size_t i = 0;

    if (utf8_count >= limits->word_length)
    {
      fprintf(stderr,
              "A word's length has reached the limit "
              "(%ld), exiting.\n",
              limits->word_length);

      exit(EXIT_FAILURE);
    }

    if (byte == '\\' && !is_special)
    {
      is_special = 1;
      goto next;
    }

    /* Parse special characters */
    /* """""""""""""""""""""""" */
    if (is_special)
      switch (byte)
      {
        case 'a':
          utf8_buffer[0] = byte = '\a';
          utf8_buffer[1]        = '\0';
          break;

        case 'b':
          utf8_buffer[0] = byte = '\b';
          utf8_buffer[1]        = '\0';
          break;

        case 't':
          utf8_buffer[0] = byte = '\t';
          utf8_buffer[1]        = '\0';
          break;

        case 'n':
          utf8_buffer[0] = byte = '\n';
          utf8_buffer[1]        = '\0';
          break;

        case 'v':
          utf8_buffer[0] = byte = '\v';
          utf8_buffer[1]        = '\0';
          break;

        case 'f':
          utf8_buffer[0] = byte = '\f';
          utf8_buffer[1]        = '\0';
          break;

        case 'r':
          utf8_buffer[0] = byte = '\r';
          utf8_buffer[1]        = '\0';
          break;

        case 'u':
          utf8_buffer[0] = '\\';
          utf8_buffer[1] = 'u';
          utf8_buffer[2] = '\0';
          break;

        case '\\':
          utf8_buffer[0] = byte = '\\';
          utf8_buffer[1]        = '\0';
          break;
      }
    else
    {
      /* Manage double quotes */
      /* """""""""""""""""""" */
      if (byte == '"' && !is_squote)
        is_dquote = !is_dquote;

      /* Manage single quotes */
      /* """""""""""""""""""" */
      if (byte == '\'' && !is_dquote)
        is_squote = !is_squote;
    }

    /* Only consider delimiters when outside quotations */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!is_dquote && !is_squote)
        && ll_find(word_delims_list, utf8_buffer, delims_cmp) != NULL)
      break;

    /* We no dot count the significant quotes */
    /* """""""""""""""""""""""""""""""""""""" */
    if (!is_special
        && ((byte == '"' && !is_squote) || (byte == '\'' && !is_dquote)))
    {
      is_special = 0;
      goto next;
    }

    /* Feed temp with the content of utf8_buffer */
    /* """"""""""""""""""""""""""""""""""""""""" */
    while (utf8_buffer[i] != '\0')
    {
      if (utf8_count >= wordsize - 1)
        temp = xrealloc(temp,
                        wordsize += (utf8_count / CHARSCHUNK + 1) * CHARSCHUNK);

      *(temp + utf8_count++) = utf8_buffer[i];
      i++;
    }

    is_special = 0;

  next:
    byte = get_bytes(input, utf8_buffer, langinfo);
  }

  /* Nul-terminate the word to make it a string */
  /* """""""""""""""""""""""""""""""""""""""""" */
  *(temp + utf8_count) = '\0';

  /* Replace the UTF-8 ASCII representations in the word just */
  /* read by their binary values.                             */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(temp, langinfo);

  /* Skip all field delimiters before a record delimiter */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ll_find(record_delims_list, utf8_buffer, delims_cmp) == NULL)
  {
    byte = get_bytes(input, utf8_buffer, langinfo);
    while (byte != EOF
           && ll_find(word_delims_list, utf8_buffer, delims_cmp) != NULL
           && ll_find(record_delims_list, utf8_buffer, delims_cmp) == NULL)
      byte = get_bytes(input, utf8_buffer, langinfo);

    if (langinfo->utf8 && utf8_get_length(utf8_buffer[0]) > 1)
    {
      size_t pos;

      pos = strlen(utf8_buffer);
      while (pos > 0)
        my_ungetc(utf8_buffer[--pos]);
    }
    else
      my_ungetc(byte);
  }

  /* Mark it as the last word of a record if its sequence matches a */
  /* record delimiter except in tab mode                            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (byte == EOF
      || ((win->col_mode || win->line_mode)
          && ll_find(record_delims_list, utf8_buffer, delims_cmp) != NULL))
    *is_last = 1;
  else
    *is_last = 0;

  /* Remove the ANSI color escape sequences from the word */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  strip_ansi_color(temp, toggle);

  return temp;
}

/* ========================================================= */
/* Set a foreground color according to terminal capabilities */
/* ========================================================= */
void
set_foreground_color(term_t * term, short color)
{
  if (term->color_method == 0 && term->has_setf)
    tputs(TPARM2(set_foreground, color), 1, outch);
  else if (term->color_method == 1 && term->has_setaf)
    tputs(TPARM2(set_a_foreground, color), 1, outch);
}

/* ========================================================= */
/* Set a background color according to terminal capabilities */
/* ========================================================= */
void
set_background_color(term_t * term, short color)
{
  if (term->color_method == 0 && term->has_setb)
    tputs(TPARM2(set_background, color), 1, outch);
  else if (term->color_method == 1 && term->has_setab)
    tputs(TPARM2(set_a_background, color), 1, outch);
}

/* ====================================================== */
/* Put a scrolling symbol at the first column of the line */
/* ====================================================== */
void
left_margin_putp(char * s, term_t * term, win_t * win)
{
  apply_attr(term, win->shift_attr);

  /* We won't print this symbol when not in column mode */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  if (*s != '\0')
    fputs(s, stdout);

  tputs(TPARM1(exit_attribute_mode), 1, outch);
}

/* ===================================================== */
/* Put a scrolling symbol at the last column of the line */
/* ===================================================== */
void
right_margin_putp(char * s1, char * s2, langinfo_t * langinfo, term_t * term,
                  win_t * win, long line, long offset)
{
  apply_attr(term, win->bar_attr);

  if (term->has_hpa)
    tputs(TPARM2(column_address, offset + win->max_width + 1), 1, outch);
  else if (term->has_cursor_address)
    tputs(TPARM3(cursor_address, term->curs_line + line - 2,
                 offset + win->max_width + 1),
          1, outch);
  else if (term->has_parm_right_cursor)
  {
    fputc('\r', stdout);
    tputs(TPARM2(parm_right_cursor, offset + win->max_width + 1), 1, outch);
  }
  else
  {
    long i;

    fputc('\r', stdout);
    for (i = 0; i < offset + win->max_width + 1; i++)
      tputs(TPARM1(cursor_right), 1, outch);
  }

  if (langinfo->utf8)
    fputs(s1, stdout);
  else
    fputs(s2, stdout);

  tputs(TPARM1(exit_attribute_mode), 1, outch);
}

/* ************** */
/* Core functions */
/* ************** */

/* ============================================================== */
/* Split the lines of the message given to -m to a linked list of */
/* lines.                                                         */
/* Also fill the maximum screen width and the maximum number      */
/* of bytes of the longest line                                   */
/* ============================================================== */
void
get_message_lines(char * message, ll_t * message_lines_list,
                  long * message_max_width, long * message_max_len)
{
  char *    str;
  char *    ptr;
  char *    cr_ptr;
  long      n;
  wchar_t * w = NULL;

  *message_max_width = 0;
  *message_max_len   = 0;
  ptr                = message;

  /* For each line terminated with a EOL character */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  while (*ptr != '\0' && (cr_ptr = strchr(ptr, '\n')) != NULL)
  {
    if (cr_ptr > ptr)
    {
      str               = xmalloc(cr_ptr - ptr + 1);
      str[cr_ptr - ptr] = '\0';
      memcpy(str, ptr, cr_ptr - ptr);
    }
    else
      str = xstrdup("");

    ll_append(message_lines_list, str);

    /* If needed, update the message maximum width */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    n = wcswidth((w = utf8_strtowcs(str)), utf8_strlen(str));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line            */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = (long)strlen(str)) > *message_max_len)
      *message_max_len = n;

    ptr = cr_ptr + 1;
  }

  /* For the last line */
  /* """"""""""""""""" */
  if (*ptr != '\0')
  {
    ll_append(message_lines_list, xstrdup(ptr));

    n = wcswidth((w = utf8_strtowcs(ptr)), utf8_strlen(ptr));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line            */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = (long)strlen(ptr)) > *message_max_len)
      *message_max_len = n;
  }
  else
    ll_append(message_lines_list, xstrdup(""));
}

/* =================================================================== */
/* Set the new start and the new end of the window structure according */
/* to the current cursor position.                                     */
/* =================================================================== */
void
set_win_start_end(win_t * win, long current, long last)
{
  long cur_line, end_line;

  cur_line = line_nb_of_word_a[current];
  if (cur_line == last)
    win->end = count - 1;
  else
  {
    /* In help mode we must not modify the windows start/end position as */
    /* It must be redrawn exactly as it was before.                      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (!help_mode)
    {
      if (cur_line + win->max_lines / 2 + 1 <= last)
        win->end = first_word_in_line_a[cur_line + win->max_lines / 2 + 1] - 1;
      else
        win->end = first_word_in_line_a[last];
    }
  }
  end_line = line_nb_of_word_a[win->end];

  if (end_line < win->max_lines)
    win->start = 0;
  else
    win->start = first_word_in_line_a[end_line - win->max_lines + 1];
}

/* ========================================================================= */
/* Set the metadata associated with a word, its starting and ending position */
/* the line in which it is put and so on.                                    */
/* Set win.start win.end and the starting and ending position of each word.  */
/* This function is only called initially, when resizing the terminal and    */
/* potentially when the search function is used.                             */
/* ========================================================================= */
long
build_metadata(term_t * term, long count, win_t * win)
{
  long i = 0;
  long word_len;
  long len  = 0;
  long last = 0;
  long word_width;
  long tab_count; /* Current number of words in the line, *
                   * used in tab_mode                     */
  wchar_t * w;

  line_nb_of_word_a[0]    = 0;
  first_word_in_line_a[0] = 0;

  /* In column mode we need to calculate win->max_width, first initialize */
  /* it to 0 and increment it later in the loop                           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win->col_mode)
    win->max_width = 0;

  tab_count = 0;
  while (i < count)
  {
    /* Determine the number of screen positions used by the word */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len   = mbstowcs(NULL, word_a[i].str, 0);
    word_width = wcswidth((w = utf8_strtowcs(word_a[i].str)), word_len);

    /* Manage the case where the word is larger than the terminal width */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_width >= term->ncolumns - 2)
    {
      /* Shorten the word until it fits */
      /* """""""""""""""""""""""""""""" */
      do
      {
        word_width = wcswidth(w, word_len--);
      } while (word_len > 0 && word_width >= term->ncolumns - 2);
    }
    free(w);

    /* Look if there is enough remaining place on the line when not in column */
    /* mode. Force a break if the 'is_last' flag is set in all modes or if we */
    /* hit the max number of allowed columns in tab mode                      */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!win->col_mode && !win->line_mode
         && (len + word_width + 1) >= term->ncolumns - 1)
        || ((win->col_mode || win->line_mode || win->tab_mode) && i > 0
            && word_a[i - 1].is_last)
        || (win->tab_mode && win->max_cols > 0 && tab_count >= win->max_cols))
    {

      /* We must build another line */
      /* """""""""""""""""""""""""" */
      line_nb_of_word_a[i]       = ++last;
      first_word_in_line_a[last] = i;

      word_a[i].start = 0;

      len       = word_width + 1; /* Resets the current line length     */
      tab_count = 1;              /* Resets the current number of words *
                                   * in the line                        */
      word_a[i].end = word_width - 1;
      word_a[i].mb  = word_len;
    }
    else
    {
      word_a[i].start      = len;
      word_a[i].end        = word_a[i].start + word_width - 1;
      word_a[i].mb         = word_len;
      line_nb_of_word_a[i] = last;

      len += word_width + 1; /* Increase line length */
      tab_count++;           /* We've seen another word in the line */
    }

    /* If not in column mode, we need to calculate win->(real_)max_width */
    /* as it hasn't been already done                                    */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (len > win->max_width)
    {
      if (len > term->ncolumns)
        win->max_width = term->ncolumns - 2;
      else
        win->max_width = len;
    }

    if (len > win->real_max_width)
      win->real_max_width = len;

    i++;
  }

  if (!win->center || win->max_width > term->ncolumns - 2)
    win->offset = 0;
  else
    win->offset = (term->ncolumns - 2 - win->max_width) / 2;

  /* We need to recalculate win->start and win->end here */
  /* because of a possible terminal resizing             */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  set_win_start_end(win, current, last);

  return last;
}

/* ==================================================================== */
/* Helper function used by disp_word to print a matching word under the */
/* cursor when not in search mode with the matching characters of the   */
/* word highlighted.                                                    */
/* ==================================================================== */
void
disp_cursor_word(long pos, win_t * win, term_t * term, int err)
{
  size_t i;
  int    att_set = 0;
  char * p       = word_a[pos].str + daccess.flength;
  char * np;

  /* Set the cursor attribute */
  /* """""""""""""""""""""""" */
  tputs(TPARM1(exit_attribute_mode), 1, outch);

  tputs(TPARM1(exit_attribute_mode), 1, outch);
  if (word_a[pos].is_tagged)
    apply_attr(term, win->cursor_on_tag_attr);
  else
    apply_attr(term, win->cursor_attr);

  for (i = 0; i < word_a[pos].mb - daccess.flength; i++)
  {
    if (BIT_ISSET(word_a[pos].bitmap, i))
    {
      if (!att_set)
      {
        att_set = 1;

        /* Set the buffer display attribute */
        /* """""""""""""""""""""""""""""""" */
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (err)
          apply_attr(term, win->match_err_text_attr);
        else
          apply_attr(term, win->match_text_attr);

        if (word_a[pos].is_tagged)
          apply_attr(term, win->cursor_on_tag_attr);
        else
          apply_attr(term, win->cursor_attr);
      }
    }
    else
    {
      if (att_set)
      {
        att_set = 0;

        /* Set the search cursor attribute */
        /* """"""""""""""""""""""""""""""" */
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (word_a[pos].is_tagged)
          apply_attr(term, win->cursor_on_tag_attr);
        else
          apply_attr(term, win->cursor_attr);
      }
    }
    np = utf8_next(p);
    if (np == NULL)
      fputs(p, stdout);
    else
      printf("%.*s", (int)(np - p), p);
    p = np;
  }
}

/* ==================================================================== */
/* Helper function used by disp_word to print a matching word NOT under */
/* the cursor withe the matching characters of the word highlighted.    */
/* ==================================================================== */
void
disp_matching_word(long pos, win_t * win, term_t * term, int is_cursor, int err)
{
  size_t i;
  int    att_set = 0;
  char * p       = word_a[pos].str + daccess.flength;
  char * np;

  /* Set the search cursor attribute */
  /* """"""""""""""""""""""""""""""" */
  tputs(TPARM1(exit_attribute_mode), 1, outch);

  if (is_cursor)
  {
    if (err)
      apply_attr(term, win->match_err_field_attr);
    else
      apply_attr(term, win->match_field_attr);
  }
  else
  {
    if (err)
      apply_attr(term, win->search_err_field_attr);
    else
      apply_attr(term, win->search_field_attr);
  }

  if (word_a[pos].is_tagged)
    apply_attr(term, win->tag_attr);

  for (i = 0; i < word_a[pos].mb - daccess.flength; i++)
  {
    if (BIT_ISSET(word_a[pos].bitmap, i))
    {
      if (!att_set)
      {
        att_set = 1;

        /* Set the buffer display attribute */
        /* """""""""""""""""""""""""""""""" */
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (is_cursor)
        {
          if (err)
            apply_attr(term, win->match_err_text_attr);
          else
            apply_attr(term, win->match_text_attr);
        }
        else
          apply_attr(term, win->search_text_attr);

        if (word_a[pos].is_tagged)
          apply_attr(term, win->tag_attr);
      }
    }
    else
    {
      if (att_set)
      {
        att_set = 0;

        /* Set the search cursor attribute */
        /* """"""""""""""""""""""""""""""" */
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (is_cursor)
        {
          if (err)
            apply_attr(term, win->match_err_field_attr);
          else
            apply_attr(term, win->match_field_attr);
        }
        else
        {
          if (err)
            apply_attr(term, win->search_err_field_attr);
          else
            apply_attr(term, win->search_field_attr);
        }

        if (word_a[pos].is_tagged)
          apply_attr(term, win->tag_attr);
      }
    }

    np = utf8_next(p);
    if (np == NULL)
      fputs(p, stdout);
    else
      printf("%.*s", (int)(np - p), p);
    p = np;
  }
}

/* ====================================================================== */
/* Display a word in, the windows. Manages the following different cases: */
/* - Search mode display                                                  */
/* - Cursor display                                                       */
/* - Normal display                                                       */
/* - Color or mono display                                                */
/* ====================================================================== */
void
disp_word(long pos, search_mode_t search_mode, search_data_t * search_data,
          term_t * term, win_t * win, char * tmp_word)
{
  long s = word_a[pos].start;
  long e = word_a[pos].end;
  long p;

  char * buffer = search_data->buf;

  if (pos == current)
  {
    if (search_mode != NONE)
    {
      utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);
      if (word_a[pos].is_numbered)
      {
        /* Set the direct access number attribute */
        /* """""""""""""""""""""""""""""""""""""" */
        apply_attr(term, win->daccess_attr);

        /* And print it */
        /* """""""""""" */
        fputs(daccess.left, stdout);
        printf("%.*s", daccess.length, tmp_word + 1);
        fputs(daccess.right, stdout);
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        fputc(' ', stdout);
      }
      else if (daccess.length > 0)
      {
        /* prints the leading spaces */
        /* """"""""""""""""""""""""" */
        tputs(TPARM1(exit_attribute_mode), 1, outch);
        printf("%.*s", daccess.flength, tmp_word);
      }

      /* Set the search cursor attribute */
      /* """"""""""""""""""""""""""""""" */
      if (search_data->fuzzy_err)
        apply_attr(term, win->search_err_field_attr);
      else
        apply_attr(term, win->search_field_attr);

      /* The tab attribute must complete the attributes already set */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_a[pos].is_tagged)
        apply_attr(term, win->tag_attr);

      /* Print the word part */
      /* """"""""""""""""""" */
      fputs(tmp_word + daccess.flength, stdout);

      if (buffer[0] != '\0')
      {
        long i = 0;

        /* Put the cursor at the beginning of the word */
        /* """"""""""""""""""""""""""""""""""""""""""" */
        for (i = 0; i < e - s + 1 - daccess.flength; i++)
          tputs(TPARM1(cursor_left), 1, outch);

        tputs(TPARM1(exit_attribute_mode), 1, outch);

        /* Set the search cursor attribute */
        /* """"""""""""""""""""""""""""""" */
        if (search_data->fuzzy_err)
          apply_attr(term, win->search_err_field_attr);
        else
          apply_attr(term, win->search_field_attr);

        disp_matching_word(pos, win, term, 0, search_data->fuzzy_err);
      }
    }
    else
    {
      if (daccess.length > 0)
      {
        /* If this word is not numbered, reset the display */
        /* attributes before printing the leading spaces.  */
        /* """"""""""""""""""""""""""""""""""""""""""""""" */
        if (!word_a[pos].is_numbered)
        {
          /* Print the non significant part of the word */
          /* """""""""""""""""""""""""""""""""""""""""" */
          tputs(TPARM1(exit_attribute_mode), 1, outch);
          printf("%.*s", daccess.flength - 1, word_a[pos].str);
          tputs(TPARM1(exit_attribute_mode), 1, outch);
          fputc(' ', stdout);
        }
        else
        {
          apply_attr(term, win->daccess_attr);

          /* Print the non significant part of the word */
          /* """""""""""""""""""""""""""""""""""""""""" */
          fputs(daccess.left, stdout);
          printf("%.*s", daccess.length, word_a[pos].str + 1);
          fputs(daccess.right, stdout);
          tputs(TPARM1(exit_attribute_mode), 1, outch);
          fputc(' ', stdout);
        }
      }

      /* If we are not in search mode, display a normal cursor */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);
      if (word_a[pos].is_matching)
        disp_cursor_word(pos, win, term, search_data->fuzzy_err);
      else
      {
        if (word_a[pos].is_tagged)
          apply_attr(term, win->cursor_on_tag_attr);
        else
          apply_attr(term, win->cursor_attr);

        fputs(tmp_word + daccess.flength, stdout);
      }
    }
    tputs(TPARM1(exit_attribute_mode), 1, outch);
  }
  else
  {
    /* Display a normal word without any attribute */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);

    /* If words are numbered, emphasis their numbers */
    /* """"""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[pos].is_numbered)
    {
      apply_attr(term, win->daccess_attr);

      fputs(daccess.left, stdout);
      printf("%.*s", daccess.length, tmp_word + 1);
      fputs(daccess.right, stdout);

      tputs(TPARM1(exit_attribute_mode), 1, outch);
      fputc(' ', stdout);
    }
    else if (daccess.length > 0)
    {
      long i;

      /* Insert leading spaces if the word is non numbered and */
      /* padding for all words is set                          */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      tputs(TPARM1(exit_attribute_mode), 1, outch);
      if (daccess.padding == 'a')
        for (i = 0; i < daccess.flength; i++)
          fputc(' ', stdout);
    }

    if (!word_a[pos].is_selectable)
      apply_attr(term, win->exclude_attr);
    else if (word_a[pos].special_level > 0)
    {
      long level = word_a[pos].special_level - 1;

      apply_attr(term, win->special_attr[level]);
    }
    else
      apply_attr(term, win->include_attr);

    if (word_a[pos].is_matching)
      disp_matching_word(pos, win, term, 1, search_data->fuzzy_err);
    else
    {
      if (word_a[pos].is_tagged)
        apply_attr(term, win->tag_attr);

      if ((daccess.length > 0 && daccess.padding == 'a')
          || word_a[pos].is_numbered)
        fputs(tmp_word + daccess.flength, stdout);
      else
        fputs(tmp_word, stdout);
    }

    tputs(TPARM1(exit_attribute_mode), 1, outch);
  }
}

/* ======================================= */
/* Display a message line above the window */
/* ======================================= */
void
disp_message(ll_t * message_lines_list, long message_max_width,
             long message_max_len, term_t * term, win_t * win)
{
  ll_node_t * node;
  char *      line;
  char *      buf;
  size_t      len;
  long        size;
  long        offset;
  wchar_t *   w;

  win->message_lines = 0;

  /* Disarm the periodic timer to prevent the interruptions to corrupt */
  /* screen by altering the timing of the decoding of the terminfo     */
  /* capabilities de                                                   */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = 0;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);

  /* Do nothing if there is no message to display */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (message_lines_list == NULL)
    return;

  node = message_lines_list->head;
  buf  = xmalloc(message_max_len + 1);

  if (term->has_bold)
    tputs(TPARM1(enter_bold_mode), 1, outch);

  /* Follow the message lines list and display each line */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  while (node != NULL)
  {
    line = node->data;
    len  = utf8_strlen(line);
    w    = utf8_strtowcs(line);

    size = wcswidth(w, len);
    while (len > 0 && size > term->ncolumns)
      size = wcswidth(w, --len);

    free(w);

    /* Compute the offset from the left screen border if -M option is set */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    offset = (term->ncolumns - message_max_width - 3) / 2;

    if (win->center && offset > 0)
    {
      long i;

      for (i = 0; i < offset; i++)
        putc(' ', stdout);
    }

    /* Only print the start of a line if the screen width if too small */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    utf8_strprefix(buf, line, len, &size);
    puts(buf);

    node = node->next;
    win->message_lines++;
  }

  tputs(TPARM1(exit_attribute_mode), 1, outch);

  free(buf);

  /* Re-arm the periodic timer */
  /* """"""""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = TICK;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = TICK;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);
}

/* ============================ */
/* Display the selection window */
/* ============================ */
long
disp_lines(win_t * win, toggle_t * toggle, long current, long count,
           search_mode_t search_mode, search_data_t * search_data,
           term_t * term, long last_line, char * tmp_word,
           langinfo_t * langinfo)
{
  long lines_disp;
  long i;
  char scroll_symbol[5];
  long len;
  long display_bar;

  /* Disarm the periodic timer to prevent the interruptions to corrupt */
  /* screen by altering the timing of the decoding of the terminfo     */
  /* capabilities de                                                   */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = 0;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);

  scroll_symbol[0] = ' ';
  scroll_symbol[1] = '\0';

  lines_disp = 1;

  tputs(TPARM1(save_cursor), 1, outch);

  i = win->start;

  /* Modify the max number of displayed lines if we do not have */
  /* enough place                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->max_lines > term->nlines - win->message_lines)
    win->max_lines = term->nlines - win->message_lines;

  if (last_line >= win->max_lines)
    display_bar = 1;
  else
    display_bar = 0;

  if (win->col_mode || win->line_mode)
    len = term->ncolumns - 3;
  else
    len = term->ncolumns - 2;

  /* If in column mode and the sum of the columns sizes + gutters is      */
  /* greater than the terminal width,  then prepend a space to be able to */
  /* display the left arrow indicating that the first displayed column    */
  /* is not the first one.                                                */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (len > 1
      && ((win->col_mode || win->line_mode)
          && win->real_max_width > term->ncolumns - 2))
  {
    if (win->first_column > 0)
    {
      if (langinfo->utf8)
        strcpy(scroll_symbol, shift_left_sym);
      else
        strcpy(scroll_symbol, "<");
    }
  }
  else
    scroll_symbol[0] = '\0';

  /* Center the display ? */
  /* """""""""""""""""""" */
  if (win->offset > 0)
  {
    long i;
    for (i = 0; i < win->offset; i++)
      fputc(' ', stdout);
  }

  left_margin_putp(scroll_symbol, term, win);
  while (len > 1 && i <= count - 1)
  {
    /* Display one word and the space or symbol following it */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[i].start >= win->first_column
        && word_a[i].end < len + win->first_column)
    {
      disp_word(i, search_mode, search_data, term, win, tmp_word);

      /* If there are more element to be displayed after the right margin */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((win->col_mode || win->line_mode) && i < count - 1
          && word_a[i + 1].end >= len + win->first_column)
      {
        apply_attr(term, win->shift_attr);

        if (langinfo->utf8)
          fputs(shift_right_sym, stdout);
        else
          fputc('>', stdout);

        tputs(TPARM1(exit_attribute_mode), 1, outch);
      }

      /* If we want to display the gutter */
      /* """""""""""""""""""""""""""""""" */
      else if (!word_a[i].is_last && win->col_sep
               && (win->tab_mode || win->col_mode))
      {
        long pos;

        /* Make sure that we are using the right gutter character even */
        /* if the first displayed word is * not the first of its line  */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        pos = i - first_word_in_line_a[line_nb_of_word_a[i]];

        if (pos >= win->gutter_nb) /* Use the last gutter character */
          fputs(win->gutter_a[win->gutter_nb - 1], stdout);
        else
          fputs(win->gutter_a[pos], stdout);
      }
      else
        /* Else just display a space */
        /* """"""""""""""""""""""""" */
        fputc(' ', stdout);
    }

    /* Mark the line as the current line, the line containing the cursor */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (i == current)
      win->cur_line = lines_disp;

    /* Check if we must start a new line */
    /* """"""""""""""""""""""""""""""""" */
    if (i == count - 1 || word_a[i + 1].start == 0)
    {
      tputs(TPARM1(clr_eol), 1, outch);
      if (lines_disp < win->max_lines)
      {
        /* If we have more than one line to display */
        /* """""""""""""""""""""""""""""""""""""""" */
        if (display_bar && !toggle->no_scrollbar
            && (lines_disp > 1 || i < count - 1))
        {
          /* Display the next element of the scrollbar */
          /* """"""""""""""""""""""""""""""""""""""""" */
          if (line_nb_of_word_a[i] == 0)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_top, "\\", langinfo, term, win, lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_down, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
          else if (lines_disp == 1)
            right_margin_putp(sbar_arr_up, "^", langinfo, term, win, lines_disp,
                              win->offset);
          else if (line_nb_of_word_a[i] == last_line)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down, "/", langinfo, term, win, lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
          else if (last_line + 1 > win->max_lines
                   && (long)((float)(line_nb_of_word_a[current])
                               / (last_line + 1) * (win->max_lines - 2)
                             + 2)
                        == lines_disp)
            right_margin_putp(sbar_curs, "+", langinfo, term, win, lines_disp,
                              win->offset);
          else
            right_margin_putp(sbar_line, "|", langinfo, term, win, lines_disp,
                              win->offset);
        }

        /* Print a newline character if we are not at the end of */
        /* the input nor at the end of the window                */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (i < count - 1 && lines_disp < win->max_lines)
        {
          fputc('\n', stdout);

          if (win->offset > 0)
          {
            long i;
            for (i = 0; i < win->offset; i++)
              fputc(' ', stdout);
          }

          left_margin_putp(scroll_symbol, term, win);
        }

        /* We do not increment the number of lines seen after */
        /* a premature end of input                           */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        if (i < count - 1)
          lines_disp++;

        if (win->max_lines == 1)
          break;
      }
      else if (i <= count - 1 && lines_disp == win->max_lines)
      {
        /* The last line of the window has been displayed */
        /* """""""""""""""""""""""""""""""""""""""""""""" */
        if (display_bar && line_nb_of_word_a[i] == last_line)
        {
          if (!toggle->no_scrollbar)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down, "/", langinfo, term, win, lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
        }
        else
        {
          if (display_bar && !toggle->no_scrollbar)
            right_margin_putp(sbar_arr_down, "v", langinfo, term, win,
                              lines_disp, win->offset);
          break;
        }
      }
      else
        /* These lines were not in the widows and so we have nothing to do */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        break;
    }

    /* Next word */
    /* """"""""" */
    i++;
  }

  /* Update win->end, this is necessary because we only  */
  /* call build_metadata on start and on terminal resize */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (i == count)
    win->end = i - 1;
  else
    win->end = i;
  /* We restore the cursor position saved before the display of the window */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tputs(TPARM1(restore_cursor), 1, outch);

  /* Re-arm the periodic timer */
  /* """"""""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = TICK;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = TICK;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);

  return lines_disp;
}

/* ============================================ */
/* Signal handler. Manages SIGWINCH and SIGALRM */
/* ============================================ */
void
sig_handler(int s)
{
  switch (s)
  {
    /* Standard termination signals */
    /* """""""""""""""""""""""""""" */
    case SIGSEGV:
      fputs("SIGSEGV received!\n", stderr);
      tputs(TPARM1(carriage_return), 1, outch);
      tputs(TPARM1(cursor_visible), 1, outch);
      restore_term(fileno(stdin));

      exit(EXIT_FAILURE);

    case SIGTERM:
    case SIGHUP:
      fputs("Interrupted!\n", stderr);
      tputs(TPARM1(carriage_return), 1, outch);
      tputs(TPARM1(cursor_visible), 1, outch);
      restore_term(fileno(stdin));

      exit(EXIT_FAILURE);

    /* Terminal resize */
    /* """"""""""""""" */
    case SIGWINCH:
      got_winch = 1;
      break;

    /* Alarm triggered, This signal is used by the search mechanism to     */
    /* forces a window refresh.                                            */
    /* The help mechanism uses it to clear the message                     */
    /* It is also used to redisplay the window after the end of a terminal */
    /* resizing.                                                           */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    case SIGALRM:
      if (timeout.initial_value > 0)
        got_timeout_tick = 1;

      if (help_timer > 0)
        help_timer--;

      if (daccess_timer > 0)
        daccess_timer--;

      if (help_timer == 0 && help_mode)
        got_help_alrm = 1;

      if (daccess_timer == 0)
        got_daccess_alrm = 1;

      if (winch_timer > 0)
        winch_timer--;

      if (winch_timer == 0)
      {
        got_winch      = 0;
        got_help_alrm  = 0;
        got_winch_alrm = 1;
      }

      break;
  }
}

/* egetopt.c -- Extended 'getopt'.
 *
 * A while back, a public-domain version of getopt() was posted to the
 * net.  A bit later, a gentleman by the name of Keith Bostic made some
 * enhancements and reposted it.
 *
 * In recent weeks (i.e., early-to-mid 1988) there's been some
 * heated discussion in comp.lang.c about the merits and drawbacks
 * of getopt(), especially with regard to its handling of '?'.
 *
 * In light of this, I have taken Mr. Bostic's public-domain getopt()
 * and have made some changes that I hope will be considered to be
 * improvements.  I call this routine 'egetopt' ("Extended getopt").
 * The default behavior of this routine is the same as that of getopt(),
 * but it has some optional features that make it more useful.  These
 * options are controlled by the settings of some global variables.
 * By not setting any of these extra global variables, you will have
 * the same functionality as getopt(), which should satisfy those
 * purists who believe getopt() is perfect and can never be improved.
 * If, on the other hand, you are someone who isn't satisfied with the
 * status quo, egetopt() may very well give you the added capabilities
 * you want.
 *
 * Look at the enclosed README file for a description of egetopt()'s
 * new features.
 *
 * The code was originally posted to the net as getopt.c by ...
 *
 * Keith Bostic
 * ARPA: keith@seismo
 * UUCP: seismo!keith
 *
 * Current version: added enhancements and comments, reformatted code.
 *
 * Lloyd Zusman
 * Master Byte Software
 * Los Gatos, California
 * Internet: ljz@fx.com
 * UUCP:     ...!ames!fxgrp!ljz
 *
 * May, 1988
 */

/* None of these constants are referenced in the executable portion of */
/* the code ... their sole purpose is to initialize global variables.  */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define BADCH (int)'@'
#define NEEDSEP (int)':'
#define MAYBESEP (int)'%'
#define ERRFD 2
#define EMSG ""
#define START "-"

/* Here are all the pertinent global variables. */
/* """""""""""""""""""""""""""""""""""""""""""" */
int    opterr = 1;          /* if true, output error message */
int    optind = 1;          /* index into parent argv vector */
int    optopt;              /* character checked for validity */
int    optbad   = BADCH;    /* character returned on error */
int    optchar  = 0;        /* character that begins returned option */
int    optneed  = NEEDSEP;  /* flag for mandatory argument */
int    optmaybe = MAYBESEP; /* flag for optional argument */
int    opterrfd = ERRFD;    /* file descriptor for error text */
char * optarg   = NULL;     /* argument associated with option */
char * optstart = START;    /* list of characters that start options */

/* Macros. */
/* """"""" */

/* Conditionally print out an error message and return (depends on the */
/* setting of 'opterr' and 'opterrfd').  Note that this version of     */
/* TELL() doesn't require the existence of stdio.h.                    */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define TELL(S)                                      \
  {                                                  \
    if (opterr && opterrfd >= 0)                     \
    {                                                \
      char option = (char)optopt;                    \
      dummy_rc    = write(opterrfd, (S), strlen(S)); \
      dummy_rc    = write(opterrfd, &option, 1);     \
      dummy_rc    = write(opterrfd, "\n", 1);        \
    }                                                \
    short_usage();                                   \
    return (optbad);                                 \
  }

/* Here it is: */
/* """"""""""" */
int
egetopt(int nargc, char ** nargv, char * ostr)
{
  static char *   place = EMSG; /* option letter processing */
  register char * oli;          /* option letter list index */
  register char * osi = NULL;   /* option start list index  */

  if (nargv == (char **)NULL)
    return (EOF);

  if (nargc <= optind || nargv[optind] == NULL)
    return (EOF);

  if (place == NULL)
    place = EMSG;

  /* Update scanning pointer. */
  /* """""""""""""""""""""""" */
  if (*place == '\0')
  {
    place = nargv[optind];
    if (place == NULL)
      return (EOF);
    else if (*place == '\0')
      return (EOF);

    osi = strchr(optstart, *place);
    if (osi != NULL)
      optchar = (int)*osi;

    if (optind >= nargc || osi == NULL || *++place == '\0')
      return (EOF);

    /* Two adjacent, identical flag characters were found. */
    /* This takes care of "--", for example.               */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == place[-1])
    {
      ++optind;
      return (EOF);
    }
  }

  /* If the option is a separator or the option isn't in the list, */
  /* we've got an error.                                           */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  optopt = (int)*place++;
  oli    = strchr(ostr, optopt);
  if (optopt == optneed || optopt == (int)optmaybe || oli == NULL)
  {
    /* If we're at the end of the current argument, bump the */
    /* argument index.                                       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == '\0')
      ++optind;

    TELL("Illegal option -- "); /* bye bye */
  }

  /* If there is no argument indicator, then we don't even try to */
  /* return an argument.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ++oli;
  if (*oli == '\0' || (*oli != (char)optneed && *oli != (char)optmaybe))
  {
    /* If we're at the end of the current argument, bump the */
    /* argument index.                                       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == '\0')
      ++optind;

    optarg = NULL;
  }
  /* If we're here, there's an argument indicator.  It's handled */
  /* differently depending on whether it's a mandatory or an     */
  /* optional argument.                                          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  else
  {
    /* If there's no white space, use the rest of the    */
    /* string as the argument.  In this case, it doesn't */
    /* matter if the argument is mandatory or optional.  */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place != '\0')
      optarg = place;

    /* If we're here, there's whitespace after the option. */
    /*                                                     */
    /* Is it a mandatory argument?  If so, return the      */
    /* next command-line argument if there is one.         */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    else if (*oli == (char)optneed)
    {
      /* If we're at the end of the argument list, there */
      /* isn't an argument and hence we have an error.   */
      /* Otherwise, make 'optarg' point to the argument. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      if (nargc <= ++optind)
      {
        place = EMSG;
        TELL("Option requires an argument -- ");
      }
      else
        optarg = nargv[optind];
    }
    /* If we're here it must have been an optional argument. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    else
    {
      if (nargc <= ++optind)
      {
        place  = EMSG;
        optarg = NULL;
      }
      else
      {
        optarg = nargv[optind];
        if (optarg == NULL)
          place = EMSG;

        /* If the next item begins with a flag */
        /* character, we treat it like a new   */
        /* argument.  This is accomplished by  */
        /* decrementing 'optind' and returning */
        /* a null argument.                    */
        /* """"""""""""""""""""""""""""""""""" */
        else if (strchr(optstart, *optarg) != NULL)
        {
          --optind;
          optarg = NULL;
        }
      }
    }
    place = EMSG;
    ++optind;
  }

  /* Return option letter. */
  /* """"""""""""""""""""" */
  return (optopt);
}

/* =========================================================== */
/* Helper function to compare to delimiters for use by ll_find */
/* =========================================================== */
int
delims_cmp(const void * a, const void * b)
{
  return strcmp((const char *)a, (const char *)b);
}

/* ========================================================= */
/* Set new first column to display when horizontal scrolling */
/* Alter win->first_column                                   */
/* ========================================================= */
void
set_new_first_column(win_t * win, term_t * term)
{
  long pos;

  if (word_a[current].start < win->first_column)
  {
    pos = current;

    while (win->first_column > 0 && word_a[current].start < win->first_column)
    {
      win->first_column = word_a[pos].start;
      pos--;
    }
  }
  else if (word_a[current].end - win->first_column >= term->ncolumns - 3)
  {
    pos = first_word_in_line_a[line_nb_of_word_a[current]];

    while (!word_a[pos].is_last
           && word_a[current].end - win->first_column >= term->ncolumns - 3)
    {
      pos++;
      win->first_column = word_a[pos].start;
    }
  }
}

/* ==================================================== */
/* Restrict the matches to word ending with the pattern */
/* ==================================================== */
void
select_ending_matches(win_t * win, term_t * term, search_data_t * search_data,
                      long * last_line)
{
  if (matches_count > 0)
  {
    long   i;
    long   j = 0;
    long   index;
    long   nb;
    long * tmp;
    char * ptr;
    char * last_mb;
    int    utf8_len;

    /* Creation of an alternate array which will      */
    /* contain only the candidates having potentially */
    /* an ending pattern, if this array becomes non   */
    /* empty then it will replace the original array. */
    /* """""""""""""""""""""""""""""""""""""""""""""" */
    alt_matching_words_a = xrealloc(alt_matching_words_a,
                                    matches_count * (sizeof(long)));

    for (i = 0; i < matches_count; i++)
    {
      index      = matching_words_a[i];
      long   len = word_a[index].len;
      char * str = word_a[index].str;

      /* count the trailing blanks non counted in the bitmap */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      ptr = str + strlen(str);

      nb = 0;
      while ((ptr = utf8_prev(str, ptr)) != NULL && isblank(*ptr))
        if (ptr - str + 1 > len)
          nb++;
        else
          break;

      /* Check the bit corresponding to the last non blank glyph  */
      /* If set we add the index to an alternate array, if not we */
      /* clear the bitmap of the corresponding word               */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (BIT_ISSET(word_a[index].bitmap,
                    word_a[index].mb - nb - daccess.flength - 1))
        alt_matching_words_a[j++] = index;
      else
      {
        /* Look if the end of the word potentially contain an */
        /* ending pattern.                                    */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        if (search_mode == FUZZY)
        {
          utf8_len = mblen(ptr, 4);
          last_mb  = search_data->buf + search_data->len - utf8_len;

          /* in fuzzy search mode we only look the last glyph */
          /* """""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(ptr, last_mb, utf8_len) == 0)
            alt_matching_words_a[j++] = index;
          else
            memset(word_a[index].bitmap, '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
        else
        {
          /* in not fuzzy search mode use all the pattern */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          for (nb = 0; nb < search_data->utf8_len - 1; nb++)
            ptr = utf8_prev(str, ptr);
          if (memcmp(ptr, search_data->buf, search_data->len) == 0)
            alt_matching_words_a[j++] = index;
          else
            memset(word_a[index].bitmap, '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
      }
    }

    /* Swap the normal and alt array */
    /* """"""""""""""""""""""""""""" */
    matches_count         = j;
    matching_words_a_size = j;

    tmp                  = matching_words_a;
    matching_words_a     = alt_matching_words_a;
    alt_matching_words_a = tmp;

    if (j > 0)
    {
      /* Adjust the bitmap to the ending version */
      /* """"""""""""""""""""""""""""""""""""""" */
      update_bitmaps(search_mode, search_data, END_AFFINITY);

      current = matching_words_a[0];

      if (current < win->start || current > win->end)
        *last_line = build_metadata(term, count, win);

      /* Set new first column to display */
      /* """"""""""""""""""""""""""""""" */
      set_new_first_column(win, term);
    }
  }
}

/* ====================================================== */
/* Restrict the matches to word starting with the pattern */
/* ====================================================== */
void
select_starting_matches(win_t * win, term_t * term, search_data_t * search_data,
                        long * last_line)
{
  if (matches_count > 0)
  {
    long   i;
    long   j = 0;
    long   index;
    long * tmp;
    long   pos;
    char * first_mb;
    int    utf8_len;

    alt_matching_words_a = xrealloc(alt_matching_words_a,
                                    matches_count * (sizeof(long)));

    first_mb = xmalloc(5);

    for (i = 0; i < matches_count; i++)
    {
      index = matching_words_a[i];
      if (BIT_ISSET(word_a[index].bitmap, 0))
        alt_matching_words_a[j++] = index;
      else
      {
        if (search_mode == FUZZY)
        {
          first_mb = utf8_strprefix(first_mb,
                                    word_a[index].str + daccess.flength, 1,
                                    &pos);
          utf8_len = pos;

          /* in fuzzy search mode we only look the first glyph */
          /* """"""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(search_data->buf, first_mb, utf8_len) == 0)
            alt_matching_words_a[j++] = index;
          else
            memset(word_a[index].bitmap, '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
        else
        {
          /* in not fuzzy search mode use all the pattern */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(search_data->buf, word_a[index].str, search_data->len)
              == 0)
            alt_matching_words_a[j++] = index;
          else
            memset(word_a[index].bitmap, '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
      }
    }

    free(first_mb);

    matches_count         = j;
    matching_words_a_size = j;

    tmp                  = matching_words_a;
    matching_words_a     = alt_matching_words_a;
    alt_matching_words_a = tmp;

    if (j > 0)
    {
      /* Adjust the bitmap to the ending version */
      /* """"""""""""""""""""""""""""""""""""""" */
      update_bitmaps(search_mode, search_data, START_AFFINITY);

      current = matching_words_a[0];

      if (current < win->start || current > win->end)
        *last_line = build_metadata(term, count, win);

      /* Set new first column to display */
      /* """"""""""""""""""""""""""""""" */
      set_new_first_column(win, term);
    }
  }
}

/* ================ */
/* Main entry point */
/* ================ */
int
main(int argc, char * argv[])
{
  /* Mapping of supported charsets and the number of bits used in them */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  charsetinfo_t all_supported_charsets[] =
    { { "UTF-8", 8 },

      { "ANSI_X3.4-1968", 7 },
      { "ANSI_X3.4-1986", 7 },
      { "646", 7 },
      { "ASCII", 7 },
      { "CP367", 7 },
      { "IBM367", 7 },
      { "ISO_646.BASIC", 7 },
      { "ISO_646.IRV:1991", 7 },
      { "ISO_646.IRV", 7 },
      { "ISO646-US", 7 },
      { "ISO-IR-6", 7 },
      { "US", 7 },
      { "US-ASCII", 7 },

      { "hp-roman8", 8 },
      { "roman8", 8 },
      { "r8", 8 },

      { "ISO-8859-1", 8 },
      { "ISO-IR-100", 8 },
      { "ISO_8859-1:1987", 8 },
      { "ISO_8859-1", 8 },
      { "LATIN1", 8 },
      { "L1", 8 },
      { "IBM819", 8 },
      { "CP819", 8 },

      { "ISO-8859-2", 8 },
      { "ISO-IR-101", 8 },
      { "ISO_8859-2:1987", 8 },
      { "ISO_8859-2", 8 },
      { "LATIN2", 8 },
      { "L2", 8 },
      { "CP28592", 8 },

      { "ISO-8859-3", 8 },
      { "ISO-IR-109", 8 },
      { "ISO_8859-3:1988", 8 },
      { "ISO_8859-3", 8 },
      { "LATIN3", 8 },
      { "L3", 8 },
      { "CP28593", 8 },

      { "ISO-8859-4", 8 },
      { "ISO-IR-110", 8 },
      { "ISO_8859-4:1988", 8 },
      { "LATIN4", 8 },
      { "L4", 8 },
      { "CP28594", 8 },

      { "ISO-8859-5", 8 },
      { "ISO-IR-144", 8 },
      { "ISO_8859-5:1988", 8 },
      { "CYRILLIC", 8 },
      { "CP28595", 8 },

      { "KOI8-R", 8 },
      { "KOI8-RU", 8 },
      { "KOI8-U", 8 },

      { "ISO-8859-6", 8 },
      { "ISO-IR-127", 8 },
      { "ISO_8859-6:1987", 8 },
      { "ECMA-114", 8 },
      { "ASMO-708", 8 },
      { "ARABIC", 8 },
      { "CP28596", 8 },

      { "ISO-8859-7", 8 },
      { "ISO-IR-126", 8 },
      { "ISO_8859-7:2003", 8 },
      { "ISO_8859-7:1987", 8 },
      { "ELOT_928", 8 },
      { "ECMA-118", 8 },
      { "GREEK", 8 },
      { "GREEK8", 8 },
      { "CP28597", 8 },

      { "ISO-8859-8", 8 },
      { "ISO-IR-138", 8 },
      { "ISO_8859-8:1988", 8 },
      { "HEBREW", 8 },
      { "CP28598", 8 },

      { "ISO-8859-9", 8 },
      { "ISO-IR-148", 8 },
      { "ISO_8859-9:1989", 8 },
      { "LATIN5", 8 },
      { "L5", 8 },
      { "CP28599", 8 },

      { "ISO-8859-10", 8 },
      { "ISO-IR-157", 8 },
      { "ISO_8859-10:1992", 8 },
      { "LATIN6", 8 },
      { "L6", 8 },
      { "CP28600", 8 },

      { "ISO-8859-11", 8 },
      { "ISO-8859-11:2001", 8 },
      { "ISO-IR-166", 8 },
      { "CP474", 8 },

      { "TIS-620", 8 },
      { "TIS620", 8 },
      { "TIS620-0", 8 },
      { "TIS620.2529-1", 8 },
      { "TIS620.2533-0", 8 },

      /* ISO-8859-12 was abandoned in 1997 */
      /* """"""""""""""""""""""""""""""""" */

      { "ISO-8859-13", 8 },
      { "ISO-IR-179", 8 },
      { "LATIN7", 8 },
      { "L7", 8 },
      { "CP28603", 8 },

      { "ISO-8859-14", 8 },
      { "LATIN8", 8 },
      { "L8", 8 },

      { "ISO-8859-15", 8 },
      { "LATIN-9", 8 },
      { "CP28605", 8 },

      { "ISO-8859-16", 8 },
      { "ISO-IR-226", 8 },
      { "ISO_8859-16:2001", 8 },
      { "LATIN10", 8 },
      { "L10", 8 },

      { "CP1250", 8 },
      { "CP1251", 8 },

      { "CP1252", 8 },
      { "MS-ANSI", 8 },
      { NULL, 0 } };

  char * message = NULL; /* message to be displayed above the selection      *
                          * window                                           */
  ll_t * message_lines_list = NULL; /* list of the lines in the message to   *
                                     * be displayed                          */
  long message_max_width = 0; /* total width of the message (longest line)    */
  long message_max_len   = 0; /* max number of bytes taken by a message       *
                               * line */

  FILE * input_file; /* The name of the file passed as argument if any       */

  long index; /* generic counter */

  long daccess_index = 1; /* First index of the numbered words               */

  char *  daccess_np = NULL; /* direct access numbered pattern               */
  regex_t daccess_np_re; /* variable to store the compiled direct access     *
                          * pattern (-N) RE                                  */

  char *  daccess_up = NULL; /* direct access un-numbered pattern            */
  regex_t daccess_up_re; /* variable to store the compiled direct access     *
                          * pattern (-U) RE                                  */

  char * include_pattern     = NULL;
  char * exclude_pattern     = NULL;
  int    pattern_def_include = -1; /* Set to -1 until an -i or -e option is  *
                                    * specified, This variable remembers if  *
                                    * the words not matched will be included *
                                    * (value 1) or excluded (value 0) by     *
                                    * default.                               */
  regex_t include_re; /* variable to store the compiled include (-i) REs     */
  regex_t exclude_re; /* variable to store the compiled exclude (-e) REs     */

  ll_t * sed_list = NULL;         /* List of sed like string representation  *
                                   * of regex given after (-S)               */
  ll_t * include_sed_list = NULL; /* idem for -I */
  ll_t * exclude_sed_list = NULL; /* idem for -E */

  ll_t * inc_col_interval_list = NULL; /* list of included or                */
  ll_t * exc_col_interval_list = NULL; /* excluded numerical intervals       */
  ll_t * inc_row_interval_list = NULL; /* for lines and columns              */
  ll_t * exc_row_interval_list = NULL;

  ll_t * inc_col_regex_list = NULL; /* same for lines and columns specified  */
  ll_t * exc_col_regex_list = NULL; /* by regular expressions                */
  ll_t * inc_row_regex_list = NULL;
  ll_t * exc_row_regex_list = NULL;

  filters_t rows_filter_type = UNKNOWN_FILTER;

  char *  first_word_pattern = NULL; /* used by -A/-Z                        */
  char *  last_word_pattern  = NULL;
  regex_t first_word_re;
  regex_t last_word_re;

  char *  special_pattern[5] = { NULL, NULL, NULL, NULL, NULL }; /* -1 .. -5 */
  regex_t special_re[5];

  int include_visual_only = 0; /* If set to 1, the original word which is    *
                                * read from stdin will be output even if its */
  int exclude_visual_only = 0; /* visual representation was modified via     *
                                * -S/-I/-E                                   */

  ll_t * cols_selector_list = NULL;
  char * cols_selector      = NULL;

  ll_t * rows_selector_list = NULL;
  char * rows_selector      = NULL;

  long wi; /* word index */

  term_t term; /* Terminal structure                                         */

  tst_node_t * tst_word    = NULL; /* TST used by the search function        */
  tst_node_t * tst_daccess = NULL; /* TST used by the direct access system   */

  long   page;     /* Step for the vertical cursor moves                     */
  char * word;     /* Temporary variable to work on words                    */
  char * tmp_word; /* Temporary variable able to contain  the beginning of   *
                    * the word to be displayed                               */

  long     last_line = 0; /* last logical line number (from 0)               */
  long     opt;
  win_t    win;
  limits_t limits; /* set of various limitations                             */
  timers_t timers; /* timers contents                                        */
  misc_t   misc;   /* misc contents                                          */
  toggle_t toggle; /* set of binary indicators                               */

  int    old_fd1;    /* backups of the old stdout file descriptor            */
  FILE * old_stdout; /* The selected word will go there                      */

  long nl;     /* Number of lines displayed in the window                    */
  long offset; /* Used to correctly put the cursor at the start of the       *
                * selection window, even after a terminal vertical scroll    */

  long first_selectable; /* Index of the first selectable word in the input  *
                          * stream */
  long last_selectable;  /* Index of the last selectable word in the input   *
                          * stream  */

  long s, e;     /* word variable to contain the starting and ending         *
                  * terminal position of a word                              */
  long min_size; /* Minimum screen width of a column in tabular mode         */

  long tab_max_size;      /* Maximum screen width of a column in tabular     *
                           * mode                                            */
  long tab_real_max_size; /* Maximum size in bytes of a column in tabular    *
                           * mode */

  long * col_real_max_size = NULL; /* Array of maximum sizes (bytes) of each */
                                   /* column in column mode                  */
  long * col_max_size = NULL;      /* Array of maximum sizes of each column  */
                                   /* in column mode                         */

  long word_real_max_size = 0; /* size of the longer word after expansion    */
  long cols_real_max_size = 0; /* Max real width of all columns used when    *
                                * -w and -c are both set                     */
  long cols_max_size = 0;      /* Same as above for the columns widths       */

  long col_index = 0;   /* Index of the current column when reading words,i  *
                         * used  in column mode                              */
  long cols_number = 0; /* Number of columns in column mode                  */

  char * pre_selection_index = NULL;    /* pattern used to set the initial   *
                                         * cursor position                   */
  unsigned char * buffer = xmalloc(16); /* Input buffer                      */

  search_data_t search_data;
  search_data.buf = NULL;   /* Search buffer                                 */
  search_data.len = 0;      /* Current position in the search buffer         */
  search_data.utf8_len = 0; /* Current position in the search buffer in      *
                             * UTF-8 units                                   */
  search_data.fuzzy_err     = 0;  /* reset the error indicator               */
  search_data.fuzzy_err_pos = -1; /* no last error position in search buffer */

  long matching_word_cur_index = -1; /* cache for the next/previous moves *
                                      * in the matching words array */

  struct sigaction sa; /* Signal structure                                   */

  char * iws = NULL, *ils = NULL;
  ll_t * word_delims_list   = NULL;
  ll_t * record_delims_list = NULL;

  char utf8_buffer[5]; /* buffer to store the bytes of a UTF-8 glyph         *
                        * (4 chars max)                                      */
  unsigned char is_last;
  char *        charset;

  char * custom_ini_file = NULL; /* init file full path                      */
  char * home_ini_file;          /* init file full path                      */
  char * local_ini_file;         /* init file full path                      */

  charsetinfo_t * charset_ptr;
  langinfo_t      langinfo;
  int             is_supported_charset;

  long line_count = 0; /* Only used when -R is selected */

  attr_t init_attr;

  ll_node_t * inc_interval_node = NULL; /* one node of this list             */
  ll_node_t * exc_interval_node = NULL; /* one node of this list             */

  interval_t * inc_interval;       /* the data in each node                  */
  interval_t * exc_interval;       /* the data in each node                  */
  int          row_def_selectable; /* default selectable value               */

  int line_selected_by_regex = 0;
  int line_excluded          = 0;

  char * timeout_message;

  /* Win fields initialization */
  /* """"""""""""""""""""""""" */
  win.max_lines       = 5;
  win.message_lines   = 0;
  win.asked_max_lines = -1;
  win.center          = 0;
  win.max_cols        = 0;
  win.col_sep         = 0;
  win.wide            = 0;
  win.tab_mode        = 0;
  win.col_mode        = 0;
  win.line_mode       = 0;
  win.first_column    = 0;
  win.real_max_width  = 0;

  init_attr.is_set    = UNSET;
  init_attr.fg        = -1;
  init_attr.bg        = -1;
  init_attr.bold      = -1;
  init_attr.dim       = -1;
  init_attr.reverse   = -1;
  init_attr.standout  = -1;
  init_attr.underline = -1;
  init_attr.italic    = -1;

  win.cursor_attr           = init_attr;
  win.cursor_on_tag_attr    = init_attr;
  win.bar_attr              = init_attr;
  win.shift_attr            = init_attr;
  win.search_field_attr     = init_attr;
  win.search_text_attr      = init_attr;
  win.search_err_field_attr = init_attr;
  win.search_err_text_attr  = init_attr;
  win.match_field_attr      = init_attr;
  win.match_text_attr       = init_attr;
  win.match_err_field_attr  = init_attr;
  win.match_err_text_attr   = init_attr;
  win.include_attr          = init_attr;
  win.exclude_attr          = init_attr;
  win.tag_attr              = init_attr;
  win.daccess_attr          = init_attr;

  win.sel_sep = NULL;

  for (index = 0; index < 5; index++)
    win.special_attr[index] = init_attr;

  /* Default limits initialization */
  /* """"""""""""""""""""""""""""" */
  limits.words       = 32767;
  limits.cols        = 256;
  limits.word_length = 512;

  /* Default timers in 1/10 s */
  /* """""""""""""""""""""""" */
  timers.search        = 60 * FREQ / 10;
  timers.help          = 150 * FREQ / 10;
  timers.winch         = 7 * FREQ / 10;
  timers.direct_access = 6 * FREQ / 10;

  /* Toggles initialization */
  /* """""""""""""""""""""" */
  toggle.del_line            = 0;
  toggle.enter_val_in_search = 0;
  toggle.no_scrollbar        = 0;
  toggle.blank_nonprintable  = 0;
  toggle.keep_spaces         = 0;
  toggle.taggable            = 0;
  toggle.autotag             = 0;
  toggle.pinable             = 0;
  toggle.visual_bell         = 0;

  /* misc default values */
  /* """"""""""""""""""" */
  misc.default_search_method = NONE;

  /* direct access variable initialization */
  /* """"""""""""""""""""""""""""""""""""" */
  daccess_stack      = xcalloc(6, 1);
  daccess_stack_head = 0;

  /* fuzzy variables initialization */
  /* """""""""""""""""""""""""""""" */
  tst_search_list = ll_new();
  ll_append(tst_search_list, sub_tst_new());

  matching_words_a_size = 64;
  matching_words_a      = xmalloc(matching_words_a_size * sizeof(long));
  matches_count         = 0;

  best_matching_words_a_size = 16;
  best_matching_words_a = xmalloc(best_matching_words_a_size * sizeof(long));
  best_matches_count    = 0;

  /* Initialize the tag hit number which will permit to sort the */
  /* pinned words when displayed.                                */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  long next_tag_nb = 0;

  /* Columns selection variables */
  /* """"""""""""""""""""""""""" */
  char * cols_filter = NULL;

  /* Get the current locale */
  /* """""""""""""""""""""" */
  setlocale(LC_ALL, "");
  charset = nl_langinfo(CODESET);

  /* Check if the local charset is supported */
  /* """"""""""""""""""""""""""""""""""""""" */
  is_supported_charset = 0;
  charset_ptr          = all_supported_charsets;

  /* Count of the words currently tagged */
  /* """"""""""""""""""""""""""""""""""" */
  long tagged_words = 0;

  while (charset_ptr->name != NULL)
  {
    if (my_strcasecmp(charset, charset_ptr->name) == 0)
    {
      is_supported_charset = 1;
      langinfo.bits        = charset_ptr->bits;
      break;
    }
    charset_ptr++;
  }

  if (!is_supported_charset)
  {
    fprintf(stderr, "%s: %s\n", "Unsupported charset", charset);

    exit(EXIT_FAILURE);
  }

  if (strcmp(charset, "UTF-8") == 0)
    langinfo.utf8 = 1;
  else
    langinfo.utf8 = 0;

  /* my_isprint is a function pointer that points to the 7 or 8-bit */
  /* version of isprint according to the content of UTF-8           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo.utf8 || langinfo.bits == 8)
    my_isprint = isprint8;
  else
    my_isprint = isprint7;

  /* Set terminal in noncanonical, noecho mode and  */
  /* if TERM is unset or unknown, vt100 is assumed. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (getenv("TERM") == NULL)
    setupterm("vt100", 1, (int *)0);
  else
    setupterm((char *)0, 1, (int *)0);

  /* Get some terminal capabilities */
  /* """""""""""""""""""""""""""""" */

  if (getenv("NO_COLOR") != NULL)
    term.colors = 0;
  else
  {
    term.colors = tigetnum("colors");
    if (term.colors < 0)
      term.colors = 0;
  }

  /* Set the default timeout to 0: no expiration */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  timeout.initial_value = 0;
  timeout.remain        = 0;
  timeout.reached       = 0;

  daccess.mode       = DA_TYPE_NONE;
  daccess.left       = xstrdup(" ");
  daccess.right      = xstrdup(")");
  daccess.alignment  = 'r';
  daccess.padding    = 'a';
  daccess.head       = 'k'; /* keep by default */
  daccess.length     = -2;
  daccess.flength    = 0;
  daccess.offset     = 0;
  daccess.plus       = 0;
  daccess.size       = 0;
  daccess.ignore     = 0;
  daccess.follow     = 'y';
  daccess.num_sep    = NULL;
  daccess.def_number = -1;

  /* Command line options analysis */
  /* """"""""""""""""""""""""""""" */
  while ((opt = egetopt(argc, argv,
                        "Vf:h?X:x:qdMba:i:e:S:I:E:A:Z:1:2:3:4:5:C:R:"
                        "kvclwrg%n:t%m:s:W:L:T%P%pN%U%FD:/:"))
         != -1)
  {
    switch (opt)
    {
      case 'V':
        fputs("Version: " VERSION "\n", stdout);
        exit(0);

      case 'f':
        if (optarg && *optarg != '-')
          custom_ini_file = xstrdup(optarg);
        else
          TELL("Option requires an argument -- ");
        break;

      case 'n':
        if (optarg && *optarg != '-')
          win.asked_max_lines = abs(atoi(optarg));
        else
          TELL("Option requires an argument -- ");
        break;

      case 'd':
        toggle.del_line = 1;
        break;

      case 'M':
        win.center = 1;
        break;

      case 's':
        if (optarg && *optarg != '-')
        {
          pre_selection_index = xstrdup(optarg);
          utf8_interpret(pre_selection_index, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 't':
        if (optarg != NULL)
        {
          if (sscanf(optarg, "%ld", &(win.max_cols)) != 1)
            TELL("Argument must be numeric -- ");

          if (win.max_cols < 1)
            TELL("Argument must be at least 1 -- ");
        }

        win.tab_mode  = 1;
        win.col_mode  = 0;
        win.line_mode = 0;
        break;

      case 'k':
        toggle.keep_spaces = 1;
        break;

      case 'v':
        toggle.visual_bell = 1;
        break;

      case 'c':
        win.tab_mode  = 0;
        win.col_mode  = 1;
        win.line_mode = 0;
        win.max_cols  = 0;
        break;

      case 'l':
        win.line_mode = 1;
        win.tab_mode  = 0;
        win.col_mode  = 0;
        win.max_cols  = 0;
        break;

      case 'g':
        if (win.col_sep)
          break;

        if (optarg == NULL)
        {
          win.gutter_a = xmalloc(1 * sizeof(char *));

          if (langinfo.utf8)
            win.gutter_a[0] = xstrdup(vertical_bar);
          else
            win.gutter_a[0] = xstrdup("|");

          win.gutter_nb = 1;
        }
        else
        {
          long      n;
          wchar_t * w;
          long      i, offset;
          int       mblen;
          char *    gutter;

          gutter = xstrdup(optarg);

          utf8_interpret(gutter, &langinfo); /* Guarantees a well formed *
                                              * UTF-8 string             */

          win.gutter_nb = utf8_strlen(gutter);
          win.gutter_a  = xmalloc(win.gutter_nb * sizeof(char *));

          offset = 0;

          for (i = 0; i < win.gutter_nb; i++)
          {
            mblen           = utf8_get_length(*(gutter + offset));
            win.gutter_a[i] = xcalloc(1, mblen + 1);
            memcpy(win.gutter_a[i], gutter + offset, mblen);

            n = wcswidth((w = utf8_strtowcs(win.gutter_a[i])), 1);
            free(w);

            if (n > 1)
            {
              long j;

              for (j = 0; j <= i; j++)
                free(win.gutter_a[j]);

              free(win.gutter_a);
              free(gutter);

              TELL("A multi columns gutter is not allowed -- ");
            }

            offset += mblen;
          }

          free(gutter);
        }

        win.col_sep = 1;
        break;

      case 'w':
        win.wide = 1;
        break;

      case 'm':
        if (optarg && *optarg != '-')
        {
          message = xstrdup(optarg);
          if (!langinfo.utf8)
            utf8_sanitize(message);
          utf8_interpret(message, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'r':
        toggle.enter_val_in_search = 1;
        break;

      case 'b':
        toggle.blank_nonprintable = 1;
        break;

      case 'i':
        if (optarg && *optarg != '-')
        {
          /* Set the default behaviour if not already set */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          if (pattern_def_include == -1)
            pattern_def_include = 0;

          if (include_pattern == NULL)
            include_pattern = concat("(", optarg, ")", NULL);
          else
            include_pattern = concat(include_pattern, "|(", optarg, ")", NULL);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'e':
        if (optarg && *optarg != '-')
        {
          /* Set the default behaviour if not already set */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          if (pattern_def_include == -1)
            pattern_def_include = 1;

          if (exclude_pattern == NULL)
            exclude_pattern = concat("(", optarg, ")", NULL);
          else
            exclude_pattern = concat(exclude_pattern, "|(", optarg, ")", NULL);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'C':
        if (optarg && *optarg != '-')
        {
          if (cols_selector_list == NULL)
            cols_selector_list = ll_new();

          ll_append(cols_selector_list, xstrdup(optarg));
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'R':
        if (optarg && *optarg != '-')
        {
          if (rows_selector_list == NULL)
            rows_selector_list = ll_new();

          ll_append(rows_selector_list, xstrdup(optarg));
          win.max_cols = 0;
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'S':
        if (optarg && *optarg != '-')
        {
          sed_t * sed_node;

          if (sed_list == NULL)
            sed_list = ll_new();

          sed_node          = xmalloc(sizeof(sed_t));
          sed_node->pattern = xstrdup(optarg);
          utf8_interpret(sed_node->pattern, &langinfo);
          sed_node->stop = 0;
          ll_append(sed_list, sed_node);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'I':
        if (optarg && *optarg != '-')
        {
          sed_t * sed_node;

          if (include_sed_list == NULL)
            include_sed_list = ll_new();

          sed_node          = xmalloc(sizeof(sed_t));
          sed_node->pattern = xstrdup(optarg);
          utf8_interpret(sed_node->pattern, &langinfo);
          sed_node->stop = 0;
          ll_append(include_sed_list, sed_node);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'E':
        if (optarg && *optarg != '-')
        {
          sed_t * sed_node;

          if (exclude_sed_list == NULL)
            exclude_sed_list = ll_new();

          sed_node          = xmalloc(sizeof(sed_t));
          sed_node->pattern = xstrdup(optarg);
          utf8_interpret(sed_node->pattern, &langinfo);
          sed_node->stop = 0;
          ll_append(exclude_sed_list, sed_node);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
        if (optarg && *optarg != '-')
        {
          long   count = 1;
          attr_t attr  = init_attr;

          special_pattern[opt - '1'] = xstrdup(optarg);
          utf8_interpret(special_pattern[opt - '1'], &langinfo);

          /* Parse optional additional arguments */
          /* """"""""""""""""""""""""""""""""""" */
          while (argv[optind] && *argv[optind] != '-')
          {
            if (count > 2)
              TELL("Too many arguments -- ");

            /* Colors must respect the format: <fg color>/<bg color> */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (parse_attr(argv[optind], &attr, term.colors))
            {
              win.special_attr[opt - '1'].is_set    = FORCED;
              win.special_attr[opt - '1'].fg        = attr.fg;
              win.special_attr[opt - '1'].bg        = attr.bg;
              win.special_attr[opt - '1'].bold      = attr.bold;
              win.special_attr[opt - '1'].dim       = attr.dim;
              win.special_attr[opt - '1'].reverse   = attr.reverse;
              win.special_attr[opt - '1'].standout  = attr.standout;
              win.special_attr[opt - '1'].underline = attr.underline;
              win.special_attr[opt - '1'].italic    = attr.italic;
            }
            else
              TELL("Bad optional color settings -- ");

            optind++;
            count++;
          }
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'a':
        if (optarg && *optarg != '-')
        {
          long i;          /* loop index                                */
          long offset = 0; /* nb of chars to ship to find the attribute *
                            * representation (prefix size)              */

          attr_t   attr;
          attr_t * attr_to_set = NULL;

          /* Flags to check if an attribute is already set */
          /* """"""""""""""""""""""""""""""""""""""""""""" */
          int inc_attr_set           = 0; /* included words                  */
          int exc_attr_set           = 0; /* excluded words                  */
          int cur_attr_set           = 0; /* highlighted word (cursor)       */
          int bar_attr_set           = 0; /* scroll bar                      */
          int shift_attr_set         = 0; /* hor. scrolling arrows           */
          int tag_attr_set           = 0; /* selected (tagged) words         */
          int cursor_on_tag_attr_set = 0; /* selected words under the cursor */
          int sf_attr_set            = 0; /* currently searched field color  */
          int st_attr_set            = 0; /* currently searched text color   */
          int mf_attr_set            = 0; /* matching word field color       */
          int mt_attr_set            = 0; /* matching word text color        */
          int daccess_attr_set       = 0; /* Direct access text color        */

          /* Information relatives to the attributes to be searched and
           * set */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
           */
          struct
          {
            attr_t * attr;
            char *   msg;
            int *    flag;
            char *   prefix;
            int      prefix_len;
          } attr_infos[] = {
            { &win.exclude_attr, "The exclude attribute is already set -- ",
              &exc_attr_set, "e:", 2 },
            { &win.include_attr, "The include attribute is already set -- ",
              &inc_attr_set, "i:", 2 },
            { &win.cursor_attr, "The cursor attribute is already set -- ",
              &cur_attr_set, "c:", 2 },
            { &win.bar_attr, "The scroll bar attribute is already set -- ",
              &bar_attr_set, "b:", 2 },
            { &win.shift_attr, "The shift attribute is already set -- ",
              &shift_attr_set, "s:", 2 },
            { &win.tag_attr, "The tag attribute is already set -- ",
              &tag_attr_set, "t:", 2 },
            { &win.cursor_on_tag_attr,
              "The cursor on tagged word attribute is already set -- ",
              &cursor_on_tag_attr_set, "ct:", 3 },
            { &win.search_field_attr,
              "The search field attribute is already set -- ", &sf_attr_set,
              "sf:", 3 },
            { &win.search_text_attr,
              "The search text attribute is already set -- ", &st_attr_set,
              "st:", 3 },
            { &win.search_err_field_attr,
              "The search with error field attribute is already set -- ",
              &sf_attr_set, "sfe:", 4 },
            { &win.search_err_text_attr,
              "The search text with error attribute is already set -- ",
              &st_attr_set, "ste:", 4 },
            { &win.match_field_attr,
              "The matching word field attribute is already set -- ",
              &mf_attr_set, "mf:", 3 },
            { &win.match_text_attr,
              "The matching word text attribute is already set -- ",
              &mt_attr_set, "mt:", 3 },
            { &win.match_err_field_attr,
              "The matching word with error field attribute is already set -- ",
              &mf_attr_set, "mfe:", 4 },
            { &win.match_err_text_attr,
              "The matching word with error text attribute is already set -- ",
              &mt_attr_set, "mte:", 4 },
            { &win.daccess_attr,
              "The direct access tag attribute is already set -- ",
              &daccess_attr_set, "da:", 3 },
            { NULL, NULL, NULL, NULL, 0 }
          };

          optind--;

          if (*argv[optind] == '-')
            TELL("A blank is required before the first sub-option -- ");

          /* Parse the arguments arguments */
          /* """"""""""""""""""""""""""""" */
          while (argv[optind] && *argv[optind] != '-')
          {
            attr = init_attr;

            if (strlen(argv[optind]) < 3)
              TELL("Empty attribute value -- ");

            i = 0;
            while (attr_infos[i].flag != NULL)
            {
              if (strncmp(argv[optind], attr_infos[i].prefix,
                          attr_infos[i].prefix_len)
                  == 0)
              {
                if (*attr_infos[i].flag)
                  TELL(attr_infos[i].msg);

                attr_to_set         = attr_infos[i].attr;
                *attr_infos[i].flag = 1;
                offset              = attr_infos[i].prefix_len;
                break; /* We have found a matching prefix, *
                        * no need to continue              */
              }
              i++;
            }
            if (attr_infos[i].flag == NULL)
              TELL("Bad attribute prefix -- ");

            /* Attributes must respect the format: */
            /* <fg color>/<bg color>,<styles>      */
            /* """"""""""""""""""""""""""""""""""" */
            if (parse_attr(argv[optind] + offset, &attr, term.colors))
            {
              attr_to_set->is_set    = FORCED;
              attr_to_set->fg        = attr.fg;
              attr_to_set->bg        = attr.bg;
              attr_to_set->bold      = attr.bold;
              attr_to_set->dim       = attr.dim;
              attr_to_set->reverse   = attr.reverse;
              attr_to_set->standout  = attr.standout;
              attr_to_set->underline = attr.underline;
              attr_to_set->italic    = attr.italic;
            }
            else
              TELL("Bad attribute settings -- ");

            optind++;
          }
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'X':
        quiet_timeout = 1;
      case 'x':
        if (optarg && *optarg != '-')
        {
          if (strprefix("current", optarg))
            timeout.mode = CURRENT;
          else if (strprefix("quit", optarg))
            timeout.mode = QUIT;
          else if (strprefix("word", optarg))
          {
            if (argv[optind] && *argv[optind] != '-')
            {
              timeout.mode = WORD;
              timeout_word = argv[optind];
              utf8_interpret(timeout_word, &langinfo);
              optind++;
            }
            else
              TELL("Missing timeout word -- ");
          }
          else
            TELL("Invalid timeout type -- ");

          if (argv[optind] && *argv[optind] != '-')
          {
            if (sscanf(argv[optind], "%5u", &timeout.initial_value) == 1)
            {
              timeout.initial_value *= FREQ;
              timeout.remain = timeout.initial_value;
            }
            else
              TELL("Invalid timeout delay -- ");
          }
          else
            TELL("Missing timeout delay -- ");

          optind++;
        }
        break;

      case 'q':
        toggle.no_scrollbar = 1;
        break;

      case 'A':
        if (optarg && *optarg != '-')
        {
          first_word_pattern = xstrdup(optarg);
          utf8_interpret(first_word_pattern, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'Z':
        if (optarg && *optarg != '-')
        {
          last_word_pattern = xstrdup(optarg);
          utf8_interpret(last_word_pattern, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'W':
        if (optarg && *optarg != '-')
        {
          iws = xstrdup(optarg);
          utf8_interpret(iws, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'L':
        if (optarg && *optarg != '-')
        {
          ils = xstrdup(optarg);
          utf8_interpret(ils, &langinfo);
        }
        else
          TELL("Option requires an argument -- ");
        break;

      case 'P':
        toggle.pinable = 1;
      case 'T':
        toggle.taggable = 1;
        if (optarg != NULL)
        {
          win.sel_sep = xstrdup(optarg);
          utf8_interpret(win.sel_sep, &langinfo);
        }
        break;

      case 'p':
        toggle.autotag = 1;
        break;

      case 'D':
        if (optarg && *optarg != '-')
        {
          int       pos;
          wchar_t * w;
          int       n;

          /* Parse optional additional arguments */
          /* """"""""""""""""""""""""""""""""""" */
          optind--;

          if (*argv[optind] == '-')
            TELL("A blank is required before the first sub-option -- ");

          while (argv[optind] && *argv[optind] != '-')
          {
            if (argv[optind][1] != ':')
              TELL("Bad format -- ");

            switch (*(argv[optind]))
            {
              case 'l': /* left char */
                free(daccess.left);

                daccess.left = xstrdup(argv[optind] + 2);
                utf8_interpret(daccess.left, &langinfo);

                if (utf8_strlen(daccess.left) != 1)
                  TELL("Too many characters after l: -- ");

                n = wcswidth((w = utf8_strtowcs(daccess.left)), 1);
                free(w);

                if (n > 1)
                  TELL(
                    "A multi columns character is not allowed "
                    "after l: -- ");
                break;

              case 'r': /* right char */
                free(daccess.right);

                daccess.right = xstrdup(argv[optind] + 2);
                utf8_interpret(daccess.right, &langinfo);

                if (utf8_strlen(daccess.right) != 1)
                  TELL("Too many characters after r: -- ");

                n = wcswidth((w = utf8_strtowcs(daccess.right)), 1);
                free(w);

                if (n > 1)
                  TELL(
                    "A multi columns character is not allowed "
                    "after r: -- ");
                break;

              case 'a': /* alignment */
                if (strprefix("left", argv[optind] + 2))
                  daccess.alignment = 'l';
                else if (strprefix("right", argv[optind] + 2))
                  daccess.alignment = 'r';
                else
                  TELL("Bad format -- ");
                break;

              case 'p': /* padding */
                if (strprefix("all", argv[optind] + 2))
                  daccess.padding = 'a';
                else if (strprefix("included", argv[optind] + 2))
                  daccess.padding = 'i';
                else
                  TELL("Bad format -- ");
                break;

              case 'w': /* width */
                if (sscanf(argv[optind] + 2, "%d%n", &daccess.length, &pos)
                    != 1)
                  TELL("Bad format -- ");
                if (argv[optind][pos + 2] != '\0')
                  TELL("Bad format -- ");
                if (daccess.length <= 0 || daccess.length > 5)
                  TELL("w suboption must be between 1 and 5 -- ");
                break;

              case 'o': /* start offset */
                if (sscanf(argv[optind] + 2, "%zu%n+", &daccess.offset, &pos)
                    == 1)
                {
                  if (argv[optind][pos + 2] == '+')
                  {
                    daccess.plus = 1;

                    if (argv[optind][pos + 3] != '\0')
                      TELL("Bad format -- ");
                  }
                  else if (argv[optind][pos + 2] != '\0')
                    TELL("Bad format -- ");
                }
                else
                  TELL("Bad format -- ");

                break;

              case 'n': /* numbor of digits to extract */
                if (sscanf(argv[optind] + 2, "%d%n", &daccess.size, &pos) != 1)
                  TELL("Bad format -- ");
                if (argv[optind][pos + 2] != '\0')
                  TELL("Bad format -- ");
                if (daccess.size <= 0 || daccess.size > 5)
                  TELL("n suboption must be between 1 and 5 -- ");
                break;

              case 'i': /* Number of UTF-8 glyphs to ignore after the *
                         * selector to extract                        */
                if (sscanf(argv[optind] + 2, "%zu%n", &daccess.ignore, &pos)
                    != 1)
                  TELL("Bad format -- ");
                if (argv[optind][pos + 2] != '\0')
                  TELL("Bad format -- ");
                break;

              case 'f': /* follow */
                if (strprefix("yes", argv[optind] + 2))
                  daccess.follow = 'y';
                else if (strprefix("no", argv[optind] + 2))
                  daccess.follow = 'n';
                else
                  TELL("Bad format -- ");
                break;

              case 'd': /* decorate */
                free(daccess.num_sep);

                daccess.num_sep = xstrdup(argv[optind] + 2);
                utf8_interpret(daccess.num_sep, &langinfo);

                if (utf8_strlen(daccess.num_sep) != 1)
                  TELL("Too many characters after d: -- ");

                n = wcswidth((w = utf8_strtowcs(daccess.num_sep)), 1);
                free(w);

                if (n > 1)
                  TELL(
                    "A multi columns separator is not allowed "
                    "after d: -- ");
                break;

              case 's': /* start index */
              {
                long pos;

                if (sscanf(argv[optind] + 2, "%ld%ln", &daccess_index, &pos)
                    == 1)
                {
                  if (daccess_index < 0 || *(argv[optind] + 2 + pos) != '\0')
                    daccess_index = 1;
                }
                else
                  TELL("Invalid first index after s: -- ");
              }
              break;

              case 'h': /* head */
                if (strprefix("trim", argv[optind] + 2))
                  daccess.head = 't';
                else if (strprefix("cut", argv[optind] + 2))
                  daccess.head = 'c';
                else if (strprefix("keep", argv[optind] + 2))
                  daccess.head = 'k';
                else
                  TELL("Bad format -- ");
                break;

              default:
                TELL("Bad format -- ");
            }

            if (daccess.length <= 0 || daccess.length > 5)
              daccess.length = -2; /* special value -> auto */

            optind++;
          }
        }
        else
          TELL("Option requires an argument -- ");

        break;

      case 'N':
        if (optarg == NULL)
          optarg = ".";
        else if (*optarg == '\0')
          optarg = ".";

        if (daccess_np == NULL)
        {
          daccess_np = concat("(", optarg, ")", NULL);
          daccess.mode |= DA_TYPE_AUTO; /* auto */
        }
        else
          daccess_np = concat(daccess_np, "|(", optarg, ")", NULL);

        if (daccess.def_number < 0)
          daccess.def_number = 0;

        break;

      case 'U':
        if (optarg == NULL)
          optarg = ".";
        else if (*optarg == '\0')
          optarg = ".";

        if (daccess_up == NULL)
        {
          daccess_up = concat("(", optarg, ")", NULL);
          daccess.mode |= DA_TYPE_AUTO; /* auto */
        }
        else
          daccess_up = concat(daccess_up, "|(", optarg, ")", NULL);

        if (daccess.def_number < 0)
          daccess.def_number = 1;

        break;

      case 'F':
        daccess.mode |= DA_TYPE_POS;

        break;

      case '/':
        if (strprefix("prefix", optarg))
          misc.default_search_method = PREFIX;
        else if (strprefix("fuzzy", optarg))
          misc.default_search_method = FUZZY;
        else if (strprefix("substring", optarg))
          misc.default_search_method = SUBSTRING;
        else
          TELL("Bad format -- ");

        break;

      case '?':
        fputc('\n', stderr);
        short_usage();

        exit(EXIT_FAILURE);

      case 'h':
        usage();

        exit(EXIT_FAILURE);

      default:
        exit(EXIT_FAILURE);
    }
    optarg = NULL;
  }

  if (optind < argc)
  {
    if (argv[argc - 1][0] == '-')
    {
      fprintf(stderr, "Not an option -- %s\n", argv[argc - 1]);
      short_usage();

      exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[argc - 1], "r");
    if (input_file == NULL)
    {
      fprintf(stderr, "Cannot open \"%s\"\n", argv[argc - 1]);
      short_usage();

      exit(EXIT_FAILURE);
    }
  }
  else
    input_file = stdin;

  /* Replace the UTF-8 ASCII representations by their binary values in */
  /* the inclusion and exclusion patterns.                             */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (include_pattern != NULL)
    utf8_interpret(include_pattern, &langinfo);

  if (exclude_pattern != NULL)
    utf8_interpret(exclude_pattern, &langinfo);

  /* Force the right modes when the -C option is given */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  if (cols_selector_list)
  {
    if (win.tab_mode || win.col_mode || win.line_mode)
      win.tab_mode = 0;

    win.col_mode = 1;
  }

  /* Force the right modes when the -R option is given */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  if (rows_selector_list)
  {
    if (win.tab_mode)
      win.tab_mode = 0;

    if (!win.col_mode && !win.line_mode)
      win.line_mode = 1;
  }

  /* If we did not impose the number of columns, use the whole terminal width */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.tab_mode && !win.max_cols)
    win.wide = 1;

  /* Disable the wide mode if not in column mode and not in tab mode */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win.tab_mode && !win.col_mode)
    win.wide = 0;

  win.start = 0;

  void sig_handler(int s);

  /* Ignore SIGTTIN and SIGTTOU */
  /* """""""""""""""""""""""""" */
  sigset_t sigs, oldsigs;

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTTIN);
  sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

  sa.sa_handler = sig_handler;
  sa.sa_flags   = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGALRM, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);

  term.color_method = 1; /* we default to setaf/setbf to set colors */
  term.curs_line = term.curs_column = 0;

  {
    char * str;

    str                        = tigetstr("cuu1");
    term.has_cursor_up         = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("cud1");
    term.has_cursor_down       = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("cub1");
    term.has_cursor_left       = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("cuf1");
    term.has_cursor_right      = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("cup");
    term.has_cursor_address    = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("sc");
    term.has_save_cursor       = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("rc");
    term.has_restore_cursor    = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("setf");
    term.has_setf              = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("setb");
    term.has_setb              = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("setaf");
    term.has_setaf             = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("setab");
    term.has_setab             = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("hpa");
    term.has_hpa               = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("cuf");
    term.has_parm_right_cursor = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("bold");
    term.has_bold              = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("dim");
    term.has_dim               = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("rev");
    term.has_reverse           = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("smul");
    term.has_underline         = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("smso");
    term.has_standout          = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("sitm");
    term.has_italic            = (str == (char *)-1 || str == NULL) ? 0 : 1;
  }

  if (!term.has_cursor_up || !term.has_cursor_down || !term.has_cursor_left
      || !term.has_cursor_right || !term.has_save_cursor
      || !term.has_restore_cursor)
  {
    fprintf(stderr,
            "The terminal does not have the required cursor "
            "management capabilities.\n");

    exit(EXIT_FAILURE);
  }

  /* Get the number of lines/columns of the terminal */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  get_terminal_size(&term.nlines, &term.ncolumns);

  if (custom_ini_file != NULL)
  {
    if (ini_load(custom_ini_file, &win, &term, &limits, &timers, &misc, ini_cb))
      exit(EXIT_FAILURE);
  }
  else
  {
    /* Build the full path of the ini file */
    /* """"""""""""""""""""""""""""""""""" */
    home_ini_file  = make_ini_path(argv[0], "HOME");
    local_ini_file = make_ini_path(argv[0], "PWD");

    /* Set the attributes from the configuration file if possible */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (ini_load(home_ini_file, &win, &term, &limits, &timers, &misc, ini_cb))
      exit(EXIT_FAILURE);

    if (ini_load(local_ini_file, &win, &term, &limits, &timers, &misc, ini_cb))
      exit(EXIT_FAILURE);

    free(home_ini_file);
    free(local_ini_file);
  }

  word_buffer = xcalloc(1, daccess.flength + limits.word_length + 1);

  /* default_search_method is not set in the command line nor in a config */
  /* file, set it to fuzzy.                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (misc.default_search_method == NONE)
    misc.default_search_method = FUZZY;

  /* If some attributes were not set, set their default values */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term.colors > 7)
  {
    int def_attr[5] = { 1, 2, 3, 5, 6 };

    if (!win.cursor_attr.is_set)
    {
      if (term.has_reverse)
        win.cursor_attr.reverse = 1;
      else if (term.has_standout)
        win.cursor_attr.standout = 1;
      else
      {
        win.cursor_attr.fg = 0;
        win.cursor_attr.bg = 1;
      }

      win.cursor_attr.is_set = SET;
    }

    if (!win.cursor_on_tag_attr.is_set)
    {
      if (term.has_reverse)
        win.cursor_on_tag_attr.reverse = 1;

      if (term.has_underline)
        win.cursor_on_tag_attr.underline = 1;
      else
        win.cursor_on_tag_attr.fg = 2;

      win.cursor_on_tag_attr.is_set = SET;
    }

    if (!win.bar_attr.is_set)
    {
      win.bar_attr.fg     = 2;
      win.bar_attr.is_set = SET;
    }

    if (!win.shift_attr.is_set)
    {
      if (term.has_bold)
        win.shift_attr.bold = 1;
      else
        win.shift_attr.fg = 4;

      win.shift_attr.is_set = SET;
    }

    if (!win.search_field_attr.is_set)
    {
      win.search_field_attr.bg     = 5;
      win.search_field_attr.is_set = SET;
    }

    if (!win.search_text_attr.is_set)
    {
      win.search_text_attr.fg = 0;
      win.search_text_attr.bg = 6;

      win.search_text_attr.is_set = SET;
    }

    if (!win.search_err_field_attr.is_set)
    {
      win.search_err_field_attr.bg     = 1;
      win.search_err_field_attr.is_set = SET;
    }

    if (!win.search_err_text_attr.is_set)
    {
      if (term.has_reverse)
        win.search_err_text_attr.reverse = 1;

      win.search_err_text_attr.fg     = 1;
      win.search_err_text_attr.is_set = SET;
    }

    if (!win.match_field_attr.is_set)
    {
      win.match_field_attr.is_set = SET;
    }

    if (!win.match_text_attr.is_set)
    {
      win.match_text_attr.fg     = 5;
      win.match_text_attr.is_set = SET;
    }

    if (!win.match_err_field_attr.is_set)
    {
      win.match_err_field_attr.is_set = SET;
    }

    if (!win.match_err_text_attr.is_set)
    {
      win.match_err_text_attr.fg     = 1;
      win.match_err_text_attr.is_set = SET;
    }

    if (!win.exclude_attr.is_set)
    {
      win.exclude_attr.fg = 6;

      win.exclude_attr.is_set = SET;
    }

    /* This attribute should complete the attributes already set */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (!win.tag_attr.is_set)
    {
      if (term.has_underline)
        win.tag_attr.underline = 1;
      else if (term.has_bold)
        win.tag_attr.bold = 1;
      else
        win.tag_attr.fg = 2;

      win.tag_attr.is_set = SET;
    }

    if (!win.daccess_attr.is_set)
    {
      if (term.has_bold)
        win.daccess_attr.bold = 1;

      win.daccess_attr.is_set = SET;
    }

    for (index = 0; index < 5; index++)
    {
      if (!win.special_attr[index].is_set)
      {
        win.special_attr[index].fg     = def_attr[index];
        win.special_attr[index].is_set = SET;
      }
    }
  }
  else
  {
    if (!win.cursor_attr.is_set)
    {
      if (term.has_reverse)
        win.cursor_attr.reverse = 1;

      win.cursor_attr.is_set = SET;
    }

    if (!win.cursor_on_tag_attr.is_set)
    {
      if (term.has_reverse)
        win.cursor_on_tag_attr.reverse = 1;

      if (term.has_underline)
        win.cursor_on_tag_attr.underline = 1;
      else if (term.has_bold)
        win.cursor_on_tag_attr.bold = 1;

      win.cursor_on_tag_attr.is_set = SET;
    }

    if (!win.bar_attr.is_set)
    {
      if (term.has_bold)
        win.bar_attr.bold = 1;

      win.bar_attr.is_set = SET;
    }

    if (!win.shift_attr.is_set)
    {
      if (term.has_reverse)
        win.shift_attr.reverse = 1;

      win.shift_attr.is_set = SET;
    }

    if (!win.search_field_attr.is_set)
    {
      if (term.has_reverse)
        win.search_field_attr.reverse = 1;

      win.search_field_attr.is_set = SET;
    }

    if (!win.search_text_attr.is_set)
    {
      if (term.has_bold)
        win.search_text_attr.bold = 1;

      win.search_text_attr.is_set = SET;
    }

    if (!win.search_err_field_attr.is_set)
    {
      if (term.has_bold)
        win.search_err_field_attr.bold = 1;

      win.search_err_field_attr.is_set = SET;
    }

    if (!win.search_err_text_attr.is_set)
    {
      if (term.has_reverse)
        win.search_err_text_attr.reverse = 1;

      win.search_err_text_attr.is_set = SET;
    }

    if (!win.match_field_attr.is_set)
    {
      if (term.has_bold)
        win.match_field_attr.bold = 1;
      else if (term.has_reverse)
        win.match_field_attr.reverse = 1;

      win.match_field_attr.is_set = SET;
    }

    if (!win.match_text_attr.is_set)
    {
      if (term.has_reverse)
        win.match_text_attr.reverse = 1;
      else if (term.has_bold)
        win.match_text_attr.bold = 1;

      win.match_text_attr.is_set = SET;
    }

    if (!win.exclude_attr.is_set)
    {
      if (term.has_dim)
        win.exclude_attr.dim = 1;
      else if (term.has_italic)
        win.exclude_attr.italic = 1;
      else if (term.has_bold)
        win.exclude_attr.bold = 1;

      win.exclude_attr.is_set = SET;
    }

    if (!win.tag_attr.is_set)
    {
      if (term.has_underline)
        win.tag_attr.underline = 1;
      else if (term.has_standout)
        win.tag_attr.standout = 1;
      else if (term.has_reverse)
        win.tag_attr.reverse = 1;

      win.tag_attr.is_set = SET;
    }

    if (!win.daccess_attr.is_set)
    {
      if (term.has_bold)
        win.daccess_attr.bold = 1;

      win.daccess_attr.is_set = SET;
    }

    for (index = 0; index < 5; index++)
    {
      if (!win.special_attr[index].is_set)
      {
        if (term.has_bold)
          win.special_attr[index].bold = 1;
        else if (term.has_standout)
          win.special_attr[index].standout = 1;

        win.special_attr[index].is_set = SET;
      }
    }
  }

  if (!quiet_timeout && timeout.initial_value > 0)
  {
    switch (timeout.mode)
    {
      case QUIT:
        timeout_message = xstrdup(
          "[     s before quitting without selecting anything]");
        break;
      case CURRENT:
        timeout_message = xstrdup(
          "[     s before selecting the current highlighted word]");
        break;
      case WORD:
        timeout_message = xcalloc(1, 4
                                       + strlen(
                                           "[     s before selecting the word ")
                                       + strlen(timeout_word));
        strcpy(timeout_message, "[     s before selecting the word \"");
        strcat(timeout_message, timeout_word);
        strcat(timeout_message, "\"]");
        break;
    }

    timeout_seconds = xcalloc(1, 6);
    sprintf(timeout_seconds, "%5u", timeout.initial_value / FREQ);
    memcpy(timeout_message + 1, timeout_seconds, 5);

    message_lines_list = ll_new();

    if (message)
    {
      long len;

      get_message_lines(message, message_lines_list, &message_max_width,
                        &message_max_len);
      ll_append(message_lines_list, timeout_message);

      if ((len = strlen(timeout_message)) > message_max_len)
        message_max_len = message_max_width = len;
    }
    else
    {
      ll_append(message_lines_list, timeout_message);
      message_max_len = message_max_width = strlen(timeout_message);
    }
  }
  else if (message)
  {
    message_lines_list = ll_new();
    get_message_lines(message, message_lines_list, &message_max_width,
                      &message_max_len);
  }

  /* Force the maximum number of window's line if -n is used */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term.nlines <= win.message_lines)
    win.max_lines = 0;
  else if (win.asked_max_lines >= 0)
  {
    if (win.asked_max_lines == 0)
      win.max_lines = term.nlines - win.message_lines;
    else
    {
      if (win.asked_max_lines > term.nlines - win.message_lines)
        win.max_lines = term.nlines - win.message_lines;
      else
        win.max_lines = win.asked_max_lines;
    }
  }
  else
    win.asked_max_lines = win.max_lines;

  /* Allocate the memory for our words structures */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  word_a = xmalloc(WORDSCHUNK * sizeof(word_t));

  /* Fill an array of word_t elements obtained from stdin */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  tab_real_max_size = 0;
  tab_max_size      = 0;
  min_size          = 0;

  /* Parse the word separators string (option -W). If it is empty then  */
  /* the standard delimiters (space, tab and EOL) are used. Each of its */
  /* UTF-8 sequences are stored in a linked list.                       */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  word_delims_list = ll_new();

  if (iws == NULL)
  {
    ll_append(word_delims_list, " ");
    ll_append(word_delims_list, "\t");
    ll_append(word_delims_list, "\n");
  }
  else
  {
    int    utf8_len;
    char * iws_ptr = iws;
    char * tmp;

    utf8_len = mblen(iws_ptr, 4);

    while (utf8_len != 0)
    {
      tmp = xmalloc(utf8_len + 1);
      memcpy(tmp, iws_ptr, utf8_len);
      tmp[utf8_len] = '\0';
      ll_append(word_delims_list, tmp);

      iws_ptr += utf8_len;
      utf8_len = mblen(iws_ptr, 4);
    }
  }

  /* Parse the line separators string (option -L). If it is empty then */
  /* the standard delimiter (newline) is used. Each of its UTF-8       */
  /* sequences are stored in a linked list.                            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  record_delims_list = ll_new();

  if (ils == NULL)
    ll_append(record_delims_list, "\n");
  else
  {
    int    utf8_len;
    char * ils_ptr = ils;
    char * tmp;

    utf8_len = mblen(ils_ptr, 4);

    while (utf8_len != 0)
    {
      tmp = xmalloc(utf8_len + 1);
      memcpy(tmp, ils_ptr, utf8_len);
      tmp[utf8_len] = '\0';
      ll_append(record_delims_list, tmp);

      /* Add this record delimiter as a word delimiter */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      if (ll_find(word_delims_list, tmp, delims_cmp) == NULL)
        ll_append(word_delims_list, tmp);

      ils_ptr += utf8_len;
      utf8_len = mblen(ils_ptr, 4);
    }
  }

  /* Initialize the first chunks of the arrays which will contain the */
  /* maximum length of each column in column mode                     */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    long ci; /* column index */

    col_real_max_size = xmalloc(COLSCHUNK * sizeof(long));
    col_max_size      = xmalloc(COLSCHUNK * sizeof(long));

    for (ci = 0; ci < COLSCHUNK; ci++)
      col_real_max_size[ci] = col_max_size[ci] = 0;

    col_index = cols_number = 0;
  }

  if (daccess_np
      && regcomp(&daccess_np_re, daccess_np, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", daccess_np);

    exit(EXIT_FAILURE);
  }

  if (daccess_up
      && regcomp(&daccess_up_re, daccess_up, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", daccess_up);

    exit(EXIT_FAILURE);
  }

  if (include_pattern
      && regcomp(&include_re, include_pattern, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", include_pattern);

    exit(EXIT_FAILURE);
  }

  if (exclude_pattern
      && regcomp(&exclude_re, exclude_pattern, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", exclude_pattern);

    exit(EXIT_FAILURE);
  }

  if (first_word_pattern
      && regcomp(&first_word_re, first_word_pattern, REG_EXTENDED | REG_NOSUB)
           != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", first_word_pattern);

    exit(EXIT_FAILURE);
  }

  if (last_word_pattern
      && regcomp(&last_word_re, last_word_pattern, REG_EXTENDED | REG_NOSUB)
           != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", last_word_pattern);

    exit(EXIT_FAILURE);
  }

  for (index = 0; index < 5; index++)
  {
    if (special_pattern[index]
        && regcomp(&special_re[index], special_pattern[index],
                   REG_EXTENDED | REG_NOSUB)
             != 0)
    {
      fprintf(stderr, "Bad regular expression %s\n", special_pattern[index]);

      exit(EXIT_FAILURE);
    }
  }

  /* Parse the post-processing patterns and extract its values */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (sed_list != NULL)
  {

    ll_node_t * node = sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -S argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i]\n");

        exit(EXIT_FAILURE);
      }
      if ((!include_visual_only || !exclude_visual_only)
          && ((sed_t *)(node->data))->visual)
      {
        include_visual_only = 1;
        exclude_visual_only = 1;
      }

      node = node->next;
    }
  }

  if (include_sed_list != NULL)
  {
    ll_node_t * node = include_sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -I argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i]\n");

        exit(EXIT_FAILURE);
      }
      if (!include_visual_only && ((sed_t *)(node->data))->visual)
        include_visual_only = 1;

      node = node->next;
    }
  }

  if (exclude_sed_list != NULL)
  {

    ll_node_t * node = exclude_sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -E argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i]\n");

        exit(EXIT_FAILURE);
      }
      if (!exclude_visual_only && ((sed_t *)(node->data))->visual)
        exclude_visual_only = 1;

      node = node->next;
    }
  }

  /* Parse the row selection string if any */
  /* """"""""""""""""""""""""""""""""""""" */
  if (rows_selector_list != NULL)
  {
    ll_node_t * node_selector = rows_selector_list->head;
    filters_t   filter_type;

    rows_filter_type = UNKNOWN_FILTER;
    while (node_selector != NULL)
    {
      rows_selector   = node_selector->data;
      char * unparsed = xstrdup((char *)rows_selector);

      parse_selectors(rows_selector, &filter_type, unparsed,
                      &inc_row_interval_list, &inc_row_regex_list,
                      &exc_row_interval_list, &exc_row_regex_list, &langinfo);

      if (*unparsed != '\0')
      {
        fprintf(stderr, "Bad -R argument. Unparsed part: %s\n", unparsed);

        exit(EXIT_FAILURE);
      }

      if (rows_filter_type == UNKNOWN_FILTER)
        rows_filter_type = filter_type;

      node_selector = node_selector->next;

      free(unparsed);
    }
    merge_intervals(inc_row_interval_list);
    merge_intervals(exc_row_interval_list);
  }

  /* Parse the column selection string if any */
  /* """""""""""""""""""""""""""""""""""""""" */
  if (cols_selector_list != NULL)
  {
    filters_t    filter_type, cols_filter_type;
    interval_t * data;
    ll_node_t *  node;
    ll_node_t *  node_selector = cols_selector_list->head;

    cols_filter = xmalloc(limits.cols);

    cols_filter_type = UNKNOWN_FILTER;
    while (node_selector != NULL)
    {
      cols_selector   = node_selector->data;
      char * unparsed = xstrdup((char *)cols_selector);

      parse_selectors(cols_selector, &filter_type, unparsed,
                      &inc_col_interval_list, &inc_col_regex_list,
                      &exc_col_interval_list, &exc_col_regex_list, &langinfo);

      if (*unparsed != '\0')
      {
        fprintf(stderr, "Bad -C argument. Unparsed part: %s\n", unparsed);

        exit(EXIT_FAILURE);
      }

      merge_intervals(inc_col_interval_list);
      merge_intervals(exc_col_interval_list);

      free(unparsed);

      if (cols_filter_type == UNKNOWN_FILTER)
        cols_filter_type = filter_type;

      /* Only initialize the whole set when -C is encountered for the */
      /* first time.                                                  */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (cols_filter_type == INCLUDE_FILTER)
        memset(cols_filter, SOFT_EXCLUDE_MARK, limits.cols);
      else
        memset(cols_filter, SOFT_INCLUDE_MARK, limits.cols);

      /* Process the explicitly included columns intervals */
      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
      if (inc_col_interval_list != NULL)
        for (node = inc_col_interval_list->head; node; node = node->next)
        {
          data = node->data;

          if (data->low >= limits.cols)
            break;

          if (data->high >= limits.cols)
            data->high = limits.cols - 1;

          memset(cols_filter + data->low, INCLUDE_MARK,
                 data->high - data->low + 1);
        }

      /* Process the explicitly excluded column intervals */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if (exc_col_interval_list != NULL)
        for (node = exc_col_interval_list->head; node; node = node->next)
        {
          data = node->data;

          if (data->low >= limits.cols)
            break;

          if (data->high >= limits.cols)
            data->high = limits.cols - 1;

          memset(cols_filter + data->low, EXCLUDE_MARK,
                 data->high - data->low + 1);
        }

      node_selector = node_selector->next;
    }
  }

  /* Initialize the useful values needed to walk through the rows intervals */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (rows_filter_type == INCLUDE_FILTER)
    row_def_selectable = SOFT_EXCLUDE_MARK;
  else if (rows_filter_type == EXCLUDE_FILTER)
    row_def_selectable = SOFT_INCLUDE_MARK;
  else
  {
    if (pattern_def_include == 0)
      row_def_selectable = SOFT_EXCLUDE_MARK;
    else
      row_def_selectable = SOFT_INCLUDE_MARK;
  }

  /* Set the head of the interval list */
  /* """"""""""""""""""""""""""""""""" */
  if (inc_row_interval_list)
    inc_interval_node = inc_row_interval_list->head;
  else
    inc_interval_node = NULL;

  if (exc_row_interval_list)
    exc_interval_node = exc_row_interval_list->head;
  else
    exc_interval_node = NULL;

  /* And get the first interval */
  /* """""""""""""""""""""""""" */
  if (inc_interval_node)
    inc_interval = (interval_t *)inc_interval_node->data;
  else
    inc_interval = NULL;

  if (exc_interval_node)
    exc_interval = (interval_t *)exc_interval_node->data;
  else
    exc_interval = NULL;

  /* First pass:                                                  */
  /* Get and process the input stream words                       */
  /* In this pass, the different actions will occur:              */
  /* - A new word is read from stdin                              */
  /* - A new SOL and or EOL is possibly set                       */
  /* - A special level is possibly affected to the word just read */
  /* - The -R is taken into account                               */
  /* - The first part of the -C option is done                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  while (
    (word = get_word(input_file, word_delims_list, record_delims_list,
                     utf8_buffer, &is_last, &toggle, &langinfo, &win, &limits))
    != NULL)
  {
    int selectable;
    int is_first = 0;
    int special_level;
    int row_inc_matched = 0;

    if (*word == '\0')
      continue;

    /* Manipulates the is_last flag word indicator to make this word     */
    /* the first or last one of the current line in column/line/tab mode */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode || win.line_mode || win.tab_mode)
    {
      if (first_word_pattern
          && regexec(&first_word_re, word, (int)0, NULL, 0) == 0)
        is_first = 1;

      if (last_word_pattern && !is_last
          && regexec(&last_word_re, word, (int)0, NULL, 0) == 0)
        is_last = 1;
    }

    /* Check if the word is special */
    /* """""""""""""""""""""""""""" */
    special_level = 0;
    for (index = 0; index < 5; index++)
    {
      if (special_pattern[index] != NULL
          && regexec(&special_re[index], word, (int)0, NULL, 0) == 0)
      {
        special_level = (int)index + 1;
        break;
      }
    }

    /* Default selectable state */
    /* """""""""""""""""""""""" */
    selectable = SOFT_INCLUDE_MARK;

    /* For each new line check if the line is in the current  */
    /* interval or if we need to get the next interval if any */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (rows_selector)
    {
      if (count > 0 && word_a[count - 1].is_last)
      {
        /* We are in a new line, reset the flag indicating that we are on */
        /* a line selected by a regular expression  and the flag saying   */
        /* that the whole line has been excluded.                         */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        line_selected_by_regex = 0;
        line_excluded          = 0;

        /* And also reset the flag telling that the row has been explicitly */
        /* removed from the selectable list of words                        */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        row_inc_matched = 0;

        /* Increment the line counter used to see if we are an include or */
        /* exclude set of lines                                           */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        line_count++;

        /* Look if we need to use the next interval of the list */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
        if (inc_interval_node && line_count > inc_interval->high)
        {
          inc_interval_node = inc_interval_node->next;
          if (inc_interval_node)
            inc_interval = (interval_t *)inc_interval_node->data;
        }

        if (exc_interval_node && line_count > exc_interval->high)
        {
          exc_interval_node = exc_interval_node->next;
          if (exc_interval_node)
            exc_interval = (interval_t *)exc_interval_node->data;
        }
      }

      /* Look if the line is in an excluded or included line.             */
      /* The included line intervals are only checked if the word didn't  */
      /* belong to an excluded line interval before.                      */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (exc_interval)
      {
        if (line_count >= exc_interval->low && line_count <= exc_interval->high)
          selectable = EXCLUDE_MARK;
      }
      if (selectable != EXCLUDE_MARK && inc_interval)
      {
        if (line_count >= inc_interval->low && line_count <= inc_interval->high)
        {
          selectable = INCLUDE_MARK;

          /* As the raw has been explicitly selected, record that so than */
          /* we can distinguish that from the implicit selection          */
          /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
          row_inc_matched = 1;
        }
      }
    }

    /* Check if the all the words in the current row must be included or */
    /* excluded from the selectable set of words                         */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (selectable != EXCLUDE_MARK)
    {
      /* Look in the excluded list of regular expressions */
      /* '''''''''''''''''''''''''''''''''''''''''''''''' */
      if (exc_row_regex_list != NULL)
      {
        regex_t * row_re;

        ll_node_t * row_regex_node = exc_row_regex_list->head;

        while (row_regex_node != NULL)
        {
          row_re = row_regex_node->data;
          if (regexec(row_re, word, (int)0, NULL, 0) == 0)
          {
            long c = count - 1;

            /* Mark all the next words of the line as excluded */
            /* ''''''''''''''''''''''''''''''''''''''''''''''' */
            line_selected_by_regex = 1;
            line_excluded          = 1;

            /* Mark all the previous words of the line as excluded */
            /* ''''''''''''''''''''''''''''''''''''''''''''''''''' */
            while (c >= 0 && !word_a[c].is_last)
            {
              word_a[c].is_selectable = EXCLUDE_MARK;
              c--;
            }

            /* Mark the current word as not excluded */
            /* ''''''''''''''''''''''''''''''''''''' */
            selectable = EXCLUDE_MARK;

            /* No need to continue as the line is already marked as excluded. */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
            break;
          }

          row_regex_node = row_regex_node->next;
        }
      }

      /* If the line has not yet been excluded and the list of explicitly  */
      /* include regular expressions is not empty then give them a chance. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (selectable != EXCLUDE_MARK && inc_row_regex_list != NULL)
      {
        regex_t * row_re;

        ll_node_t * row_regex_node = inc_row_regex_list->head;

        while (row_regex_node != NULL)
        {
          row_re = row_regex_node->data;
          if (regexec(row_re, word, (int)0, NULL, 0) == 0)
          {
            long c = count - 1;

            while (c >= 0 && !word_a[c].is_last)
            {
              /* Do not include an already excluded word */
              /* """"""""""""""""""""""""""""""""""""""" */
              if (word_a[c].is_selectable)
                word_a[c].is_selectable = INCLUDE_MARK;

              c--;
            }

            /* Mark all the next words of the line as included */
            /* ''''''''''''''''''''''''''''''''''''''''''''''' */
            line_selected_by_regex = 1;

            /* Mark all the previous words of the line as included */
            /* ''''''''''''''''''''''''''''''''''''''''''''''''''' */
            selectable = INCLUDE_MARK;
          }

          row_regex_node = row_regex_node->next;
        }
      }
    }

    /* If the line contains a word that matched a regex which determines */
    /* the inclusion of exclusion of this line, then use the regex       */
    /* selection flag to determine the inclusion/exclusion of the future */
    /* words in the line.                                                */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (line_excluded)
      selectable = EXCLUDE_MARK;
    else
    {
      if (line_selected_by_regex)
        selectable = (row_def_selectable == ROW_REGEX_EXCLUDE)
                       ? SOFT_EXCLUDE_MARK
                       : INCLUDE_MARK;

      /* Check if the current word is matching an include or exclude */
      /* pattern                                                     */
      /* Only do it if if hasn't be explicitly deselected before.    */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (selectable != EXCLUDE_MARK)
      {
        /* Check if the word will be excluded in the list of selectable */
        /* words or not.                                                */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        if (exclude_pattern)
        {
          if (regexec(&exclude_re, word, (int)0, NULL, 0) == 0)
            selectable = EXCLUDE_MARK;
        }

        if (selectable != 0 && !line_selected_by_regex)
        {
          /* Check if the word will be included in the list of selectable */
          /* words or not.                                                */
          /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
          if (include_pattern)
          {
            if (regexec(&include_re, word, (int)0, NULL, 0) == 0)
              selectable = INCLUDE_MARK;
            else if (!row_inc_matched)
              selectable = row_def_selectable;
          }
          else if (rows_selector && !row_inc_matched)
            selectable = row_def_selectable;
        }
      }
    }

    if (win.col_mode)
    {
      /* In column mode we must manage the allocation space for some       */
      /* column's related data structures and check if some limits ave not */
      /* been reached.                                                     */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

      if (is_first)
        col_index = 1;
      else
      {
        col_index++;

        if (col_index > cols_number)
        {
          /* Check the limits */
          /* '''''''''''''''' */
          if (col_index == limits.cols)
          {
            fprintf(stderr,
                    "The number of columns has reached the limit "
                    "(%ld), exiting.\n",
                    limits.cols);

            exit(EXIT_FAILURE);
          }

          cols_number++;

          /* Look if we need to enlarge the arrays indexed by the */
          /* number of columns.                                   */
          /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
          if (cols_number % COLSCHUNK == 0)
          {
            long ci; /* column index */

            col_real_max_size = xrealloc(col_real_max_size,
                                         (cols_number + COLSCHUNK)
                                           * sizeof(long));

            col_max_size = xrealloc(col_max_size,
                                    (cols_number + COLSCHUNK) * sizeof(long));

            /* Initialize the max size for the new columns */
            /* ''''''''''''''''''''''''''''''''''''''''''' */
            for (ci = 0; ci < COLSCHUNK; ci++)
            {
              col_real_max_size[cols_number + ci] = 0;
              col_max_size[cols_number + ci]      = 0;
            }
          }
        }
      }

      /* We must now check if the word matches a RE that */
      /* exclude the whole column.                       */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      if (cols_selector != NULL)
      {
        long ci; /* column index */

        regex_t * col_re;

        if (cols_filter[col_index - 1] == EXCLUDE_MARK)
          selectable = EXCLUDE_MARK;
        else
        {
          if (exc_col_regex_list != NULL)
          {
            /* Some columns must be excluded by regex */
            /* '''''''''''''''''''''''''''''''''''''' */
            ll_node_t * col_regex_node = exc_col_regex_list->head;

            while (col_regex_node != NULL)
            {
              col_re = col_regex_node->data;

              if (regexec(col_re, word, (int)0, NULL, 0) == 0)
              {
                cols_filter[col_index - 1] = EXCLUDE_MARK;
                selectable                 = EXCLUDE_MARK;

                /* Mark non selectable the items above in the column */
                /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
                ci = 0;
                for (wi = 0; wi < count; wi++)
                {
                  if (ci == col_index - 1)
                    word_a[wi].is_selectable = EXCLUDE_MARK;

                  if (word_a[wi].is_last)
                    ci = 0;
                  else
                    ci++;
                }
                break;
              }

              col_regex_node = col_regex_node->next;
            }
          }

          if (inc_col_regex_list != NULL)
          {
            /* Some columns must be included by regex */
            /* '''''''''''''''''''''''''''''''''''''' */
            ll_node_t * col_regex_node = inc_col_regex_list->head;

            while (col_regex_node != NULL)
            {
              col_re = col_regex_node->data;

              if (regexec(col_re, word, (int)0, NULL, 0) == 0)
              {
                cols_filter[col_index - 1] = INCLUDE_MARK;
                break;
              }

              col_regex_node = col_regex_node->next;
            }
          }
        }
      }
    }

    /* Store some known values in the current word's structure */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_a[count].start = word_a[count].end = 0;

    word_a[count].str           = word;
    word_a[count].is_selectable = selectable;

    word_a[count].special_level = special_level;
    word_a[count].is_matching   = 0;
    word_a[count].is_tagged     = 0;
    word_a[count].is_numbered   = 0;
    word_a[count].tag_order     = 0;

    if (win.col_mode || win.line_mode || win.tab_mode)
    {
      /* Set the last word in line indicator when in column/line/tab mode */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (is_first && count > 0)
        word_a[count - 1].is_last = 1;
      word_a[count].is_last = is_last;
      if (is_last)
        col_index = 0;
    }
    else
      word_a[count].is_last = 0;

    /* One more word... */
    /* """""""""""""""" */
    if (count == limits.words)
    {
      fprintf(stderr,
              "The number of read words has reached the limit "
              "(%ld), exiting.\n",
              limits.words);

      exit(EXIT_FAILURE);
    }

    count++;

    if (count % WORDSCHUNK == 0)
      word_a = xrealloc(word_a, (count + WORDSCHUNK) * sizeof(word_t));
  }

  /* Early exit if there is no input or if no word is selected */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (count == 0)
    exit(EXIT_FAILURE);

  /* The last word is always the last of its line */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode || win.line_mode || win.tab_mode)
    word_a[count - 1].is_last = 1;

  /* Second pass to modify  the word according to all/include/exclude       */
  /* regular expressions and the columns settings set in the previous pass. */
  /* This must be done separately because in the first  pass, some word     */
  /* could have been marked as excluded before the currently processed word */
  /*  (second part of the -C option)                                        */
  /* In this pass the following actions will also be done:                  */
  /* - Finish the work on columns.                                          */
  /* - Possibly modify the word according to -S/-I/-E arguments             */
  /* - Replace unprintable characters in the word by mnemonics              */
  /* - Remember the max size of the words/columns/tabs                      */
  /* - Insert the word in a TST tree for future search                      */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  col_index = 0;
  for (wi = 0; wi < count; wi++)
  {
    char *    unaltered_word;
    long      size;
    long      word_len;
    wchar_t * tmpw;
    word_t *  word;
    long      s;
    long      len;
    char *    expanded_word;
    long      i;

    /* If the column section argument is set, then adjust the final        */
    /* selectable attribute  according to the already set words and column */
    /* selectable flag contents                                            */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (cols_selector_list != NULL)
    {
      if (cols_filter[col_index] == EXCLUDE_MARK)
        word_a[wi].is_selectable = EXCLUDE_MARK;
      else if (word_a[wi].is_selectable != EXCLUDE_MARK)
      {
        switch (cols_filter[col_index])
        {
          case INCLUDE_MARK:
            word_a[wi].is_selectable = INCLUDE_MARK;
            break;

          case SOFT_EXCLUDE_MARK:
            if (word_a[wi].is_selectable == SOFT_EXCLUDE_MARK
                || word_a[wi].is_selectable == SOFT_INCLUDE_MARK)
              word_a[wi].is_selectable = EXCLUDE_MARK;
            else
              word_a[wi].is_selectable = INCLUDE_MARK;
            break;

          case SOFT_INCLUDE_MARK:
            if (word_a[wi].is_selectable == SOFT_EXCLUDE_MARK)
              word_a[wi].is_selectable = EXCLUDE_MARK;
            else
              word_a[wi].is_selectable = INCLUDE_MARK;
            break;
        }
      }
    }

    word = &word_a[wi];

    /* Make sure that daccess.length >= daccess.size */
    /* with DA_TYPE_POS.                             */
    /* """"""""""""""""""""""""""""""""""""""""""""" */
    if (daccess.mode != DA_TYPE_NONE)
    {
      if (daccess.mode & DA_TYPE_POS)
      {
        if (daccess.size > 0)
          if (daccess.size > daccess.length)
            daccess.length = daccess.size;
      }

      /* Auto determination of the length of the selector */
      /* with DA_TYPE_AUTO.                               */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((daccess.mode & DA_TYPE_AUTO) && daccess.length == -2)
      {
        long n = count;

        daccess.length = 0;

        while (n)
        {
          n /= 10;
          daccess.length++;
        }
      }

      /* Set the full length of the prefix in case of numbering */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (daccess.length > 0)
        daccess.flength = 3 + daccess.length;

      if (word->is_selectable != EXCLUDE_MARK
          && word->is_selectable != SOFT_EXCLUDE_MARK)
      {
        char * selector;
        char * tmp      = xmalloc(strlen(word->str) + 4 + daccess.length);
        long * word_pos = xmalloc(sizeof(long));
        int    may_number;

        if (!isempty(word->str))
        {
          *word_pos = wi;

          tmp[0] = ' ';

          /* Check if the word is eligible to the numbering process */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (daccess_up == NULL && daccess_np == NULL)
          {
            if (daccess.mode & DA_TYPE_POS)
              may_number = 1;
            else
              may_number = 0;
          }
          else
          {
            if (daccess_up != NULL
                && !!regexec(&daccess_up_re, word->str, (int)0, NULL, 0) == 0)
              may_number = 0;
            else
            {
              if (daccess_np != NULL
                  && !!regexec(&daccess_np_re, word->str, (int)0, NULL, 0) == 0)
                may_number = 1;
              else
                may_number = daccess.def_number;
            }
          }

          /* It is... */
          /* """""""" */
          if (may_number)
          {
            if (daccess.mode & DA_TYPE_POS)
            {
              if (!word->is_numbered)
              {
                if (daccess.size > 0
                    && daccess.offset + daccess.size + daccess.ignore
                         <= utf8_strlen(word->str))
                {
                  unsigned selector_value; /* numerical value of the         *
                                            * extracted selector             */
                  long selector_offset;    /* offset in byte to the selector *
                                            * to extract                     */
                  char * ptr;              /* points just after the selector *
                                            * to extract   */
                  long plus_offset;        /* points to the first occurrence *
                                            * of a number in word->str after *
                                            * the offset given               */

                  selector_offset = utf8_offset(word->str, daccess.offset);

                  if (daccess.plus)
                  {
                    plus_offset = strcspn(word->str + selector_offset,
                                          "0123456789");

                    if (plus_offset + daccess.size + daccess.ignore
                        <= strlen(word->str))
                      selector_offset += plus_offset;
                  }

                  ptr      = word->str + selector_offset;
                  selector = xstrndup(ptr, daccess.size);

                  /* read the embedded number and, if correct, format */
                  /* it according to daccess.alignment                */
                  /* """""""""""""""""""""""""""""""""""""""""""""""" */
                  if (sscanf(selector, "%u", &selector_value) == 1)
                  {
                    sprintf(selector, "%u", selector_value);

                    sprintf(tmp + 1, "%*u",
                            daccess.alignment == 'l' ? -daccess.length
                                                     : daccess.length,
                            selector_value);

                    /* Overwrite the end of the word to erase the selector */
                    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                    my_strcpy(ptr, ptr + daccess.size
                                     + utf8_offset(ptr + daccess.size,
                                                   daccess.ignore));

                    /* Modify the word according to the 'h' directive of -D */
                    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
                    if (daccess.head == 'c')
                      /* h:c is present cut the leading characters */
                      /* before the selector                       */
                      /* ''''''''''''''''''''''''''''''''''''''''' */
                      memmove(word->str, ptr, strlen(ptr) + 1);
                    else if (daccess.head == 't')
                    {
                      /* h:t is present trim the leading characters  */
                      /* before the selector if they are ' ' or '\t' */
                      /* ''''''''''''''''''''''''''''''''''''''''''' */
                      char * p = word->str;

                      while (p != ptr && (*p == ' ' || *p == '\t'))
                        p++;

                      if (p == ptr)
                        memmove(word->str, ptr, strlen(ptr) + 1);
                    }

                    ltrim(selector, " ");
                    rtrim(selector, " ", 0);

                    tst_daccess = tst_insert(tst_daccess,
                                             utf8_strtowcs(selector), word_pos);

                    if (daccess.follow == 'y')
                      daccess_index = selector_value + 1;

                    word->is_numbered = 1;
                  }
                  free(selector);
                }
              }
            }

            /* Try to number this word if it is still non numbered and */
            /* the -N/-U option is given.                              */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (!word->is_numbered && (daccess.mode & DA_TYPE_AUTO))
            {
              sprintf(tmp + 1, "%*ld",
                      daccess.alignment == 'l' ? -daccess.length
                                               : daccess.length,
                      daccess_index);

              selector = xstrdup(tmp + 1);
              ltrim(selector, " ");
              rtrim(selector, " ", 0);

              /* Insert it in the tst tree containing the selector's */
              /* digits.                                             */
              /* ''''''''''''''''''''''''''''''''''''''''''''''''''' */
              tst_daccess = tst_insert(tst_daccess, utf8_strtowcs(selector),
                                       word_pos);
              daccess_index++;

              free(selector);

              word->is_numbered = 1;
            }
          }

          /* Fill the space taken by the numbering by space if the word */
          /* is not numbered.                                           */
          if (daccess.length > 0 && !word->is_numbered)
          {
            for (i = 0; i < daccess.flength; i++)
              tmp[i] = ' ';
          }

          /* Make sure that the 2 character after this placeholder */
          /* are initialized.                                      */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (daccess.length > 0)
          {
            tmp[1 + daccess.length] = ' ';
            tmp[2 + daccess.length] = ' ';
          }
        }
        else if (daccess.length > 0)
        {
          /* make sure that the prefix of empty word is blank */
          /* as they may be display in column mode *          */
          /* """""""""""""""""""""""""""""""""""""""""""""""" */
          for (i = 0; i < daccess.flength; i++)
            tmp[i] = ' ';
        }

        if (daccess.length > 0)
        {
          my_strcpy(tmp + daccess.flength, word->str);
          free(word->str);
          word->str = tmp;
        }
        else
          free(tmp);
      }
      else
      {
        /* Should we also add space at the beginning of excluded words ?  */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (daccess.padding == 'a')
        {
          char * tmp = xmalloc(strlen(word->str) + 4 + daccess.length);
          for (i = 0; i < daccess.flength; i++)
            tmp[i] = ' ';
          my_strcpy(tmp + daccess.flength, word->str);
          free(word->str);
          word->str = tmp;
        }
      }
    }
    else
    {
      daccess.size   = 0;
      daccess.length = 0;
    }

    /* Save the original word */
    /* """""""""""""""""""""" */
    unaltered_word = xstrdup(word->str);

    /* Possibly modify the word according to -S/-I/-E arguments */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    {
      ll_node_t * node = NULL;
      char *      tmp;

      /* Manage the -S case */
      /* """""""""""""""""" */
      if (sed_list != NULL)
      {
        node = sed_list->head;

        while (node != NULL)
        {
          tmp = xstrndup(word->str, daccess.flength);
          if (replace(word->str + daccess.flength, (sed_t *)(node->data)))
          {

            free(word->str);
            memmove(word_buffer + daccess.flength, word_buffer,
                    strlen(word_buffer) + 1);
            memmove(word_buffer, tmp, daccess.flength);

            word->str = xstrdup(word_buffer);

            if (((sed_t *)(node->data))->stop)
              break;
          }

          *word_buffer = '\0';
          node         = node->next;
          free(tmp);
        }
      }
      else
      {
        /* Manage the -I/-E case  */
        /* """""""""""""""""""""" */
        if ((word->is_selectable == INCLUDE_MARK
             || word->is_selectable == SOFT_INCLUDE_MARK)
            && include_sed_list != NULL)
          node = include_sed_list->head;
        else if ((word->is_selectable == EXCLUDE_MARK
                  || word->is_selectable == SOFT_EXCLUDE_MARK)
                 && exclude_sed_list != NULL)
          node = exclude_sed_list->head;
        else
          node = NULL;

        *word_buffer = '\0';

        while (node != NULL)
        {
          tmp = xstrndup(word->str, daccess.flength);
          if (replace(word->str + daccess.flength, (sed_t *)(node->data)))
          {

            free(word->str);
            memmove(word_buffer + daccess.flength, word_buffer,
                    strlen(word_buffer) + 1);
            memmove(word_buffer, tmp, daccess.flength);

            word->str = xstrdup(word_buffer);

            if (((sed_t *)(node->data))->stop)
              break;
          }
          *word_buffer = '\0';
          node         = node->next;
          free(tmp);
        }
      }
    }

    /* A substitution leading to an empty word is invalid in column mode. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode)
    {
      if (*(word->str + daccess.flength) == '\0')
        exit(EXIT_FAILURE);
    }

    /* Alter the word just read be replacing special chars  by their */
    /* escaped equivalents.                                          */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len = strlen(word->str);

    expanded_word = xmalloc(5 * word_len + 1);
    len           = expand(word->str, expanded_word, &langinfo, &toggle);

    /* Update it if needed */
    /* ''''''''''''''''''' */
    if (strcmp(expanded_word, word->str) != 0)
    {
      word_len = len;
      free(word->str);
      word->str = xstrdup(expanded_word);
    }

    free(expanded_word);

    if (win.col_mode)
    {
      /* Update the max values of col_real_max_size[col_index] */
      /* and col_max_size[col_index]                           */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((s = (long)word_len) > col_real_max_size[col_index])
      {
        col_real_max_size[col_index] = s;

        /* Also update the real max size of all columns seen */
        /* """"""""""""""""""""""""""""""""""""""""""""""""" */
        if (s > cols_real_max_size)
          cols_real_max_size = s;
      }

      s = (long)mbstowcs(NULL, word->str, 0);
      s = wcswidth((tmpw = utf8_strtowcs(word->str)), s);
      free(tmpw);

      if (s > col_max_size[col_index])
      {
        col_max_size[col_index] = s;

        /* Also update the max size of all columns seen */
        /* """""""""""""""""""""""""""""""""""""""""""" */
        if (s > cols_max_size)
          cols_max_size = s;
      }
      /* Update the size of the longest expanded word */
      /* """""""""""""""""""""""""""""""""""""""""""" */
      word_real_max_size = cols_real_max_size;
    }
    else if (win.tab_mode)
    {
      /* Store the new max number of bytes in a word      */
      /* and update the size of the longest expanded word */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((size = (long)word_len) > tab_real_max_size)
        word_real_max_size = tab_real_max_size = (long)word_len;

      /* Store the new max word width */
      /* """""""""""""""""""""""""""" */
      size = (long)mbstowcs(NULL, word->str, 0);

      if ((size = wcswidth((tmpw = utf8_strtowcs(word->str)), size))
          > tab_max_size)
        tab_max_size = size;

      free(tmpw);
    }
    else if (word_real_max_size < word_len)
      /* Update the size of the longest expanded word */
      /* """""""""""""""""""""""""""""""""""""""""""" */
      word_real_max_size = word_len;

    /* When the visual only flag is set, we keep the unaltered word so */
    /* that it can be restored even if its visual and searchable       */
    /* representation may have been altered by the previous code       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

    /* Record the length of the word in bytes. This information will be */
    /* used if the -k option (keep spaces ) is not set.                 */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word->len = strlen(word->str);

    /* Save the non modified word in .orig if it has been altered */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((strcmp(word->str, unaltered_word) != 0)
        && ((word->is_selectable && include_visual_only)
            || (!word->is_selectable && exclude_visual_only)))
    {
      word->orig = unaltered_word;
    }
    else
    {
      word->orig = NULL;
      free(unaltered_word);
    }

    if (win.col_mode)
    {
      if (word_a[wi].is_last)
        col_index = 0;
      else
        col_index++;
    }
  }

  /* Set the minimum width of a column (-w and -t or -c option) */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.wide)
  {
    if (win.tab_mode)
    {
      if (win.max_cols > 0)
        min_size = (term.ncolumns - 2) / win.max_cols - 1;

      if (min_size < tab_max_size)
        min_size = tab_max_size;

      word_real_max_size = min_size + tab_real_max_size - tab_max_size;
    }
    else /* column mode */
    {
      min_size = (term.ncolumns - 2) / cols_number;
      if (min_size < cols_max_size)
        min_size = cols_max_size;

      word_real_max_size = cols_real_max_size;
    }
  }

  /* Third (compress) pass: remove all empty word and words containing */
  /* only spaces when not in column mode.                              */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win.col_mode)
  {
    long offset;

    offset = 0;
    for (wi = 0; wi < count; wi++)
    {
      while (wi + offset < count
             && isempty(word_a[wi + offset].str + daccess.flength))
      {
        /* Keep non selectable empty words to allow special effects */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        if (word_a[wi + offset].is_selectable == SOFT_EXCLUDE_MARK
            || word_a[wi + offset].is_selectable == EXCLUDE_MARK)
          break;

        offset++;
      }

      if (offset > 0)
        word_a[wi] = word_a[wi + offset];
    }
    count -= offset;
  }

  if (count == 0)
    exit(EXIT_FAILURE);

  /* Allocate the space for the satellites arrays */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  line_nb_of_word_a    = xmalloc(count * sizeof(long));
  first_word_in_line_a = xmalloc(count * sizeof(long));

  /* Fourth pass:                                                         */
  /* When in column or tabulating mode, we need to adjust the length of   */
  /* all the words by adding the right number of spaces so that they will */
  /* be aligned correctly. In column mode the size of each column is      */
  /* variable; in tabulate mode it is constant.                           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    char * temp;

    /* Sets all columns to the same size when -w and -c are both set */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.wide)
      for (col_index = 0; col_index < cols_number; col_index++)
      {
        col_max_size[col_index]      = cols_max_size;
        col_real_max_size[col_index] = cols_real_max_size;
      }

    /* Total space taken by all the columns plus the gutter */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    win.max_width      = 0;
    win.real_max_width = 0;
    for (col_index = 0; col_index < cols_number; col_index++)
    {
      if (win.max_width + col_max_size[col_index] + 1 <= term.ncolumns - 2)
        win.max_width += col_max_size[col_index] + 1;

      win.real_max_width += col_max_size[col_index] + 1;
    }

    col_index = 0;
    for (wi = 0; wi < count; wi++)
    {
      long      s1, s2;
      long      word_width;
      wchar_t * w;

      s1         = (long)strlen(word_a[wi].str);
      word_width = mbstowcs(NULL, word_a[wi].str, 0);
      s2         = wcswidth((w = utf8_strtowcs(word_a[wi].str)), word_width);
      free(w);
      temp = xcalloc(1, col_real_max_size[col_index] + s1 - s2 + 1);
      memset(temp, ' ', col_max_size[col_index] + s1 - s2);
      memcpy(temp, word_a[wi].str, s1);
      temp[col_real_max_size[col_index] + s1 - s2] = '\0';
      free(word_a[wi].str);
      word_a[wi].str = temp;

      if (word_a[wi].is_last)
        col_index = 0;
      else
        col_index++;
    }
  }
  else if (win.tab_mode)
  {
    char * temp;

    if (tab_max_size < min_size)
    {
      tab_max_size = min_size;
      if (tab_max_size > tab_real_max_size)
        tab_real_max_size = tab_max_size;
    }

    for (wi = 0; wi < count; wi++)
    {
      long      s1, s2;
      long      word_width;
      wchar_t * w;

      s1         = (long)strlen(word_a[wi].str);
      word_width = mbstowcs(NULL, word_a[wi].str, 0);
      s2         = wcswidth((w = utf8_strtowcs(word_a[wi].str)), word_width);
      free(w);
      temp = xcalloc(1, tab_real_max_size + s1 - s2 + 1);
      memset(temp, ' ', tab_max_size + s1 - s2);
      memcpy(temp, word_a[wi].str, s1);
      temp[tab_real_max_size + s1 - s2] = '\0';
      free(word_a[wi].str);
      word_a[wi].str = temp;
    }
  }

  /* Fifth pass: transforms the remaining SOFT_EXCLUDE_MARKs with */
  /* EXCLUDE_MARKs.                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (wi = 0; wi < count; wi++)
  {
    long *    data;
    wchar_t * w;
    ll_t *    list;

    if (word_a[wi].is_selectable == SOFT_EXCLUDE_MARK)
      word_a[wi].is_selectable = EXCLUDE_MARK;

    /* If the word is selectable insert it in the TST tree */
    /* with its associated index in the input stream.      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[wi].is_selectable)
    {
      data  = xmalloc(sizeof(long));
      *data = wi;

      /* Create a wide characters string from the word screen representation */
      /* to be able to store in in the TST.                                  */
      /* Note that the direct access selector,if any, is not stored.         */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_a[wi].is_numbered)
        w = utf8_strtowcs(word_a[wi].str + daccess.flength);
      else
        w = utf8_strtowcs(word_a[wi].str);

      /* If we didn't already encounter this word, then create a new entry in */
      /* the TST for it and store its index in its list.                      */
      /* Otherwise, add its index in its index list.                          */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (tst_word && (list = tst_search(tst_word, w)) != NULL)
        ll_append(list, data);
      else
      {
        list = ll_new();
        ll_append(list, data);
        tst_word = tst_insert(tst_word, w, list);
      }
      free(w);
    }
  }

  /* The word after the last one is set to NULL */
  /* """""""""""""""""""""""""""""""""""""""""" */
  word_a[count].str = NULL;

  /* We can now allocate the space for our tmp_word work variable */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tmp_word = xmalloc(word_real_max_size + 1);

  search_data.utf8_off_a = xmalloc(word_real_max_size * sizeof(long));
  search_data.utf8_len_a = xmalloc(word_real_max_size * sizeof(long));

  win.start = 0; /* index of the first element in the    *
                  * words array to be  displayed         */

  /* We can now build the first metadata */
  /* """"""""""""""""""""""""""""""""""" */
  last_line = build_metadata(&term, count, &win);

  /* Index of the selected element in the array words                */
  /* The string can be:                                              */
  /*   "last"    The string "last"   put the cursor on the last word */
  /*   n         a number            put the cursor on the word n    */
  /*   /pref     /+a regexp          put the cursor on the first     */
  /*                                 word matching the prefix "pref" */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  for (wi = 0; wi < count; wi++)
    word_a[wi].bitmap = xcalloc(1, (word_a[wi].mb - daccess.flength) / CHAR_BIT
                                     + 1);

  /* Find the first selectable word (if any) in the input stream */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (first_selectable = 0;
       first_selectable < count && !word_a[first_selectable].is_selectable;
       first_selectable++)
    ;

  /* If not found, abort */
  /* """"""""""""""""""" */
  if (first_selectable == count)
  {
    fprintf(stderr, "No selectable word found.\n");

    exit(EXIT_FAILURE);
  }

  /* Else find the last selectable word in the input stream */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
  last_selectable = count - 1;
  while (last_selectable > 0 && !word_a[last_selectable].is_selectable)
    last_selectable--;

  if (pre_selection_index == NULL)
    /* Option -s was not used */
    /* """""""""""""""""""""" */
    current = first_selectable;
  else if (*pre_selection_index == '/')
  {
    /* A regular expression is expected */
    /* """""""""""""""""""""""""""""""" */
    regex_t re;
    long    index;

    if (regcomp(&re, pre_selection_index + 1, REG_EXTENDED | REG_NOSUB) != 0)
    {
      fprintf(stderr, "Invalid regular expression (%s)\n", pre_selection_index);

      exit(EXIT_FAILURE);
    }
    else
    {
      int    found = 0;
      char * word;

      for (index = first_selectable; index <= last_selectable; index++)
      {
        if (!word_a[index].is_selectable)
          continue;

        if (word_a[index].orig != NULL)
          word = word_a[index].orig;
        else
          word = word_a[index].str;

        if (regexec(&re, word, (int)0, NULL, 0) == 0)
        {
          current = index;
          found   = 1;
          break;
        }
      }

      if (!found)
        current = first_selectable;
    }
  }
  else if (*pre_selection_index == '=') /* exact search */
  {
    /* A prefix is expected */
    /* """""""""""""""""""" */
    wchar_t * w;

    ll_t *      list;
    ll_node_t * node;

    list = tst_search(tst_word, w = utf8_strtowcs(pre_selection_index + 1));
    if (list != NULL)
    {
      node    = list->head;
      current = *(long *)(node->data);
    }
    else
      current = first_selectable;

    free(w);
  }
  else if (*pre_selection_index != '\0')
  {
    /* A prefix string or an index is expected */
    /* """"""""""""""""""""""""""""""""""""""" */
    int    len;
    char * ptr = pre_selection_index;

    if (*ptr == '#')
    {
      /* An index is expected */
      /* """""""""""""""""""" */
      ptr++;

      if (sscanf(ptr, "%ld%n", &current, &len) == 1 && len == (int)strlen(ptr))
      {
        /* We got an index (numeric value) */
        /* """"""""""""""""""""""""""""""" */
        if (current < 0)
          current = first_selectable;

        if (current >= count)
          current = count - 1;

        if (!word_a[current].is_selectable)
        {
          if (current > last_selectable)
            current = last_selectable;
          else if (current < first_selectable)
            current = first_selectable;
          else
            while (current > first_selectable && !word_a[current].is_selectable)
              current--;
        }
      }
      else if (*ptr == '\0' || strcmp(ptr, "last") == 0)
        /* We got a special index (empty or last) */
        /* """""""""""""""""""""""""""""""""""""" */
        current = last_selectable;
      else
      {
        fprintf(stderr, "Invalid index (%s)\n", ptr);

        exit(EXIT_FAILURE);
      }
    }
    else
    {
      /* A prefix is expected */
      /* """""""""""""""""""" */
      wchar_t * w;

      new_current = last_selectable;
      if (NULL
          != tst_prefix_search(tst_word, w = utf8_strtowcs(ptr), tst_cb_cli))
        current = new_current;
      else
        current = first_selectable;
      free(w);
    }
  }
  else
    current = first_selectable;

  /* We now need to adjust the 'start'/'end' fields of the structure 'win' */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  set_win_start_end(&win, current, last_line);

  /* We've finished reading from stdin                               */
  /* we will now get the inputs from the controlling terminal if any */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  errno = 0;
  if (freopen("/dev/tty", "r", stdin) == NULL)
    fprintf(stderr, "/dev/tty: %s\n", strerror(errno));

  old_fd1    = dup(1);
  old_stdout = fdopen(old_fd1, "w");
  setbuf(old_stdout, NULL);

  errno = 0;
  if (freopen("/dev/tty", "w", stdout) == NULL)
    fprintf(stderr, "/dev/tty: %s\n", strerror(errno));

  setvbuf(stdout, NULL, _IONBF, 0);

  /* Set the characteristics of the terminal */
  /* """"""""""""""""""""""""""""""""""""""" */
  setup_term(fileno(stdin));

  /* Initialize the search buffer with tab_real_max_size+1 NULs  */
  /* It will never be reallocated, only cleared.                 */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  search_data.buf = xcalloc(1, word_real_max_size + 1 - daccess.flength);

  /* Hide the cursor */
  /* """"""""""""""" */
  tputs(TPARM1(cursor_invisible), 1, outch);

  /* Force the display to start at a beginning of line */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);
  if (term.curs_column > 1)
    puts("");

  /* Display the words window and its title for the first time */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  disp_message(message_lines_list, message_max_width, message_max_len, &term,
               &win);

  /* Before displaying the word windows for the first time when in column */
  /* or line mode, we need to ensure that the word under the cursor will  */
  /* be visible by setting the number of the first column to be displayed */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode || win.line_mode)
  {
    long pos;
    long len;

    len = term.ncolumns - 3;

    /* Adjust win.first_column if the cursor is not visible */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    pos = first_word_in_line_a[line_nb_of_word_a[current]];

    while (word_a[current].end - word_a[pos].start >= len)
      pos++;

    win.first_column = word_a[pos].start;
  }

  /* Save the initial cursor line and column, here only the line is    */
  /* interesting us. This will tell us if we are in need to compensate */
  /* a terminal automatic scrolling                                    */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  nl = disp_lines(&win, &toggle, current, count, search_mode, &search_data,
                  &term, last_line, tmp_word, &langinfo);

  /* Determine the number of lines to move the cursor up if the window */
  /* display needed a terminal scrolling                               */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (nl + term.curs_line - 1 > term.nlines)
    offset = term.curs_line + nl - term.nlines;
  else
    offset = 0;

  /* Set the cursor to the first line of the window */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  {
    long i; /* generic index in this block */

    for (i = 1; i < offset; i++)
      tputs(TPARM1(cursor_up), 1, outch);
  }

  /* Save again the cursor current line and column positions so that we */
  /* will be able to put the terminal cursor back here                  */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  /* Arm the periodic timer */
  /* """""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = TICK;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = TICK;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);

  /* Main loop */
  /* """"""""" */
  while (1)
  {
    int sc = 0; /* scancode */

    /* If this alarm is triggered, then redisplay the window */
    /* to remove the help message and disable this timer.    */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_help_alrm)
    {
      got_help_alrm = 0;

      /* Calculate the new metadata and draw the window again */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(&term, count, &win);

      help_mode = 0;
      nl = disp_lines(&win, &toggle, current, count, search_mode, &search_data,
                      &term, last_line, tmp_word, &langinfo);
    }

    /* Reset the direct access selector if the direct access alarm rang */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_daccess_alrm)
    {
      got_daccess_alrm = 0;
      memset(daccess_stack, '\0', 6);
      daccess_stack_head = 0;

      daccess_timer = timers.direct_access;
    }

    if (got_winch)
    {
      got_winch   = 0;
      winch_timer = timers.winch; /* default 7 / FREQ s */
    }

    /* If the timeout is set then decrement its remaining value   */
    /* Upon expiration of this alarm, we trigger a content update */
    /* of the window                                              */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_winch_alrm)
    {
      long i; /* generic index in this block */
      int  nlines, ncolumns;
      int  line, column;

      /* Re-arm the timer */
      /* """""""""""""""" */
      winch_timer = timers.winch; /* default 7 / FREQ s */

      get_terminal_size(&nlines, &ncolumns);

      got_winch_alrm = 0;

      if (got_winch || term.nlines != nlines || term.ncolumns != ncolumns)
      {
        term.nlines   = nlines;
        term.ncolumns = ncolumns;
        continue;
      }

      winch_timer = -1;

      /* Reset the number of lines if the terminal has enough lines */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (term.nlines <= win.message_lines)
        win.max_lines = 0;
      else if (win.max_lines < term.nlines - win.message_lines)
      {
        if (win.asked_max_lines == 0)
          win.max_lines = term.nlines - win.message_lines;
        else
        {
          if (win.asked_max_lines > term.nlines - win.message_lines)
            win.max_lines = term.nlines - win.message_lines;
          else
            win.max_lines = win.asked_max_lines;
        }
      }
      else
        win.max_lines = term.nlines - win.message_lines;

      /* Erase the visible part of the displayed window */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      for (i = 0; i < win.message_lines; i++)
      {
        tputs(TPARM1(clr_bol), 1, outch);
        tputs(TPARM1(clr_eol), 1, outch);
        tputs(TPARM1(cursor_up), 1, outch);
      }

      tputs(TPARM1(clr_bol), 1, outch);
      tputs(TPARM1(clr_eol), 1, outch);
      tputs(TPARM1(save_cursor), 1, outch);

      get_cursor_position(&line, &column);

      for (i = 1; i < nl + win.message_lines; i++)
      {
        if (line + i >= nlines)
          break; /* We have reached the last terminal line */

        tputs(TPARM1(cursor_down), 1, outch);
        tputs(TPARM1(clr_bol), 1, outch);
        tputs(TPARM1(clr_eol), 1, outch);
      }
      tputs(TPARM1(restore_cursor), 1, outch);

      /* Get new cursor position */
      /* """"""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Calculate the new metadata and draw the window again */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(&term, count, &win);

      if (win.col_mode || win.line_mode)
      {
        long pos;
        long len;

        len = term.ncolumns - 3;

        /* Adjust win.first_column if the cursor is no more visible */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        pos = first_word_in_line_a[line_nb_of_word_a[current]];

        while (word_a[current].end - word_a[pos].start >= len)
          pos++;

        win.first_column = word_a[pos].start;
      }

      disp_message(message_lines_list, message_max_width, message_max_len,
                   &term, &win);

      nl = disp_lines(&win, &toggle, current, count, search_mode, &search_data,
                      &term, last_line, tmp_word, &langinfo);

      /* Determine the number of lines to move the cursor up if the window  */
      /* display needed a terminal scrolling                                */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (nl + win.message_lines + term.curs_line > term.nlines)
        offset = term.curs_line + nl + win.message_lines - term.nlines;
      else
        offset = 0;

      /* Set the cursor to the first line of the window */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      for (i = 1; i < offset; i++)
        tputs(TPARM1(cursor_up), 1, outch);

      /* Get new cursor position */
      /* """"""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Short-circuit the loop */
      /* """""""""""""""""""""" */
      continue;
    }

    /* and possibly set its reached value.                      */
    /* The counter is frozen in search and help mode            */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (timeout.initial_value && search_mode == NONE && !help_mode)
    {
      if (got_timeout_tick)
      {
        long   i;
        char * timeout_string;

        got_timeout_tick = 0;

        timeout.remain--;

        if (!quiet_timeout && timeout.remain % FREQ == 0)
        {
          sprintf(timeout_seconds, "%5u", timeout.remain / FREQ);
          timeout_string =
            (char *)(((ll_node_t *)(message_lines_list->tail))->data);
          memcpy(timeout_string + 1, timeout_seconds, 5);

          /* Erase the current window */
          /* """""""""""""""""""""""" */
          for (i = 0; i < win.message_lines; i++)
          {
            tputs(TPARM1(cursor_up), 1, outch);
            tputs(TPARM1(clr_bol), 1, outch);
            tputs(TPARM1(clr_eol), 1, outch);
          }

          tputs(TPARM1(clr_bol), 1, outch);
          tputs(TPARM1(clr_eol), 1, outch);

          /* Display the words window and its title for the first time */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          disp_message(message_lines_list, message_max_width, message_max_len,
                       &term, &win);
        }
        /* The timeout has expired */
        /* """"""""""""""""""""""" */
        if (timeout.remain == 0)
          timeout.reached = 1;
      }
    }

    if (timeout.reached)
    {
      if (timeout.mode == QUIT)
        goto quit;
      else if (timeout.mode == CURRENT || timeout.mode == WORD)
        goto enter;
    }

    /* Pressed keys scancodes processing */
    /* """"""""""""""""""""""""""""""""" */
    page = 1; /* Default number of lines to do down/up *
               * with PgDn/PgUp                        */

    sc = get_scancode(buffer, 15);

    if (sc)
    {
      if (timeout.initial_value && buffer[0] != 0x0d && buffer[0] != 'q'
          && buffer[0] != 'Q' && buffer[0] != 3)
      {
        long i;

        char * timeout_string;

        /* Reset the timeout to its initial value */
        /* """""""""""""""""""""""""""""""""""""" */
        timeout.remain = timeout.initial_value;

        if (!quiet_timeout)
        {
          sprintf(timeout_seconds, "%5u", timeout.initial_value / FREQ);
          timeout_string =
            (char *)(((ll_node_t *)(message_lines_list->tail))->data);
          memcpy(timeout_string + 1, timeout_seconds, 5);

          /* Clear the message */
          /* """"""""""""""""" */
          for (i = 0; i < win.message_lines; i++)
          {
            tputs(TPARM1(cursor_up), 1, outch);
            tputs(TPARM1(clr_bol), 1, outch);
            tputs(TPARM1(clr_eol), 1, outch);
          }

          tputs(TPARM1(clr_bol), 1, outch);
          tputs(TPARM1(clr_eol), 1, outch);

          /* Display the words window and its title for the first time */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          disp_message(message_lines_list, message_max_width, message_max_len,
                       &term, &win);
        }

        setitimer(ITIMER_REAL, &periodic_itv, NULL);
      }

      if (search_mode == NONE)
        if (help_mode && buffer[0] != '?')
        {
          got_help_alrm = 1;
          continue;
        }

      switch (buffer[0])
      {
        case 0x01: /* ^A */
          if (search_mode != NONE)
            goto khome;

          break;

        case 0x1a: /* ^Z */
          if (search_mode != NONE)
            goto kend;

          break;

        case 0x1b: /* ESC */
          /* An escape sequence or a UTF-8 sequence has been pressed */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp("\x1bOH", buffer, 3) == 0
              || memcmp("\x1bk", buffer, 2) == 0
              || memcmp("\x1b[H", buffer, 3) == 0
              || memcmp("\x1b[1~", buffer, 4) == 0
              || memcmp("\x1b[7~", buffer, 4) == 0)
          {
            /* HOME key has been pressed */
            /* """"""""""""""""""""""""" */
            if (search_mode != NONE)
            {
            khome:

              search_data.only_starting = 1;
              search_data.only_ending   = 0;
              select_starting_matches(&win, &term, &search_data, &last_line);
            }
            else
            {
              /* Find the first selectable word */
              /* """""""""""""""""""""""""""""" */
              current = win.start;

              while (current < win.end && !word_a[current].is_selectable)
                current++;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode || win.line_mode)
                if (word_a[current].end < win.first_column)
                  win.first_column = word_a[current].start;
            }

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
            break;
          }

          if (memcmp("\x1b[1;5H", buffer, 6) == 0
              || memcmp("\x1b[1;2H", buffer, 6) == 0
              || memcmp("\x1b[7^", buffer, 4) == 0)
            /* SHIFT/CTRL HOME key has been pressed */
            /* """""""""""""""""""""""""""""""""""" */
            goto kschome;

          if (memcmp("\x1bOF", buffer, 3) == 0
              || memcmp("\x1bj", buffer, 2) == 0
              || memcmp("\x1b[F", buffer, 3) == 0
              || memcmp("\x1b[4~", buffer, 4) == 0
              || memcmp("\x1b[8~", buffer, 4) == 0)
          {
            /* END key has been pressed */
            /* """""""""""""""""""""""" */
            if (search_mode != NONE)
            {
            kend:

              if (matches_count > 0 && search_mode != PREFIX)
              {
                search_data.only_starting = 0;
                search_data.only_ending   = 1;
                select_ending_matches(&win, &term, &search_data, &last_line);
              }
            }
            else
            {
              /* Find the last selectable word */
              /* """"""""""""""""""""""""""""" */
              current = win.end;

              while (current > win.start && !word_a[current].is_selectable)
                current--;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode || win.line_mode)
              {
                long pos;
                long len;

                len = term.ncolumns - 3;

                if (word_a[current].end >= len + win.first_column)
                {
                  /* Find the first word to be displayed in this line */
                  /* """""""""""""""""""""""""""""""""""""""""""""""" */
                  pos = first_word_in_line_a[line_nb_of_word_a[current]];

                  while (word_a[pos].start <= win.first_column)
                    pos++;

                  /* If the new current word cannot be displayed, search */
                  /* the first word in the line that can be displayed by */
                  /* iterating on pos.                                   */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                  pos--;

                  while (word_a[current].end - word_a[pos].start >= len)
                    pos++;

                  if (word_a[pos].start > 0)
                    win.first_column = word_a[pos].start;
                }
              }
            }

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
            break;
          }

          if (memcmp("\x1b[1;5F", buffer, 6) == 0
              || memcmp("\x1b[1;2F", buffer, 6) == 0
              || memcmp("\x1b[8^", buffer, 4) == 0)
            /* SHIFT/CTRL END key has been pressed */
            /* """"""""""""""""""""""""""""""""""" */
            goto kscend;

          if (memcmp("\x1bOD", buffer, 3) == 0
              || memcmp("\x1b[D", buffer, 3) == 0)
            /* Left arrow key has been pressed */
            /* """"""""""""""""""""""""""""""" */
            goto kl;

          if (memcmp("\x1bOC", buffer, 3) == 0
              || memcmp("\x1b[C", buffer, 3) == 0)
            /* Right arrow key has been pressed */
            /* """""""""""""""""""""""""""""""" */
            goto kr;

          if (memcmp("\x1bOA", buffer, 3) == 0
              || memcmp("\x1b[A", buffer, 3) == 0)
            /* Up arrow key has been pressed */
            /* """"""""""""""""""""""""""""" */
            goto ku;

          if (memcmp("\x1bOB", buffer, 3) == 0
              || memcmp("\x1b[B", buffer, 3) == 0)
            /* Down arrow key has been pressed */
            /* """"""""""""""""""""""""""""""" */
            goto kd;

          if (memcmp("\x1b[I", buffer, 3) == 0
              || memcmp("\x1b[5~", buffer, 4) == 0)
            /* PgUp key has been pressed */
            /* """"""""""""""""""""""""" */
            goto kpp;

          if (memcmp("\x1b[G", buffer, 3) == 0
              || memcmp("\x1b[6~", buffer, 4) == 0)
            /* PgDn key has been pressed */
            /* """"""""""""""""""""""""" */
            goto knp;

          if (memcmp("\x1b[L", buffer, 3) == 0
              || memcmp("\x1b[2~", buffer, 4) == 0)
            /* Ins key has been pressed */
            /* """"""""""""""""""""""""" */
            goto kins;

          if (memcmp("\x1b[3~", buffer, 4) == 0)
            /* Del key has been pressed */
            /* """"""""""""""""""""""""" */
            goto kdel;

          if (buffer[0] == 0x1b && buffer[1] == '\0')
          {
            /* ESC key has been pressed */
            /* """""""""""""""""""""""" */
            search_mode_t old_search_mode = search_mode;

            search_data.fuzzy_err     = 0;
            search_data.only_starting = 0;
            search_data.only_ending   = 0;

            if (help_mode)
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);

            /* Reset the direct access selector stack */
            /* """""""""""""""""""""""""""""""""""""" */
            memset(daccess_stack, '\0', 6);
            daccess_stack_head = 0;
            daccess_timer      = timers.direct_access;

            /* Clean the potential matching words non empty list */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            search_mode = NONE;

            if (matches_count > 0 || old_search_mode != search_mode)
            {
              clean_matches(&search_data, word_real_max_size);

              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }

            break;
          }

          /* Else ignore key */
          break;

        quit:
        case 'q':
        case 'Q':
        case 3: /* ^C */
          /* q or Q of ^C has been pressed */
          /* """"""""""""""""""""""""""""" */
          if (search_mode != NONE)
            goto special_cmds_when_searching;

          {
            long i; /* generic index in this block */

            for (i = 0; i < win.message_lines; i++)
              tputs(TPARM1(cursor_up), 1, outch);

            if (toggle.del_line)
            {
              tputs(TPARM1(clr_eol), 1, outch);
              tputs(TPARM1(clr_bol), 1, outch);
              tputs(TPARM1(save_cursor), 1, outch);

              for (i = 1; i < nl + win.message_lines; i++)
              {
                tputs(TPARM1(cursor_down), 1, outch);
                tputs(TPARM1(clr_eol), 1, outch);
                tputs(TPARM1(clr_bol), 1, outch);
              }
              tputs(TPARM1(restore_cursor), 1, outch);
            }
            else
            {
              for (i = 1; i < nl + win.message_lines; i++)
                tputs(TPARM1(cursor_down), 1, outch);
              puts("");
            }
          }

          tputs(TPARM1(cursor_normal), 1, outch);
          restore_term(fileno(stdin));

          exit(EXIT_SUCCESS);

        case 0x0c: /* ^L */
          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          nl = disp_lines(&win, &toggle, current, count, search_mode,
                          &search_data, &term, last_line, tmp_word, &langinfo);
          break;

        case 'n':
        case ' ':
          /* n or the sspace bar has been pressed */
          /* """""""""""""""""""""""""""""""""""" */
          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (matches_count > 0)
          {
            long pos = find_next_matching_word(matching_words_a, matches_count,
                                               current,
                                               &matching_word_cur_index);
            if (pos >= 0)
              current = pos;

            if (current < win.start || current > win.end)
              last_line = build_metadata(&term, count, &win);

            /* Set new first column to display */
            /* """"""""""""""""""""""""""""""" */
            set_new_first_column(&win, &term);

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
          }
          break;

        case 'N':
          /* N has been pressed */
          /* """""""""""""""""" */
          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (matches_count > 0)
          {
            long pos = find_prev_matching_word(matching_words_a, matches_count,
                                               current,
                                               &matching_word_cur_index);
            if (pos >= 0)
              current = pos;
          }

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* Set new first column to display */
          /* """"""""""""""""""""""""""""""" */
          set_new_first_column(&win, &term);

          nl = disp_lines(&win, &toggle, current, count, search_mode,
                          &search_data, &term, last_line, tmp_word, &langinfo);
          break;

        case 's':
          /* s has been pressed */
          /* """""""""""""""""" */
          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (matches_count > 0)
          {
            long pos;

            if (best_matches_count > 0)
              pos = find_next_matching_word(best_matching_words_a,
                                            best_matches_count, current,
                                            &matching_word_cur_index);
            else
              pos = find_next_matching_word(matching_words_a, matches_count,
                                            current, &matching_word_cur_index);

            if (pos >= 0)
              current = pos;

            if (current < win.start || current > win.end)
              last_line = build_metadata(&term, count, &win);

            /* Set new first column to display */
            /* """"""""""""""""""""""""""""""" */
            set_new_first_column(&win, &term);

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
          }
          break;

        case 'S':
          /* S has been pressed */
          /* """""""""""""""""" */
          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (matches_count > 0)
          {
            long pos;

            if (best_matches_count > 0)
              pos = find_prev_matching_word(best_matching_words_a,
                                            best_matches_count, current,
                                            &matching_word_cur_index);
            else
              pos = find_prev_matching_word(matching_words_a, matches_count,
                                            current, &matching_word_cur_index);

            if (pos >= 0)
              current = pos;
          }

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* Set new first column to display */
          /* """"""""""""""""""""""""""""""" */
          set_new_first_column(&win, &term);

          nl = disp_lines(&win, &toggle, current, count, search_mode,
                          &search_data, &term, last_line, tmp_word, &langinfo);
          break;

        enter:
        case 0x0d: /* CR */
        {
          /* <Enter> has been pressed */
          /* """""""""""""""""""""""" */

          int        extra_lines;
          char *     output_str;
          output_t * output_node;

          int width = 0;

          wchar_t * w;
          long      i; /* generic index in this block */

          if (help_mode)
            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);

          if (search_mode != NONE)
          {
            search_mode               = NONE;
            search_data.only_starting = 0;
            search_data.only_ending   = 0;

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);

            if (!toggle.enter_val_in_search)
              break;
          }

          /* Erase or jump after the window before printing the */
          /* selected string.                                   */
          /* """""""""""""""""""""""""""""""""""""""""""""""""" */
          if (toggle.del_line)
          {
            for (i = 0; i < win.message_lines; i++)
              tputs(TPARM1(cursor_up), 1, outch);

            tputs(TPARM1(clr_eol), 1, outch);
            tputs(TPARM1(clr_bol), 1, outch);
            tputs(TPARM1(save_cursor), 1, outch);

            for (i = 1; i < nl + win.message_lines; i++)
            {
              tputs(TPARM1(cursor_down), 1, outch);
              tputs(TPARM1(clr_eol), 1, outch);
              tputs(TPARM1(clr_bol), 1, outch);
            }

            tputs(TPARM1(restore_cursor), 1, outch);
          }
          else
          {
            for (i = 1; i < nl; i++)
              tputs(TPARM1(cursor_down), 1, outch);
            puts("");
          }

          /* When a timeout of type WORD is set, prints the specified word */
          /* else prints the current selected entries.                     */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (timeout.initial_value > 0 && timeout.remain == 0
              && timeout.mode == WORD)
            fprintf(old_stdout, "%s", timeout_word);
          else
          {
            char * num_str;
            char * str;

            if (toggle.taggable)
            {
              ll_t *      output_list = ll_new();
              ll_node_t * node;

              /* When using -P, updates the tagging order of this word to */
              /* make sure that the output will be correctly sorted.      */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (toggle.pinable)
                word_a[current].tag_order = next_tag_nb++;

              for (wi = 0; wi < count; wi++)
              {
                if (word_a[wi].is_tagged || wi == current)
                {
                  /* If the -p option is not used we do not take into */
                  /* account an untagged word under the cursor.       */
                  /* """""""""""""""""""""""""""""""""""""""""""""""" */
                  if (wi == current && tagged_words > 0 && !toggle.autotag
                      && !word_a[wi].is_tagged)
                    continue;

                  /* Chose the original string to print if the current one */
                  /* has been altered by a possible expansion.             */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
                  output_node = xmalloc(sizeof(output_t));

                  if (word_a[wi].orig != NULL)
                    str = word_a[wi].orig;
                  else
                    str = word_a[wi].str;

                  if (word_a[wi].is_numbered && daccess.num_sep)
                  {
                    num_str = xstrndup(str + 1, daccess.length);

                    ltrim(num_str, " ");
                    rtrim(num_str, " ", 0);

                    output_node->output_str = concat(num_str, daccess.num_sep,
                                                     str + daccess.flength,
                                                     NULL);

                    free(num_str);
                  }
                  else
                    output_node->output_str = xstrdup(str + daccess.flength);

                  output_node->order = word_a[wi].tag_order;

                  /* Trim the trailing spaces if -k is given in tabular or    */
                  /* column mode. Leading spaces are always preserved because */
                  /* I consider their presence intentional as the only way to */
                  /* have them is to use quotes in the command line.          */
                  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
                  if (!toggle.keep_spaces)
                  {
                    ltrim(output_node->output_str, " \t");
                    rtrim(output_node->output_str, " \t", 0);
                  }

                  ll_append(output_list, output_node);
                }
              }

              /* If nothing is selected, exist without printing anything */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (output_list->head == NULL)
                goto exit;

              /* If -P is in use, then sort the output list */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (toggle.pinable)
                ll_sort(output_list, tag_comp, tag_swap);

              /* And print them. */
              /* """"""""""""""" */
              node = output_list->head;
              while (node->next != NULL)
              {
                str = ((output_t *)(node->data))->output_str;

                fprintf(old_stdout, "%s", str);
                width += wcswidth((w = utf8_strtowcs(str)), 65535);
                free(w);
                free(str);
                free(node->data);

                if (win.sel_sep != NULL)
                {
                  fprintf(old_stdout, "%s", win.sel_sep);
                  width += wcswidth((w = utf8_strtowcs(win.sel_sep)), 65535);
                  free(w);
                }
                else
                {
                  fprintf(old_stdout, " ");
                  width++;
                }

                node = node->next;
              }

              str = ((output_t *)(node->data))->output_str;
              fprintf(old_stdout, "%s", str);
              width += wcswidth((w = utf8_strtowcs(str)), 65535);
              free(w);
              free(str);
              free(node->data);
            }
            else
            {
              /* Chose the original string to print if the current one has */
              /* been altered by a possible expansion.                     */
              /* Once this made, print it.                                 */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (word_a[current].orig != NULL)
                str = word_a[current].orig;
              else
                str = word_a[current].str;

              if (word_a[current].is_numbered && daccess.num_sep)
              {
                num_str = xstrndup(str + 1, daccess.length);

                ltrim(num_str, " ");
                rtrim(num_str, " ", 0);

                output_str = concat(num_str, daccess.num_sep,
                                    str + daccess.flength, NULL);

                free(num_str);
              }
              else
                output_str = str + daccess.flength;

              /* Trim the trailing spaces if -k is given in tabular or       */
              /* column mode. Leading spaces are always preserved because I  */
              /* consider their presence intentional as the only way to have */
              /* them is to use quotes in the command line.                  */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (!toggle.keep_spaces)
              {
                ltrim(output_str, " \t");
                rtrim(output_str, " \t", 0);
              }

              width = wcswidth((w = utf8_strtowcs(output_str)), 65535);
              free(w);

              /* And print it. */
              /* """"""""""""" */
              fprintf(old_stdout, "%s", output_str);
            }

            /* If the output stream is a terminal */
            /* """""""""""""""""""""""""""""""""" */
            if (isatty(old_fd1))
            {
              /* width is (in term of terminal columns) the size of the    */
              /* string to be displayed.                                   */
              /*                                                           */
              /* With that information, count the number of terminal lines */
              /* printed.                                                  */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              extra_lines = width / term.ncolumns;

              /* Clean the printed line and all the extra lines used. */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
              tputs(TPARM1(delete_line), 1, outch);

              for (i = 0; i < extra_lines; i++)
              {
                tputs(TPARM1(cursor_up), 1, outch);
                tputs(TPARM1(clr_eol), 1, outch);
                tputs(TPARM1(clr_bol), 1, outch);
              }
            }
          }

        exit:

          /* Restore the visibility of the cursor */
          /* """""""""""""""""""""""""""""""""""" */
          tputs(TPARM1(cursor_normal), 1, outch);

          /* Set the cursor at the start on the line an restore the */
          /* original terminal state before exiting                 */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          tputs(TPARM1(carriage_return), 1, outch);
          restore_term(fileno(stdin));

          exit(EXIT_SUCCESS);
        }

        kl:
          /* Cursor Left key has been pressed */
          /* """""""""""""""""""""""""""""""" */
          if (search_mode != NONE)
            search_mode = NONE;

        case 'H':
        case 'h':
          if (search_mode == NONE)
          {
            long old_current      = current;
            long old_start        = win.start;
            long old_first_column = win.first_column;

            do
            {
              if (current > 0)
              {
                if (current == win.start)
                  if (win.start > 0)
                  {
                    for (wi = win.start - 1; wi >= 0 && word_a[wi].start != 0;
                         wi--)
                    {
                    }
                    win.start = wi;

                    if (word_a[wi].str != NULL)
                      win.start = wi;

                    if (win.end < count - 1)
                    {
                      for (wi = win.end + 2;
                           wi < count - 1 && word_a[wi].start != 0; wi++)
                      {
                      }
                      if (word_a[wi].str != NULL)
                        win.end = wi;
                    }
                  }

                /* In column mode we need to take care of the */
                /* horizontal scrolling                       */
                /* """""""""""""""""""""""""""""""""""""""""" */
                if (win.col_mode || win.line_mode)
                {
                  long pos;

                  if (word_a[current].start == 0)
                  {
                    long len;

                    len = term.ncolumns - 3;
                    pos = first_word_in_line_a[line_nb_of_word_a[current - 1]];

                    while (word_a[current - 1].end - win.first_column >= len)
                    {
                      win.first_column += word_a[pos].end - word_a[pos].start
                                          + 2;

                      pos++;
                    }
                  }
                  else if (word_a[current - 1].start < win.first_column)
                    win.first_column = word_a[current - 1].start;
                }
                current--;
              }
              else
                break;
            } while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current   = old_current;
              win.start = old_start;
              if (win.col_mode || win.line_mode)
                win.first_column = old_first_column;
            }

            if (current != old_current)
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        kr:
          /* Right key has been pressed */
          /* """""""""""""""""""""""""" */
          if (search_mode != NONE)
            search_mode = NONE;

        case 'L':
        case 'l':
          if (search_mode == NONE)
          {
            long old_current      = current;
            long old_start        = win.start;
            long old_first_column = win.first_column;

            do
            {
              if (current < count - 1)
              {
                if (current == win.end)
                  if (win.start < count - 1 && win.end != count - 1)
                  {
                    for (wi = win.start + 1;
                         wi < count - 1 && word_a[wi].start != 0; wi++)
                    {
                    }

                    if (word_a[wi].str != NULL)
                      win.start = wi;

                    if (win.end < count - 1)
                    {
                      for (wi = win.end + 2;
                           wi < count - 1 && word_a[wi].start != 0; wi++)
                      {
                      }
                      if (word_a[wi].str != NULL)
                        win.end = wi;
                    }
                  }

                /* In column mode we need to take care of the */
                /* horizontal scrolling                       */
                /* """""""""""""""""""""""""""""""""""""""""" */
                if (win.col_mode || win.line_mode)
                {
                  if (word_a[current].is_last)
                    win.first_column = 0;
                  else
                  {
                    long pos;
                    long len;

                    len = term.ncolumns - 3;

                    if (word_a[current + 1].end >= len + win.first_column)
                    {
                      /* Find the first word to be displayed in this line */
                      /* """""""""""""""""""""""""""""""""""""""""""""""" */
                      pos = first_word_in_line_a[line_nb_of_word_a[current]];

                      while (word_a[pos].start <= win.first_column)
                        pos++;

                      pos--;

                      /* If the new current word cannot be displayed, search */
                      /* the first word in the line that can be displayed by */
                      /* iterating on pos.                                   */
                      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                      while (word_a[current + 1].end - word_a[pos].start >= len)
                        pos++;

                      if (word_a[pos].start > 0)
                        win.first_column = word_a[pos].start;
                    }
                  }
                }
                current++;
              }
              else
                break;
            } while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current   = old_current;
              win.start = old_start;
              if (win.col_mode || win.line_mode)
                win.first_column = old_first_column;
            }

            if (current != old_current)
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        case 0x0b:
          /* ^K key has been pressed */
          /* """"""""""""""""""""""" */
          goto kschome;

        case 'K':
          if (search_mode != NONE)
            goto special_cmds_when_searching;

        kpp:
          /* PgUp key has been pressed */
          /* """"""""""""""""""""""""" */
          page = win.max_lines;

        ku:
          /* Cursor Up key has been pressed */
          /* """""""""""""""""""""""""""""" */
          if (search_mode != NONE)
            search_mode = NONE;

        case 'k':
          if (search_mode == NONE)
          {
            long cur_line;
            long start_line;
            long last_word;
            long cursor;
            long old_current = current;
            long old_start   = win.start;
            long index;

            /* Store the initial starting and ending positions of */
            /* the word under the cursor                          */
            /* """""""""""""""""""""""""""""""""""""""""""""""""" */
            s = word_a[current].start;
            e = word_a[current].end;

            do
            {
              /* Identify the line number of the first window's line */
              /* and the line number of the current line             */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
              start_line = line_nb_of_word_a[win.start];
              cur_line   = line_nb_of_word_a[current];

              if (cur_line == 0)
                break;

              /* Manage the different cases */
              /* """""""""""""""""""""""""" */
              if (start_line >= page)
              {
                if (start_line > cur_line - page)
                  start_line -= page;
              }
              else
                start_line = 0;

              /* Get the index of the last word of the destination line */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (cur_line >= page)
                last_word = first_word_in_line_a[cur_line - page + 1] - 1;
              else
                last_word = first_word_in_line_a[1] - 1;

              /* And set the new value of the starting word of the window */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              win.start = first_word_in_line_a[start_line];

              /* Look for the first word whose start position in the line is */
              /* less or equal to the source word starting position          */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              cursor = last_word;
              while (word_a[cursor].start > s)
                cursor--;

              /* In case no word is eligible, keep the cursor on */
              /* the last word                                   */
              /* """"""""""""""""""""""""""""""""""""""""""""""" */
              if (cursor == last_word && word_a[cursor].start > 0)
                cursor--;

              /* Try to guess the best choice if we have multiple choices */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (word_a[cursor].end >= s
                  && word_a[cursor].end - s >= e - word_a[cursor + 1].start)
                current = cursor;
              else
              {
                if (cursor < last_word)
                  current = cursor + 1;
                else
                  current = cursor;
              }

              /* Set new first column to display */
              /* """"""""""""""""""""""""""""""" */
              set_new_first_column(&win, &term);

              /* If the word is not selectable, try to find a selectable word */
              /* in ts line                                                   */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (!word_a[current].is_selectable)
              {
                index = 0;
                while (word_a[current - index].start > 0
                       && !word_a[current - index].is_selectable)
                  index++;

                if (word_a[current - index].is_selectable)
                  current -= index;
                else
                {
                  index = 0;
                  while (current + index < last_word
                         && !word_a[current + index].is_selectable)
                    index++;

                  if (word_a[current + index].is_selectable)
                    current += index;
                }
              }
            } while (current != old_current && !word_a[current].is_selectable);

            /* If no selectable word could be find; stay at the original */
            /* position                                                  */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (!word_a[current].is_selectable)
            {
              current   = old_current;
              win.start = old_start;
            }

            /* Display the window */
            /* """""""""""""""""" */
            if (current != old_current)
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            else
            {
              /* We couldn't move to a selectable word, */
              /* try to move the window offset instead  */
              /* """""""""""""""""""""""""""""""""""""" */
              if (line_nb_of_word_a[old_start] > 0
                  && win.cur_line < win.max_lines && page == 1)
              {
                win.start =
                  first_word_in_line_a[line_nb_of_word_a[old_start] - 1];

                nl = disp_lines(&win, &toggle, current, count, search_mode,
                                &search_data, &term, last_line, tmp_word,
                                &langinfo);
              }
            }
          }
          else
            goto special_cmds_when_searching;

          break;

        kschome:
          /* Go to the first selectable word. */
          /* """""""""""""""""""""""""""""""" */
          current = 0;

          if (search_mode != NONE)
            search_mode = NONE;

          /* Find the first selectable word */
          /* """""""""""""""""""""""""""""" */
          while (!word_a[current].is_selectable)
            current++;

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* In column mode we need to take care of the */
          /* horizontal scrolling                       */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (win.col_mode || win.line_mode)
          {
            long pos;

            /* Adjust win.first_column if the cursor is no more visible */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            pos = first_word_in_line_a[line_nb_of_word_a[current]];

            while (word_a[current].end - word_a[pos].start >= term.ncolumns - 3)
              pos++;

            win.first_column = word_a[pos].start;
          }

          nl = disp_lines(&win, &toggle, current, count, search_mode,
                          &search_data, &term, last_line, tmp_word, &langinfo);
          break;

        case 0x0a:
          /* ^J key has been pressed */
          /* """"""""""""""""""""""" */
          goto kscend;

        case 'J':
          if (search_mode != NONE)
            goto special_cmds_when_searching;

        knp:
          /* PgDn key has been pressed */
          /* """"""""""""""""""""""""" */
          page = win.max_lines;

        kd:
          /* Cursor Down key has been pressed */
          /* """""""""""""""""""""""""""""""" */
          if (search_mode != NONE)
            search_mode = NONE;

        case 'j':
          if (search_mode == NONE)
          {
            long cur_line;
            long start_line;
            long last_word;
            long cursor;
            long old_current = current;
            long old_start   = win.start;
            long index;

            /* Store the initial starting and ending positions of */
            /* the word under the cursor                          */
            /* """""""""""""""""""""""""""""""""""""""""""""""""" */
            s = word_a[current].start;
            e = word_a[current].end;

            do
            {
              /* Identify the line number of the first window's line */
              /* and the line number of the current line             */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
              start_line = line_nb_of_word_a[win.start];
              cur_line   = line_nb_of_word_a[current];

              /* Do nothing when we are already on the last line */
              /* """"""""""""""""""""""""""""""""""""""""""""""" */
              if (cur_line == last_line)
                break;

              /* Determine and set the future start of the window */
              /* """""""""""""""""""""""""""""""""""""""""""""""" */
              if (start_line > 0 || last_line >= page)
                if (cur_line + page > start_line + win.max_lines - 1)
                {
                  if (last_line - (cur_line + page) < page)
                  {
                    start_line = last_line - win.max_lines + 1;
                    win.start  = first_word_in_line_a[start_line];
                  }
                  else
                  {
                    if (win.end < count - 1)
                    {
                      start_line += page;
                      win.start = first_word_in_line_a[start_line];
                    }
                  }
                }

              /* Calculate the index of the last word of the target line */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (cur_line + 1 == last_line)
                last_word = count - 1;
              else
              {
                if (cur_line + page < last_line)
                  last_word = first_word_in_line_a[cur_line + page + 1] - 1;
                else
                  last_word = count - 1;
              }

              /* Look for the first word whose start position in the line is */
              /* less or equal than the source word starting position        */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              cursor = last_word;
              while (word_a[cursor].start > s)
                cursor--;

              /* In case no word is eligible, keep the cursor on */
              /* the last word                                   */
              /* """"""""""""""""""""""""""""""""""""""""""""""" */
              if (cursor == last_word && word_a[cursor].start > 0)
                cursor--;

              /* Try to guess the best choice if we have multiple choices */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (cursor < count - 1
                  && word_a[cursor].end - s >= e - word_a[cursor + 1].start)
                current = cursor;
              else
              {
                if (cursor < count - 1)
                {
                  if (cursor < last_word)
                    current = cursor + 1;
                  else
                    current = cursor;
                }
                else
                  current = count - 1;
              }

              /* Set the new first column to display */
              /* """"""""""""""""""""""""""""""""""" */
              set_new_first_column(&win, &term);

              /* If the word is not selectable, try to find a selectable word */
              /* in ts line                                                   */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (!word_a[current].is_selectable)
              {
                index = 0;
                while (word_a[current - index].start > 0
                       && !word_a[current - index].is_selectable)
                  index++;

                if (word_a[current - index].is_selectable)
                  current -= index;
                else
                {
                  index = 0;
                  while (current + index < last_word
                         && !word_a[current + index].is_selectable)
                    index++;

                  if (word_a[current + index].is_selectable)
                    current += index;
                }
              }
            } while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current   = old_current;
              win.start = old_start;
            }

            /* Display the window */
            /* """""""""""""""""" */
            if (current != old_current)
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            else
            {
              /* We couldn't move to a selectable word, */
              /* try to move the window offset instead  */
              /* """""""""""""""""""""""""""""""""""""" */
              if (win.cur_line > 1 && win.end < count - 1 && page == 1)
              {
                win.start =
                  first_word_in_line_a[line_nb_of_word_a[old_start] + 1];

                nl = disp_lines(&win, &toggle, current, count, search_mode,
                                &search_data, &term, last_line, tmp_word,
                                &langinfo);
              }
            }
          }
          else
            goto special_cmds_when_searching;

          break;

        kscend:
          /* Go to the last selectable word. */
          /* """"""""""""""""""""""""""""""" */
          current = count - 1;

          if (search_mode != NONE)
            search_mode = NONE;

          /* Find the first selectable word */
          /* """""""""""""""""""""""""""""" */
          while (!word_a[current].is_selectable)
            current--;

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* In column mode we need to take care of the */
          /* horizontal scrolling                       */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (win.col_mode || win.line_mode)
          {
            long pos;

            /* Adjust win.first_column if the cursor is no more visible */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            pos = first_word_in_line_a[line_nb_of_word_a[current]];

            while (word_a[current].end - word_a[pos].start >= term.ncolumns - 3)
              pos++;

            win.first_column = word_a[pos].start;
          }

          nl = disp_lines(&win, &toggle, current, count, search_mode,
                          &search_data, &term, last_line, tmp_word, &langinfo);
          break;

        case '/':
          /* generic search method according the misc settings */
          /* """"""""""""""""""""""""""""""""""""""""""""""""" */
          if (misc.default_search_method == PREFIX)
            goto prefix_method;
          else if (misc.default_search_method == SUBSTRING)
            goto substring_method;
          else if (misc.default_search_method == FUZZY)
            goto fuzzy_method;

          break;

        case '~':
        case '*':
        /* ~ or * key has been pressed       */
        /* (start of a fuzzy search session) */
        /* """"""""""""""""""""""""""""""""" */
        fuzzy_method:

          if (search_mode == NONE)
          {
            search_mode = FUZZY;

            if (old_search_mode != FUZZY)
            {
              old_search_mode = FUZZY;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        case '\'':
        case '\"':
        /* ' or " key has been pressed           */
        /* (start of a substring search session) */
        /* """"""""""""""""""""""""""""""""""""" */
        substring_method:

          if (search_mode == NONE)
          {
            search_mode = SUBSTRING;

            if (old_search_mode != SUBSTRING)
            {
              old_search_mode = SUBSTRING;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        case '=':
        case '^':
        /* ^ or = key has been pressed        */
        /* (start of a prefix search session) */
        /* """""""""""""""""""""""""""""""""" */
        prefix_method:

          if (search_mode == NONE)
          {
            search_mode = PREFIX;

            if (old_search_mode != PREFIX)
            {
              old_search_mode = PREFIX;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win, &toggle, current, count, search_mode,
                            &search_data, &term, last_line, tmp_word,
                            &langinfo);
            break;
          }
          else
            goto special_cmds_when_searching;

          break;

        kins:
          if (toggle.taggable)
          {
            if (word_a[current].is_tagged == 0)
            {
              tagged_words++;
              word_a[current].is_tagged = 1;

              if (toggle.pinable)
                word_a[current].tag_order = next_tag_nb++;

              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
          }
          break;

        kdel:
          if (toggle.taggable)
          {
            if (word_a[current].is_tagged == 1)
            {
              word_a[current].is_tagged = 0;
              tagged_words--;

              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
          }
          break;

        case 't':
          if (search_mode == NONE)
          {
            if (toggle.taggable)
            {
              if (word_a[current].is_tagged)
              {
                word_a[current].is_tagged = 0;
                tagged_words--;
              }
              else
              {
                word_a[current].is_tagged = 1;
                tagged_words++;

                if (toggle.pinable)
                  word_a[current].tag_order = next_tag_nb++;
              }
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'T':
          if (search_mode == NONE)
          {
            if (toggle.taggable)
            {
              long i, n;

              for (i = 0; i < matches_count; i++)
              {
                n = matching_words_a[i];

                if (!word_a[n].is_tagged)
                {
                  word_a[n].is_tagged = 1;
                  tagged_words++;

                  if (toggle.pinable)
                    word_a[n].tag_order = next_tag_nb++;
                }
              }
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'U':
          if (search_mode == NONE)
          {
            if (toggle.taggable)
            {
              long i, n;

              for (i = 0; i < matches_count; i++)
              {
                n = matching_words_a[i];

                if (word_a[n].is_tagged)
                {
                  word_a[n].is_tagged = 0;
                  tagged_words--;
                }
              }
              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
          if (search_mode == NONE && daccess.mode != DA_TYPE_NONE)
          {
            wchar_t * w;
            long *    pos;

            /* Set prev_current to the initial current word to be    */
            /* able to return here if the first direct access fails. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (daccess_stack_head == 0)
              prev_current = current;

            if (daccess_stack_head == daccess.length)
              break;

            daccess_stack[daccess_stack_head] = buffer[0];
            daccess_stack_head++;
            w   = utf8_strtowcs(daccess_stack);
            pos = tst_search(tst_daccess, w);
            free(w);

            if (pos != NULL)
            {
              current = *pos;

              if (current < win.start || current > win.end)
                last_line = build_metadata(&term, count, &win);

              /* Set new first column to display */
              /* """"""""""""""""""""""""""""""" */
              set_new_first_column(&win, &term);

              nl = disp_lines(&win, &toggle, current, count, search_mode,
                              &search_data, &term, last_line, tmp_word,
                              &langinfo);
            }
            else
            {
              if (current != prev_current)
              {
                current = prev_current;

                if (current < win.start || current > win.end)
                  last_line = build_metadata(&term, count, &win);

                /* Set new first column to display */
                /* """"""""""""""""""""""""""""""" */
                set_new_first_column(&win, &term);

                nl = disp_lines(&win, &toggle, current, count, search_mode,
                                &search_data, &term, last_line, tmp_word,
                                &langinfo);
              }
            }

            daccess_timer = timers.direct_access;
          }
          else
            goto special_cmds_when_searching;
        }
        break;

        case 0x08: /* ^H */
        case 0x7f: /* BS */
        {
          long i;

          /* Backspace or CTRL-H */
          /* """"""""""""""""""" */
          if (daccess_stack_head > 0)
            daccess_stack[--daccess_stack_head] = '\0';

          if (search_mode != NONE)
          {
            if (search_data.utf8_len > 0)
            {
              char * prev;

              prev = utf8_prev(search_data.buf,
                               search_data.buf + search_data.len - 1);

              if (search_data.utf8_len == search_data.fuzzy_err_pos - 1)
              {
                search_data.fuzzy_err     = 0;
                search_data.fuzzy_err_pos = -1;
              }
              search_data.utf8_len--;

              if (prev)
              {
                *(utf8_next(prev)) = '\0';
                search_data.len    = prev - search_data.buf + 1;
              }
              else
              {
                *search_data.buf = '\0';
                search_data.len  = 0;

                for (i = 0; i < matches_count; i++)
                {
                  long n = matching_words_a[i];

                  word_a[n].is_matching = 0;

                  memset(word_a[n].bitmap, '\0',
                         (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
                }

                matches_count = 0;

                nl = disp_lines(&win, &toggle, current, count, search_mode,
                                &search_data, &term, last_line, tmp_word,
                                &langinfo);
              }
            }
            else
              beep(&toggle);

            if (search_data.utf8_len > 0)
              goto special_cmds_when_searching;
            else
              /* When there is only one glyph in the search list in       */
              /* FUZZY and SUBSTRING mode then all is already done except */
              /* the cleanup of the first level of the tst_search_list.   */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (search_mode != PREFIX)
            {
              sub_tst_t * sub_tst_data;
              ll_node_t * node;

              node         = tst_search_list->tail;
              sub_tst_data = (sub_tst_t *)(node->data);

              search_data.fuzzy_err = 0;

              sub_tst_data->count = 0;
            }
          }
        }

        break;

        case '?':
          /* Help mode */
          /* """"""""" */
          if (search_mode == NONE)
          {
            help(&win, &term, last_line, &toggle);
            help_mode = 1;

            /* Arm the help timer */
            /* """""""""""""""""" */
            help_timer = timers.help; /* default 15 s */
          }
          else
            goto special_cmds_when_searching;
          break;

        special_cmds_when_searching:
        default:
        {
          int                c; /* byte index in the scancode string */
          sub_tst_t *        sub_tst_data;
          unsigned long long index;
          long               i;

          if (search_mode != NONE)
          {
            long        old_len      = search_data.len;
            long        old_utf8_len = search_data.utf8_len;
            ll_node_t * node;
            wchar_t *   ws;

            /* Copy all the bytes included in the key press to buffer */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (buffer[0] != 0x08 && buffer[0] != 0x7f) /* Backspace */
            {
              /* The only case where we have to manage backspace hits */
              /* here is if the user has entered them in fuzzy search */
              /* mode. As the search buffer has already been amended, */
              /* we do not have to update the search buffer again.    */
              /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
              for (c = 0; c < sc
                          && search_data.utf8_len
                               < word_real_max_size - daccess.flength;
                   c++)
                search_data.buf[search_data.len++] = buffer[c];

              /* Update the glyph array with the content of the search buffer */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (search_data.utf8_len < word_real_max_size - daccess.flength)
              {
                search_data.utf8_off_a[search_data.utf8_len] = old_len;
                search_data.utf8_len_a[search_data.utf8_len] = search_data.len
                                                               - old_len;
                search_data.utf8_len++;
              }
            }

            if (search_mode == PREFIX)
            {
              ws = utf8_strtowcs(search_data.buf);

              /* Purge the matching words list */
              /* """"""""""""""""""""""""""""" */
              for (i = 0; i < matches_count; i++)
              {
                long n = matching_words_a[i];

                word_a[n].is_matching = 0;

                memset(word_a[n].bitmap, '\0',
                       (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
              }

              matches_count = 0;

              tst_prefix_search(tst_word, ws, tst_cb);

              /* matches_count is updated by tst_cb */
              /* """"""""""""""""""""""""""""""""""""" */
              if (matches_count > 0)
              {
                if (search_data.len == old_len && matches_count == 1
                    && buffer[0] != 0x08 && buffer[0] != 0x7f)
                  beep(&toggle);
                else
                {
                  /* Adjust the bitmap to the ending version */
                  /* """"""""""""""""""""""""""""""""""""""" */
                  update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                  current = matching_words_a[0];

                  if (current < win.start || current > win.end)
                    last_line = build_metadata(&term, count, &win);

                  /* Set new first column to display */
                  /* """"""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win, &toggle, current, count, search_mode,
                                  &search_data, &term, last_line, tmp_word,
                                  &langinfo);
                }
              }
              else
              {
                beep(&toggle);

                search_data.len                  = old_len;
                search_data.utf8_len             = old_utf8_len;
                search_data.buf[search_data.len] = '\0';
              }
            }
            else if (search_mode == FUZZY)
            {
              /* tst_search_list: [sub_tst_t *] -> [sub_tst_t *]...     */
              /*                    ^               ^                   */
              /*                    |               |                   */
              /*                  level 1         level_2               */
              /*                                                        */
              /* Each sub_tst_t * points to a data structure including  */
              /* a sorted array to starting nodes in the words tst.     */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
              wchar_t * w = utf8_strtowcs(search_data.buf + old_len);

              /* zero previous matching indicators */
              /* """"""""""""""""""""""""""""""""" */
              for (i = 0; i < matches_count; i++)
              {
                long n = matching_words_a[i];

                word_a[n].is_matching = 0;

                memset(word_a[n].bitmap, '\0',
                       (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
              }

              matches_count = 0;

              if (buffer[0] == 0x08 || buffer[0] == 0x7f) /* Backspace */
              {
                node         = tst_search_list->tail;
                sub_tst_data = (sub_tst_t *)(node->data);

                sub_tst_data->count = 0;

                if (tst_search_list->len > 0)
                {
                  free(sub_tst_data->array);
                  free(sub_tst_data);

                  ll_delete(tst_search_list, tst_search_list->tail);
                }

                if (search_data.utf8_len == search_data.fuzzy_err_pos - 1)
                {
                  search_data.fuzzy_err     = 0;
                  search_data.fuzzy_err_pos = -1;
                }
              }
              else
              {
                if (search_data.utf8_len == 1)
                {
                  /* Search all the sub-tst trees having the searched        */
                  /* character as children, the resulting sub-tst are put    */
                  /* in the sub tst array attached to the currently searched */
                  /* symbol.                                                 */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                  tst_fuzzy_traverse(tst_word, NULL, 0, w[0]);

                  node         = tst_search_list->tail;
                  sub_tst_data = (sub_tst_t *)(node->data);
                  if (sub_tst_data->count == 0)
                  {
                    beep(&toggle);

                    search_data.len      = 0;
                    search_data.utf8_len = 0;
                    search_data.buf[0]   = '\0';

                    break;
                  }
                }
                else
                {
                  /* Prevent the list to grow larger than the maximal */
                  /* word's length.                                   */
                  /* """""""""""""""""""""""""""""""""""""""""""""""" */
                  if (tst_search_list->len
                      < word_real_max_size - daccess.flength)
                  {
                    /* use the results in the level n-1 list to build the */
                    /* level n list                                       */
                    /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                    int rc;

                    sub_tst_t * tst_fuzzy_level_data;

                    tst_fuzzy_level_data = sub_tst_new();

                    ll_append(tst_search_list, tst_fuzzy_level_data);

                    node         = tst_search_list->tail->prev;
                    sub_tst_data = (sub_tst_t *)(node->data);

                    rc = 0;
                    for (index = 0; index < sub_tst_data->count; index++)
                      rc += tst_fuzzy_traverse(sub_tst_data->array[index], NULL,
                                               1, w[0]);

                    if (rc == 0)
                    {
                      free(tst_fuzzy_level_data->array);
                      free(tst_fuzzy_level_data);

                      ll_delete(tst_search_list, tst_search_list->tail);

                      search_data.fuzzy_err = 1;
                      if (search_data.fuzzy_err_pos == -1)
                        search_data.fuzzy_err_pos = search_data.utf8_len;

                      beep(&toggle);

                      search_data.len                  = old_len;
                      search_data.utf8_len             = old_utf8_len;
                      search_data.buf[search_data.len] = '\0';
                    }
                  }
                  else
                    beep(&toggle);
                }
              }
              free(w);

              /* Process this level to mark the word found as a matching word */
              /* if any.                                                      */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              node         = tst_search_list->tail;
              sub_tst_data = (sub_tst_t *)(node->data);

              for (index = 0; index < sub_tst_data->count; index++)
                tst_traverse(sub_tst_data->array[index], set_matching_flag, 0);

              /* Update the bitmap and re-display the window */
              /* """"""""""""""""""""""""""""""""""""""""""" */
              if (matches_count > 0)
              {
                if (search_data.only_starting)
                  select_starting_matches(&win, &term, &search_data,
                                          &last_line);
                else if (search_data.only_ending)
                  select_ending_matches(&win, &term, &search_data, &last_line);
                else
                  /* Adjust the bitmap to the ending version */
                  /* """"""""""""""""""""""""""""""""""""""" */
                  update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                current = matching_words_a[0];

                if (current < win.start || current > win.end)
                  last_line = build_metadata(&term, count, &win);

                /* Set new first column to display */
                /* """"""""""""""""""""""""""""""" */
                set_new_first_column(&win, &term);

                nl = disp_lines(&win, &toggle, current, count, search_mode,
                                &search_data, &term, last_line, tmp_word,
                                &langinfo);
              }
              else
                beep(&toggle);
            }
            else /* SUBSTRING */
            {
              wchar_t * w = utf8_strtowcs(search_data.buf);

              /* Purge the matching words list */
              /* """"""""""""""""""""""""""""" */
              for (i = 0; i < matches_count; i++)
              {
                long n = matching_words_a[i];

                word_a[n].is_matching = 0;

                memset(word_a[n].bitmap, '\0',
                       (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
              }

              matches_count = 0;

              if (search_data.utf8_len == 1)
              {
                /* Search all the sub-tst trees having the searched     */
                /* character as children, the resulting sub-tst are put */
                /* in the level list corresponding to the letter order. */
                /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
                tst_substring_traverse(tst_word, NULL, 0, w[0]);

                node         = tst_search_list->tail;
                sub_tst_data = (sub_tst_t *)(node->data);

                for (index = 0; index < sub_tst_data->count; index++)
                  tst_traverse(sub_tst_data->array[index], set_matching_flag,
                               0);
              }
              else
              {
                /* Search for the rest of the word in all the sub-tst */
                /* trees previously found.                            */
                /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                node         = tst_search_list->tail;
                sub_tst_data = (sub_tst_t *)(node->data);

                matches_count = 0;

                for (index = 0; index < sub_tst_data->count; index++)
                  tst_prefix_search(sub_tst_data->array[index], w + 1, tst_cb);
              }

              if (matches_count > 0)
              {
                if (search_data.len == old_len && matches_count == 1
                    && buffer[0] != 0x08 && buffer[0] != 0x7f)
                  beep(&toggle);
                else
                {
                  if (search_data.only_starting)
                    select_starting_matches(&win, &term, &search_data,
                                            &last_line);
                  else if (search_data.only_ending)
                    select_ending_matches(&win, &term, &search_data,
                                          &last_line);
                  else
                    update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                  current = matching_words_a[0];

                  if (current < win.start || current > win.end)
                    last_line = build_metadata(&term, count, &win);

                  /* Set new first column to display */
                  /* """"""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win, &toggle, current, count, search_mode,
                                  &search_data, &term, last_line, tmp_word,
                                  &langinfo);
                }
              }
              else
              {
                beep(&toggle);

                search_data.len = old_len;
                search_data.utf8_len--;
                search_data.buf[search_data.len] = '\0';
              }
            }
          }
        }
      }
    }
  }
}
