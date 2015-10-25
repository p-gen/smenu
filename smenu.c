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

#define CHARSCHUNK 8
#define WORDSCHUNK 8
#define COLSCHUNK 16

#define _XOPEN_SOURCE 600
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <term.h>
#include <regex.h>
#include <errno.h>
#include <wchar.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "smenu.h"

int count = 0;               /* number of words read from stdin  */
int current;                 /* index the current selection      *
                              * (under the cursor)               */
int new_current;             /* final current position, (used in *
                              * search function)                 */
int *line_nb_of_word_a;      /* array containig the line number  *
                              * (from 0) of each word read       */
int *first_word_in_line_a;   /* array containing the index of    *
                              * the first word of each lines     */
int search_mode = 0;         /* 1 if in search mode else 0       */
int help_mode = 0;           /* 1 if help is display else 0      */

int (*my_isprint) (int);

static int delims_cmp(const void *a, const void *b);

/* UTF-8 useful symbols */
/* """"""""""""""""""""" */
char *sbar_broken_line = "\xc2\xa6";    /* broken_bar                       */
char *sbar_arr_left = "\xe2\x86\x90";   /* leftwards_arrow                  */
char *sbar_arr_right = "\xe2\x86\x92";  /* rightwards_arrow                 */

char *sbar_line = "\xe2\x94\x82";       /* box_drawings_light_vertical      */
char *sbar_top = "\xe2\x94\x90";        /* box_drawings_light_down_and_left */
char *sbar_down = "\xe2\x94\x98";       /* box_drawings_light_up_and_left   */
char *sbar_curs = "\xe2\x95\x91";       /* box_drawings_double_vertical     */
char *sbar_arr_up = "\xe2\x96\xb2";     /* black_up_pointing_triangle       */
char *sbar_arr_down = "\xe2\x96\xbc";   /* black_down_pointing_triangle     */

/* Locale informations */
/* """"""""""""""""""" */
struct langinfo_s
{
  int utf8;                  /* charset is UTF-8              */
  size_t bits;               /* number of bits in the charset */
};

struct charsetinfo_s
{
  char *name;                /* canonical name of the allowed charset */
  size_t bits;               /* number of bits in this charset        */
};

struct toggle_s
{
  int del_line;              /* 1 if the clean option is set (-d) else 0   */
  int enter_val_in_search;   /* 1 if ENTER validates in search mode else 0 */
  int no_scrollbar;          /* 1 to disable the scrollbar display else 0  */
  int blank_nonprintable;    /* 1 to try to display non-blanks in          *
                              * symbolic form else 0                       */
};

struct limits_s
{
  int word_length;
  int words;
  int cols;
};

/* Mapping of supported charsets and the number of bits used in them */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
charsetinfo_t all_supported_charsets[] = {
  {"UTF-8", 8},
  {"ANSI_X3.4-1968", 7},
  {"ANSI_X3.4-1986", 7},
  {"ISO_646.BASIC", 7},
  {"ISO_646.IRV", 7},
  {"ISO-8859-1", 8},
  {"ISO-8859-2", 8},
  {"ISO-8859-3", 8},
  {"ISO-8859-4", 8},
  {"ISO-8859-5", 8},
  {"ISO-8859-6", 8},
  {"ISO-8859-7", 8},
  {"ISO-8859-8", 8},
  {"ISO-8859-9", 8},
  {"ISO-8859-10", 8},
  {"ISO-8859-11", 8},
  {"ISO-8859-12", 8},
  {"ISO-8859-13", 8},
  {"ISO-8859-14", 8},
  {"ISO-8859-15", 8},
  {"ISO-8859-16", 8},
  {"CP1252", 8},
  {0, 0}
};

/* Variables used in signal handlers */
/* """"""""""""""""""""""""""""""""" */
volatile sig_atomic_t got_winch = 0;
volatile sig_atomic_t got_winch_alrm = 0;
volatile sig_atomic_t got_search_alrm = 0;
volatile sig_atomic_t got_help_alrm = 0;

/* Terminal setting variables */
/* """""""""""""""""""""""""" */
struct termios new_in_attrs;
struct termios old_in_attrs;

/* Interval timers used */
/* """""""""""""""""""" */
struct itimerval search_itv;
struct itimerval hlp_itv;
struct itimerval winch_itv;

/* Structure containing the color components */
/* """"""""""""""""""""""""""""""""""""""""" */
struct color_s
{
  int fg;
  int bg;
};

/* Structure containing some terminal characteristics */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
struct term_s
{
  int ncolumns;              /* number of columns                      */
  int nlines;                /* number of lines                        */
  int curs_column;           /* current cursor column                  */
  int curs_line;             /* current cursor line                    */
  int colors;                /* number of available colors             */
  int color_method;          /* color method (0=classic (0-7), 1=ANSI) */

  int has_setf;              /* has set_foreground terminfo capability */
  int has_setb;              /* has set_background terminfo capability */
  int has_setaf;             /* idem for set_a_foreground (ANSI)       */
  int has_setab;             /* idem for set_a_background (ANSI)       */
  int has_hpa;               /* has column_address terminfo capability */
};

/* Structure describing a word */
/* """"""""""""""""""""""""""" */
struct word_s
{
  char *str;                 /* display string associated with this word  */
  char *orig;                /* NULL or original string if is had been    *
                              * shortenend for being displayed or altered *
                              * by is expansion.                          */
  int is_last;               /* 1 if the word is the last of a line       */
  int start, end, mbytes;    /* start/end absolute horiz. word positions  *
                              * on the screen                             *
                              * mbytes: number of multibytes to display   */
  int is_selectable;         /* word is is_selectable                     */
  int attention_level;       /* can vary from 0 to 5; 0 meaning normal    */
};

/* Structure describing the window in which the user */
/* will be able to select a word                     */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
struct win_s
{
  int start, end;
  int first_column;
  size_t cur_line;
  int asked_max_lines, max_lines, max_cols;
  int col_sep;
  int tab_mode;
  int col_mode;
  int line_mode;
  int max_width;
  int wide_columns;
  int center;
  int offset;

  color_t bar_color;         /* scrollbar color                  */
  color_t search_color;      /* search mode colors               */
  color_t exclude_color;     /* non-selectable words colors      */
  color_t attention1_color;  /* attention level 1 words colors   */
  color_t attention2_color;  /* attention level 2 words colors   */
  color_t attention3_color;  /* attention level 3 words colors   */
  color_t attention4_color;  /* attention level 4 words colors   */
  color_t attention5_color;  /* attention level 5 words colors   */
};

/* *************************************** */
/* Ternary Search Tree specific structures */
/* *************************************** */

/* Ternary node structure */
/* """""""""""""""""""""" */
struct tst_node_s
{
  wchar_t splitchar;

  tst_node_t *lokid, *eqkid, *hikid;
  void *data;
};

tst_node_t *root;

/* ******************************* */
/* Linked list specific structures */
/* ******************************* */

/* Linked list node structure */
/* """""""""""""""""""""""""" */
struct ll_node_s
{
  void *data;
  struct ll_node_s *next;
};

/* Linked List structure */
/* """"""""""""""""""""" */
struct ll_s
{
  ll_node_t *head;
  ll_node_t *tail;
  size_t len;
};

/* ************** */
/* Help functions */
/* ************** */

/* ====================== */
/* Usage display and exit */
/* ====================== */
void
usage(void)
{
  fprintf(stderr, "Usage: smenu [-h] [-n lines] [-c] [-l] [-s pattern] ");
  fprintf(stderr, "[-m message] [-w] [-d]   \\\n");
  fprintf(stderr, "       [-M] [-t [cols]] [-r] [-b] [-i regex] [-e regex]");
  fprintf(stderr, "                      \\\n");
  fprintf(stderr, "       [-C [a|A|s|S|r|R|d|D]col1[-col2],[col1[-col2]]...] ");
  fprintf(stderr, "                   \\\n");
  fprintf(stderr, "       [-I /regex/repl/[g][v]] ");
  fprintf(stderr, "[-E /regex/repl/[g][v]] ");
  fprintf(stderr, "[-A regex] [-Z regex] \\\n");
  fprintf(stderr, "       [-1 regex] [-2 regex] ... [-5 regex] "
          "[-g] [-W bytes] [-L bytes] [-V]\n");
  fprintf(stderr, "\nThis is a filter that gets words from stdin ");
  fprintf(stderr, "and outputs the\n");
  fprintf(stderr, "selected word (or nothing) on stdout.\n\n");
  fprintf(stderr, "The selection window appears on /dev/tty ");
  fprintf(stderr, "immediately after the current line.\n\n");
  fprintf(stderr, "The following options are available:\n\n");
  fprintf(stderr, "-h displays this help.\n");
  fprintf(stderr, "-n sets the number of lines in the selection window.\n");
  fprintf(stderr, "-t tabulates the items. The number of columns can be "
          "limited with\n");
  fprintf(stderr, "   an optional number.\n");
  fprintf(stderr, "-s sets the initial cursor position (read the manual for "
          "details.\n");
  fprintf(stderr, "-m displays a one-line message above the window\n");
  fprintf(stderr, "-w uses all the terminal width for the columns if "
          "their numbers is given.\n");
  fprintf(stderr, "-d deletes the selection window on exit.\n");
  fprintf(stderr, "-M centers the display if possible.\n");
  fprintf(stderr, "-c is like -t without argument "
          "but respects end of lines.\n");
  fprintf(stderr, "-l is like -c without column alignments.\n");
  fprintf(stderr, "-r enables ENTER to validate the selection even "
          "in search mode.\n");
  fprintf(stderr, "-b displays the non printable characters as space.\n");
  fprintf(stderr, "-i sets the regex input filter to match the "
          "selectable words.\n");
  fprintf(stderr, "-e sets the regex input filter to match the "
          "non-selectable words.\n");
  fprintf(stderr, "-C sets column restrictions for selections.\n");
  fprintf(stderr, "-I sets the post-processing action to apply to "
          "selectable words.\n");
  fprintf(stderr, "-E sets the post-processing action to apply to "
          "non-selectable words.\n");
  fprintf(stderr, "-A forces a class of words to be the first "
          "of the line they appear in.\n");
  fprintf(stderr, "-Z forces a class of words to be the latest "
          "of the line they appear in.\n");
  fprintf(stderr, "-1,-2,...,-5 gives specific colors to up to 5 "
          "classes of selectable words.\n");
  fprintf(stderr, "-g separates columns with '|' in tabulate mode.\n");
  fprintf(stderr, "-q prevents the scrollbar display.\n");
  fprintf(stderr, "-W sets the input words separators.\n");
  fprintf(stderr, "-L sets the input lines separators.\n");
  fprintf(stderr, "-V displays the current version and quits.\n");
  fprintf(stderr, "\nNavigation keys are:\n");
  fprintf(stderr, "  Left/Down/Up/Right arrows or h/j/k/l\n");
  fprintf(stderr, "  Home/End\n");
  fprintf(stderr, "  SPACE to search for the next match of a previously\n");
  fprintf(stderr, "        entered search prefix if any, see below.\n\n");
  fprintf(stderr, "Exit key without output (do nothing): q\n");
  fprintf(stderr, "Selection key                       : ENTER\n");
  fprintf(stderr, "Cancel key                          : ESC\n");
  fprintf(stderr, "Search key                          : / or CTRL-F\n\n");
  fprintf(stderr, "The search key activates a timed search mode in which\n");
  fprintf(stderr, "you can enter the first letters of the searched word.\n");
  fprintf(stderr, "When entering this mode you have 7s to start typing\n");
  fprintf(stderr, "and each entered letter gives you 5 more seconds before\n");
  fprintf(stderr, "the timeout. After that the search mode is ended.\n\n");
  fprintf(stderr, "Notes:\n");
  fprintf(stderr, "- the timer can be cancelled by pressing ESC.\n");
  fprintf(stderr, "- a bad search letter can be removed with ");
  fprintf(stderr, "CTRL-H or Backspace\n\n");
  fprintf(stderr, "(C) Pierre Gentile (2015).\n");

  exit(EXIT_FAILURE);
}

