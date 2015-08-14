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
#include <errno.h>
#include <wchar.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "smenu.h"

int bar_color;               /* scrollbar color                  */
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

/* UTF-8 usefull symbols */
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
  int del_line;              /* 1 if the clean option is set (-d) else 0  */
  int enter_val_in_search;   /* 1 if ENTER validate in search mode else 0 */
  int no_scrollbar;          /* 1 to disable the scrollbar display else 0 */
  int blank_nonprintable;    /* 1 to try to display non-blanks in         *
                              * symbolic form else 0                      */
};

/* Mapping of supported charset and the number of bits used in them */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

/* Structure containing some terminal characteristics */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
struct term_s
{
  int ncolumns;              /* number of columns                      */
  int nlines;                /* number of lines                        */
  int curs_column;           /* current cursor column                  */
  int curs_line;             /* current cursor line                    */
  int colors;                /* number of available colors             */

  int has_setf;              /* has set_foreground terminfo capability */
  int has_setb;              /* has set_background terminfo capability */
  int has_hpa;               /* has column_address terminfo capability */
};

/* structure describing a word */
/* """"""""""""""""""""""""""" */
struct word_s
{
  char *str;                 /* display string                              */
  char *orig;                /* NULL or original string if is had been      *
                              * shortenend to being displayed  or altered   *
                              * by is expansion.                            */
  int is_last;               /* 1 if the word it the last of a line         */
  int start, end, mbytes;    /* start/end absolute horizontal word position *
                              * on the screen                               *
                              * mbytes number of multibytes to display      */
};

