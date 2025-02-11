/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
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
#if (defined(__sun) && defined(__SVR4)) || defined(_AIX)
#include <curses.h>
#endif
#include <term.h>
#include <termios.h>
#include <regex.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <wchar.h>

#include "xmalloc.h"
#include "list.h"
#include "index.h"
#include "utf8.h"
#include "fgetc.h"
#include "utils.h"
#include "ctxopt.h"
#include "usage.h"
#include "safe.h"
#define BUF_MALLOC xmalloc
#define BUF_REALLOC xrealloc
#include "tinybuf.h"
#include "smenu.h"

/* ***************** */
/* Extern variables. */
/* ***************** */

extern ll_t *tst_search_list;

/* ***************** */
/* Global variables. */
/* ***************** */

word_t *word_a;       /* array containing words data (size: count).        */
long    count = 0;    /* number of words read from stdin.                  */
long    current;      /* index the current selection under the cursor).    */
long    new_current;  /* final cur. position, (used in search function).   */
long    prev_current; /* prev. position stored when using direct access.   */

long *line_nb_of_word_a;     /* array containing the line number (from 0)  *
                              | of each word read.                         */
long *first_word_in_line_a;  /* array containing the index of the first    *
                              | word of each lines.                        */
long *shift_right_sym_pos_a; /* screen column number of the right          *
                              | scrolling symbol if any when in line or    *
                              | column mode.                               */

int forgotten_timer = -1;
int help_timer      = -1;
int winch_timer     = -1;
int daccess_timer   = -1;
int search_timer    = -1;

search_mode_t search_mode     = NONE;
search_mode_t old_search_mode = NONE;

int help_mode = 0; /* 1 if help is displayed else 0. */

int marked = -1; /* Index of the marked word or -1. */

char *word_buffer;

int (*my_isprint)(int);
int (*my_isempty)(const unsigned char *);

/* UTF-8 useful symbols. */
/* """"""""""""""""""""" */
/* clang-format off */
char * left_arrow      = "\xe2\x86\x90"; /* ← leftwards arrow.               */
char * up_arrow        = "\xe2\x86\x91"; /* ↑ upwards arrow.                 */
char * right_arrow     = "\xe2\x86\x92"; /* → rightwards arrow.              */
char * down_arrow      = "\xe2\x86\x93"; /* ↓ downwards arrow.               */
char * vertical_bar    = "\xe2\x94\x82"; /* │ box drawings light vertical.   */
char * shift_left_sym  = "\xe2\x97\x80"; /* ◀ black left-pointing triangle.  */
char * shift_right_sym = "\xe2\x96\xb6"; /* ▶ black right-pointing triangle. */
char * sbar_line       = "\xe2\x94\x82"; /* │ box drawings light vertical.   */
char * hbar_line       = "\xe2\x94\x80"; /* ─ box drawings light horizontal. */
char * hbar_left       = "\xe2\x97\x80"; /* ◀ black left-pointing triangle.  */
char * hbar_right      = "\xe2\x96\xb6"; /* ▶ black right-pointing triangle. */
char * sbar_top        = "\xe2\x94\x90"; /* ┐ box drawings light down and l. */
char * sbar_down       = "\xe2\x94\x98"; /* ┘ box drawings light up and l.   */
char * hbar_begin      = "\xe2\x94\x94"; /* └ box drawings light up and r.   */
char * hbar_end        = "\xe2\x94\x98"; /* ┘ box drawings light up and l.   */
char * sbar_curs       = "\xe2\x94\x83"; /* ┃ box drawings heavy vertical.   */
char * hbar_curs       = "\xe2\x94\x81"; /* ━ box drawings heavy horizontal. */
char * sbar_arr_up     = "\xe2\x96\xb2"; /* ▲ black up pointing triangle.    */
char * sbar_arr_down   = "\xe2\x96\xbc"; /* ▼ black down pointing triangle.  */
char * msg_arr_down    = "\xe2\x96\xbc"; /* ▼ black down pointing triangle.  */
/* clang-format on */

/* Mouse tracking. */
/* """"""""""""""" */
char *mouse_trk_on;
char *mouse_trk_off;

/* Variables used to manage the direct access entries. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""" */
daccess_t daccess;
char     *daccess_stack;
int       daccess_stack_head;

/* Variables used for fuzzy and substring searching. */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
long *matching_words_da = NULL; /* Array containing the index of all *
                                 | matching words.                   */

long *best_matching_words_da = NULL; /* Array containing the index of *
                                      | matching words containing a   *
                                      | consecutive suite of matching *
                                      | glyphs.                       */

long *alt_matching_words_da = NULL; /* Alternate array to contain only *
                                     | the matching candidates having  *
                                     | potentially a starting/ending   *
                                     | pattern.                        */

/* Variables used in signal handlers. */
/* """""""""""""""""""""""""""""""""" */
volatile sig_atomic_t got_winch          = 0;
volatile sig_atomic_t got_winch_alrm     = 0;
volatile sig_atomic_t got_forgotten_alrm = 0;
volatile sig_atomic_t got_help_alrm      = 0;
volatile sig_atomic_t got_daccess_alrm   = 0;
volatile sig_atomic_t got_search_alrm    = 0;
volatile sig_atomic_t got_timeout_tick   = 0;
volatile sig_atomic_t got_sigpipe        = 0;
volatile sig_atomic_t got_sigsegv        = 0;
volatile sig_atomic_t got_sigterm        = 0;
volatile sig_atomic_t got_sighup         = 0;

/* Variables used when a timeout is set (option -x). */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
timeout_t timeout;
char     *timeout_word;      /* printed word when the timeout type is WORD. */
char     *timeout_seconds;   /* string containing the number of remaining   *
                              | seconds.                                    */
int       quiet_timeout = 0; /* 1 when we want no message to be displayed.  */

/* *************** */
/* Help functions. */
/* *************** */

/* ============================================================ */
/* Parse, create and add an help entry.                         */
/* - attributes are preprocessed for a future quicker display.  */
/* - special entries t,m and c allow to ignore entries when not */
/*   in tag mode, when the mouse is not usable or when not in   */
/*   column mode.                                               */
/*                                                              */
/* entries       IN  array of help data.                        */
/* help_items_da OUT dynamic array to be updated.               */
/*                                                              */
/* Return the number of help entries added in help_items_da.    */
/* ============================================================ */
int
help_add_entries(win_t               *win,
                 term_t              *term,
                 toggle_t            *toggles,
                 help_attr_entry_t ***help_items_da,
                 help_entry_t        *entries,
                 int                  entries_nb)
{
  int index;
  int nb = 0;

  help_attr_entry_t *attr_entry;

  for (index = 0; index < entries_nb; index++)
  {
    /* Do not create a tag/pin related entry if tagging/pinning is not */
    /* enabled.                                                        */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (strchr(entries[index].flags, 't') && !toggles->taggable
        && !toggles->pinable)
      continue;

    /* Do not create a mouse related entry if -no_mouse is activated. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (strchr(entries[index].flags, 'm') && toggles->no_mouse)
      continue;

    /* Do not create a column/row related entry if the column or line_mode */
    /* is not activated.                                                   */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (strchr(entries[index].flags, 'c') && !win->line_mode && !win->col_mode)
      continue;

    /* Allocate and initialize a new attribute. */
    /* """""""""""""""""""""""""""""""""""""""" */
    attr_entry = xmalloc(sizeof(help_attr_entry_t));

    attr_entry->str  = xstrdup(entries[index].str);
    attr_entry->len  = entries[index].len;
    attr_entry->attr = attr_new();

    if (term->colors > 0)
      parse_attr(entries[index].main_attr_str, attr_entry->attr, term->colors);
    else
      parse_attr(entries[index].alt_attr_str, attr_entry->attr, term->colors);

    /* Add it into a dynamic array. */
    /* """""""""""""""""""""""""""" */
    BUF_PUSH(*help_items_da, attr_entry);

    nb++;
  }

  return nb;
}

/* ======================================================== */
/* Create an array of lines to be displayed by disp_help.   */
/* Created lines will have a dynamic length with leading    */
/* spaces removed.                                          */
/*                                                          */
/* last_line     IN  last logical line number (from 0).     */
/* help_items_da OUT dynamic array to be updated.           */
/*                                                          */
/* Return the dynamic array containing help data with       */
/*        attributes processed.                             */
/* ======================================================== */
help_attr_entry_t ***
init_help(win_t               *win,
          term_t              *term,
          toggle_t            *toggles,
          long                 last_line,
          help_attr_entry_t ***help_items_da)
{
  int index;            /* used to identify the objects long the help line. */
  int entries_nb;       /* number of all potentially displayable help       *
                         | entries.                                         */
  int entries_total_nb; /* number of help entries to display.               */
  int line = 0;         /* number of windows lines used by the help line.   */
  int len  = 0;         /* length of the help line.                         */
  int max_col;          /* when to split the help line.                     */
  int forced_nl;
  int first_in_line;

  /* array of arrays containing help items.                        */
  /* the content of this darray will be returned by this function. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  help_attr_entry_t ***help_lines_da = NULL;

  /* array of help items, element of help_lines_da. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  help_attr_entry_t **items_da = NULL;

  entries_nb = BUF_LEN(*help_items_da);

  if (entries_nb == 0)
  {
    char *Cleft_arrow  = concat("^", left_arrow, (char *)0);
    char *Cright_arrow = concat("^", right_arrow, (char *)0);
    char *du_arrows    = concat(down_arrow, up_arrow, (char *)0);
    char *arrows       = concat(left_arrow,
                          down_arrow,
                          up_arrow,
                          right_arrow,
                          (char *)0);

    help_entry_t header_entries[] = {
      { "Start", 5, "u", "u", "" },     { " ", 1, "u", "u", "" },
      { "of", 2, "u", "u", "" },        { " ", 1, "u", "u", "" },
      { "quick", 5, "u", "u", "" },     { " ", 1, "u", "u", "" },
      { "help.", 5, "u", "u", "" },     { " ", 1, "u", "u", "" },
      { "jk", 2, "bu", "bu", "" },      { "/", 1, "u", "u", "" },
      { du_arrows, 2, "bu", "bu", "" }, { " ", 1, "u", "u", "" },
      { "to", 2, "u", "u", "" },        { " ", 1, "u", "u", "" },
      { "scroll.", 7, "u", "u", "" },   { " ", 1, "u", "u", "" },
      { "\"", 1, "u", "u", "" },        { "man", 3, "bu", "bu", "" },
      { " ", 1, "u", "u", "" },         { "smenu", 5, "bu", "bu", "" },
      { "\"", 1, "u", "u", "" },        { " ", 1, "u", "u", "" },
      { "for", 3, "u", "u", "" },       { " ", 1, "u", "u", "" },
      { "more", 4, "u", "u", "" },      { " ", 1, "u", "u", "" },
      { "help.", 5, "u", "u", "" },
    };

    help_entry_t generic_entries[] = {
      { "\n", 0, "", "", "" },
      { "Move:", 5, "5+b", "r", "" },
      { "Mouse:", 6, "2", "u", "m" },
      { "B1", 2, "b", "b", "m" },
      { ",", 1, "", "", "m" },
      { "B3", 2, "b", "b", "m" },
      { ",", 1, "", "", "m" },
      { "Wheel", 5, "b", "b", "m" },
      { " ", 1, "", "", "m" },
      { "Keyb:", 5, "2", "u", "m" },
      { "hjkl", 4, "b", "b", "" },
      { ",", 1, "", "", "" },
      { arrows, 4, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "PgUp", 4, "b", "b", "" },
      { "/", 1, "", "", "" },
      { "Dn", 2, "b", "b", "" },
      { ",", 1, "", "", "c" },
      { "S-HOME", 6, "b", "b", "c" },
      { "/", 1, "", "", "c" },
      { Cleft_arrow, 2, "b", "b", "c" },
      { "/", 1, "", "", "c" },
      { "H", 1, "b", "b", "c" },
      { ",", 1, "", "", "c" },
      { "S-END", 5, "b", "b", "c" },
      { "/", 1, "", "", "c" },
      { Cright_arrow, 2, "b", "b", "c" },
      { "/", 1, "", "", "c" },
      { "L", 1, "b", "b", "c" },
      { "\n", 0, "", "", "c" },
      { "Shift:", 6, "5+b", "r", "c" },
      { "<", 1, "b", "b", "c" },
      { ",", 1, "", "", "c" },
      { ">", 1, "b", "b", "c" },
      { "\n", 0, "", "", "" },
      { "Abort:", 6, "5+b", "r", "" },
      { "q", 1, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "^C", 2, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Cancel:", 7, "5+b", "r", "" },
      { "ESC", 3, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Select:", 7, "5+b", "r", "" },
      { "Mouse:", 6, "2", "u", "m" },
      { "Double-B1", 9, "b", "b", "m" },
      { " ", 1, "", "", "m" },
      { "Keyb:", 5, "2", "u", "m" },
      { "CR", 2, "b", "b", "" },
      { "\n", 0, "", "", "" },
      { "Search:", 7, "5+b", "r", "" },
      { " ", 1, "", "", "" },
      { "Default", 7, "3", "i", "" },
      { "(", 1, "", "", "" },
      { "Fuzzy", 5, "3", "i", "" },
      { ")", 1, "", "", "" },
      { ":", 1, "3", "i", "" },
      { "/", 1, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Substr:", 7, "3", "i", "" },
      { "\"", 1, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "\'", 1, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Prefix:", 7, "3", "i", "" },
      { "=", 1, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "^", 1, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Fuzzy:", 6, "3", "i", "" },
      { "~", 1, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "*", 1, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "HOME", 4, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "^A", 2, "b", "b", "" },
      { ",", 1, "", "", "" },
      { " ", 1, "", "", "" },
      { "END", 3, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "^Z", 2, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Auto-", 5, "3", "i", "" },
      { "complete:", 9, "3", "i", "" },
      { "TAB", 3, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Validate:", 9, "3", "i", "" },
      { "CR", 2, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Next:", 5, "3", "i", "" },
      { "SP", 2, "b", "b", "" },
      { ",", 1, "", "", "" },
      { "n", 1, "b", "b", "" },
      { " ", 1, "", "", "" },
      { "Prev:", 5, "3", "i", "" },
      { "N", 1, "b", "b", "" },
      { "\n", 0, "", "", "t" },
      { "Mark:", 5, "5+b", "r", "t" },
      { "Mouse:", 6, "2", "u", "tm" },
      { "^B1", 3, "b", "b", "tm" },
      { "/", 1, "b", "b", "tm" },
      { "B3", 2, "b", "b", "tm" },
      { " ", 1, "", "", "tm" },
      { "Keyb:", 5, "2", "u", "tm" },
      { "mM", 2, "b", "b", "t" },
      { "\n", 0, "", "", "t" },
      { "Tag:", 4, "5+b", "r", "t" },
      { "Mouse:", 6, "2", "u", "tm" },
      { "B3", 2, "B", "B", "tm" },
      { " ", 1, "", "", "tm" },
      { "Zone:", 5, "3", "i", "ctm" },
      { "/", 1, "b", "b", "ctm" },
      { "Row:", 4, "3", "i", "ctm" },
      { "/", 1, "b", "b", "ctm" },
      { "Col:", 4, "3", "i", "ctm" },
      { "^B3", 3, "B", "B", "ctm" },
      { " ", 1, "b", "b", "ct" },
      { "Keyb:", 5, "2", "u", "tm" },
      { "t", 1, "b", "b", "t" },
      { ",", 1, "", "", "t" },
      { "u", 1, "b", "b", "ct" },
      { " ", 1, "", "", "ct" },
      { "r", 1, "b", "b", "ct" },
      { ",", 1, "", "", "ct" },
      { "c", 1, "b", "b", "ct" },
      { " ", 1, "", "", "ct" },
      { "Cont:", 5, "3", "i", "t" },
      { "T", 1, "b", "b", "t" },
      { "...", 3, "", "", "t" },
      { "T", 1, "b", "b", "t" },
      { ",", 1, "", "", "t" },
      { "Zone:", 5, "3", "i", "t" },
      { "Z", 1, "b", "b", "t" },
      { "...", 3, "", "", "t" },
      { "Z", 1, "b", "b", "t" },
      { ",", 1, "", "", "ct" },
      { "Row:", 4, "3", "i", "ct" },
      { "R", 1, "b", "b", "ct" },
      { "...", 3, "", "", "ct" },
      { "R", 1, "b", "b", "ct" },
      { ",", 1, "", "", "ct" },
      { "Col:", 4, "3", "i", "ct" },
      { "C", 1, "b", "b", "ct" },
      { "...", 3, "", "", "ct" },
      { "C", 1, "b", "b", "ct" },
      { " ", 1, "", "", "t" },
      { "Undo:", 5, "3", "i", "t" },
      { "U", 1, "b", "b", "t" },
      { ",", 1, "", "", "t" },
      { "^T", 2, "b", "b", "t" },
      { " ", 1, "", "", "t" },
    };

    help_entry_t trailer_entries[] = {
      { "\n", 0, "", "", "" },  { "End", 3, "u", "u", "" },
      { " ", 1, "u", "u", "" }, { "of", 2, "u", "u", "" },
      { " ", 1, "u", "u", "" }, { "quick", 5, "u", "u", "" },
      { " ", 1, "u", "u", "" }, { "help.", 5, "u", "u", "" },
    };

    entries_total_nb = 0;

    entries_nb = sizeof(header_entries) / sizeof(help_entry_t);
    entries_total_nb += help_add_entries(win,
                                         term,
                                         toggles,
                                         help_items_da,
                                         header_entries,
                                         entries_nb);

    entries_nb = sizeof(generic_entries) / sizeof(help_entry_t);
    entries_total_nb += help_add_entries(win,
                                         term,
                                         toggles,
                                         help_items_da,
                                         generic_entries,
                                         entries_nb);

    entries_nb = sizeof(trailer_entries) / sizeof(help_entry_t);
    entries_total_nb += help_add_entries(win,
                                         term,
                                         toggles,
                                         help_items_da,
                                         trailer_entries,
                                         entries_nb);
  }
  else
    entries_total_nb = entries_nb;

  /* Determine when to split the help line if necessary. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  max_col = term->ncolumns - 1;

  help_lines_da = NULL;
  items_da      = NULL;

  first_in_line = 0;

  /* Fill the darray of darrays, each line is a darray of help_attr_entry_t */
  /* A new line is added each time the next entry does not fit in the       */
  /* width of the window. This is done as long as there is space left in    */
  /* the window.                                                            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (index = 0; index < entries_total_nb; index++)
  {
    /* Ignore item if its length cannot be fully displayed. */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
    if ((*help_items_da)[index]->len >= max_col)
      continue;

    /* increase the total length of displayed items in the line. */
    /* ''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
    len += (*help_items_da)[index]->len;

    /* Set a flag if we need to start a new line. */
    /* '''''''''''''''''''''''''''''''''''''''''' */
    if (strcmp((*help_items_da)[index]->str, "\n") == 0)
      forced_nl = 1;
    else
      forced_nl = 0;

    if (len >= max_col || forced_nl)
    {
      line++;
      first_in_line = 1;

      /* Exit early if we do not have enough space. */
      /* '''''''''''''''''''''''''''''''''''''''''' */
      if (line > last_line)
        break;

      len = 0;

      /* Dynamically push the current items_da in help_lines_da */
      /* And initialize a new items_da.                         */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      BUF_PUSH(help_lines_da, items_da);
      items_da = NULL;

      /* Do not put the '\n' in the current items_da. */
      /* '''''''''''''''''''''''''''''''''''''''''''' */
      if (forced_nl)
        continue;
    }

    /* Skip ' ' and ',' when they appear first in a line. */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''' */
    if (first_in_line)
      if (strcmp((*help_items_da)[index]->str, " ") == 0
          || strcmp((*help_items_da)[index]->str, ",") == 0)
        continue;

    first_in_line = 0;
    BUF_PUSH(items_da, (*help_items_da)[index]);
  }

  if (items_da != NULL)
    BUF_PUSH(help_lines_da, items_da);

  return help_lines_da;
}

/* ================================================= */
/* Display the quick help content.                   */
/*                                                   */
/* help_lines_da      IN  array of help lines.       */
/* fst_disp_help_line IN first help line to display. */
/* ================================================= */
void
disp_help(win_t               *win,
          term_t              *term,
          help_attr_entry_t ***help_lines_da,
          int                  fst_disp_help_line)
{
  int index;   /* used to identify the objects long the help line. */
  int max_col; /* when to split the help line.                     */
  int displayed_lines;
  int total_lines;

  help_attr_entry_t **items_da;

  /* Determine when to split the help line if necessary. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  max_col = term->ncolumns - 1;

  /* Save the position of the terminal cursor so that it can be */
  /* put back there after printing of the help line.            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  (void)tputs(TPARM1(save_cursor), 1, outch);

  displayed_lines = 0;
  total_lines     = BUF_LEN(help_lines_da);

  /* Print the different objects forming the help lines.                 */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (index = fst_disp_help_line; index < total_lines; index++)
  {
    int i;
    int item;
    int nb_items;
    int len;

    if (displayed_lines == win->max_lines)
      break;

    displayed_lines++;

    len = 0;

    items_da = help_lines_da[index];
    nb_items = BUF_LEN(items_da);

    (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
    for (item = 0; item < nb_items; item++)
    {
      help_attr_entry_t *entry;

      entry = items_da[item];
      len += entry->len;

      (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
      apply_attr(term, *(entry->attr));

      fputs_safe(entry->str, stdout);
      (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
    }

    /* Fill the remaining space with spaces. */
    /* """"""""""""""""""""""""""""""""""""" */
    for (i = len; i < max_col; i++)
      fputc_safe(' ', stdout);
    if (displayed_lines < win->max_lines)
      fputc_safe('\n', stdout);
  }

  /* Put back the cursor to its saved position. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  (void)tputs(TPARM1(restore_cursor), 1, outch);
}

/* *********************************** */
/* Attributes string parsing function. */
/* *********************************** */

/* =========================================================== */
/* Allocation and initialization of a new attribute structure. */
/*                                                             */
/* Return the newly created attribute.                         */
/* =========================================================== */
attrib_t *
attr_new(void)
{
  attrib_t *attr;

  attr = xmalloc(sizeof(attrib_t));

  attr->is_set    = UNSET;
  attr->fg        = -1;
  attr->bg        = -1;
  attr->bold      = (signed char)-1;
  attr->dim       = (signed char)-1;
  attr->reverse   = (signed char)-1;
  attr->standout  = (signed char)-1;
  attr->underline = (signed char)-1;
  attr->italic    = (signed char)-1;
  attr->invis     = (signed char)-1;
  attr->blink     = (signed char)-1;

  return attr;
}

/* ============================================ */
/* Decode attributes toggles if any.            */
/* b -> bold                                    */
/* d -> dim                                     */
/* r -> reverse                                 */
/* s -> standout                                */
/* u -> underline                               */
/* i -> italic                                  */
/* x -> invis                                   */
/* l -> blink                                   */
/*                                              */
/* Return 0 if some unexpected toggle is found, */
/*        1 otherwise.                          */
/* ============================================ */
int
decode_attr_toggles(char *s, attrib_t *attr)
{
  int rc = 1;

  attr->bold      = (signed char)0;
  attr->dim       = (signed char)0;
  attr->reverse   = (signed char)0;
  attr->standout  = (signed char)0;
  attr->underline = (signed char)0;
  attr->italic    = (signed char)0;
  attr->invis     = (signed char)0;
  attr->blink     = (signed char)0;

  while (*s != '\0')
  {
    switch (*s)
    {
      case 'b':
        attr->bold   = (signed char)1;
        attr->is_set = SET;
        break;
      case 'd':
        attr->dim    = (signed char)1;
        attr->is_set = SET;
        break;
      case 'r':
        attr->reverse = (signed char)1;
        attr->is_set  = SET;
        break;
      case 's':
        attr->standout = (signed char)1;
        attr->is_set   = SET;
        break;
      case 'u':
        attr->underline = (signed char)1;
        attr->is_set    = SET;
        break;
      case 'i':
        attr->italic = (signed char)1;
        attr->is_set = SET;
        break;
      case 'n':
        attr->invis  = (signed char)1;
        attr->is_set = SET;
        break;
      case 'l':
        attr->blink  = (signed char)1;
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

/* =============================================================*/
/* Parse attributes in str in the form [fg][/bg][[,.+]toggles]  */
/* where:                                                       */
/* fg and bg are short representing a color value               */
/* toggles is an array of toggles (see decode_attr_toggles)     */
/* Returns 1 on success else 0.                                 */
/* attr will be filled by the function.                         */
/* =============================================================*/
int
parse_attr(char *str, attrib_t *attr, short colors)
{
  int   n;
  char *pos;
  char  s1[12] = { (char)0 }; /* For the colors.     */
  char  s2[9]  = { (char)0 }; /* For the attributes. */
  short d1 = -1, d2 = -1;     /* colors. */
  int   rc = 1;
  char  c  = '\0';

  /* 11: 4 type+colon,2x3 for colors, 1 for slash.     */
  /* 8 : max size for the concatenation of attributes. */
  /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
  n = sscanf(str, "%11[^,.+]%*[,.+]%8s%c", s1, s2, &c);

  if (n == 0 || c != '\0')
    goto error;

  if ((pos = strchr(s1, '/')))
  {
    if (pos == s1) /* s1 starts with a / */
    {
      d1 = -1;
      if (sscanf(s1 + 1, "%hd", &d2) == 0)
      {
        d2 = -1;
        if (n == 1)
          goto error;
      }
      else if (d2 < 0)
        goto error;
    }
    else if (sscanf(s1, "%hd/%hd", &d1, &d2) < 2)
    {
      d1 = d2 = -1;
      if (n == 1)
        goto error;
    }
    else if (d1 < 0 || d2 < 0)
      goto error;
  }
  else /* no / in the first string. */
  {
    d2 = -1;
    if (sscanf(s1, "%hd", &d1) == 0)
    {
      d1 = -1;
      if (n == 2 || decode_attr_toggles(s1, attr) == 0)
        goto error;
    }
  }

  if (colors == 0) /* Monochrome. */
  {
    attr->fg = -1;
    attr->bg = -1;
  }
  else
  {
    attr->fg = d1 < colors ? d1 : -1;
    attr->bg = d2 < colors ? d2 : -1;
  }

  if (n == 2)
    rc = decode_attr_toggles(s2, attr);

  return rc;

error:
  return 0;
}

/* ============================================== */
/* Set the terminal attributes according to attr. */
/* ============================================== */
void
apply_attr(term_t *term, attrib_t attr)
{
  if (attr.fg >= 0)
    set_foreground_color(term, attr.fg);

  if (attr.bg >= 0)
    set_background_color(term, attr.bg);

  if (attr.bold > (signed char)0)
    (void)tputs(TPARM1(enter_bold_mode), 1, outch);

  if (attr.dim > (signed char)0)
    (void)tputs(TPARM1(enter_dim_mode), 1, outch);

  if (attr.reverse > (signed char)0)
    (void)tputs(TPARM1(enter_reverse_mode), 1, outch);

  if (attr.standout > (signed char)0)
    (void)tputs(TPARM1(enter_standout_mode), 1, outch);

  if (attr.underline > (signed char)0)
    (void)tputs(TPARM1(enter_underline_mode), 1, outch);

  if (attr.italic > (signed char)0)
    (void)tputs(TPARM1(enter_italics_mode), 1, outch);

  if (attr.invis > (signed char)0)
    (void)tputs(TPARM1(enter_secure_mode), 1, outch);

  if (attr.blink > (signed char)0)
    (void)tputs(TPARM1(enter_blink_mode), 1, outch);
}

/* ********************* */
/* ini parsing function. */
/* ********************* */

/* ===================================================== */
/* Callback function called when parsing each non-header */
/* line of the ini file.                                 */
/* Returns 0 if OK, 1 if not.                            */
/* ===================================================== */
int
ini_cb(win_t      *win,
       term_t     *term,
       limit_t    *limits,
       ticker_t   *timers,
       misc_t     *misc,
       mouse_t    *mouse,
       const char *section,
       const char *name,
       char       *value)
{
  int error      = 0;
  int has_colors = term->colors > 7;

  if (strcmp(section, "colors") == 0)
  {
    attrib_t v = { UNSET,
                   /* fg        */ -1,
                   /* bg        */ -1,
                   /* bold      */ (signed char)-1,
                   /* dim       */ (signed char)-1,
                   /* reverse   */ (signed char)-1,
                   /* standout  */ (signed char)-1,
                   /* underline */ (signed char)-1,
                   /* italic    */ (signed char)-1,
                   /* invis     */ (signed char)-1,
                   /* blink     */ (signed char)-1 };

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
        if (v.bold >= (signed char)0)             \
          win->x##_attr.bold = v.bold;            \
        if (v.dim >= (signed char)0)              \
          win->x##_attr.dim = v.dim;              \
        if (v.reverse >= (signed char)0)          \
          win->x##_attr.reverse = v.reverse;      \
        if (v.standout >= (signed char)0)         \
          win->x##_attr.standout = v.standout;    \
        if (v.underline >= (signed char)0)        \
          win->x##_attr.underline = v.underline;  \
        if (v.italic >= (signed char)0)           \
          win->x##_attr.italic = v.italic;        \
        if (v.invis >= (signed char)0)            \
          win->x##_attr.invis = v.invis;          \
        if (v.blink >= (signed char)0)            \
          win->x##_attr.blink = v.blink;          \
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
        if (v.bold >= (signed char)0)                   \
          win->x##_attr[y - 1].bold = v.bold;           \
        if (v.dim >= (signed char)0)                    \
          win->x##_attr[y - 1].dim = v.dim;             \
        if (v.reverse >= (signed char)0)                \
          win->x##_attr[y - 1].reverse = v.reverse;     \
        if (v.standout >= (signed char)0)               \
          win->x##_attr[y - 1].standout = v.standout;   \
        if (v.underline >= (signed char)0)              \
          win->x##_attr[y - 1].underline = v.underline; \
        if (v.italic >= (signed char)0)                 \
          win->x##_attr[y - 1].italic = v.italic;       \
        if (v.invis >= (signed char)0)                  \
          win->x##_attr[y - 1].invis = v.invis;         \
        if (v.blink >= (signed char)0)                  \
          win->x##_attr[y - 1].blink = v.blink;         \
      }                                                 \
    }                                                   \
  }

    /* [colors] section. */
    /* """"""""""""""""" */
    if (has_colors)
    {
      if (strcmp(name, "method") == 0)
      {
        if (strcmp(value, "classic") == 0)
          term->color_method = CLASSIC;
        else if (strcmp(value, "ansi") == 0)
          term->color_method = ANSI;
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
      CHECK_ATTR(message)
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
      CHECK_ATT_ATTR(special, 6)
      CHECK_ATT_ATTR(special, 7)
      CHECK_ATT_ATTR(special, 8)
      CHECK_ATT_ATTR(special, 9)
      /* clang-format on */
    }
  }
  else if (strcmp(section, "window") == 0)
  {
    int v;

    /* [window] section. */
    /* """"""""""""""""" */
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
    long v;

    /* [limits] section. */
    /* """"""""""""""""" */
    if (strcmp(name, "word_length") == 0)
    {
      if ((error = !(sscanf(value, "%ld", &v) == 1 && v > 0)))
        goto out;
      else
        limits->word_length = v;
    }
    else if (strcmp(name, "words") == 0)
    {
      if ((error = !(sscanf(value, "%ld", &v) == 1 && v > 0)))
        goto out;
      else
        limits->words = v;
    }
    else if (strcmp(name, "columns") == 0)
    {
      if ((error = !(sscanf(value, "%ld", &v) == 1 && v > 0)))
        goto out;
      else
        limits->cols = v;
    }
  }
  else if (strcmp(section, "timers") == 0)
  {
    int v;

    /* [timers] section. */
    /* """"""""""""""""" */
    if (strcmp(name, "help") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->help = v;
    }
    else if (strcmp(name, "forgotten") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->forgotten = v;
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
    else if (strcmp(name, "search") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        timers->search = v;
    }
  }
  else if (strcmp(section, "mouse") == 0)
  {
    int v;

    /* [mouse] section. */
    /* """"""""""""""" */
    if (strcmp(name, "double_click_delay") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        mouse->double_click_delay = v;
    }
  }
  else if (strcmp(section, "misc") == 0)
  {
    /* [misc] section. */
    /* """"""""""""""" */
    if (strcmp(name, "default_search_method") == 0)
    {
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
  }

out:

  return error;
}

/* ======================================================================== */
/* Load an .ini format file.                                                */
/* filename - path to a file.                                               */
/* report   - callback can return non-zero to stop, the callback error code */
/*            returned from this function.                                  */
/* return   - return 0 on success.                                          */
/*                                                                          */
/* This function is public domain. No copyright is claimed.                 */
/* Jon Mayo April 2011.                                                     */
/* ======================================================================== */
int
ini_load(const char *filename,
         win_t      *win,
         term_t     *term,
         limit_t    *limits,
         ticker_t   *timers,
         misc_t     *misc,
         mouse_t    *mouse,
         int (*report)(win_t      *win,
                       term_t     *term,
                       limit_t    *limits,
                       ticker_t   *timers,
                       misc_t     *misc,
                       mouse_t    *mouse,
                       const char *section,
                       const char *name,
                       char       *value))
{
  char  name[64]     = "";
  char  value[256]   = "";
  char  section[128] = "";
  char *s;
  FILE *f;
  int   cnt;
  int   error;

  /* If the filename is empty we skip this phase and use the */
  /* default values.                                         */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (filename == NULL)
    return 1;

  /* We do that if the file is not readable as well. */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  f = fopen_safe(filename, "r");
  if (f == NULL)
    return 0; /* Returns success as the presence of this file *
               | is optional.                                 */

  error = 0;

  /* Skip blank lines. */
  /* """"""""""""""""" */
  while (fscanf(f, "%*[\n]") == 1)
  {
  }

  while (!feof(f))
  {
    if (fscanf(f, " [%127[^];\n]]", section) == 1)
    {
      /* Do nothing. */
      /* """"""""""" */
    }

    if ((cnt = fscanf(f, " %63[^=;\n] = %255[^;\n]", name, value)))
    {
      if (cnt == 1)
        *value = 0;

      for (s = name + strlen(name) - 1; s > name && isspace(*s); s--)
        *s = 0;

      for (s = value + strlen(value) - 1; s > value && isspace(*s); s--)
        *s = 0;

      /* Callback function calling. */
      /* """""""""""""""""""""""""" */
      error =
        report(win, term, limits, timers, misc, mouse, section, name, value);

      if (error)
        goto out;
    }

    if (fscanf(f, " ;%*[^\n]"))
    {
      /* To silence the compiler about unused results. */
    }

    /* Skip blank lines. */
    /* """"""""""""""""" */
    while (fscanf(f, "%*[\n]") == 1)
    {
      /* Do nothing. */
      /* """"""""""" */
    }
  }

out:
  fclose(f);

  if (error)
    fprintf(stderr,
            "Invalid entry found: %s=%s in %s.\n",
            name,
            value,
            filename);

  return error;
}

/* ====================================================================== */
/* Returns the full path of the configuration file supposed to be in the  */
/* user's home directory if base is NULL or in the content of the         */
/* environment variable base.                                             */
/*                                                                        */
/* Returns NULL if the built path is too large or if base does not exist. */
/* ====================================================================== */
char *
make_ini_path(char *name, char *base)
{
  char *path;
  char *home;
  long  path_max;
  long  len;

  /* Set the prefix of the path from the environment */
  /* base can be "HOME" or "PWD".                    */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  home = getenv(base);

  if (home == NULL)
    home = "";

  path_max = pathconf(".", _PC_PATH_MAX);
  len      = strlen(home) + strlen(name) + 3;

  if (path_max < 0)
    path_max = 4096; /* POSIX minimal value. path_max >= 4096. */

  if (len <= path_max)
  {
    char *conf;

    path = xmalloc(len);
    conf = strrchr(name, '/');

    if (conf != NULL)
      conf++;
    else
      conf = name;

    snprintf(path, len, "%s/.%s", home, conf);
  }
  else
    path = NULL;

  return path;
}

/* ********************************* */
/* Functions used when sorting tags. */
/* ********************************* */

/* =================================================================== */
/* Compare the pin order of two pinned word in the output list.        */
/* Returns -1 if the pinning value is less than the second, 0 if it is */
/* equal to the second and 1 if it is greater than the second.         */
/* =================================================================== */
int
tag_comp(void const *a, void const *b)
{
  output_t *oa = (output_t *)a;
  output_t *ob = (output_t *)b;

  if (oa->order == ob->order)
    return 0;

  return (oa->order < ob->order) ? -1 : 1;
}

/* ========================================================= */
/* Swap the values of two selected words in the output list. */
/* ========================================================= */
void
tag_swap(void **a, void **b)
{
  output_t *oa = (output_t *)*a;
  output_t *ob = (output_t *)*b;

  char *tmp_str;
  long  tmp_order;

  tmp_str        = oa->output_str;
  oa->output_str = ob->output_str;
  ob->output_str = tmp_str;

  tmp_order = oa->order;
  oa->order = ob->order;
  ob->order = tmp_order;
}

/* ****************** */
/* Utility functions. */
/* ****************** */

/* =================================================================== */
/* Create a new element to be added to the tst_search_list used by the */
/* search mechanism.                                                   */
/* Return the newly created element.                                   */
/* =================================================================== */
sub_tst_t *
sub_tst_new(void)
{
  sub_tst_t *elem = xmalloc(sizeof(sub_tst_t));

  elem->size  = 64;
  elem->count = 0;
  elem->array = xmalloc(elem->size * sizeof(tst_node_t));

  return elem;
}

/* ========================================= */
/* Emit a small (visual) beep warn the user. */
/* ========================================= */
void
my_beep(toggle_t *toggles)
{
  struct timespec ts, rem;

  if (!toggles->visual_bell)
    fputc_safe('\a', stdout);
  else
  {
    int rc;

    (void)tputs(TPARM1(cursor_visible), 1, outch);

    ts.tv_sec  = 0;
    ts.tv_nsec = 200000000; /* 0.2s */

    errno = 0;
    rc    = nanosleep(&ts, &rem);

    while (rc < 0 && errno == EINTR)
    {
      errno = 0;
      rc    = nanosleep(&rem, &rem);
    }

    (void)tputs(TPARM1(cursor_invisible), 1, outch);
  }
}

/* ================================================================== */
/* Integer verification constraint for ctxopt.                        */
/* Return 1 if par is the representation of an integer else return 0. */
/* ================================================================== */
int
check_integer_constraint(int nb_args, char **args, char *value, char *par)
{
  if (!is_integer(value))
  {
    fprintf(stderr, "The argument of %s is not an integer: %s", par, value);
    return 0;
  }
  return 1;
}

/* ======================================================================= */
/* Update the bitmap associated with a word. The bits set to 1 in this     */
/* bitmap indicate the positions of the UFT-8 glyphs of the search buffer  */
/* in the word.                                                            */
/*                                                                         */
/* The disp_word function will use it to display these special characters. */
/*                                                                         */
/* mode     is the search method.                                          */
/* data     contains information about the search buffer.                  */
/* affinity determines if we must only consider matches that occur at      */
/*          the start, the end or if we just don't care.                   */
/* ======================================================================= */
void
update_bitmaps(search_mode_t     mode,
               search_data_t    *data,
               bitmap_affinity_t affinity)
{
  long i, j, n; /* work variables.                                       */

  long bm_len; /* number of chars taken by the bit mask.                 */

  char *start; /* pointer on the position of the matching position       *
                | of the last search buffer glyph in the word.           */

  char *bm; /* the word's current bitmap.                                */

  char *str;      /* copy of the current word put in lower case.         */
  char *str_orig; /* original version of the word.                       */

  char *sb_orig = data->buf; /* sb: search buffer.                       */

  long *o    = data->off_a;      /* array of the offsets of the search   *
                                  | buffer glyphs.                       */
  long *l    = data->len_a;      /* array of the lengths in bytes of     *
                                  | the search buffer glyphs.            */
  long  last = data->mb_len - 1; /* offset of the last glyph in the      *
                                  | search buffer.                       */

  BUF_CLEAR(best_matching_words_da);

  if (mode == FUZZY || mode == SUBSTRING)
  {
    char *sb;
    char *first_glyph;
    long  badness = 0; /* number of 0s between two 1s. */

    first_glyph = xmalloc(5);

    /* In fuzzy search mode, case is not taken into account */
    /* during the search.                                   */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (mode == FUZZY)
    {
      sb = xstrdup(sb_orig); /* sb initially points to sb_orig. */
      utf8_strtolower(sb, sb_orig);
    }
    else
      sb = sb_orig;

    for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
    {
      long lmg; /* Position of the last matching glyph of the search buffer *
                 | in a word.                                               */

      n = matching_words_da[i];

      str_orig = xstrdup(word_a[n].str + daccess.flength);

      /* We need to remove the trailing spaces to use the     */
      /* following algorithm.                                 */
      /* .len holds the original length in bytes of the word. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      rtrim(str_orig, " \t", 0);

      bm_len = (word_a[n].mb - daccess.flength) / CHAR_BIT + 1;
      bm     = word_a[n].bitmap;

      /* In fuzzy search mode str are converted in lower case letters */
      /* for comparison reason.                                       */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (mode == FUZZY)
      {
        str = xstrdup(str_orig);
        utf8_strtolower(str, str_orig);
      }
      else
        str = str_orig;

      start = str;
      lmg   = 0;

      /* Start points to the first UTF-8 glyph of the word. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      while ((size_t)(start - str) < word_a[n].len - daccess.flength)
      {
        /* Reset the bitmap. */
        /* """"""""""""""""" */
        memset(bm, '\0', bm_len);

        /* Compare the glyph pointed to by start to the last glyph of */
        /* the search buffer, the aim is to point to the first        */
        /* occurrence of the last glyph of it.                        */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (memcmp(start, sb + o[last], l[last]) == 0)
        {
          char *p; /* Pointer to the beginning of an UTF-8 glyph in *
                    | the potential lowercase version of the word.  */

          long sg; /* Index going from lmg backward to 0 of the tested *
                    | glyphs of the search buffer (searched glyph).    */

          if (last == 0)
          {
            /* There is only one glyph in the search buffer, we can */
            /* stop here.                                           */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
            BIT_ON(bm, lmg);
            if (affinity != END_AFFINITY)
              break;
          }

          /* If the search buffer contains more than one glyph, we need  */
          /* to search the first combination which match the buffer in   */
          /* the word.                                                   */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          p = start;
          j = last; /* j counts the number of glyphs in the search buffer *
                     | not found in the word.                             */

          /* Proceed backwards from the position of last glyph of the      */
          /* search to check if all the previous glyphs can be fond before */
          /* in the word. If not try to find the next position of this     */
          /* last glyph in the word.                                       */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          sg = lmg;
          while (j > 0 && (p = utf8_prev(str, p)) != NULL)
          {
            if (memcmp(p, sb + o[j - 1], l[j - 1]) == 0)
            {
              BIT_ON(bm, sg - 1);
              j--;
            }
            else if (mode == SUBSTRING)
              break;

            sg--;
          }

          /* All the glyphs have been found. */
          /* """"""""""""""""""""""""""""""" */
          if (j == 0)
          {
            BIT_ON(bm, lmg);
            if (affinity != END_AFFINITY)
              break;
          }
        }

        lmg++;
        start = utf8_next(start);
      }

      if (mode == FUZZY)
      {
        size_t mb_index;

        free(str);

        /* We know that the first non blank glyph is part of the pattern, */
        /* so highlight it if it is not and suppresses the highlighting   */
        /* of the next occurrence that must be here because this word has */
        /* already been filtered by select_starting_pattern().            */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (affinity == START_AFFINITY)
        {
          size_t i;
          long   mb_len;

          /* Skip leading spaces and tabs. */
          /* """"""""""""""""""""""""""""" */
          for (i = 0; i < word_a[n].mb; i++)
            if (!isblank(*(word_a[n].str + daccess.flength + i)))
              break;

          first_glyph = utf8_strprefix(first_glyph,
                                       word_a[n].str + i,
                                       1,
                                       &mb_len);

          if (!BIT_ISSET(word_a[n].bitmap, i))
          {
            char *ptr1, *ptr2;

            BIT_ON(word_a[n].bitmap, i);

            ptr1 = word_a[n].str + i;
            i++;
            while ((ptr2 = utf8_next(ptr1)) != NULL)
            {
              if (memcmp(ptr2, first_glyph, mb_len) == 0)
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
        /* badness of a match. The goal is to put the cursor on the word */
        /* with the smallest badness.                                    */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        mb_index = 0;
        j        = 0;
        badness  = 0;

        while (mb_index < word_a[n].mb
               && !BIT_ISSET(word_a[n].bitmap, mb_index))
          mb_index++;

        while (mb_index < word_a[n].mb)
        {
          if (!BIT_ISSET(word_a[n].bitmap, mb_index))
            badness++;
          else
            j++;

          /* Stop here if all the possible bits has been checked as they  */
          /* cannot be more numerous than the number of UTF-8 glyphs in   */
          /* the search buffer.                                           */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (j == data->mb_len)
            break;

          mb_index++;
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
          BUF_PUSH(best_matching_words_da, n);
      }
    }

    if (mode == FUZZY)
      free(sb);

    free(first_glyph);
  }
  else if (mode == PREFIX)
  {
    for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
    {
      n      = matching_words_da[i];
      bm     = word_a[n].bitmap;
      bm_len = (word_a[n].mb - daccess.flength) / CHAR_BIT + 1;

      memset(bm, '\0', bm_len);

      for (j = 0; j <= last; j++)
        BIT_ON(bm, j);
    }
  }
}

/* ========================================================= */
/* Find the next word index in the list of matching words    */
/* using the bisection search algorithm.                     */
/* Set the value of index to -1 and returns -1 if not found. */
/* ========================================================= */
long
find_next_matching_word(long *array, long nb, long value, long *index)
{
  /* Use the cached value when possible. */
  /* """"""""""""""""""""""""""""""""""" */
  if (*index >= 0 && *index < nb - 1 && array[*index] == value)
    return (array[++(*index)]);

  if (nb > 0)
  {
    long left = 0, right = nb;

    /* Bisection search. */
    /* ''''''''''''''''' */
    while (left < right)
    {
      long middle = (left + right) / 2;

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

    if (value > array[nb - 1])
    {
      *index = -1;
      return -1;
    }

    *index = nb - 1;
    return array[*index];
  }

  *index = -1;
  return -1;
}

/* ========================================================== */
/* Find the previous word index in the list of matching words */
/* using the bisection search algorithm.                      */
/* set the value of index to -1 and returns -1 if not found.  */
/* ========================================================== */
long
find_prev_matching_word(long *array, long nb, long value, long *index)
{
  /* Use the cached value when possible. */
  /* """"""""""""""""""""""""""""""""""" */
  if (*index > 0 && array[*index] == value)
    return (array[--(*index)]);

  if (nb > 0)
  {
    /* Bisection search. */
    /* ''''''''''''''''' */

    long left = 0, right = nb;

    while (left < right)
    {
      long middle = (left + right) / 2;

      if (array[middle] == value)
      {
        if (middle > 0)
        {
          *index = middle - 1;
          return array[*index];
        }

        *index = -1;
        return -1;
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

    *index = -1;
    return -1;
  }

  *index = -1;
  return -1;
}

/* ============================================================= */
/* Remove all traces of matched words and redisplay the windows. */
/* ============================================================= */
void
clean_matches(search_data_t *search_data, long size)
{
  long i;

  /* Clean the list of lists data-structure containing the search levels */
  /* Note that the first element of the list is never deleted.           */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (tst_search_list != NULL)
  {
    ll_node_t *fuzzy_node;
    sub_tst_t *sub_tst_data;

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

  search_data->err = 0;

  /* Clean the search buffer. */
  /* """""""""""""""""""""""" */
  memset(search_data->buf, '\0', size - daccess.flength);

  search_data->len           = 0;
  search_data->mb_len        = 0;
  search_data->only_ending   = 0;
  search_data->only_starting = 0;

  /* Clean the match flags and bitmaps. */
  /* """""""""""""""""""""""""""""""""" */
  for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
  {
    long n = matching_words_da[i];

    word_a[n].is_matching = 0;

    memset(word_a[n].bitmap,
           '\0',
           (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
  }

  BUF_CLEAR(matching_words_da);
}

/* *************************** */
/* Terminal utility functions. */
/* *************************** */

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
  fputc_safe(c, stdout);
  return 1;
}

/* =============================================== */
/* Set the terminal in non echo/non canonical mode */
/* wait for at least one byte, no timeout.         */
/* =============================================== */
void
setup_term(int const       fd,
           struct termios *old_in_attrs,
           struct termios *new_in_attrs)
{
  int error;

  /* Save the terminal parameters and configure it in row mode. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tcgetattr(fd, old_in_attrs);

  new_in_attrs->c_iflag = 0;
  new_in_attrs->c_oflag = old_in_attrs->c_oflag;
  new_in_attrs->c_cflag = old_in_attrs->c_cflag;
  new_in_attrs->c_lflag = old_in_attrs->c_lflag & ~(ICANON | ECHO | ISIG);

  new_in_attrs->c_cc[VMIN]   = 1;    /* wait for at least 1 byte. */
  new_in_attrs->c_cc[VTIME]  = 0;    /* no timeout.               */
  new_in_attrs->c_cc[VERASE] = 0x08; /* ^H.                       */

  error = tcsetattr_safe(fd, TCSANOW, new_in_attrs);

  if (error == -1)
  {
    perror("smenu");
    exit(EXIT_FAILURE);
  }
}

/* ====================================== */
/* Set the terminal in its previous mode. */
/* ====================================== */
void
restore_term(int const fd, struct termios *old_in_attrs)
{
  int error;

  error = tcsetattr_safe(fd, TCSANOW, old_in_attrs);

  if (error == -1)
  {
    perror("smenu");
    exit(EXIT_FAILURE);
  }
}

/* ============================================== */
/* Get the terminal numbers of lines and columns  */
/* Assume that the TIOCGWINSZ, ioctl is available */
/* Defaults to 80x24.                             */
/* ============================================== */
void
get_terminal_size(int * const r, int * const c, term_t *term)
{
  struct winsize ws;

  *r = *c = -1;

  if (ioctl(0, TIOCGWINSZ, &ws) == 0)
  {
    *r = ws.ws_row;
    *c = ws.ws_col;

    if (*r > 0 && *c > 0)
      return;
  }

  *r = tigetnum("lines");
  *c = tigetnum("cols");

  if (*r <= 0 || *c <= 0)
  {
    *r = 80;
    *c = 24;
  }
}

/* =========================================================== */
/* Gets the cursor position in the terminal.                   */
/* Assume that the Escape sequence ESC [ 6 n is available.     */
/* Retries up to 64 times in case of system call interruption. */
/* Returns 1 on success and 0 on error, in this case *r and *c */
/* will contain 0.                                             */
/* =========================================================== */
int
get_cursor_position(int * const r, int * const c)
{
  char  buf[32] = { 0 };
  char *s;

  int attempts = 64;
  int v;
  int rc = 1;

  int ask; /* Number of asked characters.    */
  int got; /* Number of characters obtained. */

  int buf_size = sizeof(buf);

  *r = *c = 0;

  /* Report cursor location. */
  /* """"""""""""""""""""""" */
  while ((v = write(STDOUT_FILENO, "\x1b[6n", 4)) == -1 && attempts)
  {
    if (errno == EINTR)
      attempts--;
    else
    {
      rc = 0;
      goto read;
    }

    errno = 0;
  }

  if (v != 4)
    rc = 0;

read:

  /* Read the response: ESC [ rows ; cols R. */
  /* """"""""""""""""""""""""""""""""""""""" */
  *(s = buf) = 0;

  do
  {
    ask = buf_size - 1 - (s - buf);
    got = read(STDIN_FILENO, s, ask);

    if (got < 0 && errno == EINTR)
      got = 0;
    else if (got == 0)
      break;

    s += got;
  } while (strchr(buf, 'R') == NULL);

  /* Parse it. */
  /* """"""""" */
  if (buf[0] != 0x1b || buf[1] != '[')
    return 0;

  if (sscanf(buf + 2, "%d;%d", r, c) != 2)
    rc = 0;

  return rc;
}

/* ======================================================================== */
/* Parse a regular expression based selector.                               */
/* The string to parse is bounded by a delimiter so we must parse something */
/* like: <delim><regex string><delim> as in /a.*b/ by example.              */
/*                                                                          */
/* str            (in)  delimited string to parse                           */
/* regex_list     (out) regex list to modify.                               */
/* ======================================================================== */
void
parse_regex_selector_part(char *str, ll_t **regex_list)
{
  regex_t *regex;

  /* Remove the last character of str (the delimiter).*/
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  str[strlen(str) - 1] = '\0';

  /* Ignore the first character of str (the delimiter).       */
  /* compile it and add a compiled regex in a dedicated list. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  regex = xmalloc(sizeof(regex_t));
  if (regcomp(regex, str + 1, REG_EXTENDED | REG_NOSUB) == 0)
  {
    if (*regex_list == NULL)
      *regex_list = ll_new();

    ll_append(*regex_list, regex);
  }
}

/* ================================================================= */
/* Compare two elements of type attr_elem_t.                         */
/* The comparison key is the first member of the structure which is  */
/* of type attrib_t.                                                 */
/* Returns 0 is the two elements are equals otherwise returns 1.     */
/* ================================================================= */
int
attr_elem_cmp(void const *a, void const *b)
{
  const attr_elem_t *ai = a;
  const attr_elem_t *bi = b;

  if (ai->attr->fg != bi->attr->fg)
    return 1;
  if (ai->attr->bg != bi->attr->bg)
    return 1;
  if (ai->attr->bold != bi->attr->bold)
    return 1;
  if (ai->attr->dim != bi->attr->dim)
    return 1;
  if (ai->attr->reverse != bi->attr->reverse)
    return 1;
  if (ai->attr->standout != bi->attr->standout)
    return 1;
  if (ai->attr->underline != bi->attr->underline)
    return 1;
  if (ai->attr->italic != bi->attr->italic)
    return 1;
  if (ai->attr->invis != bi->attr->invis)
    return 1;
  if (ai->attr->blink != bi->attr->blink)
    return 1;

  return 0;
}

/* ===================================================================== */
/* Parse a column or row selector string whose syntax is defined as:     */
/* [<letter>]<item1>|,<item>|,...                                        */
/* <item> is <range>| <delimited regex>                                  */
/* <item> is (<range>| <delimited regex>):<attribute> if letter is a|A.  */
/* <range> is n1-n2 | n1 | -n2 | n1- where n1 starts with 1.             */
/* <delimited regex> is <char><regex><char> (e.g. /<regex>/).            */
/*                                                                       */
/* <letter> is a|A|s|S|r|R|u|U where:                                    */
/* i|I is for 'include'.                                                 */
/* e|E is for 'exclude'.                                                 */
/* l|L is for 'left' alignment.                                          */
/* r|R is for 'right' alignment.                                         */
/* c|C is for 'center' alignment.                                        */
/* a|A for defining attributes for rows or columns.                      */
/*                                                                       */
/* str               (in)  string to parse.                              */
/* filter            (out) is INCLUDE_FILTER or EXCLUDE_FILTER according */
/*                         to <letter>.                                  */
/* unparsed          (out) is empty on success and contains the unparsed */
/*                         part on failure.                              */
/* inc_interval_list (out) is a list of the interval of elements to      */
/*                         be included.                                  */
/* inc_regex_list    (out) is a list of extended regular expressions of  */
/*                         elements to be included.                      */
/* exc_interval_list (out) is a list of the interval of elements to be   */
/*                         excluded.                                     */
/* exc_regex_list    (out) is a list of regex matching elements to       */
/*                         be excluded.                                  */
/* aX_interval_list  (out) is a list of the interval of elements to be   */
/*                         aligned to the left, right or centered.       */
/* aX_regex_list     (out) is a list of regex matching elements to       */
/*                         be aligned to the left, right or centered.    */
/* at_interval_list  (out) is a list of the interval of elements with    */
/*                         a given attribute.                            */
/* at_regex_list     (out) is a list of regex matching elements with     */
/*                         a given attribute.                            */
/* ===================================================================== */
void
parse_selectors(char        *str,
                filters_t   *filter,
                char       **unparsed,
                ll_t       **inc_interval_list,
                ll_t       **inc_regex_list,
                ll_t       **exc_interval_list,
                ll_t       **exc_regex_list,
                ll_t       **al_interval_list,
                ll_t       **al_regex_list,
                ll_t       **ar_interval_list,
                ll_t       **ar_regex_list,
                ll_t       **ac_interval_list,
                ll_t       **ac_regex_list,
                ll_t       **at_interval_list,
                ll_t       **at_regex_list,
                alignment_t *default_alignment,
                win_t       *win,
                misc_t      *misc,
                term_t      *term)
{
  char         c;
  long         start = 1;     /* column string offset in the parsed string. */
  long         first, second; /* range starting and ending values.          */
  char        *ptr;           /* pointer to the remaining string to parse.  */
  interval_t  *interval;
  selector_t   type;
  char        *attr_str = NULL;
  attrib_t    *attr;
  attr_elem_t *attr_elem;

  /* Replace the UTF-8 string representation in the selector by */
  /* their binary values.                                       */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(str, misc->invalid_char_substitute);

  /* Get the first character to see if this is */
  /* an additive or restrictive operation.     */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (sscanf(str, "%c", &c) == 0)
    return;

  switch (c)
  {
    case 'l':
    case 'L':
      if (!win->col_mode)
      {
        *unparsed = xstrdup(str);
        return;
      }
      type = ALEFT;
      break;

    case 'r':
    case 'R':
      if (!win->col_mode)
      {
        *unparsed = xstrdup(str);
        return;
      }
      type = ARIGHT;
      break;

    case 'c':
    case 'C':
      if (!win->col_mode)
      {
        *unparsed = xstrdup(str);
        return;
      }
      type = ACENTER;
      break;

    case 'i':
    case 'I':
      type    = IN;
      *filter = INCLUDE_FILTER;
      break;

    case 'e':
    case 'E':
      type    = EX;
      *filter = EXCLUDE_FILTER;
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
      type    = IN;
      *filter = INCLUDE_FILTER;
      start   = 0;
      break;

    case 'a': /* Attribute. */
      type = ATTR;
      break;

    default:
      if (!isgraph(c))
      {
        *unparsed = strprint(str);
        return;
      }

      type    = IN;
      *filter = INCLUDE_FILTER;
      start   = 0;
      break;
  }

  /* Set ptr to the start of the interval list to parse. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  ptr = str + start;

  if (*ptr == '\0' && *default_alignment == AL_NONE)
    switch (type)
    {
      case ALEFT:
        *default_alignment = AL_LEFT;
        break;
      case ARIGHT:
        *default_alignment = AL_RIGHT;
        break;
      case ACENTER:
        *default_alignment = AL_CENTERED;
        break;
      default:
        *unparsed = xstrdup(str);
        return;
    }

  /* Scan the comma separated ranges. */
  /* '\' can be used to escape a ','. */
  /* """""""""""""""""""""""""""""""" */
  while (*ptr)
  {
    int   is_range = 0;
    char  delim1, delim2 = '\0';
    char *oldptr;
    char *colon = NULL;
    int   l_open_range; /* 1 if the range is left-open.  */
    int   r_open_range; /* 1 if the range is right-open. */

    l_open_range = r_open_range = 0;
    first = second = -1;
    oldptr         = ptr;

    while (*ptr && *ptr != ',')
    {
      if (*ptr == '-')
      {
        is_range = 1;
        ptr++;
      }
      else if (*ptr == '\\' && *(ptr + 1) != '\0' && *(ptr + 1) == ',')
        ptr += 2;
      else if (type == ATTR && *ptr == '\\' && *(ptr + 1) != '\0'
               && *(ptr + 1) == ':')
        ptr += 2;
      else if (type == ATTR && *ptr && *ptr == ':')
      {
        colon = ptr;
        ptr++;
      }
      else
        ptr++;
    }

    /* Forbid the trailing comma (ex: xxx,). */
    /* """"""""""""""""""""""""""""""""""""" */
    if (*ptr == ',' && *(ptr + 1) == '\0')
    {
      *unparsed = strprint(ptr);
      return;
    }

    /* Forbid the empty patterns (ex: xxx,,yyy). */
    /* """"""""""""""""""""""""""""""""""""""""" */
    if (oldptr == ptr)
    {
      *unparsed = strprint(ptr);
      return;
    }

    /* Mark the end of the interval found. */
    /* """"""""""""""""""""""""""""""""""" */
    if (*ptr)
      *ptr++ = '\0';

    delim1 = *(str + start);
    if (delim1 == '-')
      l_open_range = 1;

    if (type != ATTR)
    {
      if (*ptr == '\0')
        delim2 = *(ptr - 1);
      else if (ptr > str + start + 2)
        delim2 = *(ptr - 2);
    }
    else
    {
      if (colon == NULL || colon == str + start)
      {
        *unparsed = strprint(str + start);
        return;
      }

      delim2 = *(colon - 1);
    }

    /* Regex ranges are not yet supported in selectors. */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if (!isdigit(delim1) && delim1 != '-' && is_range)
    {
      *unparsed = strprint(str + start);
      return;
    }

    if (delim2 == '-')
      r_open_range = 1;

    /* Check is we have found a well described regular expression. */
    /* If the segment to parse  contains at least three characters */
    /* then delim1 and delim2 point to the first and last          */
    /* delimiter of the regular expression.                        */
    /* E.g. /abc/                                                  */
    /*      ^   ^                                                  */
    /*      |   |                                                  */
    /* delim1   delim2                                             */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (ptr > str + start + 2 && delim1 == delim2 && isgraph(delim1)
        && isgraph(delim2) && !isdigit(delim1) && !isdigit(delim2))
    {
      /* Process the regex. */
      /* """""""""""""""""" */
      switch (type)
      {
        case IN:
          parse_regex_selector_part(str + start, inc_regex_list);
          break;
        case EX:
          parse_regex_selector_part(str + start, exc_regex_list);
          break;
        case ALEFT:
          parse_regex_selector_part(str + start, al_regex_list);
          break;
        case ARIGHT:
          parse_regex_selector_part(str + start, ar_regex_list);
          break;
        case ACENTER:
          parse_regex_selector_part(str + start, ac_regex_list);
          break;
        case ATTR:
          *colon = '\0';
          /* xxxxx\0yyyy      */
          /* |    | |         */
          /* |    | attr part */
          /* |    colon       */
          /* str+start        */
          /* """""""""""""""" */

          attr = attr_new();

          /* parse the attribute part (after the colon) */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (!parse_attr(colon + 1, attr, term->colors))
          {
            *unparsed = strprint(str + start);
            free(attr);
            return;
          }

          /* Parse the regex part (before the colon) and add it to the */
          /* list of elements of type attr_elem_t.                     */
          /* In each of these elements a list of regex for the same    */
          /* attributes is updated.                                    */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (*at_regex_list == NULL) /* The list doesn't already exists. */
          {
            *at_regex_list    = ll_new();
            attr_elem_t *elem = xmalloc(sizeof(attr_elem_t));
            elem->attr        = attr;
            elem->list        = NULL;
            parse_regex_selector_part(str + start, &elem->list);
            ll_append(*at_regex_list, elem);
          }
          else
          {
            attr_elem_t e;
            ll_node_t  *node;
            e.attr = attr;
            /* Update the list of regex of the attribute attr. */
            /* """"""""""""""""""""""""""""""""""""""""""""""" */
            if ((node = ll_find(*at_regex_list, &e, attr_elem_cmp)) != NULL)
            {
              ((attr_elem_t *)(node->data))->attr = attr;
              parse_regex_selector_part(str + start,
                                        &((attr_elem_t *)(node->data))->list);
            }
            else
            {
              attr_elem_t *elem = xmalloc(sizeof(attr_elem_t));
              elem->attr        = attr;
              elem->list        = NULL;
              parse_regex_selector_part(str + start, &elem->list);
              ll_append(*at_regex_list, elem);
            }
          }

          break;
      }

      /* Adjust the start of the new interval to read in the */
      /* initial string.                                     */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      start = ptr - str;

      continue;
    }

    if (is_range)
    {
      /* We must parse 2 numbers separated by a dash. */
      /* """""""""""""""""""""""""""""""""""""""""""" */

      int rc;
      int pos;

      if (l_open_range == 0 && r_open_range == 0)
      {
        rc = sscanf(str + start, "%ld-%ld%n", &first, &second, &pos);

        if (type != ATTR)
        {
          if (rc != 2 || *(str + start + pos) != '\0')
          {
            *unparsed = strprint(str + start);
            return;
          }
        }
        else
        {
          if (*(str + start + pos) != ':')
          {
            if (rc != 2 || *(str + start + pos) != '\0')
              *unparsed = strprint(str + start + pos);
            else
              *unparsed = strprint(str + start);
            return;
          }
          else
            attr_str = xstrdup(str + start + pos + 1);
        }
      }
      else if (l_open_range == 1 && r_open_range == 0)
      {
        rc = sscanf(str + start, "-%ld%n", &second, &pos);

        if (type != ATTR)
        {
          if (rc != 1 || *(str + start + pos) != '\0')
          {
            *unparsed = strprint(str + start);
            return;
          }
        }
        else
        {
          if (*(str + start + pos) != ':')
          {
            if (rc != 1 || *(str + start + pos) != '\0')
              *unparsed = strprint(str + start + pos);
            else
              *unparsed = strprint(str + start);
            return;
          }
          else
            attr_str = xstrdup(str + start + pos + 1);
        }

        first = 1;
      }
      else if (l_open_range == 0 && r_open_range == 1)
      {
        rc = sscanf(str + start, "%ld-%n", &first, &pos);

        if (type != ATTR)
        {
          if (rc != 1 || *(str + start + pos) != '\0')
          {
            *unparsed = strprint(str + start);
            return;
          }
        }
        else
        {
          if (*(str + start + pos) != ':')
          {
            if (rc != 1 || *(str + start + pos) != '\0')
              *unparsed = strprint(str + start + pos);
            else
              *unparsed = strprint(str + start);
            return;
          }
          else
            attr_str = xstrdup(str + start + pos + 1);
        }

        second = LONG_MAX;
      }

      if (first < 1 || second < 1)
      {
        /* Both interval boundaries must be strictly positive. */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
        *unparsed = strprint(str + start);
        return;
      }

      /* Ensure that the low bound of the interval is lower or equal */
      /* to the high one. Swap them if needed.                       */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      interval = interval_new();

      if (first > second)
      {
        size_t swap;

        swap   = first;
        first  = second;
        second = swap;
      }

      interval->low  = first - 1;
      interval->high = second - 1;
    }
    else
    {
      /* We must parse a single number. */
      /* """""""""""""""""""""""""""""" */

      int rc;
      int pos;

      rc = sscanf(str + start, "%ld%n", &first, &pos);

      if (type != ATTR)
      {
        if (rc != 1 || *(str + start + pos) != '\0')
        {
          *unparsed = strprint(str + start);
          return;
        }
      }
      else
      {
        if (*(str + start + pos) != ':')
        {
          if (rc != 1 || *(str + start + pos) != '\0')
            *unparsed = strprint(str + start + pos);
          else
            *unparsed = strprint(str + start);
          return;
        }
        else
          attr_str = xstrdup(str + start + pos + 1);
      }

      interval      = interval_new();
      interval->low = interval->high = first - 1;
    }

    /* Adjust the start of the new interval to read in the */
    /* initial string.                                     */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    start = ptr - str;

    /* Add the new interval to the correct list. */
    /* """"""""""""""""""""""""""""""""""""""""" */
    switch (type)
    {
      case IN:
        if (*inc_interval_list == NULL)
          *inc_interval_list = ll_new();

        ll_append(*inc_interval_list, interval);
        break;

      case EX:
        if (*exc_interval_list == NULL)
          *exc_interval_list = ll_new();

        ll_append(*exc_interval_list, interval);
        break;

      case ALEFT:
        if (*al_interval_list == NULL)
          *al_interval_list = ll_new();

        ll_append(*al_interval_list, interval);
        break;

      case ARIGHT:
        if (*ar_interval_list == NULL)
          *ar_interval_list = ll_new();

        ll_append(*ar_interval_list, interval);
        break;

      case ACENTER:
        if (*ac_interval_list == NULL)
          *ac_interval_list = ll_new();

        ll_append(*ac_interval_list, interval);
        break;

      case ATTR:
        attr = attr_new();
        if (parse_attr(attr_str, attr, term->colors))
        {
          free(attr_str);
          attr_elem       = xmalloc(sizeof(attr_elem_t));
          attr_elem->attr = attr;

          if (*at_interval_list == NULL)
          {
            *at_interval_list = ll_new();
            attr_elem->list   = ll_new();
            ll_append(attr_elem->list, interval);
            ll_append(*at_interval_list, attr_elem);
          }
          else
          {
            ll_node_t *node;
            if ((node = ll_find(*at_interval_list, attr_elem, attr_elem_cmp))
                != NULL)
            {
              free(attr_elem);
              attr_elem = (attr_elem_t *)node->data;

              ll_append(attr_elem->list, interval);
            }
            else
            {
              attr_elem->list = ll_new();
              ll_append(attr_elem->list, interval);
              ll_append(*at_interval_list, attr_elem);
            }
          }
        }
        else
        {
          free(attr_str);
          *unparsed = strprint(str + start);
          free(attr);
          return;
        }
        break;
    }
  }

  /* If we are here, then all the intervals have be successfully parsed */
  /* Ensure that the unparsed string is empty.                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  *unparsed = xstrdup("");
}

/* ===================================================================== */
/* Parse a commas separated sequence of regular expression.              */
/* Uses to align to the left, right or center some words base on regular */
/* expressions.                                                          */
/*                                                                       */
/* str               (in)  sequence of regular expression.               */
/* al_regex_list     (out) list of RE for left-aligned words.            */
/* ar_regex_list     (out) list of RE for left-aligned words.            */
/* ac_regex_list     (out) list of RE for centered words.                */
/* default_alignment (out) new default alignment.                        */
/* misc              (in)  used by utf8_interpret.                       */
/* ===================================================================== */
void
parse_al_selectors(char        *str,
                   char       **unparsed,
                   ll_t       **al_regex_list,
                   ll_t       **ar_regex_list,
                   ll_t       **ac_regex_list,
                   alignment_t *default_alignment,
                   misc_t      *misc)
{
  char   c;
  size_t start = 1; /* column string offset in the parsed string. */
  char  *ptr;       /* pointer to the remaining string to parse.  */
  int    type;

  /* Replace the UTF-8 string representation in the selector by */
  /* their binary values.                                       */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(str, misc->invalid_char_substitute);

  /* Get the first character to see if this is */
  /* an additive or restrictive operation.     */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (sscanf(str, "%c", &c) == 0)
    return;

  type = *default_alignment;

  switch (c)
  {
    case 'l':
    case 'L':
      type = ALEFT;
      break;

    case 'r':
    case 'R':
      type = ARIGHT;
      break;

    case 'c':
    case 'C':
      type = ACENTER;
      break;

    default:
      if (!isgraph(c))
        return;

      start = 0;
      break;
  }

  /* Set ptr to the start of the interval list to parse. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  ptr = str + start;

  if (*ptr == '\0' && *default_alignment == AL_NONE)
    switch (type)
    {
      case ALEFT:
        *default_alignment = AL_LEFT;
        break;
      case ARIGHT:
        *default_alignment = AL_RIGHT;
        break;
      case ACENTER:
        *default_alignment = AL_CENTERED;
        break;
      default:
        *unparsed = xstrdup(str);
        return;
    }

  /* Scan the comma separated ranges. */
  /* '\' can be used to escape a ','. */
  /* """""""""""""""""""""""""""""""" */
  while (*ptr)
  {
    char  delim1, delim2 = '\0';
    char *oldptr;

    oldptr = ptr;
    while (*ptr && *ptr != ',')
    {
      if (*ptr == '\\' && *(ptr + 1) != '\0' && *(ptr + 1) == ',')
        ptr += 2;
      else
        ptr++;
    }

    /* Forbid the trailing comma (ex: xxx,). */
    /* """"""""""""""""""""""""""""""""""""" */
    if (*ptr == ',' && (*(ptr + 1) == '\0'))
    {
      *unparsed = xstrdup(ptr);
      return;
    }

    /* Forbid the empty patterns (ex: xxx,,yyy). */
    /* """"""""""""""""""""""""""""""""""""""""" */
    if (oldptr == ptr)
    {
      *unparsed = xstrdup(ptr);
      return;
    }

    /* Mark the end of the interval found. */
    /* """"""""""""""""""""""""""""""""""" */
    if (*ptr)
      *ptr++ = '\0';

    /* If the regex contains at least three characters then delim1 */
    /* and delim2 point to the first and last delimiter of the     */
    /* regular expression. E.g. /abc/                              */
    /*                          ^   ^                              */
    /*                          |   |                              */
    /*                     delim1   delim2                         */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    delim1 = *(str + start);
    if (*ptr == '\0')
      delim2 = *(ptr - 1);
    else if (ptr > str + start + 2)
      delim2 = *(ptr - 2);

    /* Forbid the empty patterns (ex: xxx,,yyy) and check is we have */
    /* found a well described regular expression.                    */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (ptr > str + start + 2 && delim1 == delim2 && isgraph(delim1)
        && isgraph(delim2) && !isdigit(delim1) && !isdigit(delim2))
    {
      /* Process the regex. */
      /* """""""""""""""""" */
      switch (type)
      {
        case ALEFT:
          parse_regex_selector_part(str + start, al_regex_list);
          break;
        case ARIGHT:
          parse_regex_selector_part(str + start, ar_regex_list);
          break;
        case ACENTER:
          parse_regex_selector_part(str + start, ac_regex_list);
          break;
      }

      /* Adjust the start of the new interval to read in the */
      /* initial string.                                     */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      start = ptr - str;

      continue;
    }
    else
    {
      *unparsed = xstrdup(ptr - 1);
      return;
    }
  }

  /* If we are here, then all the intervals have be successfully parsed */
  /* Ensure that the unparsed string is empty.                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  *unparsed = xstrdup("");
}

/* ========================================================= */
/* Parse the sed like string passed as argument to -S/-I/-E. */
/* This updates the sed parameter.                           */
/* Return 1 on success and 0 on error.                       */
/* ========================================================= */
int
parse_sed_like_string(sed_t *sed)
{
  char          sep;
  char         *first_sep_pos;
  char         *last_sep_pos;
  char         *buf;
  long          index;
  unsigned char icase;
  char          c;

  if (strlen(sed->pattern) < 4)
    return 0;

  /* Get the separator (the 1st character). */
  /* """""""""""""""""""""""""""""""""""""" */
  buf = xstrdup(sed->pattern);
  sep = buf[0];

  /* Space like separators are not permitted. */
  /* """""""""""""""""""""""""""""""""""""""" */
  if (isspace(sep))
    goto err;

  /* Get the extended regular expression. */
  /* """""""""""""""""""""""""""""""""""" */
  if ((first_sep_pos = strchr(buf + 1, sep)) == NULL)
    goto err;

  *first_sep_pos = '\0';

  /* Get the substitution string. */
  /* """""""""""""""""""""""""""" */
  if ((last_sep_pos = strchr(first_sep_pos + 1, sep)) == NULL)
    goto err;

  *last_sep_pos = '\0';

  sed->substitution = xstrdup(first_sep_pos + 1);

  /* Get the global indicator (trailing g)  */
  /* and the visual indicator (trailing v)  */
  /* and the stop indicator   (trailing s). */
  /* """""""""""""""""""""""""""""""""""""" */
  sed->global = sed->visual = icase = (unsigned char)0;

  index = 1;
  while ((c = *(last_sep_pos + index)) != '\0')
  {
    if (c == 'g')
      sed->global = (unsigned char)1;
    else if (c == 'v')
      sed->visual = (unsigned char)1;
    else if (c == 's')
      sed->stop = (unsigned char)1;
    else if (c == 'i')
      icase = (unsigned char)1;
    else
      goto err;

    index++;
  }

  /* Empty regular expression ? */
  /* """""""""""""""""""""""""" */
  if (*(buf + 1) == '\0')
    goto err;

  /* Compile the regular expression and abort on failure. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (regcomp(&(sed->re),
              buf + 1,
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
/* Utility function used by replace to expand the replacement string.    */
/* IN:                                                                   */
/* orig:            matching part of the original string to be replaced. */
/* repl:            string containing the replacement directives         */
/* subs_a:          array of ranges containing the start and end offset  */
/*                  of the remembered parts of the strings referenced    */
/*                  by the sequence \n where n is in [1,10].             */
/* match_start/end: offset in orig for the current matched string        */
/* subs_nb:         number of elements containing significant values in  */
/*                  the array described above.                           */
/* error:           set to 0 if no error in replacement string and to 1  */
/*                  otherwise.                                           */
/* match:           current match number in the original string.         */
/*                                                                       */
/* Return:                                                               */
/* The modified string according to the content of repl.                 */
/* ===================================================================== */
char *
build_repl_string(char    *orig,
                  char    *repl,
                  long     match_start,
                  long     match_end,
                  range_t *subs_a,
                  long     subs_nb,
                  long     match,
                  int     *error)
{
  char *str;     /*string to return */
  int   special; /* 1 if the next character is protected. */
  long  offset;  /* offset of the 1st sub corresponding to the match. */

  str     = (char *)0;
  special = 0;
  *error  = 0;

  offset = match * subs_nb;

  if (*repl == '\0')
    BUF_FIT(str, 1);
  else
    while (!*error && *repl)
    {
      switch (*repl)
      {
        case '\\':
          if (special)
          {
            BUF_PUSH(str, '\\');
            special = 0;
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

              if (delta > 0)
              {
                char *ptr = BUF_ADD(str, delta);
                memcpy(ptr, orig + subs_a[index].start, delta);
              }
            }
            else
            {
              *error = 1;
              break;
            }
            special = 0;
          }
          else
          {
            BUF_PUSH(str, *repl);
          }
          break;

        case '0':
          if (special)
          {
            long delta = match_end - match_start;

            if (delta > 0)
            {
              char *ptr = BUF_ADD(str, delta);
              memcpy(ptr, orig + match_start, delta);
            }

            special = 0;
          }
          else
          {
            BUF_PUSH(str, *repl);
            special = 0;
          }
          break;

        case '&':
          if (!special)
          {
            long delta = match_end - match_start;

            if (delta > 0)
            {
              char *ptr = BUF_ADD(str, delta);
              memcpy(ptr, orig + match_start, delta);
            }
          }
          else
          {
            BUF_PUSH(str, '&');
            special = 0;
          }
          break;

        default:
          BUF_PUSH(str, *repl);
          special = 0;
      }
      repl++;
    }

  BUF_PUSH(str, '\0');

  if (special > 0)
    *error = 1;

  return str;
}

/* ====================================================================== */
/* Replace the part of a string matched by an extender regular expression */
/* by the substitution string.                                            */
/* The regex used must have been previously compiled.                     */
/*                                                                        */
/* orig: original string                                                  */
/* sed:      composite variable containing the regular expression, a      */
/*           substitution string and various other information.           */
/* output:   destination buffer.                                          */
/*                                                                        */
/* return 1 if the replacement has been successful else 0.                */
/*                                                                        */
/* NOTE:                                                                  */
/* uses the global variable word_buffer.                                  */
/* ====================================================================== */
int
replace(char *orig, sed_t *sed)
{
  size_t match_nb   = 0; /* number of matches in the original string. */
  int    sub_nb     = 0; /* number of remembered matches in the       *
                          | original string.                          */
  size_t target_len = 0; /* length of the resulting string.           */
  size_t subs_max   = 0;

  if (*orig == '\0')
    return 1;

  range_t *matches_a = xmalloc(strlen(orig) * sizeof(range_t));
  range_t *subs_a    = xmalloc(10 * sizeof(range_t));

  const char *p = orig; /* points to the end of the previous match. */
  regmatch_t  m[10];    /* array containing the start/end offsets   *
                         | of the matches found.                    */

  while (1)
  {
    size_t i = 0;
    size_t match;       /* current match index.                        */
    size_t index   = 0; /* current char index in the original string.  */
    int    nomatch = 0; /* equals to 1 when there is no more matching. */
    char  *exp_repl;    /* expanded replacement string.                */

    if (*p == '\0')
      nomatch = 1;
    else
      nomatch = regexec(&sed->re, p, 10, m, 0);

    if (nomatch)
    {
      if (match_nb > 0)
      {
        for (index = 0; index < matches_a[0].start; index++)
          word_buffer[target_len++] = orig[index];

        for (match = 0; match < match_nb; match++)
        {
          size_t len;
          size_t end;
          int    error;

          exp_repl = build_repl_string(orig,
                                       sed->substitution,
                                       matches_a[match].start,
                                       matches_a[match].end,
                                       subs_a,
                                       subs_max,
                                       match,
                                       &error);

          if (error)
          {
            fprintf(stderr,
                    "Invalid matching group reference "
                    "in the replacement string \"%s\".\n",
                    sed->substitution);
            exit(EXIT_FAILURE);
          }

          len = strlen(exp_repl);

          my_strcpy(word_buffer + target_len, exp_repl);
          target_len += len;

          BUF_FREE(exp_repl);

          index += matches_a[match].end - matches_a[match].start;

          if (match < match_nb - 1 && sed->global)
            end = matches_a[match + 1].start;
          else
            end = strlen(orig);

          while (index < end)
            word_buffer[target_len++] = orig[index++];

          word_buffer[target_len] = '\0';

          if (!sed->global)
            break;
        }
      }
      else
      {
        my_strcpy(word_buffer, orig);
        free(matches_a);
        free(subs_a);
        return 0;
      }

      free(matches_a);
      free(subs_a);
      return nomatch;
    }

    subs_max = 0;
    for (i = 0; i < 10; i++)
    {
      size_t start;
      size_t finish;

      if (m[i].rm_so == -1)
        break;

      start  = m[i].rm_so + (p - orig);
      finish = m[i].rm_eo + (p - orig);

      if (i == 0)
      {
        matches_a[match_nb].start = start;
        matches_a[match_nb].end   = finish;
        match_nb++;

        if (match_nb > utf8_strlen(orig))
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
      p++; /* Empty match. */
  }

fail:
  free(matches_a);
  free(subs_a);
  return 0;
}

/* ============================================================ */
/* Remove all ANSI color codes from s and puts the result in d. */
/* Memory space for d must have been allocated before.          */
/* ============================================================ */
void
strip_ansi_color(char *s, toggle_t *toggles, misc_t *misc)
{
  char *p   = s;
  long  len = strlen(s);

  while (*s != '\0')
  {
    /* Remove a sequence of \x1b[...m from s. */
    /* """""""""""""""""""""""""""""""""""""" */
    if ((*s == 0x1b) && (*(s + 1) == '['))
    {
      while ((*s != '\0') && (*s++ != 'm'))
        ;
    }
    /* Convert a single \x1b in the invalid substitute character. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    else if (*s == 0x1b)
    {
      if (toggles->blank_nonprintable && len > 1)
        *s++ = ' '; /* Non printable character -> ' '. */
      else
        *s++ = misc->invalid_char_substitute;
      p++;
    }
    /* No ESC char, we can move on. */
    /* """""""""""""""""""""""""""" */
    else
      *p++ = *s++;
  }

  *p = '\0';
}

/* ================================================================== */
/* Callback function used by tst_traverse to insert the index         */
/* of a matching word index in the sorted list of the already matched */
/* words.                                                             */
/* Always succeeds and returns 1.                                     */
/* ================================================================== */
int
set_matching_flag(void *elem)
{
  ll_t *list = (ll_t *)elem;

  ll_node_t *node = list->head;
  long       target;

  while (node)
  {
    size_t pos;

    pos = *(size_t *)(node->data);
    if (word_a[pos].is_selectable)
      word_a[pos].is_matching = 1;

    target = get_sorted_array_target_pos(matching_words_da,
                                         BUF_LEN(matching_words_da),
                                         pos);
    if (target >= 0)
      BUF_INSERT(matching_words_da, target, pos);

    node = node->next;
  }
  return 1;
}

/* ======================================================================= */
/* Callback function used by tst_traverse applied to tst_word so this      */
/* function is applied on each node of this tst.                           */
/*                                                                         */
/* Each node of this tst contains a linked list storing the indexes of     */
/* the words in the input flow.                                            */
/* Each position in this list is used to:                                  */
/* - mark the word at that position as matching,                           */
/* - add this position in the sorted array matching_words_da.              */
/* Always succeeds and returns 1.                                          */
/* ======================================================================= */
int
tst_search_cb(void *elem)
{
  /* The data attached to the string in the tst is a linked list of   */
  /* position of the string in the input flow, This list is naturally */
  /* sorted.                                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ll_t *list = (ll_t *)elem;

  ll_node_t *node = list->head;
  long       target;

  while (node)
  {
    long pos;

    pos = *(long *)(node->data);

    word_a[pos].is_matching = 1;

    target = get_sorted_array_target_pos(matching_words_da,
                                         BUF_LEN(matching_words_da),
                                         pos);
    if (target >= 0)
      BUF_INSERT(matching_words_da, target, pos);

    node = node->next;
  }
  return 1; /* OK. */
}

/* **************** */
/* Input functions. */
/* **************** */

/* ===================================================================== */
/* Non delay reading of a scancode.                                      */
/* Update a scancodes buffer and return its length  in bytes.            */
/* The length of the scancode cannot reach max which must be lower of    */
/* equal than 63 which is the size minus one of the buffer array defined */
/* in main.                                                              */
/* ===================================================================== */
int
get_scancode(unsigned char *s, size_t max)
{
  int            c;
  size_t         i = 1;
  struct termios original_ts, nowait_ts;

  /* Wait until all data has been transmitted to stdin. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  tcdrain(0);

  if ((c = my_fgetc(stdin)) == EOF)
    return 0;

  /* Initialize the string with the first byte. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  memset(s, '\0', max);
  s[0] = c;

  /* 0x1b (ESC) has been found, proceed to check if additional codes */
  /* are available.                                                  */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (c == 0x1b || c > 0x80)
  {
    /* Save the terminal parameters and configure getchar() */
    /* to return immediately.                               */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    tcgetattr(0, &original_ts);
    nowait_ts = original_ts;
    nowait_ts.c_lflag &= ~ISIG;
    nowait_ts.c_cc[VMIN]  = 0;
    nowait_ts.c_cc[VTIME] = 0;

    /* tcsetattr_safe cannot fail here. */
    /* """""""""""""""""""""""""""""""" */
    (void)tcsetattr_safe(0, TCSADRAIN, &nowait_ts);

    /* Check if additional code is available after 0x1b. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if ((c = my_fgetc(stdin)) != EOF)
    {
      s[1] = c;

      i = 2;
      while (i < max && (c = my_fgetc(stdin)) != EOF)
        s[i++] = c;
    }
    else
    {
      /* There isn't a new code, this mean 0x1b came from ESC key. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    }

    /* Restore the save terminal parameters. */
    /* tcsetattr_safe cannot fail here.      */
    /* """"""""""""""""""""""""""""""""""""" */
    (void)tcsetattr_safe(0, TCSADRAIN, &original_ts);

    /* Ignore EOF when a scancode contains an escape sequence. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
    clearerr(stdin);
  }

  return i;
}

/* ============================================================ */
/* Helper function to compare to delimiters for use by ll_find. */
/* ============================================================ */
int
buffer_cmp(const void *a, const void *b)
{
  return strcmp((const char *)a, (const char *)b);
}

/* ===================================================================== */
/* Get bytes from stdin. If the first byte is the leading character of a */
/* UTF-8 glyph, the following ones are also read.                        */
/* The utf8_get_length function is used to get the number of bytes of    */
/* the character.                                                        */
/* ===================================================================== */
int
read_bytes(FILE       *input,
           char       *buffer,
           ll_t       *zapped_glyphs_list,
           langinfo_t *langinfo,
           misc_t     *misc)
{
  int byte;
  int last;
  int n;

  do
  {
    last = 0;

    /* Read the first byte. */
    /* """""""""""""""""""" */
    byte = my_fgetc(input);

    if (byte == EOF)
      return EOF;

    buffer[last++] = byte;

    /* Check if we need to read more bytes to form a sequence */
    /* and put the number of bytes of the sequence in last.   */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (langinfo->utf8 && ((n = utf8_get_length(byte)) > 1))
    {
      while (last < n && (byte = my_fgetc(input)) != EOF
             && (byte & 0xc0) == 0x80)
        buffer[last++] = byte;

      if (byte == EOF)
        return EOF;
    }

    buffer[last] = '\0';

    /* Replace an invalid UTF-8 byte sequence by a single dot.  */
    /* In this case the original sequence is lost (unsupported  */
    /* encoding).                                               */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (langinfo->utf8 && utf8_validate(buffer) != NULL)
    {
      byte = buffer[0] = misc->invalid_char_substitute;
      buffer[1]        = '\0';
    }
  } while (ll_find(zapped_glyphs_list, buffer, buffer_cmp) != NULL);

  return byte;
}

/* =======================================================================*/
/* Expand the string str by replacing all its embedded special characters */
/* by their corresponding escape sequence.                                */
/*                                                                        */
/* dest must be long enough to contain the expanded string.               */
/*                                                                        */
/* Replace also UTF-8 glyphs by the substitution character if the         */
/* current locale if not UTF-8.                                           */
/*                                                                        */
/* Return the number of resulting glyphs.                                 */
/* ====================================================================== */
size_t
expand(char       *src,
       char       *dest,
       langinfo_t *langinfo,
       toggle_t   *toggles,
       misc_t     *misc)
{
  char   c;
  int    n;
  int    all_spaces = 1;
  char  *ptr        = dest;
  size_t len        = 0;

  while ((c = *(src++)))
  {
    /* UTF-8 codepoints may take more than on character. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
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
        /* If not, ignore the bytes composing the UTF-8 glyph and replace */
        /* them with the substitution character.                          */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        do
        {
          /* Skip this byte. */
          /* ''''''''''''''' */
        } while (--n && ('\0' != *(src++)));

        *(ptr++) = misc->invalid_char_substitute;
        len++;
      }
    }
    else
      /* This is not a multibyte UTF-8 glyph. */
      /* """""""""""""""""""""""""""""""""""" */
      switch (c)
      {
        case '\a':
          *(ptr++) = '\\';
          *(ptr++) = 'a';
          goto common_code;
        case '\b':
          *(ptr++) = '\\';
          *(ptr++) = 'b';
          goto common_code;
        case '\t':
          *(ptr++) = '\\';
          *(ptr++) = 't';
          goto common_code;
        case '\n':
          *(ptr++) = '\\';
          *(ptr++) = 'n';
          goto common_code;
        case '\v':
          *(ptr++) = '\\';
          *(ptr++) = 'v';
          goto common_code;
        case '\f':
          *(ptr++) = '\\';
          *(ptr++) = 'f';
          goto common_code;
        case '\r':
          *(ptr++) = '\\';
          *(ptr++) = 'r';
          goto common_code;
        case '\\':
          *(ptr++) = '\\';
          *(ptr++) = '\\';
          goto common_code;

        common_code:
          len += 2;
          all_spaces = 0;
          break;

        default:
          if (my_isprint(c))
          {
            if (c != ' ')
              all_spaces = 0;

            *(ptr++) = c;
          }
          else
          {
            if (toggles->blank_nonprintable)
              *(ptr++) = ' '; /* Non printable character -> ' '. */
            else
            {
              *(ptr++)   = misc->invalid_char_substitute;
              all_spaces = 0;
            }
          }
          len++;
      }
  }

  /* If the word contains only spaces, replace them */
  /* by underscores so that it can be seen.         */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (toggles->show_blank_words && all_spaces)
    memset(dest, misc->blank_char_substitute, len);

  *ptr = '\0'; /* Ensure that dest has a nul terminator. */

  return len;
}

/* ====================================================================== */
/* read_word(input): return a char pointer to the next word (as a string) */
/* Accept: a FILE * for the input stream.                                 */
/* Return: a char *                                                       */
/*    On Success: the return value will point to a nul-terminated         */
/*                string.                                                 */
/*    On Failure: the return value will be set to NULL.                   */
/* ====================================================================== */
char *
read_word(FILE          *input,
          ll_t          *word_delims_list,
          ll_t          *line_delims_list,
          ll_t          *zapped_glyphs_list,
          char          *buffer,
          unsigned char *is_last,
          toggle_t      *toggles,
          langinfo_t    *langinfo,
          win_t         *win,
          limit_t       *limits,
          misc_t        *misc)
{
  char         *temp = NULL;
  int           byte;
  long          byte_count = 0; /* count chars used in current allocation. */
  long          wordsize;       /* size of current allocation in chars.    */
  unsigned char is_dquote;      /* double quote presence indicator.        */
  unsigned char is_squote;      /* single quote presence indicator.        */
  int           is_special;     /* a character is special after a \        */

  /* Skip leading delimiters. */
  /* """""""""""""""""""""""" */
  do
    byte = read_bytes(input, buffer, zapped_glyphs_list, langinfo, misc);
  while (byte != EOF && ll_find(word_delims_list, buffer, buffer_cmp) != NULL);

  if (byte == EOF)
    return NULL;

  /* Allocate initial word storage space. */
  /* """""""""""""""""""""""""""""""""""" */
  wordsize = CHARSCHUNK;
  temp     = xmalloc(wordsize);

  /* Start stashing bytes. Stop when we meet a non delimiter or EOF. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  byte_count = 0;
  is_dquote  = 0;
  is_squote  = 0;
  is_special = 0;

  while (byte != EOF)
  {
    size_t i = 0;

    if (byte_count >= limits->word_length)
    {
      fprintf(stderr,
              "The length of a word has reached the limit of "
              "%ld characters.\n",
              limits->word_length);

      exit(EXIT_FAILURE);
    }

    if (byte == '\\' && !is_special)
    {
      is_special = 1;
      goto next;
    }

    /* Parse special characters. */
    /* """"""""""""""""""""""""" */
    if (is_special)
      switch (byte)
      {
        case 'a':
          buffer[0] = byte = '\a';
          buffer[1]        = '\0';
          break;

        case 'b':
          buffer[0] = byte = '\b';
          buffer[1]        = '\0';
          break;

        case 't':
          buffer[0] = byte = '\t';
          buffer[1]        = '\0';
          break;

        case 'n':
          buffer[0] = byte = '\n';
          buffer[1]        = '\0';
          break;

        case 'v':
          buffer[0] = byte = '\v';
          buffer[1]        = '\0';
          break;

        case 'f':
          buffer[0] = byte = '\f';
          buffer[1]        = '\0';
          break;

        case 'r':
          buffer[0] = byte = '\r';
          buffer[1]        = '\0';
          break;

        case 'u':
          buffer[0] = '\\';
          buffer[1] = 'u';
          buffer[2] = '\0';
          break;

        case 'U':
          buffer[0] = '\\';
          buffer[1] = 'U';
          buffer[2] = '\0';
          break;

        case '\\':
          buffer[0] = byte = '\\';
          buffer[1]        = '\0';
          break;
      }
    else
    {
      if (!misc->ignore_quotes)
      {
        /* Manage double quotes. */
        /* """"""""""""""""""""" */
        if (byte == '"' && !is_squote)
          is_dquote ^= 1;

        /* Manage single quotes. */
        /* """"""""""""""""""""" */
        if (byte == '\'' && !is_dquote)
          is_squote ^= 1;
      }
    }

    /* Only consider delimiters when outside quotations. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!is_dquote && !is_squote)
        && ll_find(word_delims_list, buffer, buffer_cmp) != NULL)
      break;

    /* We no dot count the significant quotes. */
    /* """"""""""""""""""""""""""""""""""""""" */
    if (!misc->ignore_quotes && !is_special
        && ((byte == '"' && !is_squote) || (byte == '\'' && !is_dquote)))
    {
      is_special = 0;
      goto next;
    }

    /* Feed temp with the content of buffer. */
    /* """"""""""""""""""""""""""""""""""""" */
    while (buffer[i] != '\0')
    {
      if (byte_count >= wordsize - 1)
        temp = xrealloc(temp,
                        wordsize += (byte_count / CHARSCHUNK + 1) * CHARSCHUNK);

      *(temp + byte_count++) = buffer[i];
      i++;
    }

    is_special = 0;

  next:
    byte = read_bytes(input, buffer, zapped_glyphs_list, langinfo, misc);
  }

  /* Nul-terminate the word to make it a string. */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  *(temp + byte_count) = '\0';

  /* Replace the UTF-8 ASCII representations in the word just */
  /* read by their binary values.                             */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(temp, misc->invalid_char_substitute);

  /* Skip all field delimiters before a record delimiter. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ll_find(line_delims_list, buffer, buffer_cmp) == NULL)
  {
    byte = read_bytes(input, buffer, zapped_glyphs_list, langinfo, misc);

    while (byte != EOF && ll_find(word_delims_list, buffer, buffer_cmp) != NULL
           && ll_find(line_delims_list, buffer, buffer_cmp) == NULL)
      byte = read_bytes(input, buffer, zapped_glyphs_list, langinfo, misc);

    if (byte != EOF)
    {
      if (langinfo->utf8 && utf8_get_length(buffer[0]) > 1)
      {
        size_t pos;

        pos = strlen(buffer);
        while (pos > 0)
          my_ungetc(buffer[--pos], input);
      }
      else
        my_ungetc(byte, input);
    }
  }

  /* Mark it as the last word of a record if its sequence matches a */
  /* record delimiter.                                              */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (byte == EOF
      || ((win->col_mode || win->line_mode || win->tab_mode)
          && ll_find(line_delims_list, buffer, buffer_cmp) != NULL))
    *is_last = 1;
  else
    *is_last = 0;

  /* Remove the ANSI color escape sequences from the word. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  strip_ansi_color(temp, toggles, misc);

  return temp;
}

/* ================================================================ */
/* Convert the 8 first colors from setf/setaf coding to setaf/setf. */
/* ================================================================ */
short
color_transcode(short color)
{
  switch (color)
  {
    case 1:
      return 4;
    case 3:
      return 6;
    case 4:
      return 1;
    case 6:
      return 3;
    default:
      return color;
  }
}

/* ========================================================== */
/* Set a foreground color according to terminal capabilities. */
/* ========================================================== */
void
set_foreground_color(term_t *term, short color)
{
  if (term->color_method == CLASSIC)
  {
    if (term->has_setf)
      (void)tputs(TPARM2(set_foreground, color), 1, outch);
    if (term->has_setaf)
      (void)tputs(TPARM2(set_a_foreground, color_transcode(color)), 1, outch);
  }

  else if (term->color_method == ANSI)
  {
    if (term->has_setaf)
      (void)tputs(TPARM2(set_a_foreground, color), 1, outch);
    if (term->has_setf)
      (void)tputs(TPARM2(set_foreground, color_transcode(color)), 1, outch);
  }
}

/* ========================================================== */
/* Set a background color according to terminal capabilities. */
/* ========================================================== */
void
set_background_color(term_t *term, short color)
{
  if (term->color_method == CLASSIC)
  {
    if (term->has_setb)
      (void)tputs(TPARM2(set_background, color), 1, outch);
    if (term->has_setab)
      (void)tputs(TPARM2(set_a_background, color_transcode(color)), 1, outch);
  }

  else if (term->color_method == ANSI)
  {
    if (term->has_setab)
      (void)tputs(TPARM2(set_a_background, color), 1, outch);
    if (term->has_setb)
      (void)tputs(TPARM2(set_background, color_transcode(color)), 1, outch);
  }
}

/* ======================================================= */
/* Put a scrolling symbol at the first column of the line. */
/* ======================================================= */
void
left_margin_putp(char *s, term_t *term, win_t *win)
{
  apply_attr(term, win->shift_attr);

  /* We won't print this symbol when not in column mode. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (*s != '\0')
    fputs_safe(s, stdout);

  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
}

/* ====================================================== */
/* Put a scrolling symbol at the last column of the line. */
/* ====================================================== */
void
right_margin_putp(char       *s1,
                  char       *s2,
                  langinfo_t *langinfo,
                  term_t     *term,
                  win_t      *win,
                  long        line,
                  long        offset)
{
  apply_attr(term, win->bar_attr);

  if (term->has_hpa)
    (void)tputs(TPARM2(column_address, offset + win->max_width + 1), 1, outch);
  else if (term->has_cursor_address)
    (void)tputs(TPARM3(cursor_address,
                       term->curs_line + line - 2,
                       offset + win->max_width + 1),
                1,
                outch);
  else if (term->has_parm_right_cursor)
  {
    fputc_safe('\r', stdout);
    (void)tputs(TPARM2(parm_right_cursor, offset + win->max_width + 1),
                1,
                outch);
  }
  else
  {
    long i;

    fputc_safe('\r', stdout);
    for (i = 0; i < offset + win->max_width + 1; i++)
      (void)tputs(TPARM1(cursor_right), 1, outch);
  }

  if (langinfo->utf8)
    fputs_safe(s1, stdout);
  else
    fputs_safe(s2, stdout);

  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
}

/* ==========================================================*/
/* Displays an horizontal scroll bar below the window.       */
/* The bar if only displayed in line or column mode when the */
/* line containing the cursor is truncated.                  */
/*                                                           */
/* mark is the offset of the cursor in the bar.              */
/* ==========================================================*/
void
disp_hbar(win_t *win, term_t *term, langinfo_t *langinfo, int pos1, int pos2)
{
  int i;

  apply_attr(term, win->bar_attr);

  (void)tputs(TPARM1(clr_eol), 1, outch);

  /* Draw the left symbol arrow. */
  /* """""""""""""""""""""""""""" */
  if (langinfo->utf8)
  {
    if (pos1 == 0)
      fputs_safe(hbar_begin, stdout);
    else
      fputs_safe(hbar_left, stdout);
  }
  else
  {
    if (pos1 == 0)
      fputc_safe('<', stdout);
    else
      fputc_safe('\\', stdout);
  }

  /* Draw the line in the horizontal bar. */
  /* """""""""""""""""""""""""""""""""""" */
  if (term->has_rep && !langinfo->utf8)
    (void)tputs(TPARM3(repeat_char, '-', term->ncolumns - 2), 1, outch);
  else
  {
    char *s;

    if (langinfo->utf8)
      s = hbar_line;
    else
      s = "-";

    for (i = 0; i < term->ncolumns - 3; i++)
      fputs_safe(s, stdout);
  }

  /* Draw the cursor. */
  /* """""""""""""""" */
  if (term->has_hpa)
  {
    char *s;

    if (langinfo->utf8)
      s = hbar_curs;
    else
      s = "#";

    (void)tputs(TPARM2(column_address, pos1 + 1), 1, outch);
    for (i = pos1 + 1; i <= pos2 + 1; i++)
      fputs_safe(s, stdout);

    (void)tputs(TPARM2(column_address, term->ncolumns - 2), 1, outch);
  }
  else if (term->has_parm_right_cursor)
  {
    char *s;

    if (langinfo->utf8)
      s = hbar_curs;
    else
      s = "#";

    fputs_safe("\r", stdout);
    (void)tputs(TPARM2(parm_right_cursor, pos1 + 1), 1, outch);

    for (i = pos1; i <= pos2; i++)
      fputs_safe(s, stdout);

    fputs_safe("\r", stdout);
    (void)tputs(TPARM2(parm_right_cursor, term->ncolumns - 2), 1, outch);
  }
  else
  {
    char *s;

    if (langinfo->utf8)
      s = hbar_curs;
    else
      s = "#";

    fputs_safe("\r", stdout);
    (void)tputs(TPARM2(cursor_right, 1), 1, outch);
    for (i = 0; i < pos1; i++)
      (void)tputs(TPARM1(cursor_right), 1, outch);

    for (i = pos1 + 2; i <= pos2 + 1; i++)
      fputs_safe(s, stdout);

    for (i = pos1; i < term->ncolumns - 3 - pos2 - 2; i++)
      (void)tputs(TPARM1(cursor_right), 1, outch);
  }

  /* Draw the right symbol arrow. */
  /* """""""""""""""""""""""""""" */
  if (langinfo->utf8)
  {
    if (pos2 == term->ncolumns - 4)
      fputs_safe(hbar_end, stdout);
    else
      fputs_safe(hbar_right, stdout);
  }
  else
  {
    if (pos2 == term->ncolumns - 4)
      fputc_safe('>', stdout);
    else
      fputc_safe('/', stdout);
  }

  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
}

/* *************** */
/* Core functions. */
/* *************** */

/* ============================================================== */
/* Split the lines of the message given to -m to a linked list of */
/* lines.                                                         */
/* Also fill the maximum screen width and the maximum number      */
/* of bytes of the longest line.                                  */
/* ============================================================== */
void
get_message_lines(char *message,
                  ll_t *message_lines_list,
                  long *message_max_width,
                  long *message_max_len)
{
  char    *str;
  char    *ptr;
  char    *cr_ptr;
  long     n;
  wchar_t *w = NULL;

  *message_max_width = 0;
  *message_max_len   = 0;
  ptr                = message;

  /* For each line terminated with a EOL character. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
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

    /* If needed, update the message maximum width. */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    n = my_wcswidth((w = utf8_strtowcs(str)), utf8_strlen(str));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line.           */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = (long)strlen(str)) > *message_max_len)
      *message_max_len = n;

    ptr = cr_ptr + 1;
  }

  /* For the last line. */
  /* """""""""""""""""" */
  if (*ptr != '\0')
  {
    ll_append(message_lines_list, xstrdup(ptr));

    n = my_wcswidth((w = utf8_strtowcs(ptr)), utf8_strlen(ptr));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line.           */
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
set_win_start_end(win_t *win, long current, long last)
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

/* ======================================================================== */
/* Set the metadata associated with a word, its starting and ending         */
/* position, the line in which it is put and so on.                         */
/* Set win.start win.end and the starting and ending position of each word. */
/* This function is only called initially, when resizing the terminal and   */
/* potentially when the search function is used.                            */
/*                                                                          */
/* Returns the number of the last line built.                               */
/* ======================================================================== */
long
build_metadata(term_t *term, long count, win_t *win)
{
  long     i = 0;
  long     word_len;   /* Apparent length of the current word on the    *
                        | terminal.                                     */
  long     len  = 0;   /* Current line length.                          */
  long     last = 0;   /* Number of the last line built.                */
  long     word_width; /* Number of screen positions taken by the word. */
  long     tab_count;  /* Current number of words in the line, used in  *
                        | tab_mode.                                     */
  wchar_t *w;

  line_nb_of_word_a[0]    = 0;
  first_word_in_line_a[0] = 0;

  /* In column mode we need to calculate win->max_width, first initialize */
  /* it to 0 and increment it later in the loop.                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win->col_mode)
  {
    win->max_width      = 0;
    win->real_max_width = 0;
  }

  tab_count = 0;
  while (i < count)
  {
    /* Determine the number of screen positions taken by the word. */
    /* Note: mbstowcs will always succeed here as word_a[i].str    */
    /*       has already been utf8_validated/repaired.             */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len   = mbstowcs(NULL, word_a[i].str, 0);
    word_width = my_wcswidth((w = utf8_strtowcs(word_a[i].str)), word_len);

    /* Manage the case where the word is larger than the terminal width. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_width >= term->ncolumns - 2)
    {
      /* Shorten the word until it fits. */
      /* """"""""""""""""""""""""""""""" */
      do
      {
        word_width = my_wcswidth(w, word_len--);
      } while (word_len > 0 && word_width >= term->ncolumns - 2);
    }
    free(w);

    /* Look if there is enough remaining place on the line when not in   */
    /* column mode. Force a break if the 'is_last' flag is set in all    */
    /* modes or if we hit the max number of allowed columns in tab mode. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!win->col_mode && !win->line_mode
         && (len + word_width + 1) >= term->ncolumns - 1)
        || ((win->col_mode || win->line_mode || win->tab_mode) && i > 0
            && word_a[i - 1].is_last)
        || (win->tab_mode && win->max_cols > 0 && tab_count >= win->max_cols))
    {

      /* We must build another line. */
      /* """"""""""""""""""""""""""" */
      line_nb_of_word_a[i]       = ++last;
      first_word_in_line_a[last] = i;

      word_a[i].start = 0;

      len           = word_width + 1; /* Resets the current line length.    */
      tab_count     = 1;              /* Resets the current number of words *
                                       | in the line.                       */
      word_a[i].end = word_width - 1;
      word_a[i].mb  = word_len;
    }
    else
    {
      word_a[i].start      = len;
      word_a[i].end        = word_a[i].start + word_width - 1;
      word_a[i].mb         = word_len;
      line_nb_of_word_a[i] = last;

      len += word_width + 1; /* Increase line length.                */
      tab_count++;           /* We've seen another word in the line/ */
    }

    /* Update win->(real_)max_width if necessary. */
    /* """""""""""""""""""""""""""""""""""""""""" */
    if (len > win->max_width)
    {
      /* Update the effective line width. */
      /* '''''''''''''''''''''''''''''''' */
      if (len > term->ncolumns)
      {
        win->max_width = term->ncolumns - 2;
      }
      else
        win->max_width = len;
    }

    /* Update the real line width. */
    /* ''''''''''''''''''''''''''' */
    if (len > win->real_max_width)
      win->real_max_width = len;

    i++;
  }

  /* Set the left margin when in centered mode. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  if (!win->center || win->max_width > term->ncolumns - 2)
    win->offset = 0;
  else
    win->offset = (term->ncolumns - 2 - win->max_width) / 2;

  /* We need to recalculate win->start and win->end here */
  /* because of a possible terminal resizing.            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  set_win_start_end(win, current, last);

  return last;
}

/* ======================================================================= */
/* Helper function used by disp_word to print the cursor with the matching */
/* characters of the word highlighted.                                     */
/* ======================================================================= */
void
disp_cursor_word(long pos, win_t *win, term_t *term, int err)
{
  size_t i;
  int    att_set = 0;
  char  *p       = word_a[pos].str + daccess.flength;
  char  *np;

  /* Set the cursor attribute. */
  /* """"""""""""""""""""""""" */
  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);

  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
  if (word_a[pos].tag_id > 0)
  {
    if (marked == -1)
      apply_attr(term, win->cursor_on_tag_attr);
    else
      apply_attr(term, win->cursor_on_tag_marked_attr);
  }
  else
  {
    if (marked == -1)
      apply_attr(term, win->cursor_attr);
    else
      apply_attr(term, win->cursor_marked_attr);
  }

  for (i = 0; i < word_a[pos].mb - daccess.flength; i++)
  {
    if (BIT_ISSET(word_a[pos].bitmap, i))
    {
      if (!att_set)
      {
        att_set = 1;

        /* Set the buffer display attribute. */
        /* """"""""""""""""""""""""""""""""" */
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (err)
          apply_attr(term, win->match_err_text_attr);
        else
          apply_attr(term, win->match_text_attr);

        if (word_a[pos].tag_id > 0)
        {
          if (marked == -1)
            apply_attr(term, win->cursor_on_tag_attr);
          else
            apply_attr(term, win->cursor_on_tag_marked_attr);
        }
        else
        {
          if (marked == -1)
            apply_attr(term, win->cursor_attr);
          else
            apply_attr(term, win->cursor_marked_attr);
        }
      }
    }
    else
    {
      if (att_set)
      {
        att_set = 0;

        /* Set the search cursor attribute. */
        /* """""""""""""""""""""""""""""""" */
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (word_a[pos].tag_id > 0)
        {
          if (marked == -1)
            apply_attr(term, win->cursor_on_tag_attr);
          else
            apply_attr(term, win->cursor_on_tag_marked_attr);
        }
        else
        {
          if (marked == -1)
            apply_attr(term, win->cursor_attr);
          else
            apply_attr(term, win->cursor_marked_attr);
        }
      }
    }
    np = utf8_next(p);
    if (np == NULL)
      fputs_safe(p, stdout);
    else
      printf("%.*s", (int)(np - p), p);
    p = np;
  }
}

/* ========================================================== */
/* Helper function used by disp_word to print a matching word */
/* with the matching characters of the word highlighted.      */
/* ========================================================== */
void
disp_matching_word(long pos, win_t *win, term_t *term, int is_current, int err)
{
  size_t        i;
  int           att_set = 0;
  char         *p       = word_a[pos].str + daccess.flength;
  char         *np;
  unsigned char level = 0;

  level = word_a[pos].special_level;

  /* Set the search cursor attribute. */
  /* """""""""""""""""""""""""""""""" */
  (void)tputs(TPARM1(exit_attribute_mode), 1, outch);

  if (!is_current)
  {
    if (err)
      apply_attr(term, win->match_err_field_attr);
    else
    {
      if (level > 0)
        apply_attr(term, win->special_attr[level - 1]);
      else
        apply_attr(term, win->match_field_attr);
    }
  }
  else
  {
    if (err)
      apply_attr(term, win->search_err_field_attr);
    else
      apply_attr(term, win->search_field_attr);
  }

  if (word_a[pos].tag_id > 0)
    apply_attr(term, win->tag_attr);

  for (i = 0; i < word_a[pos].mb - daccess.flength; i++)
  {
    if (BIT_ISSET(word_a[pos].bitmap, i))
    {
      if (!att_set)
      {
        att_set = 1;

        /* Set the buffer display attribute. */
        /* """"""""""""""""""""""""""""""""" */
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (!is_current)
        {
          if (err)
            apply_attr(term, win->match_err_text_attr);
          else
            apply_attr(term, win->match_text_attr);
        }
        else
          apply_attr(term, win->search_text_attr);

        if (word_a[pos].tag_id > 0)
          apply_attr(term, win->tag_attr);
      }
    }
    else
    {
      if (att_set)
      {
        att_set = 0;

        /* Set the search cursor attribute. */
        /* """""""""""""""""""""""""""""""" */
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        if (!is_current)
        {
          if (err)
            apply_attr(term, win->match_err_field_attr);
          else
          {
            if (level > 0)
              apply_attr(term, win->special_attr[level - 1]);
            else
              apply_attr(term, win->match_field_attr);
          }
        }
        else
        {
          if (err)
            apply_attr(term, win->search_err_field_attr);
          else
            apply_attr(term, win->search_field_attr);
        }

        if (word_a[pos].tag_id > 0)
          apply_attr(term, win->tag_attr);
      }
    }

    np = utf8_next(p);
    if (np == NULL)
      fputs_safe(p, stdout);
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
disp_word(long           pos,
          search_mode_t  search_mode,
          search_data_t *search_data,
          term_t        *term,
          win_t         *win,
          toggle_t      *toggles,
          char          *tmp_word)
{
  long s = word_a[pos].start;
  long e = word_a[pos].end;
  long p;

  char *buffer = search_data->buf;

  if (pos == current)
  {
    if (search_mode != NONE)
    {
      utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);
      if (word_a[pos].is_numbered)
      {
        /* Set the direct access number attribute. */
        /* """"""""""""""""""""""""""""""""""""""" */
        apply_attr(term, win->daccess_attr);

        /* And print it. */
        /* """"""""""""" */
        fputs_safe(daccess.left, stdout);
        printf("%.*s", daccess.length, tmp_word + 1);
        fputs_safe(daccess.right, stdout);
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        fputc_safe(' ', stdout);
      }
      else if (daccess.length > 0)
      {
        /* Prints the leading spaces. */
        /* """""""""""""""""""""""""" */
        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
        printf("%.*s", daccess.flength, tmp_word);
      }

      /* Set the search cursor attribute. */
      /* """""""""""""""""""""""""""""""" */
      if (search_data->err)
        apply_attr(term, win->search_err_field_attr);
      else
        apply_attr(term, win->search_field_attr);

      /* The tab attribute must complete the attributes already set. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_a[pos].tag_id > 0)
        apply_attr(term, win->tag_attr);

      /* Print the word part. */
      /* """""""""""""""""""" */
      fputs_safe(tmp_word + daccess.flength, stdout);

      if (buffer[0] != '\0')
      {
        long i = 0;

        /* Put the cursor at the beginning of the word. */
        /* """""""""""""""""""""""""""""""""""""""""""" */
        for (i = 0; i < e - s + 1 - daccess.flength; i++)
          (void)tputs(TPARM1(cursor_left), 1, outch);

        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);

        /* Set the search cursor attribute. */
        /* """""""""""""""""""""""""""""""" */
        if (search_data->err)
          apply_attr(term, win->search_err_field_attr);
        else
          apply_attr(term, win->search_field_attr);

        disp_matching_word(pos, win, term, 1, search_data->err);
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
          /* Print the non significant part of the word. */
          /* """"""""""""""""""""""""""""""""""""""""""" */
          (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
          printf("%.*s", daccess.flength - 1, word_a[pos].str);
          (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
          fputc_safe(' ', stdout);
        }
        else
        {
          apply_attr(term, win->daccess_attr);

          /* Print the non significant part of the word. */
          /* """"""""""""""""""""""""""""""""""""""""""" */
          fputs_safe(daccess.left, stdout);
          printf("%.*s", daccess.length, word_a[pos].str + 1);
          fputs_safe(daccess.right, stdout);
          (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
          fputc_safe(' ', stdout);
        }
      }

      /* If we are not in search mode, display a normal cursor. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
      utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);
      if (word_a[pos].is_matching)
        disp_cursor_word(pos, win, term, search_data->err);
      else
      {
        if (word_a[pos].tag_id > 0)
        {
          if (marked == -1)
            apply_attr(term, win->cursor_on_tag_attr);
          else
          {
            if (pos == marked)
              apply_attr(term, win->cursor_on_marked_attr);

            apply_attr(term, win->cursor_on_tag_marked_attr);
          }
        }
        else
        {
          if (marked == -1)
            apply_attr(term, win->cursor_attr);
          else
          {
            if (pos == marked)
              apply_attr(term, win->cursor_on_marked_attr);

            apply_attr(term, win->cursor_marked_attr);
          }
        }

        fputs_safe(tmp_word + daccess.flength, stdout);
      }
    }
    (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
  }
  else
  {
    /* Display a normal word without any attribute. */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    utf8_strprefix(tmp_word, word_a[pos].str, (long)word_a[pos].mb, &p);

    /* If words are numbered, emphasis their numbers. */
    /* """""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[pos].is_numbered)
    {
      apply_attr(term, win->daccess_attr);

      fputs_safe(daccess.left, stdout);
      printf("%.*s", daccess.length, tmp_word + 1);
      fputs_safe(daccess.right, stdout);

      (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
      fputc_safe(' ', stdout);
    }
    else if (daccess.length > 0)
    {
      long i;

      /* Insert leading spaces if the word is non numbered and */
      /* padding for all words is set.                         */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
      if (daccess.padding == 'a')
        for (i = 0; i < daccess.flength; i++)
          fputc_safe(' ', stdout);
    }

    if (!word_a[pos].is_selectable)
      apply_attr(term, win->exclude_attr);
    else if (word_a[pos].special_level > 0)
    {
      unsigned char level = word_a[pos].special_level - 1;

      apply_attr(term, win->special_attr[level]);
    }
    else if (toggles->taggable && pos == marked)
      apply_attr(term, win->marked_attr);
    else
    {
      if (word_a[pos].iattr != NULL) /* is a specific attribute set? */
        apply_attr(term, *(word_a[pos].iattr));
      else
        apply_attr(term, win->include_attr);
    }

    if (word_a[pos].is_matching)
      disp_matching_word(pos, win, term, 0, search_data->err);
    else
    {
      if (word_a[pos].tag_id > 0)
        apply_attr(term, win->tag_attr);

      if ((daccess.length > 0 && daccess.padding == 'a')
          || word_a[pos].is_numbered)
        fputs_safe(tmp_word + daccess.flength, stdout);
      else
        fputs_safe(tmp_word, stdout);
    }

    (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
  }
}

/* =================================== */
/* Display a message above the window. */
/* =================================== */
void
disp_message(ll_t       *message_lines_list,
             long        message_max_width,
             long        message_max_len,
             term_t     *term,
             win_t      *win,
             langinfo_t *langinfo)
{
  ll_node_t *node;
  char      *line;
  char      *buf;
  size_t     len;
  long       size;
  long       offset;
  wchar_t   *w;
  int        n   = 0; /* Counter used to display message lines. */
  int        cut = 0; /* Will be 1 if the message is shortened. */

  sigset_t mask;

  win->message_lines = 0;

  /* Do nothing if there is no message to display. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (message_lines_list == NULL)
    return;

  /* Recalculate the number of to-be-displayed lines in the messages */
  /* if space is missing.                                            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term->nlines < 3)
    return;

  win->message_lines = message_lines_list->len;
  if (win->message_lines > term->nlines - 2)
  {
    win->message_lines = term->nlines - 2;
    win->max_lines     = term->nlines - win->message_lines - 1;
    cut                = 1;
  }
  win->message_lines++;

  /* Deactivate the periodic timer to prevent the interruptions to corrupt */
  /* screen by altering the timing of the decoding of the terminfo         */
  /* capabilities.                                                         */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  node = message_lines_list->head;
  buf  = xmalloc(message_max_len + 1);

  /* Follow the list of message lines and display each line. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (n = 1; n < win->message_lines; n++)
  {
    long i;

    line = node->data;
    len  = utf8_strlen(line);
    w    = utf8_strtowcs(line);

    /* Adjust size and len if the terminal is not large enough. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    size = my_wcswidth(w, len);
    while (len > 0 && size > term->ncolumns)
      size = my_wcswidth(w, --len);

    free(w);

    /* Compute the offset from the left screen border if -M option is set. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    offset = (term->ncolumns - message_max_width - 3) / 2;

    if (win->center && offset > 0)
      for (i = 0; i < offset; i++)
        fputc_safe(' ', stdout);

    apply_attr(term, win->message_attr);

    /* Only print the start of a line if the screen width if too small. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    utf8_strprefix(buf, line, len, &size);

    /* Print the line without the ending \n. */
    /* ''''''''''''''''''''''''''''''''''''' */
    if (n > 1 && cut && n == win->message_lines - 1)
    {
      if (langinfo->utf8)
        fputs_safe(msg_arr_down, stdout);
      else
        fputc_safe('v', stdout);
    }
    else
      printf("%s", buf);

    /* Complete the short line with spaces until it reach the */
    /* message max size.                                      */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''''''' */
    for (i = size; i < message_max_width; i++)
    {
      if (i + (offset < 0 ? 0 : offset) >= term->ncolumns)
        break;
      fputc_safe(' ', stdout);
    }

    /* Drop the attributes and print a \n. */
    /* ''''''''''''''''''''''''''''''''''' */
    if (term->nlines > 2)
    {
      (void)tputs(TPARM1(exit_attribute_mode), 1, outch);
      puts("");
    }

    node = node->next;
  }

  /* Add an empty line without attribute to separate the menu title */
  /* and the menu content.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  puts("");

  free(buf);

  /* Re-enable the periodic timer. */
  /* """"""""""""""""""""""""""""" */
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

/* ============================= */
/* Display the selection window. */
/* ============================= */
long
disp_lines(win_t         *win,
           toggle_t      *toggles,
           long           current,
           long           count,
           search_mode_t  search_mode,
           search_data_t *search_data,
           term_t        *term,
           long           last_line,
           char          *tmp_word,
           langinfo_t    *langinfo)
{
  long lines_disp;
  long i;
  char left_margin_symbol[5]; /* Placeholder for the arrow symbols. */
  long len;
  long has_vbar; /* Flag to signal the presence of the vertical bar. */
  long first_start;
  int  leftmost_start = 0; /* Starting position of the leftmost selectable *
                            | word in the window lines.                    */
  int  rightmost_end  = 0; /* Ending position of the rightmost selectable *
                            | word in the window lines.                   */

  long first_line; /* real line # on the first line of the window. */

  int row1 = 0, row2 = 0, col = 0; /* Only the rows are used to detect a *
                                    | bottom-of-page scrolling, col is   *
                                    | necessary but not required here.   */

  sigset_t mask;

  /* Disable the periodic timer to prevent the interruptions to corrupt */
  /* screen by altering the timing of the decoding of the terminfo      */
  /* capabilities.                                                      */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  left_margin_symbol[0] = ' ';
  left_margin_symbol[1] = '\0';

  lines_disp     = 1;
  first_start    = -1;
  leftmost_start = 0;

  /* Initialize the truncated lines flag. */
  /* """""""""""""""""""""""""""""""""""" */
  win->has_truncated_lines = 0;

  /* Initialize the necessity of displaying the horizontal scroll bar or not. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  win->has_hbar = 0;

  /* Initialize the selectable column guard to its maximum position. */
  /* for the first window line.                                      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  shift_right_sym_pos_a[line_nb_of_word_a[win->start] + lines_disp - 1] =
    term->ncolumns;

  (void)tputs(TPARM1(save_cursor), 1, outch);

  i = win->start; /* Index of the first word in the window. */

  /* Modify the max number of displayed lines if we do not have */
  /* enough place.                                              */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->max_lines > term->nlines - win->message_lines)
    win->max_lines = term->nlines - win->message_lines;

  if (last_line >= win->max_lines)
    has_vbar = 1;
  else
    has_vbar = 0;

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
        strcpy(left_margin_symbol, shift_left_sym);
      else
        strcpy(left_margin_symbol, "<");

      if (!toggles->no_hor_scrollbar)
        win->has_hbar = 1;
    }
    else if (toggles->hor_scrollbar)
      win->has_hbar = 1;

    if (!win->has_truncated_lines)
      win->has_truncated_lines = 1;
  }
  else
    left_margin_symbol[0] = '\0';

  /* Center the display ? */
  /* """""""""""""""""""" */
  if (win->offset > 0)
  {
    long i;
    for (i = 0; i < win->offset; i++)
      fputc_safe(' ', stdout);
  }

  left_margin_putp(left_margin_symbol, term, win);

  first_line = line_nb_of_word_a[i];

  while (len > 1 && i <= count - 1)
  {
    /* Display one word and the space or symbol following it. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[i].start >= win->first_column
        && word_a[i].end < len + win->first_column)
    {
      if (first_start < 0)
        first_start = word_a[i].start;

      disp_word(i, search_mode, search_data, term, win, toggles, tmp_word);

      /* Calculate the start offset of the last word of the line */
      /* containing the cursor in column or line mode.           */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((win->col_mode || win->line_mode)
          && line_nb_of_word_a[current] - first_line + 1 == lines_disp)
      {
        long wi;

        wi = first_word_in_line_a[lines_disp + first_line - 1];
        while (wi < current && !word_a[wi].is_selectable)
          wi++;

        leftmost_start = word_a[wi].start;

        if (lines_disp + first_line > last_line)
          wi = count - 1;
        else
          wi = first_word_in_line_a[lines_disp + first_line] - 1;

        while (!word_a[wi].is_selectable)
          wi--;

        rightmost_end = word_a[wi].end;
      }

      /* If there are more element to be displayed after the right margin */
      /* in column or line mode.                                          */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((win->col_mode || win->line_mode) && i < count - 1
          && word_a[i + 1].end >= len + win->first_column)
      {
        if (!win->has_truncated_lines)
          win->has_truncated_lines = 1;

        /* Toggle the presence of the horizontal bar if allowed and */
        /* if this line contains the cursor in column or line mode. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (!toggles->hor_scrollbar && !toggles->no_hor_scrollbar
            && line_nb_of_word_a[current] - first_line + 1 == lines_disp)
          win->has_hbar = 1; /* the line containing the cursor is *
                              | truncated in the window.          */

        apply_attr(term, win->shift_attr);

        if (langinfo->utf8)
          fputs_safe(shift_right_sym, stdout);
        else
          fputc_safe('>', stdout);

        (void)tputs(TPARM1(exit_attribute_mode), 1, outch);

        /* Adjust the selectable column guard to the column just after */
        /* the last displayed word.                                    */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        shift_right_sym_pos_a[line_nb_of_word_a[i]] = word_a[i].end + 1
                                                      - first_start + 2;
      }
      else
        /* If we want to display the gutter. */
        /* """"""""""""""""""""""""""""""""" */
        if (!word_a[i].is_last && win->col_sep
            && (win->tab_mode || win->col_mode))
        {
          long pos;

          /* Make sure that we are using the right gutter character even */
          /* if the first displayed word is * not the first of its line. */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          pos = i - first_word_in_line_a[line_nb_of_word_a[i]];

          if (pos >= win->gutter_nb) /* Use the last gutter character. */
            fputs_safe(win->gutter_a[win->gutter_nb - 1], stdout);
          else
            fputs_safe(win->gutter_a[pos], stdout);
        }
        else
          /* Else just display a space. */
          /* """""""""""""""""""""""""" */
          fputc_safe(' ', stdout);
    }

    /* Mark the line as the current line, the line containing the cursor. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (i == current)
      win->cur_line = lines_disp;

    /* Check if we must start a new line. */
    /* """""""""""""""""""""""""""""""""" */
    if (i == count - 1 || word_a[i + 1].start == 0)
    {
      (void)tputs(TPARM1(clr_eol), 1, outch);
      if (lines_disp < win->max_lines)
      {
        /* If we have more than one line to display. */
        /* """"""""""""""""""""""""""""""""""""""""" */
        if (has_vbar && !toggles->no_scrollbar
            && (lines_disp > 1 || i < count - 1))
        {
          /* Store the scroll bar column position. */
          /* """"""""""""""""""""""""""""""""""""" */
          win->sb_column = win->offset + win->max_width + 1;

          /* Display the next element of the scrollbar. */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (line_nb_of_word_a[i] == 0)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_top,
                                "\\",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_down,
                                "^",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
          }
          else if (lines_disp == 1)
            right_margin_putp(sbar_arr_up,
                              "^",
                              langinfo,
                              term,
                              win,
                              lines_disp,
                              win->offset);
          else if (line_nb_of_word_a[i] == last_line)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down,
                                "/",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_up,
                                "^",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
          }
          else if (last_line + 1 > win->max_lines
                   && (long)((float)(line_nb_of_word_a[current])
                               / (last_line + 1) * (win->max_lines - 2)
                             + 2)
                        == lines_disp)
            right_margin_putp(sbar_curs,
                              "#",
                              langinfo,
                              term,
                              win,
                              lines_disp,
                              win->offset);
          else
            right_margin_putp(sbar_line,
                              "|",
                              langinfo,
                              term,
                              win,
                              lines_disp,
                              win->offset);
        }

        /* Print a newline character if we are not at the end of */
        /* the input nor at the end of the window.               */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (i < count - 1 && lines_disp < win->max_lines)
        {
          fputc_safe('\n', stdout);

          if (win->offset > 0)
          {
            long i;
            for (i = 0; i < win->offset; i++)
              fputc_safe(' ', stdout);
          }

          left_margin_putp(left_margin_symbol, term, win);
        }

        /* We do not increment the number of lines seen after */
        /* a premature end of input.                          */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        if (i < count - 1)
        {
          lines_disp++;
          first_start = -1;

          /* Initialize the selectable column guard to its maximum position. */
          /* for the next window line.                                       */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          shift_right_sym_pos_a[line_nb_of_word_a[win->start] + lines_disp
                                - 1] = term->ncolumns;
        }

        if (win->max_lines == 1)
          break;
      }
      else if (lines_disp == win->max_lines)
      {
        /* The last line of the window has been displayed. */
        /* """"""""""""""""""""""""""""""""""""""""""""""" */
        if (has_vbar && line_nb_of_word_a[i] == last_line)
        {
          if (!toggles->no_scrollbar)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down,
                                "/",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
            else
              right_margin_putp(sbar_arr_up,
                                "^",
                                langinfo,
                                term,
                                win,
                                lines_disp,
                                win->offset);
          }
        }
        else
        {
          if (has_vbar && !toggles->no_scrollbar)
            right_margin_putp(sbar_arr_down,
                              "v",
                              langinfo,
                              term,
                              win,
                              lines_disp,
                              win->offset);
          break;
        }
      }
      else
        /* These lines were not in the widows and so we have nothing to do. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        break;
    }

    /* Next word. */
    /* """""""""" */
    i++;
  }

  /* Display the horizontal bar when needed. */
  /* """"""""""""""""""""""""""""""""""""""" */
  if (win->col_mode || win->line_mode)
  {
    /* Save again the cursor position before drawing the horizontal bar. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    get_cursor_position(&row1, &col); /* col is not needed here. */

    if (win->has_hbar)
    {
      int pos1; /* Pos. of the cursor's start in the horizontal scroll bar. */
      int pos2; /* Pos. of the cursor's end in the horizontal scroll bar.   */

      /* Note: in the following expression, rightmost_start is always   */
      /* greater then leftmost_start as a line containing a single word */
      /* cannot be truncated and have an horizontal scroll bar hence    */
      /* win->has_hbar = 0.                                             */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      pos1 = (int)(word_a[current].start - leftmost_start)
             / (float)(rightmost_end - leftmost_start) * (term->ncolumns - 4);
      pos2 = (int)(word_a[current].end - leftmost_start)
             / (float)(rightmost_end - leftmost_start) * (term->ncolumns - 4);

      if (pos2 > term->ncolumns - 4)
        pos2 = term->ncolumns
               - 4; /* just to make sure but should not happen. */

      fputs_safe("\n", stdout);
      disp_hbar(win, term, langinfo, pos1, pos2);

      /* Mark the fact that an horizontal scroll bar has been displayed */
      /* and its space allocated.                                       */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (win->hbar_displayed == 0)
        win->hbar_displayed = 1;
    }
    else if (win->hbar_displayed) /* The horizontal scroll bar has already *
                                   | been displayed, keep this space empty *
                                   | to not disturb the display.           */
    {
      fputs_safe("\n", stdout);
      (void)tputs(TPARM1(clr_eol), 1, outch);
    }

    /* Save again the cursor position again (especially the row) to detect   */
    /* an automatic scroll up when the cursor is at the bottom of the window */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    get_cursor_position(&row2, &col);
  }

  /* Update win->end, this is necessary because we only   */
  /* call build_metadata on start and on terminal resize. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (i == count)
    win->end = i - 1;
  else
    win->end = i;

  /* We restore the cursor position saved before the display of the window. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  (void)tputs(TPARM1(restore_cursor), 1, outch);

  /* Make sure the cursor is correctly moved when the horizontal scroll */
  /* bar is displayed and the window is at the bottom of the screen.    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->col_mode || win->line_mode)
  {
    if (win->has_hbar && row1 == row2) /* Screen scrolled up. */
    {
      (void)tputs(TPARM1(cursor_up), 1, outch);
      term->curs_line--;
    }
  }

  /* Re-enable the periodic timer. */
  /* """"""""""""""""""""""""""""" */
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  return lines_disp + (win->hbar_displayed ? 1 : 0);
}

/* ======================================================= */
/* Signal handler.                                         */
/* Manages SIGSEGV, SIGTERM, SIGHUP, SIGWINCH and SIGALRM. */
/* ======================================================= */
void
sig_handler(int s)
{
  switch (s)
  {
    /* Standard termination signals. */
    /* """"""""""""""""""""""""""""" */
    case SIGPIPE:
      got_sigpipe = 1;
      break;

    case SIGSEGV:
      got_sigsegv = 1;
      break;

    case SIGTERM:
      got_sigterm = 1;
      break;

    case SIGHUP:
      got_sighup = 1;
      break;

    /* Terminal resize. */
    /* """""""""""""""" */
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

      if (forgotten_timer > 0)
        forgotten_timer--;

      if (forgotten_timer == 0)
        got_forgotten_alrm = 1;

      if (help_timer > 0)
        help_timer--;

      if (help_timer == 0 && help_mode)
        got_help_alrm = 1;

      if (daccess_timer > 0)
        daccess_timer--;

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

      if (search_timer > 0)
        search_timer--;

      if (search_timer == 0 && search_mode != NONE)
        got_search_alrm = 1;

      break;
  }
}

/* ========================================================= */
/* Set new first column to display when horizontal scrolling */
/* Alter win->first_column.                                  */
/* ========================================================= */
void
set_new_first_column(win_t *win, term_t *term)
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

/* ===================================================== */
/* Restrict the matches to word ending with the pattern. */
/* ===================================================== */
void
select_ending_matches(win_t         *win,
                      term_t        *term,
                      search_data_t *search_data,
                      long          *last_line)
{
  if (BUF_LEN(matching_words_da) > 0)
  {
    long  i;
    long  index;
    long  nb;
    long *tmp;
    char *ptr;
    char *last_glyph;
    long  mb_len;

    /* Creation of an alternate array which will      */
    /* contain only the candidates having potentially */
    /* an ending pattern, if this array becomes non   */
    /* empty then it will replace the original array. */
    /* """""""""""""""""""""""""""""""""""""""""""""" */
    BUF_FREE(alt_matching_words_da);
    BUF_FIT(alt_matching_words_da, BUF_LEN(matching_words_da));

    for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
    {
      index     = matching_words_da[i];
      char *str = word_a[index].str;

      /* count the trailing blanks non counted in the bitmap. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      ptr = str + strlen(str);

      nb = 0;
      while ((ptr = utf8_prev(str, ptr)) != NULL && isblank(*ptr))
        if (ptr - str > 0)
          nb++;
        else
          break;

      if (ptr == NULL) /* str is blank or contain an invalid uft8 sequence. */
        ptr = str;

      /* NOTE: utf8_prev cannot return NULL in the previous loop */
      /* because str always contains at least one UTF-8 valid    */
      /* sequence, so does ptr.                                  */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */

      /* Check the bit corresponding to the last non blank glyph  */
      /* If set we add the index to an alternate array, if not we */
      /* clear the bitmap of the corresponding word.              */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (BIT_ISSET(word_a[index].bitmap,
                    word_a[index].mb - nb - daccess.flength - 1))
        BUF_PUSH(alt_matching_words_da, index);
      else
      {
        /* Look if the end of the word potentially contain an */
        /* ending pattern.                                    */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        if (search_mode == FUZZY)
        {
          mb_len     = mblen(ptr, 4);
          last_glyph = search_data->buf + search_data->len - mb_len;

          /* in fuzzy search mode we only look the last glyph. */
          /* """"""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(ptr, last_glyph, mb_len) == 0)
            BUF_PUSH(alt_matching_words_da, index);
          else
            memset(word_a[index].bitmap,
                   '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
        else
        {
          /* in not fuzzy search mode use all the pattern. */
          /* """"""""""""""""""""""""""""""""""""""""""""" */
          for (nb = 0; nb < search_data->mb_len - 1; nb++)
            ptr = utf8_prev(str, ptr);
          if (memcmp(ptr, search_data->buf, search_data->len) == 0)
            BUF_PUSH(alt_matching_words_da, index);
          else
            memset(word_a[index].bitmap,
                   '\0',
                   (word_a[index].mb - daccess.flength) / CHAR_BIT + 1);
        }
      }
    }

    /* Swap the normal and alt array. */
    /* """""""""""""""""""""""""""""" */
    tmp                   = matching_words_da;
    matching_words_da     = alt_matching_words_da;
    alt_matching_words_da = tmp;

    if (BUF_LEN(matching_words_da) > 0)
    {
      /* Adjust the bitmap to the ending version. */
      /* """""""""""""""""""""""""""""""""""""""" */
      update_bitmaps(search_mode, search_data, END_AFFINITY);

      current = matching_words_da[0];

      if (current < win->start || current > win->end)
        *last_line = build_metadata(term, count, win);

      /* Set new first column to display. */
      /* """""""""""""""""""""""""""""""" */
      set_new_first_column(win, term);
    }
  }
}

/* ======================================================= */
/* Restrict the matches to word starting with the pattern. */
/* ======================================================= */
void
select_starting_matches(win_t         *win,
                        term_t        *term,
                        search_data_t *search_data,
                        long          *last_line)
{
  if (BUF_LEN(matching_words_da) > 0)
  {
    long   i;
    long   index;
    size_t nb;
    long  *tmp;
    long   pos;
    char  *first_glyph;
    int    mb_len;

    BUF_FREE(alt_matching_words_da);
    BUF_FIT(alt_matching_words_da, BUF_LEN(matching_words_da));

    first_glyph = xmalloc(5);

    for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
    {
      index = matching_words_da[i];

      for (nb = 0; nb < word_a[index].mb; nb++)
        if (!isblank(*(word_a[index].str + daccess.flength + nb)))
          break;

      if (BIT_ISSET(word_a[index].bitmap, nb))
        BUF_PUSH(alt_matching_words_da, index);
      else
      {

        if (search_mode == FUZZY)
        {
          first_glyph = utf8_strprefix(first_glyph,
                                       word_a[index].str + nb + daccess.flength,
                                       1,
                                       &pos);

          mb_len = pos;

          /* in fuzzy search mode we only look the first glyph. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(search_data->buf, first_glyph, mb_len) == 0)
            BUF_PUSH(alt_matching_words_da, index);
          else
            memset(word_a[index].bitmap,
                   '\0',
                   (word_a[index].mb + nb - daccess.flength) / CHAR_BIT + 1);
        }
        else
        {
          /* in not fuzzy search mode use all the pattern. */
          /* """"""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp(search_data->buf,
                     word_a[index].str + nb,
                     search_data->len - nb)
              == 0)
            BUF_PUSH(alt_matching_words_da, index);
          else
            memset(word_a[index].bitmap,
                   '\0',
                   (word_a[index].mb + nb - daccess.flength) / CHAR_BIT + 1);
        }
      }
    }

    free(first_glyph);

    /* Swap the normal and alt array. */
    /* """""""""""""""""""""""""""""" */
    tmp                   = matching_words_da;
    matching_words_da     = alt_matching_words_da;
    alt_matching_words_da = tmp;

    if (BUF_LEN(matching_words_da) > 0)
    {
      /* Adjust the bitmap to the starting version. */
      /* """""""""""""""""""""""""""""""""""""""""" */
      update_bitmaps(search_mode, search_data, START_AFFINITY);

      current = matching_words_da[0];

      if (current < win->start || current > win->end)
        *last_line = build_metadata(term, count, win);

      /* Set new first column to display. */
      /* """""""""""""""""""""""""""""""" */
      set_new_first_column(win, term);
    }
  }
}

/* ============================= */
/* Moves the cursor to the left. */
/* ============================= */
void
move_left(win_t         *win,
          term_t        *term,
          toggle_t      *toggles,
          search_data_t *search_data,
          langinfo_t    *langinfo,
          long          *nl,
          long           last_line,
          char          *tmp_word)
{
  long old_current      = current;
  long old_start        = win->start;
  long old_first_column = win->first_column;
  long wi; /* Word index. */

  do
  {
    if (current > 0)
    {
      /* Sets the new win->start and win->end if the cursor */
      /* is at the beginning of the windows.                */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (current == win->start && win->start > 0)
      {
        for (wi = win->start - 1; wi >= 0 && word_a[wi].start != 0; wi--)
        {
        }
        win->start = wi;

        if (word_a[wi].str != NULL)
          win->start = wi;

        if (win->end < count - 1)
        {
          for (wi = win->end + 2; wi < count - 1 && word_a[wi].start != 0; wi++)
          {
          }
          if (word_a[wi].str != NULL)
            win->end = wi;
        }
      }

      /* In column mode we need to take care of the */
      /* horizontal scrolling.                      */
      /* """""""""""""""""""""""""""""""""""""""""" */
      if (win->col_mode || win->line_mode)
      {
        long pos;

        if (word_a[current].start == 0)
        {
          long len;

          len = term->ncolumns - 3;
          pos = first_word_in_line_a[line_nb_of_word_a[current - 1]];

          while (word_a[current - 1].end - win->first_column >= len)
          {
            win->first_column += word_a[pos].end - word_a[pos].start + 2;

            pos++;
          }
        }
        else if (word_a[current - 1].start < win->first_column)
          win->first_column = word_a[current - 1].start;
      }
      current--;
    }
    else
      break;
  } while (current != old_current && !word_a[current].is_selectable);

  /* The old settings need to be restored if the */
  /* new current word is not selectable.         */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  if (!word_a[current].is_selectable)
  {
    current    = old_current;
    win->start = old_start;
    if (win->col_mode || win->line_mode)
      win->first_column = old_first_column;
  }

  if (current != old_current)
    *nl = disp_lines(win,
                     toggles,
                     current,
                     count,
                     search_mode,
                     search_data,
                     term,
                     last_line,
                     tmp_word,
                     langinfo);
}

/* ============================================ */
/* Shift the content of the window to the left. */
/* Stop if the cursor will stop to be visible.  */
/* ============================================ */
void
shift_left(win_t         *win,
           term_t        *term,
           toggle_t      *toggles,
           search_data_t *search_data,
           langinfo_t    *langinfo,
           long          *nl,
           long           last_line,
           char          *tmp_word,
           long           line)
{
  long pos;
  long len;

  /* No lines to shift if not in lire or column mode. */
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win->col_mode && !win->line_mode)
    return;

  /* Do  nothing if we already are on the left side of if the line */
  /* is already fully displayed.                                   */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->first_column == 0)
    return;

  /* Find the first word to be displayed in this line. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  pos = first_word_in_line_a[line];

  while (pos < count - 1 && word_a[pos].start < win->first_column
         && word_a[pos + 1].start > 0)
    pos++;

  if (word_a[pos].start >= win->first_column)
    pos--;

  /* Make sure the word under the cursor remains visible. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  len = term->ncolumns - 3;
  if (word_a[current].end >= len + word_a[pos].start)
    return;

  win->first_column = word_a[pos].start;

  *nl = disp_lines(win,
                   toggles,
                   current,
                   count,
                   search_mode,
                   search_data,
                   term,
                   last_line,
                   tmp_word,
                   langinfo);
}

/* ============================== */
/* Moves the cursor to the right. */
/* ============================== */
void
move_right(win_t         *win,
           term_t        *term,
           toggle_t      *toggles,
           search_data_t *search_data,
           langinfo_t    *langinfo,
           long          *nl,
           long           last_line,
           char          *tmp_word)
{
  long old_current      = current;
  long old_start        = win->start;
  long old_first_column = win->first_column;
  long wi; /* word index */

  do
  {
    if (current < count - 1)
    {
      /* Sets the new win->start and win->end if the cursor */
      /* is at the end of the windows.                      */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (current == win->end && win->start < count - 1
          && win->end != count - 1)
      {
        for (wi = win->start + 1; wi < count - 1 && word_a[wi].start != 0; wi++)
        {
        }

        if (word_a[wi].str != NULL)
          win->start = wi;

        if (win->end < count - 1)
        {
          for (wi = win->end + 2; wi < count - 1 && word_a[wi].start != 0; wi++)
          {
          }
          if (word_a[wi].str != NULL)
            win->end = wi;
        }
      }

      /* In column mode we need to take care of the */
      /* horizontal scrolling.                      */
      /* """""""""""""""""""""""""""""""""""""""""" */
      if (win->col_mode || win->line_mode)
      {
        if (word_a[current].is_last)
          win->first_column = 0;
        else
        {
          long pos;
          long len;

          len = term->ncolumns - 3;

          if (word_a[current + 1].end >= len + win->first_column)
          {
            /* Find the first word to be displayed in this line. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            pos = first_word_in_line_a[line_nb_of_word_a[current]];

            while (word_a[pos].start <= win->first_column)
              pos++;

            pos--;

            /* If the new current word cannot be displayed, search */
            /* the first word in the line that can be displayed by */
            /* iterating on pos.                                   */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
            while (word_a[current + 1].end - word_a[pos].start >= len)
              pos++;

            if (word_a[pos].start > 0)
              win->first_column = word_a[pos].start;
          }
        }
      }
      current++;
    }
    else
      break;
  } while (current != old_current && !word_a[current].is_selectable);

  /* The old settings need to be restored if the */
  /* new current word is not selectable.         */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  if (!word_a[current].is_selectable)
  {
    current    = old_current;
    win->start = old_start;
    if (win->col_mode || win->line_mode)
      win->first_column = old_first_column;
  }

  if (current != old_current)
    *nl = disp_lines(win,
                     toggles,
                     current,
                     count,
                     search_mode,
                     search_data,
                     term,
                     last_line,
                     tmp_word,
                     langinfo);
}

/* =========================================== */
/* Shift content of the window to the right.   */
/* Stop if the cursor will stop to be visible. */
/* ============================================*/
void
shift_right(win_t         *win,
            term_t        *term,
            toggle_t      *toggles,
            search_data_t *search_data,
            langinfo_t    *langinfo,
            long          *nl,
            long           last_line,
            char          *tmp_word,
            long           line)
{
  long pos;

  /* No lines to shift if not in lire or column mode. */
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win->col_mode && !win->line_mode)
    return;

  /* Do nothing if we already are on the right side and all or lines */
  /* are already fully displayed.                                    */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (shift_right_sym_pos_a[line] == term->ncolumns)
  {
    int found = 0;

    /* Search for at least one line not fully displayed. */
    /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
    for (long l = line_nb_of_word_a[win->start];
         l < line_nb_of_word_a[win->start] + win->max_lines;
         l++)
    {
      if (shift_right_sym_pos_a[l] == term->ncolumns) /* fully displayed. */
        continue;

      found = 1; /* Use this line instead of line to calculate the shifting. */

      break;
    }

    /* Do nothing if all lines are fully displayed. */
    /* '''''''''''''''''''''''''''''''''''''''''''' */
    if (!found)
      return;
  }

  /* Find the first word to be displayed in this line. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  pos = first_word_in_line_a[line];

  while (pos < count - 1 && word_a[pos].start < win->first_column
         && word_a[pos + 1].start > 0)
    pos++;

  if (pos < count - 1 && word_a[pos + 1].start > 0)
    pos++;

  /* Make sure the word under the cursor remains visible. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (word_a[pos].start <= word_a[current].start)
    win->first_column = word_a[pos].start;
  else
    return;

  *nl = disp_lines(win,
                   toggles,
                   current,
                   count,
                   search_mode,
                   search_data,
                   term,
                   last_line,
                   tmp_word,
                   langinfo);
}

/* ================================================================== */
/* Get the last word of a line after it has been formed to fit in the */
/* terminal.                                                          */
/* ================================================================== */
long
get_line_last_word(long line, long last_line)
{
  if (line == last_line)
    return count - 1;

  return first_word_in_line_a[line + 1] - 1;
}

/* ==================================================================== */
/* Try to locate the best word in the target line when trying to move   */
/* the cursor upwards.                                                  */
/* returns 1 if a word has been found else 0.                           */
/* This function has the side effect to potentially change the value of */
/* the variable 'current' if an adequate word is found.                 */
/* ==================================================================== */
int
find_best_word_upwards(long last_word, long s, long e)
{
  int  found = 0;
  long index;
  long cursor;

  /* Look for the first word whose start position in the line is */
  /* less or equal to the source word starting position.         */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  cursor = last_word;
  while (word_a[cursor].start > s)
    cursor--;

  /* In case no word is eligible, keep the cursor on */
  /* the last word.                                  */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  if (cursor == last_word && word_a[cursor].start > 0)
    cursor--;

  /* Try to guess the best choice if we have multiple choices. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

  /* If the word is not selectable, try to find a selectable word */
  /* in its line.                                                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!word_a[current].is_selectable)
  {
    index = 0;
    while (word_a[current - index].start > 0
           && !word_a[current - index].is_selectable)
      index++;

    if (word_a[current - index].is_selectable)
    {
      current -= index;
      found = 1;
    }
    else
    {
      index = 0;
      while (current + index < last_word
             && !word_a[current + index].is_selectable)
        index++;

      if (word_a[current + index].is_selectable)
      {
        current += index;
        found = 1;
      }
    }
  }
  else
    found = 1;

  return found;
}

/* ==================================================================== */
/* Try to locate the best word in the target line when trying to move   */
/* the cursor downwards.                                                */
/* returns 1 if a word has been found else 0.                           */
/* This function has the side effect to potentially change the value of */
/* the variable 'current' if an adequate word is found.                 */
/* ==================================================================== */
int
find_best_word_downwards(long last_word, long s, long e)
{
  int  found = 0;
  long index;
  long cursor;

  /* Look for the first word whose start position in the line is */
  /* less or equal than the source word starting position.       */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  cursor = last_word;
  while (word_a[cursor].start > s)
    cursor--;

  /* In case no word is eligible, keep the cursor on */
  /* the last word.                                  */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  if (cursor == last_word && word_a[cursor].start > 0)
    cursor--;

  /* Try to guess the best choice if we have multiple choices. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

  /* If the word is not selectable, try to find a selectable word */
  /* in its line.                                                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!word_a[current].is_selectable)
  {
    index = 0;
    while (word_a[current - index].start > 0
           && !word_a[current - index].is_selectable)
      index++;

    if (word_a[current - index].is_selectable)
    {
      current -= index;
      found = 1;
    }
    else
    {
      index = 0;
      while (current + index < last_word
             && !word_a[current + index].is_selectable)
        index++;

      if (word_a[current + index].is_selectable)
      {
        current += index;
        found = 1;
      }
    }
  }
  else
    found = 1;

  return found;
}

/* ========================= */
/* Moves the cursor upwards. */
/* ========================= */
void
move_up(win_t         *win,
        term_t        *term,
        toggle_t      *toggles,
        search_data_t *search_data,
        langinfo_t    *langinfo,
        long          *nl,
        long           page,
        long           first_selectable,
        long           last_line,
        char          *tmp_word)
{
  long line;                  /* The line being processed (target line).    */
  long start_line;            /* The first line of the window.              */
  long cur_line;              /* The line of the cursor.                    */
  long nlines;                /* Number of line in the window.              */
  long first_selectable_line; /* the line containing the first              *
                               | selectable word.                           */
  long lines_skipped; /* The number of line between the target line and the *
                       | first line containing a selectable word in case of *
                       | exclusions.                                        */
  long last_word;     /* The last word on the target line.                  */
  long s, e;          /* Starting and ending terminal position of a word.   */
  int  found;         /* 1 if a line could be fond else 0.                  */

  /* Store the initial starting and ending positions of */
  /* the word under the cursor.                         */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  s = word_a[current].start;
  e = word_a[current].end;

  /* Identify the line number of the first window's line */
  /* and the line number of the current line.            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  start_line            = line_nb_of_word_a[win->start];
  cur_line              = line_nb_of_word_a[current];
  first_selectable_line = line_nb_of_word_a[first_selectable];
  lines_skipped         = 0;
  found                 = 0;
  nlines = win->max_lines < last_line + 1 ? win->max_lines : last_line + 1;

  /* initialise the target line. */
  /* """"""""""""""""""""""""""" */
  line = cur_line;

  /* Special case if the cursor is already in the line containing the */
  /* first selectable word.                                           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (line == first_selectable_line)
  {
    /* we can't move the cursor up but we still can try to show the */
    /* more non selectable words as we can.                         */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
    if (line <= start_line + nlines - 1 - page)
    {
      /* We are scrolling one line at a time and the cursor is not in */
      /* the last line of the window.                                 */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (start_line - page > 0)
        /* There is enough remaining line to fill a window. */
        /* '''''''''''''''''''''''''''''''''''''''''''''''' */
        start_line -= page;
      else
        /* We cannot scroll further. */
        /* ''''''''''''''''''''''''' */
        start_line = 0;
    }
    else
    {
      /* The cursor is already in the last line of the windows. */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (line >= nlines)
        /* There is enough remaining line to fill a window. */
        /* '''''''''''''''''''''''''''''''''''''''''''''''' */
        start_line = line - nlines + 1;
      else
        /* We cannot scroll further. */
        /* ''''''''''''''''''''''''' */
        start_line = 0;
    }
  }
  else
  {
    if (line - page < 0)
    {
      /* Trivial case, we are already on the first page */
      /* just jump to the first selectable line.        */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      line      = first_selectable_line;
      last_word = get_line_last_word(line, last_line);
      find_best_word_upwards(last_word, s, e);
    }
    else
    {
      /* Temporarily move up one page. */
      /* """"""""""""""""""""""""""""" */
      line -= page;

      /* The target line cannot be before the line containing the first */
      /* selectable word.                                               */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (line < first_selectable_line)
      {
        line      = first_selectable_line;
        last_word = get_line_last_word(line, last_line);
        find_best_word_upwards(last_word, s, e);
      }
      else
      {
        /* If this is not the case, search upwards for the line with a */
        /* selectable word. This line is guaranteed to exist.          */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        while (line >= first_selectable_line)
        {
          last_word = get_line_last_word(line, last_line);

          if (find_best_word_upwards(last_word, s, e))
          {
            found = 1;
            break;
          }

          line--;
          lines_skipped++;
        }
      }
    }
  }

  /* Look if we need to adjust the window to follow the cursor. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!found && start_line - page >= 0)
  {
    /* We are on the first line containing a selectable word and  */
    /* There is enough place to scroll up a page but only scrolls */
    /* up if the cursor remains in the window.                    */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (start_line + nlines - line > page)
      start_line -= page;
  }
  else if (line < start_line)
  {
    /* The target line is above the windows. */
    /* """"""""""""""""""""""""""""""""""""" */
    if (start_line - page - lines_skipped < 0)
      /* There isn't enough remaining lines to scroll up */
      /* a page size.                                    */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      start_line = 0;
    else
      start_line -= page + lines_skipped;
  }

  /* And set the new value of the starting word of the window. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  win->start = first_word_in_line_a[start_line];

  /* Set the new first column to display */
  /* """"""""""""""""""""""""""""""""""" */
  set_new_first_column(win, term);

  /* Redisplay the window. */
  /* """"""""""""""""""""" */
  *nl = disp_lines(win,
                   toggles,
                   current,
                   count,
                   search_mode,
                   search_data,
                   term,
                   last_line,
                   tmp_word,
                   langinfo);
}

/* =========================== */
/* Moves the cursor downwards. */
/* =========================== */
void
move_down(win_t         *win,
          term_t        *term,
          toggle_t      *toggles,
          search_data_t *search_data,
          langinfo_t    *langinfo,
          long          *nl,
          long           page,
          long           last_selectable,
          long           last_line,
          char          *tmp_word)
{
  long line;                 /* The line being processed (target line).     */
  long start_line;           /* The first line of the window.               */
  long cur_line;             /* The line of the cursor.                     */
  long nlines;               /* Number of line in the window.               */
  long last_selectable_line; /* the line containing the last                *
                              | selectable word.                            */
  long lines_skipped; /* The number of line between the target line and the *
                       | first line containing a selectable word in case of *
                       | exclusions.                                        */
  long last_word;     /* The last word on the target line.                  */
  long s, e;          /* Starting and ending terminal position of a word.   */
  int  found;         /* 1 if a line could be fond in the next page else 0. */

  /* Store the initial starting and ending positions of */
  /* the word under the cursor.                         */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  s = word_a[current].start;
  e = word_a[current].end;

  /* Identify the line number of the first window's line */
  /* and the line number of the current line.            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  start_line           = line_nb_of_word_a[win->start];
  cur_line             = line_nb_of_word_a[current];
  last_selectable_line = line_nb_of_word_a[last_selectable];
  lines_skipped        = 0;
  found                = 0;
  nlines = win->max_lines < last_line + 1 ? win->max_lines : last_line + 1;

  /* initialise the target line. */
  /* """"""""""""""""""""""""""" */
  line = cur_line;

  /* Special case if the cursor is already in the line containing the */
  /* last selectable word.                                            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (line == last_selectable_line)
  {
    /* we can't move the cursor down but we still can try to show the */
    /* more non selectable words as we can.                           */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
    if (line >= start_line + page)
    {
      /* We are scrolling one line at a time and the cursor is not in */
      /* the first line of the window.                                */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (start_line + page + nlines - 1 <= last_line)
        /* There is enough remaining line to fill a window. */
        /* '''''''''''''''''''''''''''''''''''''''''''''''' */
        start_line += page;
      else
        /* We cannot scroll further. */
        /* ''''''''''''''''''''''''' */
        start_line = last_line - nlines + 1;
    }
    else
    {
      /* The cursor is already in the first line of the windows. */
      /* ''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (last_line - line + 1 > nlines)
        /* There is enough remaining line to fill a window. */
        /* '''''''''''''''''''''''''''''''''''''''''''''''' */
        start_line = line;
      else
        /* We cannot scroll further. */
        /* ''''''''''''''''''''''''' */
        start_line = last_line - nlines + 1;
    }
  }
  else
  {
    /* The cursor is above the line containing the last selectable word. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (last_line - line - page < 0)
    {
      /* Trivial case, we are already on the last page */
      /* just jump to the last selectable line.        */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      line      = last_selectable_line;
      last_word = get_line_last_word(line, last_line);
      find_best_word_downwards(last_word, s, e);
    }
    else
    {
      /* Temporarily move down one page. */
      /* """"""""""""""""""""""""""""""" */
      line += page;

      /* The target line cannot be after the line containing the last */
      /* selectable word.                                             */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (line > last_selectable_line)
      {
        line      = last_selectable_line;
        last_word = get_line_last_word(line, last_line);
        find_best_word_downwards(last_word, s, e);
      }
      else
      {
        /* If this is not the case, search downwards for the line with a */
        /* selectable word. This line is guaranteed to exist.            */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        while (line <= last_selectable_line)
        {
          last_word = get_line_last_word(line, last_line);

          if (find_best_word_downwards(last_word, s, e))
          {
            found = 1;
            break;
          }

          line++;
          lines_skipped++;
        }
      }
    }

    /* Look if we need to adjust the window to follow the cursor. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (!found && start_line + nlines - 1 + page <= last_line)
    {
      /* We are on the last line containing a selectable word and     */
      /* There is enough place to scroll down a page but only scrolls */
      /* down if the cursor remains in the window.                    */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (line - start_line >= page)
        start_line += page;
    }
    else if (line > start_line + nlines - 1)
    {
      /* The target line is below the windows. */
      /* """"""""""""""""""""""""""""""""""""" */
      if (start_line + nlines + page + lines_skipped - 1 > last_line)
        /* There isn't enough remaining lines to scroll down */
        /* a page size.                                      */
        /* """"""""""""""""""""""""""""""""""""""""""""""""" */
        start_line = last_line - nlines + 1;
      else
        start_line += page + lines_skipped;
    }
  }

  /* And set the new value of the starting word of the window. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  win->start = first_word_in_line_a[start_line];

  /* Set the new first column to display. */
  /* """""""""""""""""""""""""""""""""""" */
  set_new_first_column(win, term);

  /* Redisplay the window. */
  /* """"""""""""""""""""" */
  *nl = disp_lines(win,
                   toggles,
                   current,
                   count,
                   search_mode,
                   search_data,
                   term,
                   last_line,
                   tmp_word,
                   langinfo);
}

/* ========================================= */
/* Initialize some internal data structures. */
/* ========================================= */
void
init_main_ds(attrib_t  *init_attr,
             win_t     *win,
             limit_t   *limits,
             ticker_t  *timers,
             toggle_t  *toggles,
             misc_t    *misc,
             mouse_t   *mouse,
             timeout_t *timeout,
             daccess_t *daccess)
{
  int i;

  /* Initial attribute settings. */
  /* """"""""""""""""""""""""""" */
  init_attr->is_set    = UNSET;
  init_attr->fg        = -1;
  init_attr->bg        = -1;
  init_attr->bold      = (signed char)-1;
  init_attr->dim       = (signed char)-1;
  init_attr->reverse   = (signed char)-1;
  init_attr->standout  = (signed char)-1;
  init_attr->underline = (signed char)-1;
  init_attr->italic    = (signed char)-1;
  init_attr->invis     = (signed char)-1;
  init_attr->blink     = (signed char)-1;

  /* Win fields initialization. */
  /* """""""""""""""""""""""""" */
  win->max_lines       = 5;
  win->message_lines   = 0;
  win->asked_max_lines = -1;
  win->center          = 0;
  win->max_cols        = 0;
  win->col_sep         = 0;
  win->wide            = 0;
  win->tab_mode        = 0;
  win->col_mode        = 0;
  win->line_mode       = 0;
  win->first_column    = 0;
  win->real_max_width  = 0;
  win->sb_column       = -1;
  win->hbar_displayed  = 0;

  win->cursor_attr               = *init_attr;
  win->cursor_marked_attr        = *init_attr;
  win->cursor_on_marked_attr     = *init_attr;
  win->cursor_on_tag_attr        = *init_attr;
  win->cursor_on_tag_marked_attr = *init_attr;
  win->marked_attr               = *init_attr;
  win->bar_attr                  = *init_attr;
  win->shift_attr                = *init_attr;
  win->message_attr              = *init_attr;
  win->search_field_attr         = *init_attr;
  win->search_text_attr          = *init_attr;
  win->search_err_field_attr     = *init_attr;
  win->search_err_text_attr      = *init_attr;
  win->match_field_attr          = *init_attr;
  win->match_text_attr           = *init_attr;
  win->match_err_field_attr      = *init_attr;
  win->match_err_text_attr       = *init_attr;
  win->include_attr              = *init_attr;
  win->exclude_attr              = *init_attr;
  win->tag_attr                  = *init_attr;
  win->daccess_attr              = *init_attr;

  win->next_tag_id = 1; /* No word is currently tagged. */
  win->sel_sep     = NULL;

  for (i = 0; i < 9; i++)
    win->special_attr[i] = *init_attr;

  /* Default limits initialization. */
  /* """""""""""""""""""""""""""""" */
  limits->words       = 32767;
  limits->cols        = 256;
  limits->word_length = 512;

  /* Default timers in 1/10 s. */
  /* """"""""""""""""""""""""" */
  timers->search        = 100 * FREQ / 10;
  timers->forgotten     = -1; /*  Disabled by default. */
  timers->help          = 300 * FREQ / 10;
  timers->winch         = 20 * FREQ / 10;
  timers->direct_access = 6 * FREQ / 10;

  /* Toggles initialization. */
  /* """"""""""""""""""""""" */
  toggles->del_line            = 0;
  toggles->enter_val_in_search = 0;
  toggles->no_scrollbar        = 0;
  toggles->no_hor_scrollbar    = 0;
  toggles->hor_scrollbar       = 0;
  toggles->blank_nonprintable  = 0;
  toggles->keep_spaces         = 0;
  toggles->taggable            = 0;
  toggles->autotag             = 0;
  toggles->noautotag           = 0;
  toggles->pinable             = 0;
  toggles->visual_bell         = 0;
  toggles->incremental_search  = 0;
  toggles->no_mouse            = 0;
  toggles->show_blank_words    = 0;
  toggles->cols_first          = 0;
  toggles->rows_first          = 0;

  /* Misc default values. */
  /* """""""""""""""""""" */
  misc->default_search_method   = NONE;
  misc->ignore_quotes           = 0;
  misc->invalid_char_substitute = '.';
  misc->blank_char_substitute   = '_';

  /* Mouse values. */
  /* """"""""""""" */
  mouse->button[0] = 1;
  mouse->button[1] = 2;
  mouse->button[2] = 3;

  mouse->double_click_delay = 150;

  /* Set the default timeout to 0 (no expiration). */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  timeout->initial_value = 0;
  timeout->remain        = 0;
  timeout->reached       = 0;

  /* Initialize Direct Access settings. */
  /* """""""""""""""""""""""""""""""""" */
  daccess->mode       = DA_TYPE_NONE;
  daccess->left       = xstrdup(" ");
  daccess->right      = xstrdup(")");
  daccess->alignment  = 'r';
  daccess->padding    = 'a';
  daccess->head       = 'k'; /* Keep by default. */
  daccess->length     = -2;
  daccess->flength    = 0;
  daccess->offset     = 0;
  daccess->plus       = 0;
  daccess->size       = 0;
  daccess->ignore     = 0;
  daccess->follow     = 'y';
  daccess->missing    = 'y';
  daccess->num_sep    = NULL;
  daccess->def_number = -1;
}

/* *********************************** */
/* ctxopt contexts callback functions. */
/* *********************************** */

/* ******************************** */
/* ctxopt option callback function. */
/* ******************************** */

void
help_action(char  *ctx_name,
            char  *opt_name,
            char  *param,
            int    nb_values,
            char **values,
            int    nb_opt_data,
            void **opt_data,
            int    nb_ctx_data,
            void **ctx_data)
{
  if (strcmp(ctx_name, "Columns") == 0)
    columns_help();
  else if (strcmp(ctx_name, "Lines") == 0)
    lines_help();
  else if (strcmp(ctx_name, "Tabulations") == 0)
    tabulations_help();
  else if (strcmp(ctx_name, "Tagging") == 0)
    tagging_help();
  else
    main_help();

  exit(EXIT_FAILURE);
}

void
long_help_action(char  *ctx_name,
                 char  *opt_name,
                 char  *param,
                 int    nb_values,
                 char **values,
                 int    nb_opt_data,
                 void **opt_data,
                 int    nb_ctx_data,
                 void **ctx_data)
{
  ctxopt_disp_usage(continue_after);

  printf("\nRead the manual for more information.\n");

  exit(EXIT_FAILURE);
}

void
usage_action(char  *ctx_name,
             char  *opt_name,
             char  *param,
             int    nb_values,
             char **values,
             int    nb_opt_data,
             void **opt_data,
             int    nb_ctx_data,
             void **ctx_data)
{
  ctxopt_ctx_disp_usage(ctx_name, exit_after);
}

void
lines_action(char  *ctx_name,
             char  *opt_name,
             char  *param,
             int    nb_values,
             char **values,
             int    nb_opt_data,
             void **opt_data,
             int    nb_ctx_data,
             void **ctx_data)
{
  win_t *win = opt_data[0];

  if (nb_values == 1)
    /* No need to validate if values are numeric here, they have      */
    /* already been validated by the check_integer_constraint filter. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    sscanf(values[0], "%d", &(win->asked_max_lines));
  else
    win->asked_max_lines = 0;
}

void
tab_mode_action(char  *ctx_name,
                char  *opt_name,
                char  *param,
                int    nb_values,
                char **values,
                int    nb_opt_data,
                void **opt_data,
                int    nb_ctx_data,
                void **ctx_data)
{
  win_t *win = opt_data[0];

  long max_cols;

  if (nb_values == 1)
  {
    /* No need to validate if values are numeric or out of range here,  */
    /* they have already been validated by the check_integer_constraint */
    /* and ctxopt_range_constraint filters.                             */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    sscanf(values[0], "%ld", &max_cols);
    win->max_cols = max_cols;
  }

  win->tab_mode  = 1;
  win->col_mode  = 0;
  win->line_mode = 0;
}

void
set_pattern_action(char  *ctx_name,
                   char  *opt_name,
                   char  *param,
                   int    nb_values,
                   char **values,
                   int    nb_opt_data,
                   void **opt_data,
                   int    nb_ctx_data,
                   void **ctx_data)
{
  char  **pattern = opt_data[0];
  misc_t *misc    = opt_data[1];

  *pattern = xstrdup(values[0]);
  utf8_interpret(*pattern, misc->invalid_char_substitute);
}

void
int_action(char  *ctx_name,
           char  *opt_name,
           char  *param,
           int    nb_values,
           char **values,
           int    nb_opt_data,
           void **opt_data,
           int    nb_ctx_data,
           void **ctx_data)
{
  char      **string     = opt_data[0];
  int        *shell_like = opt_data[1];
  langinfo_t *langinfo   = opt_data[2];
  misc_t     *misc       = opt_data[3];

  if (nb_values == 1)
  {
    *string = xstrdup(values[0]);
    if (!langinfo->utf8)
      utf8_sanitize(*string, misc->invalid_char_substitute);
    utf8_interpret(*string, misc->invalid_char_substitute);
  }

  *shell_like = 0;
}

void
set_string_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  char      **string   = opt_data[0];
  langinfo_t *langinfo = opt_data[1];
  misc_t     *misc     = opt_data[2];

  *string = xstrdup(values[0]);
  if (!langinfo->utf8)
    utf8_sanitize(*string, misc->invalid_char_substitute);
  utf8_interpret(*string, misc->invalid_char_substitute);
}

void
wide_mode_action(char  *ctx_name,
                 char  *opt_name,
                 char  *param,
                 int    nb_values,
                 char **values,
                 int    nb_opt_data,
                 void **opt_data,
                 int    nb_ctx_data,
                 void **ctx_data)
{
  win_t *win = opt_data[0];

  win->wide = 1;
}

void
center_mode_action(char  *ctx_name,
                   char  *opt_name,
                   char  *param,
                   int    nb_values,
                   char **values,
                   int    nb_opt_data,
                   void **opt_data,
                   int    nb_ctx_data,
                   void **ctx_data)
{
  win_t *win = opt_data[0];

  win->center = 1;
}

void
columns_select_action(char  *ctx_name,
                      char  *opt_name,
                      char  *param,
                      int    nb_values,
                      char **values,
                      int    nb_opt_data,
                      void **opt_data,
                      int    nb_ctx_data,
                      void **ctx_data)
{
  int       v;
  ll_t    **cols_selector_list = opt_data[0];
  toggle_t *toggles            = opt_data[1];

  if (*cols_selector_list == NULL)
    *cols_selector_list = ll_new();

  if (toggles->cols_first == 0 && toggles->rows_first == 0)
    toggles->cols_first = 1;

  for (v = 0; v < nb_values; v++)
    ll_append(*cols_selector_list, xstrdup(values[v]));
}

void
rows_select_action(char  *ctx_name,
                   char  *opt_name,
                   char  *param,
                   int    nb_values,
                   char **values,
                   int    nb_opt_data,
                   void **opt_data,
                   int    nb_ctx_data,
                   void **ctx_data)
{
  int       v;
  ll_t    **rows_selector_list = opt_data[0];
  win_t    *win                = opt_data[1];
  toggle_t *toggles            = opt_data[2];

  if (*rows_selector_list == NULL)
    *rows_selector_list = ll_new();

  if (toggles->cols_first == 0 && toggles->rows_first == 0)
    toggles->rows_first = 1;

  for (v = 0; v < nb_values; v++)
    ll_append(*rows_selector_list, xstrdup(values[v]));

  win->max_cols = 0; /* Disable the window column restriction. */
}

void
aligns_select_action(char  *ctx_name,
                     char  *opt_name,
                     char  *param,
                     int    nb_values,
                     char **values,
                     int    nb_opt_data,
                     void **opt_data,
                     int    nb_ctx_data,
                     void **ctx_data)
{
  int    v;
  ll_t **aligns_selector_list = opt_data[0];

  if (*aligns_selector_list == NULL)
    *aligns_selector_list = ll_new();

  for (v = 0; v < nb_values; v++)
    ll_append(*aligns_selector_list, xstrdup(values[v]));
}

void
toggle_action(char  *ctx_name,
              char  *opt_name,
              char  *param,
              int    nb_values,
              char **values,
              int    nb_opt_data,
              void **opt_data,
              int    nb_ctx_data,
              void **ctx_data)
{
  toggle_t *toggles = opt_data[0];

  if (strcmp(opt_name, "clean") == 0)
    toggles->del_line = 1;
  else if (strcmp(opt_name, "keep_spaces") == 0)
    toggles->keep_spaces = 1;
  else if (strcmp(opt_name, "visual_bell") == 0)
    toggles->visual_bell = 1;
  else if (strcmp(opt_name, "validate_in_search_mode") == 0)
    toggles->enter_val_in_search = 1;
  else if (strcmp(opt_name, "blank_nonprintable") == 0)
    toggles->blank_nonprintable = 1;
  else if (strcmp(opt_name, "no_scroll_bar") == 0)
  {
    toggles->no_scrollbar     = 1;
    toggles->no_hor_scrollbar = 1;
    toggles->hor_scrollbar    = 0;
  }
  else if (strcmp(opt_name, "no_hor_scroll_bar") == 0)
  {
    if (!toggles->no_scrollbar)
    {
      toggles->no_hor_scrollbar = 1;
      toggles->hor_scrollbar    = 0;
    }
  }
  else if (strcmp(opt_name, "hor_scroll_bar") == 0)
  {
    if (!toggles->no_scrollbar && !toggles->no_hor_scrollbar)
      toggles->hor_scrollbar = 1;
  }
  else if (strcmp(opt_name, "auto_tag") == 0)
    toggles->autotag = 1;
  else if (strcmp(opt_name, "no_auto_tag") == 0)
    toggles->noautotag = 1;
  else if (strcmp(opt_name, "incremental_search") == 0)
    toggles->incremental_search = 1;
  else if (strcmp(opt_name, "no_mouse") == 0)
    toggles->no_mouse = 1;
}

void
invalid_char_action(char  *ctx_name,
                    char  *opt_name,
                    char  *param,
                    int    nb_values,
                    char **values,
                    int    nb_opt_data,
                    void **opt_data,
                    int    nb_ctx_data,
                    void **ctx_data)
{
  misc_t *misc = opt_data[0];

  char ic = *values[0];

  if (isprint(ic))
    misc->invalid_char_substitute = ic;
  else
    misc->invalid_char_substitute = '.';
}

void
show_blank_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  toggle_t *toggles = opt_data[0];
  misc_t   *misc    = opt_data[1];

  unsigned char bc;

  toggles->show_blank_words = 1;

  if (nb_values == 1)
  {
    bc = (unsigned char)*values[0];

    if (isprint(bc))
      misc->blank_char_substitute = bc;
    else
      misc->blank_char_substitute = '_';
  }
}

void
gutter_action(char  *ctx_name,
              char  *opt_name,
              char  *param,
              int    nb_values,
              char **values,
              int    nb_opt_data,
              void **opt_data,
              int    nb_ctx_data,
              void **ctx_data)
{
  win_t      *win      = opt_data[0];
  langinfo_t *langinfo = opt_data[1];
  misc_t     *misc     = opt_data[2];

  if (nb_values == 0)
  {
    /* As there is no argument, the gutter array will only contain */
    /* a vertical bar.                                             */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    win->gutter_a = xmalloc(1 * sizeof(char *));

    if (langinfo->utf8)
      win->gutter_a[0] = xstrdup(vertical_bar);
    else
      win->gutter_a[0] = xstrdup("|");

    win->gutter_nb = 1;
  }
  else
  {
    /* The argument is used to feed the gutter array, each of its character */
    /* Will serve as gutter in a round-robin way.                           */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    long     n;
    wchar_t *w;
    long     i, offset;
    char    *gutter;

    gutter = xstrdup(values[0]);

    utf8_interpret(gutter,
                   misc->invalid_char_substitute); /* Guarantees a well    *
                                                    | formed UTF-8 string. */

    win->gutter_nb = utf8_strlen(gutter);
    win->gutter_a  = xmalloc(win->gutter_nb * sizeof(char *));

    offset = 0;

    for (i = 0; i < win->gutter_nb; i++)
    {
      int mblength;

      mblength         = utf8_get_length(*(gutter + offset));
      win->gutter_a[i] = xcalloc(1, mblength + 1);
      memcpy(win->gutter_a[i], gutter + offset, mblength);

      n = my_wcswidth((w = utf8_strtowcs(win->gutter_a[i])), 1);
      free(w);

      if (n > 1)
      {
        fprintf(stderr, "%s: A multi columns gutter is not allowed.\n", param);
        ctxopt_ctx_disp_usage(ctx_name, exit_after);
      }
      offset += mblength;
    }
    free(gutter);
  }
  win->col_sep = 1; /* Activate the gutter. */
}

void
column_mode_action(char  *ctx_name,
                   char  *opt_name,
                   char  *param,
                   int    nb_values,
                   char **values,
                   int    nb_opt_data,
                   void **opt_data,
                   int    nb_ctx_data,
                   void **ctx_data)
{
  win_t *win = opt_data[0];

  win->tab_mode  = 0;
  win->col_mode  = 1;
  win->line_mode = 0;
  win->max_cols  = 0;
}

void
line_mode_action(char  *ctx_name,
                 char  *opt_name,
                 char  *param,
                 int    nb_values,
                 char **values,
                 int    nb_opt_data,
                 void **opt_data,
                 int    nb_ctx_data,
                 void **ctx_data)
{
  win_t *win = opt_data[0];

  win->line_mode = 1;
  win->tab_mode  = 0;
  win->col_mode  = 0;
  win->max_cols  = 0;
}

void
include_re_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  int    *pattern_def_include = opt_data[0];
  char  **include_pattern     = opt_data[1];
  misc_t *misc                = opt_data[2];

  /* Set the default behaviour if not already set. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (*pattern_def_include == -1)
    *pattern_def_include = 0;

  if (*include_pattern == NULL)
    *include_pattern = concat("(", values[0], ")", (char *)0);
  else
    *include_pattern = concat(*include_pattern,
                              "|(",
                              values[0],
                              ")",
                              (char *)0);

  /* Replace the UTF-8 ASCII representations by their binary values in */
  /* inclusion patterns.                                               */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(*include_pattern, misc->invalid_char_substitute);
}

void
exclude_re_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  int    *pattern_def_include = opt_data[0];
  char  **exclude_pattern     = opt_data[1];
  misc_t *misc                = opt_data[2];

  /* Set the default behaviour if not already set. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (*pattern_def_include == -1)
    *pattern_def_include = 1;

  if (*exclude_pattern == NULL)
    *exclude_pattern = concat("(", values[0], ")", (char *)0);
  else
    *exclude_pattern = concat(*exclude_pattern,
                              "|(",
                              values[0],
                              ")",
                              (char *)0);

  /* Replace the UTF-8 ASCII representations by their binary values in */
  /* exclusion patterns.                                               */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  utf8_interpret(*exclude_pattern, misc->invalid_char_substitute);
}

void
post_subst_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  ll_t  **list = opt_data[0];
  misc_t *misc = opt_data[1];

  int i;

  if (*list == NULL)
    *list = ll_new();

  for (i = 0; i < nb_values; i++)
  {
    sed_t *sed_node;

    sed_node          = xmalloc(sizeof(sed_t));
    sed_node->pattern = xstrdup(values[i]);
    utf8_interpret(sed_node->pattern, misc->invalid_char_substitute);
    sed_node->stop = 0;
    ll_append(*list, sed_node);
  }
}

void
special_level_action(char  *ctx_name,
                     char  *opt_name,
                     char  *param,
                     int    nb_values,
                     char **values,
                     int    nb_opt_data,
                     void **opt_data,
                     int    nb_ctx_data,
                     void **ctx_data)
{
  char    **special_pattern = opt_data[0];
  win_t    *win             = opt_data[1];
  term_t   *term            = opt_data[2];
  attrib_t *init_attr       = opt_data[3];
  misc_t   *misc            = opt_data[4];

  attrib_t attr = *init_attr;
  char     opt  = param[strlen(param) - 1]; /* last character of param. */
  int      i;

  special_pattern[opt - '1'] = xstrdup(values[0]);
  utf8_interpret(special_pattern[opt - '1'], misc->invalid_char_substitute);

  /* Parse optional additional arguments. */
  /* """""""""""""""""""""""""""""""""""" */
  for (i = 1; i < nb_values; i++)
  {
    /* Colors must respect the format: <fg color>/<bg color>. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (parse_attr(values[i], &attr, term->colors))
    {
      win->special_attr[opt - '1'].is_set    = FORCED;
      win->special_attr[opt - '1'].fg        = attr.fg;
      win->special_attr[opt - '1'].bg        = attr.bg;
      win->special_attr[opt - '1'].bold      = attr.bold;
      win->special_attr[opt - '1'].dim       = attr.dim;
      win->special_attr[opt - '1'].reverse   = attr.reverse;
      win->special_attr[opt - '1'].standout  = attr.standout;
      win->special_attr[opt - '1'].underline = attr.underline;
      win->special_attr[opt - '1'].italic    = attr.italic;
      win->special_attr[opt - '1'].invis     = attr.invis;
      win->special_attr[opt - '1'].blink     = attr.blink;
    }
  }
}

void
attributes_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  win_t    *win       = opt_data[0];
  term_t   *term      = opt_data[1];
  attrib_t *init_attr = opt_data[2];

  long i, a;       /* loop index.                               */
  long offset = 0; /* nb of chars to ship to find the attribute *
                    | representation (prefix size).             */

  attrib_t  attr;
  attrib_t *attr_to_set = NULL;

  /* Flags to check if an attribute is already set */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  int inc_attr_set           = 0; /* included words.                       */
  int exc_attr_set           = 0; /* excluded words.                       */
  int cur_attr_set           = 0; /* highlighted word (cursor).            */
  int bar_attr_set           = 0; /* scroll bar.                           */
  int shift_attr_set         = 0; /* horizontal scrolling arrows.          */
  int message_attr_set       = 0; /* message (title).                      */
  int tag_attr_set           = 0; /* selected (tagged) words.              */
  int cursor_on_tag_attr_set = 0; /* selected words under the cursor.      */
  int sf_attr_set            = 0; /* currently searched field color.       */
  int st_attr_set            = 0; /* currently searched text color.        */
  int mf_attr_set            = 0; /* matching word field color.            */
  int mt_attr_set            = 0; /* matching word text color.             */
  int mfe_attr_set           = 0; /* matching word with error field color. */
  int mte_attr_set           = 0; /* matching word with error text color.  */
  int daccess_attr_set       = 0; /* Direct access text color.             */

  /* Information relatives to the attributes to be searched and set. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  struct
  {
    attrib_t *attr;
    char     *msg;
    int      *flag;
    char     *prefix;
    int       prefix_len;
  } attr_infos[] = {
    { &win->exclude_attr,
      "The exclude attribute is already set.",
      &exc_attr_set,
      "e:",
      2 },
    { &win->include_attr,
      "The include attribute is already set.",
      &inc_attr_set,
      "i:",
      2 },
    { &win->cursor_attr,
      "The cursor attribute is already set.",
      &cur_attr_set,
      "c:",
      2 },
    { &win->bar_attr,
      "The scroll bar attribute is already set.",
      &bar_attr_set,
      "b:",
      2 },
    { &win->shift_attr,
      "The shift attribute is already set.",
      &shift_attr_set,
      "s:",
      2 },
    { &win->message_attr,
      "The message attribute is already set.",
      &message_attr_set,
      "m:",
      2 },
    { &win->tag_attr,
      "The tag attribute is already set.",
      &tag_attr_set,
      "t:",
      2 },
    { &win->cursor_on_tag_attr,
      "The cursor on tagged word attribute is already set.",
      &cursor_on_tag_attr_set,
      "ct:",
      3 },
    { &win->search_field_attr,
      "The search field attribute is already set.",
      &sf_attr_set,
      "sf:",
      3 },
    { &win->search_text_attr,
      "The search text attribute is already set.",
      &st_attr_set,
      "st:",
      3 },
    { &win->search_err_field_attr,
      "The search with error field attribute is already set.",
      &sf_attr_set,
      "sfe:",
      4 },
    { &win->search_err_text_attr,
      "The search text with error attribute is already set.",
      &st_attr_set,
      "ste:",
      4 },
    { &win->match_field_attr,
      "The matching word field attribute is already set.",
      &mf_attr_set,
      "mf:",
      3 },
    { &win->match_text_attr,
      "The matching word text attribute is already set.",
      &mt_attr_set,
      "mt:",
      3 },
    { &win->match_err_field_attr,
      "The matching word with error field attribute is already set.",
      &mfe_attr_set,
      "mfe:",
      4 },
    { &win->match_err_text_attr,
      "The matching word with error text attribute is already set.",
      &mte_attr_set,
      "mte:",
      4 },
    { &win->daccess_attr,
      "The direct access tag attribute is already set.",
      &daccess_attr_set,
      "da:",
      3 },
    { NULL, NULL, NULL, NULL, 0 }
  };

  /* Parse the arguments.                                               */
  /* The syntax of the attributes has already been pre validated by the */
  /* ctxopt_re_constraint filter.                                       */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (a = 0; a < nb_values; a++)
  {
    attr = *init_attr;

    i = 0;
    while (attr_infos[i].flag != NULL)
    {
      if (strncmp(values[a], attr_infos[i].prefix, attr_infos[i].prefix_len)
          == 0)
      {
        if (*attr_infos[i].flag)
        {
          fprintf(stderr, "%s: ", param);
          fputs_safe(attr_infos[i].msg, stderr);
          fputs_safe("\n", stderr);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }

        attr_to_set         = attr_infos[i].attr;
        *attr_infos[i].flag = 1;
        offset              = attr_infos[i].prefix_len;
        break; /* We have found a matching prefix, *
                | no need to continue.             */
      }
      i++;
    }
    if (attr_infos[i].flag == NULL)
    {
      fprintf(stderr, "%s: Bad attribute prefix in %s\n", param, values[a]);
      ctxopt_ctx_disp_usage(ctx_name, exit_after);
    }

    /* Attributes must respect the format: */
    /* <fg color>/<bg color>,<styles>.     */
    /* """"""""""""""""""""""""""""""""""" */
    if (parse_attr(values[a] + offset, &attr, term->colors))
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
      attr_to_set->invis     = attr.invis;
      attr_to_set->blink     = attr.blink;
    }
    else
    {
      fprintf(stderr, "%s: Bad attribute settings %s\n", param, values[a]);
      ctxopt_ctx_disp_usage(ctx_name, exit_after);
    }
  }
}

void
limits_action(char  *ctx_name,
              char  *opt_name,
              char  *param,
              int    nb_values,
              char **values,
              int    nb_opt_data,
              void **opt_data,
              int    nb_ctx_data,
              void **ctx_data)
{
  limit_t *limits = opt_data[0];

  long l;
  long val;

  char *lim;
  char *endptr;
  char *p;

  /* Parse the arguments. */
  /* """""""""""""""""""" */
  for (l = 0; l < nb_values; l++)
  {
    errno = 0;
    lim   = values[l];
    p     = lim + 2;

    switch (*lim)
    {
      case 'l': /* word length. */
        val = strtol(p, &endptr, 0);
        if (errno == ERANGE || (val == 0 && errno != 0) || endptr == p
            || *endptr != '\0')
        {
          fprintf(stderr, "%s: Invalid word length limit. ", p);
          fprintf(stderr,
                  "Using the default value: %ld\n",
                  limits->word_length);
        }
        else
          limits->word_length = val;
        break;

      case 'w': /* max number of words. */
        val = strtol(p, &endptr, 0);
        if (errno == ERANGE || (val == 0 && errno != 0) || endptr == p
            || *endptr != '\0')
        {
          fprintf(stderr, "%s: Invalid words number limit. ", p);
          fprintf(stderr, "Using the default value: %ld\n", limits->words);
        }
        else
          limits->words = val;
        break;

      case 'c': /* max number of words. */
        val = strtol(p, &endptr, 0);
        if (errno == ERANGE || (val == 0 && errno != 0) || endptr == p
            || *endptr != '\0')
        {
          fprintf(stderr, "%s: Invalid columns number limit. ", p);
          fprintf(stderr, "Using the default value: %ld\n", limits->cols);
        }
        else
          limits->cols = val;
        break;

      default:
        fprintf(stderr, "%s: Invalid limit keyword, should be l, w or c.\n", p);
        break;
    }
  }
}

void
copyright_action(char  *ctx_name,
                 char  *opt_name,
                 char  *param,
                 int    nb_values,
                 char **values,
                 int    nb_opt_data,
                 void **opt_data,
                 int    nb_ctx_data,
                 void **ctx_data)
{
  fputs_safe("Copyright 2015 - Pierre Gentile <p.gen.progs@gmail.com> - "
             "MPL-2.0\n",
             stdout);

  exit(EXIT_SUCCESS);
}

void
version_action(char  *ctx_name,
               char  *opt_name,
               char  *param,
               int    nb_values,
               char **values,
               int    nb_opt_data,
               void **opt_data,
               int    nb_ctx_data,
               void **ctx_data)
{
  fputs_safe("Version: " VERSION "\n", stdout);

  exit(EXIT_SUCCESS);
}

void
timeout_action(char  *ctx_name,
               char  *opt_name,
               char  *param,
               int    nb_values,
               char **values,
               int    nb_opt_data,
               void **opt_data,
               int    nb_ctx_data,
               void **ctx_data)
{
  misc_t *misc = opt_data[0];

  if (strcmp(opt_name, "hidden_timeout") == 0)
    quiet_timeout = 1;

  if (strprefix("current", values[0]))
    timeout.mode = CURRENT;
  else if (strprefix("quit", values[0]))
    timeout.mode = QUIT;
  else if (strprefix("word", values[0]))
  {
    if (nb_values == 3)
    {
      timeout.mode = WORD;
      timeout_word = xstrdup(values[1]);
      utf8_interpret(timeout_word, misc->invalid_char_substitute);
    }
    else
    {
      fprintf(stderr, "%s: Missing timeout selected word or delay.\n", param);
      ctxopt_ctx_disp_usage(ctx_name, exit_after);
    }
  }

  if (sscanf(values[nb_values - 1], "%5u", &timeout.initial_value) == 1)
  {
    timeout.initial_value *= FREQ;
    timeout.remain = timeout.initial_value;
  }
  else
  {
    fprintf(stderr, "%s: Invalid timeout delay.\n", param);
    ctxopt_ctx_disp_usage(ctx_name, exit_after);
  }
}

void
tag_mode_action(char  *ctx_name,
                char  *opt_name,
                char  *param,
                int    nb_values,
                char **values,
                int    nb_opt_data,
                void **opt_data,
                int    nb_ctx_data,
                void **ctx_data)
{
  toggle_t *toggles = opt_data[0];
  win_t    *win     = opt_data[1];
  misc_t   *misc    = opt_data[2];

  toggles->taggable = 1;

  if (nb_values == 1)
  {
    win->sel_sep = xstrdup(values[0]);
    utf8_interpret(win->sel_sep, misc->invalid_char_substitute);
  }
}

void
pin_mode_action(char  *ctx_name,
                char  *opt_name,
                char  *param,
                int    nb_values,
                char **values,
                int    nb_opt_data,
                void **opt_data,
                int    nb_ctx_data,
                void **ctx_data)
{
  toggle_t *toggles = opt_data[0];
  win_t    *win     = opt_data[1];
  misc_t   *misc    = opt_data[2];

  toggles->taggable = 1;
  toggles->pinable  = 1;

  if (nb_values == 1)
  {
    win->sel_sep = xstrdup(values[0]);
    utf8_interpret(win->sel_sep, misc->invalid_char_substitute);
  }
}

void
search_method_action(char  *ctx_name,
                     char  *opt_name,
                     char  *param,
                     int    nb_values,
                     char **values,
                     int    nb_opt_data,
                     void **opt_data,
                     int    nb_ctx_data,
                     void **ctx_data)
{
  misc_t *misc = opt_data[0];

  if (strprefix("prefix", values[0]))
    misc->default_search_method = PREFIX;
  else if (strprefix("fuzzy", values[0]))
    misc->default_search_method = FUZZY;
  else if (strprefix("substring", values[0]))
    misc->default_search_method = SUBSTRING;
  else
  {
    fprintf(stderr, "%s: Bad search method: %s\n", param, values[0]);
    ctxopt_ctx_disp_usage(ctx_name, exit_after);
  }
}

void
auto_da_action(char  *ctx_name,
               char  *opt_name,
               char  *param,
               int    nb_values,
               char **values,
               int    nb_opt_data,
               void **opt_data,
               int    nb_ctx_data,
               void **ctx_data)
{
  char **daccess_pattern = opt_data[0];
  int    i;

  if (nb_values == 0)
  {
    if (daccess.missing == 'y' || ((daccess.mode & DA_TYPE_POS) == 0))
    {
      if (*daccess_pattern == NULL)
      {
        *daccess_pattern = xstrdup("(.)");
        daccess.mode |= DA_TYPE_AUTO; /* Auto. */
      }
      else
        *daccess_pattern = concat(*daccess_pattern, "|(.)", (char *)0);
    }
  }
  else
    for (i = 0; i < nb_values; i++)
    {
      char *value;

      if (*values[i] == '\0'
          && (daccess.missing == 'y' || ((daccess.mode & DA_TYPE_POS) == 0)))
        value = ".";
      else
        value = values[i];

      if (*daccess_pattern == NULL)
      {
        *daccess_pattern = concat("(", value, ")", (char *)0);
        daccess.mode |= DA_TYPE_AUTO; /* Auto. */
      }
      else
        *daccess_pattern = concat(*daccess_pattern,
                                  "|(",
                                  value,
                                  ")",
                                  (char *)0);
    }

  if (daccess.def_number < 0)
  {
    if (strcmp(param, "-N") == 0)
      daccess.def_number = 0; /* Words are unnumbered by default. */
    else
      daccess.def_number = 1; /* Words are numbered by default.   */
  }
}

void
field_da_number_action(char  *ctx_name,
                       char  *opt_name,
                       char  *param,
                       int    nb_values,
                       char **values,
                       int    nb_opt_data,
                       void **opt_data,
                       int    nb_ctx_data,
                       void **ctx_data)
{
  daccess.mode |= DA_TYPE_POS;
}

void
da_options_action(char  *ctx_name,
                  char  *opt_name,
                  char  *param,
                  int    nb_values,
                  char **values,
                  int    nb_opt_data,
                  void **opt_data,
                  int    nb_ctx_data,
                  void **ctx_data)
{
  long   *daccess_index = opt_data[0];
  misc_t *misc          = opt_data[1];

  int      pos;
  wchar_t *w;
  int      n;
  int      i;

  /* Parse optional additional arguments.                              */
  /* The syntax of the arguments has already been pre validated by the */
  /* ctxopt_re_constraint filter.                                      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (i = 0; i < nb_values; i++)
  {
    char *value = values[i];

    switch (*value)
    {
      case 'l': /* Left char .*/
        free(daccess.left);

        daccess.left = xstrdup(value + 2);
        utf8_interpret(daccess.left, misc->invalid_char_substitute);

        if (utf8_strlen(daccess.left) != 1)
        {
          fprintf(stderr, "%s: Too many characters after l:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }

        n = my_wcswidth((w = utf8_strtowcs(daccess.left)), 1);
        free(w);

        if (n > 1)
        {
          fprintf(stderr,
                  "%s: A multi columns character is not allowed "
                  "after l:\n",
                  param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'r': /* Right char. */
        free(daccess.right);

        daccess.right = xstrdup(value + 2);
        utf8_interpret(daccess.right, misc->invalid_char_substitute);

        if (utf8_strlen(daccess.right) != 1)
        {
          fprintf(stderr, "%s: Too many characters after r:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }

        n = my_wcswidth((w = utf8_strtowcs(daccess.right)), 1);
        free(w);

        if (n > 1)
        {
          fprintf(stderr,
                  "%s: A multi columns character is not allowed "
                  "after r:\n",
                  param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'a': /* Alignment. */
        if (strprefix("left", value + 2))
          daccess.alignment = 'l';
        else if (strprefix("right", value + 2))
          daccess.alignment = 'r';
        else
        {
          fprintf(stderr,
                  "%s: The value after a: must be "
                  "l(eft) or r(ight)\n",
                  param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'p': /* Padding. */
        if (strprefix("all", value + 2))
          daccess.padding = 'a';
        else if (strprefix("included", value + 2))
          daccess.padding = 'i';
        else
        {
          fprintf(stderr, "%s: Bad value after p:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'w': /* Width. */
        if (sscanf(value + 2, "%d%n", &daccess.length, &pos) != 1)
        {
          fprintf(stderr, "%s: Bad value after w:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        if (value[pos + 2] != '\0')
        {
          fprintf(stderr, "%s: Bad value after w:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        if (daccess.length <= 0 || daccess.length > 5)
        {
          fprintf(stderr, "%s: w sub-option must be between 1 and 5\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'o': /* Start offset. */
        if (sscanf(value + 2, "%zu%n+", &daccess.offset, &pos) == 1)
        {
          if (value[pos + 2] == '+')
          {
            daccess.plus = 1;

            if (value[pos + 3] != '\0')
            {
              fprintf(stderr, "%s: Bad value after o:\n", param);
              ctxopt_ctx_disp_usage(ctx_name, exit_after);
            }
          }
          else if (value[pos + 2] != '\0')
          {
            fprintf(stderr, "%s: Bad value after o:\n", param);
            ctxopt_ctx_disp_usage(ctx_name, exit_after);
          }
        }
        else
        {
          fprintf(stderr, "%s: Bad value after o:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }

        break;

      case 'n': /* Number of digits to extract. */
        if (sscanf(value + 2, "%d%n", &daccess.size, &pos) != 1)
        {
          fprintf(stderr, "%s: Bad value after n:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        if (value[pos + 2] != '\0')
        {
          fprintf(stderr, "%s: Bad value after n:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        if (daccess.size <= 0 || daccess.size > 5)
        {
          fprintf(stderr, "n sub-option must have a value between 1 and 5.\n");
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'i': /* Number of UTF-8 glyphs to ignore after the *
                 | selector to extract.                       */
        if (sscanf(value + 2, "%zu%n", &daccess.ignore, &pos) != 1)
        {
          fprintf(stderr, "%s: Bad value after i:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        if (value[pos + 2] != '\0')
        {
          fprintf(stderr, "%s: Bad value after i:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'f': /* Follow. */
        if (strprefix("yes", value + 2))
          daccess.follow = 'y';
        else if (strprefix("no", value + 2))
          daccess.follow = 'n';
        else
        {
          fprintf(stderr, "%s: Bad value after f:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'm': /* Possibly number missing embedded numbers. */
        if (strprefix("yes", value + 2))
          daccess.missing = 'y';
        else if (strprefix("no", value + 2))
          daccess.missing = 'n';
        else
        {
          fprintf(stderr, "%s: Bad value after m:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 'd': /* Decorate. */
        free(daccess.num_sep);

        daccess.num_sep = xstrdup(value + 2);
        utf8_interpret(daccess.num_sep, misc->invalid_char_substitute);

        if (utf8_strlen(daccess.num_sep) != 1)
        {
          fprintf(stderr, "%s: Too many characters after d:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }

        n = my_wcswidth((w = utf8_strtowcs(daccess.num_sep)), 1);
        free(w);

        if (n > 1)
        {
          fprintf(stderr,
                  "%s: A multi columns separator is not allowed "
                  "after d:\n",
                  param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      case 's': /* Start index. */
      {
        long index;

        if (sscanf(value + 2, "%ld%ln", daccess_index, &index) == 1)
        {
          if (*daccess_index < 0 || *(value + 2 + index) != '\0')
            *daccess_index = 1;
        }
        else
        {
          fprintf(stderr, "%s: Invalid first index after s:\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
      }
      break;

      case 'h': /* Head. */
        if (strprefix("trim", value + 2))
          daccess.head = 't';
        else if (strprefix("cut", value + 2))
          daccess.head = 'c';
        else if (strprefix("keep", value + 2))
          daccess.head = 'k';
        else
        {
          fprintf(stderr, "%s: Bad value after :h\n", param);
          ctxopt_ctx_disp_usage(ctx_name, exit_after);
        }
        break;

      default:
      {
        fprintf(stderr, "%s: Bad sub-command: %s\n", param, value);
        ctxopt_ctx_disp_usage(ctx_name, exit_after);
      }
    }

    if (daccess.length <= 0 || daccess.length > 5)
      daccess.length = -2; /* special value -> auto. */
  }
}

void
ignore_quotes_action(char  *ctx_name,
                     char  *opt_name,
                     char  *param,
                     int    nb_values,
                     char **values,
                     int    nb_opt_data,
                     void **opt_data,
                     int    nb_ctx_data,
                     void **ctx_data)
{
  misc_t *misc = opt_data[0];

  misc->ignore_quotes = 1;
}

void
forgotten_action(char  *ctx_name,
                 char  *opt_name,
                 char  *param,
                 int    nb_values,
                 char **values,
                 int    nb_opt_data,
                 void **opt_data,
                 int    nb_ctx_data,
                 void **ctx_data)
{
  ticker_t *timers = opt_data[0];

  long   val;
  char  *endptr;
  char  *p;
  size_t l = strlen(values[0]);

  /* Add a 0 at the end of the parameter to multiply it by 10 */
  /* as the timer values must be in tenths of seconds.        */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  p    = xmalloc(l + 2);
  p    = strcpy(p, values[0]);
  p[l] = '0';

  errno = 0;
  val   = strtol(p, &endptr, 0);
  if (errno == ERANGE || (val == 0 && errno != 0) || endptr == p
      || *endptr != '\0')
  {
    fprintf(stderr, "%s: Invalid timeout delay.\n", values[0]);
    ctxopt_ctx_disp_usage(ctx_name, exit_after);
  }
  else if (val == 0)
    timers->forgotten = -1; /* Explicitly disable the timers. */
  else
    timers->forgotten = (int)val;

  free(p);
}

void
button_action(char  *ctx_name,
              char  *opt_name,
              char  *param,
              int    nb_values,
              char **values,
              int    nb_opt_data,
              void **opt_data,
              int    nb_ctx_data,
              void **ctx_data)
{
  mouse_t *mouse = opt_data[0];

  char *endptr;
  char *p;
  long  val;

  if (nb_values != 2)
  {
    fprintf(stderr, "Remapping of buttons 1 and 3 expected.\n");
    ctxopt_ctx_disp_usage(ctx_name, exit_after);
  }

  int const index[2] = { 0, 2 };
  int       ind;

  mouse->button[0] = 0;
  mouse->button[1] = 0;
  mouse->button[2] = 0;

  for (int i = 0; i < nb_values; i++)
  {
    ind   = index[i];
    errno = 0;
    p     = values[i];
    val   = strtol(p, &endptr, 0);
    if (errno == ERANGE || endptr == p || *endptr != '\0' || val < 1 || val > 3)
    {
      fprintf(stderr, "%s: Invalid button number.\n", values[0]);
      ctxopt_ctx_disp_usage(ctx_name, exit_after);
    }
    mouse->button[(int)val - 1] = ind + 1;
  }
}

void
double_click_action(char  *ctx_name,
                    char  *opt_name,
                    char  *param,
                    int    nb_values,
                    char **values,
                    int    nb_opt_data,
                    void **opt_data,
                    int    nb_ctx_data,
                    void **ctx_data)
{
  mouse_t *mouse = opt_data[0];
  int     *ddc   = opt_data[1];

  char *endptr;
  char *p = values[0];
  long  val;

  errno = 0;
  val   = strtol(p, &endptr, 0);
  if (errno == ERANGE || endptr == p || *endptr != '\0')
    return; /* default value (150 ~ 1/6.66th second) is set in main(). */
  if (val == 0)
    *ddc = 1; /* disable double_click; */
  else if (val >= 100 && val <= 500)
    mouse->double_click_delay = val;
}

/* =================================================================== */
/* Cancels a search. Called when ESC is hit or a new search session is */
/* initiated and the incremental_search option is not used.            */
/* =================================================================== */
void
reset_search_buffer(win_t         *win,
                    search_data_t *search_data,
                    ticker_t      *timers,
                    toggle_t      *toggles,
                    term_t        *term,
                    daccess_t     *daccess,
                    langinfo_t    *langinfo,
                    long           last_line,
                    char          *tmp_word,
                    long           word_real_max_size)
{
  /* ESC key has been pressed. */
  /* """"""""""""""""""""""""" */
  search_mode_t saved_search_mode = search_mode;

  /* Cancel the search timer. */
  /* """""""""""""""""""""""" */
  search_timer = 0;

  search_data->err           = 0;
  search_data->only_starting = 0;
  search_data->only_ending   = 0;

  if (help_mode)
    disp_lines(win,
               toggles,
               current,
               count,
               search_mode,
               search_data,
               term,
               last_line,
               tmp_word,
               langinfo);

  /* Reset the direct access selector stack. */
  /* """"""""""""""""""""""""""""""""""""""" */
  memset(daccess_stack, '\0', 6);
  daccess_stack_head = 0;
  daccess_timer      = timers->direct_access;

  /* Clean the potential matching words non empty list. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  search_mode = NONE;

  if (BUF_LEN(matching_words_da) > 0 || saved_search_mode != search_mode)
  {
    clean_matches(search_data, word_real_max_size);

    disp_lines(win,
               toggles,
               current,
               count,
               search_mode,
               search_data,
               term,
               last_line,
               tmp_word,
               langinfo);
  }
}

/* ===================================================================== */
/* Get the new index of a word based on the mouse click position.        */
/* Returns a new index if a word could be selected or the original index */
/* The error flag is set if the word was not selectable or if the user   */
/* did not click on a word.                                              */
/* ===================================================================== */
long
get_clicked_index(win_t  *win,
                  term_t *term,
                  int     line_click,
                  int     column_click,
                  int    *error)
{
  long new_current;  /* Future new current word on success.              */
  long clicked_line; /* Number of the clicked line in smenu internal     *
                      | representation on the screen.                    */
  int  delta;        /* Compensation due to the alignment of the first   *
                      | visible word of the selected line in the window. */

  /* Make sure the error indicator is initialized. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  *error = 0;

  /* Get the internal line number of the click. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  clicked_line = line_nb_of_word_a[win->start] + line_click - term->curs_line;

  /* Ignore clicks after the last word on a line. */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (column_click >= shift_right_sym_pos_a[clicked_line])
    return current;

  new_current = first_word_in_line_a[clicked_line];

  if (new_current == count - 1)
    return new_current; /* The users clicked on the last word; */
  else
  {
    int offset; /* in column or line mode lines can be larger than the *
                 | terminal size, in this case a space is added at the *
                 | start of the lines to possibly put the  shift arrow *
                 | indicator, this space must be taken into account.   */
    int found;  /* Flag to indicate if a word could have been clicked. */

    /* Take into account the presence of shift arrows if any. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win->has_truncated_lines)
      offset = 1;
    else
      offset = 0;

    /* Compensates for the offset of the window in the case of a centred */
    /* window and the additional space added by the display of truncated */
    /* lines.                                                            */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    column_click -= win->offset;

    /* Forbid click on column 1 if the windows has truncated lines. */
    /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
    if (column_click == 1 && offset == 1)
      return current;

    /* Normalize column_click. */
    /* """"""""""""""""""""""" */
    column_click -= offset;

    /* Determine the offset of the first character of the */
    /* first visible word in the clicked line.            */
    /* """""""""""""""""""""""""""""""""""""""""""""""""" */
    while (word_a[new_current].start < win->first_column)
    {
      new_current++;

      if (new_current == count || word_a[new_current].start == 0)
        return current;
    }

    delta = word_a[new_current].start - win->first_column;

    /* Find the right clicked word if any. */
    /* """"""""""""""""""""""""""""""""""" */
    found = 0;

    while (1)
    {
      if ((column_click - 1
           >= word_a[new_current].start - win->first_column - delta)
          && (column_click - 1
              <= word_a[new_current].end - win->first_column - delta))
      {
        found = 1;
        break;
      }

      new_current++;

      if (new_current == count || word_a[new_current].start == 0)
        break;
    }

    if (!found)
    {
      *error      = 1;
      new_current = current;
    }
  }

  if (!word_a[new_current].is_selectable)
  {
    *error = 1;
    return current;
  }

  return new_current;
}

/* ==================================================================== */
/* Get the number of the clicked shift arrow if any.                    */
/* arrow will contain 0 if the left arrow as clicked and 1 if the right */
/* arrow was clicked.                                                   */
/* Returns 0 is no arrow was clicked else 1.                            */
/* ==================================================================== */
int
shift_arrow_clicked(win_t  *win,
                    term_t *term,
                    int     line_click,
                    int     column_click,
                    long   *clicked_line,
                    int    *arrow)
{
  *arrow = -1;

  /* Get the internal line number of the click. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  *clicked_line = line_nb_of_word_a[win->start] + line_click - term->curs_line;

  if (column_click > shift_right_sym_pos_a[*clicked_line])
    return 0;

  /* Compensates for the offset of the window */
  /* in the case of a centred window.         */
  /* """""""""""""""""""""""""""""""""""""""" */
  if (column_click == 1)
    *arrow = 0; /* Left arrow. */
  else if (shift_right_sym_pos_a[*clicked_line] < term->ncolumns
           && column_click == shift_right_sym_pos_a[*clicked_line])
    *arrow = 1; /* Right arrow. */
  else
    return 0;

  return 1;
}

/* ============================================================== */
/* Takes a string with leading and/or trailing spaces and try to  */
/* left-align, right-align or center them by re-arranging blanks. */
/* word      IN/OUT string to work with.                          */
/* alignment IN     kind of alignments.                           */
/* ============================================================== */
void
align_word(word_t *word, alignment_t alignment, size_t prefix, char sp)
{
  switch (alignment)
  {
    char  *str;
    size_t n;
    size_t m;
    size_t wl;
    size_t l;

    case AL_LEFT:
      /* Left align the word. */
      /* """""""""""""""""""" */
      str = xstrdup(word->str + prefix);
      n   = 0;

      while (str[n] == sp)
        n++;

      swap_string_parts(&str, n);

      strcpy(word->str + prefix, str);
      free(str);

      break;

    case AL_RIGHT:
      /* Right align the word. */
      /* """"""""""""""""""""" */
      str = xstrdup(word->str + prefix);
      n   = strlen(str) - 1;

      while (n && str[n] == sp)
        n--;

      swap_string_parts(&str, n + 1);

      strcpy(word->str + prefix, str);
      free(str);

      break;

    case AL_CENTERED:
      /* Center the word. */
      /* """""""""""""""" */
      str = xstrdup(word->str + prefix);
      n   = 0;
      l   = strlen(str);
      m   = l - 1;

      while (str[n] == sp)
        n++;

      while (m && str[m] == sp)
        m--;

      if (n > m)
      {
        free(str);
        break;
      }

      wl = m - n + 1;
      memset(word->str + prefix, sp, l);

      strncpy(word->str + prefix + (l - wl) / 2, str + n, wl);
      free(str);

      break;

    default:
      break;
  }
}

/* ================= */
/* Main entry point. */
/* ================= */
int
main(int argc, char *argv[])
{
  /* Mapping of supported charsets and the number of bits used in them. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  charsetinfo_t all_supported_charsets[] = {
    { "UTF-8", 8 },

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

    { "ISO8859-1", 8 },
    { "ISO-8859-1", 8 },
    { "ISO-IR-100", 8 },
    { "ISO_8859-1:1987", 8 },
    { "ISO_8859-1", 8 },
    { "LATIN1", 8 },
    { "L1", 8 },
    { "IBM819", 8 },
    { "CP819", 8 },

    { "ISO8859-2", 8 },
    { "ISO-8859-2", 8 },
    { "ISO-IR-101", 8 },
    { "ISO_8859-2:1987", 8 },
    { "ISO_8859-2", 8 },
    { "LATIN2", 8 },
    { "L2", 8 },
    { "CP28592", 8 },

    { "ISO8859-3", 8 },
    { "ISO-8859-3", 8 },
    { "ISO-IR-109", 8 },
    { "ISO_8859-3:1988", 8 },
    { "ISO_8859-3", 8 },
    { "LATIN3", 8 },
    { "L3", 8 },
    { "CP28593", 8 },

    { "ISO8859-4", 8 },
    { "ISO-8859-4", 8 },
    { "ISO-IR-110", 8 },
    { "ISO_8859-4:1988", 8 },
    { "LATIN4", 8 },
    { "L4", 8 },
    { "CP28594", 8 },

    { "ISO8859-5", 8 },
    { "ISO-8859-5", 8 },
    { "ISO-IR-144", 8 },
    { "ISO_8859-5:1988", 8 },
    { "CYRILLIC", 8 },
    { "CP28595", 8 },

    { "KOI8-R", 8 },
    { "KOI8-RU", 8 },
    { "KOI8-U", 8 },

    { "ISO8859-6", 8 },
    { "ISO-8859-6", 8 },
    { "ISO-IR-127", 8 },
    { "ISO_8859-6:1987", 8 },
    { "ECMA-114", 8 },
    { "ASMO-708", 8 },
    { "ARABIC", 8 },
    { "CP28596", 8 },

    { "ISO8859-7", 8 },
    { "ISO-8859-7", 8 },
    { "ISO-IR-126", 8 },
    { "ISO_8859-7:2003", 8 },
    { "ISO_8859-7:1987", 8 },
    { "ELOT_928", 8 },
    { "ECMA-118", 8 },
    { "GREEK", 8 },
    { "GREEK8", 8 },
    { "CP28597", 8 },

    { "ISO8859-8", 8 },
    { "ISO-8859-8", 8 },
    { "ISO-IR-138", 8 },
    { "ISO_8859-8:1988", 8 },
    { "HEBREW", 8 },
    { "CP28598", 8 },

    { "ISO8859-9", 8 },
    { "ISO-8859-9", 8 },
    { "ISO-IR-148", 8 },
    { "ISO_8859-9:1989", 8 },
    { "LATIN5", 8 },
    { "L5", 8 },
    { "CP28599", 8 },

    { "ISO8859-10", 8 },
    { "ISO-8859-10", 8 },
    { "ISO-IR-157", 8 },
    { "ISO_8859-10:1992", 8 },
    { "LATIN6", 8 },
    { "L6", 8 },
    { "CP28600", 8 },

    { "ISO8859-11", 8 },
    { "ISO-8859-11", 8 },
    { "ISO-8859-11:2001", 8 },
    { "ISO-IR-166", 8 },
    { "CP474", 8 },

    { "TIS-620", 8 },
    { "TIS620", 8 },
    { "TIS620-0", 8 },
    { "TIS620.2529-1", 8 },
    { "TIS620.2533-0", 8 },

    /* ISO-8859-12 was abandoned in 1997. */
    /* """""""""""""""""""""""""""""""""" */

    { "ISO8859-13", 8 },
    { "ISO-8859-13", 8 },
    { "ISO-IR-179", 8 },
    { "LATIN7", 8 },
    { "L7", 8 },
    { "CP28603", 8 },

    { "ISO8859-14", 8 },
    { "ISO-8859-14", 8 },
    { "LATIN8", 8 },
    { "L8", 8 },

    { "ISO8859-15", 8 },
    { "ISO-8859-15", 8 },
    { "LATIN-9", 8 },
    { "CP28605", 8 },

    { "ISO8859-16", 8 },
    { "ISO-8859-16", 8 },
    { "ISO-IR-226", 8 },
    { "ISO_8859-16:2001", 8 },
    { "LATIN10", 8 },
    { "L10", 8 },

    { "CP1250", 8 },
    { "CP1251", 8 },

    { "CP1252", 8 },
    { "MS-ANSI", 8 },
    { NULL, 0 }
  };

  /* Terminal setting variables. */
  /* """"""""""""""""""""""""""" */
  struct termios new_in_attrs;
  struct termios old_in_attrs;

  /* Interval timers used. */
  /* """"""""""""""""""""" */
  struct itimerval periodic_itv; /* refresh rate for the timeout counter.    */

  /* Used by the internal help system. */
  /* """"""""""""""""""""""""""""""""" */
  help_attr_entry_t  **help_items_da = NULL;
  help_attr_entry_t ***help_lines_da = NULL;

  int fst_disp_help_line = 0;
  int init_help_needed   = 1; /* 1 if help lines need to be reevaluated. */

  /* Other variables. */
  /* """""""""""""""" */
  int    nb_rem_args = 0; /* Remaining non analyzed command line arguments.  */
  char **rem_args    = NULL; /* Remaining non analyzed command line          *
                              | arguments array.                             */

  char *message = NULL; /* message to be displayed above the selection       *
                         | window.                                           */
  ll_t *message_lines_list = NULL; /* list of the lines in the message to    *
                                    | be displayed.                          */
  long  message_max_width  = 0; /* total width of the message                *
                                 | (longest line).                           */
  long  message_max_len    = 0; /* max number of bytes taken by a message     *
                                 | line.                                     */

  char *int_string      = NULL; /* String to be output when typing ^C.       */
  int   int_as_in_shell = 1; /* CTRL-C mimics the shell behaviour.           */

  FILE *input_file; /* The name of the file passed as argument if any.       */

  long index; /* generic counter.                                            */

  long daccess_index = 1; /* First index of the numbered words.              */

  char   *daccess_np = NULL; /* direct access numbered pattern.              */
  regex_t daccess_np_re; /* variable to store the compiled direct access     *
                          | pattern (-N) RE.                                 */

  char   *daccess_up = NULL; /* direct access not numbered pattern.          */
  regex_t daccess_up_re; /* variable to store the compiled direct access     *
                          | pattern (-U) RE.                                 */

  char *include_pattern = NULL; /* ASCII version of the include RE           */
  char *exclude_pattern = NULL; /* ASCII version of the exclude RE           */

  int pattern_def_include = -1; /* Set to -1 until an -i or -e option is     *
                                 | specified, This variable remembers  if    *
                                 | the words not matched will be included    *
                                 | (value 1) or excluded (value 0) by        *
                                 | default.                                  */

  regex_t include_re; /* variable to store the compiled include (-i) REs.    */
  regex_t exclude_re; /* variable to store the compiled exclude (-e) REs.    */

  ll_t *early_sed_list   = NULL; /* List of sed like string representation   *
                                  | of regex given after (-ES).              */
  ll_t *sed_list         = NULL; /* List of sed like string representation   *
                                  | of regex given after (-S).               */
  ll_t *include_sed_list = NULL; /* idem for -I.                             */
  ll_t *exclude_sed_list = NULL; /* idem for -E.                             */

  ll_t *inc_col_interval_list = NULL; /* Lists of included or                */
  ll_t *exc_col_interval_list = NULL; /* excluded numerical intervals        */
  ll_t *inc_row_interval_list = NULL; /* for lines and columns.              */
  ll_t *exc_row_interval_list = NULL;

  ll_t *inc_col_regex_list = NULL; /* Same for lines and columns specified.  */
  ll_t *exc_col_regex_list = NULL; /* by regular expressions.                */
  ll_t *inc_row_regex_list = NULL;
  ll_t *exc_row_regex_list = NULL;

  ll_t *al_col_interval_list = NULL; /* Lists of left aligned rows/columns,  */
  ll_t *ar_col_interval_list = NULL; /* right aligned rows/columns,          */
  ll_t *ac_col_interval_list = NULL; /* centered rows/columns                */
  ll_t *al_row_interval_list = NULL;
  ll_t *ar_row_interval_list = NULL;
  ll_t *ac_row_interval_list = NULL;

  ll_t *at_col_interval_list = NULL; /* list of attributes for rows and      */
  ll_t *at_row_interval_list = NULL; /* columns.                             */

  ll_t *al_col_regex_list = NULL; /* Same for regular expression based       */
  ll_t *ar_col_regex_list = NULL; /* alignments.                             */
  ll_t *ac_col_regex_list = NULL;
  ll_t *al_row_regex_list = NULL;
  ll_t *ar_row_regex_list = NULL;
  ll_t *ac_row_regex_list = NULL;

  ll_t *at_col_regex_list = NULL; /* list of attributes for rows and         */
  ll_t *at_row_regex_list = NULL; /* columns.                                */

  ll_t *al_word_regex_list = NULL;
  ll_t *ar_word_regex_list = NULL;
  ll_t *ac_word_regex_list = NULL;

  unsigned char al_delim = 0x05; /* Alignment delimiter.                     */

  alignment_t default_alignment = AL_NONE;

  filters_t rows_filter_type = UNKNOWN_FILTER;

  char   *first_word_pattern = NULL; /* used by -A/-Z.                       */
  char   *last_word_pattern  = NULL;
  regex_t first_word_re; /* to hold the compiled first words RE.             */
  regex_t last_word_re;  /* to hold the compiled last words RE.              */

  char   *special_pattern[9] = { NULL, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL }; /* -1 .. -9 */
  regex_t special_re[9]; /* array to hold the compiled special patterns RE.  */

  int include_visual_only = 0; /* If set to 1, the original word which is    *
                                | read from stdin will be output even if its */
  int exclude_visual_only = 0; /* visual representation was modified via     *
                                | -S/-I/-E.                                  */

  ll_t *cols_selector_list = NULL; /* to store ASCII representations of      */
  char *cols_selector      = NULL; /* ranges add RE to select some columns   *
                                    | with -C.                               */

  ll_t *rows_selector_list = NULL; /* to store ASCII representations of      */
  char *rows_selector      = NULL; /* ranges and RE to select some rows      *
                                    | with -R.                               */

  ll_t *aligns_selector_list = NULL; /* to store ASCII representations of    */
  char *aligns_selector      = NULL; /* RE to select some rows with -al.     */

  long wi; /* word index.                                                    */

  term_t term; /* Terminal structure.                                        */

  tst_node_t *tst_word    = NULL; /* TST used by the search function.        */
  tst_node_t *tst_daccess = NULL; /* TST used by the direct access system.   */

  long  page;     /* Step for the vertical cursor moves.                     */
  char *word;     /* Temporary variable to work on words.                    */
  char *tmp_word; /* Temporary variable able to contain  the beginning of    *
                   | the word to be displayed.                               */

  long     last_line = 0; /* last logical line number (from 0).              */
  win_t    win;
  limit_t  limits;  /* set of various limitations.                           */
  ticker_t timers;  /* timers contents.                                      */
  misc_t   misc;    /* misc contents.                                        */
  mouse_t  mouse;   /* mouse default values.                                 */
  toggle_t toggles; /* set of binary indicators.                             */

  int   old_fd0; /* backups of the old stdin file descriptor.                */
  int   old_fd1; /* backups of the old stdout file descriptor.               */
  FILE *old_stdin;
  FILE *old_stdout; /* The selected word will go there.                      */

  long nl; /* Number of lines displayed in the window.                       */
  long line_offset; /* Used to correctly put the cursor at the start of the  *
                     | selection window, even after a terminal vertical      *
                     | scroll.                                               */

  long first_selectable; /* Index of the first selectable word in the input  *
                          | stream.                                          */
  long last_selectable;  /* Index of the last selectable word in the input   *
                          | stream.                                          */

  long min_size; /* Minimum screen width of a column in tabular mode.        */

  long tab_max_size;      /* Maximum screen width of a column in tabular     *
                           | mode.                                           */
  long tab_real_max_size; /* Maximum size in bytes of a column in tabular    *
                           | mode.                                           */

  long *col_real_max_size = NULL; /* Array of maximum sizes (bytes) of each  */
                                  /* column in column mode.                  */

  long *col_max_size = NULL; /* Array of maximum sizes (in display cells)    *
                              | of each column in column mode.               */

  attrib_t **col_attrs = NULL; /* attributes for each column in column mode.  */

  long word_real_max_size = 0; /* size of the longer word after expansion.   */
  long cols_real_max_size = 0; /* Max real width of all columns used when    *
                                | -w and -c are both set.                    */

  long cols_max_size = 0; /* Same as above for the columns widths            */

  long col_index = 0; /* Index of the current column when reading words,     *
                       | used  in column mode.                               */

  long cols_number = 0; /* Number of columns in column mode.                 */

  char *pre_selection_index = NULL; /* pattern used to set the initial       *
                                     | cursor position.                      */

  unsigned char buffer[64]; /* Input buffer which will contain the scancode. */

  search_data_t search_data;
  search_data.buf    = NULL; /* Search buffer                                */
  search_data.len    = 0;    /* Current position in the search buffer        */
  search_data.mb_len = 0;    /* Current position in the search buffer in     *
                              | UTF-8 units.                                 */
  search_data.err    = 0;    /* reset the error indicator.                   */
  search_data.fuzzy_err_pos = -1; /* no last error position in search        *
                                   | buffer.                                 */

  long matching_word_cur_index = -1; /* cache for the next/previous moves    *
                                      | in the matching words array.         */

  struct sigaction sa; /* Signal structure.                                  */

  char *iws = NULL, *ils = NULL, *zg = NULL; /* words/lines delimiters and   *
                                              | zapped glyphs strings.       */

  ll_t *word_delims_list   = NULL; /* and associated linked lists.           */
  ll_t *line_delims_list   = NULL;
  ll_t *zapped_glyphs_list = NULL;

  char utf8_buffer[5]; /* buffer to store the bytes of a UTF-8 glyph         *
                        | (4 chars max).                                     */
  unsigned char is_last;
  char         *charset;

  char *home_ini_file;  /* init file full path.                              */
  char *local_ini_file; /* init file full path.                              */

  charsetinfo_t *charset_ptr;
  langinfo_t     langinfo;
  int            is_supported_charset;

  long line_count = 0; /* Only used when -R is selected.                     */

  attrib_t init_attr;

  ll_node_t *inc_interval_node = NULL; /* one node of this list.             */
  ll_node_t *exc_interval_node = NULL; /* one node of this list.             */

  interval_t *inc_interval;       /* the data in each node.                  */
  interval_t *exc_interval;       /* the data in each node.                  */
  int         row_def_selectable; /* default selectable value.               */

  int line_selected_by_regex = 0;
  int line_excluded          = 0;

  char *timeout_message;

  char *common_options;
  char *main_options, *main_spec_options;
  char *col_options, *col_spec_options;
  char *line_options, *line_spec_options;
  char *tab_options, *tab_spec_options;
  char *tag_options, *tag_spec_options;

  /* Used to check if we can get the cursor position in the terminal. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  int row; /* absolute line position in terminal (1...)   */
  int col; /* absolute column position in terminal (1...) */

  int mouse_proto = -1;

  /* Initialize some internal data structures. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  init_main_ds(&init_attr,
               &win,
               &limits,
               &timers,
               &toggles,
               &misc,
               &mouse,
               &timeout,
               &daccess);

  /* direct access variable initialization.   */
  /*   0   1   2   3   4   5                  */
  /* +---+---+---+---+---+---+                */
  /* |   |   |   |   |   |   | daccess_stack  */
  /* +-^-+---+---+---+---+---+                */
  /*   |                                      */
  /*   daccess_stack_head                     */
  /*                                          */
  /* daccess_stack will be used as a string.  */
  /* We must make sure its last byte is '\0'. */
  /* """""""""""""""""""""""""""""""""""""""" */
  daccess_stack      = xcalloc(6, 1);
  daccess_stack_head = 0;

  /* fuzzy variables initialization. */
  /* """"""""""""""""""""""""""""""" */
  tst_search_list = ll_new();
  ll_append(tst_search_list, sub_tst_new());

  matching_words_da      = NULL;
  best_matching_words_da = NULL;

  /* Initialize the tag hit number which will permit to sort the */
  /* pinned words when displayed.                                */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  long tag_nb = 0;

  /* Columns selection variables. */
  /* """""""""""""""""""""""""""" */
  char *cols_filter = NULL;

  /* Initialize the count of tagged words. */
  /* """"""""""""""""""""""""""""""""""""" */
  long tagged_words = 0;

  /* Double-click related variables. */
  /* """"""""""""""""""""""""""""""" */
  int             disable_double_click = 0;
  int             click_nr             = 0;
  struct timespec last_click_ts;

  /* Get the current locale. */
  /* """"""""""""""""""""""" */
  setlocale(LC_ALL, "");
  charset = nl_langinfo(CODESET);

  /* Check if the local charset is supported. */
  /* """""""""""""""""""""""""""""""""""""""" */
  is_supported_charset = 0;
  charset_ptr          = all_supported_charsets;

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
    fprintf(stderr, "%s is not a supported charset.", charset);

    exit(EXIT_FAILURE);
  }

  /* Remember the fact that the charset is UTF-8. */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (strcmp(charset, "UTF-8") == 0)
    langinfo.utf8 = 1;
  else
    langinfo.utf8 = 0;

  /* my_isprint is a function pointers that points to   */
  /* the 7 or 8-bit version of isprint according to the */
  /* current locale.                                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo.utf8 || langinfo.bits == 8)
    my_isprint = isprint8;
  else
    my_isprint = isprint7;

  /* my_isempty is a function pointers that points to         */
  /* the utf8 or non_utf8 version of isprint according to the */
  /* current locale.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo.utf8)
    my_isempty = isempty_utf8;
  else
    my_isempty = isempty_non_utf8;

  /* Set terminal in noncanonical, noecho mode and  */
  /* if TERM is unset or unknown, vt100 is assumed. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (getenv("TERM") == NULL)
    setupterm("vt100", 1, (int *)0);
  else
    setupterm((char *)0, 1, (int *)0);

  /* Get the number of colors if the use of colors is available */
  /* and authorized.                                            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (getenv("NO_COLOR") != NULL)
    term.colors = 0;
  else
  {
    term.colors = tigetnum("colors");
    if (term.colors < 0)
      term.colors = 0;
  }

  /* Ignore SIGTTIN. */
  /* """"""""""""""" */
  sigset_t sigs, oldsigs;

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTTIN);
  sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

  /* Temporarily set /dev/tty as stdin/stdout to get its size */
  /* even in a pipe.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  old_fd0 = dup(0);
  if (old_fd0 == -1)
  {
    fprintf(stderr, "Cannot save the standard input file descriptor.\n");
    exit(EXIT_FAILURE);
  }

  old_stdin = freopen("/dev/tty", "r", stdin);

  old_fd1 = dup(1);
  if (old_fd1 == -1)
  {
    fprintf(stderr, "Cannot save the standard output file descriptor.\n");
    exit(EXIT_FAILURE);
  }

  old_stdout = freopen("/dev/tty", "w", stdout);

  if (old_stdin == NULL || old_stdout == NULL)
  {
    fprintf(stderr, "A terminal is required to use this program.\n");
    exit(EXIT_FAILURE);
  }

  /* Get the number of lines/columns of the terminal. */
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  get_terminal_size(&term.nlines, &term.ncolumns, &term);

  /* Restore the old stdin and stdout. */
  /* """"""""""""""""""""""""""""""""" */
  if (dup2(old_fd0, 0) == -1)
  {
    fprintf(stderr, "Cannot restore the standard input file descriptor.\n");
    exit(EXIT_FAILURE);
  }

  if (dup2(old_fd1, 1) == -1)
  {
    fprintf(stderr, "Cannot restore the standard output file descriptor.\n");
    exit(EXIT_FAILURE);
  }

  close(old_fd0);
  close(old_fd1);

  /* Build the full path of the .ini file. */
  /* """"""""""""""""""""""""""""""""""""" */
  home_ini_file  = make_ini_path(argv[0], "HOME");
  local_ini_file = make_ini_path(argv[0], "PWD");

  /* Set the attributes from the configuration file if possible. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ini_load(home_ini_file,
               &win,
               &term,
               &limits,
               &timers,
               &misc,
               &mouse,
               ini_cb))
    exit(EXIT_FAILURE);

  if (ini_load(local_ini_file,
               &win,
               &term,
               &limits,
               &timers,
               &misc,
               &mouse,
               ini_cb))
    exit(EXIT_FAILURE);

  free(home_ini_file);
  free(local_ini_file);

  /* Command line option settings using ctxopt. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  ctxopt_init(argv[0],
              "stop_if_non_option=No "
              "allow_abbreviations=No "
              "display_usage_on_error=Yes ");

  common_options = "[*help] "
                   "[*usage] "
                   "[include_re... #regex] "
                   "[exclude_re... #regex] "
                   "[title #message] "
                   "[int [#string]] "
                   "[attributes #prefix:attr...] "
                   "[special_level_1 #...<3] "
                   "[special_level_2 #...<3] "
                   "[special_level_3 #...<3] "
                   "[special_level_4 #...<3] "
                   "[special_level_5 #...<3] "
                   "[special_level_6 #...<3] "
                   "[special_level_7 #...<3] "
                   "[special_level_8 #...<3] "
                   "[special_level_9 #...<3] "
                   "[zapped_glyphs #bytes] "
                   "[lines [#height]] "
                   "[blank_nonprintable] "
                   "[*invalid_character #invalid_char_subst] "
                   "[center_mode] "
                   "[clean] "
                   "[keep_spaces] "
                   "[word_separators #bytes] "
                   "[no_scroll_bar] "
                   "[early_subst_all... #/regex/repl/opts] "
                   "[post_subst_all... #/regex/repl/opts] "
                   "[post_subst_included... #/regex/repl/opts] "
                   "[post_subst_excluded... #/regex/repl/opts] "
                   "[search_method #prefix|substring|fuzzy] "
                   "[start_pattern #pattern] "
                   "[timeout #...] "
                   "[hidden_timeout #...] "
                   "[validate_in_search_mode] "
                   "[visual_bell] "
                   "[ignore_quotes] "
                   "[incremental_search] "
                   "[limits... #limit:value...] "
                   "[forgotten_timeout #timeout] "
                   "[double_click_delay #delay] "
                   "[button_remapping #mapping...] "
                   "[no_mouse] "
                   "[show_blank_words [#blank_char]] "; /* <- don't remove *
                                                         | this space!     */

  main_spec_options = "[*copyright] "
                      "[*version] "
                      "[*long_help] "
                      "[da_options #prefix:attr...] "
                      "[auto_da_number... [#regex...]] "
                      "[auto_da_unnumber... [#regex...]] "
                      "[field_da_number] "
                      "[column_mode>Columns] "
                      "[line_mode>Lines] "
                      "[tab_mode>Tabulations [#cols]] "
                      "[tag_mode>Tagging [#delim]] "
                      "[pin_mode>Tagging [#delim]]";

  col_spec_options = "[wide_mode] "
                     "[columns_select... #selector...] "
                     "[rows_select... #selector...] "
                     "[aligns_select... #re_selector...] "
                     "[gutter [#string]] "
                     "[line_separators #bytes] "
                     "[da_options #prefix:attr...] "
                     "[auto_da_number... [#regex...]] "
                     "[auto_da_unnumber... [#regex...]] "
                     "[field_da_number] "
                     "[tag_mode>Tagging [#delim]] "
                     "[pin_mode>Tagging [#delim]] "
                     "[no_hor_scroll_bar] "
                     "[hor_scroll_bar] "
                     "[force_first_column #regex] "
                     "[force_last_column #regex]";

  line_spec_options = "[rows_select... #selector...] "
                      "[line_separators #bytes] "
                      "[da_options #prefix:attr...] "
                      "[auto_da_number... [#regex...]] "
                      "[auto_da_unnumber... [#regex...]] "
                      "[field_da_number] "
                      "[tag_mode>Tagging [#delim]] "
                      "[pin_mode>Tagging [#delim]] "
                      "[no_hor_scroll_bar] "
                      "[hor_scroll_bar] "
                      "[force_first_column #regex] "
                      "[force_last_column #regex]";

  tab_spec_options = "[wide_mode] "
                     "[gutter [#string]] "
                     "[line_separators #bytes] "
                     "[da_options #prefix:attr...] "
                     "[auto_da_number... [#regex...]] "
                     "[auto_da_unnumber... [#regex...]] "
                     "[field_da_number] "
                     "[tag_mode>Tagging [#delim]] "
                     "[pin_mode>Tagging [#delim]] "
                     "[force_first_column #regex] "
                     "[force_last_column #regex]";

  tag_spec_options = "[auto_tag] "
                     "[no_auto_tag] "
                     "[column_mode>Columns] "
                     "[line_mode>Lines] "
                     "[tab_mode>Tabulations [#cols]]";

  main_options = concat(common_options, main_spec_options, (char *)0);
  col_options  = concat(common_options, col_spec_options, (char *)0);
  line_options = concat(common_options, line_spec_options, (char *)0);
  tab_options  = concat(common_options, tab_spec_options, (char *)0);
  tag_options  = concat(common_options, tag_spec_options, (char *)0);

  ctxopt_new_ctx("Main", main_options);
  ctxopt_new_ctx("Columns", col_options);
  ctxopt_new_ctx("Lines", line_options);
  ctxopt_new_ctx("Tabulations", tab_options);
  ctxopt_new_ctx("Tagging", tag_options);

  free(main_options);
  free(col_options);
  free(line_options);
  free(tab_options);
  free(tag_options);

  /* ctxopt parameters. */
  /* """""""""""""""""" */

  ctxopt_add_opt_settings(parameters, "help", "-h -help");
  ctxopt_add_opt_settings(parameters, "long_help", "-H -long-help");
  ctxopt_add_opt_settings(parameters, "usage", "-? -u -usage");
  ctxopt_add_opt_settings(parameters, "copyright", "-copyright");
  ctxopt_add_opt_settings(parameters, "version", "-V -version");
  ctxopt_add_opt_settings(parameters,
                          "include_re",
                          "-i -in -inc -incl -include");
  ctxopt_add_opt_settings(parameters,
                          "exclude_re",
                          "-e -ex -exc -excl -exclude");
  ctxopt_add_opt_settings(parameters, "lines", "-n -lines -height");
  ctxopt_add_opt_settings(parameters, "title", "-m -msg -message -title");
  ctxopt_add_opt_settings(parameters, "int", "-! -int -int_string");
  ctxopt_add_opt_settings(parameters, "attributes", "-a -attr -attributes");
  ctxopt_add_opt_settings(parameters, "special_level_1", "-1 -l1 -level1");
  ctxopt_add_opt_settings(parameters, "special_level_2", "-2 -l2 -level2");
  ctxopt_add_opt_settings(parameters, "special_level_3", "-3 -l3 -level3");
  ctxopt_add_opt_settings(parameters, "special_level_4", "-4 -l4 -level4");
  ctxopt_add_opt_settings(parameters, "special_level_5", "-5 -l5 -level5");
  ctxopt_add_opt_settings(parameters, "special_level_6", "-6 -l6 -level6");
  ctxopt_add_opt_settings(parameters, "special_level_7", "-7 -l7 -level7");
  ctxopt_add_opt_settings(parameters, "special_level_8", "-8 -l8 -level8");
  ctxopt_add_opt_settings(parameters, "special_level_9", "-9 -l9 -level9");
  ctxopt_add_opt_settings(parameters, "tag_mode", "-T -tm -tag -tag_mode");
  ctxopt_add_opt_settings(parameters, "pin_mode", "-P -pm -pin -pin_mode");
  ctxopt_add_opt_settings(parameters, "auto_tag", "-p -at -auto_tag");
  ctxopt_add_opt_settings(parameters, "no_auto_tag", "-0 -noat -no_auto_tag");
  ctxopt_add_opt_settings(parameters, "auto_da_number", "-N -number");
  ctxopt_add_opt_settings(parameters, "auto_da_unnumber", "-U -unnumber");
  ctxopt_add_opt_settings(parameters,
                          "field_da_number",
                          "-F -en -embedded_number");
  ctxopt_add_opt_settings(parameters, "da_options", "-D -data -options");
  ctxopt_add_opt_settings(parameters, "invalid_character", "-. -dot -invalid");
  ctxopt_add_opt_settings(parameters, "blank_nonprintable", "-b -blank");
  ctxopt_add_opt_settings(parameters, "center_mode", "-M -middle -center");
  ctxopt_add_opt_settings(parameters,
                          "clean",
                          "-d -restore -delete -clean "
                          "-delete_window -clean_window");
  ctxopt_add_opt_settings(parameters,
                          "column_mode",
                          "-c -col -col_mode -column");
  ctxopt_add_opt_settings(parameters, "line_mode", "-l -line -line_mode");
  ctxopt_add_opt_settings(parameters,
                          "tab_mode",
                          "-t -tab -tab_mode -tabulate_mode");
  ctxopt_add_opt_settings(parameters, "wide_mode", "-w -wide -wide_mode");
  ctxopt_add_opt_settings(parameters,
                          "columns_select",
                          "-C -cs -cols -cols_select");
  ctxopt_add_opt_settings(parameters,
                          "rows_select",
                          "-R -rs -rows -rows_select");
  ctxopt_add_opt_settings(parameters, "aligns_select", "-al -align");
  ctxopt_add_opt_settings(parameters,
                          "force_first_column",
                          "-A -fc -first_column");
  ctxopt_add_opt_settings(parameters,
                          "force_last_column",
                          "-Z -lc -last_column");
  ctxopt_add_opt_settings(parameters, "gutter", "-g -gutter");
  ctxopt_add_opt_settings(parameters, "keep_spaces", "-k -ks -keep_spaces");
  ctxopt_add_opt_settings(parameters,
                          "word_separators",
                          "-W -ws -wd -word_delimiters -word_separators");
  ctxopt_add_opt_settings(parameters,
                          "line_separators",
                          "-L -ls -ld -line-delimiters -line_separators");
  ctxopt_add_opt_settings(parameters, "zapped_glyphs", "-z -zap -zap-glyphs");
  ctxopt_add_opt_settings(parameters,
                          "no_scroll_bar",
                          "-q -no_bar -no_scroll_bar");
  ctxopt_add_opt_settings(parameters,
                          "no_hor_scroll_bar",
                          "-no_hbar -no_hor_scroll_bar");
  ctxopt_add_opt_settings(parameters,
                          "hor_scroll_bar",
                          "-hbar -hor_scroll_bar");
  ctxopt_add_opt_settings(parameters, "early_subst_all", "-ES -early_subst");
  ctxopt_add_opt_settings(parameters, "post_subst_all", "-S -subst");
  ctxopt_add_opt_settings(parameters,
                          "post_subst_included",
                          "-I -si -subst_included");
  ctxopt_add_opt_settings(parameters,
                          "post_subst_excluded",
                          "-E -se -subst_excluded");
  ctxopt_add_opt_settings(parameters, "search_method", "-/ -search_method");
  ctxopt_add_opt_settings(parameters,
                          "start_pattern",
                          "-s -sp -start -start_pattern");
  ctxopt_add_opt_settings(parameters, "timeout", "-x -tmout -timeout");
  ctxopt_add_opt_settings(parameters,
                          "hidden_timeout",
                          "-X -htmout -hidden_timeout");
  ctxopt_add_opt_settings(parameters,
                          "validate_in_search_mode",
                          "-r -auto_validate");
  ctxopt_add_opt_settings(parameters, "visual_bell", "-v -vb -visual_bell");
  ctxopt_add_opt_settings(parameters, "ignore_quotes", "-Q -ignore_quotes");
  ctxopt_add_opt_settings(parameters,
                          "incremental_search",
                          "-is -incremental_search");
  ctxopt_add_opt_settings(parameters, "limits", "-lim -limits");
  ctxopt_add_opt_settings(parameters,
                          "forgotten_timeout",
                          "-f -forgotten_timeout -global_timeout");
  ctxopt_add_opt_settings(parameters, "no_mouse", "-nm -no_mouse");
  ctxopt_add_opt_settings(parameters,
                          "button_remapping",
                          "-br -buttons -button_remapping");
  ctxopt_add_opt_settings(parameters,
                          "double_click_delay",
                          "-dc -dcd -double_click -double_click_delay");
  ctxopt_add_opt_settings(parameters,
                          "show_blank_words",
                          "-sb -sbw -show_blank_words");

  /* ctxopt options incompatibilities. */
  /* """"""""""""""""""""""""""""""""" */

  ctxopt_add_ctx_settings(incompatibilities,
                          "Main",
                          "column_mode line_mode tab_mode");
  ctxopt_add_ctx_settings(incompatibilities, "Main", "tag_mode pin_mode");
  ctxopt_add_ctx_settings(incompatibilities, "Main", "help usage");
  ctxopt_add_ctx_settings(incompatibilities, "Main", "timeout hidden_timeout");
  ctxopt_add_ctx_settings(incompatibilities,
                          "Main",
                          "no_mouse button_remapping");
  ctxopt_add_ctx_settings(incompatibilities,
                          "Main",
                          "no_mouse double_click_delay");

  /* ctxopt options requirements. */
  /* """""""""""""""""""""""""""" */

  ctxopt_add_ctx_settings(requirements,
                          "Main",
                          "da_options "
                          "field_da_number auto_da_number auto_da_unnumber");
  ctxopt_add_ctx_settings(requirements,
                          "Columns",
                          "da_options "
                          "field_da_number auto_da_number auto_da_unnumber");
  ctxopt_add_ctx_settings(requirements,
                          "Lines",
                          "da_options "
                          "field_da_number auto_da_number auto_da_unnumber");
  ctxopt_add_ctx_settings(requirements,
                          "Tabulations",
                          "da_options "
                          "field_da_number auto_da_number auto_da_unnumber");

  /* ctxopt actions. */
  /* """"""""""""""" */

  ctxopt_add_opt_settings(actions,
                          "auto_tag",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "no_auto_tag",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "invalid_character",
                          invalid_char_action,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "blank_nonprintable",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "center_mode",
                          center_mode_action,
                          &win,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "clean", toggle_action, &toggles, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "column_mode",
                          column_mode_action,
                          &win,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "line_mode",
                          line_mode_action,
                          &win,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "tab_mode",
                          tab_mode_action,
                          &win,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "columns_select",
                          columns_select_action,
                          &cols_selector_list,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "rows_select",
                          rows_select_action,
                          &rows_selector_list,
                          &win,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "aligns_select",
                          aligns_select_action,
                          &aligns_selector_list,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "include_re",
                          include_re_action,
                          &pattern_def_include,
                          &include_pattern,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "exclude_re",
                          exclude_re_action,
                          &pattern_def_include,
                          &exclude_pattern,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "gutter",
                          gutter_action,
                          &win,
                          &langinfo,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "help", help_action, (char *)0);
  ctxopt_add_opt_settings(actions, "long_help", long_help_action, (char *)0);
  ctxopt_add_opt_settings(actions, "usage", usage_action, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "keep_spaces",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "lines", lines_action, &win, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "no_scroll_bar",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "no_hor_scroll_bar",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "hor_scroll_bar",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "start_pattern",
                          set_pattern_action,
                          &pre_selection_index,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "title",
                          set_string_action,
                          &message,
                          &langinfo,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "int",
                          int_action,
                          &int_string,
                          &int_as_in_shell,
                          &langinfo,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "validate_in_search_mode",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "copyright", copyright_action, (char *)0);
  ctxopt_add_opt_settings(actions, "version", version_action, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "visual_bell",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "incremental_search",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "wide_mode",
                          wide_mode_action,
                          &win,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "early_subst_all",
                          post_subst_action,
                          &early_sed_list,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "post_subst_all",
                          post_subst_action,
                          &sed_list,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "post_subst_included",
                          post_subst_action,
                          &include_sed_list,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "post_subst_excluded",
                          post_subst_action,
                          &exclude_sed_list,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_1",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_2",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_3",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_4",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_5",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_6",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_7",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_8",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "special_level_9",
                          special_level_action,
                          special_pattern,
                          &win,
                          &term,
                          &init_attr,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "attributes",
                          attributes_action,
                          &win,
                          &term,
                          &init_attr,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "timeout", timeout_action, &misc, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "hidden_timeout",
                          timeout_action,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "force_first_column",
                          set_pattern_action,
                          &first_word_pattern,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "force_last_column",
                          set_pattern_action,
                          &last_word_pattern,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "word_separators",
                          set_pattern_action,
                          &iws,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "line_separators",
                          set_pattern_action,
                          &ils,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "zapped_glyphs",
                          set_pattern_action,
                          &zg,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "tag_mode",
                          tag_mode_action,
                          &toggles,
                          &win,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "pin_mode",
                          pin_mode_action,
                          &toggles,
                          &win,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "search_method",
                          search_method_action,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "auto_da_number",
                          auto_da_action,
                          &daccess_np,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "auto_da_unnumber",
                          auto_da_action,
                          &daccess_up,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "field_da_number",
                          field_da_number_action,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "da_options",
                          da_options_action,
                          &daccess_index,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "ignore_quotes",
                          ignore_quotes_action,
                          &misc,
                          (char *)0);
  ctxopt_add_opt_settings(actions, "limits", limits_action, &limits, (char *)0);
  ctxopt_add_opt_settings(actions,
                          "forgotten_timeout",
                          forgotten_action,
                          &timers,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "button_remapping",
                          button_action,
                          &mouse,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "double_click_delay",
                          double_click_action,
                          &mouse,
                          &disable_double_click,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "no_mouse",
                          toggle_action,
                          &toggles,
                          (char *)0);
  ctxopt_add_opt_settings(actions,
                          "show_blank_words",
                          show_blank_action,
                          &toggles,
                          &misc,
                          (char *)0);

  /* ctxopt constraints. */
  /* """"""""""""""""""" */

  ctxopt_add_opt_settings(constraints,
                          "attributes",
                          ctxopt_re_constraint,
                          "[^:]+:.+");
  ctxopt_add_opt_settings(constraints,
                          "da_options",
                          ctxopt_re_constraint,
                          "[^:]+:.+");
  ctxopt_add_opt_settings(constraints, "lines", check_integer_constraint, "");

  ctxopt_add_opt_settings(constraints,
                          "tab_mode",
                          check_integer_constraint,
                          "");
  ctxopt_add_opt_settings(constraints,
                          "tab_mode",
                          ctxopt_range_constraint,
                          "1 .");
  ctxopt_add_opt_settings(constraints,
                          "double_click_delay",
                          check_integer_constraint,
                          "");

  /* Evaluation order. */
  /* """"""""""""""""" */

  ctxopt_add_opt_settings(after,
                          "field_da_number",
                          "auto_da_number auto_da_unnumber");

  ctxopt_add_opt_settings(after,
                          "da_options",
                          "field_da_number auto_da_number auto_da_unnumber");

  /* In help settings. */
  /* """"""""""""""""" */

  ctxopt_add_opt_settings(visible_in_help, "copyright", "no");

  /* Command line options analysis. */
  /* """""""""""""""""""""""""""""" */
  ctxopt_analyze(argc - 1, argv + 1, &nb_rem_args, &rem_args);

  /* Command line options evaluation. */
  /* """""""""""""""""""""""""""""""" */
  ctxopt_evaluate();

  /* Check remaining non analyzed command line arguments. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (nb_rem_args == 1)
  {
    input_file = fopen_safe(rem_args[0], "r");
    if (input_file == NULL)
    {
      fprintf(stderr,
              "The file \"%s\" does not exist or cannot be read.\n",
              rem_args[0]);
      ctxopt_ctx_disp_usage(NULL, exit_after);
      exit(EXIT_FAILURE); /* Avoid a compiler warning. */
    }
  }
  else if (nb_rem_args == 0)
    input_file = stdin;
  else
  {
    fprintf(stderr, "Extra arguments detected:\n");

    fprintf(stderr, "%s", rem_args[1]);
    for (int i = 2; i < nb_rem_args; i++)
      fprintf(stderr, ", %s", rem_args[i]);
    fprintf(stderr, ".\n");

    ctxopt_ctx_disp_usage(NULL, exit_after);
    exit(EXIT_FAILURE); /* Avoid a compiler warning. */
  }

  /* Free the memory used internally by ctxopt. */
  /* """""""""""""""""""""""""""""""""""""""""" */
  ctxopt_free_memory();

  /* If we did not impose the number of columns, use the whole */
  /* terminal width.                                           */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.tab_mode && !win.max_cols)
    win.wide = 1;

  /* Force the padding mode to all when words are numbered and not in */
  /* col/line/tab_mode.                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (daccess.padding != 'a' && (win.col_mode || win.line_mode || win.tab_mode))
    daccess.padding = 'a';

  win.start = 0;

  term.color_method = ANSI; /* We default to setaf/setbf to set colors. */
  term.curs_line = term.curs_column = 0;

  {
    char *str;

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
    str                        = tigetstr("invis");
    term.has_invis             = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("blink");
    term.has_blink             = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("kmous");
    term.has_kmous             = (str == (char *)-1 || str == NULL) ? 0 : 1;
    str                        = tigetstr("rep");
    term.has_rep               = (str == (char *)-1 || str == NULL) ? 0 : 1;
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

  word_buffer = xcalloc(1, daccess.flength + limits.word_length + 1);

  /* default_search_method is not set in the command line nor in a config */
  /* file, set it to fuzzy.                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (misc.default_search_method == NONE)
    misc.default_search_method = FUZZY;

  /* If some attributes were not set, set their default values. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term.colors > 7)
  {
    int const special_def_attr[9] = { 1, 2, 3, 5, 6, 7, 7, 7, 7 };

    if (!win.cursor_attr.is_set)
    {
      if (term.has_reverse)
        win.cursor_attr.reverse = 1;
      else if (term.has_standout)
        win.cursor_attr.standout = 1;

      win.cursor_attr.is_set = SET;
    }

    if (!win.cursor_marked_attr.is_set)
    {
      if (term.has_dim)
        win.cursor_marked_attr.dim = 1;
      else if (term.has_bold)
        win.cursor_marked_attr.bold = 1;

      if (term.has_reverse)
        win.cursor_marked_attr.reverse = 1;
      else if (term.has_standout)
        win.cursor_marked_attr.standout = 1;

      win.cursor_marked_attr.is_set = SET;
    }

    if (!win.cursor_on_marked_attr.is_set)
    {
      if (term.has_bold)
        win.cursor_on_marked_attr.bold = 1;
      if (term.has_italic)
        win.cursor_on_marked_attr.italic = 1;

      if (term.has_reverse)
        win.cursor_on_marked_attr.reverse = 1;
      else if (term.has_standout)
        win.cursor_on_marked_attr.standout = 1;

      win.cursor_on_marked_attr.is_set = SET;
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

    if (!win.cursor_on_tag_marked_attr.is_set)
    {
      if (term.has_dim)
        win.cursor_on_tag_marked_attr.dim = 1;

      if (term.has_reverse)
        win.cursor_on_tag_marked_attr.reverse = 1;

      if (term.has_underline)
        win.cursor_on_tag_marked_attr.underline = 1;

      win.cursor_on_tag_marked_attr.is_set = SET;
    }

    if (!win.marked_attr.is_set)
    {
      if (term.has_standout)
        win.marked_attr.standout = 1;

      if (term.has_bold)
        win.marked_attr.bold = 1;

      win.marked_attr.fg = 2;
      win.marked_attr.bg = 0;

      win.marked_attr.is_set = SET;
    }

    if (!win.bar_attr.is_set)
    {
      win.bar_attr.fg     = 2;
      win.bar_attr.is_set = SET;
    }

    if (!win.shift_attr.is_set)
    {
      win.shift_attr.fg     = 2;
      win.shift_attr.is_set = SET;
    }

    if (!win.message_attr.is_set)
    {
      if (term.has_bold)
        win.message_attr.bold = 1;
      else if (term.has_reverse)
        win.message_attr.reverse = 1;
      else
      {
        win.message_attr.fg = 0;
        win.message_attr.bg = 7;
      }

      win.message_attr.is_set = SET;
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

    /* This attribute should complete the attributes already set. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

    for (index = 0; index < 9; index++)
    {
      if (!win.special_attr[index].is_set)
      {
        win.special_attr[index].fg     = special_def_attr[index];
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

    if (!win.message_attr.is_set)
    {
      if (term.has_bold)
        win.message_attr.bold = 1;
      else if (term.has_reverse)
        win.message_attr.reverse = 1;

      win.message_attr.is_set = SET;
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

    for (index = 0; index < 9; index++)
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

  /* Initialize and arm the global (forgotten) timer. */
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  forgotten_timer = timers.forgotten;

  /* Initialize the timeout message when the x/X option is set. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
      {
        char *s = "[     s before selecting the word \"";

        timeout_message = xcalloc(1, 4 + strlen(s) + strlen(timeout_word));

        strcpy(timeout_message, s);
        strcat(timeout_message, timeout_word);
        strcat(timeout_message, "\"]");

        break;
      }

      default:
        /* The other cases are impossible due to options analysis. */
        /* ''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        timeout_message = xstrdup("      "); /* Just in case. */
    }

    timeout_seconds = xcalloc(1, 6);
    snprintf(timeout_seconds, 6, "%5u", timeout.initial_value / FREQ);
    memcpy(timeout_message + 1, timeout_seconds, 5);

    message_lines_list = ll_new();

    if (message)
    {
      long len;

      get_message_lines(message,
                        message_lines_list,
                        &message_max_width,
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
    get_message_lines(message,
                      message_lines_list,
                      &message_max_width,
                      &message_max_len);
  }

  /* Force the maximum number of window's line if -n is used. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term.nlines <= win.message_lines)
  {
    win.message_lines = term.nlines - 1;
    win.max_lines     = 1;
  }
  else if (win.asked_max_lines >= 0)
  {
    if (win.asked_max_lines == 0)
      win.max_lines = term.nlines - win.message_lines - 1;
    else
    {
      if (win.asked_max_lines > term.nlines - win.message_lines)
        win.max_lines = term.nlines - win.message_lines - 1;
      else
        win.max_lines = win.asked_max_lines;
    }
  }
  else /* -n was not used. Set win.asked_max_lines to its default value. */
    win.asked_max_lines = win.max_lines;

  /* Allocate the memory for our words structures. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  word_a = xmalloc(WORDSCHUNK * sizeof(word_t));

  /* Fill an array of word_t elements obtained from stdin. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  tab_real_max_size = 0;
  tab_max_size      = 0;
  min_size          = 0;

  /* Parse the list of glyphs to be zapped (option -z). */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  zapped_glyphs_list = ll_new();
  if (zg != NULL)
  {
    int   len;
    char *zg_ptr = zg;
    char *tmp;

    len = mblen(zg_ptr, MB_CUR_MAX);

    while (len != 0)
    {
      tmp = xmalloc(len + 1);
      memcpy(tmp, zg_ptr, len);
      tmp[len] = '\0';
      ll_append(zapped_glyphs_list, tmp);

      zg_ptr += len;
      len = mblen(zg_ptr, 4);
    }
  }

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
    int   len;
    char *iws_ptr = iws;
    char *tmp;

    len = mblen(iws_ptr, MB_CUR_MAX);

    while (len != 0)
    {
      tmp = xmalloc(len + 1);
      memcpy(tmp, iws_ptr, len);
      tmp[len] = '\0';
      ll_append(word_delims_list, tmp);

      iws_ptr += len;
      len = mblen(iws_ptr, 4);
    }
  }

  /* Parse the line separators string (option -L). If it is empty then */
  /* the standard delimiter (newline) is used. Each of its UTF-8       */
  /* sequences are stored in a linked list.                            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  line_delims_list = ll_new();

  /* A default line separator is set to '\n' except in tab_mode */
  /* where it should be explicitly set.                         */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ils == NULL && !win.tab_mode)
    ll_append(line_delims_list, "\n");
  else
  {
    int   len;
    char *ils_ptr = ils;
    char *tmp;

    len = mblen(ils_ptr, MB_CUR_MAX);

    while (len != 0)
    {
      tmp = xmalloc(len + 1);
      memcpy(tmp, ils_ptr, len);
      tmp[len] = '\0';
      ll_append(line_delims_list, tmp);

      /* Add this record delimiter as a word delimiter. */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      if (ll_find(word_delims_list, tmp, buffer_cmp) == NULL)
        ll_append(word_delims_list, tmp);

      ils_ptr += len;
      len = mblen(ils_ptr, 4);
    }
  }

  /* Initialize the first chunks of the arrays which will contain the */
  /* maximum length of each column in column mode.                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    long ci; /* Column index. */

    col_real_max_size = xmalloc(COLSCHUNK * sizeof(long));
    col_max_size      = xmalloc(COLSCHUNK * sizeof(long));

    for (ci = 0; ci < COLSCHUNK; ci++)
      col_real_max_size[ci] = col_max_size[ci] = 0;

    col_index = cols_number = 0;
  }

  /* Compile the regular expression patterns. */
  /* """""""""""""""""""""""""""""""""""""""" */
  if (daccess_np
      && regcomp(&daccess_np_re, daccess_np, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", daccess_np);

    exit(EXIT_FAILURE);
  }

  if (daccess_up
      && regcomp(&daccess_up_re, daccess_up, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", daccess_up);

    exit(EXIT_FAILURE);
  }

  if (include_pattern != NULL
      && regcomp(&include_re, include_pattern, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", include_pattern);

    exit(EXIT_FAILURE);
  }

  if (exclude_pattern != NULL
      && regcomp(&exclude_re, exclude_pattern, REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", exclude_pattern);

    exit(EXIT_FAILURE);
  }

  if (first_word_pattern != NULL
      && regcomp(&first_word_re, first_word_pattern, REG_EXTENDED | REG_NOSUB)
           != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", first_word_pattern);

    exit(EXIT_FAILURE);
  }

  if (last_word_pattern != NULL
      && regcomp(&last_word_re, last_word_pattern, REG_EXTENDED | REG_NOSUB)
           != 0)
  {
    fprintf(stderr, "%s: Bad regular expression.\n", last_word_pattern);

    exit(EXIT_FAILURE);
  }

  for (index = 0; index < 9; index++)
  {
    if (special_pattern[index] != NULL
        && regcomp(&special_re[index],
                   special_pattern[index],
                   REG_EXTENDED | REG_NOSUB)
             != 0)
    {
      fprintf(stderr, "%s: Bad regular expression.\n", special_pattern[index]);

      exit(EXIT_FAILURE);
    }
  }

  /* Parse the post-processing patterns and extract its values. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (early_sed_list != NULL)
  {
    ll_node_t *node = early_sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -ES argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i].\n");

        exit(EXIT_FAILURE);
      }

      node = node->next;
    }
  }

  if (sed_list != NULL)
  {
    ll_node_t *node = sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -S argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i].\n");

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
    ll_node_t *node = include_sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -I argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i].\n");

        exit(EXIT_FAILURE);
      }
      if (!include_visual_only && ((sed_t *)(node->data))->visual)
        include_visual_only = 1;

      node = node->next;
    }
  }

  if (exclude_sed_list != NULL)
  {
    ll_node_t *node = exclude_sed_list->head;

    while (node != NULL)
    {
      if (!parse_sed_like_string((sed_t *)(node->data)))
      {
        fprintf(stderr,
                "Bad -E argument. Must be something like: "
                "/regex/repl_string/[g][v][s][i].\n");

        exit(EXIT_FAILURE);
      }
      if (!exclude_visual_only && ((sed_t *)(node->data))->visual)
        exclude_visual_only = 1;

      node = node->next;
    }
  }

  /* Parse the row selection string if any. */
  /* """""""""""""""""""""""""""""""""""""" */
  if (rows_selector_list != NULL)
  {
    ll_node_t   *node_selector = rows_selector_list->head;
    ll_node_t   *node;
    filters_t    filter_type;
    attr_elem_t *elem;

    rows_filter_type = UNKNOWN_FILTER;
    while (node_selector != NULL)
    {
      char *unparsed;

      rows_selector = node_selector->data;

      parse_selectors(rows_selector,
                      &filter_type,
                      &unparsed,
                      &inc_row_interval_list,
                      &inc_row_regex_list,
                      &exc_row_interval_list,
                      &exc_row_regex_list,
                      &al_row_interval_list,
                      &al_row_regex_list,
                      &ar_row_interval_list,
                      &ar_row_regex_list,
                      &ac_row_interval_list,
                      &ac_row_regex_list,
                      &at_row_interval_list,
                      &at_row_regex_list,
                      &default_alignment,
                      &win,
                      &misc,
                      &term);

      if (*unparsed != '\0')
      {
        fprintf(stderr,
                "Bad or not allowed row selection argument. "
                "Unparsed part: %s\n",
                unparsed);

        exit(EXIT_FAILURE);
      }

      if (rows_filter_type == UNKNOWN_FILTER)
        rows_filter_type = filter_type;

      node_selector = node_selector->next;

      free(unparsed);
    }
    optimize_an_interval_list(inc_row_interval_list);
    optimize_an_interval_list(exc_row_interval_list);
    optimize_an_interval_list(al_row_interval_list);
    optimize_an_interval_list(ar_row_interval_list);
    optimize_an_interval_list(ac_row_interval_list);

    if (at_row_interval_list != NULL)
    {
      node = at_row_interval_list->head;
      while (node)
      {
        elem = node->data;
        optimize_an_interval_list(elem->list);
        node = node->next;
      }
    }
  }

  /* Parse the column selection string if any. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (cols_selector_list != NULL)
  {
    filters_t    filter_type, cols_filter_type;
    interval_t  *data;
    ll_node_t   *node;
    ll_node_t   *node_selector = cols_selector_list->head;
    attr_elem_t *elem;

    cols_filter = xmalloc(limits.cols);

    cols_filter_type = UNKNOWN_FILTER;
    while (node_selector != NULL)
    {
      char *unparsed;

      cols_selector = node_selector->data;

      parse_selectors(cols_selector,
                      &filter_type,
                      &unparsed,
                      &inc_col_interval_list,
                      &inc_col_regex_list,
                      &exc_col_interval_list,
                      &exc_col_regex_list,
                      &al_col_interval_list,
                      &al_col_regex_list,
                      &ar_col_interval_list,
                      &ar_col_regex_list,
                      &ac_col_interval_list,
                      &ac_col_regex_list,
                      &at_col_interval_list,
                      &at_col_regex_list,
                      &default_alignment,
                      &win,
                      &misc,
                      &term);

      if (*unparsed != '\0')
      {
        fprintf(stderr,
                "Bad or not allowed column selection argument. "
                "Unparsed part: %s\n",
                unparsed);

        exit(EXIT_FAILURE);
      }

      optimize_an_interval_list(inc_col_interval_list);
      optimize_an_interval_list(exc_col_interval_list);

      if (cols_filter_type == UNKNOWN_FILTER)
        cols_filter_type = filter_type;

      /* Only initialize the whole set when -C is encountered for the */
      /* first time.                                                  */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (cols_filter_type == INCLUDE_FILTER)
        memset(cols_filter, SOFT_EXCLUDE_MARK, limits.cols);
      else
        memset(cols_filter, SOFT_INCLUDE_MARK, limits.cols);

      /* Process the explicitly included columns intervals. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (inc_col_interval_list != NULL)
        for (node = inc_col_interval_list->head; node; node = node->next)
        {
          data = node->data;

          if (data->low >= limits.cols)
            break;

          if (data->high >= limits.cols)
            data->high = limits.cols - 1;

          memset(cols_filter + data->low,
                 INCLUDE_MARK,
                 data->high - data->low + 1);
        }

      /* Process the explicitly excluded column intervals. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
      if (exc_col_interval_list != NULL)
        for (node = exc_col_interval_list->head; node; node = node->next)
        {
          data = node->data;

          if (data->low >= limits.cols)
            break;

          if (data->high >= limits.cols)
            data->high = limits.cols - 1;

          memset(cols_filter + data->low,
                 EXCLUDE_MARK,
                 data->high - data->low + 1);
        }

      node_selector = node_selector->next;

      free(unparsed);
    }

    optimize_an_interval_list(al_col_interval_list);
    optimize_an_interval_list(ar_col_interval_list);
    optimize_an_interval_list(ac_col_interval_list);

    if (at_col_interval_list != NULL)
    {
      node = at_col_interval_list->head;
      while (node)
      {
        elem = node->data;
        optimize_an_interval_list(elem->list);
        node = node->next;
      }
    }
  }

  /* parse the alignment selector list if any. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (aligns_selector_list != NULL)
  {
    ll_node_t *node_selector = aligns_selector_list->head;

    while (node_selector != NULL)
    {
      char *unparsed;

      aligns_selector = node_selector->data;

      parse_al_selectors(aligns_selector,
                         &unparsed,
                         &al_word_regex_list,
                         &ar_word_regex_list,
                         &ac_word_regex_list,
                         &default_alignment,
                         &misc);

      if (*unparsed != '\0')
      {
        fprintf(stderr,
                "Bad alignment selection argument. Unparsed part: %s\n",
                unparsed);

        exit(EXIT_FAILURE);
      }

      free(unparsed);

      node_selector = node_selector->next;
    }
  }

  /* Initialize the useful values needed to walk through */
  /* the rows intervals.                                 */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
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

  /* Set the head of the interval list. */
  /* """""""""""""""""""""""""""""""""" */
  if (inc_row_interval_list)
    inc_interval_node = inc_row_interval_list->head;
  else
    inc_interval_node = NULL;

  if (exc_row_interval_list)
    exc_interval_node = exc_row_interval_list->head;
  else
    exc_interval_node = NULL;

  /* And get the first interval.*/
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
  /* Get and process the input stream words.                      */
  /* In this pass, the different actions will occur:              */
  /* - A new word is read from stdin                              */
  /* - A new SOL and or EOL is possibly set                       */
  /* - A special level is possibly affected to the word just read */
  /* - The -R is taken into account                               */
  /* - The first part of the -C option is done                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  while ((word = read_word(input_file,
                           word_delims_list,
                           line_delims_list,
                           zapped_glyphs_list,
                           utf8_buffer,
                           &is_last,
                           &toggles,
                           &langinfo,
                           &win,
                           &limits,
                           &misc))
         != NULL)
  {
    int           selectable;
    int           is_first = 0;
    unsigned char special_level;
    int           row_inc_matched = 0;
    ll_node_t    *node;

    if (*word == '\0')
      continue;

    /* Early substitution. */
    /* """"""""""""""""""" */
    if (early_sed_list != NULL)
    {
      char *tmp;

      node = early_sed_list->head;

      while (node != NULL)
      {
        tmp = xstrdup(word);
        if (replace(word, (sed_t *)(node->data)))
        {

          free(word);
          word = xstrdup(word_buffer);

          if (((sed_t *)(node->data))->stop)
            break;
        }

        *word_buffer = '\0';
        node         = node->next;
        free(tmp);
      }
    }

    if (*word == '\0')
      continue;

    /* Manipulates the is_last flag word indicator to make this word      */
    /* the first or last one of the current line in column/line/tab mode. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode || win.line_mode || win.tab_mode)
    {
      if (first_word_pattern != NULL
          && regexec(&first_word_re, word, (int)0, NULL, 0) == 0)
        is_first = 1;

      if (last_word_pattern != NULL && !is_last
          && regexec(&last_word_re, word, (int)0, NULL, 0) == 0)
        is_last = 1;
    }

    /* Check if the word is special. */
    /* """"""""""""""""""""""""""""" */
    special_level = 0;
    for (index = 0; index < 9; index++)
    {
      if (special_pattern[index] != NULL
          && regexec(&special_re[index], word, (int)0, NULL, 0) == 0)
      {
        special_level = index + 1;
        break;
      }
    }

    /* Default selectable state. */
    /* """"""""""""""""""""""""" */
    selectable = SOFT_INCLUDE_MARK;

    /* For each new line check if the line is in the current   */
    /* interval or if we need to get the next interval if any .*/
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
        /* removed from the selectable list of words.                       */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        row_inc_matched = 0;

        /* Increment the line counter used to see if we are an include or */
        /* exclude set of lines.                                          */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        line_count++;

        /* Look if we need to use the next interval of the list. */
        /* ''''''''''''''''''''''''''''''''''''''''''''''''''''' */
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
      if (exc_interval && line_count >= exc_interval->low
          && line_count <= exc_interval->high)
        selectable = EXCLUDE_MARK;

      if (selectable != EXCLUDE_MARK && inc_interval
          && line_count >= inc_interval->low
          && line_count <= inc_interval->high)
      {
        selectable = INCLUDE_MARK;

        /* As the raw has been explicitly selected, record that so than */
        /* we can distinguish that from the implicit selection.         */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
        row_inc_matched = 1;
      }
    }

    /* Check if the all the words in the current row must be included or */
    /* excluded from the selectable set of words.                        */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (selectable != EXCLUDE_MARK)
    {
      /* Look in the excluded list of regular expressions. */
      /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (exc_row_regex_list != NULL)
      {
        regex_t *row_re;

        ll_node_t *row_regex_node = exc_row_regex_list->head;

        while (row_regex_node != NULL)
        {
          row_re = row_regex_node->data;
          if (regexec(row_re, word, (int)0, NULL, 0) == 0)
          {
            long c = count - 1;

            /* Mark all the next words of the line as excluded. */
            /* '''''''''''''''''''''''''''''''''''''''''''''''' */
            line_selected_by_regex = 1;
            line_excluded          = 1;

            /* Mark all the previous words of the line as excluded. */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
            while (c >= 0 && !word_a[c].is_last)
            {
              word_a[c].is_selectable = EXCLUDE_MARK;
              c--;
            }

            /* Mark the current word as not excluded. */
            /* '''''''''''''''''''''''''''''''''''''' */
            selectable = EXCLUDE_MARK;

            /* No need to continue as the line is already marked as */
            /* excluded.                                            */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
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
        regex_t *row_re;

        ll_node_t *row_regex_node = inc_row_regex_list->head;

        while (row_regex_node != NULL)
        {
          row_re = row_regex_node->data;
          if (regexec(row_re, word, (int)0, NULL, 0) == 0)
          {
            long c = count - 1;

            while (c >= 0 && !word_a[c].is_last)
            {
              /* Do not include an already excluded word. */
              /* """""""""""""""""""""""""""""""""""""""" */
              if (word_a[c].is_selectable)
                word_a[c].is_selectable = INCLUDE_MARK;

              c--;
            }

            /* Mark all the next words of the line as included. */
            /* '''''''''''''''''''''''''''''''''''''''''''''''' */
            line_selected_by_regex = 1;

            /* Mark all the previous words of the line as included. */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
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
        selectable = (row_def_selectable == EXCLUDE_MARK) ? SOFT_EXCLUDE_MARK
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
        if (exclude_pattern != NULL
            && regexec(&exclude_re, word, (int)0, NULL, 0) == 0)
          selectable = EXCLUDE_MARK;

        if (selectable != 0 && !line_selected_by_regex)
        {
          /* Check if the word will be included in the list of selectable */
          /* words or not.                                                */
          /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
          if (include_pattern != NULL)
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
          /* Check the limits. */
          /* ''''''''''''''''' */
          if (col_index == limits.cols)
          {
            fprintf(stderr,
                    "The number of columns has reached the limit of %ld.\n",
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

            /* Initialize the max size for the new columns. */
            /* '''''''''''''''''''''''''''''''''''''''''''' */
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
        long ci; /* column index. */

        regex_t *col_re;

        if (cols_filter[col_index - 1] == EXCLUDE_MARK)
          selectable = EXCLUDE_MARK;
        else
        {
          if (exc_col_regex_list != NULL)
          {
            /* Some columns must be excluded by regex. */
            /* ''''''''''''''''''''''''''''''''''''''' */
            ll_node_t *col_regex_node = exc_col_regex_list->head;

            while (col_regex_node != NULL)
            {
              col_re = col_regex_node->data;

              if (regexec(col_re, word, (int)0, NULL, 0) == 0)
              {
                cols_filter[col_index - 1] = EXCLUDE_MARK;
                selectable                 = EXCLUDE_MARK;

                /* Mark non selectable the items above in the column. */
                /* '''''''''''''''''''''''''''''''''''''''''''''''''' */
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
            /* Some columns must be included by regex. */
            /* ''''''''''''''''''''''''''''''''''''''' */
            ll_node_t *col_regex_node = inc_col_regex_list->head;

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

    /* Initialize the alignment information of each column to be 'left'. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    col_attrs = xmalloc(cols_number * sizeof(attrib_t *));
    for (long ci = 0; ci < cols_number; ci++)
      col_attrs[ci] = NULL;

    /* Store some known values in the current word's structure. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_a[count].start = word_a[count].end = 0;

    word_a[count].str           = word;
    word_a[count].is_selectable = selectable;

    word_a[count].special_level = special_level;
    word_a[count].is_matching   = 0;
    word_a[count].is_numbered   = 0;
    word_a[count].tag_order     = 0;
    word_a[count].tag_id        = 0;
    word_a[count].iattr         = NULL;

    if (win.col_mode || win.line_mode || win.tab_mode)
    {
      /* Set the last word in line indicator when in */
      /* column/line/tab mode.                       */
      /* ''''''''''''''''''''''''''''''''''''''''''' */
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
    if (count + 1 >= limits.words)
    {
      fprintf(stderr,
              "The number of read words has reached the limit of %ld.\n",
              limits.words);

      exit(EXIT_FAILURE);
    }

    count++;

    if (count % WORDSCHUNK == 0)
      word_a = xrealloc(word_a, (count + WORDSCHUNK) * sizeof(word_t));
  }

  /* Early exit if there is no input or if no word is selected. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (count == 0)
    exit(EXIT_FAILURE);

  /* Ignore SIGINT */
  /* """"""""""""" */
  sigaddset(&sigs, SIGINT);
  sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

  /* The last word is always the last of its line. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
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
  /* - Insert the word in a TST (Ternary Search Tree) index to facilitate   */
  /*   word search (each node pf the TST will contain an UTF-8 glyph).      */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  col_index = 0;
  for (wi = 0; wi < count; wi++)
  {
    char    *unaltered_word;
    long     size;
    long     word_len;
    wchar_t *tmpw;
    word_t  *word;
    long     s;
    long     len;
    char    *expanded_word;
    long     i;

    /* If the column section argument is set, then adjust the final        */
    /* selectable attribute  according to the already set words and column */
    /* selectable flag contents.                                           */
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
      if ((daccess.mode & DA_TYPE_POS) && daccess.size > 0
          && daccess.size > daccess.length)
        daccess.length = daccess.size;

      /* Auto determination of the length of the selector */
      /* with DA_TYPE_AUTO.                               */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((daccess.mode & DA_TYPE_AUTO) && daccess.length == -2)
      {
        long n = count + daccess_index - 1;

        daccess.length = 0;

        while (n)
        {
          n /= 10;
          daccess.length++;
        }
      }

      /* Set the full length of the prefix in case of numbering. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (daccess.length > 0)
        daccess.flength = 3 + daccess.length;

      if (word->is_selectable != EXCLUDE_MARK
          && word->is_selectable != SOFT_EXCLUDE_MARK)
      {
        char *selector;
        char *tmp;
        long *word_pos = xmalloc(sizeof(long));
        int   may_number;
        long  wlen;

        wlen = strlen(word->str) + 4 + daccess.length;
        tmp  = xmalloc(wlen);

        if (!my_isempty((unsigned char *)word->str))
        {
          *word_pos = wi;

          tmp[0] = ' ';

          /* Check if the word is eligible to the numbering process. */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
            if ((daccess.mode & DA_TYPE_POS) && !word->is_numbered
                && daccess.size > 0
                && (daccess.offset + daccess.size + daccess.ignore)
                     <= utf8_strlen(word->str))
            {
              long  selector_value;  /* numerical value of the         *
                                      | extracted selector.            */
              long  selector_offset; /* offset in byte to the selector *
                                      | to extract.                    */
              char *ptr;             /* points just after the selector *
                                      | to extract.                    */
              long  plus_offset;     /* points to the first occurrence *
                                      | of a number in word->str after *
                                      | the offset given.              */

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
              /* it according to daccess.alignment.               */
              /* """""""""""""""""""""""""""""""""""""""""""""""" */
              if (sscanf(selector, "%ld", &selector_value) == 1)
              {
                snprintf(selector, daccess.size + 1, "%ld", selector_value);

                snprintf(tmp + 1,
                         wlen,
                         "%*ld",
                         daccess.alignment == 'l' ? -daccess.length
                                                  : daccess.length,
                         selector_value);

                /* Overwrite the end of the word to erase */
                /* the selector.                          */
                /* """""""""""""""""""""""""""""""""""""" */
                my_strcpy(ptr,
                          ptr + daccess.size
                            + utf8_offset(ptr + daccess.size, daccess.ignore));

                /* Modify the word according to the 'h' directive */
                /* of -D.                                         */
                /* """""""""""""""""""""""""""""""""""""""""""""" */
                if (daccess.head == 'c')
                  /* h:c is present cut the leading characters */
                  /* before the selector.                      */
                  /* ''''''''''''''''''''''''''''''''''''''''' */
                  memmove(word->str, ptr, strlen(ptr) + 1);
                else if (daccess.head == 't')
                {
                  /* h:t is present trim the leading characters   */
                  /* before the selector if they are ' ' or '\t'. */
                  /* '''''''''''''''''''''''''''''''''''''''''''' */
                  char *p = word->str;

                  while (p != ptr && (*p == ' ' || *p == '\t'))
                    p++;

                  if (p == ptr)
                    memmove(word->str, ptr, strlen(ptr) + 1);
                }

                ltrim(selector, " ");
                rtrim(selector, " ", 0);

                tst_daccess = tst_insert(tst_daccess,
                                         utf8_strtowcs(selector),
                                         word_pos);

                if (daccess.follow == 'y')
                  daccess_index = selector_value + 1;

                word->is_numbered = 1;
              }
              free(selector);
            }

            /* Try to number this word if it is still non numbered and */
            /* the -N/-U option is given.                              */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (!word->is_numbered && (daccess.mode & DA_TYPE_AUTO))
            {
              snprintf(tmp + 1,
                       wlen,
                       "%*ld",
                       daccess.alignment == 'l' ? -daccess.length
                                                : daccess.length,
                       daccess_index);

              selector = xstrdup(tmp + 1);
              ltrim(selector, " ");
              rtrim(selector, " ", 0);

              /* Insert it in the tst tree containing the selector's */
              /* digits.                                             */
              /* ''''''''''''''''''''''''''''''''''''''''''''''''''' */
              tst_daccess = tst_insert(tst_daccess,
                                       utf8_strtowcs(selector),
                                       word_pos);
              daccess_index++;

              free(selector);

              word->is_numbered = 1;
            }
          }

          /* Fill the space taken by the numbering by space if the word */
          /* is not numbered.                                           */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
          /* Make sure that the prefix of empty word is blank */
          /* as they may be display in column mode.           */
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
        /* Should we also add space at the beginning of excluded words? */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (daccess.padding == 'a')
        {
          char *tmp = xmalloc(strlen(word->str) + 4 + daccess.length);
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

    /* Save the original word. */
    /* """"""""""""""""""""""" */
    unaltered_word = xstrdup(word->str);

    /* Possibly modify the word according to -S/-I/-E arguments. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    {
      ll_node_t *node = NULL;
      char      *tmp;

      /* Manage the -S case. */
      /* """"""""""""""""""" */
      if (sed_list != NULL)
      {
        node = sed_list->head;

        while (node != NULL)
        {
          tmp = xstrndup(word->str, daccess.flength);
          if (replace(word->str + daccess.flength, (sed_t *)(node->data)))
          {

            free(word->str);
            memmove(word_buffer + daccess.flength,
                    word_buffer,
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
        /* Manage the -I/-E case. */
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
            memmove(word_buffer + daccess.flength,
                    word_buffer,
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
      long len;

      if (daccess.padding == 'a')
        len = daccess.flength;
      else
        len = 0;

      if (*(word->str + len) == '\0')
        exit(EXIT_FAILURE);
    }

    /* Alter the word just read be replacing special chars  by their */
    /* escaped equivalents.                                          */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len = strlen(word->str);

    expanded_word = xmalloc(5 * word_len + 1);
    len = expand(word->str, expanded_word, &langinfo, &toggles, &misc);

    /* Update it if needed. */
    /* '''''''''''''''''''' */
    if (strcmp(expanded_word, word->str) != 0)
    {
      word_len = len;
      free(word->str);
      word->str = xstrdup(expanded_word);
    }

    free(expanded_word);

    word->len_mb = utf8_strlen(word->str);

    if (win.col_mode)
    {
      /* Update the max values of col_real_max_size[col_index] */
      /* and col_max_size[col_index].                          */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((s = (long)word_len) > col_real_max_size[col_index])
      {
        col_real_max_size[col_index] = s;

        /* Also update the real max size of all columns seen. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""" */
        if (s > cols_real_max_size)
          cols_real_max_size = s;
      }

      s = (long)mbstowcs(NULL, word->str, 0);
      s = my_wcswidth((tmpw = utf8_strtowcs(word->str)), s);
      free(tmpw);

      if (s > col_max_size[col_index])
      {
        col_max_size[col_index] = s;

        /* Also update the max size of all columns seen. */
        /* """"""""""""""""""""""""""""""""""""""""""""" */
        if (s > cols_max_size)
          cols_max_size = s;
      }
      /* Update the size of the longest expanded word. */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      word_real_max_size = cols_real_max_size + 1;
    }
    else if (win.tab_mode)
    {
      /* Store the new max number of bytes in a word       */
      /* and update the size of the longest expanded word. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
      if ((long)word_len > tab_real_max_size)
        word_real_max_size = tab_real_max_size = (long)word_len;

      /* Store the new max word width. */
      /* """"""""""""""""""""""""""""" */
      size = (long)mbstowcs(NULL, word->str, 0);

      if ((size = my_wcswidth((tmpw = utf8_strtowcs(word->str)), size))
          > tab_max_size)
        tab_max_size = size;

      free(tmpw);
    }
    else if (word_real_max_size < word_len)
      /* Update the size of the longest expanded word. */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      word_real_max_size = word_len;

    /* When the visual only flag is set, we keep the unaltered word so */
    /* that it can be restored even if its visual and searchable       */
    /* representation may have been altered by the previous code.      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

    /* Record the length of the word in bytes. This information will be */
    /* used if the -k option (keep spaces ) is not set.                 */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word->len = strlen(word->str);

    /* Save the non modified word in .orig if it has been altered. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

  /* Set the minimum width of a column (-w and -t or -c option). */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
    else /* Column mode. */
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
    for (wi = 0; wi < count - offset; wi++)
    {
      long len;

      while (wi + offset < count)
      {
        if (daccess.padding == 'a' || word_a[wi + offset].is_numbered)
          len = daccess.flength;
        else
          len = 0;

        if (!my_isempty((unsigned char *)(word_a[wi + offset].str + len)))
          break;

        /* Keep non selectable empty words to allow special effects. */
        /* ''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
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

  /* Allocate the space for the satellites arrays. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  line_nb_of_word_a     = xmalloc(count * sizeof(long));
  first_word_in_line_a  = xmalloc(count * sizeof(long));
  shift_right_sym_pos_a = xmalloc(count * sizeof(long));

  /* Fourth pass:                                                         */
  /* When in column or tabulating mode, we need to adjust the length of   */
  /* all the words by adding the right number of spaces so that they will */
  /* be aligned correctly. In column mode the size of each column is      */
  /* variable; in tabulate mode it is constant.                           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    char *temp;

    /* Sets all columns to the same size when -w and -c are both set. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.wide)
      for (col_index = 0; col_index < cols_number; col_index++)
      {
        col_max_size[col_index]      = cols_max_size;
        col_real_max_size[col_index] = cols_real_max_size;
      }

    /* Total space taken by all the columns plus the gutter. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
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
      long        s1, s2;
      long        word_width;
      wchar_t    *w;
      regex_t     re;
      ll_node_t  *node;
      interval_t *interval;

      /* Does this word matched by one of the alignment regex?         */
      /* If yes, then add the current column number to the list of the */
      /* corresponding column_alignment list                           */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      ll_t  *regex_list_a[3]    = { al_col_regex_list,
                                    ar_col_regex_list,
                                    ac_col_regex_list };
      ll_t **interval_list_a[3] = { &al_col_interval_list,
                                    &ar_col_interval_list,
                                    &ac_col_interval_list };

      for (int i = 0; i < 3; i++) /* For each regex list. */
      {
        if (regex_list_a[i] == NULL)
          continue;

        node = regex_list_a[i]->head;
        while (node) /* For each RE in the list. */
        {
          re = *(regex_t *)(node->data);
          if (regexec(&re, word_a[wi].str + daccess.flength, (int)0, NULL, 0)
              == 0)
          {
            int already_aligned = 0;

            /* We have a match. */
            /* '''''''''''''''' */
            interval      = xmalloc(sizeof(interval_t));
            interval->low = interval->high = col_index;

            /* Look if the column has already been inserted in another */
            /* interval list.                                          */
            /* ''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
            for (int j = 0; j < 3; j++)
            {
              interval_t inter;
              ll_node_t *n;

              /* Quick continuation. */
              /* ''''''''''''''''''' */
              if (i == j || *interval_list_a[j] == NULL)
                continue;

              n = (*interval_list_a[j])->head;
              while (n)
              {
                inter = *(interval_t *)(n->data);
                if (col_index >= inter.low && col_index <= inter.high)
                {
                  already_aligned = 1; /* This column is already aligned. */
                  break;
                }
                n = n->next;
              }
            }

            if (!already_aligned)
            {
              /* Append a new interval containing the current column number */
              /* in the interval list matching the regex list.              */
              /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
              if (*interval_list_a[i] == NULL)
                *interval_list_a[i] = ll_new();

              ll_append(*interval_list_a[i], interval);
            }
          }
          node = node->next;
        }
      }

      /* Process regex based attributes and fill the attribute columns */
      /* list accordingly.                                             */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (at_col_regex_list != NULL)
      {
        attrib_t    *attr;
        attr_elem_t *elem;
        ll_node_t   *regex_node;
        ll_node_t   *interval_node;

        node = at_col_regex_list->head;
        while (node)
        {
          elem = node->data;
          attr = elem->attr;

          regex_node = elem->list->head;
          while (regex_node)
          {
            re = *(regex_t *)(regex_node->data);
            if (regexec(&re, word_a[wi].str + daccess.flength, (int)0, NULL, 0)
                == 0)
            {
              /* We have a match. */
              /* '''''''''''''''' */
              attr_elem_t *new_elem;

              if (col_attrs[col_index] != NULL)
                break;

              new_elem       = xmalloc(sizeof(attr_elem_t));
              new_elem->attr = attr;

              interval      = xmalloc(sizeof(interval_t));
              interval->low = interval->high = col_index;

              col_attrs[col_index] = attr;

              if (at_col_interval_list == NULL)
                at_col_interval_list = ll_new();

              if ((interval_node = ll_find(at_col_interval_list,
                                           elem,
                                           attr_elem_cmp))
                  != NULL)
              {
                ll_append(((attr_elem_t *)(interval_node->data))->list,
                          interval);
              }
              else
              {
                new_elem->list = ll_new();
                ll_append(new_elem->list, interval);
                ll_append(at_col_interval_list, new_elem);
              }
            }
            regex_node = regex_node->next;
          }
          node = node->next;
        }
      }

      s1         = (long)strlen(word_a[wi].str);
      word_width = mbstowcs(NULL, word_a[wi].str, 0);
      s2         = my_wcswidth((w = utf8_strtowcs(word_a[wi].str)), word_width);
      free(w);

      /* Use the al_delim (0x05) character as a placeholder to preserve  */
      /* the internal spaces of the word if there are any.               */
      /* This value has been chosen because it cannot be part of a UTF-8 */
      /* sequence and is very unlikely to be part of a normal word from  */
      /* the input stream.                                               */
      /* This placeholder will be removed during the alignment phase.    */
      /*"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      temp = xcalloc(1, col_real_max_size[col_index] + s1 - s2 + 1);
      memset(temp, al_delim, col_max_size[col_index] + s1 - s2);
      memcpy(temp, word_a[wi].str, s1);
      temp[col_real_max_size[col_index] + s1 - s2] = '\0';
      free(word_a[wi].str);
      word_a[wi].str = temp;

      if (word_a[wi].is_last)
        col_index = 0;
      else
        col_index++;
    }
    optimize_an_interval_list(al_col_interval_list);
    optimize_an_interval_list(ar_col_interval_list);
    optimize_an_interval_list(ac_col_interval_list);
  }
  else if (win.tab_mode)
  {
    char *temp;

    if (tab_max_size < min_size)
    {
      tab_max_size = min_size;
      if (tab_max_size > tab_real_max_size)
        tab_real_max_size = tab_max_size;
    }

    for (wi = 0; wi < count; wi++)
    {
      long     s1, s2;
      long     word_width;
      wchar_t *w;

      s1         = (long)strlen(word_a[wi].str);
      word_width = mbstowcs(NULL, word_a[wi].str, 0);
      s2         = my_wcswidth((w = utf8_strtowcs(word_a[wi].str)), word_width);
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
    long    *data;
    wchar_t *w;
    ll_t    *list;

    if (word_a[wi].is_selectable == SOFT_EXCLUDE_MARK)
      word_a[wi].is_selectable = EXCLUDE_MARK;

    /* If the word is selectable insert it in the TST tree */
    /* with its associated index in the input stream.      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[wi].is_selectable)
    {
      data  = xmalloc(sizeof(long));
      *data = wi;

      /* Create a wide characters string from the word screen */
      /* representation to be able to store in in the TST.    */
      /* Note that the direct access selector,if any, is not  */
      /* stored.                                              */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_a[wi].is_numbered)
        w = utf8_strtowcs(word_a[wi].str + daccess.flength);
      else
        w = utf8_strtowcs(word_a[wi].str);

      /* If we didn't already encounter this word, then create a new */
      /* entry in the TST for it and store its index in its list.    */
      /* Otherwise, add its index in its index list.                 */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

  /* Sixth pass: Apply alignment rules in column modes.                    */
  /* The column alignments, based on regular expressions, have already     */
  /* been processed in the fourth pass which converted this information    */
  /* by adding new ranges to the three lists of column ranges.             */
  /*                                                                       */
  /* It remains to interpret these lists of column intervals and the lists */
  /* of intervals and regular expressions for the rows.                    */
  /*                                                                       */
  /* To do this, a working table (aligned_a) is created and reset for      */
  /* each row to store the statistics of alignments already processed,     */
  /* taking into account the previous result and the order in which the    */
  /* row and column alignment requests were made.                          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    long       row_index = 0;
    interval_t interval;

    alignment_t alignment;      /* Future value of the word alignment. */
    alignment_t word_alignment; /* Specific word alignment.            */
    alignment_t row_alignment;  /* current row word alignments.        */

    char *str, *tstr;

    col_index = 0;

    ll_node_t *cur_word_node_a[3];

    ll_t *col_interval_list_a[3] = { al_col_interval_list,
                                     ar_col_interval_list,
                                     ac_col_interval_list };

    ll_node_t *cur_col_node_a[3] = {
      al_col_interval_list != NULL ? al_col_interval_list->head : NULL,
      ar_col_interval_list != NULL ? ar_col_interval_list->head : NULL,
      ac_col_interval_list != NULL ? ac_col_interval_list->head : NULL
    };

    ll_node_t *cur_row_interval_node_a[3] = {
      al_row_interval_list != NULL ? al_row_interval_list->head : NULL,
      ar_row_interval_list != NULL ? ar_row_interval_list->head : NULL,
      ac_row_interval_list != NULL ? ac_row_interval_list->head : NULL
    };

    ll_node_t *cur_row_regex_node_a[3];

    alignment_t const alignment_a[3] = { AL_LEFT, AL_RIGHT, AL_CENTERED };

    char *aligned_a; /* Array of indicators used to remember that a word *
                      | has been aligned with -al in a row.              */

    /* Initialize each chars of aligned_a with No ('N'). */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    aligned_a = xmalloc(cols_number);

    for (int i = 0; i < cols_number; i++)
      aligned_a[i] = 'N';

    row_alignment = AL_NONE;

    for (wi = 0; wi < count; wi++)
    {
      word_alignment = AL_NONE;

      if (row_alignment != AL_NONE)
        alignment = row_alignment;
      else
        alignment = default_alignment;

      str  = xstrdup(word_a[wi].str + daccess.flength);
      tstr = xstrdup(str);

      rtrim(tstr, "\x05", 0);
      ltrim(tstr, "\x05");

      /* First check if the current word is matched by a word specified */
      /* regular expression set by the * alignment option.              */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

      cur_word_node_a[0] = al_word_regex_list != NULL ? al_word_regex_list->head
                                                      : NULL;

      cur_word_node_a[1] = ar_word_regex_list != NULL ? ar_word_regex_list->head
                                                      : NULL;

      cur_word_node_a[2] = ac_word_regex_list != NULL ? ac_word_regex_list->head
                                                      : NULL;

      cur_row_regex_node_a[0] = al_row_regex_list != NULL
                                  ? al_row_regex_list->head
                                  : NULL;

      cur_row_regex_node_a[1] = ar_row_regex_list != NULL
                                  ? ar_row_regex_list->head
                                  : NULL;

      cur_row_regex_node_a[2] = ac_row_regex_list != NULL
                                  ? ac_row_regex_list->head
                                  : NULL;

      /* Process the word alignment regex lists. */
      /* """"""""""""""""""""""""""""""""""""""" */
      for (int i = 0; i < 3; i++)
      {
        while (word_alignment == AL_NONE && cur_word_node_a[i] != NULL)
        {
          regex_t *re;

          re = (regex_t *)(cur_word_node_a[i]->data);

          if (regexec(re, tstr, (int)0, NULL, 0) == 0)
          {
            word_alignment = alignment_a[i];

            /* Mark this word as aligned in this row. */
            /* """""""""""""""""""""""""""""""""""""" */
            aligned_a[col_index] = 'Y';

            break; /* Early exit of the while loop. */
          }

          cur_word_node_a[i] = cur_word_node_a[i]->next;
        }
      }

      /* Process the alignment lists for columns and increment     */
      /* their current pointers when needed.                       */
      /* The current interval pointer cur_col_node_a[i] is only    */
      /* modified to point to the next one of the list is the      */
      /* current column is greater than the  max column in the     */
      /* pointed interval.                                         */
      /* An already aligned column will not be realigned.          */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      for (int i = 0; i < 3; i++)
      {
        if (cur_col_node_a[i] != NULL)
        {
          interval = *(interval_t *)(cur_col_node_a[i]->data);
          if (aligned_a[col_index] == 'N' && col_index >= interval.low
              && col_index <= interval.high)
          {
            /* Tell that the word is already aligned when column */
            /* alignments have precedence over row alignments.   */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            if (toggles.cols_first)
              aligned_a[col_index] = 'Y';

            alignment = alignment_a[i];
          }
          else if (col_index > interval.high)
            cur_col_node_a[i] = cur_col_node_a[i]->next;
        }
        if (word_a[wi].is_last && col_interval_list_a[i] != NULL)
          cur_col_node_a[i] = col_interval_list_a[i]->head;
      }

      /* Process row interval and regex alignment lists. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      for (int i = 0; i < 3; i++)
      {
        /* Lists of intervals. */
        /* ''''''''''''''''''' */
        if (cur_row_interval_node_a[i] != NULL)
        {
          interval = *(interval_t *)(cur_row_interval_node_a[i]->data);
          if (row_alignment == AL_NONE && row_index >= interval.low
              && row_index <= interval.high)
          {
            row_alignment = alignment_a[i];
            if (aligned_a[col_index] == 'N')
              alignment = alignment_a[i];
          }
          else if (row_index > interval.high)
            cur_row_interval_node_a[i] = cur_row_interval_node_a[i]->next;
        }

        /* Lists of regular expression. */
        /* '''''''''''''''''''''''''''' */
        if (cur_row_regex_node_a[i] != NULL)
        {
          while (cur_row_regex_node_a[i] != NULL)
          {
            regex_t *re;

            re = (regex_t *)(cur_row_regex_node_a[i]->data);

            if (row_alignment == AL_NONE
                && regexec(re, tstr, (int)0, NULL, 0) == 0)
            {
              row_alignment = alignment_a[i];
              if (aligned_a[col_index] == 'N')
                alignment = alignment_a[i];

              /* Also aligns the previous words in the line to the */
              /* right, left or centre.                            */
              /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
              long j = wi;
              while (j > 0 && !word_a[j - 1].is_last)
              {
                j--;

                /* Do not realign words already aligned with -al */
                /* of if already aligned using a column RE when  */
                /* column alignments have precedence.            */
                /* ''''''''''''''''''''''''''''''''''''''''''''' */
                if (aligned_a[col_index - (wi - j)] == 'N')
                  align_word(&word_a[j],
                             alignment_a[i],
                             daccess.flength,
                             al_delim);
              }

              break; /* Early exit of the while loop. */
            }
            else if (toggles.rows_first && row_alignment != AL_NONE)
              alignment = row_alignment;

            cur_row_regex_node_a[i] = cur_row_regex_node_a[i]->next;
          }
        }
      }

      /* Force a word alignment if it is set for this word. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_alignment != AL_NONE)
        alignment = word_alignment;

      /* Do the alignment. */
      /* """"""""""""""""" */
      align_word(&word_a[wi], alignment, daccess.flength, al_delim);

      /* Adjusts things before a row change. */
      /* """"""""""""""""""""""""""""""""""" */
      if (word_a[wi].is_last || wi == count - 1) /* About to start a new row? */
      {
        row_index++;

        /* Re-initialize the array with No ('N'). */
        /* as this is a new row.                  */
        /* """""""""""""""""""""""""""""""""""""" */
        for (int i = 0; i < cols_number; i++)
        {
          aligned_a[i] = 'N';

          /* We can restore the spaces which are not part of the word */
          /* now that the row is fully processed.                     */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (wi - i >= 0)
            strrep(word_a[wi - i].str + daccess.flength, al_delim, ' ');
        }

        col_index     = 0; /* Restart the columns counter. */
        row_alignment = AL_NONE;
      }
      else
        col_index++;

      free(str);
      free(tstr);
    }
  }

  /* Seventh pass: sets default attributes. */
  /* """""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    if (at_row_interval_list != NULL || at_col_interval_list != NULL
        || at_row_regex_list != NULL || at_col_regex_list != NULL)
    {
      long         row_index = 0;
      interval_t  *interval;
      attrib_t    *attr;
      ll_t        *list;
      attr_elem_t *attr_elem;
      ll_node_t   *attr_elem_node;
      ll_node_t   *node;
      regex_t      re;

      col_index = 0;

      for (wi = 0; wi < count; wi++)
      {
        if (word_a[wi].iattr != NULL)
          continue;

        if (at_row_interval_list != NULL)
        {
          attr_elem_node = at_row_interval_list->head;
          while (attr_elem_node != NULL)
          {
            attr_elem = attr_elem_node->data;
            attr      = attr_elem->attr;
            list      = attr_elem->list; /* Cannot be null by construction. */
            node      = list->head;

            while (node)
            {
              interval = node->data;
              if (row_index >= interval->low && row_index <= interval->high)
              {
                word_a[wi].iattr = attr;
                goto early_row_exit;
              }
              node = node->next;
            }
            attr_elem_node = attr_elem_node->next;
          }
        }
      early_row_exit:

        if (at_col_interval_list != NULL)
        {
          attr_elem_node = at_col_interval_list->head;
          while (attr_elem_node != NULL)
          {
            attr_elem = attr_elem_node->data;
            attr      = attr_elem->attr;
            list      = attr_elem->list; /* Cannot be null by construction. */
            node      = list->head;
            while (node)
            {
              interval = node->data;
              if (col_index >= interval->low && col_index <= interval->high)
              {
                if (word_a[wi].iattr == NULL || toggles.cols_first)
                {
                  if (col_attrs[col_index] == NULL)
                  {
                    word_a[wi].iattr     = attr;
                    col_attrs[col_index] = attr;
                  }
                  else
                    word_a[wi].iattr = col_attrs[col_index];

                  goto early_col_exit;
                }
              }
              node = node->next;
            }
            attr_elem_node = attr_elem_node->next;
          }
        }
      early_col_exit:

        if (at_row_regex_list != NULL)
        {
          attr_elem_node = at_row_regex_list->head;
          while (attr_elem_node != NULL)
          {
            attr_elem = attr_elem_node->data;
            attr      = attr_elem->attr;
            list      = attr_elem->list; /* Cannot be null by construction. */
            node      = list->head;
            while (node)
            {
              re = *(regex_t *)(node->data);
              if (regexec(&re,
                          word_a[wi].str + daccess.flength,
                          (int)0,
                          NULL,
                          0)
                  == 0)
              {
                col_index = 0;
                while (wi > 0 && !word_a[wi - 1].is_last)
                  wi--;

                while (wi < count)
                {
                  if (toggles.cols_first && col_attrs[col_index] != NULL)
                    word_a[wi].iattr = col_attrs[col_index];
                  else
                    word_a[wi].iattr = attr;

                  col_index++;

                  if (word_a[wi].is_last)
                    break;
                  wi++;
                }
              }
              node = node->next;
            }
            attr_elem_node = attr_elem_node->next;
          }
        }

        /* Adjusts things before a row change. */
        /* """"""""""""""""""""""""""""""""""" */
        if (word_a[wi].is_last
            || wi == count - 1) /* About to start a new row? */
        {
          row_index++;

          col_index = 0; /* Restart the columns counter. */
        }
        else
          col_index++;
      }
    }
  }

  /* The word after the last one is set to NULL. */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  word_a[count].str = NULL;

  /* We can now allocate the space for our tmp_word work variable */
  /* augmented by the number of tabulation columns. This is not   */
  /* optimal but the loss is tiny and we have the guarantee that  */
  /* enough place will be allocated.                              */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tmp_word = xcalloc(1, word_real_max_size + tab_max_size + 1);

  search_data.off_a = xmalloc(word_real_max_size * sizeof(long));
  search_data.len_a = xmalloc(word_real_max_size * sizeof(long));

  win.start = 0; /* index of the first element in the *
                  | words array to be  displayed.     */

  /* We can now build the first metadata. */
  /* """""""""""""""""""""""""""""""""""" */
  last_line = build_metadata(&term, count, &win);

  /* Adjust the max number of lines in the windows */
  /* if it has not be explicitly set.              */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (line_nb_of_word_a[count - 1] < win.max_lines)
    win.max_lines = win.asked_max_lines = line_nb_of_word_a[count - 1] + 1;

  /* Index of the selected element in the array words                */
  /* The string can be:                                              */
  /*   "last"    The string "last"   put the cursor on the last word */
  /*   n         a number            put the cursor on the word n    */
  /*   /pref     /+a regexp          put the cursor on the first     */
  /*                                 word matching the prefix "pref" */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  for (wi = 0; wi < count; wi++)
  {
    long len;

    if (daccess.padding == 'a' || word_a[wi].is_numbered)
      len = daccess.flength;
    else
      len = 0;

    word_a[wi].bitmap = xcalloc(1, (word_a[wi].mb - len) / CHAR_BIT + 1);
  }

  /* Find the first selectable word (if any) in the input stream. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  first_selectable = 0;
  while (first_selectable < count && !word_a[first_selectable].is_selectable)
    first_selectable++;

  /* If not found, abort. */
  /* """""""""""""""""""" */
  if (first_selectable == count)
  {
    fprintf(stderr, "No selectable word found.\n");

    exit(EXIT_FAILURE);
  }

  /* Else find the last selectable word in the input stream. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  last_selectable = count - 1;
  while (last_selectable > 0 && !word_a[last_selectable].is_selectable)
    last_selectable--;

  if (pre_selection_index == NULL)
    /* Option -s was not used. */
    /* """"""""""""""""""""""" */
    current = first_selectable;
  else if (*pre_selection_index == '/')
  {
    /* A regular expression is expected. */
    /* """"""""""""""""""""""""""""""""" */
    regex_t re;

    if (regcomp(&re, pre_selection_index + 1, REG_EXTENDED | REG_NOSUB) != 0)
    {
      fprintf(stderr, "%s: Invalid regular expression.\n", pre_selection_index);

      exit(EXIT_FAILURE);
    }
    else
    {
      int   found = 0;
      char *word;

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
          long target;

          if (!found)
          {
            found   = 1;
            current = index;
          }

          /* Insert the index in the search array. */
          /* """"""""""""""""""""""""""""""""""""" */
          target = get_sorted_array_target_pos(matching_words_da,
                                               BUF_LEN(matching_words_da),
                                               index);
          if (target >= 0)
            BUF_INSERT(matching_words_da, target, index);
        }
      }

      if (!found)
        current = first_selectable;
    }
  }
  else if (*pre_selection_index == '=') /* exact search. */
  {
    /* An exact match is expected. */
    /* """"""""""""""""""""""""""" */
    wchar_t *w;

    ll_t      *list;
    ll_node_t *node;

    list = tst_search(tst_word, w = utf8_strtowcs(pre_selection_index + 1));
    if (list != NULL)
    {
      long target;

      node    = list->head;
      current = *(long *)(node->data);

      while (node)
      {
        /* Insert the index in the search array. */
        /* """"""""""""""""""""""""""""""""""""" */
        target = get_sorted_array_target_pos(matching_words_da,
                                             BUF_LEN(matching_words_da),
                                             *(long *)(node->data));
        if (target >= 0)
          BUF_INSERT(matching_words_da, target, *(long *)(node->data));

        node = node->next;
      }
    }
    else
      current = first_selectable;

    free(w);
  }
  else if (*pre_selection_index != '\0')
  {
    /* A prefix string or an index is expected. */
    /* """""""""""""""""""""""""""""""""""""""" */
    int   len;
    char *ptr = pre_selection_index;

    if (*ptr == '#')
    {
      /* An index is expected. */
      /* """"""""""""""""""""" */
      ptr++;

      if (sscanf(ptr, "%ld%n", &current, &len) == 1 && len == (int)strlen(ptr))
      {
        /* We got an index (numeric value). */
        /* """""""""""""""""""""""""""""""" */
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
        /* We got a special index (empty or last). */
        /* """"""""""""""""""""""""""""""""""""""" */
        current = last_selectable;
      else
      {
        fprintf(stderr, "%s: Invalid index.\n", ptr);

        exit(EXIT_FAILURE);
      }
    }
    else
    {
      int  found = 0;
      long target;

      /* A prefix is expected. */
      /* """"""""""""""""""""" */
      for (new_current = first_selectable; new_current < count; new_current++)
      {
        if (strprefix(word_a[new_current].str, ptr)
            && word_a[new_current].is_selectable)
        {
          if (!found)
          {
            current = new_current;
            found   = 1;
          }

          /* Insert the index in the search array. */
          /* """"""""""""""""""""""""""""""""""""" */
          target = get_sorted_array_target_pos(matching_words_da,
                                               BUF_LEN(matching_words_da),
                                               new_current);
          if (target >= 0)
            BUF_INSERT(matching_words_da, target, new_current);
        }
      }

      if (!found)
        current = first_selectable;
    }
  }
  else
    current = first_selectable;

  /* We now need to adjust the 'start'/'end' fields of the */
  /* structure 'win'.                                      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  set_win_start_end(&win, current, last_line);

  /* Re-associates /dev/tty with stdin and stdout. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (freopen("/dev/tty", "r", stdin) == NULL)
  {
    fprintf(stderr, "Unable to associate /dev/tty with stdin.\n");
    exit(EXIT_FAILURE);
  }

  old_fd1    = dup(1);
  old_stdout = fdopen(old_fd1, "w");

  setbuf(old_stdout, NULL);

  if (freopen("/dev/tty", "w", stdout) == NULL)
  {
    fprintf(stderr, "Unable to associate /dev/tty with stdout.\n");
    exit(EXIT_FAILURE);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  /* Make sure smenu runs in foreground. */
  /* """"""""""""""""""""""""""""""""""" */
  if (!is_in_foreground_process_group())
  {
    fprintf(stderr, "smenu cannot be launched in background.\n");
    exit(EXIT_FAILURE);
  }

  /* Set the characteristics of the terminal. */
  /* """""""""""""""""""""""""""""""""""""""" */
  setup_term(fileno(stdin), &old_in_attrs, &new_in_attrs);

  /* Make sure the input stream buffer is empty. */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  tcflush(0, TCIOFLUSH);

  if (!get_cursor_position(&row, &col))
  {
    fprintf(stderr,
            "The terminal does not have the capability to report "
            "the cursor position.\n");
    restore_term(fileno(stdin), &old_in_attrs);

    exit(EXIT_FAILURE);
  }

  /* Initialize the search buffer with tab_real_max_size+1 NULs  */
  /* It will never be reallocated, only cleared.                 */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  search_data.buf = xcalloc(1, word_real_max_size + 1 - daccess.flength);

  /* Hide the cursor. */
  /* """""""""""""""" */
  (void)tputs(TPARM1(cursor_invisible), 1, outch);

  /* Force the display to start at a beginning of line. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);
  if (term.curs_column > 1)
    puts("");

  /* Display the words window and its title for the first time. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  disp_message(message_lines_list,
               message_max_width,
               message_max_len,
               &term,
               &win,
               &langinfo);

  /* Before displaying the word windows for the first time when in    */
  /* column or line mode, we need to ensure that the word under the   */
  /* cursor will be visible by setting the number of the first column */
  /* to be displayed.                                                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode || win.line_mode)
  {
    long pos;
    long len;

    len = term.ncolumns - 3;

    /* Adjust win.first_column if the cursor is not visible. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    pos = first_word_in_line_a[line_nb_of_word_a[current]];

    while (word_a[current].end - word_a[pos].start >= len)
      pos++;

    win.first_column = word_a[pos].start;
  }

  /* Save the initial cursor line and column, here only the line is    */
  /* interesting us. This will tell us if we are in need to compensate */
  /* a terminal automatic scrolling.                                   */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  nl = disp_lines(&win,
                  &toggles,
                  current,
                  count,
                  search_mode,
                  &search_data,
                  &term,
                  last_line,
                  tmp_word,
                  &langinfo);

  /* Assert the presence of an early display of the horizontal bar. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.has_hbar)
    win.hbar_displayed = 1;

  /* Determine the number of lines to move the cursor up if the window */
  /* display needed a terminal scrolling.                              */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (nl + term.curs_line - 1 > term.nlines)
    line_offset = term.curs_line + nl - term.nlines;
  else
    line_offset = 0;

  /* Set the cursor to the first line of the window. */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  {
    long i; /* generic index in this block. */

    for (i = 1; i < line_offset; i++)
      (void)tputs(TPARM1(cursor_up), 1, outch);
  }

  /* Enable the reporting of the mouse events. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (!toggles.no_mouse)
  {
    if (term.has_kmous && strncmp(tigetstr("kmous"), "\x1b[<", 3) == 0)
      mouse_trk_on = "\x1b[?1005l\x1b[?1000;1006h\x1b[?2004h";
    else
      mouse_trk_on = "\x1b[?1005l\x1b[?1000;1015;1006h\x1b[?2004h";

    mouse_trk_off = "\x1b[?1000;1015;1006l\x1b[?2004l";

    printf("%s", mouse_trk_on);
  }

  /* Save again the cursor current line and column positions so that we */
  /* will be able to put the terminal cursor back here.                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  /* Arm the periodic timer. */
  /* """"""""""""""""""""""" */
  periodic_itv.it_value.tv_sec     = 0;
  periodic_itv.it_value.tv_usec    = TCK;
  periodic_itv.it_interval.tv_sec  = 0;
  periodic_itv.it_interval.tv_usec = TCK;
  setitimer(ITIMER_REAL, &periodic_itv, NULL);

  /* Signal management. */
  /* """""""""""""""""" */
  void sig_handler(int s);

  sa.sa_handler = sig_handler;
  sa.sa_flags   = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGALRM, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGPIPE, &sa, NULL);

  /* Main loop. */
  /* """""""""" */
  while (1)
  {
    int sc = 0; /* scancode */

    /* Manage the case of a broken pipe by exiting failure and restoring */
    /* the terminal and the cursor.                                      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_sigpipe)
    {
      (void)tputs(TPARM1(carriage_return), 1, outch);
      (void)tputs(TPARM1(cursor_normal), 1, outch);
      restore_term(fileno(stdin), &old_in_attrs);

      exit(128 + SIGPIPE);
    }

    /* Manage a segmentation fault by exiting with failure and restoring */
    /* the terminal and the cursor.                                      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_sigsegv)
    {
      fputs_safe("SIGSEGV received!\n", stderr);
      (void)tputs(TPARM1(carriage_return), 1, outch);
      (void)tputs(TPARM1(cursor_normal), 1, outch);
      restore_term(fileno(stdin), &old_in_attrs);

      exit(128 + SIGSEGV);
    }

    /* Manage the hangup and termination signal by exiting with failure */
    /* and restoring the terminal and the cursor.                       */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_sigterm || got_sighup)
    {
      fputs_safe("Interrupted!\n", stderr);
      (void)tputs(TPARM1(carriage_return), 1, outch);
      (void)tputs(TPARM1(cursor_normal), 1, outch);
      restore_term(fileno(stdin), &old_in_attrs);

      if (got_sigterm)
        exit(128 + SIGTERM);
      else
        exit(128 + SIGHUP);
    }

    /* If this alarm is triggered, then gracefully exit. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_forgotten_alrm)
    {
      (void)tputs(TPARM1(carriage_return), 1, outch);
      (void)tputs(TPARM1(cursor_normal), 1, outch);
      restore_term(fileno(stdin), &old_in_attrs);

      exit(EXIT_SUCCESS);
    }

    /* If this alarm is triggered, then redisplay the window */
    /* to remove the help message and disable this timer.    */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_help_alrm)
    {
      got_help_alrm = 0;

      /* Calculate the new metadata and draw the window again. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(&term, count, &win);

      help_mode = 0;
      nl        = disp_lines(&win,
                      &toggles,
                      current,
                      count,
                      search_mode,
                      &search_data,
                      &term,
                      last_line,
                      tmp_word,
                      &langinfo);
    }

    /* Reset the direct access selector if the direct access alarm rang. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_daccess_alrm)
    {
      got_daccess_alrm = 0;
      memset(daccess_stack, '\0', 6);
      daccess_stack_head = 0;

      /* +---+---+---+---+---+---+               */
      /* |   |   |   |   |   |   | daccess_stack */
      /* +-^-+---+---+---+---+---+               */
      /*   |                                     */
      /*   daccess_stack_head                    */
      /* """"""""""""""""""""""""""""""""""""""" */

      daccess_timer = timers.direct_access;
    }

    if (got_search_alrm)
    {
      got_search_alrm = 0;

      /* Calculate the new metadata and draw the window again. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(&term, count, &win);

      search_mode = NONE;

      nl = disp_lines(&win,
                      &toggles,
                      current,
                      count,
                      search_mode,
                      &search_data,
                      &term,
                      last_line,
                      tmp_word,
                      &langinfo);
    }

    if (got_winch)
    {
      got_winch      = 0;
      got_winch_alrm = 0;
      winch_timer    = timers.winch; /* Rearm the refresh timer. */
    }

    /* If the timeout is set then decrement its remaining value   */
    /* Upon expiration of this alarm, we trigger a content update */
    /* of the window.                                             */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_winch_alrm)
    {
      long i; /* generic index in this block */
      int  nlines, ncolumns;
      int  line, column;
      int  original_message_lines;

      got_winch_alrm = 0;  /* Reset the flag signaling the need for a *
                            | a refresh.                              */
      winch_timer    = -1; /* Disarm the timer used for this refresh. */

      if (message_lines_list != NULL && message_lines_list->len > 0)
        original_message_lines = message_lines_list->len + 1;
      else
        original_message_lines = 0;

      get_terminal_size(&nlines, &ncolumns, &term);

      /* Update term with the new number of lines and columns */
      /* of the real terminal.                                */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      term.nlines   = nlines;
      term.ncolumns = ncolumns;

      /* Reset the number of lines if the terminal has enough lines. */
      /* message_lines_list->len+1 is used here instead of           */
      /* win.message_lines because win.message_lines may have been   */
      /* altered by a previous scrolling and not yet recalculated.   */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (term.nlines <= original_message_lines)
      {
        win.message_lines = term.nlines - 1;
        win.max_lines     = 1;
      }
      else
      {
        win.message_lines = original_message_lines;

        if (win.max_lines < term.nlines - win.message_lines)
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
      }

      /* Erase the visible part of the displayed window. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      for (i = 0; i < win.message_lines; i++)
      {
        (void)tputs(TPARM1(clr_bol), 1, outch);
        (void)tputs(TPARM1(clr_eol), 1, outch);
        (void)tputs(TPARM1(cursor_up), 1, outch);
      }

      (void)tputs(TPARM1(clr_bol), 1, outch);
      (void)tputs(TPARM1(clr_eol), 1, outch);
      (void)tputs(TPARM1(save_cursor), 1, outch);

      get_cursor_position(&line, &column);

      for (i = 1; i < nl + win.message_lines; i++)
      {
        if (line + i >= nlines)
          break; /* We have reached the last terminal line. */

        (void)tputs(TPARM1(cursor_down), 1, outch);
        (void)tputs(TPARM1(clr_bol), 1, outch);
        (void)tputs(TPARM1(clr_eol), 1, outch);
      }
      (void)tputs(TPARM1(restore_cursor), 1, outch);

      /* Get new cursor position. */
      /* """""""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Calculate the new metadata and draw the window again. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(&term, count, &win);

      if (win.col_mode || win.line_mode)
      {
        long pos;
        long len;

        len = term.ncolumns - 3;

        /* Adjust win.first_column if the cursor is no more visible. */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        pos = first_word_in_line_a[line_nb_of_word_a[current]];

        while (word_a[current].end - word_a[pos].start >= len)
          pos++;

        win.first_column = word_a[pos].start;
      }

      /* Keep a line available for an eventual horizontal scroll bar. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (win.col_mode || win.line_mode)
      {
        if (win.asked_max_lines == 0
            || win.max_lines > term.nlines - win.message_lines)
        {
          win.max_lines = term.nlines - win.message_lines - 1;
          {
            (void)tputs(TPARM3(cursor_address, win.max_lines, 0), 1, outch);
            (void)tputs(TPARM1(clr_eol), 1, outch);
            (void)tputs(TPARM3(cursor_address, 0, 0), 1, outch);
          }
        }
        else if (win.asked_max_lines > term.nlines - win.message_lines)
          win.max_lines = term.nlines - win.message_lines - 1;
        else
          win.max_lines = win.asked_max_lines;
      }

      disp_message(message_lines_list,
                   message_max_width,
                   message_max_len,
                   &term,
                   &win,
                   &langinfo);

      nl = disp_lines(&win,
                      &toggles,
                      current,
                      count,
                      search_mode,
                      &search_data,
                      &term,
                      last_line,
                      tmp_word,
                      &langinfo);

      /* Determine the number of lines to move the cursor up if the window  */
      /* display needed a terminal scrolling.                               */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (nl + win.message_lines + term.curs_line > term.nlines)
        line_offset = term.curs_line + nl + win.message_lines - term.nlines;
      else
        line_offset = 0;

      /* Set the cursor to the first line of the window. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      for (i = 1; i < line_offset; i++)
        (void)tputs(TPARM1(cursor_up), 1, outch);

      /* Get new cursor position. */
      /* """""""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* We need to trigger the rebuild of the help lines because of */
      /* the geometry change.                                        */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      init_help_needed   = 1;
      fst_disp_help_line = 0;

      help_mode = 0;

      /* Short-circuit the loop. */
      /* """"""""""""""""""""""" */
      continue;
    }

    /* and possibly set its reached value.                      */
    /* The counter is frozen in search and help mode.           */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (timeout.initial_value && search_mode == NONE && !help_mode
        && got_timeout_tick)
    {
      long  i;
      char *timeout_string;

      got_timeout_tick = 0;

      timeout.remain--;

      if (!quiet_timeout && timeout.remain % FREQ == 0)
      {
        snprintf(timeout_seconds, 6, "%5u", timeout.remain / FREQ);
        timeout_string =
          (char *)(((ll_node_t *)(message_lines_list->tail))->data);
        memcpy(timeout_string + 1, timeout_seconds, 5);

        /* Erase the current window. */
        /* """"""""""""""""""""""""" */
        for (i = 0; i < win.message_lines; i++)
        {
          (void)tputs(TPARM1(cursor_up), 1, outch);
          (void)tputs(TPARM1(clr_bol), 1, outch);
          (void)tputs(TPARM1(clr_eol), 1, outch);
        }

        (void)tputs(TPARM1(clr_bol), 1, outch);
        (void)tputs(TPARM1(clr_eol), 1, outch);

        /* Display the words window and its title for the first time. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        disp_message(message_lines_list,
                     message_max_width,
                     message_max_len,
                     &term,
                     &win,
                     &langinfo);
      }
      /* The timeout has expired. */
      /* """""""""""""""""""""""" */
      if (timeout.remain == 0)
        timeout.reached = 1;
    }

    if (timeout.reached)
    {
      if (timeout.mode == QUIT)
        goto quit;
      else if (timeout.mode == CURRENT || timeout.mode == WORD)
        goto enter;
    }

    /* Pressed keys scancodes processing. */
    /* """""""""""""""""""""""""""""""""" */
    page = 1; /* Default number of lines to do down/up *
               | with PgDn/PgUp.                       */

    sc = get_scancode(buffer, 64);

    if (sc && winch_timer < 0) /* Do not allow input when a window *
                                | refresh is scheduled.            */
    {
      /* Rearm the timer named "forgotten". */
      /* """""""""""""""""""""""""""""""""" */
      forgotten_timer = timers.forgotten;

      if (timeout.initial_value && buffer[0] != 0x0d && buffer[0] != 'q'
          && buffer[0] != 'Q' && buffer[0] != 3)
      {
        char *timeout_string;

        /* Reset the timeout to its initial value. */
        /* """"""""""""""""""""""""""""""""""""""" */
        timeout.remain = timeout.initial_value;

        if (!quiet_timeout)
        {
          snprintf(timeout_seconds, 6, "%5u", timeout.initial_value / FREQ);
          timeout_string =
            (char *)(((ll_node_t *)(message_lines_list->tail))->data);
          memcpy(timeout_string + 1, timeout_seconds, 5);

          /* Clear the message. */
          /* """""""""""""""""" */
          for (long i = 0; i < win.message_lines; i++)
          {
            (void)tputs(TPARM1(cursor_up), 1, outch);
            (void)tputs(TPARM1(clr_bol), 1, outch);
            (void)tputs(TPARM1(clr_eol), 1, outch);
          }

          (void)tputs(TPARM1(clr_bol), 1, outch);
          (void)tputs(TPARM1(clr_eol), 1, outch);

          /* Display the words window and its title for the first time. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          disp_message(message_lines_list,
                       message_max_width,
                       message_max_len,
                       &term,
                       &win,
                       &langinfo);
        }

        setitimer(ITIMER_REAL, &periodic_itv, NULL);
      }

      long mb_index; /* index of the last selected glyph in the *
                      | bitmap used to march selected glyphs.   */

      switch (buffer[0])
      {
        case 0x01: /* ^A */
          if (!help_mode && search_mode != NONE)
            goto khome;

          break;

        case 0x1a: /* ^Z */
          if (!help_mode && search_mode != NONE)
            goto kend;

          break;

        /* Check if TAB was hit in search mode (attempt auto-completion). */
        /* Only available when searching.                                 */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        case 0x09: /* TAB */
        {
          long matching_nb; /* number of currently matching words. */

          long len_mb = word_a[current].len_mb; /* number of glyphs in the *
                                                   | current word.           */

          long shortest_mb_seq; /* shortest length (in mb) after the last *
                                   | bit set in matching words.             */

          long ref_word; /* word index with the shorted sequence of mb *
                            | to check for.                              */

          long l; /* number of bytes of the glyph following the last *
                     | searched glyph.n the current searched word.     */

          matching_nb = BUF_LEN(matching_words_da);

          /* Do nothing if there is no matching words. */
          /* """"""""""""""""""""""""""""""""""""""""" */
          if (matching_nb > 0)
          {
            mb_index        = len_mb - 1;
            shortest_mb_seq = len_mb - 1;

            /* Get the position of the latest selected glyph. */
            /* """""""""""""""""""""""""""""""""""""""""""""" */
            while (mb_index && !BIT_ISSET(word_a[current].bitmap, mb_index))
              mb_index--;

            /* If there is only one matching word select all its */
            /* remaining glyphs.                                 */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            if (matching_nb == 1)
            {
              char *ptr = word_a[current].str;

              /* Points to the first glyph to check. */
              /* """"""""""""""""""""""""""""""""""" */
              for (long i = 0; i <= mb_index; i++)
                ptr = utf8_next(ptr);

              /* Mark the remaining glyphs as selected and update the */
              /* search data.                                         */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
              for (long i = mb_index + 1; i < len_mb; i++)
              {
                BIT_ON(word_a[current].bitmap, i);

                l = utf8_get_length(*ptr);
                memcpy(search_data.buf + search_data.len, ptr, l);
                search_data.len_a[search_data.mb_len] = l;
                search_data.off_a[search_data.mb_len] = search_data.len;
                search_data.len += l;
                search_data.mb_len++;
                search_data.buf[search_data.len] = '\0';

                /* In fuzzy mode, each glyph in search_data.buf is added to */
                /* to the end of tst_search_list, so we need to do the same */
                /* thing here.                                              */
                /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
                if (search_mode == FUZZY)
                  append_tst_search_list(ptr, l);

                ptr += l;
              }

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
            else
            {
              long  len_mb;
              long  mb_index;
              long *work_a;
              long *work_mb_a;
              char *ptr;

              /* Array to contain the offset (in bytes) of the last  */
              /* significant bit in the glyph selection bitmap.      */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
              work_a = xmalloc(matching_nb * sizeof(long));

              /* Array to contain the offset (in mb) of the last  */
              /* significant bit in the glyph selection bitmap.   */
              /* """""""""""""""""""""""""""""""""""""""""""""""" */
              work_mb_a = xmalloc(matching_nb * sizeof(long));

              /* Word index in matching_words_da which be become the      */
              /* reference to compare with the other ones.                */
              /* This will be the one the the shortest sequence of 0 bits */
              /* in the selection bitmap of the word.  This will minimize */
              /* the internal loop number in the following code.          */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              ref_word = 0;

              for (long i = 0; i < matching_nb; i++)
              {
                len_mb = word_a[matching_words_da[i]].len_mb;

                mb_index = len_mb - 1;

                /* Determine the index of the first non selected glyph */
                /* after the last selected one.                        */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                while (
                  mb_index >= 0
                  && !BIT_ISSET(word_a[matching_words_da[i]].bitmap, mb_index))
                  mb_index--;
                mb_index++;

                /* ptr will point to the first non selected glyph */
                /* after the last one.                            */
                /* """""""""""""""""""""""""""""""""""""""""""""" */
                ptr = word_a[matching_words_da[i]].str + daccess.flength;

                for (long j = 0; j < mb_index; j++)
                  ptr = utf8_next(ptr);

                /* Store the offsets in mb and bytes on the first */
                /* glyph to consider.                             */
                /* """""""""""""""""""""""""""""""""""""""""""""" */
                if (ptr != NULL)
                {
                  work_a[i]    = ptr - word_a[matching_words_da[i]].str;
                  work_mb_a[i] = mb_index;
                }
                else
                  work_a[i] = 0;

                /* Update ref_word if needed. */
                /* """""""""""""""""""""""""" */
                if (shortest_mb_seq > len_mb - mb_index)
                {
                  shortest_mb_seq = len_mb - mb_index;
                  ref_word        = i;
                }
              }

              /* Check if we have something to do. */
              /* """"""""""""""""""""""""""""""""" */
              if (shortest_mb_seq > 0)
              {
                ptr = word_a[matching_words_da[ref_word]].str
                      + work_a[ref_word];

                while (ptr - word_a[matching_words_da[ref_word]].str
                       < word_a[matching_words_da[ref_word]].len)
                {
                  char glyph[5];
                  int  l    = utf8_get_length(*(ptr));
                  int  stop = 0;

                  memcpy(glyph, ptr, l);
                  glyph[l] = '\0';

                  ptr += l;

                  /* Compare the right glyph of ref_word to the right */
                  /* glyph in the other word in matching_words_da.    */
                  /* set the stop variable if a difference was seen.  */
                  /* """""""""""""""""""""""""""""""""""""""""""""""" */
                  for (long i = 0; i < matching_nb; i++)
                  {
                    if (i != ref_word)
                    {
                      if (memcmp(glyph,
                                 word_a[matching_words_da[i]].str + work_a[i],
                                 l)
                          != 0)
                      {
                        stop = 1;
                        /* Remove the previously added values in work_a and */
                        /* work_mb_a when un mismatch occurs.               */
                        /* And exit the loop.                               */
                        /* """""""""""""""""""""""""""""""""""""""""""""""" */
                        for (long j = 0; j <= i; j++)
                        {
                          work_a[j] -= l;
                          work_mb_a[j]--;
                        }
                        break;
                      }
                      else
                      {
                        work_a[i] += l;
                        work_mb_a[i]++;
                      }
                    }
                    else /* Skip the ref_word. */
                    {
                      work_a[i] += l;
                      work_mb_a[i]++;
                    }
                  }

                  if (stop)
                    break;
                  else
                  {
                    /* Update the search data. */
                    /* """"""""""""""""""""""" */
                    memcpy(search_data.buf + search_data.len, glyph, l);
                    search_data.len_a[search_data.mb_len] = l;
                    search_data.off_a[search_data.mb_len] = search_data.len;
                    search_data.len += l;
                    search_data.mb_len++;
                    search_data.buf[search_data.len] = '\0';

                    /* Mark the remaining glyphs as selected. */
                    /* """""""""""""""""""""""""""""""""""""" */
                    for (long i = 0; i < matching_nb; i++)
                      BIT_ON(word_a[matching_words_da[i]].bitmap,
                             work_mb_a[i] - 1);

                    /* In fuzzy mode, each glyph in search_data.buf is */
                    /* added to the end of tst_search_list, so we need */
                    /* to do the same thing here.                      */
                    /* """"""""""""""""""""""""""""""""""""""""""""""" */
                    if (search_mode == FUZZY)
                      append_tst_search_list(ptr, l);
                  }
                }
              }

              free(work_a);
              free(work_mb_a);

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
        }

        break;

        case 0x1b: /* ESC */

          /* Ignore mouse pastes when bracketed pastes is enabled. */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp("\x1b[200~", buffer, 6) == 0)
          {
            int  c;
            char eb[6] = { 0 };

            /* Consume stdin until a closing bracket is found. */
            /* ''''''''''''''''''''''''''''''''''''''''''''''' */
            while (1)
            {
              /* Fast reading until an ESC or the end of input is found. */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              while ((c = my_fgetc(stdin)) != EOF && c != 0x1b)
                ; /* Null action. */

              /* Exits the loop early if the first ESC character starting */
              /* the ending bracket has already be consumed while reading */
              /* the content of buffer.                                   */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (c == EOF)
                break;

              /* Read the 5 next characters to look for the */
              /* ending bracket "[201~".                    */
              /* """""""""""""""""""""""""""""""""""""""""" */
              scanf("%5c", eb);
              if (memcmp("[201~", eb, 5) == 0)
                break;
            }
          }

          /* An escape sequence or a UTF-8 sequence has been pressed. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (memcmp("\x1bOH", buffer, 3) == 0
              || memcmp("\x1bk", buffer, 2) == 0
              || memcmp("\x1b[H", buffer, 3) == 0
              || memcmp("\x1b[1~", buffer, 4) == 0
              || memcmp("\x1b[7~", buffer, 4) == 0)
          {
            /* HOME key has been pressed. */
            /* """""""""""""""""""""""""" */
            if (help_mode)
              break;

            if (search_mode != NONE)
            {
            khome:
              search_data.only_starting = 1;
              search_data.only_ending   = 0;
              select_starting_matches(&win, &term, &search_data, &last_line);
            }
            else
            {
              /* Find the first selectable word. */
              /* """"""""""""""""""""""""""""""" */
              current = win.start;

              while (current < win.end && !word_a[current].is_selectable)
                current++;

              /* In column mode we need to take care of the */
              /* horizontal scrolling.                      */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if ((win.col_mode || win.line_mode)
                  && word_a[current].end < win.first_column)
                win.first_column = word_a[current].start;
            }

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
            break;
          }

          if (memcmp("\x1b[1;5H", buffer, 6) == 0
              || memcmp("\x1b[7^", buffer, 4) == 0)
            /* CTRL HOME key has been pressed. */
            /* """"""""""""""""""""""""""""""" */
            goto kchome;

          if (memcmp("\x1bOF", buffer, 3) == 0
              || memcmp("\x1bj", buffer, 2) == 0
              || memcmp("\x1b[F", buffer, 3) == 0
              || memcmp("\x1b[4~", buffer, 4) == 0
              || memcmp("\x1b[8~", buffer, 4) == 0)
          {
            /* END key has been pressed. */
            /* """"""""""""""""""""""""" */
            if (help_mode)
              break;

            if (search_mode != NONE)
            {
            kend:

              if (BUF_LEN(matching_words_da) > 0 && search_mode != PREFIX)
              {
                search_data.only_starting = 0;
                search_data.only_ending   = 1;
                select_ending_matches(&win, &term, &search_data, &last_line);
              }
            }
            else
            {
              /* Find the last selectable word. */
              /* """""""""""""""""""""""""""""" */
              current = win.end;

              while (current > win.start && !word_a[current].is_selectable)
                current--;

              /* In column mode we need to take care of the */
              /* horizontal scrolling.                      */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode || win.line_mode)
                set_new_first_column(&win, &term);
            }

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
            break;
          }

          if (memcmp("\x1b[1;5F", buffer, 6) == 0
              || memcmp("\x1b[8^", buffer, 4) == 0)
            /* CTRL END key has been pressed. */
            /* """""""""""""""""""""""""""""" */
            goto kcend;

          if (memcmp("\x1bOD", buffer, 3) == 0
              || memcmp("\x1b[D", buffer, 3) == 0)
            /* Left arrow key has been pressed. */
            /* """""""""""""""""""""""""""""""" */
            goto kl;

          if (memcmp("\x1bOC", buffer, 3) == 0
              || memcmp("\x1b[C", buffer, 3) == 0)
            /* Right arrow key has been pressed. */
            /* """"""""""""""""""""""""""""""""" */
            goto kr;

          if (memcmp("\x1bOA", buffer, 3) == 0
              || memcmp("\x1b[A", buffer, 3) == 0)
            /* Up arrow key has been pressed. */
            /* """""""""""""""""""""""""""""" */
            goto ku;

          if (memcmp("\x1bOB", buffer, 3) == 0
              || memcmp("\x1b[B", buffer, 3) == 0)
            /* Down arrow key has been pressed. */
            /* """""""""""""""""""""""""""""""" */
            goto kd;

          if (memcmp("\x1b[I", buffer, 3) == 0
              || memcmp("\x1b[5~", buffer, 4) == 0)
            /* PgUp key has been pressed. */
            /* """""""""""""""""""""""""" */
            goto kpp;

          if (memcmp("\x1b[G", buffer, 3) == 0
              || memcmp("\x1b[6~", buffer, 4) == 0)
            /* PgDn key has been pressed. */
            /* """""""""""""""""""""""""" */
            goto knp;

          if (memcmp("\x1b[L", buffer, 3) == 0
              || memcmp("\x1b[2~", buffer, 4) == 0)
            /* Ins key has been pressed. */
            /* """"""""""""""""""""""""" */
            goto kins;

          if (memcmp("\x1b[3~", buffer, 4) == 0)
            /* Del key has been pressed. */
            /* """"""""""""""""""""""""" */
            goto kdel;

          if (memcmp("\x1b[1;2F", buffer, 6) == 0
              || memcmp("\x1b[1;5C", buffer, 6) == 0
              || memcmp("\x1b[8$", buffer, 4) == 0
              || memcmp("\x1bOc", buffer, 3) == 0)
            /* SHIFT END or CTRL -> has been pressed. */
            /* """""""""""""""""""""""""""""""""""""" */
            goto keol;

          if (memcmp("\x1b[1;2H", buffer, 6) == 0
              || memcmp("\x1b[1;5D", buffer, 6) == 0
              || memcmp("\x1b[7$", buffer, 4) == 0
              || memcmp("\x1bOd", buffer, 3) == 0)
            /* SHIFT HOME or CTRL <- key has been pressed. */
            /* """"""""""""""""""""""""""""""""""""""""""" */
            goto ksol;

          if (!toggles.no_mouse)
          {
            double          res = 5000; /* 5 s, arbitrary value. */
            struct timespec res_ts;

            /* The minimal resolution for double-click must be 1/10 s. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (clock_getres(CLOCK_MONOTONIC, &res_ts) == -1)
              disable_double_click = 1;
            else
              res = 1000.0 * res_ts.tv_sec + 1e-6 * res_ts.tv_nsec;

            if (res > 100)
              disable_double_click = 1;

            /* Detect the mouse protocol. */
            /* """""""""""""""""""""""""" */
            if (memcmp("\x1b[<", buffer, 3) == 0)
            {
              mouse_proto = MOUSE1006;
              goto kmouse;
            }

            if (memcmp("\x1b[M", buffer, 3) == 0)
            {
              mouse_proto = MOUSE1000;
              goto kmouse;
            }

            if (memcmp("\x1b[32;", buffer, 5) == 0
                || memcmp("\x1b[33;", buffer, 5) == 0
                || memcmp("\x1b[34;", buffer, 5) == 0
                || memcmp("\x1b[96;", buffer, 5) == 0
                || memcmp("\x1b[97;", buffer, 5) == 0)
            {
              mouse_proto = MOUSE1015;
              goto kmouse;
            }
          }

          if (buffer[0] == 0x1b && buffer[1] == '\0')
          {
            /* ESC key has been pressed. */
            /* """"""""""""""""""""""""" */

            /* Do not reset the search buffer when exiting the quick help */
            /* using ESC.                                                 */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
            if (help_mode)
              help_mode = 0;
            else
              reset_search_buffer(&win,
                                  &search_data,
                                  &timers,
                                  &toggles,
                                  &term,
                                  &daccess,
                                  &langinfo,
                                  last_line,
                                  tmp_word,
                                  word_real_max_size);

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);

            /* Unmark the marked word. */
            /* """"""""""""""""""""""" */
            if (toggles.taggable && marked >= 0)
              goto unmark_word;

            break;
          }

          /* Else ignore key. */
          break;

        quit:
        case 'q':
        case 'Q':
        case 3: /* ^C */
                /* q or Q of ^C has been pressed. */
                /* """""""""""""""""""""""""""""" */

          if (!help_mode && search_mode != NONE && buffer[0] != 3)
            goto special_cmds_when_searching;

          {
            long i; /* Generic index in this block. */

            for (i = 0; i < win.message_lines; i++)
              (void)tputs(TPARM1(cursor_up), 1, outch);

            if (toggles.del_line)
            {
              (void)tputs(TPARM1(clr_eol), 1, outch);
              (void)tputs(TPARM1(clr_bol), 1, outch);
              (void)tputs(TPARM1(save_cursor), 1, outch);

              for (i = 1; i < nl + win.message_lines; i++)
              {
                (void)tputs(TPARM1(cursor_down), 1, outch);
                (void)tputs(TPARM1(clr_eol), 1, outch);
                (void)tputs(TPARM1(clr_bol), 1, outch);
              }
              (void)tputs(TPARM1(restore_cursor), 1, outch);
            }
            else
            {
              for (i = 1; i < nl + win.message_lines; i++)
                (void)tputs(TPARM1(cursor_down), 1, outch);
              puts("");
            }
          }

          /* Disable the reporting of the mouse events. */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (!toggles.no_mouse)
            printf("%s", mouse_trk_off);

          /* Restore the visibility of the cursor. */
          /* """"""""""""""""""""""""""""""""""""" */
          (void)tputs(TPARM1(cursor_normal), 1, outch);

          if (buffer[0] == 3) /* ^C */
          {
            if (int_string != NULL)
              fprintf(old_stdout, "%s", int_string);

            /* Set the cursor at the start on the line an restore the */
            /* original terminal state before exiting.                */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
            (void)tputs(TPARM1(carriage_return), 1, outch);
            restore_term(fileno(stdin), &old_in_attrs);

            if (int_as_in_shell)
              exit(128 + SIGINT);
          }
          else
            restore_term(fileno(stdin), &old_in_attrs);

          exit(EXIT_SUCCESS);

        case 0x0c:
          /* Form feed (^L) is a traditional method to redraw a screen. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          nl = disp_lines(&win,
                          &toggles,
                          current,
                          count,
                          search_mode,
                          &search_data,
                          &term,
                          last_line,
                          tmp_word,
                          &langinfo);

          if (help_mode)
            disp_help(&win, &term, help_lines_da, fst_disp_help_line);
          break;

        case 'n':
        case ' ':
          /* n or the space bar has been pressed. */
          /* """""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (BUF_LEN(matching_words_da) > 0)
          {
            long pos = find_next_matching_word(matching_words_da,
                                               BUF_LEN(matching_words_da),
                                               current,
                                               &matching_word_cur_index);
            if (pos >= 0)
              current = pos;

            if (current < win.start || current > win.end)
              last_line = build_metadata(&term, count, &win);

            /* Set new first column to display. */
            /* """""""""""""""""""""""""""""""" */
            set_new_first_column(&win, &term);

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          break;

        case 'N':
          /* N has been pressed.*/
          /* """"""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (BUF_LEN(matching_words_da) > 0)
          {
            long pos = find_prev_matching_word(matching_words_da,
                                               BUF_LEN(matching_words_da),
                                               current,
                                               &matching_word_cur_index);
            if (pos >= 0)
              current = pos;
          }

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* Set new first column to display. */
          /* """""""""""""""""""""""""""""""" */
          set_new_first_column(&win, &term);

          nl = disp_lines(&win,
                          &toggles,
                          current,
                          count,
                          search_mode,
                          &search_data,
                          &term,
                          last_line,
                          tmp_word,
                          &langinfo);
          break;

        case 's':
          /* s has been pressed. */
          /* """"""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (BUF_LEN(matching_words_da) > 0)
          {
            long pos;

            if (BUF_LEN(best_matching_words_da) > 0)
              pos = find_next_matching_word(best_matching_words_da,
                                            BUF_LEN(best_matching_words_da),
                                            current,
                                            &matching_word_cur_index);
            else
              pos = find_next_matching_word(matching_words_da,
                                            BUF_LEN(matching_words_da),
                                            current,
                                            &matching_word_cur_index);

            if (pos >= 0)
              current = pos;

            if (current < win.start || current > win.end)
              last_line = build_metadata(&term, count, &win);

            /* Set new first column to display. */
            /* """""""""""""""""""""""""""""""" */
            set_new_first_column(&win, &term);

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          break;

        case 'S':
          /* S has been pressed. */
          /* """"""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

          if (BUF_LEN(matching_words_da) > 0)
          {
            long pos;

            if (BUF_LEN(best_matching_words_da) > 0)
              pos = find_prev_matching_word(best_matching_words_da,
                                            BUF_LEN(best_matching_words_da),
                                            current,
                                            &matching_word_cur_index);
            else
              pos = find_prev_matching_word(matching_words_da,
                                            BUF_LEN(matching_words_da),
                                            current,
                                            &matching_word_cur_index);

            if (pos >= 0)
              current = pos;
          }

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* Set new first column to display. */
          /* """""""""""""""""""""""""""""""" */
          set_new_first_column(&win, &term);

          nl = disp_lines(&win,
                          &toggles,
                          current,
                          count,
                          search_mode,
                          &search_data,
                          &term,
                          last_line,
                          tmp_word,
                          &langinfo);
          break;

        enter:
        case 0x0d: /* CR */
        {
          /* <Enter> has been pressed. */
          /* """"""""""""""""""""""""" */

          int       extra_lines;
          char     *output_str;
          output_t *output_node;

          int width = 0;

          wchar_t *w;
          long     i; /* Generic index in this block. */

          if (help_mode || marked >= 0)
          {
            marked = -1; /* Disable the marked mode unconditionally. */
            nl     = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }

          if (search_mode != NONE)
          {
            /* Cancel the search timer. */
            /* """""""""""""""""""""""" */
            search_timer = 0;

            search_mode               = NONE;
            search_data.only_starting = 0;
            search_data.only_ending   = 0;

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);

            if (!toggles.enter_val_in_search)
              break;
          }

          if (toggles.del_line)
          {
            for (i = 0; i < win.message_lines; i++)
              (void)tputs(TPARM1(cursor_up), 1, outch);

            (void)tputs(TPARM1(clr_eol), 1, outch);
            (void)tputs(TPARM1(clr_bol), 1, outch);
            (void)tputs(TPARM1(save_cursor), 1, outch);

            for (i = 1; i < nl + win.message_lines; i++)
            {
              (void)tputs(TPARM1(cursor_down), 1, outch);
              (void)tputs(TPARM1(clr_eol), 1, outch);
              (void)tputs(TPARM1(clr_bol), 1, outch);
            }

            (void)tputs(TPARM1(restore_cursor), 1, outch);
          }
          else
          {
            for (i = 1; i < nl; i++)
              (void)tputs(TPARM1(cursor_down), 1, outch);
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
            char *num_str;
            char *str;

            if (toggles.taggable)
            {
              ll_t      *output_list = ll_new();
              ll_node_t *node;

              /* When using -P, updates the tagging order of this word to */
              /* make sure that the output will be correctly sorted.      */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (word_a[current].tag_id == 0 && toggles.pinable)
                word_a[current].tag_order = tag_nb++;

              for (wi = 0; wi < count; wi++)
              {
                if (word_a[wi].tag_id > 0 || wi == current)
                {
                  /* If the -p option is not used we do not take into      */
                  /* account an untagged word under the cursor if at least */
                  /* on word is tagged.                                    */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
                  if (wi == current && tagged_words > 0 && !toggles.autotag
                      && word_a[wi].tag_id == 0)
                    continue;

                  /* In tagged mode, do not automatically tag the word   */
                  /* under the cursor if toggles.noautotag is set and no */
                  /* word are tagged.                                    */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                  if (tagged_words == 0 && toggles.taggable
                      && toggles.noautotag)
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

                    output_node->output_str = concat(num_str,
                                                     daccess.num_sep,
                                                     str + daccess.flength,
                                                     (char *)0);

                    free(num_str);
                  }
                  else
                    output_node->output_str = xstrdup(str + daccess.flength);

                  output_node->order = word_a[wi].tag_order;

                  /* Trim the spaces if -k is not given. */
                  /* """"""""""""""""""""""""""""""""""" */
                  if (!toggles.keep_spaces)
                  {
                    ltrim(output_node->output_str, " \t");
                    rtrim(output_node->output_str, " \t", 0);
                  }

                  ll_append(output_list, output_node);
                }
              }

              /* If nothing is selected, exist without printing anything. */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (output_list->head == NULL)
                goto exit;

              /* If -P is in use, then sort the output list. */
              /* """"""""""""""""""""""""""""""""""""""""""" */
              if (toggles.pinable)
                ll_sort(output_list, tag_comp, tag_swap);

              /* And print them. */
              /* """"""""""""""" */
              node = output_list->head;
              while (node->next != NULL)
              {
                str = ((output_t *)(node->data))->output_str;

                fprintf(old_stdout, "%s", str);
                width += my_wcswidth((w = utf8_strtowcs(str)), 65535);
                free(w);
                free(str);
                free(node->data);

                if (win.sel_sep != NULL)
                {
                  fprintf(old_stdout, "%s", win.sel_sep);
                  width += my_wcswidth((w = utf8_strtowcs(win.sel_sep)), 65535);
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
              width += my_wcswidth((w = utf8_strtowcs(str)), 65535);
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

                output_str = concat(num_str,
                                    daccess.num_sep,
                                    str + daccess.flength,
                                    (char *)0);

                free(num_str);
              }
              else
                output_str = str + daccess.flength;

              /* Trim the spaces if -k is not given. */
              /* """"""""""""""""""""""""""""""""""" */
              if (!toggles.keep_spaces)
              {
                ltrim(output_str, " \t");
                rtrim(output_str, " \t", 0);
              }

              width = my_wcswidth((w = utf8_strtowcs(output_str)), 65535);
              free(w);

              /* And print it. */
              /* """"""""""""" */
              fprintf(old_stdout, "%s", output_str);
            }

            /* If the output stream is a terminal. */
            /* """"""""""""""""""""""""""""""""""" */
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
              (void)tputs(TPARM1(delete_line), 1, outch);

              for (i = 0; i < extra_lines; i++)
              {
                (void)tputs(TPARM1(cursor_up), 1, outch);
                (void)tputs(TPARM1(clr_eol), 1, outch);
                (void)tputs(TPARM1(clr_bol), 1, outch);
              }
            }
          }

        exit:

          /* Disable mouse reporting. */
          /* '''''''''''''''''''''''' */
          if (!toggles.no_mouse)
            printf("%s", mouse_trk_off);

          /* Restore the visibility of the cursor. */
          /* """"""""""""""""""""""""""""""""""""" */
          (void)tputs(TPARM1(cursor_normal), 1, outch);

          /* Set the cursor at the start on the line an restore the */
          /* original terminal state before exiting.                */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          (void)tputs(TPARM1(carriage_return), 1, outch);
          restore_term(fileno(stdin), &old_in_attrs);

          exit(EXIT_SUCCESS);
        }

        ksol:
          /* Go to the start of the line. */
          if (help_mode)
            break;

          /* """""""""""""""""""""""""""" */
          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'H':
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (win.col_mode || win.line_mode)
            {
              long pos;

              pos = first_word_in_line_a[line_nb_of_word_a[current]];

              search_mode = NONE;

              /* Find the first selectable word. */
              /* """"""""""""""""""""""""""""""" */
              while (!word_a[pos].is_last)
              {
                if (word_a[pos].is_selectable)
                {
                  current = pos;
                  break;
                }

                pos++;

                if (pos > current)
                  break;
              }

              if (word_a[pos].is_last && word_a[pos].is_selectable)
                current = pos;

              set_new_first_column(&win, &term);

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;

          break;

        kl:
          /* Cursor Left key has been pressed. */
          /* """"""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'h':
          if (help_mode)
            break;

          if (search_mode == NONE)
            move_left(&win,
                      &term,
                      &toggles,
                      &search_data,
                      &langinfo,
                      &nl,
                      last_line,
                      tmp_word);
          else
            goto special_cmds_when_searching;

          break;

        /* shift the window to the left if possible. */
        /* """"""""""""""""""""""""""""""""""""""""" */
        case '<':
          if (help_mode)
            break;

          if (search_mode == NONE)
            shift_left(&win,
                       &term,
                       &toggles,
                       &search_data,
                       &langinfo,
                       &nl,
                       last_line,
                       tmp_word,
                       line_nb_of_word_a[current]);
          else
            goto special_cmds_when_searching;

          break;

        keol:
          /* Go to the end of the line. */
          /* """""""""""""""""""""""""" */
          if (help_mode)
            break;

          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'L':
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (win.col_mode || win.line_mode)
            {
              long pos;

              pos = current;

              search_mode = NONE;

              /* Find the first selectable word. */
              /* """"""""""""""""""""""""""""""" */
              while (!word_a[pos].is_last)
              {
                if (word_a[pos].is_selectable)
                  current = pos;

                pos++;
              }

              if (word_a[pos].is_selectable)
                current = pos;

              set_new_first_column(&win, &term);

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;

          break;

        kr:
          /* Right key has been pressed. */
          /* """"""""""""""""""""""""""" */
          if (help_mode)
            break;

          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'l':
          if (help_mode)
            break;

          if (search_mode == NONE)
            move_right(&win,
                       &term,
                       &toggles,
                       &search_data,
                       &langinfo,
                       &nl,
                       last_line,
                       tmp_word);
          else
            goto special_cmds_when_searching;

          break;

        /* shift the window to the left if possible. */
        /* """"""""""""""""""""""""""""""""""""""""" */
        case '>':
          if (help_mode)
            break;

          if (search_mode == NONE)
            shift_right(&win,
                        &term,
                        &toggles,
                        &search_data,
                        &langinfo,
                        &nl,
                        last_line,
                        tmp_word,
                        line_nb_of_word_a[current]);
          else
            goto special_cmds_when_searching;

          break;

        case 0x0b:
          /* ^K key has been pressed. */
          /* """""""""""""""""""""""" */
          if (help_mode)
            break;

          goto kchome;

        case 'K':
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

        kpp:
          /* PgUp key has been pressed. */
          /* """""""""""""""""""""""""" */
          if (help_mode)
            break;

          page = win.max_lines;

        ku:
          /* Cursor Up key has been pressed. */
          /* """"""""""""""""""""""""""""""" */
          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'k':
          if (search_mode == NONE)
          {
            if (help_mode)
            {
              int help_length = BUF_LEN(help_lines_da);

              if (help_length > win.max_lines && fst_disp_help_line > 0)
              {
                fst_disp_help_line--;
                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
                disp_help(&win, &term, help_lines_da, fst_disp_help_line);
              }
            }
            else
              move_up(&win,
                      &term,
                      &toggles,
                      &search_data,
                      &langinfo,
                      &nl,
                      page,
                      first_selectable,
                      last_line,
                      tmp_word);
          }
          else
            goto special_cmds_when_searching;

          break;

        kchome:
          /* Go to the first selectable word. */
          /* """""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          current = 0;

          search_mode = NONE;

          /* Find the first selectable word. */
          /* """"""""""""""""""""""""""""""" */
          while (!word_a[current].is_selectable)
            current++;

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* In column mode we need to take care of the */
          /* horizontal scrolling.                      */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (win.col_mode || win.line_mode)
          {
            long pos;

            /* Adjust win.first_column if the cursor is */
            /* no more visible.                         */
            /* """""""""""""""""""""""""""""""""""""""" */
            pos = first_word_in_line_a[line_nb_of_word_a[current]];

            while (word_a[current].end - word_a[pos].start >= term.ncolumns - 3)
              pos++;

            win.first_column = word_a[pos].start;
          }

          nl = disp_lines(&win,
                          &toggles,
                          current,
                          count,
                          search_mode,
                          &search_data,
                          &term,
                          last_line,
                          tmp_word,
                          &langinfo);
          break;

        case 0x0a:
          /* ^J key has been pressed. */
          /* """""""""""""""""""""""" */
          if (help_mode)
            break;

          goto kcend;

        case 'J':
          if (help_mode)
            break;

          if (search_mode != NONE)
            goto special_cmds_when_searching;

        knp:
          /* PgDn key has been pressed. */
          /* """""""""""""""""""""""""" */
          if (help_mode)
            break;

          page = win.max_lines;

        kd:
          /* Cursor Down key has been pressed. */
          /* """"""""""""""""""""""""""""""""" */
          search_mode = NONE;

          /* Fall through. */
          /* """"""""""""" */

        case 'j':
          if (search_mode == NONE)
          {
            if (help_mode)
            {
              int help_length = BUF_LEN(help_lines_da);

              if (help_length > win.max_lines
                  && fst_disp_help_line < help_length)
              {
                fst_disp_help_line++;
                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
                disp_help(&win, &term, help_lines_da, fst_disp_help_line);
              }
            }
            else
              move_down(&win,
                        &term,
                        &toggles,
                        &search_data,
                        &langinfo,
                        &nl,
                        page,
                        last_selectable,
                        last_line,
                        tmp_word);
          }
          else
            goto special_cmds_when_searching;

          break;

        kcend:
          /* Go to the last selectable word. */
          /* """"""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          current = count - 1;

          search_mode = NONE;

          /* Find the first selectable word. */
          /* """"""""""""""""""""""""""""""" */
          while (!word_a[current].is_selectable)
            current--;

          if (current < win.start || current > win.end)
            last_line = build_metadata(&term, count, &win);

          /* In column mode we need to take care of the */
          /* horizontal scrolling.                      */
          /* """""""""""""""""""""""""""""""""""""""""" */
          if (win.col_mode || win.line_mode)
          {
            long pos;

            /* Adjust win.first_column if the cursor is no more visible. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            pos = first_word_in_line_a[line_nb_of_word_a[current]];

            while (word_a[current].end - word_a[pos].start >= term.ncolumns - 3)
              pos++;

            win.first_column = word_a[pos].start;
          }

          nl = disp_lines(&win,
                          &toggles,
                          current,
                          count,
                          search_mode,
                          &search_data,
                          &term,
                          last_line,
                          tmp_word,
                          &langinfo);
          break;

        case '/':
          /* Generic search method according the misc settings. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (misc.default_search_method == PREFIX)
            goto prefix_method;
          else if (misc.default_search_method == SUBSTRING)
            goto substring_method;
          else if (misc.default_search_method == FUZZY)
            goto fuzzy_method;

          break;

        case '~':
        case '*':
          /* ~ or * key has been pressed        */
          /* (start of a fuzzy search session). */
          /* """""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

        fuzzy_method:

          if (search_mode == NONE)
          {
            if (!toggles.incremental_search)
              reset_search_buffer(&win,
                                  &search_data,
                                  &timers,
                                  &toggles,
                                  &term,
                                  &daccess,
                                  &langinfo,
                                  last_line,
                                  tmp_word,
                                  word_real_max_size);

            /* Set the search timer. */
            /* """"""""""""""""""""" */
            search_timer = timers.search; /* default 10 s. */

            search_mode = FUZZY;

            if (old_search_mode != FUZZY)
            {
              old_search_mode = FUZZY;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        case '\'':
        case '\"':
          /* ' or " key has been pressed            */
          /* (start of a substring search session). */
          /* """""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

        substring_method:

          if (search_mode == NONE)
          {
            if (!toggles.incremental_search)
              reset_search_buffer(&win,
                                  &search_data,
                                  &timers,
                                  &toggles,
                                  &term,
                                  &daccess,
                                  &langinfo,
                                  last_line,
                                  tmp_word,
                                  word_real_max_size);

            /* Set the search timer. */
            /* """"""""""""""""""""" */
            search_timer = timers.search; /* default 10 s. */

            search_mode = SUBSTRING;

            if (old_search_mode != SUBSTRING)
            {
              old_search_mode = SUBSTRING;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          else
            goto special_cmds_when_searching;

          break;

        case '=':
        case '^':
          /* ^ or = key has been pressed         */
          /* (start of a prefix search session). */
          /* """"""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

        prefix_method:

          if (search_mode == NONE)
          {
            if (!toggles.incremental_search)
              reset_search_buffer(&win,
                                  &search_data,
                                  &timers,
                                  &toggles,
                                  &term,
                                  &daccess,
                                  &langinfo,
                                  last_line,
                                  tmp_word,
                                  word_real_max_size);

            /* Set the search timer. */
            /* """"""""""""""""""""" */
            search_timer = timers.search; /* default 10 s. */

            search_mode = PREFIX;

            if (old_search_mode != PREFIX)
            {
              old_search_mode = PREFIX;
              clean_matches(&search_data, word_real_max_size);
            }

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
            break;
          }
          else
            goto special_cmds_when_searching;

          break;

        kins:
          if (help_mode)
            break;

          /* The INS key has been pressed to tag a word if */
          /* tagging is enabled.                           */
          /* """"""""""""""""""""""""""""""""""""""""""""" */
          if (toggles.taggable && word_a[current].tag_id == 0)
          {
            word_a[current].tag_id = win.next_tag_id++;
            tagged_words++;

            if (toggles.pinable)
              word_a[current].tag_order = tag_nb++;

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          break;

        kdel:
          if (help_mode)
            break;

          /* The DEL key has been pressed to untag a word if */
          /* tagging is enabled.                             */
          /* """"""""""""""""""""""""""""""""""""""""""""""" */
          if (toggles.taggable && word_a[current].tag_id > 0)
          {
            word_a[current].tag_id = 0;
            tagged_words--;

            /* We do not try to change tag_nb here to guaranty that */
            /* tag_nb will be greater than all those already stored */
            /* in all word_a[*].tag_order.                          */
            /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */

            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          break;

        case 't':
          if (help_mode)
            break;

          /* t has been pressed to tag/untag a word if */
          /* tagging is enabled.                       */
          /* """"""""""""""""""""""""""""""""""""""""" */
          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              if (word_a[current].tag_id > 0)
              {
                word_a[current].tag_id = 0;
                tagged_words--;
              }
              else
              {
                word_a[current].tag_id = win.next_tag_id++;
                tagged_words++;

                if (toggles.pinable)
                  word_a[current].tag_order = tag_nb++;
              }

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'u':
          if (help_mode)
            break;

          /* u has been pressed to untag a word if */
          /* tagging is enabled.                   */
          /* """"""""""""""""""""""""""""""""""""" */
          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              if (word_a[current].tag_id > 0)
              {
                word_a[current].tag_id = 0;
                tagged_words--;

                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
              }
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 20:
          /* (CTRL-t) Remove all tags. */
          /* """"""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable && (win.next_tag_id > 1 || marked >= 0))
            {
              tagged_words    = 0;
              win.next_tag_id = 1;
              marked          = -1;

              for (wi = 0; wi < count; wi++)
              {
                word_a[wi].tag_id    = 0;
                word_a[wi].tag_order = 0;
              }

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        tag_column:
        case 'c':
          if (help_mode)
            break;

          /* Tag all the words in the current column. */
          /* """""""""""""""""""""""""""""""""""""""" */
          if (search_mode == NONE)
          {
            if (toggles.taggable || toggles.pinable)
            {
              long col, cur_col, marked_col;
              long first, last;

              int tagged;

              if (!win.col_mode)
                break;

              /* Get the current column number. */
              /* """""""""""""""""""""""""""""" */
              cur_col = current
                        - first_word_in_line_a[line_nb_of_word_a[current]] + 1;

              /* Determine the loop values according to the existence of */
              /* a marked word and is value.                             */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (marked == -1)
              {
                first = 0;
                last  = count - 1;
              }
              else
              {
                marked_col = marked
                             - first_word_in_line_a[line_nb_of_word_a[marked]]
                             + 1;

                /* Ignore the marked word is its is not on the same column */
                /* as the current word.                                    */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                if (cur_col == marked_col)
                {
                  if (marked <= current)
                  {
                    first = first_word_in_line_a[line_nb_of_word_a[marked]];
                    last  = current;
                  }
                  else
                  {
                    first = first_word_in_line_a[line_nb_of_word_a[current]];
                    last  = marked;

                    /* Pre-increment tag_nb with is maximum mulue as     */
                    /* it will be decremented in the following loop when */
                    /* marked > current                                  */
                    /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
                    tag_nb += marked - current + 1;
                  }
                }
                else
                  break;
              }

              /* Tag from first to last. */
              /* """"""""""""""""""""""" */
              col    = 0;
              tagged = 0;

              for (wi = first; wi <= last; wi++)
              {
                col++;
                if (col == cur_col && word_a[wi].is_selectable
                    && word_a[wi].tag_id == 0)
                {
                  word_a[wi].tag_id = win.next_tag_id;
                  tagged_words++;

                  if (toggles.pinable)
                  {
                    if (marked <= current)
                      word_a[wi].tag_order = tag_nb++;
                    else
                      word_a[wi].tag_order = tag_nb--;
                  }

                  tagged = 1;
                }

                /* Time to go back to column 1? */
                /* """""""""""""""""""""""""""" */
                if (word_a[wi].is_last)
                  col = 0;
              }

              if (tagged)
                win.next_tag_id++;

              if (marked > current)
                tag_nb += marked - current + 1;

              marked = -1;

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'C':
          /* Allow to tag part of a column, the first invocation marks    */
          /* the starting word and the second marks the words in between. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            /* Mark the first word is not already marked. */
            /* """""""""""""""""""""""""""""""""""""""""" */
            if (!win.col_mode)
              break;

            if (!toggles.taggable && !toggles.pinable)
              break;

            if (marked == -1)
              goto mark_word;

            goto tag_column;
          }
          else
            goto special_cmds_when_searching;
          break;

        tag_line:
        case 'r':
          /* Tag all the words in the current line. */
          /* """""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable || toggles.pinable)
            {
              long first, last;
              long marked_line;
              int  tagged;

              if (!win.col_mode && !win.line_mode)
                break;

              if (marked >= 0)
              {
                marked_line = line_nb_of_word_a[marked];
                if (marked_line == line_nb_of_word_a[current])
                {
                  if (marked <= current)
                  {
                    first = marked;
                    last  = current;
                  }
                  else
                  {
                    first = current;
                    last  = marked;

                    /* Pre-increment tag_nb with is maximum mulue as     */
                    /* it will be decremented in the following loop when */
                    /* marked > current                                  */
                    /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
                    tag_nb += marked - current + 1;
                  }
                }
                else
                  break;
              }
              else
              {
                first = first_word_in_line_a[line_nb_of_word_a[current]];
                if (line_nb_of_word_a[current] == line_nb_of_word_a[count - 1])
                  last = count - 1;
                else
                  last = first_word_in_line_a[line_nb_of_word_a[current] + 1]
                         - 1;
              }

              /* Tag from first to last. */
              /* """"""""""""""""""""""" */
              wi     = first;
              tagged = 0;

              do
              {
                if (word_a[wi].is_selectable)
                {
                  if (word_a[wi].tag_id == 0)
                  {
                    word_a[wi].tag_id = win.next_tag_id;
                    tagged_words++;

                    if (toggles.pinable)
                    {
                      if (marked <= current)
                        word_a[wi].tag_order = tag_nb++;
                      else
                        word_a[wi].tag_order = tag_nb--;
                    }

                    tagged = 1;
                  }
                }
                wi++;
              } while (wi <= last);

              if (tagged)
                win.next_tag_id++;

              if (marked > current)
                tag_nb += marked - current + 1;

              marked = -1;

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'R':
          /* Make sure that all the words in the current line */
          /* are not tagged.                                  */
          /* """""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (!win.col_mode && !win.line_mode)
              break;

            if (!toggles.taggable && !toggles.pinable)
              break;

            if (marked == -1)
              goto mark_word;

            goto tag_line;
          }
          else
            goto special_cmds_when_searching;
          break;

        mark_word:
        case 'm':
          /* Mark the current word (ESC clears the mark). */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              marked = current;

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        unmark_word:
        case 'M':
          if (help_mode)
            break;

          /* unmark the current word (ESC clears the mark). */
          /* """""""""""""""""""""""""""""""""""""""""""""" */
          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              marked = -1;

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        tag_to_mark:
        case 'T':
          /* T has been pressed to tag all matching words resulting */
          /* from a previous search if tagging is enabled.          */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              long i;

              /* Is words have been matched by a recent search, tag them. */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (BUF_LEN(matching_words_da) > 0 && marked == -1)
              {
                int tagged = 0;

                for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
                {
                  wi = matching_words_da[i];

                  if (word_a[wi].tag_id == 0)
                  {
                    word_a[wi].tag_id = win.next_tag_id;
                    tagged_words++;

                    if (toggles.pinable)
                      word_a[wi].tag_order = tag_nb++;

                    tagged = 1;
                  }
                }
                if (tagged)
                  win.next_tag_id++;
              }
              else /* Tag word between the marked and current words. */
              {
                if (marked == -1)
                  goto mark_word;

                {
                  long first, last;
                  int  tagged = 0;

                  if (marked <= current)
                  {
                    first = marked;
                    last  = current;
                  }
                  else
                  {
                    first = current;
                    last  = marked;

                    /* Pre-increment tag_nb with is maximum mulue as     */
                    /* it will be decremented in the following loop when */
                    /* marked > current                                  */
                    /* ''''''''''''''''''''''''''''''''''''''''''''''''' */
                    tag_nb += marked - current + 1;
                  }

                  tagged = 0;

                  for (wi = first; wi <= last; wi++)
                  {
                    if (!word_a[wi].is_selectable)
                      continue;

                    if (word_a[wi].tag_id == 0)
                    {
                      word_a[wi].tag_id = win.next_tag_id;
                      tagged_words++;

                      if (toggles.pinable)
                      {
                        if (marked <= current)
                          word_a[wi].tag_order = tag_nb++;
                        else
                          word_a[wi].tag_order = tag_nb--;
                      }

                      tagged = 1;
                    }
                  }

                  if (tagged)
                    win.next_tag_id++;

                  if (marked > current)
                    tag_nb += marked - current + 1;

                  marked = -1;
                }
              }

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
                              &langinfo);
            }
          }
          else
            goto special_cmds_when_searching;
          break;

        adaptative_tag_to_mark:
        case 'Z':
          /* Z has been pressed to tag consecutive word in a given zone . */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              if (marked == -1)
                marked = current;
              else if (marked == current)
                marked = -1;
              else if (marked >= 0 && win.col_mode)
              {
                long cur_col =
                  current - first_word_in_line_a[line_nb_of_word_a[current]]
                  + 1;
                long mark_col =
                  marked - first_word_in_line_a[line_nb_of_word_a[marked]] + 1;
                if (cur_col == mark_col)
                  goto tag_column;
                else if (line_nb_of_word_a[current]
                         == line_nb_of_word_a[marked])
                  goto tag_line;
                else
                  goto tag_to_mark;
              }
              else if (marked >= 0)
              {
                if (win.line_mode
                    && line_nb_of_word_a[current] == line_nb_of_word_a[marked])
                  goto tag_line;
                else
                  goto tag_to_mark;
              }
            }
            nl = disp_lines(&win,
                            &toggles,
                            current,
                            count,
                            search_mode,
                            &search_data,
                            &term,
                            last_line,
                            tmp_word,
                            &langinfo);
          }
          else
            goto special_cmds_when_searching;
          break;

        case 'U':
          /* U has been pressed to undo the last tagging operation. */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          if (search_mode == NONE)
          {
            if (toggles.taggable)
            {
              for (wi = 0; wi < count; wi++)
              {
                if (!word_a[wi].is_selectable)
                  continue;

                if (word_a[wi].tag_id > 0)
                {
                  if (word_a[wi].tag_id == win.next_tag_id - 1)
                  {
                    word_a[wi].tag_id = 0;
                    tagged_words--;
                  }
                }
              }

              if (win.next_tag_id > 1)
                win.next_tag_id--;

              nl = disp_lines(&win,
                              &toggles,
                              current,
                              count,
                              search_mode,
                              &search_data,
                              &term,
                              last_line,
                              tmp_word,
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
          /* A digit has been pressed to build a number to be used for */
          /* A direct access to a words if direct access is enabled.   */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (help_mode)
            break;

          {
            if (search_mode == NONE && daccess.mode != DA_TYPE_NONE)
            {
              wchar_t *w;
              long    *pos;

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

                /* Set new first column to display. */
                /* """""""""""""""""""""""""""""""" */
                set_new_first_column(&win, &term);

                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
              }
              else
              {
                if (current != prev_current)
                {
                  current = prev_current;

                  if (current < win.start || current > win.end)
                    last_line = build_metadata(&term, count, &win);

                  /* Set new first column to display. */
                  /* """""""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
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
          /* backspace/CTRL-H management. */
          /* """""""""""""""""""""""""""" */
          if (help_mode)
            break;

          {
            long i;

            if (search_mode != NONE)
            {
              if (search_data.mb_len > 0)
              {
                char *prev;

                prev = utf8_prev(search_data.buf,
                                 search_data.buf + search_data.len);

                /* Reset the error indicator if we erase the first non */
                /* matching element of the search buffer.              */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                if (search_mode != FUZZY
                    || search_data.mb_len == search_data.fuzzy_err_pos - 1)
                {
                  search_data.err           = 0;
                  search_data.fuzzy_err_pos = -1;
                }
                search_data.mb_len--;

                /* Remove la last glyph in search_data.buf. */
                /* """""""""""""""""""""""""""""""""""""""" */
                if (search_data.mb_len > 0)
                {
                  search_data.len -= utf8_get_length(*prev);
                  *prev = '\0';
                }
                else /* No more glyph to remove. */
                {
                  for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
                  {
                    long n = matching_words_da[i];

                    word_a[n].is_matching = 0;

                    memset(word_a[n].bitmap,
                           '\0',
                           (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
                  }

                  BUF_CLEAR(matching_words_da);

                  *search_data.buf = '\0';
                  search_data.len  = 0;
                }

                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
              }
              else
              {
                my_beep(&toggles);
                if (search_data.err)
                {
                  search_data.err           = 0;
                  search_data.fuzzy_err_pos = -1;

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
                }
              }

              if (search_data.mb_len > 0)
                goto special_cmds_when_searching;
              else
              {
                /* When there is only one glyph in the search list in       */
                /* FUZZY and SUBSTRING mode then all is already done except */
                /* the cleanup of the first level of the tst_search_list.   */
                /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
                if (search_mode != PREFIX)
                {
                  sub_tst_t *sub_tst_data;
                  ll_node_t *node;

                  node         = tst_search_list->tail;
                  sub_tst_data = (sub_tst_t *)(node->data);

                  search_data.err = 0;

                  sub_tst_data->count = 0;
                }
              }
            }
            else
            {
              wchar_t *w;
              long    *pos;

              if (daccess_stack_head > 0)
                daccess_stack[--daccess_stack_head] = '\0';

              w   = utf8_strtowcs(daccess_stack);
              pos = tst_search(tst_daccess, w);
              free(w);

              if (pos != NULL)
              {
                current = *pos;

                if (current < win.start || current > win.end)
                  last_line = build_metadata(&term, count, &win);

                /* Set new first column to display. */
                /* """""""""""""""""""""""""""""""" */
                set_new_first_column(&win, &term);

                nl = disp_lines(&win,
                                &toggles,
                                current,
                                count,
                                search_mode,
                                &search_data,
                                &term,
                                last_line,
                                tmp_word,
                                &langinfo);
              }
            }
          }

          break;

        case '?':
          /* Help mode. */
          /* """""""""" */
          if (help_mode)
          {
            help_timer = timers.help; /* Rearm the timer; */
            if (fst_disp_help_line > 0)
              fst_disp_help_line = 0;
            else
              break;
          }

          if (search_mode == NONE)
          {
            if (init_help_needed)
            {
              /* (Re-)Initialise the content of the internal help system. */
              /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              int length = BUF_LEN(help_lines_da);
              int i;

              for (i = 0; i < length; i++)
                BUF_FREE(help_lines_da[i]);

              BUF_FREE(help_lines_da);

              help_lines_da = init_help(&win,
                                        &term,
                                        &toggles,
                                        last_line,
                                        &help_items_da);

              init_help_needed = 0;
            }

            disp_help(&win, &term, help_lines_da, fst_disp_help_line);
            help_mode = 1;

            /* Arm the help timer. */
            /* """"""""""""""""""" */
            help_timer = timers.help; /* default 30 s. */
          }
          else
            goto special_cmds_when_searching;
          break;

        kmouse:
          if (help_mode)
            break;

          {
            long          new_current;
            unsigned int  iCb, iCx, iCy;
            unsigned char cCb, cCx, cCy;
            char          action;

            int state;
            int button, old_button;
            int line_click;
            int column_click;
            int error;

            long old_current = current;

            struct timespec actual_click_ts;

            switch (mouse_proto)
            {
              case MOUSE1006:
                if (sscanf((char *)buffer + 3,
                           "%u;%u;%u%c",
                           &iCb,
                           &iCx,
                           &iCy,
                           &action)
                    != 4)
                  goto ignore_mouse_event;

                state        = iCb & ~3;
                button       = iCb & 3;
                line_click   = iCy;
                column_click = iCx;

                /* Only consider button click (not release) events. */
                /* """""""""""""""""""""""""""""""""""""""""""""""" */
                if (action == 'm') /* released. */
                  goto ignore_mouse_event;

                break;

              case MOUSE1015:
                if (sscanf((char *)buffer + 2,
                           "%u;%u;%u%c",
                           &iCb,
                           &iCx,
                           &iCy,
                           &action)
                    != 4)
                  goto ignore_mouse_event;

                state        = (iCb - 32) & ~3;
                button       = (iCb - 32) & 3;
                line_click   = iCy;
                column_click = iCx;

                /* Only consider button click (not release) events. */
                /* """""""""""""""""""""""""""""""""""""""""""""""" */
                if (state == 0 && button == 3) /* released. */
                  goto ignore_mouse_event;

                break;

              case MOUSE1000:
                if (sscanf((char *)buffer + 3, "%c%c%c", &cCb, &cCx, &cCy) != 3)
                  goto ignore_mouse_event;

                state        = (cCb - 32) & ~3;
                button       = (cCb - 32) & 3;
                line_click   = cCy - 32;
                column_click = cCx - 32;

                /* Only consider button click (not release) events. */
                /* """""""""""""""""""""""""""""""""""""""""""""""" */
                if (button == 3) /* released. */
                  goto ignore_mouse_event;

                break;

              default:
                goto ignore_mouse_event;
            }

            /* Mouse Button remapping. */
            /* """"""""""""""""""""""" */
            old_button = button;
            button     = mouse.button[button] - 1;
            long clicked_line;
            int  clicked_arrow;

            /* Only buttons 0,1 and 2 are considered for clicks. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            if (button < 0 || button > 2)
              goto ignore_mouse_event;

            /* Do not do anything if the user has above or below the window. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (line_click < term.curs_line
                || line_click
                     >= term.curs_line + win.max_lines + (win.has_hbar ? 1 : 0))
              break;

            /* Mouse wheel scroll down. */
            /* """""""""""""""""""""""" */
            if (state == 64 && old_button == 1)
              goto kd; /* down arrow. */

            /* Mouse wheel scroll down + CTRL. */
            /* """"""""""""""""""""""""""""""" */
            if (state == 80 && old_button == 1)
              goto knp; /* PgDn. */

            /* Mouse wheel scroll up. */
            /* """""""""""""""""""""" */
            if (state == 64 && old_button == 0)
              goto ku; /* up arrow. */

            /* Mouse wheel scroll up + CTRL. */
            /* """"""""""""""""""""""""""""" */
            if (state == 80 && old_button == 0)
              goto kpp; /* PgUp. */

            /* Manage the clicks at the ends of the vertical scroll bar. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (button == 0 && column_click == win.sb_column + 1)
            {
              if (line_click == term.curs_line + win.max_lines - 1)
              {
                if (state == 16)
                  goto knp; /* PgDn. */
                if (state == 0)
                  goto kd; /* down arrow. */
              }
              else if (line_click == term.curs_line)
              {
                if (state == 16)
                  goto kpp; /* PgUp. */
                if (state == 0)
                  goto ku; /* up arrow. */
              }
              else if (line_click > term.curs_line
                       && line_click < term.curs_line + win.max_lines - 1)
              {
                float ratio; /* cursor ratio in the between the extremities *
                              | of the scroll bar.                          */

                if (win.max_lines > 3)
                {
                  float corr; /* scaling correction. */
                  long  line; /* new selected line.  */

                  corr  = (float)(win.max_lines - 2) / (win.max_lines - 3);
                  ratio = (float)(line_click - term.curs_line - 1)
                          / (win.max_lines - 2);

                  /* Find the location of the new current word based on  */
                  /* the position of the cursor in the scroll bar.       */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                  line    = ratio * corr * line_nb_of_word_a[count - 1];
                  current = first_word_in_line_a[line];

                  /* Make sure the new current word is selectable. */
                  /* """"""""""""""""""""""""""""""""""""""""""""" */
                  if (current < first_selectable)
                    current = first_selectable;

                  if (current > last_selectable)
                    current = last_selectable;

                  while (!word_a[current].is_selectable)
                    current++;

                  /* Display the window. */
                  /* """"""""""""""""""" */
                  last_line = build_metadata(&term, count, &win);

                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
                }
              }
            }
            else
              /* Manage the clicks in the horizontal scroll bar. */
              /* """"""""""""""""""""""""""""""""""""""""""""""" */
              if (win.has_hbar && button == 0
                  && line_click == term.curs_line + win.max_lines)
              {
                long wi; /* Word index. */
                long line = line_nb_of_word_a[current];
                long leftmost;
                long rightmost;
                int  leftmost_start;
                int  rightmost_end;

                /* Find the first selectable word in the line */
                /* containing the cursor.                     */
                /* """""""""""""""""""""""""""""""""""""""""" */
                wi = first_word_in_line_a[line];

                while (!word_a[wi].is_selectable)
                  wi++;

                leftmost       = wi;
                leftmost_start = word_a[leftmost].start;

                if (column_click == 1)
                {
                  /* First, manage the case where the user clicked at the */
                  /* beginning of the scroll bar.                         */
                  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
                  if (current > leftmost && word_a[current].start > 0)
                    goto kl;
                }

                /* Else we need to calculate the rightmost selectable word */
                /* in the line containing the cursor.                      */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                else if (line == last_line)
                  wi = count - 1;
                else
                  wi = first_word_in_line_a[line + 1] - 1;

                while (!word_a[wi].is_selectable)
                  wi--;

                rightmost = wi;

                /* Manage the case where the users clicked at the end */
                /* of the scroll bar.                                 */
                /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                if (column_click == term.ncolumns - 1)
                  if (current < rightmost && current < count - 1
                      && word_a[current + 1].start > 0)
                    goto kr;

                /* Finally manage the core where the users clicked in */
                /* the crossbar.                                      */
                /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                rightmost_end = word_a[rightmost].end;

                {
                  long   index;
                  int    target;
                  double ratio;

                  if (column_click >= 2
                      && column_click - 2 <= term.ncolumns - 4)
                  {
                    ratio  = (1.0 * column_click - 2) / (term.ncolumns - 4);
                    target = (int)((rightmost_end - leftmost_start + 1) * ratio)
                             + leftmost_start;

                    if (target > 0) /* General case. */
                    {
                      if (word_a[current].start <= target)
                        index = current;
                      else
                        index = leftmost;

                      while (index <= rightmost
                             && word_a[index].start <= target)
                        current = index++;
                    }
                    else /* Trivial case. */
                      current = leftmost;

                    set_new_first_column(&win, &term);

                    nl = disp_lines(&win,
                                    &toggles,
                                    current,
                                    count,
                                    search_mode,
                                    &search_data,
                                    &term,
                                    last_line,
                                    tmp_word,
                                    &langinfo);
                  }
                }

                break;
              }
              else
                /* Manage clicks on the horizontal arrows in lines if any. */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                if (win.has_truncated_lines
                    && shift_arrow_clicked(&win,
                                           &term,
                                           line_click,
                                           column_click,
                                           &clicked_line,
                                           &clicked_arrow))
                {
                  if (button == 0)
                    switch (clicked_arrow)
                    {
                      case 0: /* left */
                        shift_left(&win,
                                   &term,
                                   &toggles,
                                   &search_data,
                                   &langinfo,
                                   &nl,
                                   last_line,
                                   tmp_word,
                                   clicked_line);
                        break;

                      case 1: /* right */
                        shift_right(&win,
                                    &term,
                                    &toggles,
                                    &search_data,
                                    &langinfo,
                                    &nl,
                                    last_line,
                                    tmp_word,
                                    clicked_line);
                        break;
                    }
                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
                }
                else
                {
                  /* Get the new current word on click. */
                  /* """""""""""""""""""""""""""""""""" */
                  error       = 0;
                  new_current = get_clicked_index(&win,
                                                  &term,
                                                  line_click,
                                                  column_click,
                                                  &error);

                  /* Update the selection index and refresh if needed. */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
                  if ((toggles.taggable || toggles.pinable
                       || new_current != current)
                      && error == 0)
                  {
                    current = new_current;

                    /* Manage the marking of a word.          */
                    /* (button 0 + CTRL or button 2 pressed). */
                    /* """""""""""""""""""""""""""""""""""""" */
                    if ((button == 0 && state == 16)
                        && (toggles.taggable || toggles.pinable))
                    {
                      if (marked == -1)
                        goto mark_word;
                      else
                        goto unmark_word;
                    }

                    /* Manage the tagging of a word.          */
                    /* (button 2 + CTRL or button 2 pressed). */
                    /* """""""""""""""""""""""""""""""""""""" */
                    if ((button == 2 && state == 0)
                        && (toggles.taggable || toggles.pinable))
                    {
                      if (word_a[current].tag_id > 0)
                        goto kdel;
                      else
                        goto kins;
                    }

                    if ((button == 2 && state == 16)
                        && (toggles.taggable || toggles.pinable))
                      goto adaptative_tag_to_mark; /* Like 'Z' keyboard command. */

                    /* Redisplay the new window if the first button was pressed   */
                    /* otherwise reset the cursor position to its previous value. */
                    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
                    if (button == 0 || button == 2)
                      nl = disp_lines(&win,
                                      &toggles,
                                      current,
                                      count,
                                      search_mode,
                                      &search_data,
                                      &term,
                                      last_line,
                                      tmp_word,
                                      &langinfo);
                    else
                      current = old_current;
                  }

                  /* Manage double clicks if the clicked word is selectable. */
                  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                  if (!disable_double_click && button == 0)
                  {
                    /* More than one clicks on the same word? */
                    /* """""""""""""""""""""""""""""""""""""" */
                    if (click_nr > 0 && old_current == current)
                    {
                      double delay;

                      /* Get the delay between the actual click and */
                      /* the previous one.                          */
                      /* """""""""""""""""""""""""""""""""""""""""" */
                      clock_gettime(CLOCK_MONOTONIC, &actual_click_ts);
                      delay = (1000.0 * actual_click_ts.tv_sec
                               + 1e-6 * actual_click_ts.tv_nsec)
                              - (1000.0 * last_click_ts.tv_sec
                                 + 1e-6 * last_click_ts.tv_nsec);

                      if (!error && delay > 0
                          && delay <= mouse.double_click_delay)
                        goto enter; /* Press Enter. */
                      else
                        clock_gettime(CLOCK_MONOTONIC, &last_click_ts);
                    }
                    else
                    {
                      /* The first click on a selectable was not make yet. */
                      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
                      clock_gettime(CLOCK_MONOTONIC, &last_click_ts);

                      if (!error)
                        click_nr = 1;
                    }
                  }
                }
          }

        ignore_mouse_event:
          break;

        special_cmds_when_searching:
        default:
          if (help_mode)
            break;

          {
            int        c; /* byte index in the scancode string .*/
            sub_tst_t *sub_tst_data;
            long       i;

            if (search_mode != NONE)
            {
              long       old_len    = search_data.len;
              long       old_mb_len = search_data.mb_len;
              ll_node_t *node;
              wchar_t   *ws;

              /* Copy all the bytes included in the key press to buffer. */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (buffer[0] != 0x08 && buffer[0] != 0x7f) /* Backspace. */
              {
                /* The only case where we have to manage backspace hits */
                /* here is if the user has entered them in fuzzy search */
                /* mode. As the search buffer has already been amended, */
                /* we do not have to update the search buffer again.    */
                /* '''''''''''''''''''''''''''''''''''''''''''''''''''' */
                for (c = 0; c < sc
                            && search_data.mb_len
                                 < word_real_max_size - daccess.flength;
                     c++)
                  search_data.buf[search_data.len++] = buffer[c];

                /* Update the glyph array with the content of the search */
                /* buffer.                                               */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
                if (search_data.mb_len < word_real_max_size - daccess.flength)
                {
                  search_data.off_a[search_data.mb_len] = old_len;
                  search_data.len_a[search_data.mb_len] = search_data.len
                                                          - old_len;
                  search_data.mb_len++;
                }
              }

              /* Restart the search timer. */
              /* """"""""""""""""""""""""" */
              search_timer = timers.search; /* default 10 s. */

              if (search_mode == PREFIX)
              {
                ws = utf8_strtowcs(search_data.buf);

                /* Purge the matching words list. */
                /* """""""""""""""""""""""""""""" */
                for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
                {
                  long n = matching_words_da[i];

                  word_a[n].is_matching = 0;

                  memset(word_a[n].bitmap,
                         '\0',
                         (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
                }

                /* matching_words_da is updated by tst_search_cb. */
                /* """""""""""""""""""""""""""""""""""""""""""""" */
                BUF_CLEAR(matching_words_da);
                tst_prefix_search(tst_word, ws, tst_search_cb);

                if (BUF_LEN(matching_words_da) > 0)
                {
                  if (search_data.len == old_len
                      && BUF_LEN(matching_words_da) == 1 && buffer[0] != 0x08
                      && buffer[0] != 0x7f)
                    my_beep(&toggles);
                  else
                  {
                    /* Adjust the bitmap to the ending version. */
                    /* """""""""""""""""""""""""""""""""""""""" */
                    update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                    current = matching_words_da[0];

                    if (current < win.start || current > win.end)
                      last_line = build_metadata(&term, count, &win);

                    /* Set new first column to display. */
                    /* """""""""""""""""""""""""""""""" */
                    set_new_first_column(&win, &term);

                    nl = disp_lines(&win,
                                    &toggles,
                                    current,
                                    count,
                                    search_mode,
                                    &search_data,
                                    &term,
                                    last_line,
                                    tmp_word,
                                    &langinfo);
                  }
                }
                else
                {
                  my_beep(&toggles);

                  search_data.err    = 1;
                  search_data.len    = old_len;
                  search_data.mb_len = old_mb_len;

                  search_data.buf[search_data.len] = '\0';

                  /* Set new first column to display. */
                  /* """""""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
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
                /* a sorted array to nodes in the words tst.              */
                /* Each of these node starts a matching candidate.        */
                /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
                wchar_t *w = utf8_strtowcs(search_data.buf + old_len);

                /* zero previous matching indicators. */
                /* """""""""""""""""""""""""""""""""" */
                for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
                {
                  long n = matching_words_da[i];

                  word_a[n].is_matching = 0;

                  memset(word_a[n].bitmap,
                         '\0',
                         (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
                }

                BUF_CLEAR(matching_words_da);

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
                }
                else
                {
                  if (search_data.mb_len == 1)
                  {
                    /* Search all the sub-tst trees having the first      */
                    /* searched character as children and store them in   */
                    /* the sub tst array attached to the searched symbol. */
                    /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                    tst_fuzzy_traverse(tst_word, NULL, 0, w[0]);

                    node         = tst_search_list->tail;
                    sub_tst_data = (sub_tst_t *)(node->data);
                    if (sub_tst_data->count == 0)
                    {
                      my_beep(&toggles);

                      search_data.len    = 0;
                      search_data.mb_len = 0;
                      search_data.buf[0] = '\0';

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
                      /* level n list.                                      */
                      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                      int rc;

                      sub_tst_t *tst_fuzzy_level_data;

                      tst_fuzzy_level_data = sub_tst_new();

                      ll_append(tst_search_list, tst_fuzzy_level_data);

                      node         = tst_search_list->tail->prev;
                      sub_tst_data = (sub_tst_t *)(node->data);

                      rc = 0;
                      for (index = 0; index < sub_tst_data->count; index++)
                        rc += tst_fuzzy_traverse(sub_tst_data->array[index],
                                                 NULL,
                                                 1,
                                                 w[0]);

                      if (rc == 0)
                      {
                        free(tst_fuzzy_level_data->array);
                        free(tst_fuzzy_level_data);

                        ll_delete(tst_search_list, tst_search_list->tail);

                        search_data.err = 1;
                        if (search_data.fuzzy_err_pos == -1)
                          search_data.fuzzy_err_pos = search_data.mb_len;

                        my_beep(&toggles);

                        search_data.len    = old_len;
                        search_data.mb_len = old_mb_len;

                        search_data.buf[search_data.len] = '\0';
                      }
                    }
                    else
                      my_beep(&toggles);
                  }
                }
                free(w);

                /* Process this level to mark the word found as a matching */
                /* word if any.                                            */
                /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
                node         = tst_search_list->tail;
                sub_tst_data = (sub_tst_t *)(node->data);

                for (index = 0; index < sub_tst_data->count; index++)
                  tst_traverse(sub_tst_data->array[index],
                               set_matching_flag,
                               0);

                /* Update the bitmap and re-display the window. */
                /* """""""""""""""""""""""""""""""""""""""""""" */
                if (BUF_LEN(matching_words_da) > 0)
                {
                  if (search_data.only_starting)
                    select_starting_matches(&win,
                                            &term,
                                            &search_data,
                                            &last_line);
                  else if (search_data.only_ending)
                    select_ending_matches(&win,
                                          &term,
                                          &search_data,
                                          &last_line);
                  else
                    /* Adjust the bitmap to the ending version. */
                    /* """""""""""""""""""""""""""""""""""""""" */
                    update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                  current = matching_words_da[0];

                  if (current < win.start || current > win.end)
                    last_line = build_metadata(&term, count, &win);

                  /* Set new first column to display. */
                  /* """""""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
                }
                else
                  my_beep(&toggles);
              }
              else /* SUBSTRING. */
              {
                wchar_t *w = utf8_strtowcs(search_data.buf);

                /* Purge the matching words list. */
                /* """""""""""""""""""""""""""""" */
                for (i = 0; i < (long)BUF_LEN(matching_words_da); i++)
                {
                  long n = matching_words_da[i];

                  word_a[n].is_matching = 0;

                  memset(word_a[n].bitmap,
                         '\0',
                         (word_a[n].mb - daccess.flength) / CHAR_BIT + 1);
                }

                BUF_CLEAR(matching_words_da);

                if (search_data.mb_len == 1)
                {
                  /* Search all the sub-tst trees having the first      */
                  /* searched character as children and store them in   */
                  /* the sub tst array attached to the searched symbol. */
                  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                  tst_substring_traverse(tst_word, NULL, 0, w[0]);

                  node         = tst_search_list->tail;
                  sub_tst_data = (sub_tst_t *)(node->data);

                  for (index = 0; index < sub_tst_data->count; index++)
                    tst_traverse(sub_tst_data->array[index],
                                 set_matching_flag,
                                 0);
                }
                else
                {
                  /* Search for the rest of the word in all the sub-tst */
                  /* trees previously found.                            */
                  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
                  node         = tst_search_list->tail;
                  sub_tst_data = (sub_tst_t *)(node->data);

                  BUF_CLEAR(matching_words_da);

                  for (index = 0; index < sub_tst_data->count; index++)
                    tst_prefix_search(sub_tst_data->array[index],
                                      w + 1,
                                      tst_search_cb);
                }

                if (BUF_LEN(matching_words_da) > 0)
                {
                  if (search_data.len == old_len
                      && BUF_LEN(matching_words_da) == 1 && buffer[0] != 0x08
                      && buffer[0] != 0x7f)
                    my_beep(&toggles);
                  else
                  {
                    if (search_data.only_starting)
                      select_starting_matches(&win,
                                              &term,
                                              &search_data,
                                              &last_line);
                    else if (search_data.only_ending)
                      select_ending_matches(&win,
                                            &term,
                                            &search_data,
                                            &last_line);
                    else
                      update_bitmaps(search_mode, &search_data, NO_AFFINITY);

                    current = matching_words_da[0];

                    if (current < win.start || current > win.end)
                      last_line = build_metadata(&term, count, &win);

                    /* Set new first column to display. */
                    /* """""""""""""""""""""""""""""""" */
                    set_new_first_column(&win, &term);

                    nl = disp_lines(&win,
                                    &toggles,
                                    current,
                                    count,
                                    search_mode,
                                    &search_data,
                                    &term,
                                    last_line,
                                    tmp_word,
                                    &langinfo);
                  }
                }
                else
                {
                  my_beep(&toggles);

                  search_data.err = 1;
                  search_data.len = old_len;
                  search_data.mb_len--;
                  search_data.buf[search_data.len] = '\0';

                  /* Set new first column to display. */
                  /* """""""""""""""""""""""""""""""" */
                  set_new_first_column(&win, &term);

                  nl = disp_lines(&win,
                                  &toggles,
                                  current,
                                  count,
                                  search_mode,
                                  &search_data,
                                  &term,
                                  last_line,
                                  tmp_word,
                                  &langinfo);
                }
              }
            }
          }
      }
    }
  }
}