/* ==================== */
/* Help message display */
/* ==================== */
void
help(win_t * win, term_t * term, int last_line)
{
  size_t index;
  int line = 0;
  int len = 0;
  int offset = 0;

  struct entry_s
  {
    char attr;
    char *str;
    int len;
  };

  struct entry_s entries[] = {
    {'r', "HLP", 3},
    {'n', " ", 1},
    {'n', "Move:", 5},
    {'b', "Arrows", 6},
    {'n', "|", 1},
    {'b', "h", 1},
    {'n', "/", 1},
    {'b', "j", 1},
    {'n', "/", 1},
    {'b', "k", 1},
    {'n', "/", 1},
    {'b', "l", 1},
    {'n', ",", 1},
    {'b', "PgUp", 4},
    {'n', "/", 1},
    {'b', "PgDn", 4},
    {'n', "/", 1},
    {'b', "Home", 4},
    {'n', "/", 1},
    {'b', "End", 3},
    {'n', " ", 1},
    {'n', "Cancel:", 7},
    {'b', "q", 1},
    {'n', " ", 1},
    {'n', "Confirm:", 8},
    {'b', "CR", 2},
    {'n', " ", 1},
    {'n', "Search:", 7},
    {'b', "/", 1},
    {'n', "|", 1},
    {'b', "^F", 2},
    {'n', "|", 1},
    {'b', "SP", 2},
    {'n', "|", 1},
    {'b', "n", 1},
    {0, 0, 0}
  };

  tputs(save_cursor, 1, outch);

  if (win->offset > 0)
    if ((offset = win->offset + win->max_width / 2 - 39) > 0)
      printf("%*s", offset, " ");

  for (index = 0; index < sizeof(entries) / sizeof(struct entry_s) - 1; index++)
  {
    if ((len += entries[index].len) >= term->ncolumns - 2)
    {
      line++;

      if (line > last_line || line == win->max_lines)
        break;

      len = entries[index].len;
      fputs("\n", stdout);
    }

    switch (entries[index].attr)
    {
      case 'b':
        tputs(enter_bold_mode, 1, outch);
        break;
      case 'r':
        tputs(enter_reverse_mode, 1, outch);
        break;
      case 'n':
        tputs(exit_attribute_mode, 1, outch);
        break;
    }
    fputs(entries[index].str, stdout);
  }

  tputs(exit_attribute_mode, 1, outch);
  tputs(clr_eol, 1, outch);
  tputs(restore_cursor, 1, outch);
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

/* ================= */
/* Customized malloc */
/* ================= */
void *
xmalloc(size_t size)
{
  void *allocated = malloc(size);

  if (allocated == NULL)
  {
    fprintf(stderr, "Error:  Insufficient memory "
            "(attempt to malloc %lu bytes)\n", (unsigned long int) size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================= */
/* Customized calloc */
/* ================= */
void *
xcalloc(size_t num, size_t size)
{
  void *allocated = calloc(num, size);

  if (allocated == NULL)
  {
    fprintf(stderr, "Error:  Insufficient memory "
            "(attempt to calloc %lu bytes)\n", (unsigned long int) size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================== */
/* Customized realloc */
/* ================== */
void *
xrealloc(void *ptr, size_t size)
{
  void *allocated;

  /* Protect against non-standard behavior */
  /* """"""""""""""""""""""""""""""""""""" */
  if (ptr == NULL)
    allocated = malloc(size);
  else
    allocated = realloc(ptr, size);

  if (allocated == NULL)
  {
    fprintf(stderr, "Error:  Insufficient memory "
            "(attempt to realloc %lu bytes)\n", (unsigned long int) size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ******************** */
/* ini parsing function */
/* ******************** */

/* ============================================================ */
/* Extract a color from the passed string, return 1 on error or */
/* if the color number is tool large                            */
/* ============================================================ */
int
get_ini_color(const char *str, int *v, int max_color)
{
  int rc = 1;

  if (sscanf(str, "%d", v) == 1 && *v >= 0 && *v <= max_color)
    rc = 0;

  return rc;
}

/* ===================================================== */
/* Callback function called when parsing each non-header */
/* line of the ini file.                                 */
/* Returns 0 if OK, 1 if not.                            */
/* ===================================================== */
int
ini_cb(win_t * win, term_t * term, limits_t * limits,
       const char *section, const char *name, const char *value)
{
  int error = 0;
  int has_colors = (term->colors > 7);

  if (strcmp(section, "colors") == 0)
  {
    int v;

#define CHECK_FG(x)                                            \
  else if (strcmp(name, #x "_foreground") == 0)                \
       {                                                       \
         if ((error = get_ini_color(value, &v, term->colors))) \
           goto out;                                           \
         else                                                  \
           win->x ## _color.fg = v;                            \
       }

#define CHECK_BG(x)                                            \
  else if (strcmp(name, #x "_background") == 0)                \
       {                                                       \
         if ((error = get_ini_color(value, &v, term->colors))) \
           goto out;                                           \
         else                                                  \
           win->x ## _color.bg = v;                            \
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
      /* *INDENT-OFF* */
      CHECK_FG(bar)        CHECK_BG(bar)
      CHECK_FG(search)     CHECK_BG(search)
      CHECK_FG(exclude)    CHECK_BG(exclude)
      CHECK_FG(attention1) CHECK_BG(attention1)
      CHECK_FG(attention2) CHECK_BG(attention2)
      CHECK_FG(attention3) CHECK_BG(attention3)
      CHECK_FG(attention4) CHECK_BG(attention4)
      CHECK_FG(attention5) CHECK_BG(attention5)
      /* *INDENT-ON* */
    }
  }
  else if (strcmp(section, "window") == 0)
  {
    int v;

    /* [window] section */
    /* """""""""""""""" */
    if (strcmp(name, "lines") == 0)
    {
      if ((error = !(sscanf(value, "%d", &v) == 1 && v > 0)))
        goto out;
      else
        win->max_lines = v;
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
ini_load(const char *filename, win_t * win, term_t * term, limits_t * limits,
         int (*report) (win_t * win, term_t * term, limits_t * limits,
                        const char *section, const char *name,
                        const char *value))
{
  char name[64];
  char value[256];
  char section[128] = "";
  char *s;
  FILE *f;
  int cnt;
  int error;

  /* If the filename is empty we skip this phase and use the default values */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (filename == NULL)
    return 0;

  /* We do that if the file is not readable as well */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  f = fopen(filename, "r");
  if (!f)
    return 0;

  error = 0;

  /* skip blank lines */
  /* """""""""""""""" */
  while (fscanf(f, "%*[\n]") == 1)
  {
  }

  while (!feof(f))
  {
    if (fscanf(f, "[%127[^];\n]]", section) == 1)
    {
    }
    else if ((cnt = fscanf(f, " %63[^=;\n] = %255[^;\n]", name, value)))
    {
      if (cnt == 1)
        *value = 0;

      for (s = name + strlen(name) - 1; s > name && isspace(*s); s--)
        *s = 0;

      for (s = value + strlen(value) - 1; s > value && isspace(*s); s--)
        *s = 0;

      /* Callback funstion calling */
      /* """"""""""""""""""""""""" */
      error = report(win, term, limits, section, name, value);

      if (error)
        goto out;
    }
    fscanf(f, " ;%*[^\n]");

    /* skip blank lines */
    /* """""""""""""""" */
    while (fscanf(f, "%*[\n]") == 1)
    {
    }
  }

out:
  fclose(f);

  if (error)
    fprintf(stderr, "Invalid entry: %s=%s in %s, exiting.\n",
            name, value, filename);

  return error;
}

/* ======================================================= */
/* Return the full path on the configuration file supposed */
/* to be in the home directory of the user.                */
/* NULL is returned if the built path is too large.        */
/* ======================================================= */
char *
make_ini_path(char *name, char *base)
{
  char *path;
  char *home = getenv(base);
  int path_max = pathconf(".", _PC_PATH_MAX);
  int len = strlen(home) + strlen(name) + 3;
  char *conf;

  if (path_max < 0)
    path_max = 4096;         /* POSIX minimal value */

  if (len <= path_max)
  {
    path = xmalloc(len);
    conf = strrchr(name, '/');
    if (conf != NULL)
      conf++;
    else
      conf = name;

    sprintf(path, "%s/.%s", home, conf);
  }
  else
    path = NULL;

  return path;
}

/* ********************* */
/* Linked List functions */
/* ********************* */

/* ======================== */
/* Create a new linked list */
/* ======================== */
ll_t *
ll_new(void)
{
  ll_t *ret = xmalloc(sizeof(ll_t));

  if (ret)
    ll_init(ret);
  else
    errno = ENOMEM;

  return ret;
}

/* ======================== */
/* Initialize a linked list */
/* ======================== */
void
ll_init(ll_t * list)
{
  list->head = NULL;
  list->tail = NULL;
  list->len = 0;
}

/* ==================================================== */
/* Allocate the space for a new node in the linked list */
/* ==================================================== */
ll_node_t *
ll_new_node(void)
{
  ll_node_t *ret = xmalloc(sizeof(ll_node_t));

  if (!ret)
    errno = ENOMEM;

  return ret;
}

/* ==================================================================== */
/* Append a new node filled with its data at the end of the linked list */
/* The user is responsible for the memory management of the data        */
/* ==================================================================== */
int
ll_append(ll_t * const list, void *const data)
{
  int ret = 1;
  ll_node_t *node;

  if (list)
  {
    node = ll_new_node();
    if (node)
    {
      node->data = data;
      node->next = NULL;

      if (list->tail)
        list->tail->next = node;
      else
        list->head = node;
      list->tail = node;

      ++list->len;
      ret = 0;
    }
  }

  return ret;
}

/* =========================================================================*/
/* Find a node in the list containing data. Return the node pointer or NULL */
/* if not found.                                                            */
/* A comparison function must be provided to compare a and b (strcmp like). */
/* =========================================================================*/
ll_node_t *
ll_find(ll_t * const list, void *const data,
        int (*cmpfunc) (const void *a, const void *b))
{
  ll_node_t *node;

  if (NULL == (node = list->head))
    return NULL;

  do
  {
    if (0 == cmpfunc(node->data, data))
      return node;
  }
  while (NULL != (node = node->next));

  return NULL;
}

/* ***************** */
/* Utility functions */
/* ***************** */

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
  unsigned char c = i & (unsigned char) 0xff;

  return (c >= 0x20 && c < 0x7f) || (c >= (unsigned char) 0xa0);
}

/* ************************* */
/* Terminal utilty functions */
/* ************************* */

/* ===================================================================== */
/* outch is a function version of putchar that can be passed to tputs as */
/* a routine to call.                                                    */
/* ===================================================================== */
int
outch(int c)
{
  putchar(c);
  return (1);
}

/* =============================================== */
/* Set the terminal in non echo/non canonical mode */
/* wait for at least one byte, no timeout.         */
/* =============================================== */
void
setup_term(int const fd)
{
  /* Save the terminal parameters and configure it in raw mode */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tcgetattr(fd, &old_in_attrs);

  new_in_attrs.c_iflag = 0;
  new_in_attrs.c_oflag = old_in_attrs.c_oflag;
  new_in_attrs.c_cflag = old_in_attrs.c_cflag;
  new_in_attrs.c_lflag = old_in_attrs.c_lflag & ~(ICANON | ECHO | ISIG);

  new_in_attrs.c_cc[VMIN] = 1;  /* wait for at lear 1 byte */
  new_in_attrs.c_cc[VTIME] = 0; /* no timeout              */

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
get_terminal_size(int *const r, int *const c)
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
get_cursor_position(int *const r, int *const c)
{
  char buf[32];
  int i = 0;

  /* Report cursor location */
  /* """""""""""""""""""""" */
  if (write(1, "\x1b[6n", 4) != 4)
    return -1;

  /* Read the response: ESC [ rows ; cols R */
  /* """""""""""""""""""""""""""""""""""""" */
  while ((size_t) i < sizeof(buf) - 1)
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

/* *********************************************** */
/* Strings and multibyte strings utility functions */
/* *********************************************** */

/* ====================================================================== */
/* Parse a columns description string                                     */
/* <letter><range1>,<range2>,...                                          */
/* <range> is n1-n2 | n1 where n1 starts with 1                           */
/*                                                                        */
/* <letter> is a|A|s|S|r|R|u|U where                                      */
/* a|A is for Add                                                         */
/* s|S is for Select (same as Add)                                        */
/* r|R is for Remove                                                      */
/* d|D is for Deselect (same as Remove)                                   */
/*                                                                        */
/* unparsed is empty on success and contains the unpaserd part on failure */
/* filter   is a string where each character is                           */
/*          '1' if the column is selected or 'O' if not                   */
/* A maximum of 1024 columns is allowed                                   */
/* ====================================================================== */
void
parse_cols_selector(char *str, char **filter, char *unparsed)
{
  char def;                  /* default value */
  char mark;                 /* Value to set */
  char c;
  int len;                   /* parsed string length */
  int start = 1;             /* column string offset in the parsed string */
  int offset1, offset2;      /* Positions in the parsed strings */
  int n;                     /* number of successfully parsed values */
  int first, second;         /* range starting and ending values */
  char *ptr;                 /* pointer to the remaining string to parse */
  int max;                   /* Max number of columns allowed */

  len = strlen(str);
  max = 1024;
  *filter = xmalloc(max + 1);

  /* Get the first character to see if this is */
  /* an additive or restrictive operation.     */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (sscanf(str, "%c", &c) == 0)
    return;

  switch (c)
  {
    case 'a':
    case 'A':
    case 's':
    case 'S':
      def = '0';
      mark = '1';
      break;

    case 'r':
    case 'R':
    case 'd':
    case 'D':
      def = '1';
      mark = '0';
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
      def = '0';
      mark = '1';
      start = 0;
      break;

    default:
      return;
  }

  ptr = str + start;
  first = second = -1;

  /* Fill the array with the default value */
  /* """"""""""""""""""""""""""""""""""""" */
  memset(*filter, def, max);
  (*filter)[max] = '\0';

  /* Scan the comma separated ranges */
  /* """"""""""""""""""""""""""""""" */
  while ((n = sscanf(ptr, "%d %n-%d %n%*c",
                     &first, &offset1, &second, &offset2)) >= 1)
  {
    int swap;

    if (n == 1)
    {
      /* Only one value could have been read */
      /* """"""""""""""""""""""""""""""""""" */
      if (first < 1 || first > max)
      {
        strcpy(unparsed, ptr);

        return;
      }

      ptr += offset1 + 1;
      (*filter)[first - 1] = mark;
    }
    else if (n == 2)
    {
      /* The two range values have been read */
      /* """"""""""""""""""""""""""""""""""" */
      if (first < 1 || second < 1 || first > max || second > max)
      {
        strcpy(unparsed, ptr);

        return;
      }

      /* swap the values if necessary */
      /* """""""""""""""""""""""""""" */
      if (second < first)
      {
        swap = first;
        first = second;
        second = swap;
      }

      ptr += offset2;
      memset(*filter + first - 1, mark, second - first + 1);
    }
    first = second = -1;
  }

  /* Set the unparsed part of the string */
  /* """"""""""""""""""""""""""""""""""" */
  if (ptr - str <= len)
    strcpy(unparsed, ptr);
  else
    *unparsed = '\0';
}

/* ==================================================== */
/* Parse the sed like sring passed as argument to -I/-E */
/* ==================================================== */
int
parse_replacement_string(char *str, regex_t * re, char **rs,
                         int *global, int *visual)
{
  char sep;
  char *first_sep_pos;
  char *last_sep_pos;
  char *buf;
  int index;
  char c;

  if (strlen(str) < 4)
    return 0;

  /* Get the separator (the 1st character) */
  /* """"""""""""""""""""""""""""""""""""" */
  buf = strdup(str);
  sep = buf[0];

  /* Space like separators are not permited */
  /* """""""""""""""""""""""""""""""""""""" */
  if (isspace(sep))
    goto err;

  /* Get the extended regular expression */
  /* """"""""""""""""""""""""""""""""""" */
  if ((first_sep_pos = strchr(buf + 1, sep)) == NULL)
    goto err;

  *first_sep_pos = '\0';

  /* Get the replacement string */
  /* """""""""""""""""""""""""" */
  if ((last_sep_pos = strchr(first_sep_pos + 1, sep)) == NULL)
    goto err;

  *last_sep_pos = '\0';

  *rs = strdup(first_sep_pos + 1);

  /* Get the global indicator (trailing g) */
  /* and the visual indicator (trailing v) */
  /* """"""""""""""""""""""""""""""""""""" */
  *global = *visual = 0;

  index = 1;
  while ((c = *(last_sep_pos + index)) != '\0')
  {
    if (c == 'g')
      *global = 1;
    else if (c == 'v')
      *visual = 1;
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
  if (regcomp(re, buf + 1, REG_EXTENDED) != 0)
    goto err;

  free(buf);

  return 1;

err:
  free(buf);

  return 0;
}

/* ====================================================================== */
/* Replace the part of a string matched by an extender regular expression */
/* by the replacement string                                              */
/* The regex used must have been previously compiled                      */
/*                                                                        */
/* orig:   original string                                                */
/* subst:  substitute containing \n's                                     */
/* re:     regcomp compiled extended RE                                   */
/* global: 1 if all occurences must be replaced, else 0                   */
/* buf:    destination buffer                                             */
/* bufsiz: destination buffer max size                                    */
/* ====================================================================== */
int
replace(char *orig, char *subst, regex_t * re, int global, char *buf,
        size_t bufsiz)
{
  char *s;
  regmatch_t match[10];      /* substrings from regexec */
  size_t length = strlen(orig);
  int j;

  char *ptr_orig = orig;
  char *ptr_subst;
  int rc = 0;
  int end_loop = 0;

  match[0].rm_so = match[0].rm_eo = 0;

  while (*ptr_orig && strlen(buf) < bufsiz)
  {
    s = buf + strlen(buf);
    ptr_subst = subst;

    if (regexec(re, ptr_orig, 10, match, 0))
      goto end;

    for (j = 0; j < match[0].rm_so; j++)
      *s++ = ptr_orig[j];

    end_loop = 0;
    for (; !end_loop && s < buf + bufsiz; s++)
    {
      switch (*s = *ptr_subst++)
      {
        case '\\':
          switch (*s = *ptr_subst++)
          {
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
              int i;
              regmatch_t *m = &match[*s - '0'];

              if ((i = m->rm_so) >= 0)
              {
                if ((size_t) m->rm_eo > length)
                {
                  /* buggy GNU regexec!! */
                  /* =================== */
                  m->rm_eo = length;
                }

                while (i < m->rm_eo && s < buf + bufsiz)
                  *s++ = ptr_orig[i++];
              }
              s--;
              break;
            }

            case '\0':
              ptr_orig += match[0].rm_eo;
              end_loop = 1;
              rc = 1;
          }
          break;

        case '\0':
          /* end of the substitution string */
          /* """""""""""""""""""""""""""""" */
          ptr_orig += match[0].rm_eo;
          end_loop = 1;
          rc = 1;
      }
    }

    /* Retry the replacement if global == 1 */
    /* """""""""""""""""""""""""""""""""""" */
    if (!global)
      goto end;

    /* In case of empty string matching */
    /* """""""""""""""""""""""""""""""" */
    if (match[0].rm_eo == 0)
      ptr_orig++;

    match[0].rm_so = match[0].rm_eo = 0;
  }

end:

  if (end_loop)
    strcat(buf, ptr_orig);
  else
    strcat(buf, ptr_orig + match[0].rm_eo);

  return rc;
}

/* ============================================================ */
/* Remove all ANSI color codes from s and puts the result in d. */
/* Memory space for d must have been allocated before.          */
/* ============================================================ */
void
strip_ansi_color(char *s, toggle_t * toggle)
{
  char *p = s;
  long len = strlen(s);

  while (*s != '\0')
  {
    /* Remove a sequence of \x1b[...m from s */
    /* """"""""""""""""""""""""""""""""""""" */
    if ((*s == 0x1b) && (*(s + 1) == '['))
    {
      while ((*s != '\0') && (*s++ != 'm'))
      {
        /* do nothing */
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

/* ========================================================= */
/* Decode the number of bytes taken by a character (UTF-8)   */
/* It is the length of the eading sequence of bits set to 1  */
/* (Count Leading Ones)                                      */
/* ========================================================= */
int
count_leading_set_bits(unsigned char c)
{
  if (c >= 0xf0)
    return 4;
  else if (c >= 0xe0)
    return 3;
  else if (c >= 0xc2)
    return 2;
  else
    return 1;
}

/* ============================================================== */
/* Thank you Neil (https://github.com/sheredom)                   */
/* Return 0 if the UTF-8 sequence is valid or the position of the */
/* invalid UTF-8 codepoint on failure.                            */
/* ============================================================== */
void *
validate_mb(const void *str)
{
  const char *s = (const char *) str;

  while ('\0' != *s)
  {
    if (0xf0 == (0xf8 & *s))
    {
      /* Ensure that each of the 3 following bytes in this 4-byte */
      /* UTF-8 codepoint bega, with 0b10xxxxxx                    */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0x80 != (0xc0 & s[1])) || (0x80 != (0xc0 & s[2]))
          || (0x80 != (0xc0 & s[3])))
      {
        return (void *) s;
      }

      /* Ensure that our UTF-8 codepoint ended after 4 bytes */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[4]))
      {
        return (void *) s;
      }

      /* Ensure that the top 5 bits of this 4-byte UTF-8  */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0 == (0x07 & s[0])) && (0 == (0x30 & s[1])))
      {
        return (void *) s;
      }

      /* 4-byte UTF-8 code point (began with 0b11110xxx) */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      s += 4;
    }
    else if (0xe0 == (0xf0 & *s))
    {
      /* Ensure each of the 2 following bytes in this 3-byte */
      /* UTF-8 codepoint began with 0b10xxxxxx               */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0x80 != (0xc0 & s[1])) || (0x80 != (0xc0 & s[2])))
      {
        return (void *) s;
      }

      /* Ensure that our UTF-8 codepoint ended after 3 bytes */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[3]))
      {
        return (void *) s;
      }

      /* Ensure that the top 5 bits of this 3-byte UTF-8  */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0 == (0x0f & s[0])) && (0 == (0x20 & s[1])))
      {
        return (void *) s;
      }

      /* 3-byte UTF-8 code point (began with 0b1110xxxx) */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      s += 3;
    }
    else if (0xc0 == (0xe0 & *s))
    {
      /* Ensure the 1 following byte in this 2-byte */
      /* UTF-8 codepoint began with 0b10xxxxxx      */
      /* """""""""""""""""""""""""""""""""""""""""" */
      if (0x80 != (0xc0 & s[1]))
      {
        return (void *) s;
      }

      /* Ensure that our UTF-8 codepoint ended after 2 bytes */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[2]))
      {
        return (void *) s;
      }

      /* Ensure that the top 4 bits of this 2-byte UTD-8  */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if (0 == (0x1e & s[0]))
      {
        return (void *) s;
      }

      /* 2-byte UTF-8 code point (began with 0b110xxxxx) */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      s += 2;
    }
    else if (0x00 == (0x80 & *s))
    {
      /* 1-byte ascii (began with 0b0xxxxxxx) */
      /* """""""""""""""""""""""""""""""""""" */
      s += 1;
    }
    else
    {
      /* We have an invalid 0b1xxxxxxx UTF-8 code point entry */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      return (void *) s;
    }
  }

  return 0;
}

/* =============================================== */
/* Is the string str2 a prefix of the string str1? */
/* =============================================== */
int
strprefix(char *str1, char *str2)
{
  while (*str1 != '\0' && *str1 == *str2)
  {
    str1++;
    str2++;
  }

  return *str2 == '\0';
}

/* ====================== */
/* Multibyte UTF-8 strlen */
/* ====================== */
int
mb_strlen(char *str)
{
  int i = 0, j = 0;

  while (str[i])
  {
    if ((str[i] & 0xc0) != 0x80)
      j++;
    i++;
  }
  return j;
}

/* ====================================================================== */
/* Multibytes extraction of the prefix of n multibyte chars from a string */
/* The destination string d must have been allocated before.              */
/* pos is updated to reflect the postion AFTER the prefix.                */
/* ====================================================================== */
char *
mb_strprefix(char *d, char *s, int n, int *pos)
{
  int i = 0;
  int j = 0;

  *pos = 0;

  while (s[i] && j < n)
  {
    d[i] = s[i];
    i++;
    j++;
    while (s[i] && (s[i] & 0xC0) == 0x80)
    {
      d[i] = s[i];
      i++;
    }
  }

  *pos = i;

  d[i] = '\0';

  return d;
}

/* =========================================================== */
/* Convert a multibyte (UTF-8) char string to a wchar_t string */
/* =========================================================== */
wchar_t *
mb_strtowcs(char *s)
{
  int converted = 0;
  unsigned char *ch;
  wchar_t *wptr, *w;
  int size;

  size = strlen(s);
  w = xmalloc((size + 1) * sizeof(wchar_t));
  w[0] = L'\0';

  wptr = w;
  for (ch = (unsigned char *) s; *ch; ch += converted)
  {
    if ((converted = mbtowc(wptr, (char *) ch, 4)) > 0)
      wptr++;
    else
    {
      *wptr++ = (wchar_t) * ch;
      converted = 1;
    }
  }

  *wptr = L'\0';

  return w;
}

/* *************************************************************** */
/* Ternary Search Tree functions                                   */
/* Inspired by: https://www.cs.princeton.edu/~rs/strings/tstdemo.c */
/* *************************************************************** */

/* ====================================== */
/* Ternary search tree insertion function */
/* ====================================== */
tst_node_t *
tst_insert(tst_node_t * p, wchar_t * w, void *data)
{
  if (p == NULL)
  {
    p = (tst_node_t *) xmalloc(sizeof(struct tst_node_s));
    p->splitchar = *w;
    p->lokid = p->eqkid = p->hikid = NULL;
    p->data = NULL;
  }

  if (*w < p->splitchar)
    p->lokid = tst_insert(p->lokid, w, data);
  else if (*w == p->splitchar)
  {
    if (*w == L'\0')
    {
      p->data = data;
      p->eqkid = NULL;
    }
    else
      p->eqkid = tst_insert(p->eqkid, ++w, data);
  }
  else
    p->hikid = tst_insert(p->hikid, w, data);

  return (p);
}

/* ====================================== */
/* Ternary search tree insertion function */
/* user data aree not cleaned             */
/* ====================================== */
void
tst_cleanup(tst_node_t * p)
{
  if (p)
  {
    tst_cleanup(p->lokid);
    if (p->splitchar)
      tst_cleanup(p->eqkid);
    tst_cleanup(p->hikid);
    free(p);
  }
}

/* ========================================================== */
/* Recurvive traversal of a ternary tree. A callback function */
/* is also called when a complete string is found             */
/* returns 1 if the callback function succeed (returned 1) at */
/* least once                                                 */
/* the first_call argument is for initializing the static     */
/* variable                                                   */
/* ========================================================== */
int
tst_traverse(tst_node_t * p, int (*callback) (void *), int first_call)
{
  static int rc;

  if (first_call)
    rc = 0;

  if (!p)
    return 0;
  tst_traverse(p->lokid, callback, 0);
  if (p->splitchar != L'\0')
    tst_traverse(p->eqkid, callback, 0);
  else
    rc += (*callback) (p->data);
  tst_traverse(p->hikid, callback, 0);

  return !!rc;
}

/* ============================================ */
/* search a complete string in the Ternary tree */
/* ============================================ */
void *
tst_search(tst_node_t * root, wchar_t * w)
{
  tst_node_t *p;

  p = root;

  while (p)
  {
    if (*w < p->splitchar)
      p = p->lokid;
    else if (*w == p->splitchar)
    {
      if (*w++ == L'\0')
        return p->data;
      p = p->eqkid;
    }
    else
      p = p->hikid;
  }

  return NULL;
}

/* =============================================================== */
/* search all srings begining with the same prefix                 */
/* the callback function will be applied to each of theses strings */
/* returns NULL if no string matched the prefix                    */
/* =============================================================== */
void *
tst_prefix_search(tst_node_t * root, wchar_t * w, int (*callback) (void *))
{
  tst_node_t *p = root;
  int len = wcslen(w);
  int rc;

  while (p)
  {
    if (*w < p->splitchar)
      p = p->lokid;
    else if (*w == p->splitchar)
    {
      len--;
      if (*w++ == L'\0')
        return p->data;
      if (len == 0)
      {
        rc = tst_traverse(p->eqkid, callback, 1);
        return (void *) (long) rc;
      }
      p = p->eqkid;
    }
    else
      p = p->hikid;
  }

  return NULL;
}

/* ====================================================================== */
/* Callback function used by tst_traverse                                 */
/* Iterate the linked list attached to the string containing the index of */
/* the words in the input flow. each page number is then used to detemine */
/* the lower page greater than the cursor position                        */
/* ---------------------------------------------------------------------- */
/* Require new_current to be set to count-1 at start                      */
/* Update new_current to the smallest greater position than current       */
/* ====================================================================== */
int
tst_cb(void *elem)
{
  size_t n = 0;
  int rc = 0;

  /* The data attached to the string in the tst is a linked list of position */
  /* of the string in the the input flow, This list is naturally sorted      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ll_t *list = (ll_t *) elem;

  ll_node_t *node = list->head;

  while (n++ < list->len)
  {
    int pos;

    pos = *(int *) (node->data);

    /* We already are at the last word, report the finding */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos == count - 1)
      return 1;

    /* Only consider the indexes above the current cursor position */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos > current)
    {
      /* As the future new current index has been set to the hihgest possible */
      /* value, each new possible position can only improve the estimation    */
      /* we se rc to 1 to mark that                                           */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (pos < new_current)
      {
        new_current = pos;
        rc = 1;
      }
    }

    node = node->next;
  }
  return rc;
}

/* ====================================================================== */
/* Callback function used by tst_traverse                                 */
/* Iterate the linked list attached to the string containing the index of */
/* the words in the input flow. each page number is then used to detemine */
/* the lower page greater than the cursor position                        */
/* ---------------------------------------------------------------------- */
/* This is a special version of tst_cb wich permit to find the first word */
/* ---------------------------------------------------------------------- */
/* Require new_current to be set to count - 1 at start                    */
/* Update new_current to the smallest greater position than current       */
/* ====================================================================== */
int
tst_cb_cli(void *elem)
{
  size_t n = 0;
  int rc = 0;

  /* The data attached to the string in the tst is a linked list of position */
  /* of the string in the the input flow, This list is naturally sorted      */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ll_t *list = (ll_t *) elem;

  ll_node_t *node = list->head;

  while (n++ < list->len)
  {
    int pos;

    pos = *(int *) (node->data);

    /* We already are at the last word, report the finding */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos == count - 1)
      return 1;

    /* Only consider the indexes above the current cursor position */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (pos >= current)      /* Enable the search of the current word */
    {
      /* As the future new current index has been set to the hihgest possible */
      /* value, each new possible position can only improve the estimation    */
      /* we se rc to 1 to mark that                                           */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (pos < new_current)
      {
        new_current = pos;
        rc = 1;
      }
    }

    node = node->next;
  }
  return rc;
}

/* *************** */
/* Input functions */
/* *************** */

/* ================================================ */
/* Non delay reading of a scancode.                 */
/* Update a scancodes buffer aand return its length */
/* in bytes.                                        */
/* ================================================ */
int
get_scancode(unsigned char *s, int max)
{
  int c;
  int i = 1;
  struct termios original_ts, nowait_ts;

  if ((c = getchar()) == EOF)
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
    nowait_ts.c_cc[VMIN] = 0;
    nowait_ts.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &nowait_ts);

    /* Check if additional code is available after 0x1b */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if ((c = getchar()) != EOF)
    {
      s[1] = c;

      i = 2;
      while (i < max && (c = getchar()) != EOF)
        s[i++] = c;
    }
    else
    {
      /* There isn't a new code, this mean 0x1b came from ESC key pression */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    }

    /* Restore the save terminal parameters */
    /* """""""""""""""""""""""""""""""""""" */
    tcsetattr(0, TCSADRAIN, &original_ts);
  }

  return i;
}

/* ===================================================================== */
/* Get bytes from stdin. If the first byte is the leading character of a */
/* multibyte one, the following ones a also read                         */
/* Th count_leading_set_bits function is used to obten the number of     */
/* bytes of the character                                                */
/* ===================================================================== */
int
get_bytes(FILE * input, char *mb_buffer, ll_t * word_delims_list,
          toggle_t * toggle, langinfo_t * langinfo)
{
  int byte;
  int last = 0;
  int n;

  /* read the first byte */
  /* """"""""""""""""""" */
  byte = mb_buffer[last++] = fgetc(input);

  if (byte == EOF)
    return EOF;

  /* Check if we need to read more bytes to form a sequence */
  /* and put the number of bytes of the sequence in last.   */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo->utf8 && ((n = count_leading_set_bits(byte)) > 1))
  {
    while (last < n && (byte = fgetc(input)) != EOF && (byte & 0xc0) == 0x80)
      mb_buffer[last++] = byte;

    if (byte == EOF)
      return EOF;
  }

  /* Convert a well known byte (if alone in) into */
  /* its canonical representation or into a dot   */
  /* when not printable.                          */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (last == 1 && toggle->blank_nonprintable && byte != 0x1b && byte != EOF
      && !my_isprint(byte)
      && ll_find(word_delims_list, mb_buffer, delims_cmp) == NULL)
  {
    mb_buffer[0] = ' ';
    mb_buffer[1] = '\0';
  }
  else
    mb_buffer[last] = '\0';

  /* Replace an invalid UTF-8 byte sequence by a single dot.  */
  /* In this case the original sequence is lost (unsupported  */
  /* encoding).                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo->utf8 && validate_mb(mb_buffer) != 0)
  {
    byte = mb_buffer[0] = '.';
    mb_buffer[1] = '\0';
  }

  return byte;
}

/* ==========================================================================*/
/* Expand the string str by replacing all its embedded special characters by */
/* their corresponding escape sequence                                       */
/* dest must be long enough to contain the expanded string                   */
/* ==========================================================================*/
int
expand(char *src, char *dest, langinfo_t * langinfo)
{
  char c;
  int n;
  int all_spaces = 1;
  char *ptr = dest;
  size_t len = 0;

  while ((c = *(src++)))
  {
    if (c != ' ')
      all_spaces = 0;

    /* UTF-8 codepoints take more than on character */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = count_leading_set_bits(c)) > 1)
    {
      if (langinfo->utf8)
        /* If the locale is UTF-8 aware, copy src into ptr. */
        /* """""""""""""""""""""""""""""""""""""""""""""""" */
        do
        {
          *(ptr++) = c;
          len++;
        }
        while (--n && (c = *(src++)));
      else
      {
        /* If not, ignore the bytes composing the multibyte */
        /* character and replace them with a single '.'.    */
        /* """""""""""""""""""""""""""""""""""""""""""""""" */
        do
        {
          /* skip this byte. */
        }
        while (--n && ('\0' != *(src++)));

        *(ptr++) = '.';
        len++;
      }
    }
    else
      /* This is not a multibyte character */
      /* """"""""""""""""""""""""""""""""" */
      switch (c)
      {
        case '\a':
          *(ptr++) = '\\';
          *(ptr++) = 'a';
          len += 2;
          break;
        case '\b':
          *(ptr++) = '\\';
          *(ptr++) = 'b';
          len += 2;
          break;
        case '\t':
          *(ptr++) = '\\';
          *(ptr++) = 't';
          len += 2;
          break;
        case '\n':
          *(ptr++) = '\\';
          *(ptr++) = 'n';
          len += 2;
          break;
        case '\v':
          *(ptr++) = '\\';
          *(ptr++) = 'v';
          len += 2;
          break;
        case '\f':
          *(ptr++) = '\\';
          *(ptr++) = 'f';
          len += 2;
          break;
        case '\r':
          *(ptr++) = '\\';
          *(ptr++) = 'r';
          len += 2;
          break;
        case '\\':
          *(ptr++) = '\\';
          *(ptr++) = '\\';
          len += 2;
          break;
        default:
          if (my_isprint(c))
            *(ptr++) = c;
          else
            *(ptr++) = '.';
          len++;
      }
  }

  /* If the word contains only spaces, replace them */
  /* by underscores so that it can be seen          */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (all_spaces)
    memset(dest, '_', len);

  *ptr = '\0';               /* Ensure that dest has a nul terminator */

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
         char *mb_buffer, int *is_last, toggle_t * toggle,
         langinfo_t * langinfo, win_t * win, limits_t * limits)
{
  char *temp = NULL;
  int byte, count = 0;       /* count chars used in current allocation */
  int wordsize;              /* size of current allocation in chars    */
  int is_dquote;             /* double quote presence indicator        */
  int is_squote;             /* single quote presence indicator        */
  int is_special;            /* a character is special after a \       */

  /* Skip leading delimiters */
  /* """"""""""""""""""""""" */
  byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);

  while (byte == EOF
         || ll_find(word_delims_list, mb_buffer, delims_cmp) != NULL)
  {
    if (byte == EOF)
      return NULL;

    byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);
  }

  /* allocate initial word storage space */
  /* """"""""""""""""""""""""""""""""""" */
  temp = xmalloc(wordsize = CHARSCHUNK);

  /* Start stashing bytes. stop when we meet a non delimiter or EOF */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  count = 0;
  is_dquote = 0;
  is_squote = 0;
  is_special = 0;

  while (byte != EOF)
  {
    int i = 0;

    if (count >= limits->word_length)
    {
      fprintf(stderr, "A word's length has reached the limit "
              "(%d), exiting.\n", limits->word_length);

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
          mb_buffer[0] = byte = '\a';
          mb_buffer[1] = '\0';
          break;

        case 'b':
          mb_buffer[0] = byte = '\b';
          mb_buffer[1] = '\0';
          break;

        case 't':
          mb_buffer[0] = byte = '\t';
          mb_buffer[1] = '\0';
          break;

        case 'n':
          mb_buffer[0] = byte = '\n';
          mb_buffer[1] = '\0';
          break;

        case 'v':
          mb_buffer[0] = byte = '\v';
          mb_buffer[1] = '\0';
          break;

        case 'f':
          mb_buffer[0] = byte = '\f';
          mb_buffer[1] = '\0';
          break;

        case 'r':
          mb_buffer[0] = byte = '\r';
          mb_buffer[1] = '\0';
          break;

        case '\\':
          mb_buffer[0] = byte = '\\';
          mb_buffer[1] = '\0';
          break;
      }
    else
    {
      /* manage double quotes */
      /* """""""""""""""""""" */
      if (byte == '"' && !is_squote)
        is_dquote = !is_dquote;

      /* manage single quotes */
      /* """""""""""""""""""" */
      if (byte == '\'' && !is_dquote)
        is_squote = !is_squote;
    }

    /* Only consider delimiters when outside quotations */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!is_dquote && !is_squote)
        && ll_find(word_delims_list, mb_buffer, delims_cmp) != NULL)
      break;

    /* We no dot count the meaningfull quotes */
    /* """""""""""""""""""""""""""""""""""""" */
    if (!is_special
        && ((byte == '"' && !is_squote) || (byte == '\'' && !is_dquote)))
    {
      is_special = 0;
      goto next;
    }

    /* Feed temp with the content of mb_buffer */
    /* """"""""""""""""""""""""""""""""""""""" */
    while (mb_buffer[i] != '\0')
    {
      if (count >= wordsize - 1)
        temp = xrealloc(temp,
                        wordsize += (count / CHARSCHUNK + 1) * CHARSCHUNK);

      *(temp + count++) = mb_buffer[i];
      i++;
    }

    is_special = 0;

  next:
    byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);
  }

  /* Nul-terminate the word to make it a string */
  /* """""""""""""""""""""""""""""""""""""""""" */
  *(temp + count) = '\0';

  /* Skip all field delimiters before a record delimiter */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ll_find(record_delims_list, mb_buffer, delims_cmp) == NULL)
  {
    byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);
    while (ll_find(word_delims_list, mb_buffer, delims_cmp) != NULL
           && ll_find(record_delims_list, mb_buffer, delims_cmp) == NULL)
      byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);

    if (langinfo->utf8 && count_leading_set_bits(mb_buffer[0]) > 1)
    {
      size_t pos;

      /* FIXME We cannot assume we can ungetc more than one byte          */
      /* This seems to work with Linux/glibc though, at least for 4 bytes */

      pos = strlen(mb_buffer);
      while (pos > 0)
        ungetc(mb_buffer[--pos], stdin);

      /* FIXME ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
    }
    else
      ungetc(byte, stdin);
  }

  /* Mark it as the last word of a record if its sequence matches a */
  /* record delimiter except in tab mode                            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (byte == EOF
      || ((win->col_mode | win->line_mode)
          && ll_find(record_delims_list, mb_buffer, delims_cmp) != NULL))
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
set_foreground_color(term_t * term, int color)
{
  if (term->color_method == 0 && term->has_setf)
    tputs(tparm(set_foreground, color), 1, outch);
  else if (term->color_method == 1 && term->has_setaf)
    tputs(tparm(set_a_foreground, color), 1, outch);
}

/* ========================================================= */
/* Set a background color according to terminal capabilities */
/* ========================================================= */
void
set_background_color(term_t * term, int color)
{
  if (term->color_method == 0 && term->has_setb)
    tputs(tparm(set_background, color), 1, outch);
  else if (term->color_method == 1 && term->has_setab)
    tputs(tparm(set_a_background, color), 1, outch);
}

/* ====================================================== */
/* Put a scrolling symbol at the first column of the line */
/* ====================================================== */
void
left_margin_putp(char *s, term_t * term, win_t * win)
{
  tputs(enter_bold_mode, 1, outch);

  if (win->bar_color.fg >= 0)
    set_foreground_color(term, win->bar_color.fg);

  if (win->bar_color.bg >= 0)
    set_background_color(term, win->bar_color.bg);

  /* We won't print this symbol when not in column mode */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  if (*s != '\0')
    fputs(s, stdout);

  tputs(exit_attribute_mode, 1, outch);
}

/* ===================================================== */
/* Put a scrolling symbol at the last column of the line */
/* ===================================================== */
void
right_margin_putp(char *s1, char *s2, langinfo_t * langinfo,
                  term_t * term, win_t * win, int line, int offset)
{
  tputs(enter_bold_mode, 1, outch);

  if (win->bar_color.fg >= 0)
    set_foreground_color(term, win->bar_color.fg);

  if (win->bar_color.bg >= 0)
    set_background_color(term, win->bar_color.bg);

  if (term->has_hpa)
    tputs(tparm(column_address, offset + win->max_width + 1), 1, outch);
  else
    tputs(tparm(cursor_address,
                term->curs_line + line - 2, offset + win->max_width + 1), 1,
          outch);

  if (langinfo->utf8)
    fputs(s1, stdout);
  else
    fputs(s2, stdout);

  tputs(exit_attribute_mode, 1, outch);
}

/* ************** */
/* Core functions */
/* ************** */

/* ============================================================== */
/* Split the lines of the message given to -m to a linked list of */
/* multibytes lines.                                              */
/* Also fill the maximum screen width and the maximum number      */
/* of bytes of the longest line                                   */
/* ============================================================== */
void
get_message_lines(char *message, ll_t * message_lines_list,
                  int *message_max_width, int *message_max_len)
{
  char *str;
  char *ptr;
  char *cr_ptr;
  int n;
  wchar_t *w = NULL;

  *message_max_width = 0;
  *message_max_len = 0;
  ptr = message;

  /* For each line terminated with a EOL character */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  while (*ptr != 0 && (cr_ptr = strchr(ptr, '\n')) != NULL)
  {
    if (cr_ptr > ptr)
    {
      str = xmalloc(cr_ptr - ptr + 1);
      str[cr_ptr - ptr] = '\0';
      memcpy(str, ptr, cr_ptr - ptr);
    }
    else
      str = strdup("");

    ll_append(message_lines_list, str);

    /* If needed, update the message maximum width */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    n = wcswidth((w = mb_strtowcs(str)), mb_strlen(str));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line            */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = strlen(str)) > *message_max_len)
      *message_max_len = n;

    ptr = cr_ptr + 1;
  }

  /* For the last line */
  /* """"""""""""""""" */
  if (*ptr != '\0')
  {
    ll_append(message_lines_list, strdup(ptr));

    n = wcswidth((w = mb_strtowcs(ptr)), mb_strlen(ptr));
    free(w);

    if (n > *message_max_width)
      *message_max_width = n;

    /* If needed, update the message maximum number */
    /* of bytes used by the longest line            */
    /* """""""""""""""""""""""""""""""""""""""""""" */
    if ((n = strlen(ptr)) > *message_max_len)
      *message_max_len = n;
  }
  else
    ll_append(message_lines_list, strdup(""));
}