/* structure describing the window in which we will be able to select a word */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct win_s
{
  int start, end;
  int count;
  int first_column;
  size_t cur_line;
  int asked_max_lines, max_lines, max_cols;
  int col_sep;
  int tab_mode;
  int col_mode;
  int wide_columns;
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

/* linked list node structure */
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
usage(char *prog)
{
  fprintf(stderr, "Usage: %s [-h] [-n lines] [-c cols] [-s pattern] ", prog);
  fprintf(stderr, "[-m message] \\\n       [-w] [-d] [-t] [-e] [-b] [-g]\n");
  fprintf(stderr, "\nThis is a filter that gets words from stdin ");
  fprintf(stderr, "and outputs the\n");
  fprintf(stderr, "selected word (or nothing) on stdout.\n");
  fprintf(stderr, "A selection window is displayed on /dev/tty ");
  fprintf(stderr, "to ease that.\n");
  fprintf(stderr, "\n-h displays this help.\n");
  fprintf(stderr, "-n sets the number of lines in the selection window.\n");
  fprintf(stderr, "-c sets the number of columns in the selection window. ");
  fprintf(stderr, "implies -t.\n");
  fprintf(stderr, "-s sets the initial cursor position (read the manual for ");
  fprintf(stderr, "details).\n");
  fprintf(stderr, "-m displays a one-line message above the window\n");
  fprintf(stderr, "-w uses all the terminal width for the columns.\n");
  fprintf(stderr, "-d deletes the selection window on exit.\n");
  fprintf(stderr, "-t tabulates the items in the selection window.\n");
  fprintf(stderr,
          "-e enables ENTER to validate the selection even "
          "in search mode.\n");
  fprintf(stderr, "-b displays the non printable characters as space.\n");
  fprintf(stderr, "-g separates columns with '|' in tabulate mode.\n");
  fprintf(stderr, "-q prevents the scrollbar display.\n");
  fprintf(stderr, "-V displays the current version of this program.\n");
  fprintf(stderr, "\nNavigation keys are:\n");
  fprintf(stderr, "  Left/Down/Up/Right arrows or h/j/k/l\n");
  fprintf(stderr, "  Home/End\n");
  fprintf(stderr, "  SPACE to search for the next match of a previously\n");
  fprintf(stderr, "        entered search prefix, if any, see below\n");
  fprintf(stderr, "\nExit key without output: q\n");
  fprintf(stderr, "Selection key          : ENTER\n");
  fprintf(stderr, "Cancel key             : ESC\n");
  fprintf(stderr, "Search key             : / or CTRL-F\n");
  fprintf(stderr, "\nThe search key activates a timed search mode in which\n");
  fprintf(stderr, "you can enter the first letters of the searched word.\n");
  fprintf(stderr, "When entering this mode you have 7s to start typing\n");
  fprintf(stderr, "and each entered letter gives you 5 more seconds before\n");
  fprintf(stderr, "the timeout. After that the search mode is ended.\n");
  fprintf(stderr, "Notes:\n");
  fprintf(stderr, "- the timer can be cancelled by pressing ESC.\n");
  fprintf(stderr, "- a bad search letter can be removed with ");
  fprintf(stderr, "CTRL-H or Backspace\n");
  fprintf(stderr, "- ENTER and SPACE/n can be used even in search mode.\n");
  fprintf(stderr, "\n(C) Pierre Gentile (2015).\n");

  exit(EXIT_FAILURE);
}

/* ==================== */
/* Help message display */
/* ==================== */
void
help(void)
{
  tputs(save_cursor, 1, outch);
  tputs(enter_reverse_mode, 1, outch);
  fputs("HLP", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs(" ", stdout);

  fputs("Move:", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("Arrows", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("|", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("h", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("j", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("k", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("l", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs(",", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("PgUp", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("PgDn", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("Home", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("/", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("End", stdout);
  tputs(exit_attribute_mode, 1, outch);

  fputs(" Cancel:", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("q", stdout);
  tputs(exit_attribute_mode, 1, outch);

  fputs(" Confirm:", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("CR", stdout);
  tputs(exit_attribute_mode, 1, outch);

  fputs(" Search:", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("/", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("|", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("^F", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("|", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("SP", stdout);
  tputs(exit_attribute_mode, 1, outch);
  fputs("|", stdout);
  tputs(enter_bold_mode, 1, outch);
  fputs("n", stdout);
  tputs(exit_attribute_mode, 1, outch);

  tputs(clr_eol, 1, outch);
  tputs(exit_attribute_mode, 1, outch);
  tputs(restore_cursor, 1, outch);
}

/* ********************* */
/* Linked List functions */
/* ********************* */

/* ==================================== */
/* Function to create a new linked list */
/* ==================================== */
ll_t *
ll_new(void)
{
  ll_t *ret = malloc(sizeof(ll_t));

  if (ret)
    ll_init(ret);
  else
    errno = ENOMEM;

  return ret;
}

/* ======================== */
/* initialize a linked list */
/* ======================== */
void
ll_init(ll_t * list)
{
  list->head = NULL;
  list->tail = NULL;
  list->len = 0;
}

/* ==================================================== */
/* allocate the space for a new node in the linked list */
/* ==================================================== */
ll_node_t *
ll_new_node(void)
{
  ll_node_t *ret = malloc(sizeof(ll_node_t));

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

/* ================================================ */
/* Sets the terminal in non echo/non canonical mode */
/* wait for at least one byte, no timeout.          */
/* ================================================ */
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

/* ====================================== */
/* Sets the terminal in its previous mode */
/* ====================================== */
void
restore_term(int const fd)
{
  tcsetattr(fd, TCSANOW, &old_in_attrs);
}

/* =============================================== */
/* Gets the terminal numbers of lines and columns  */
/* Assume that the TIOCGWINSZ, ioctl is available  */
/* Defaults to 80x24                               */
/* =============================================== */
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

/* ======================================================= */
/* Gets cursor position the terminal                       */
/* Assumes that the Escape sequence ESC [ 6 n is available */
/* ======================================================= */
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

/* ============================================================= */
/* Removes all ANSI color codes from s and puts the result in d. */
/* Memory space for d must have been allocated before.           */
/* ============================================================= */
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
        ;                    /* do nothing */
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
  /* If the whole string has been stripped, leave a signe dot */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (s - p == len)
    *p++ = '.';

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

/* =============================================================== */
/* Thank you Neil (https://github.com/sheredom)                    */
/* Returns 0 if the UTF-8 sequence is valid or the position of the */
/* invalid utf8 codepoint on failure.                              */
/* =============================================================== */
void *
validate_mb(const void *str)
{
  const char *s = (const char *) str;

  while ('\0' != *s)
  {
    if (0xf0 == (0xf8 & *s))
    {
      /* ensure each of the 3 following bytes in this 4-byte */
      /* utf8 codepoint began with 0b10xxxxxx                */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0x80 != (0xc0 & s[1])) || (0x80 != (0xc0 & s[2]))
          || (0x80 != (0xc0 & s[3])))
      {
        return (void *) s;
      }

      /* ensure that our utf8 codepoint ended after 4 bytes */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[4]))
      {
        return (void *) s;
      }

      /* ensure that the top 5 bits of this 4-byte utf8   */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0 == (0x07 & s[0])) && (0 == (0x30 & s[1])))
      {
        return (void *) s;
      }

      /* 4-byte utf8 code point (began with 0b11110xxx) */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      s += 4;
    }
    else if (0xe0 == (0xf0 & *s))
    {
      /* ensure each of the 2 following bytes in this 3-byte */
      /* utf8 codepoint began with 0b10xxxxxx                */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0x80 != (0xc0 & s[1])) || (0x80 != (0xc0 & s[2])))
      {
        return (void *) s;
      }

      /* ensure that our utf8 codepoint ended after 3 bytes */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[3]))
      {
        return (void *) s;
      }

      /* ensure that the top 5 bits of this 3-byte utf8   */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if ((0 == (0x0f & s[0])) && (0 == (0x20 & s[1])))
      {
        return (void *) s;
      }

      /* 3-byte utf8 code point (began with 0b1110xxxx) */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
      s += 3;
    }
    else if (0xc0 == (0xe0 & *s))
    {
      /* ensure the 1 following byte in this 2-byte */
      /* utf8 codepoint began with 0b10xxxxxx       */
      /* """""""""""""""""""""""""""""""""""""""""" */
      if (0x80 != (0xc0 & s[1]))
      {
        return (void *) s;
      }

      /* ensure that our utf8 codepoint ended after 2 bytes */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (0x80 == (0xc0 & s[2]))
      {
        return (void *) s;
      }

      /* ensure that the top 4 bits of this 2-byte utf8   */
      /* codepoint were not 0, as then we could have used */
      /* one of the smaller encodings                     */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if (0 == (0x1e & s[0]))
      {
        return (void *) s;
      }

      /* 2-byte utf8 code point (began with 0b110xxxxx) */
      /* """""""""""""""""""""""""""""""""""""""""""""" */
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
      /* we have an invalid 0b1xxxxxxx utf8 code point entry */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
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

/* ============================================================ */
/* Converts a multibyte (UTF-8) char string to a wchar_t string */
/* ============================================================ */
wchar_t *
mb_strtowcs(char *s)
{
  int converted = 0;
  unsigned char *ch;
  wchar_t *wptr, *w;
  int size;

  size = strlen(s);
  w = malloc((size + 1) * sizeof(wchar_t));
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
    p = (tst_node_t *) malloc(sizeof(struct tst_node_s));
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

/* ======================================================================= */
/* callback function used by tst_traverse                                  */
/* iterates the linked list attached to the string containing the index of */
/* the words in the input flow. each page number is then used to detemine  */
/* the lower page greater than the cursor position                         */
/* ----------------------------------------------------------------------- */
/* Requires new_current to be set to count - 1 at start                    */
/* Updates new_current to the smallest greater position than current       */
/* ======================================================================= */
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

/* ======================================================================= */
/* callback function used by tst_traverse                                  */
/* iterates the linked list attached to the string containing the index of */
/* the words in the input flow. each page number is then used to detemine  */
/* the lower page greater than the cursor position                         */
/* ----------------------------------------------------------------------- */
/* This is a special version of tst_cb wich permit to find the first word. */
/* ----------------------------------------------------------------------- */
/* Requires new_current to be set to count - 1 at start                    */
/* Updates new_current to the smallest greater position than current       */
/* ======================================================================= */
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

/* ================================================= */
/* Non delay reading of a scancode                   */
/* updates a scancodes buffer aand return its length */
/* in bytes                                          */
/* ================================================= */
int
get_scancode(unsigned char *s, int max)
{
  int c;
  int i = 1;
  struct termios original_ts, nowait_ts;

  if ((c = getchar()) == EOF)
    return 0;

  /* initialize the string withe the first byte */
  /* """""""""""""""""""""""""""""""""""""""""" */
  memset(s, '\0', max);
  s[0] = c;

  /* Code 0x1b (ESC) found, proceed to check if additional codes */
  /* are available                                               */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
      /* There isn't new code, this mean 0x1b came from ESC key pression */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
  if (validate_mb(mb_buffer) != 0)
  {
    byte = mb_buffer[0] = '.';
    mb_buffer[1] = '\0';
  }

  return byte;
}

/* ==========================================================================*/
/* expand the string str by replacing all its embedded special characters by */
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
          ;                  /* skip this byte. */
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
        case '\"':
          *(ptr++) = '\\';
          *(ptr++) = '\"';
          len += 2;
          break;
        case '\'':
          *(ptr++) = '\\';
          *(ptr++) = '\'';
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

  /* If the word contains only spaces, replaces them */
  /* by underscores so that it can be seen           */
  /* """"""""""""""""""""""""""""""""""""""""""""""" */
  if (all_spaces)
    memset(dest, '_', len);

  *ptr = '\0';               /* Ensure that dest has a nul terminator */

  return len;
}

/* ===================================================================== */
/* get_word(input): return a char pointer to the next word (as a string) */
/* Accepts: a FILE * for the input stream                                */
/* Returns: a char *                                                     */
/*    On Success: the return value will point to a nul-terminated string */
/*    On Failure: the return value will be set to NULL                   */
/* ===================================================================== */
char *
get_word(FILE * input, ll_t * word_delims_list, ll_t * record_delims_list,
         char *mb_buffer, int *is_last, toggle_t * toggle,
         langinfo_t * langinfo)
{
  char *temp = NULL;
  int byte, count = 0;       /* counts chars used in current allocation */
  int wordsize;              /* size of current allocation in chars     */

  /* skip leading delimiters */
  /* """"""""""""""""""""""" */
  for (byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo);
       byte == EOF || ll_find(word_delims_list, mb_buffer, delims_cmp) != NULL;
       byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo))
  {
    if (byte == EOF)
      return NULL;
  }

  /* allocate initial word storage space */
  /* """"""""""""""""""""""""""""""""""" */
  if ((temp = malloc(wordsize = CHARSCHUNK)) == NULL)
    return NULL;

  /* start stashing bytes. stop when we meet a non delimiter */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (count = 0;
       byte != EOF && ll_find(word_delims_list, mb_buffer, delims_cmp) == NULL;
       byte = get_bytes(input, mb_buffer, word_delims_list, toggle, langinfo))
  {
    int i = 0;

    while (mb_buffer[i] != '\0')
    {
      if (count >= wordsize - 1)
      {
        char *temp2;

        if ((temp2 = realloc(temp, wordsize +=
                             (count / CHARSCHUNK + 1) * CHARSCHUNK)) == NULL)
          return NULL;
        else
          temp = temp2;
      }
      *(temp + count++) = mb_buffer[i];
      i++;
    }
  }

  /* nul-terminate the word, to make it a string */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  *(temp + count) = '\0';

  /* skip all field delimiters before a record delimiter */
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

      /* FIXME We cannot assume we can ungetc more than one byte        */
      /* This seems to work with Linux/glibc though at least fo 4 bytes */

      pos = strlen(mb_buffer);
      while (pos > 0)
        ungetc(mb_buffer[--pos], stdin);

      /* FIXME ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
    }
    else
      ungetc(byte, stdin);
  }

  /* Mark it as the last record word if its */
  /* sequence matches a record delimiter    */
  /* """""""""""""""""""""""""""""""""""""" */
  if (byte == EOF || ll_find(record_delims_list, mb_buffer, delims_cmp) != NULL)
    *is_last = 1;
  else
    *is_last = 0;

  /* Remove the ANSI color escape sequences from the word */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  strip_ansi_color(temp, toggle);

  return temp;
}

/* ======================================================= */
/* Puts a scrolling symbol at the first column of the line */
/* ======================================================= */
void
left_margin_putp(char *s, term_t * term)
{
  tputs(enter_bold_mode, 1, outch);
  if (bar_color > 0 && term->has_setf)
    tputs(tparm(set_foreground, bar_color), 1, outch);

  /* we won't print this symbol when not in column mode */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  if (*s != '\0')
    fputs(s, stdout);

  tputs(exit_attribute_mode, 1, outch);
}

/* ====================================================== */
/* Puts a scrolling symbol at the last column of the line */
/* ====================================================== */
void
right_margin_putp(char *s1, char *s2, langinfo_t * langinfo, term_t * term,
                  int line)
{
  tputs(enter_bold_mode, 1, outch);

  if (bar_color > 0 && term->has_setf)
    tputs(tparm(set_foreground, bar_color), 1, outch);

  if (term->has_hpa)
    tputs(tparm(column_address, term->ncolumns - 1), 1, outch);
  else
    tputs(tparm(cursor_address,
                term->curs_line + line - 2, term->ncolumns - 1), 1, outch);

  if (langinfo->utf8)
    fputs(s1, stdout);
  else
    fputs(s2, stdout);

  tputs(exit_attribute_mode, 1, outch);
}

/* ************** */
/* Core functions */
/* ************** */

/* ========================================================================== */
/* Sets the metadata associated with a word, its starting and ending position */
/* the line in which it is put and so on                                      */
/* sets win.start win.end ans the starting and ending position of each word.  */
/* This fonction is only called initially, when resizing the terminal and     */
/* potentially when the seah function is used.                                */
/* ========================================================================== */
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
  wchar_t *w;

  line_nb_of_word_a[0] = 0;
  first_word_in_line_a[0] = 0;

  /* Modify the max number of displayed lines if we do not have */
  /* enough place                                               */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win->asked_max_lines > term->nlines - 1)
    win->max_lines = term->nlines - 1;
  else
    win->max_lines = win->asked_max_lines;

  while (i < count)
  {
    /* Determine the number of screen position used by the word */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    word_len = mbstowcs(0, word_a[i].str, 0);
    word_width = wcswidth((w = mb_strtowcs(word_a[i].str)), word_len);

    /* Manage the case where the word is larger than the terminal width */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_width >= term->ncolumns - 2)
    {
      /* Shorten the word until it fits */
      /* '''''''''''''''''''''''''''''' */
      do
      {
        word_width = wcswidth(w, word_len--);
      }
      while (word_len > 0 && word_width >= term->ncolumns - 2);
    }
    free(w);

    /* Look if there is enough remaining place on the line when not in column */
    /* mode or if we hit a record delimiter in column mode                    */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if ((!win->col_mode && (len + word_width + 1) >= term->ncolumns - 1)
        || (win->col_mode && i > 0 && word_a[i - 1].is_last))
    {
      /* We must build another line */
      /* """""""""""""""""""""""""" */
      line_nb_of_word_a[i] = ++last;
      first_word_in_line_a[last] = i;
      word_a[i].start = 0;

      len = word_width + 1;
      word_a[i].end = word_width - 1;
      word_a[i].mbytes = word_len + 1;
    }
    else
    {
      word_a[i].start = len;
      word_a[i].end = word_a[i].start + word_width - 1;
      word_a[i].mbytes = word_len + 1;
      line_nb_of_word_a[i] = last;
      len += word_width + 1;
    }

    i++;
  }

  /* we need to recalculate win->start and win->end here */
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

/* ======================================================================= */
/* Displays a word in, the windows. Manages the following different cases: */
/* - Search mode display                                                   */
/* - Cursor display                                                        */
/* - Normal display                                                        */
/* - Color or mono display                                                 */
/* ======================================================================= */
void
disp_word(word_t * word_a, int pos, int search_mode, char *buffer,
          term_t * term, char *tmp_max_word)
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

      /* set the search cursor attribute */
      /* """"""""""""""""""""""""""""""" */
      if (term->colors > 7 && term->has_setf && term->has_setb)
      {
        tputs(tparm(set_background, 4), 1, outch);
        tputs(tparm(set_foreground, 0), 1, outch);
      }
      else
      {
        tputs(enter_underline_mode, 1, outch);
        tputs(enter_reverse_mode, 1, outch);
      }

      mb_strprefix(tmp_max_word, word_a[pos].str, word_a[pos].mbytes - 1, &p);
      fputs(tmp_max_word, stdout);

      /* set the buffer display attribute */
      /* """""""""""""""""""""""""""""""" */
      if (term->colors > 7 && term->has_setf && term->has_setb)
        tputs(tparm(set_foreground, 7), 1, outch);
      else
        tputs(enter_bold_mode, 1, outch);

      /* Overwrite the beginning of the word with the search buffer */
      /* content if it is not empty                                 */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (buffer[0] != '\0')
      {
        int i = 0;

        int buf_width;

        /* Calculates the space taken by the buffer on screen */
        /* '''''''''''''''''''''''''''''''''''''''''''''''''' */
        buf_width = wcswidth((w = mb_strtowcs(buffer)), mbstowcs(0, buffer, 0));
        free(w);

        /* Puts the cursor at the beginning of the word */
        /* '''''''''''''''''''''''''''''''''''''''''''' */
        for (i = 0; i < e - s + 1; i++)
          tputs(cursor_left, 1, outch);

        fputs(buffer, stdout);

        /* Puts back the cursor after the word */
        /* ''''''''''''''''''''''''''''''''''' */
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
    fputs(tmp_max_word, stdout);
  }
}

/* ======================================== */
/* Displays a message line above the window */
/* ======================================== */
int
disp_message(char *message, term_t * term)
{
  int message_lines = 0;

  if (message)
  {
    char *ptr;

    ptr = strchr(message, '\n');
    if (ptr)
      *ptr = '\0';

    message_lines = strlen(message) / term->ncolumns + 1;
    tputs(enter_bold_mode, 1, outch);
    fputs(message, stdout);
    tputs(exit_attribute_mode, 1, outch);
    tputs(clr_eol, 1, outch);
    puts("");
  }
  return message_lines;
}

/* ============================= */
/* Displays the selection window */
/* ============================= */
int
disp_lines(word_t * word_a, win_t * win, toggle_t * toggle, int current,
           int count, int search_mode, char *search_buf, term_t * term,
           int last_line, char *tmp_max_word, langinfo_t * langinfo,
           int cols_size)
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

  if (win->col_mode)
    len = term->ncolumns - 3;
  else
    len = term->ncolumns - 2;

  /* If in column mode and the sum of the columns sizes + gutters is      */
  /* greater than the terminal width,  then prepend a space to be able to */
  /* display the left arrow indicating that the first displayed column    */
  /* is not the first one.                                                */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (len > 1 && win->col_mode && cols_size > term->ncolumns - 2)
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

  left_margin_putp(scroll_symbol, term);
  while (len > 1 && i <= count - 1)
  {
    /* Display one word and the space ou symbol following it */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (word_a[i].start >= win->first_column
        && word_a[i].end < len + win->first_column)
    {
      disp_word(word_a, i, search_mode, search_buf, term, tmp_max_word);

      /* If there are more element to be displayed after the right margin */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      if (win->col_mode && i < count - 1
          && word_a[i + 1].end >= len + win->first_column)
      {
        tputs(enter_bold_mode, 1, outch);

        if (bar_color > 0 && term->has_setf)
          tputs(tparm(set_foreground, bar_color), 1, outch);

        if (langinfo->utf8)
          fputs(sbar_arr_right, stdout);
        else
          fputs(">", stdout);

        tputs(exit_attribute_mode, 1, outch);
      }

      /* If we want to display the gutter */
      /* '''''''''''''''''''''''''''''''' */
      else if (win->col_sep && win->tab_mode)
      {
        if (langinfo->utf8)
          fputs(sbar_broken_line, stdout);
        else
          fputs("|", stdout);
      }
      /* Else just display a space */
      /* ''''''''''''''''''''''''' */
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
              right_margin_putp(sbar_top, "\\", langinfo, term, lines_disp);
            else
              right_margin_putp(sbar_arr_down, "^", langinfo, term, lines_disp);
          }
          else if (lines_disp == 1)
            right_margin_putp(sbar_arr_up, "^", langinfo, term, lines_disp);
          else if (line_nb_of_word_a[i] == last_line)
          {
            if (win->max_lines > 1)
              right_margin_putp(sbar_down, "/", langinfo, term, lines_disp);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, lines_disp);
          }
          else if (last_line + 1 > win->max_lines
                   && (int) ((float) (line_nb_of_word_a[current])
                             / (last_line + 1) * (win->max_lines - 2) + 2)
                   == lines_disp)
            right_margin_putp(sbar_curs, "+", langinfo, term, lines_disp);
          else
            right_margin_putp(sbar_line, "|", langinfo, term, lines_disp);
        }

        /* print a newline character if we are not at the end of */
        /* the input nor the window                              */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (i < count - 1 && lines_disp < win->max_lines)
        {
          puts("");
          left_margin_putp(scroll_symbol, term);
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
              right_margin_putp(sbar_down, "/", langinfo, term, lines_disp);
            else
              right_margin_putp(sbar_arr_up, "^", langinfo, term, lines_disp);
          }
        }
        else
        {
          if (!toggle->no_scrollbar)
            right_margin_putp(sbar_arr_down, "v", langinfo, term, lines_disp);
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
      /* Standards Terminaison signals */
      /* """"""""""""""""""""""""""""" */
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
char *optarg;                /* argument associated with option */
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
    write(opterrfd, *nargv, strlen(*nargv)); \
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

    TELL(": illegal option -- ");       /* byebye */
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
        TELL(": option requires an argument -- ");
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
set_new_first_column(win_t * win, word_t * word_a)
{
  if (word_a[current].start < win->first_column)
  {
    int pos = current;

    while (win->first_column > 0 && word_a[current].start < win->first_column)
      win->first_column = word_a[pos--].start;
  }
}

/* ================ */
/* Main entry point */
/* ================ */
int
main(int argc, char *argv[])
{
  char *message = NULL;      /* One line message to be desplayed           *
                              * above the selection window                 */
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
  toggle_t toggle;
  word_t *word_a, *ptr;      /* Array contanings words data (size: count)  */

  int old_fd1;               /* backups of the old stdout file descriptor  */
  FILE *old_stdout;          /* The selecterd word will go there           */

  int nl;                    /* Number of lines displayed in the window    */
  int offset;                /* Used to correstly put the curson at the    *
                              * start of the selection window, even after  *
                              * a terminal vertical scroll                 */
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
  int cols_size = 0;         /* Space taken by all the columns plus the    *
                              * gutters in column mode                     */
  int col_index;             /* Index of the current column when reading   *
                              * words, used in column mode                 */
  int cols_number;           /* Number of columns in column mode           */

  char *pre_selection_index = NULL;     /* pattern used to set the initial *
                                         * cursor position                 */
  unsigned char *buffer = malloc(16);   /* Input buffer                    */

  char *search_buf = NULL;   /* Search buffer                              */
  int search_pos = 0;        /* Current position in the search buffer      */

  struct sigaction sa;       /* Signal structure                           */

  char *ifs, *irs;
  ll_t *word_delims_list = NULL;
  ll_t *record_delims_list = NULL;

  char mb_buffer[5];         /* buffer to store the bytes of a             *
                              * multibyte character (4 chars max)          */
  int is_last;
  char *charset;

  charsetinfo_t *charset_ptr;
  langinfo_t langinfo;
  int is_supported_charset;

  /* win fields initialization */
  /* """"""""""""""""""""""""" */
  win.max_lines = 5;
  win.asked_max_lines = 5;
  win.max_cols = 0;
  win.col_sep = 0;
  win.wide_columns = 0;
  win.tab_mode = 0;
  win.col_mode = 0;
  win.first_column = 0;

  /* toggles initialization */
  /* """""""""""""""""""""" */
  toggle.del_line = 0;
  toggle.enter_val_in_search = 0;
  toggle.no_scrollbar = 0;
  toggle.blank_nonprintable = 0;

  /* Gets the current locale */
  /* """"""""""""""""""""""" */
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
  /* version of isprint accorging to the content of utf8            */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (langinfo.utf8 || langinfo.bits == 8)
    my_isprint = isprint8;
  else
    my_isprint = isprint7;

  /* Command line options analysis */
  /* """"""""""""""""""""""""""""" */
  opterr = 0;
  while ((opt = egetopt(argc, argv, "Vhqdbcwegn:t%:m:s:")) != -1)
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
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;

      case 'd':
        toggle.del_line = 1;
        break;

      case 's':
        if (optarg && *optarg != '-')
          pre_selection_index = strdup(optarg);
        else
        {
          usage(argv[0]);
          exit(EXIT_FAILURE);
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

      case 'g':
        win.col_sep = 1;
        break;

      case 'w':
        win.wide_columns = 1;
        break;

      case 'm':
        message = optarg;
        break;

      case 'e':
        toggle.enter_val_in_search = 1;
        break;

      case 'b':
        toggle.blank_nonprintable = 1;
        break;

      case 'q':
        toggle.no_scrollbar = 1;
        break;

      case '?':
        fprintf(stderr, "Invalid option (%s).\n", argv[optind - 1]);
        exit(EXIT_FAILURE);

      case 'h':
      default:              /* '?' */
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    optarg = NULL;
  }

  if (optind < argc)
  {
    fprintf(stderr, "Invalid option (%s).\n", argv[argc - 1]);
    exit(EXIT_FAILURE);
  }

  if (optind < argc)
  {
    fprintf(stderr, "Invalid option (%s).\n", argv[argc - 1]);
    exit(EXIT_FAILURE);
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
  term.has_setb = (tigetstr("setb") == 0) ? 0 : 1;
  term.has_setf = (tigetstr("setf") == 0) ? 0 : 1;
  term.has_hpa = (tigetstr("hpa") == 0) ? 0 : 1;

  get_terminal_size(&term.nlines, &term.ncolumns);

  /* Sets the scrollbar color if possible */
  /* """""""""""""""""""""""""""""""""""" */
  bar_color = -1;

  if (term.colors > 7)
    bar_color = 2;

  /* Allocate the memory for out words structures */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  word_a = malloc(WORDSCHUNK * sizeof(word_t));

  /* Fill an array of word_t elements obtained from stdin */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  tab_real_max_size = 0;
  tab_max_size = 0;
  min_size = 0;

  //sleep(15);
  /* Parse the IFS environment variable to set the delimiters. If IFS   */
  /* is not set, then the standard delimiters (space, tab and EOL) are  */
  /* used. Each of its multibyte sequences are stored in a linked list. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  word_delims_list = ll_new();
  ifs = getenv("IFS");

  if (ifs == NULL)
  {
    ll_append(word_delims_list, " ");
    ll_append(word_delims_list, "\t");
    ll_append(word_delims_list, "\n");
  }
  else
  {
    int mb_len;
    char *ifs_ptr = ifs;
    char *tmp;

    mb_len = mblen(ifs_ptr, 4);

    while (mb_len != 0)
    {
      tmp = malloc(mb_len + 1);
      memcpy(tmp, ifs_ptr, mb_len);
      tmp[mb_len] = '\0';
      ll_append(word_delims_list, tmp);

      ifs_ptr += mb_len;
      mb_len = mblen(ifs_ptr, 4);
    }
  }

  /* Parse the IRS environment variable to set the delimiters. If IRS   */
  /* is not set, then the standard delimiters (space, tab and EOL) are  */
  /* used. Each of its multibyte sequences are stored in a linked list. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  record_delims_list = ll_new();
  irs = getenv("IRS");

  if (irs == NULL)
    ll_append(record_delims_list, "\n");
  else
  {
    int mb_len;
    char *irs_ptr = irs;
    char *tmp;

    mb_len = mblen(irs_ptr, 4);

    while (mb_len != 0)
    {
      tmp = malloc(mb_len + 1);
      memcpy(tmp, irs_ptr, mb_len);
      tmp[mb_len] = '\0';
      ll_append(record_delims_list, tmp);

      /* Add this record delimiter as a word delimiter */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      if (ll_find(word_delims_list, tmp, delims_cmp) == NULL)
        ll_append(word_delims_list, tmp);

      irs_ptr += mb_len;
      mb_len = mblen(irs_ptr, 4);
    }
  }

  /* Initialize the first chunks of the arrays which will contain the */
  /* maximum length of each column in column mode                     */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (win.col_mode)
  {
    col_real_max_size = malloc(COLSCHUNK * sizeof(int));
    col_max_size = malloc(COLSCHUNK * sizeof(int));

    for (i = 0; i < COLSCHUNK; i++)
      col_real_max_size[i] = col_max_size[i] = 0;

    col_index = cols_number = 0;
  }

  /* Get and process the input stream words */
  /* """""""""""""""""""""""""""""""""""""" */
  while ((word = get_word(stdin, word_delims_list, record_delims_list,
                          mb_buffer, &is_last, &toggle, &langinfo)) != NULL)
  {
    int size;
    int *data;
    wchar_t *w = mb_strtowcs(word);
    wchar_t *tmpw;
    int s;
    ll_t *list = NULL;
    char *dest = malloc(5 * strlen(word) + 1);
    char *new_dest;
    size_t len;
    size_t word_len;

    /* Alter the word just read be replacing special chars  by their */
    /* escaped equivalents.                                          */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    len = expand(word, dest, &langinfo);

    /* Reduce the memory footprint of dest and store its new length */
    /* after its potential expansion.                               */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    new_dest = strdup(dest);
    free(dest);
    dest = new_dest;

    word_len = len;

    /* In column mode each column have their own max size, so me need to */
    /* maintain an array of the max column size for them                 */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode)
    {
      col_index++;

      if (col_index > cols_number)
      {
        cols_number++;

        /* We have a new column, see if we need to enlarge the arrays */
        /* indexed on the number of columns                           */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (cols_number % COLSCHUNK == 0)
        {
          int *tmp;
          int i;

          tmp = realloc(col_real_max_size,
                        (cols_number + COLSCHUNK) * sizeof(int));

          if (tmp == NULL)
          {
            perror("realloc");
            exit(EXIT_FAILURE);
          }
          else
            col_real_max_size = tmp;

          tmp = realloc(col_max_size, (cols_number + COLSCHUNK) * sizeof(int));

          if (tmp == NULL)
          {
            perror("realloc");
            exit(EXIT_FAILURE);
          }
          else
            col_max_size = tmp;

          /* Initialize the max size for the new columns */
          /* """"""""""""""""""""""""""""""""""""""""""" */
          for (i = 0; i < COLSCHUNK; i++)
          {
            col_real_max_size[cols_number + i] = 0;
            col_max_size[cols_number + i] = 0;
          }
        }
      }

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

    if (strcmp(word, dest) != 0)
      word_a[count].orig = word;
    else
    {
      word_a[count].orig = NULL;
      free(word);
    }

    /* Set the last word in line indicator when in column mode */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (win.col_mode)
      word_a[count].is_last = is_last;
    else
      word_a[count].is_last = 0;

    data = malloc(sizeof(int));
    *data = count;

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

    /* One more word... */
    /* """""""""""""""" */
    count++;
    if (count % WORDSCHUNK == 0)
    {
      ptr = realloc(word_a, (count + WORDSCHUNK) * sizeof(word_t));

      if (ptr == NULL)
      {
        perror("malloc");
        exit(EXIT_FAILURE);
      }

      word_a = ptr;
    }
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
  line_nb_of_word_a = malloc(count * sizeof(int));
  first_word_in_line_a = malloc(count * sizeof(int));

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
      cols_size += col_max_size[col_index] + 1;

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
      temp = calloc(1, col_real_max_size[col_index] + s1 - s2 + 1);
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
      temp = calloc(1, tab_real_max_size + s1 - s2 + 1);
      memset(temp, ' ', tab_max_size + s1 - s2);
      memcpy(temp, word_a[i].str, s1);
      temp[tab_real_max_size + s1 - s2] = '\0';
      free(word_a[i].str);
      word_a[i].str = temp;
    }
  }

  word_a[count].str = NULL;

  if (win.col_mode && cols_size < term.ncolumns - 3)
    term.ncolumns = cols_size + 3;
  else
  {
    if (!win.wide_columns
        && win.max_cols * (tab_max_size + 1) + 2 <= term.ncolumns)
      term.ncolumns = win.max_cols * (tab_max_size + 1) + 2;
  }

  /* We can now allocate the space for our tmp_max_word work variable */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  tmp_max_word = malloc(tab_real_max_size + 1);

  win.start = 0;             /* index of the first element in the    *
                              * words array to be  displayed         */

  /* Index of the selected element in the array words                */
  /* Ths string can be:                                              */
  /*   "last"    The string "last"   put the cursor on the last word */
  /*   n         a number            put the cursor on th word n     */
  /*   /pref     /+a regexp          put the cursor on the first     */
  /*                                 word matching the prefix "pref" */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (pre_selection_index == NULL)
    current = 0;
  else if (strcmp(pre_selection_index, "last") == 0)
    current = count - 1;
  else if (*pre_selection_index == '/')
  {
    wchar_t *w;

    new_current = count - 1;
    if (NULL != tst_prefix_search(tst, w = mb_strtowcs(pre_selection_index + 1),
                                  tst_cb_cli))
      current = new_current;
    else
      current = 0;
    free(w);
  }
  else if (*pre_selection_index != '\0')
  {
    current = atoi(pre_selection_index);
    if (current >= count)
      current = count - 1;
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

  /* Sets the caracteristics of the terminal */
  /* """"""""""""""""""""""""""""""""""""""" */
  setup_term(fileno(stdin));

  /* Save the cursor line column */
  /* """"""""""""""""""""""""""" */
  get_cursor_position(&term.curs_line, &term.curs_column);

  /* Initialize the search buffer with tab_real_max_size+1 NULs */
  /* It wil never be reallocated, only cleared.                 */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  search_buf = calloc(1, tab_real_max_size + 1);

  /* Hide the cursor */
  /* """"""""""""""" */
  tputs(cursor_invisible, 1, outch);

  /* Display the words window for the first time */
  /* """"""""""""""""""""""""""""""""""""""""""" */
  message_lines = disp_message(message, &term);
  nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                  search_buf, &term, last_line, tmp_max_word, &langinfo,
                  cols_size);

  /* Determine the number of line to move the cursor up if the window display */
  /* needed a terminel scrolling                                              */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (nl + term.curs_line - 1 > term.nlines)
    offset = term.curs_line + nl - term.nlines;
  else
    offset = 0;

  /* Set the cursor to the first line of the window */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  for (i = 1; i < offset; i++)
    tputs(cursor_up, 1, outch);

  /* Save again the cursor line and column */
  /* """"""""""""""""""""""""""""""""""""" */
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
                      search_buf, &term, last_line, tmp_max_word, &langinfo,
                      cols_size);
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
                        search_buf, &term, last_line, tmp_max_word, &langinfo,
                        cols_size);
      }
    }

    /* If the terminal has been resized */
    /* """""""""""""""""""""""""""""""" */
    if (got_winch)
    {
      /* Re-arm winch timer to 10s */
      /* """"""""""""""""""""""""" */
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

      if (win.col_mode && cols_size < term.ncolumns - 3)
        term.ncolumns = cols_size + 3;
      else
      {
        if (!win.wide_columns
            && win.max_cols * (tab_max_size + 1) + 2 <= term.ncolumns)
          term.ncolumns = win.max_cols * (tab_max_size + 1) + 2;
      }

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

      if (win.col_mode)
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

      message_lines = disp_message(message, &term);
      nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                      search_buf, &term, last_line, tmp_max_word, &langinfo,
                      cols_size);

      /* Get new cursor position */
      /* """"""""""""""""""""""" */
      get_cursor_position(&term.curs_line, &term.curs_column);

      /* Determine the number of line to move the cursor up if the window  */
      /* display needed a terminel scrolling                               */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
            /* HOME key pressed */
            /* """""""""""""""" */
            if (memcmp("\x1bOH", buffer, 3) == 0)
            {
              current = win.start;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode)
              {
                int pos;

                pos = first_word_in_line_a[line_nb_of_word_a[current]];
                win.first_column = word_a[pos].start;
              }

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo, cols_size);
              break;
            }

            /* END key pressed */
            /* """"""""""""""" */
            if (memcmp("\x1bOF", buffer, 3) == 0)
            {
              current = win.end;

              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode)
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
                              tmp_max_word, &langinfo, cols_size);
              break;
            }

            /* Left arrow key pressed */
            /* """""""""""""""""""""" */
            if (memcmp("\x1bOD", buffer, 3) == 0
                || memcmp("\x1b[D", buffer, 3) == 0)
              goto kl;

            /* Right arrow key pressed */
            /* """"""""""""""""""""""" */
            if (memcmp("\x1bOC", buffer, 3) == 0
                || memcmp("\x1b[C", buffer, 3) == 0)
              goto kr;

            /* Up arrow key pressed */
            /* """""""""""""""""""" */
            if (memcmp("\x1bOA", buffer, 3) == 0
                || memcmp("\x1b[A", buffer, 3) == 0)
              goto ku;

            /* Down arrow key pressed */
            /* """""""""""""""""""""" */
            if (memcmp("\x1bOB", buffer, 3) == 0
                || memcmp("\x1b[B", buffer, 3) == 0)
              goto kd;

            /* PgUp key pressed */
            /* """""""""""""""" */
            if (memcmp("\x1b[5~", buffer, 4) == 0)
              goto kpp;

            /* PgDn key pressed */
            /* """""""""""""""" */
            if (memcmp("\x1b[6~", buffer, 4) == 0)
              goto knp;
          }

          /* ESC key pressed */
          /* """"""""""""""" */
          if (memcmp("\x1b", buffer, 1) == 0)
          {
            if (search_mode || help_mode)
            {
              search_mode = 0;
              help_mode = 0;
              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo, cols_size);
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
                          tmp_max_word, &langinfo, cols_size);
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
                            &langinfo, cols_size);

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

            /* Cleans the printed line and all the extra lines used. */
            /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
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

        kl:
          /* Cursor Left key has been pressed pressed */
          /* """""""""""""""""""""""""""""""""""""""" */
        case 'H':
        case 'h':
          if (!search_mode)
          {
            if (current == win.start)
              if (win.start > 0)
              {
                for (i = win.start - 1; i >= 0 && word_a[i].start != 0; i--)
                  ;
                win.start = i;
              }

            if (current > 0)
            {
              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode)
              {
                int pos;

                if (word_a[current].start == 0)
                {
                  int len;

                  len = term.ncolumns - 3;
                  pos = first_word_in_line_a[line_nb_of_word_a[current - 1]];

                  while (word_a[current - 1].end - win.first_column >= len)
                  {
                    win.first_column += word_a[pos].end - word_a[pos].start + 2;
                    pos++;
                  }
                }
                else if (word_a[current - 1].start < win.first_column)
                  win.first_column = word_a[current - 1].start;
              }

              current--;

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo, cols_size);
            }
            break;
          }
          else
            goto special_cmds_when_searching;

        kr:
          /* Right key has been pressed pressed */
          /* """""""""""""""""""""""""""""""""" */
        case 'L':
        case 'l':
          if (!search_mode)
          {
            if (current == win.end)
              if (win.start < count - 1 && win.end != count - 1)
              {
                for (i = win.start + 1; i < count - 1 && word_a[i].start != 0;
                     i++)
                  ;
                if (word_a[i].str != NULL)
                  win.start = i;
              }

            if (current < count - 1)
            {
              /* In column mode we need to take care of the */
              /* horizontal scrolling                       */
              /* """""""""""""""""""""""""""""""""""""""""" */
              if (win.col_mode)
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

                    /* If the new current word cannot be displayed, search */
                    /* the first word in the line that can be displayed by */
                    /* iterating on pos.                                   */
                    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
                    pos--;

                    while (word_a[current + 1].end - word_a[pos].start >= len)
                      pos++;

                    if (word_a[pos].start > 0)
                      win.first_column = word_a[pos].start;
                  }
                }
              }

              current++;

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo, cols_size);
            }
            break;
          }
          else
            goto special_cmds_when_searching;

        kpp:
          /* PgUp key has been pressed pressed */
          /* """"""""""""""""""""""""""""""""" */
          page = win.max_lines;

        ku:
          /* Cursor Up key has been pressed pressed */
          /* """""""""""""""""""""""""""""""""""""" */
        case 'K':
        case 'k':
          if (!search_mode)
          {
            int cur_line;
            int start_line;
            int last_word;
            int cursor;

            /* Store the initial starting and ending position of */
            /* the word under the cursor                         */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            s = word_a[current].start;
            e = word_a[current].end;

            /* Identify the line number of the first line of the window */
            /* and the line number of the current line                  */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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

            /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
            if (word_a[cursor].end - s >= e - word_a[cursor + 1].start)
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
            set_new_first_column(&win, word_a);

            /* Display the window */
            /* """""""""""""""""" */
            nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                            search_buf, &term, last_line, tmp_max_word,
                            &langinfo, cols_size);

            break;
          }
          else
            goto special_cmds_when_searching;

        knp:
          /* PgDn key has been pressed pressed */
          /* """"""""""""""""""""""""""""""""" */
          page = win.max_lines;

        kd:
          /* Cursor Down key has been pressed pressed */
          /* """""""""""""""""""""""""""""""""""""""" */
        case 'J':
        case 'j':
          if (!search_mode)
          {
            int cur_line;
            int start_line;
            int last_word;
            int cursor;

            /* Store the initial starting and ending position of */
            /* the word under the cursor                         */
            /* """"""""""""""""""""""""""""""""""""""""""""""""" */
            s = word_a[current].start;
            e = word_a[current].end;

            /* Identify the line number of the first line of the window */
            /* and the line number of the current line                  */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
            set_new_first_column(&win, word_a);

            nl = disp_lines(word_a, &win, &toggle, current, count, search_mode,
                            search_buf, &term, last_line, tmp_max_word,
                            &langinfo, cols_size);

            break;
          }
          else
            goto special_cmds_when_searching;

          /* / or CTRL-F key pressed (start of a search session) */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
        case 6:
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
                            &langinfo, cols_size);
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

              new_search_buf = calloc(1, tab_real_max_size + 1);
              mb_strprefix(new_search_buf, search_buf,
                           mbstowcs(0, search_buf, 0) - 1, &pos);

              free(search_buf);
              search_buf = new_search_buf;
              search_pos = pos;

              nl = disp_lines(word_a, &win, &toggle, current, count,
                              search_mode, search_buf, &term, last_line,
                              tmp_max_word, &langinfo, cols_size);
            }
          }
          break;

        case '?':
          if (!search_mode)
          {
            help();
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
                              tmp_max_word, &langinfo, cols_size);
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