/* ========================================================================= */
/* Set the metadata associated with a word, its starting and ending position */
/* the line in which it is put and so on.                                    */
/* Set win.start win.end and the starting and ending position of each word.  */
/* This fonction is only called initially, when resizing the terminal and    */
/* potentially when the seah function is used.                               */
/* ========================================================================= */
int
build_metadata(word_t * word_a, term_t * term, int count, win_t * win)
{
  int i = 0;
  size_t word_len;
  int len = 0;
  int last = 0;
  int cur_line;
  int end_line;
  int word_width;
  int tab_count;             /* Current number of words in the line, *
                              * used in tab_mode                     */
  wchar_t *w;

  line_nb_of_word_a[0] = 0;
  first_word_in_line_a[0] = 0;
  win->max_width = 0;

  /* Modify the max number of displayed lines if we do not have */
  /* enough place                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->max_lines > term->nlines - 1)
    win->max_lines = term->nlines - 1;

  tab_count = 0;
  while (i < count)
  {
    /* Determine the number of screen positions used by the word */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len = mbstowcs(0, word_a[i].str, 0);
    word_width = wcswidth((w = mb_strtowcs(word_a[i].str)), word_len);

    /* Manage the case where the word is larger than the terminal width */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_width >= term->ncolumns - 2)
    {
      /* Shorten the word until it fits */
      /* """""""""""""""""""""""""""""" */
      do
      {
        word_width = wcswidth(w, word_len--);
      }
      while (word_len > 0 && word_width >= term->ncolumns - 2);
    }
    free(w);

    /* Look if there is enough remaining place on the line when not in column */
    /* mode. Force a break if the 'is_last' flag is set in all modes or if we */
    /* hit the max number of allowed columns in tab mode                      */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!win->col_mode && !win->line_mode
         && (len + word_width + 1) >= term->ncolumns - 1)
        || ((win->col_mode | win->line_mode | win->tab_mode)
            && i > 0 && word_a[i - 1].is_last)
        || (win->tab_mode && win->max_cols > 0 && tab_count >= win->max_cols))
    {

      /* We must build another line */
      /* """""""""""""""""""""""""" */
      line_nb_of_word_a[i] = ++last;
      first_word_in_line_a[last] = i;

      word_a[i].start = 0;

      len = word_width + 1;  /* Resets the current line length     */
      tab_count = 1;         /* Resets the current number of words *
                              * in the line                        */
      word_a[i].end = word_width - 1;
      word_a[i].mbytes = word_len + 1;
    }
    else
    {
      word_a[i].start = len;
      word_a[i].end = word_a[i].start + word_width - 1;
      word_a[i].mbytes = word_len + 1;

      line_nb_of_word_a[i] = last;

      len += word_width + 1; /* Increase line length */
      tab_count++;           /* We've seen another word in the line */
    }

    if (len > win->max_width)
      win->max_width = len;

    i++;
  }

  if (!win->center || win->max_width > term->ncolumns - 2)
    win->offset = 0;
  else
    win->offset = (term->ncolumns - 2 - win->max_width) / 2;

  /* We need to recalculate win->start and win->end here */
  /* because of a possible terminal resizing             */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  cur_line = line_nb_of_word_a[current];
  if (cur_line == last)
    win->end = count - 1;
  else
    win->end = first_word_in_line_a[cur_line + 1] - 1;
  end_line = line_nb_of_word_a[win->end];

  if (end_line < win->max_lines)
    win->start = 0;
  else
    win->start = first_word_in_line_a[end_line - win->max_lines + 1];

  return last;
}

/* ====================================================================== */
/* Display a word in, the windows. Manages the following different cases: */
/* - Search mode display                                                  */
/* - Cursor display                                                       */
/* - Normal display                                                       */
/* - Color or mono display                                                */
/* ====================================================================== */
void
disp_word(word_t * word_a, int pos, int search_mode, char *buffer,
          term_t * term, win_t * win, char *tmp_max_word)
{
  int s = word_a[pos].start;
  int e = word_a[pos].end;
  int p;
  wchar_t *w;

  if (pos == current)
  {
    if (search_mode)
    {
      size_t word_len;

      /* Prevent buffer to become larger than the word */
      /* to be displayed                               */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      word_len = strlen(word_a[pos].str);

      if (strlen(buffer) >= word_len)
        buffer[word_len] = '\0';

      /* Set the search cursor attribute */
      /* """"""""""""""""""""""""""""""" */
      if (win->search_color.bg >= 0 && term->colors > 7)
        set_background_color(term, win->search_color.bg);
      else
      {
        tputs(enter_underline_mode, 1, outch);
        tputs(enter_reverse_mode, 1, outch);
      }

      mb_strprefix(tmp_max_word, word_a[pos].str, word_a[pos].mbytes - 1, &p);
      fputs(tmp_max_word, stdout);

      /* Overwrite the beginning of the word with the search buffer */
      /* content if it is not empty                                 */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (buffer[0] != '\0')
      {
        int i = 0;

        int buf_width;

        /* Calculate the space taken by the buffer on screen */
        /* """"""""""""""""""""""""""""""""""""""""""""""""" */
        buf_width = wcswidth((w = mb_strtowcs(buffer)), mbstowcs(0, buffer, 0));
        free(w);

        /* Put the cursor at the beginning of the word */
        /* """"""""""""""""""""""""""""""""""""""""""" */
        for (i = 0; i < e - s + 1; i++)
          tputs(cursor_left, 1, outch);

        /* Set the buffer display attribute */
        /* """""""""""""""""""""""""""""""" */
        if (win->search_color.fg >= 0 && term->colors > 7)
          set_foreground_color(term, win->search_color.fg);
        else
          tputs(enter_bold_mode, 1, outch);

        fputs(buffer, stdout);

        /* Put back the cursor after the word */
        /* """""""""""""""""""""""""""""""""" */
        for (i = 0; i < e - s - buf_width + 1; i++)
          tputs(cursor_right, 1, outch);
      }
    }
    else
    {
      /* If we are not in search mode, display in the cursor in reverse video */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      tputs(enter_reverse_mode, 1, outch);
      mb_strprefix(tmp_max_word, word_a[pos].str, word_a[pos].mbytes - 1, &p);
      fputs(tmp_max_word, stdout);
    }
    tputs(exit_attribute_mode, 1, outch);
  }
  else
  {
    /* Display a normal word without any attribute */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    mb_strprefix(tmp_max_word, word_a[pos].str, word_a[pos].mbytes - 1, &p);

    if (!word_a[pos].is_selectable)
    {
      if (win->exclude_color.fg >= 0)
        set_foreground_color(term, win->exclude_color.fg);
      if (win->exclude_color.bg >= 0)
        set_background_color(term, win->exclude_color.bg);
    }
    else if (word_a[pos].attention_level > 0)
    {
      switch (word_a[pos].attention_level)
      {
        case 1:
          if (win->attention1_color.fg >= 0)
            set_foreground_color(term, win->attention1_color.fg);
          if (win->attention1_color.bg >= 0)
            set_background_color(term, win->attention1_color.bg);
          break;
        case 2:
          if (win->attention2_color.fg >= 0)
            set_foreground_color(term, win->attention2_color.fg);
          if (win->attention2_color.bg >= 0)
            set_background_color(term, win->attention2_color.bg);
          break;
        case 3:
          if (win->attention3_color.fg >= 0)
            set_foreground_color(term, win->attention3_color.fg);
          if (win->attention3_color.bg >= 0)
            set_background_color(term, win->attention3_color.bg);
          break;
        case 4:
          if (win->attention4_color.fg >= 0)
            set_foreground_color(term, win->attention4_color.fg);
          if (win->attention4_color.bg >= 0)
            set_background_color(term, win->attention4_color.bg);
          break;
        case 5:
          if (win->attention5_color.fg >= 0)
            set_foreground_color(term, win->attention5_color.fg);
          if (win->attention5_color.bg >= 0)
            set_background_color(term, win->attention5_color.bg);
          break;
      }
    }
    fputs(tmp_max_word, stdout);
    tputs(exit_attribute_mode, 1, outch);
  }
}

/* ======================================= */
/* Display a message line above the window */
/* ======================================= */
int
disp_message(ll_t * message_lines_list, int message_max_width,
             int message_max_len, term_t * term, win_t * win)
{
  ll_node_t *node;
  char *line;
  char *buf;
  int len;
  int size;
  int message_lines = 0;
  int offset;
  wchar_t *w;

  /* Do nothing if thre is no message to display */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  if (message_lines_list == NULL)
    return 0;

  node = message_lines_list->head;
  buf = xmalloc(message_max_len + 1);

  tputs(enter_bold_mode, 1, outch);

  /* Follow the message lines list and display eachh line */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  while (node != NULL)
  {
    line = node->data;
    len = mb_strlen(line);
    w = mb_strtowcs(line);

    size = wcswidth(w, len);
    while (len > 0 && size > term->ncolumns)
      size = wcswidth(w, --len);

    free(w);

    /* Compute the offset from the left screen border if -M option is set */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    offset = (term->ncolumns - message_max_width) / 2;

    if (win->offset > 0 && offset > 0)
      printf("%*s", offset, "");

    /* Only print the start of a line if the screen width if too small */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    mb_strprefix(buf, line, len, &size);
    printf("%s\n", buf);

    node = node->next;
    message_lines++;
  }

  tputs(exit_attribute_mode, 1, outch);

  free(buf);

  return message_lines;
}

/* ============================ */
/* Display the selection window */
/* ============================ */
int
disp_lines(word_t * word_a, win_t * win, toggle_t * toggle, int current,
           int count, int search_mode, char *search_buf, term_t * term,
           int last_line, char *tmp_max_word, langinfo_t * langinfo)
{
  int lines_disp;
  int i;
  char scroll_symbol[5];
  int len;

  scroll_symbol[0] = ' ';
  scroll_symbol[1] = '\0';

  lines_disp = 1;

  tputs(save_cursor, 1, outch);

  i = win->start;

  if (win->col_mode | win->line_mode)
    len = term->ncolumns - 3;
  else
    len = term->ncolumns - 2;

  /* If in column mode and the sum of the columns sizes + gutters is      */
  /* greater than the terminal width,  then prepend a space to be able to */
  /* display the left arrow indicating that the first displayed column    */
  /* is not the first one.                                                */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (len > 1 && ((win->col_mode | win->line_mode)
                  && win->max_width > term->ncolumns - 2))
  {
    if (win->first_column > 0)
    {
      if (langinfo->utf8)
        strcpy(scroll_symbol, sbar_arr_left);
      else
        strcpy(scroll_symbol, "<");
    }
  }
  else
    scroll_symbol[0] = '\0';

  /* Center the display ? */
  /* """""""""""""""""""" */
  if (win->offset > 0)
    printf("%*s", win->offset, " ");

  left_margin_putp(scroll_symbol, term, win);
  while (len > 1 && i <= count - 1)
  {
    /* Display one word and the space ou symbol following it */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[i].start >= win->first_column
        && word_a[i].end < len + win->first_column)
    {
      disp_word(word_a, i, search_mode, search_buf, term, win, tmp_max_word);

      /* If there are more element to be displayed after the right margin */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((win->col_mode | win->line_mode) && i < count - 1
          && word_a[i + 1].end >= len + win->first_column)
      {
        tputs(enter_bold_mode, 1, outch);

        if (win->bar_color.fg >= 0)
          set_foreground_color(term, win->bar_color.fg);

        if (win->bar_color.bg >= 0)
          set_background_color(term, win->bar_color.bg);

        if (langinfo->utf8)
          fputs(sbar_arr_right, stdout);
        else
          fputs(">", stdout);

        tputs(exit_attribute_mode, 1, outch);
      }

      /* If we want to display the gutter */
      /* """""""""""""""""""""""""""""""" */
      else if (!word_a[i].is_last && win->col_sep && win->tab_mode)
      {
        if (langinfo->utf8)
          fputs(sbar_broken_line, stdout);
        else
          fputs("|", stdout);
      }
      /* Else just display a space */
      /* """"""""""""""""""""""""" */
      else
        fputs(" ", stdout);
    }

    /* Mark the line as the current line, the line containing the cursor */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (i == current)
      win->cur_line = lines_disp;

    /* Check if we must start a new line */
    /* """"""""""""""""""""""""""""""""" */
    if (i == count - 1 || (i < count - 1 && word_a[i + 1].start == 0))
    {
      tputs(clr_eol, 1, outch);
      if (lines_disp < win->max_lines)
      {
        /* If we have more than one line to display */
        /* """""""""""""""""""""""""""""""""""""""" */
        if (!toggle->no_scrollbar && (lines_disp > 1 || i < count - 1))
        {
          /* Display the next element of the scrollbar */
          /* """"""""""""""""""""""""""""""""""""""""" */
          if (line_nb_of_word_a[i] == 0)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_top, "\\", langinfo, term, win,
                                lines_disp, win->offset);
            else
              right_margin_putp(sbar_arr_down, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
          else if (lines_disp == 1)
            right_margin_putp(sbar_arr_up, "^", langinfo, term, win,
                              lines_disp, win->offset);
          else if (line_nb_of_word_a[i] == last_line)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down, "/", langinfo, term, win,
                                lines_disp, win->offset);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
          else if (last_line + 1 > win->max_lines
                   && (int) ((float) (line_nb_of_word_a[current])
                             / (last_line + 1) * (win->max_lines - 2) + 2)
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
          puts("");
          if (win->offset > 0)
            printf("%*s", win->offset, " ");
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
        if (line_nb_of_word_a[i] == last_line)
        {
          if (!toggle->no_scrollbar)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down, "/", langinfo, term, win,
                                lines_disp, win->offset);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, win,
                                lines_disp, win->offset);
          }
        }
        else
        {
          if (!toggle->no_scrollbar)
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

    /* next word */
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
  tputs(restore_cursor, 1, outch);

  return lines_disp;
}

/* ============================================ */
/* Signal handler. manages SIGWINCH and SIGALRM */
/* ============================================ */
void
sig_handler(int s)
{
  switch (s)
  {
      /* Standard Terminaison signals */
      /* """""""""""""""""""""""""""" */
    case SIGTERM:
    case SIGHUP:
      fputs("Interrupted!\n", stderr);
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
      if (help_mode)
        got_help_alrm = 1;

      if (search_mode)
        got_search_alrm = 1;

      if (got_winch)
      {
        got_winch = 0;
        got_winch_alrm = 1;
      }

      break;
  }
}

/* ========================================================== */
/* Try to find the next word mathing the prefix in search_buf */
/* return 1 if one ha been found, 0 otherwise                 */
/* ========================================================== */
int
search_next(tst_node_t * tst, word_t * word_a, char *search_buf, int after_only)
{
  wchar_t *w;
  int found = 0;

  /* Consider a word under the cursor found if it matches the search prefix. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!after_only)
    if (memcmp(word_a[current].str, search_buf, strlen(search_buf)) == 0)
      return 1;

  /* Search the next matching word in the ternary search tree */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  w = mb_strtowcs(search_buf);
  new_current = count - 1;
  if (NULL == tst_prefix_search(tst, w, tst_cb))
    new_current = current;
  else
  {
    current = new_current;
    found = 1;
  }
  free(w);

  return found;
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
#define BADCH     (int)'?'
#define NEEDSEP   (int)':'
#define MAYBESEP  (int)'%'
#define ERRFD   2
#define EMSG    ""
#define START   "-"

/* Here are all the pertinent global variables. */
/* """""""""""""""""""""""""""""""""""""""""""" */
int opterr = 1;              /* if true, output error message */
int optind = 1;              /* index into parent argv vector */
int optopt;                  /* character checked for validity */
int optbad = BADCH;          /* character returned on error */
int optchar = 0;             /* character that begins returned option */
int optneed = NEEDSEP;       /* flag for mandatory argument */
int optmaybe = MAYBESEP;     /* flag for optional argument */
int opterrfd = ERRFD;        /* file descriptor for error text */
char *optarg = NULL;         /* argument associated with option */
char *optstart = START;      /* list of characters that start options */

/* Macros. */
/* """"""" */

/* Conditionally print out an error message and return (depends on the */
/* setting of 'opterr' and 'opterrfd').  Note that this version of     */
/* TELL() doesn't require the existence of stdio.h.                    */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define TELL(S) { \
  if (opterr && opterrfd >= 0) { \
    char option = (char) optopt; \
    write(opterrfd, (S), strlen(S)); \
    write(opterrfd, &option, 1); \
    write(opterrfd, "\n", 1); \
  } \
  return (optbad); \
}

 /* Here it is: */
 /* """"""""""" */
int
egetopt(int nargc, char **nargv, char *ostr)
{
  static char *place = EMSG; /* option letter processing */
  register char *oli;        /* option letter list index */
  register char *osi = NULL; /* option start list index  */

  if (nargv == (char **) NULL)
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

    osi = strchr(optstart, *place);
    if (osi != NULL)
      optchar = (int) *osi;

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
  optopt = (int) *place++;
  oli = strchr(ostr, optopt);
  if (optopt == optneed || optopt == (int) optmaybe || oli == NULL)
  {
    /* If we're at the end of the current argument, bump the */
    /* argument index.                                       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == '\0')
      ++optind;

    TELL("Illegal option -- "); /* byebye */
  }

  /* If there is no argument indicator, then we don't even try to */
  /* return an argument.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ++oli;
  if (*oli == '\0' || (*oli != (char) optneed && *oli != (char) optmaybe))
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
    else if (*oli == (char) optneed)
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
        place = EMSG;
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
static int
delims_cmp(const void *a, const void *b)
{
  return strcmp((char *) a, (char *) b);
}

/* ========================================================= */
/* Set new first column to display when horizontal scrolling */
/* Alter win->first_column                                   */
/* ========================================================= */
void
set_new_first_column(win_t * win, term_t * term, word_t * word_a)
{
  int pos;

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

/* ================ */
/* Main entry point */
/* ================ */
int
main(int argc, char *argv[])
{
  char *message = NULL;      /* message to be desplayed above the          *
                              * selection window                           */
  ll_t *message_lines_list = NULL;      /* list of the lines in the        *
                                           message to be displayed         */
  int message_max_width;     /* total width of the message (longest line)  */
  int message_max_len;       /* max number of bytes taken by a message     *
                              * line                                       */

  int index;                 /* generic counter                            */

  char *include_pattern = ".";  /* Used by -e/-i                           */
  char *exclude_pattern = NULL;
  regex_t include_re;
  regex_t exclude_re;

  char *include_sed_pattern = NULL;     /* used by -E/-I                   */
  char *exclude_sed_pattern = NULL;
  char *include_replace_str = NULL;
  char *exclude_replace_str = NULL;
  regex_t include_sed_re;
  regex_t exclude_sed_re;

  char *first_word_pattern = NULL;      /* used by -A/-Z                   */
  char *last_word_pattern = NULL;
  regex_t first_word_re;
  regex_t last_word_re;

  char *attention_pattern[5] = { NULL };        /* used by -1 ... -5       */
  regex_t attention_re[5];

  int include_global_replace;   /* control if we replace only the first    */
  int exclude_global_replace;   /* instance                                */

  int include_visual_only = 0;  /* The original word read from stdin will  */
  int exclude_visual_only = 0;  /* be restituted unaltered even if its     *
                                 * visual representation was modified via  *
                                 * -E/-I                                   */

  char *cols_selector = NULL;

  int message_lines;

  int i;                     /* word index                                 */

  term_t term;               /* Terminal structure                         */

  tst_node_t *tst = NULL;    /* TST used by the search function            */

  int colors;                /* Temporary variable used to set term.colors */
  int page;                  /* Step for the vertical cursor deplacements  */
  char *word;                /* Temporarry variable to work on words       */
  char *tmp_max_word;
  int last_line = 0;         /* last logical line number (from 0)          */
  int opt;
  win_t win;
  limits_t limits;
  toggle_t toggle;
  word_t *word_a;            /* Array contanings words data (size: count)  */

  int old_fd1;               /* backups of the old stdout file descriptor  */
  FILE *old_stdout;          /* The selecterd word will go there           */

  int nl;                    /* Number of lines displayed in the window    */
  int offset;                /* Used to correstly put the curson at the    *
                              * start of the selection window, even after  *
                              * a terminal vertical scroll                 */

  int first_selectable;      /* Index of the first selectable word in the  *
                              * input stream                               */
  int last_selectable;       /* Index of the last selectable word in the   *
                              * input stream                               */

  int s, e;                  /* word variable to contain the starting and  *
                              * ending terminal position of a word         */
  int min_size;              /* Minimum screen width of a column in        *
                              * tabular mode                               */

  int tab_max_size;          /* Maximum screen width of a column in        *
                              * tabular mode                               */
  int tab_real_max_size;     /* Maximum size in bytes of a column in       *
                              * tabular mode                               */

  int *col_real_max_size = NULL;        /* Array of maximum sizes (bytes)  *
                                         * of each  column in column mode  */
  int *col_max_size = NULL;  /* Array of maximum sizes of each column in   *
                              * column mode                                */
  int col_index;             /* Index of the current column when reading   *
                              * words, used in column mode                 */
  int cols_number;           /* Number of columns in column mode           */

  char *pre_selection_index = NULL;     /* pattern used to set the initial *
                                         * cursor position                 */
  unsigned char *buffer = xmalloc(16);  /* Input buffer                    */

  char *search_buf = NULL;   /* Search buffer                              */
  int search_pos = 0;        /* Current position in the search buffer      */

  struct sigaction sa;       /* Signal structure                           */

  char *iws = NULL, *ils = NULL;
  ll_t *word_delims_list = NULL;
  ll_t *record_delims_list = NULL;

  char mb_buffer[5];         /* buffer to store the bytes of a             *
                              * multibyte character (4 chars max)          */
  int is_last;
  char *charset;

  char *home_ini_file;       /* init file full path                        */
  char *local_ini_file;      /* init file full path                        */

  charsetinfo_t *charset_ptr;
  langinfo_t langinfo;
  int is_supported_charset;

  /* Win fields initialization */
  /* """"""""""""""""""""""""" */
  win.max_lines = 5;
  win.asked_max_lines = -1;
  win.center = 0;
  win.max_cols = 0;
  win.col_sep = 0;
  win.wide_columns = 0;
  win.tab_mode = 0;
  win.col_mode = 0;
  win.line_mode = 0;
  win.first_column = 0;

  /* Defaulkt limts initialization */
  /* """"""""""""""""""""""""""""" */
  limits.words = 32767;
  limits.cols = 256;
  limits.word_length = 256;

  /* Toggles initialization */
  /* """""""""""""""""""""" */
  toggle.del_line = 0;
  toggle.enter_val_in_search = 0;
  toggle.no_scrollbar = 0;
  toggle.blank_nonprintable = 0;

  /* Columms seletion variables */
  /* """""""""""""""""""""""""" */
  char *cols_filter;

  /* Get the current locale */
  /* """""""""""""""""""""" */
  setlocale(LC_ALL, "");
  charset = nl_langinfo(CODESET);

  /* Check if the local charset is supported */
  /* """"""""""""""""""""""""""""""""""""""" */
  is_supported_charset = 0;
  charset_ptr = all_supported_charsets;

  while (charset_ptr->name != NULL)
  {
    if (strcmp(charset, charset_ptr->name) == 0)
    {
      is_supported_charset = 1;
      langinfo.bits = charset_ptr->bits;
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
  /* version of isprint accorging to the content of UTF-8           */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo.utf8 || langinfo.bits == 8)
    my_isprint = isprint8;
  else
    my_isprint = isprint7;

  /* Command line options analysis */
  /* """"""""""""""""""""""""""""" */
  while ((opt = egetopt(argc, argv,
                        "VhqdMbi:e:I:E:A:Z:1:2:3:4:5:C:"
                        "clwrgn:t%m:s:W:L:1:2:3:4:")) != -1)
  {
    switch (opt)
    {
      case 'V':
        fprintf(stderr, "Version: " VERSION "\n");
        exit(0);

      case 'n':
        if (optarg && *optarg != '-')
          win.asked_max_lines = atoi(optarg);
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'd':
        toggle.del_line = 1;
        break;

      case 'M':
        win.center = 1;
        break;

      case 's':
        if (optarg && *optarg != '-')
          pre_selection_index = strdup(optarg);
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 't':
        if (optarg != NULL)
          win.max_cols = atoi(optarg);
        win.tab_mode = 1;
        break;

      case 'c':
        win.tab_mode = 1;
        win.col_mode = 1;
        break;

      case 'l':
        win.line_mode = 1;
        win.tab_mode = 0;
        win.col_mode = 0;

      case 'g':
        win.col_sep = 1;
        break;

      case 'w':
        win.wide_columns = 1;
        break;

      case 'm':
        if (optarg && *optarg != '-')
          message = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'r':
        toggle.enter_val_in_search = 1;
        break;

      case 'b':
        toggle.blank_nonprintable = 1;
        break;

      case 'i':
        if (optarg && *optarg != '-')
          include_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'e':
        if (optarg && *optarg != '-')
          exclude_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'C':
        if (optarg && *optarg != '-')
        {
          cols_selector = optarg;
          win.col_mode = 1;
        }
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'I':
        if (optarg && *optarg != '-')
          include_sed_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'E':
        if (optarg && *optarg != '-')
          exclude_sed_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
        if (optarg && *optarg != '-')
          attention_pattern[opt - '1'] = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'q':
        toggle.no_scrollbar = 1;
        break;

      case 'A':
        if (optarg && *optarg != '-')
          first_word_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'Z':
        if (optarg && *optarg != '-')
          last_word_pattern = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'W':
        if (optarg && *optarg != '-')
          iws = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case 'L':
        if (optarg && *optarg != '-')
          ils = optarg;
        else
        {
          fprintf(stderr, "Option requires an argument -- %c\n\n",
                  (char) optopt);
          usage();
        }
        break;

      case '?':
        fputs("\n", stderr);
      case 'h':
      default:
        usage();
    }
    optarg = NULL;
  }

  if (optind < argc)
  {
    fprintf(stderr, "Not an option -- %s\n\n", argv[argc - 1]);
    usage();
  }

  /* If we did not impose the number of columns, uwe the whole terminal width */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!win.max_cols)
    win.wide_columns = 1;

  win.start = 0;

  void sig_handler(int s);

  /* Ignore SIGTTIN and SIGTTOU */
  /* """""""""""""""""""""""""" */
  sigset_t sigs, oldsigs;

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGTTIN);
  sigprocmask(SIG_BLOCK, &sigs, &oldsigs);

  sa.sa_handler = sig_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGALRM, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  /* Set terminal in noncanoniccal, noecho mode */
  /* """""""""""""""""""""""""""""""""""""""""" */
  setupterm((char *) 0, 1, (int *) 0);

  term.colors = ((colors = tigetnum("colors")) == -1) ? 0 : colors;
  term.has_setf = (tigetstr("setf") == 0) ? 0 : 1;
  term.has_setb = (tigetstr("setb") == 0) ? 0 : 1;
  term.has_setaf = (tigetstr("setaf") == 0) ? 0 : 1;
  term.has_setab = (tigetstr("setab") == 0) ? 0 : 1;
  term.has_hpa = (tigetstr("hpa") == 0) ? 0 : 1;
  term.color_method = 1;     /* we default to setaf/setbf to set colors */

  /* Get the number of lines/columns of the terminal */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  get_terminal_size(&term.nlines, &term.ncolumns);

  /* Build the full path of the ini file */
  /* """"""""""""""""""""""""""""""""""" */
  home_ini_file = make_ini_path(argv[0], "HOME");
  local_ini_file = make_ini_path(argv[0], "PWD");

  /* If the ini file failed to provide custom colors or is inexistant */
  /* or not readable then  set their defaut values                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (term.colors > 7)
  {
    win.bar_color.fg = 2;
    win.bar_color.bg = -1;
    win.search_color.fg = 0;
    win.search_color.bg = 5;
    win.exclude_color.fg = 3;
    win.exclude_color.bg = -1;

    win.attention1_color.fg = 1;
    win.attention2_color.fg = 2;
    win.attention3_color.fg = 3;
    win.attention4_color.fg = -1;
    win.attention5_color.fg = -1;

    win.attention1_color.bg = -1;
    win.attention2_color.bg = -1;
    win.attention3_color.bg = -1;
    win.attention4_color.bg = -1;
    win.attention5_color.bg = -1;
  }

  /* Set the colors from the config file if possible */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  if (ini_load(home_ini_file, &win, &term, &limits, ini_cb))
    exit(EXIT_FAILURE);
  if (ini_load(local_ini_file, &win, &term, &limits, ini_cb))
    exit(EXIT_FAILURE);

  free(home_ini_file);
  free(local_ini_file);

  if (message != NULL)
  {
    message_lines_list = ll_new();
    get_message_lines(message, message_lines_list,
                      &message_max_width, &message_max_len);
  }

  /* Force the maximum number of window's line if -n is used */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.asked_max_lines > 0)
    win.max_lines = win.asked_max_lines;

  /* Allocate the memory for our words structures */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  word_a = xmalloc(WORDSCHUNK * sizeof(word_t));

  /* Fill an array of word_t elements obtained from stdin */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  tab_real_max_size = 0;
  tab_max_size = 0;
  min_size = 0;

  /* Parse the word separators string (option -W). If it is empty then  */
  /* the standard delimiters (space, tab and EOL) are used. Each of its */
  /* multibyte sequences are stored in a linked list.                   */
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
    int mb_len;
    char *iws_ptr = iws;
    char *tmp;

    mb_len = mblen(iws_ptr, 4);

    while (mb_len != 0)
    {
      tmp = xmalloc(mb_len + 1);
      memcpy(tmp, iws_ptr, mb_len);
      tmp[mb_len] = '\0';
      ll_append(word_delims_list, tmp);

      iws_ptr += mb_len;
      mb_len = mblen(iws_ptr, 4);
    }
  }

  /* Parse the line separators string (option -L). If it is empty then */
  /* the standard delimiter (newline) is used. Each of its multibyte   */
  /* sequences are stored in a linked list.                            */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  record_delims_list = ll_new();

  if (ils == NULL)
    ll_append(record_delims_list, "\n");
  else
  {
    int mb_len;
    char *ils_ptr = ils;
    char *tmp;

    mb_len = mblen(ils_ptr, 4);

    while (mb_len != 0)
    {
      tmp = xmalloc(mb_len + 1);
      memcpy(tmp, ils_ptr, mb_len);
      tmp[mb_len] = '\0';
      ll_append(record_delims_list, tmp);

      /* Add this record delimiter as a word delimiter */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      if (ll_find(word_delims_list, tmp, delims_cmp) == NULL)
        ll_append(word_delims_list, tmp);

      ils_ptr += mb_len;
      mb_len = mblen(ils_ptr, 4);
    }
  }

  /* Initialize the first chunks of the arrays which will contain the */
  /* maximum length of each column in column mode                     */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    col_real_max_size = xmalloc(COLSCHUNK * sizeof(int));
    col_max_size = xmalloc(COLSCHUNK * sizeof(int));

    for (i = 0; i < COLSCHUNK; i++)
      col_real_max_size[i] = col_max_size[i] = 0;

    col_index = cols_number = 0;
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
      && regcomp(&first_word_re, first_word_pattern,
                 REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", first_word_pattern);

    exit(EXIT_FAILURE);
  }

  if (last_word_pattern
      && regcomp(&last_word_re, last_word_pattern,
                 REG_EXTENDED | REG_NOSUB) != 0)
  {
    fprintf(stderr, "Bad regular expression %s\n", last_word_pattern);

    exit(EXIT_FAILURE);
  }

  for (index = 0; index < 5; index++)
  {
    if (attention_pattern[index]
        && regcomp(&attention_re[index], attention_pattern[index],
                   REG_EXTENDED | REG_NOSUB) != 0)
    {
      fprintf(stderr, "Bad regular expression %s\n", attention_pattern[index]);

      exit(EXIT_FAILURE);
    }
  }

  /* Parse the post-processing patterns and extract its values */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (include_sed_pattern)
    if (parse_replacement_string(include_sed_pattern,
                                 &include_sed_re,
                                 &include_replace_str,
                                 &include_global_replace,
                                 &include_visual_only) == 0)
    {
      fprintf(stderr, "Bad -I argument. Must be something like: "
              "/regex/repl_string/[g][v]\n");

      exit(EXIT_FAILURE);
    }

  if (exclude_sed_pattern)
    if (parse_replacement_string(exclude_sed_pattern,
                                 &exclude_sed_re,
                                 &exclude_replace_str,
                                 &exclude_global_replace,
                                 &exclude_visual_only) == 0)
    {
      fprintf(stderr, "Bad -E argument. Must be something like: "
              "/regex/repl_string/[g][v]\n");

      exit(EXIT_FAILURE);
    }

  /* Parse the column selection string if any */
  /* """""""""""""""""""""""""""""""""""""""" */
  if (cols_selector != NULL)
  {
    char *unparsed = strdup(cols_selector);

    parse_cols_selector(cols_selector, &cols_filter, unparsed);

    if (*unparsed != '\0')
    {
      fprintf(stderr, "Bad -C argument. Unparsed part: %s\n", unparsed);

      exit(EXIT_FAILURE);
    }

    free(unparsed);
  }

  /* Get and process the input stream words */
  /* """""""""""""""""""""""""""""""""""""" */
  while ((word = get_word(stdin, word_delims_list, record_delims_list,
                          mb_buffer, &is_last, &toggle, &langinfo,
                          &win, &limits)) != NULL)
  {
    int size;
    int *data;
    wchar_t *w;
    wchar_t *tmpw;
    int s;
    ll_t *list = NULL;
    char *dest;
    char *new_dest;
    size_t len;
    size_t word_len;
    int selectable;
    char buf[1024] = { 0 };
    int is_first = 0;
    char *unaltered_word;
    int attention_level;

    if (*word == '\0')
      continue;

    /* Manipulates the is_last flag word indicator to make this word     */
    /* the first or last one of the current line in column/line/tab mode */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode | win.line_mode | win.tab_mode)
    {
      if (first_word_pattern
          && regexec(&first_word_re, word, (size_t) 0, NULL, 0) == 0)
        is_first = 1;

      if (last_word_pattern
          && !is_last && regexec(&last_word_re, word, (size_t) 0, NULL, 0) == 0)
        is_last = 1;
    }

    /* Check if the word is special */
    /* """""""""""""""""""""""""""" */
    attention_level = 0;
    for (index = 0; index < 5; index++)
    {
      if (attention_pattern[index] != NULL
          && regexec(&attention_re[index], word, (size_t) 0, NULL, 0) == 0)
      {
        attention_level = index + 1;
        break;
      }
    }

    /* Check if the word will be selectable or not */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    if (include_pattern == NULL && exclude_pattern == NULL)
      selectable = 1;
    else if (exclude_pattern
             && regexec(&exclude_re, word, (size_t) 0, NULL, 0) == 0)
      selectable = 0;
    else if (include_pattern
             && regexec(&include_re, word, (size_t) 0, NULL, 0) == 0)
      selectable = 1;
    else
      selectable = 0;

    /* Save the original word */
    /* """""""""""""""""""""" */
    unaltered_word = strdup(word);

    /* Possibly modify the word acording to -I/-E arguments */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (selectable && include_sed_pattern)
    {
      if (replace(word, include_replace_str, &include_sed_re,
                  include_global_replace, buf, 1024))
      {
        free(word);

        if (*buf == '\0')
          word = strdup("_");
        else
          word = strdup(buf);
      }
    }

    if (!selectable && exclude_sed_pattern)
    {
      if (replace(word, exclude_replace_str, &exclude_sed_re,
                  exclude_global_replace, buf, 1024))
      {
        free(word);

        if (*buf == '\0')
          word = strdup("");
        else
          word = strdup(buf);
      }
    }

    /* Alter the word just read be replacing special chars  by their */
    /* escaped equivalents.                                          */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    dest = xmalloc(5 * strlen(word) + 1);
    len = expand(word, dest, &langinfo);
    new_dest = strdup(dest);
    free(dest);
    dest = new_dest;

    word_len = len;

    /* In column mode each column have their own max size, so me need to */
    /* maintain an array of the max column size for them                 */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode)
    {
      if (is_first)
        col_index = 1;
      else
        col_index++;

      if (col_index > cols_number)
      {
        if (col_index == limits.cols)
        {
          fprintf(stderr, "The number of columns has reached the limit "
                  "(%d), exiting.\n", limits.cols);

          exit(EXIT_FAILURE);
        }

        cols_number++;

        /* We have a new column, see if we need to enlarge the arrays */
        /* indexed on the number of columns                           */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (cols_number % COLSCHUNK == 0)
        {
          int i;

          col_real_max_size = xrealloc(col_real_max_size,
                                       (cols_number + COLSCHUNK) * sizeof(int));

          col_max_size = xrealloc(col_max_size,
                                  (cols_number + COLSCHUNK) * sizeof(int));

          /* Initialize the max size for the new columns */
          /* """"""""""""""""""""""""""""""""""""""""""" */
          for (i = 0; i < COLSCHUNK; i++)
          {
            col_real_max_size[cols_number + i] = 0;
            col_max_size[cols_number + i] = 0;
          }
        }
      }

      /* Restricts the selection to certain columns */
      /* """""""""""""""""""""""""""""""""""""""""" */
      if (selectable == 1 && cols_selector != NULL)
        if (cols_filter[col_index - 1] == '0')
          selectable = 0;

      /* Update the max values of col_real_max_size[col_index - 1] */
      /* and col_max_size[col_index - 1]                           */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((s = word_len) > col_real_max_size[col_index - 1])
        col_real_max_size[col_index - 1] = s;

      s = mbstowcs(0, dest, 0);
      s = wcswidth((tmpw = mb_strtowcs(dest)), s);
      free(tmpw);

      if (s > col_max_size[col_index - 1])
        col_max_size[col_index - 1] = s;

      /* Reset the column index when we enconuter and end-of-line */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (is_last)
        col_index = 0;
    }

    word_a[count].start = word_a[count].end = 0;

    word_a[count].str = dest;
    word_a[count].is_selectable = selectable;

    word_a[count].attention_level = attention_level;

    if (strcmp(word, dest) != 0)
      word_a[count].orig = word;
    else
    {
      word_a[count].orig = NULL;
      free(word);
    }

    /* Set the last word in line indicator when in column/line/tab mode */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode | win.line_mode | win.tab_mode)
    {
      if (is_first && count > 0)
        word_a[count - 1].is_last = 1;
      word_a[count].is_last = is_last;
    }
    else
      word_a[count].is_last = 0;

    /* If the word is selectable insert it in the TST tree */
    /* with its associated index in the input stream.      */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[count].is_selectable)
    {
      data = xmalloc(sizeof(int));
      *data = count;

      /* Create a wide characters string from the word string content */
      /* to be able to store in in the TST.                           */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (word_a[count].orig == NULL)
        w = mb_strtowcs(word_a[count].str);
      else
        w = mb_strtowcs(word_a[count].orig);

      /* If we didn't already encounter this word, then create a new entry in */
      /* the TST for it and store its index in its list.                      */
      /* Otherwise, add its index in its index list.                          */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (tst && (list = tst_search(tst, w)) != NULL)
        ll_append(list, data);
      else
      {
        list = ll_new();
        ll_append(list, data);
        tst = tst_insert(tst, w, list);
      }
      free(w);
    }

    /* Store the new max number of bytes in a word */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    if ((size = word_len) > tab_real_max_size)
      tab_real_max_size = word_len;

    /* Store the new max word width */
    /* """""""""""""""""""""""""""" */
    size = mbstowcs(0, dest, 0);

    if ((size = wcswidth((tmpw = mb_strtowcs(dest)), size)) > tab_max_size)
      tab_max_size = size;

    free(tmpw);

    /* When the visual only flag is set, we keep the unaltered word so */
    /* that it can be restituted even if its visual ans dearchable     */
    /* representation may have been altered by the previous code       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((word_a[count].is_selectable & include_visual_only)
        | ((!word_a[count].is_selectable) & exclude_visual_only))
    {
      free(word_a[count].orig);
      word_a[count].orig = unaltered_word;
    }
    else
      free(unaltered_word);

    /* One more word... */
    /* """""""""""""""" */
    if (count == limits.words)
    {
      fprintf(stderr, "The number of read words has reached the limit "
              "(%d), exiting.\n", limits.words);

      exit(EXIT_FAILURE);
    }

    count++;

    if (count % WORDSCHUNK == 0)
      word_a = xrealloc(word_a, (count + WORDSCHUNK) * sizeof(word_t));
  }

  /* All input word have now been read */
  /* """"""""""""""""""""""""""""""""" */

  /* Early exit if there is no input */
  /* """"""""""""""""""""""""""""""" */
  if (count == 0)
    exit(EXIT_FAILURE);

  /* Set the minimum width of a column (-t option) */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (win.wide_columns)
  {
    if (win.max_cols > 0)
      min_size = (term.ncolumns - 2) / win.max_cols - 1;

    if (min_size < 0)
      min_size = term.ncolumns - 2;
  }

  /* Allocate the space for the satellites arrays */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  line_nb_of_word_a = xmalloc(count * sizeof(int));
  first_word_in_line_a = xmalloc(count * sizeof(int));

  /* When in column or tabulating mode, we need to adjust the length of   */
  /* all the words.                                                       */
  /* In column mode the size of each column is variable; in tabulate mode */
  /* it is constant.                                                      */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    char *temp;

    /* Total space taken by all the columns plus the gutter */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    for (col_index = 0; col_index < cols_number; col_index++)
      win.max_width += col_max_size[col_index] + 1;

    col_index = 0;
    for (i = 0; i < count; i++)
    {
      int s1, s2;
      size_t word_width;
      wchar_t *w;

      s1 = strlen(word_a[i].str);
      word_width = mbstowcs(0, word_a[i].str, 0);
      s2 = wcswidth((w = mb_strtowcs(word_a[i].str)), word_width);
      free(w);
      temp = xcalloc(1, col_real_max_size[col_index] + s1 - s2 + 1);
      memset(temp, ' ', col_max_size[col_index] + s1 - s2);
      memcpy(temp, word_a[i].str, s1);
      temp[col_real_max_size[col_index] + s1 - s2] = '\0';
      free(word_a[i].str);
      word_a[i].str = temp;

      if (word_a[i].is_last)
        col_index = 0;
      else
        col_index++;
    }
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

    for (i = 0; i < count; i++)
    {
      int s1, s2;
      size_t word_width;
      wchar_t *w;

      s1 = strlen(word_a[i].str);
      word_width = mbstowcs(0, word_a[i].str, 0);
      s2 = wcswidth((w = mb_strtowcs(word_a[i].str)), word_width);
      free(w);
      temp = xcalloc(1, tab_real_max_size + s1 - s2 + 1);
      memset(temp, ' ', tab_max_size + s1 - s2);
      memcpy(temp, word_a[i].str, s1);
      temp[tab_real_max_size + s1 - s2] = '\0';
      free(word_a[i].str);
      word_a[i].str = temp;
    }
  }

  word_a[count].str = NULL;

  /* We can now allocate the space for our tmp_max_word work variable */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tmp_max_word = xmalloc(tab_real_max_size + 1);

  win.start = 0;             /* index of the first element in the    *
                              * words array to be  displayed         */

  /* Index of the selected element in the array words                */
  /* Ths string can be:                                              */
  /*   "last"    The string "last"   put the cursor on the last word */
  /*   n         a number            put the cursor on th word n     */
  /*   /pref     /+a regexp          put the cursor on the first     */
  /*                                 word matching the prefix "pref" */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  /* Find the first selectable word (if any) in the input stream */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (first_selectable = 0;
       first_selectable < count - 1
       && !word_a[first_selectable].is_selectable; first_selectable++);

  /* If not found, abort */
  /* """"""""""""""""""" */
  if (!word_a[first_selectable].is_selectable)
  {
    fprintf(stderr, "No selectable word found.\n");

    exit(EXIT_FAILURE);
  }

  /* If not found, abort */
  /* """"""""""""""""""" */
  if (!word_a[first_selectable].is_selectable)
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
    current = first_selectable;
  else if (strcmp(pre_selection_index, "last") == 0)
    current = last_selectable;
  else if (*pre_selection_index == '/')
  {
    wchar_t *w;

    new_current = last_selectable;
    if (NULL != tst_prefix_search(tst,
                                  w = mb_strtowcs(pre_selection_index + 1),
                                  tst_cb_cli))
      current = new_current;
    else
      current = first_selectable;
    free(w);
  }
  else if (*pre_selection_index != '\0')
  {
    current = atoi(pre_selection_index);
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

  /* We can now build the metadata */
  /* """"""""""""""""""""""""""""" */
  last_line = build_metadata(word_a, &term, count, &win);

  /* We've finished reading from stdin                              */
  /* we will now get the inputs from the controling terminal if any */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if ((stdin = freopen("/dev/tty", "r", stdin)) == NULL)
    fprintf(stderr, "%s\n", "freopen");

  old_fd1 = dup(1);
  old_stdout = fdopen(old_fd1, "w");
  setbuf(old_stdout, NULL);

  if ((stdout = freopen("/dev/tty", "w", stdout)) == NULL)
    fprintf(stderr, "%s\n", "freopen");

  setbuf(stdout, NULL);

  /* Set the caracteristics of the terminal */
  /* """""""""""""""""""""""""""""""""""""" */
  setup_term(fileno(stdin));

  /* Initialize the search buffer with tab_real_max_size+1 NULs */
  /* It wil never be reallocated, only cleared.                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  search_buf = xcalloc(1, tab_real_max_size + 1);

  /* Hide the cursor */
  /* """"""""""""""" */
  tputs(cursor_invisible, 1, outch);

  /* Force the display to start at a beginning of line */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);
  if (term.curs_column > 1)
    puts("");

  /* Display the words window for the first time */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  message_lines = disp_message(message_lines_list,
                               message_max_width, message_max_len, &term, &win);

  if (win.col_mode | win.line_mode)
  {
    int pos;
    int len;

    len = term.ncolumns - 3;

    /* Adjust win.first_column if the cursor is no more visible */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    pos = first_word_in_line_a[line_nb_of_word_a[current]];

    while (word_a[current].end - word_a[pos].start >= len)
      pos++;

    win.first_column = word_a[pos].start;
  }

  /* Save the cursor line column */
  /* """"""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                  search_buf, &term, last_line, tmp_max_word, &langinfo);

  /* Determine the number of lines to move the cursor up if the window */
  /* display needed a terminel scrolling                               */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (nl + term.curs_line - 1 > term.nlines)
    offset = term.curs_line + nl - term.nlines;
  else
    offset = 0;

  /* Set the cursor to the first line of the window */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  for (i = 1; i < offset; i++)
    tputs(cursor_up, 1, outch);

  /* Save again the cursor current line and column positions */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  /* Main loop */
  /* """"""""" */
  while (1)
  {
    int l;

    /* If this alarm is triggered, then redisplay the window */
    /* to remove the help message and disable this timer.    */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_help_alrm)
    {
      got_help_alrm = 0;
      help_mode = 0;

      /* Disarm the help timer to 10s */
      /* """""""""""""""""""""""""""" */
      hlp_itv.it_value.tv_sec = 0;
      hlp_itv.it_value.tv_usec = 0;
      hlp_itv.it_interval.tv_sec = 0;
      hlp_itv.it_interval.tv_usec = 0;
      setitimer(ITIMER_REAL, &hlp_itv, NULL);

      nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                      search_buf, &term, last_line, tmp_max_word, &langinfo);
    }

    /* If an alarm has been tiggered and we are in search mode, try to    */
    /* use the  prefix in search_buf to find the first mord matching this */
    /* prefix. If a word if found trigger a window refresh as if the      */
    /* terminal has been  resized                                         */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_search_alrm)
    {
      got_search_alrm = 0;

      if (search_mode)
      {
        search_mode = 0;
        nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                        search_buf, &term, last_line, tmp_max_word, &langinfo);
      }
    }

    /* If the terminal has been resized */
    /* """""""""""""""""""""""""""""""" */
    if (got_winch)
    {
      /* Re-arm winch timer to 1s */
      /* """""""""""""""""""""""" */
      winch_itv.it_value.tv_sec = 1;
      winch_itv.it_value.tv_usec = 0;
      winch_itv.it_interval.tv_sec = 0;
      winch_itv.it_interval.tv_usec = 0;
      setitimer(ITIMER_REAL, &winch_itv, NULL);
    }

    /* Upon expiration of this alarm, we trigger a content update of the window */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (got_winch_alrm)
    {
      got_winch_alrm = 0;

      get_terminal_size(&term.nlines, &term.ncolumns);

      /* Erase the current window */
      /* """""""""""""""""""""""" */
      for (i = 0; i < message_lines; i++)
      {
        tputs(cursor_up, 1, outch);
        tputs(clr_bol, 1, outch);
        tputs(clr_eol, 1, outch);
      }

      tputs(clr_bol, 1, outch);
      tputs(clr_eol, 1, outch);
      tputs(save_cursor, 1, outch);

      for (i = 1; i < nl + message_lines; i++)
      {
        tputs(cursor_down, 1, outch);
        tputs(clr_bol, 1, outch);
        tputs(clr_eol, 1, outch);
      }

      tputs(restore_cursor, 1, outch);

      /* Calculate the new metadata and draw the window again */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      last_line = build_metadata(word_a, &term, count, &win);

      if (win.col_mode | win.line_mode)
      {
        int pos;
        int len;

        len = term.ncolumns - 3;

        /* Adjust win.first_column if the cursor is no more visible */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        pos = first_word_in_line_a[line_nb_of_word_a[current]];

        while (word_a[current].end - word_a[pos].start >= len)
          pos++;

        win.first_column = word_a[pos].start;
      }

      message_lines = disp_message(message_lines_list,
                                   message_max_width, message_max_len,
                                   &term, &win);

      nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                      search_buf, &term, last_line, tmp_max_word, &langinfo);

      /* Get new cursor position */
      /* """"""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Determine the number of lines to move the cursor up if the window  */
      /* display needed a terminel scrolling                                */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (nl + term.curs_line > term.nlines)
        offset = term.curs_line + nl - term.nlines;
      else
        offset = 0;

      /* Set the curser to the first line of the window */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      for (i = 1; i < offset; i++)
        tputs(cursor_up, 1, outch);

      /* Get new cursor position */
      /* """"""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Short-circuit the loop */
      /* """""""""""""""""""""" */
      continue;
    }

    page = 1;                /* Default number of lines to do down/up *
                              * with PgDn/PgUp                        */

    l = get_scancode(buffer, 15);

    if (l)
    {
      switch (buffer[0])
      {
          /* An esccape sequence or a multibyte character has been pressed */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        case 0x1b:          /* ESC */
          if (!search_mode)
          {
            /* HOME key has been pressed */
            /* """"""""""""""""""""""""" */
            if (memcmp("\x1bOH", buffer, 3) == 0)
            {
              current = win.start;

              /* Find the first selectable word */
              /* """""""""""""""""""""""""""""" */
              while (current < win.end && !word_a[current].is_selectable)
                current++;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode | win.line_mode)
                if (word_a[current].end < win.first_column)
                  win.first_column = word_a[current].start;

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
              break;
            }

            /* END key has been pressed */
            /* """""""""""""""""""""""" */
            if (memcmp("\x1bOF", buffer, 3) == 0)
            {
              current = win.end;

              /* Find the last selectable word */
              /* """"""""""""""""""""""""""""" */
              while (current > win.start && !word_a[current].is_selectable)
                current--;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode | win.line_mode)
              {
                int pos;
                int len;

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

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
              break;
            }

            /* Left arrow key has been pressed */
            /* """"""""""""""""""""""""""""""" */
            if (memcmp("\x1bOD", buffer, 3) == 0
                || memcmp("\x1b[D", buffer, 3) == 0)
              goto kl;

            /* Right arrow key has been pressed */
            /* """""""""""""""""""""""""""""""" */
            if (memcmp("\x1bOC", buffer, 3) == 0
                || memcmp("\x1b[C", buffer, 3) == 0)
              goto kr;

            /* Up arrow key has been pressed */
            /* """"""""""""""""""""""""""""" */
            if (memcmp("\x1bOA", buffer, 3) == 0
                || memcmp("\x1b[A", buffer, 3) == 0)
              goto ku;

            /* Down arrow key has been pressed */
            /* """"""""""""""""""""""""""""""" */
            if (memcmp("\x1bOB", buffer, 3) == 0
                || memcmp("\x1b[B", buffer, 3) == 0)
              goto kd;

            /* PgUp key has been pressed */
            /* """"""""""""""""""""""""" */
            if (memcmp("\x1b[5~", buffer, 4) == 0)
              goto kpp;

            /* PgDn key has been pressed */
            /* """"""""""""""""""""""""" */
            if (memcmp("\x1b[6~", buffer, 4) == 0)
              goto knp;
          }

          /* ESC key has been pressed */
          /* """""""""""""""""""""""" */
          if (memcmp("\x1b", buffer, 1) == 0)
          {
            if (search_mode || help_mode)
            {
              search_mode = 0;
              help_mode = 0;
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
              break;
            }
          }

          /* Else ignore key */
          break;

          /* q or Q has been pressed */
          /* """"""""""""""""""""""" */
        case 'q':
        case 'Q':
        case 3:             /* ^C */
          if (search_mode)
            goto special_cmds_when_searching;

          for (i = 0; i < message_lines; i++)
            tputs(cursor_up, 1, outch);

          if (toggle.del_line)
          {
            tputs(clr_eol, 1, outch);
            tputs(clr_bol, 1, outch);
            tputs(save_cursor, 1, outch);

            for (i = 1; i < nl + message_lines; i++)
            {
              tputs(cursor_down, 1, outch);
              tputs(clr_eol, 1, outch);
              tputs(clr_bol, 1, outch);
            }
            tputs(restore_cursor, 1, outch);
          }
          else
          {
            for (i = 1; i < nl + message_lines; i++)
              tputs(cursor_down, 1, outch);
            puts("");
          }

          tputs(cursor_normal, 1, outch);
          restore_term(fileno(stdin));

          exit(EXIT_SUCCESS);

          /* n or <Space Bar> has been pressed */
          /* """"""""""""""""""""""""""""""""" */
        case 'n':
          if (search_mode)
            goto special_cmds_when_searching;

        case ' ':
          if (search_next(tst, word_a, search_buf, 1))
            if (current > win.end)
              last_line = build_metadata(word_a, &term, count, &win);

          nl = disp_lines(word_a, &win, &toggle, current, count,
                          search_mode, search_buf, &term, last_line,
                          tmp_max_word, &langinfo);
          break;

          /* <Enter> has been pressed */
          /* """""""""""""""""""""""" */
        case 0x0d:          /* CR */
        {

          int extra_lines;
          char *output_str;
          int width;
          wchar_t *w;

          if (search_mode || help_mode)
          {
            search_mode = 0;
            help_mode = 0;

            nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                            search_buf, &term, last_line, tmp_max_word,
                            &langinfo);

            if (!toggle.enter_val_in_search)
              break;
          }

          /* Erase or jump after the window before printing the */
          /* selected string.                                   */
          /* """""""""""""""""""""""""""""""""""""""""""""""""" */
          if (toggle.del_line)
          {
            for (i = 0; i < message_lines; i++)
              tputs(cursor_up, 1, outch);

            tputs(clr_eol, 1, outch);
            tputs(clr_bol, 1, outch);
            tputs(save_cursor, 1, outch);

            for (i = 1; i < nl + message_lines; i++)
            {
              tputs(cursor_down, 1, outch);
              tputs(clr_eol, 1, outch);
              tputs(clr_bol, 1, outch);
            }
            tputs(restore_cursor, 1, outch);
          }
          else
          {
            for (i = 1; i < nl; i++)
              tputs(cursor_down, 1, outch);
            puts("");
          }

          /* Restore the visibility of the cursor */
          /* """""""""""""""""""""""""""""""""""" */
          tputs(cursor_normal, 1, outch);

          /* Chose the original string to print if the current one has */
          /* been alterated by a possible expansion.                   */
          /* Once this made, print it.                                 */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (word_a[current].orig != NULL)
            output_str = word_a[current].orig;
          else
            output_str = word_a[current].str;

          /* And print it. */
          /* """"""""""""" */
          fprintf(old_stdout, "%s", output_str);

          /* If the output stream is a terminal */
          /* """""""""""""""""""""""""""""""""" */
          if (isatty(old_fd1))
          {
            /* Determine the width (in term of terminal columns) of the  */
            /* string to be displayed.                                   */
            /* 65535 is arbitrary max string width                       */
            /* if width gets the value -1, then a least one characher in */
            /* output_str is not printable. In this case we do not try   */
            /* To remove the possible extra lines because the resulting  */
            /* output is unpredictable.                                  */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            width = wcswidth((w = mb_strtowcs(output_str)), 65535);
            free(w);

            /* With that information, count the number of terminal lines */
            /* printed.                                                  */
            /* Notice that extra_lines == 0 if width == -1               */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            extra_lines = width / term.ncolumns;

            /* Clean the printed line and all the extra lines used. */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
            tputs(delete_line, 1, outch);

            for (i = 0; i < extra_lines; i++)
            {
              tputs(cursor_up, 1, outch);
              tputs(clr_eol, 1, outch);
              tputs(clr_bol, 1, outch);
            }
          }

          /* Set the cursor at the start on the line an restore the */
          /* original terminal state before exiting                 */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
          tputs(carriage_return, 1, outch);
          restore_term(fileno(stdin));

          exit(EXIT_SUCCESS);
        }

          /* Cursor Left key has been pressed */
          /* """""""""""""""""""""""""""""""" */
        kl:
        case 'H':
        case 'h':
          if (!search_mode)
          {
            int old_current = current;
            int old_start = win.start;

            do
            {
              if (current > 0)
              {
                if (current == win.start)
                  if (win.start > 0)
                  {
                    for (i = win.start - 1; i >= 0 && word_a[i].start != 0; i--)
                    {
                    }
                    win.start = i;

                    if (word_a[i].str != NULL)
                      win.start = i;

                    if (win.end < count - 1)
                    {
                      for (i = win.end + 2;
                           i < count - 1 && word_a[i].start != 0; i++)
                      {
                      }
                      if (word_a[i].str != NULL)
                        win.end = i;
                    }
                  }

                /* In column mode we need to take care of the */
                /* horizontal scrolling                       */
                /* """""""""""""""""""""""""""""""""""""""""" */
                if (win.col_mode | win.line_mode)
                {
                  int pos;

                  if (word_a[current].start == 0)
                  {
                    int len;

                    len = term.ncolumns - 3;
                    pos = first_word_in_line_a[line_nb_of_word_a[current - 1]];

                    while (word_a[current - 1].end - win.first_column >= len)
                    {
                      win.first_column +=
                        word_a[pos].end - word_a[pos].start + 2;
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
            }
            while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current = old_current;
              win.start = old_start;
            }

            if (current != old_current)
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
            break;
          }
          else
            goto special_cmds_when_searching;

          /* Right key has been pressed */
          /* """""""""""""""""""""""""" */
        kr:
        case 'L':
        case 'l':
          if (!search_mode)
          {
            int old_current = current;
            int old_start = win.start;

            do
            {
              if (current < count - 1)
              {
                if (current == win.end)
                  if (win.start < count - 1 && win.end != count - 1)
                  {
                    for (i = win.start + 1;
                         i < count - 1 && word_a[i].start != 0; i++)
                    {
                    }

                    if (word_a[i].str != NULL)
                      win.start = i;

                    if (win.end < count - 1)
                    {
                      for (i = win.end + 2;
                           i < count - 1 && word_a[i].start != 0; i++)
                      {
                      }
                      if (word_a[i].str != NULL)
                        win.end = i;
                    }
                  }

                /* In column mode we need to take care of the */
                /* horizontal scrolling                       */
                /* """""""""""""""""""""""""""""""""""""""""" */
                if (win.col_mode | win.line_mode)
                {
                  if (word_a[current].is_last)
                    win.first_column = 0;
                  else
                  {
                    int pos;
                    int len;

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
            }
            while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current = old_current;
              win.start = old_start;
            }

            if (current != old_current)
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
            break;
          }
          else
            goto special_cmds_when_searching;

          /* PgUp key has been pressed */
          /* """"""""""""""""""""""""" */
        kpp:
          page = win.max_lines;

          /* Cursor Up key has been pressed */
          /* """""""""""""""""""""""""""""" */
        ku:
        case 'K':
        case 'k':
          if (!search_mode)
          {
            int cur_line;
            int start_line;
            int last_word;
            int cursor;
            int old_current = current;
            int old_start = win.start;
            int index = 0;

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
              cur_line = line_nb_of_word_a[current];

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

              /* In case no word fullfill this criterium, keep the cursor on */
              /* the last word                                               */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (cursor == last_word && word_a[cursor].start > 0)
                cursor--;

              /* Try to guess the best choice if we have muliple choices */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (word_a[cursor].end >= s &&
                  word_a[cursor].end - s >= e - word_a[cursor + 1].start)
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
              set_new_first_column(&win, &term, word_a);

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
            }
            while (current != old_current && !word_a[current].is_selectable);

            /* If no selectable word could be find; stay at the original */
            /* position                                                  */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (!word_a[current].is_selectable)
            {
              current = old_current;
              win.start = old_start;
            }

            /* Display the window */
            /* """""""""""""""""" */
            if (current != old_current)
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);

            break;
          }
          else
            goto special_cmds_when_searching;

          /* PgDn key has been pressed */
          /* """"""""""""""""""""""""" */
        knp:
          page = win.max_lines;

          /* Cursor Down key has been pressed */
          /* """""""""""""""""""""""""""""""" */
        kd:
        case 'J':
        case 'j':
          if (!search_mode)
          {
            int cur_line;
            int start_line;
            int last_word;
            int cursor;
            int old_current = current;
            int old_start = win.start;
            int index;

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
              cur_line = line_nb_of_word_a[current];

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
                    win.start = first_word_in_line_a[start_line];
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

              /* In case no word fullfill this criterium, keep the cursor on */
              /* the last word                                               */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
              if (cursor == last_word && word_a[cursor].start > 0)
                cursor--;

              /* Try to guess the best choice if we have muliple choices */
              /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
              set_new_first_column(&win, &term, word_a);

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
            }
            while (current != old_current && !word_a[current].is_selectable);

            if (!word_a[current].is_selectable)
            {
              current = old_current;
              win.start = old_start;
            }

            /* Display the window */
            /* """""""""""""""""" */
            if (current != old_current)
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);

            break;
          }
          else
            goto special_cmds_when_searching;

          /* / or CTRL-F key has been pressed (start of a search session) */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        case 0x06:
        case '/':
          if (!search_mode)
          {
            search_mode = 1;

            /* Arm the search timer to 7s */
            /* """""""""""""""""""""""""" */
            search_itv.it_value.tv_sec = 7;
            search_itv.it_value.tv_usec = 0;
            search_itv.it_interval.tv_sec = 0;
            search_itv.it_interval.tv_usec = 0;
            setitimer(ITIMER_REAL, &search_itv, NULL);

            /* Clear the search buffer when the cursor is on a word which */
            /* doesn't match the current prefix                           */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
            if (search_pos > 0 && !strprefix(word_a[current].str, search_buf))
            {
              memset(search_buf, '\0', tab_real_max_size);
              search_pos = 0;
            }

            nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                            search_buf, &term, last_line, tmp_max_word,
                            &langinfo);
            break;
          }
          else
            goto special_cmds_when_searching;

          /* Backspace and CTRL-H */
          /* """""""""""""""""""" */
        case 0x08:          /* ^H */
        case 0x7f:          /* BS */
          if (search_mode)
          {
            if (*search_buf != '\0')
            {
              char *new_search_buf;

              int pos;

              new_search_buf = xcalloc(1, tab_real_max_size + 1);
              mb_strprefix(new_search_buf, search_buf,
                           mbstowcs(0, search_buf, 0) - 1, &pos);

              free(search_buf);
              search_buf = new_search_buf;
              search_pos = pos;

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
            }
          }
          break;

          /* Help mode */
          /* """"""""" */
        case '?':
          if (!search_mode)
          {
            help(&win, &term, last_line);
            help_mode = 1;

            /* Arm the help timer to 15s */
            /* """"""""""""""""""""""""" */
            hlp_itv.it_value.tv_sec = 15;
            hlp_itv.it_value.tv_usec = 0;
            hlp_itv.it_interval.tv_sec = 0;
            hlp_itv.it_interval.tv_usec = 0;
            setitimer(ITIMER_REAL, &hlp_itv, NULL);
          }
          break;

        special_cmds_when_searching:
        default:
        {
          int c;

          if (search_mode)
          {
            int old_pos = search_pos;

            /* Re-arm the search timer to 5s */
            /* """"""""""""""""""""""""""""" */
            search_itv.it_value.tv_sec = 5;
            search_itv.it_value.tv_usec = 0;
            search_itv.it_interval.tv_sec = 0;
            search_itv.it_interval.tv_usec = 0;
            setitimer(ITIMER_REAL, &search_itv, NULL);

            /* Copy all the bytes included in the keypress to buffer */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
            for (c = 0; c < l && search_pos < tab_real_max_size; c++)
              search_buf[search_pos++] = buffer[c];

            if (search_next(tst, word_a, search_buf, 0))
            {
              if (current > win.end)
                last_line = build_metadata(word_a, &term, count, &win);

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo);
            }
            else
            {
              search_pos = old_pos;
              search_buf[search_pos] = '\0';
            }
          }
        }
      }
    }
  }
}
