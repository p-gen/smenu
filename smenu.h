/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef SMENU_H
#define SMENU_H

#include <stdio.h>
#include <regex.h>
#include <termios.h>
#include "utf8.h"
#include "list.h"

#define CHARSCHUNK 8
#define WORDSCHUNK 8
#define COLSCHUNK 16

#define TPARM1(p) tparm(p, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define TPARM2(p, q) tparm(p, q, 0, 0, 0, 0, 0, 0, 0, 0)
#define TPARM3(p, q, r) tparm(p, q, r, 0, 0, 0, 0, 0, 0, 0)

/* Used for timers management. */
/* """"""""""""""""""""""""""" */
#define SECOND 1000000
#define FREQ 10
#define TCK (SECOND / FREQ)

/* Large bit array management written by           */
/* Scott Dudley, Auke Reitsma and Bob Stout.       */
/* Assumes CHAR_BIT is one of either 8, 16, or 32. */
/* Public domain.                                  */
/* """"""""""""""""""""""""""""""""""""""""""""""" */
#define MASK (CHAR_BIT - 1)
#define SHIFT ((CHAR_BIT == 8) ? 3 : (CHAR_BIT == 16) ? 4 : 8)

#define BIT_OFF(a, x) ((void)((a)[(x) >> SHIFT] &= ~(1 << ((x) & MASK))))
#define BIT_ON(a, x) ((void)((a)[(x) >> SHIFT] |= (1 << ((x) & MASK))))
#define BIT_FLIP(a, x) ((void)((a)[(x) >> SHIFT] ^= (1 << ((x) & MASK))))
#define BIT_ISSET(a, x) ((a)[(x) >> SHIFT] & (1 << ((x) & MASK)))

/* ********* */
/* Typedefs. */
/* ********* */

typedef struct charsetinfo_s     charsetinfo_t;
typedef struct term_s            term_t;
typedef struct toggle_s          toggle_t;
typedef struct win_s             win_t;
typedef struct word_s            word_t;
typedef struct attrib_s          attrib_t;
typedef struct attrib_ex_s       attrib_ex_t;
typedef struct limit_s           limit_t;
typedef struct ticker_s          ticker_t;
typedef struct misc_s            misc_t;
typedef struct mouse_s           mouse_t;
typedef struct sed_s             sed_t;
typedef struct timeout_s         timeout_t;
typedef struct output_s          output_t;
typedef struct daccess_s         daccess_t;
typedef struct search_data_s     search_data_t;
typedef struct attr_elem_s       attr_elem_t;
typedef struct help_entry_s      help_entry_t;
typedef struct help_attr_entry_s help_attr_entry_t;

/* ****** */
/* Enums. */
/* ****** */

/* Various filter pseudo constants. */
/* """""""""""""""""""""""""""""""" */
typedef enum filter_types
{
  UNKNOWN_FILTER,
  INCLUDE_FILTER,
  EXCLUDE_FILTER
} filters_t;

/* Types of selectors. */
/* """"""""""""""""""" */
typedef enum selector_types
{
  IN,      /* Inclusion. */
  EX,      /* Exclusion. */
  ALEFT,   /* Alignment to the left. */
  ARIGHT,  /* Alignment to the right. */
  ACENTER, /* Alignment to the center. */
  ATTR     /* Attribute. */
} selector_t;

/* Used by the -N -F and -D options. */
/* """"""""""""""""""""""""""""""""" */
typedef enum daccess_modes
{
  DA_TYPE_NONE = 0, /* must be 0 (boolean value). */
  DA_TYPE_AUTO = 1,
  DA_TYPE_POS  = 2
} da_mode_t;

/* Various timeout mode used by the -x/-X option. */
/* """""""""""""""""""""""""""""""""""""""""""""" */
typedef enum timeout_modes
{
  CURRENT, /* on timeout, outputs the selected word.       */
  QUIT,    /* on timeout, quit without selecting anything. */
  WORD     /* on timeout , outputs the specified word.     */
} to_mode_t;

/* Constants used to set the color attributes. */
/* """"""""""""""""""""""""""""""""""""""""""" */
typedef enum attribute_settings
{
  UNSET = 0, /* must be 0 for future testings. */
  SET,
  FORCED /* an attribute setting has been given in the command line. */
} attr_set_t;

/* Method used to interpret the color numbers. */
/* """"""""""""""""""""""""""""""""""""""""""" */
typedef enum color_method
{
  CLASSIC,
  ANSI
} color_method_t;

/* Constant to distinguish between the various search modes. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
typedef enum search_modes
{
  NONE,
  PREFIX,
  FUZZY,
  SUBSTRING
} search_mode_t;

/* Constants used in search mode to orient the bit-mask building. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
typedef enum bitmap_affinities
{
  NO_AFFINITY,
  END_AFFINITY,
  START_AFFINITY
} bitmap_affinity_t;

typedef enum alignment
{
  AL_NONE,
  AL_LEFT,
  AL_RIGHT,
  AL_CENTERED
} alignment_t;

/* Used when managing the -C option. */
/* """"""""""""""""""""""""""""""""" */
enum
{
  EXCLUDE_MARK      = 0, /* must be 0 because used in various tests      *
                          | these words cannot be re-included.           */
  INCLUDE_MARK      = 1, /* to forcibly include a word, these words can  *
                          | be excluded later.                           */
  SOFT_EXCLUDE_MARK = 2, /* word with this mark are excluded by default  *
                          | but can be included later.                   */
  SOFT_INCLUDE_MARK = 3  /* word with this mark are included by default  *
                          | but can be excluded later.                   */
};

/* Mouse protocols. */
/* """""""""""""""" */
enum
{
  MOUSE1000, /* VT200          */
  MOUSE1006, /* SGR_EXT_MODE   */
  MOUSE1015  /* URXVT_EXT_MODE */
};

/* ******* */
/* Structs */
/* ******* */

/* Used to store the different allowed charsets data. */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
struct charsetinfo_s
{
  char *name; /* canonical name of the allowed charset. */
  int   bits; /* number of bits in this charset.        */
};

/* Various toggles which can be set with command line options. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct toggle_s
{
  int del_line;            /* 1 if the clean option is set (-d) else 0.   */
  int enter_val_in_search; /* 1 if ENTER validates in search mode else 0. */
  int no_scrollbar;        /* 1 to disable the scrollbar display else 0.  */
  int no_hor_scrollbar;    /* 1 to disable the horizontab scrollbar       *
                            | display else 0.                             */
  int hor_scrollbar;       /* 1 to always enable the scrollbar display    *
                            * when lines are truncated in line/col mode   *
                            * else 0.                                     */
  int blank_nonprintable;  /* 1 to try to display non-blanks in           *
                            | symbolic form else 0.                       */
  int keep_spaces;         /* 1 to keep the trailing spaces in columns    *
                            | and tabulate mode.                          */
  int taggable;            /* 1 if tagging is enabled.                    */
  int pinable;             /* 1 if pinning is selected.                   */
  int autotag;             /* 1 if tagging is selected and pinning is     *
                            | not and we do no want an automatic tagging  *
                            | when the users presses <ENTER>.             */
  int noautotag;           /* 1 if the word under the cursor must not be  *
                            | autotagged when no other word are tagged.   */
  int visual_bell;         /* 1 to flash the window, 0 to make a sound.   */
  int incremental_search;  /* 1 makes the searching process incremental.  *
                            | 0 keeps it forgetful.                       */
  int no_mouse;            /* 1 to disable the possibly auto-detected     *
                            | mouse, 0 to let smenu auto-detect it.       */
  int show_blank_words;    /* 1 if blank words are allowed then they will *
                            | be filled by an underscore, 0 to leave them *
                            | blank.                                      */
  int cols_first;          /* 1 if column alignment has priority over     *
                            | row alignment else 0.                       */
  int rows_first;          /* 1 if row alignment has priority over        *
                            | column alignment else 0.                    */
};

/* Structure to store the default or imposed smenu limits. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct limit_s
{
  long word_length; /* maximum number of bytes in a word. */
  long words;       /* maximum number of words.           */
  long cols;        /* maximum number of columns.         */
};

/* Structure to store the default or imposed timers. */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
struct ticker_s
{
  int search;
  int forgotten;
  int help;
  int winch;
  int direct_access;
};

/* Structure to store miscellaneous information. */
/* """"""""""""""""""""""""""""""""""""""""""""" */
struct misc_s
{
  search_mode_t default_search_method;
  char          invalid_char_substitute;
  char          blank_char_substitute;
  char          ignore_quotes;
};

/* Structure to store mouse information. */
/* """"""""""""""""""""""""""""""""""""" */
struct mouse_s
{
  int double_click_delay;
  int button[3];
};

/* Structure containing the attributes components. */
/* """"""""""""""""""""""""""""""""""""""""""""""" */
struct attrib_s
{
  attr_set_t  is_set;
  short       fg;
  short       bg;
  signed char bold;
  signed char dim;
  signed char reverse;
  signed char standout;
  signed char no_standout;
  signed char underline;
  signed char no_underline;
  signed char italic;
  signed char no_italic;
  signed char invis;
  signed char blink;
  signed char reset;
};

/* Structure used when displaying attributes in the message lines. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct attrib_ex_s
{
  attrib_t *attr;
  int       start; /* Offset of the start of attr in the message line. */
  int       end;   /* Offset of the end of attr in the message line.   */
};

/* Structure containing some terminal characteristics. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""" */
struct term_s
{
  int            ncolumns;     /* number of columns.                     */
  int            nlines;       /* number of lines.                       */
  int            curs_column;  /* current cursor column.                 */
  int            curs_line;    /* current cursor line.                   */
  short          colors;       /* number of available colors.            */
  color_method_t color_method; /* color method (CLASSIC (0-7), ANSI).    */

  char has_cursor_up;         /* has cuu1 terminfo capability.           */
  char has_cursor_down;       /* has cud1 terminfo capability.           */
  char has_cursor_left;       /* has cub1 terminfo capability.           */
  char has_cursor_right;      /* has cuf1 terminfo capability.           */
  char has_parm_right_cursor; /* has cuf terminfo capability.            */
  char has_cursor_address;    /* has cup terminfo capability.            */
  char has_save_cursor;       /* has sc terminfo capability.             */
  char has_restore_cursor;    /* has rc terminfo capability.             */
  char has_setf;              /* has set_foreground terminfo capability. */
  char has_setb;              /* has set_background terminfo capability. */
  char has_setaf;             /* idem for set_a_foreground (ANSI).       */
  char has_setab;             /* idem for set_a_background (ANSI).       */
  char has_hpa;               /* has column_address terminfo capability. */
  char has_bold;              /* has bold mode.                          */
  char has_dim;               /* has dim mode.                           */
  char has_reverse;           /* has reverse mode.                       */
  char has_underline;         /* has underline mode.                     */
  char has_no_underline;      /* has exit underline.                     */
  char has_standout;          /* has standout mode.                      */
  char has_no_standout;       /* has exit standout.                      */
  char has_italic;            /* has italic mode.                        */
  char has_no_italic;         /* has exit italic.                        */
  char has_invis;             /* has invis mode.                         */
  char has_blink;             /* has blink mode.                         */
  char has_kmous;             /* has mouse reporting.                    */
  char has_rep;               /* has repeat char.                        */
};

/* Structure describing a word. */
/* """""""""""""""""""""""""""" */
struct word_s
{
  long           start, end;    /* start/end absolute horiz. word positions *
                                 | on the screen.                           */
  size_t         mb;            /* number of UTF-8 glyphs to display.       */
  size_t         len_mb;        /* number of UTF-8 glyphs before filling    *
                                 * the column.                              */
  size_t         len;           /* number of bytes in str (for trimming).   */
  char          *str;           /* display string associated with this word */
  char          *orig;          /* NULL or original string if is had been.  *
                                 | shortened for being displayed or altered *
                                 | by is expansion.                         */
  char          *bitmap;        /* used to store the position of the.       *
                                 | currently searched chars in a word. The  *
                                 | objective is to speed their display.     */
  attrib_t      *iattr;         /* Specific attribute set with the -Ra/-Ca  *
                                 | options.                                 */
  long           tag_order;     /* each time a word is tagged, this value.  *
                                 | is increased.                            */
  unsigned short tag_id;        /* tag id. 0 means no tag.                  */
  unsigned char  is_matching;   /* word is matching a search ERE.           */
  unsigned char  is_last;       /* 1 if the word is the last of a line.     */
  unsigned char  is_selectable; /* word is selectable.                      */
  unsigned char  is_numbered;   /* word has a direct access index.          */
  unsigned char  special_level; /* can vary from 0 to 9; 0 meaning normal.  */
  int            offset;        /* may be > 0 in case of center or right    *
                                 * alignment (# of spaces added).           */
};

/* Structure describing the window in which the user  */
/* will be able to select a word.                     */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
struct win_s
{
  long     start, end;      /* index of the first and last word.        */
  int      first_column;    /* number of the first character displayed. */
  int      cur_line;        /* relative number of the cursor line (1+). */
  int      asked_max_lines; /* requested number of lines in the window. */
  int      has_hbar;        /* 1 if an horizontal bar must be           *
                             | displayed else 0.                        */
  int      hbar_displayed;  /* 1 if an hozizontal bas has ever been     *
                             | displayed else 0.                        */
  int      max_lines;       /* effective number of lines in the window. */
  int      max_cols;        /* max number of words in a single line.    */
  int      real_max_width;  /* max line length. In column, tab or line  *
                             | mode it can be greater than the          *
                             | terminal width.                          */
  int      message_lines;   /* number of lines taken by the messages    *
                             | (updated by disp_message.                */
  int      max_width;       /* max usable line width or the terminal.   */
  int      offset;          /* Left margin, used in centered mode.      */
  char    *sel_sep;         /* output separator when tags are enabled.  */
  char   **gutter_a;        /* array of UTF-8 gutter glyphs.            */
  int      gutter_nb;       /* number of UTF-8 gutter glyphs.           */
  int      sb_column;       /* scroll bar column (-1) if no scroll bar. */
  unsigned next_tag_id;     /* Next tag ID, increased on each tagging   *
                             | operation.                               */

  unsigned char tab_mode;  /* -t */
  unsigned char col_mode;  /* -c */
  unsigned char line_mode; /* -l */
  unsigned char col_sep;   /* -g */
  unsigned char wide;      /* -w */
  unsigned char center;    /* -M */

  unsigned char has_truncated_lines; /* 1 if win has tr. lines else 0. */

  attrib_t cursor_attr;               /* current cursor attributes.          */
  attrib_t cursor_marked_attr;        /* current cursor when a mark is set.  */
  attrib_t cursor_on_marked_attr;     /* current cursor on marked word       *
                                       | attributes.                         */
  attrib_t cursor_on_tag_attr;        /* current cursor on tag attributes.   */
  attrib_t cursor_on_tag_marked_attr; /* current cursor on tag attributes    *
                                       | if current word is marked.          */
  attrib_t marked_attr;               /* marked word.                        */
  attrib_t bar_attr;                  /* scrollbar attributes.               */
  attrib_t shift_attr;                /* shift indicator attributes.         */
  attrib_t message_attr;              /* message (title) attributes.         */
  attrib_t search_field_attr;         /* search mode field attributes.       */
  attrib_t search_text_attr;          /* search mode text attributes.        */
  attrib_t search_err_field_attr;     /* bad search mode field attributes.   */
  attrib_t search_err_text_attr;      /* bad search mode text attributes.    */
  attrib_t match_field_attr;          /* matching word field attributes.     */
  attrib_t match_text_attr;           /* matching word text attributes.      */
  attrib_t match_err_field_attr;      /* bad matching word field attributes. */
  attrib_t match_err_text_attr;       /* bad matching word text attributes.  */
  attrib_t include_attr;              /* selectable words attributes.        */
  attrib_t exclude_attr;              /* non-selectable words attributes.    */
  attrib_t tag_attr;                  /* non-selectable words attributes.    */
  attrib_t daccess_attr;              /* direct access tag attributes.       */
  attrib_t special_attr[9];           /* special (-1,...) words attributes.  */
};

/* Sed like node structure. */
/* """""""""""""""""""""""" */
struct sed_s
{
  char         *pattern;      /* pattern to be matched.                    */
  char         *substitution; /* substitution string.                      */
  regex_t       re;           /* compiled regular expression.              */
  unsigned char visual;       /* visual flag: alterations are only visual. */
  unsigned char global;       /* global flag: alterations can occur more   *
                               | than once.                                */
  unsigned char stop;         /* stop flag:   only one alteration per word *
                               | is allowed.                               */
};

/* Structure used to keep track of the different timeout values. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct timeout_s
{
  to_mode_t mode;          /* timeout mode: current/quit/word. */
  unsigned  initial_value; /* 0: no timeout else value in sec. */
  unsigned  remain;        /* remaining seconds.               */
  unsigned  reached;       /* 1: timeout has expired, else 0.  */
};

/* Structure used during the construction of the pinned words list. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct output_s
{
  long  order;      /* this field is incremented each time a word is *
                     | pinned.                                       */
  char *output_str; /* The pinned word itself.                       */
};

/* Structure describing the formatting of the automatic */
/* direct access entries.                               */
/* """""""""""""""""""""""""""""""""""""""""""""""""""" */
struct daccess_s
{
  char     *left;       /* character to put before the direct access       *
                         | selector.                                       */
  char     *right;      /* character to put after the direct access        *
                         | selector.                                       */
  char     *num_sep;    /* character to separate number and selection.     */
  size_t    offset;     /* offset to the start of the selector.            */
  size_t    ignore;     /* number of UTF-8 glyphs to ignore after the      *
                         | number.                                         */
  da_mode_t mode;       /* DA_TYPE_NONE (0), DA_TYPE_AUTO, DA_TYPE_POS.    */
  int       length;     /* selector size (5 max).                          */
  int       flength;    /* 0 or length + 3 (full prefix length.            */
  int       plus;       /* 1 if we can look for the number to extract      *
                         | after the offset, else 0. (a '+' follows the    *
                         | offset).                                        */
  int       size;       /* size in bytes of the selector to extract.       */
  int       def_number; /* 1: the numbering is on by default 0: it is not. */
  char      alignment;  /* l: left; r: right.                              */
  char      padding;    /* a: all; i: only included words are padded.      */
  char      head;       /* What to do with chars before the embedded       *
                         | number.                                         */
  char      missing;    /* y: number missing embedded numbers.             */
  char      follow;     /* y: the numbering follows the last number set.   */
};

/* Structure used in search mod to store the current buffer and various   */
/* related values.                                                        */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct search_data_s
{
  char *buf;    /* search buffer.                                */
  long  len;    /* current position in the search buffer.        */
  long  mb_len; /* current position in the search buffer in      *
                 | UTF-8 units.                                  */
  long *off_a;  /* array of mb offsets in buf.                   */
  long *len_a;  /* array of mb lengths in buf.                   */

  long fuzzy_err_pos; /* last good position in search buffer.    */
  int  err;           /* match error indicator.                  */

  int only_ending;   /* only searches giving a result with the.  *
                      | pattern at the end of the word will be   *
                      | selected.                                */
  int only_starting; /* same with the pattern at the beginning.  */
};

/* Structure used to store an attribute and the list of elements      */
/* (columns, rows or RE) for which this attribute must be the default */
/* one.                                                               */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct attr_elem_s
{
  attrib_t *attr; /* Attribute                           */
  ll_t     *list; /* list of RE or intervals of columns. */
};

struct help_entry_s
{
  char *str; /* string to be displayed for an object in the help line. */
  int   len; /* screen space taken by of one of these objects.         */
  char *main_attr_str; /* Style attribute when colors are available.   */
  char *alt_attr_str;  /* Style attribute when colors are missing.     */
  char *flags;         /* string which can only contain occurrences of *
                        | c: only on column/line mode,                 *
                        | t: only in tag mode,                         *
                        | m: -no_mouse is set.                         */
};

struct help_attr_entry_s
{
  char     *str;  /* string to be displayed for an object in the help line. */
  attrib_t *attr; /* attribute.                                             */
  int       len;  /* screen space taken by of one of these objects.         */
};

/* *********** */
/* Prototypes. */
/* *********** */

attrib_t *
attr_new(void);

int
help_add_entries(win_t               *win,
                 term_t              *term,
                 toggle_t            *toggles,
                 help_attr_entry_t ***help_items_da,
                 help_entry_t        *entries,
                 int                  entries_nb);

help_attr_entry_t ***
init_help(win_t               *win,
          term_t              *term,
          toggle_t            *toggles,
          long                 last_line,
          help_attr_entry_t ***help_items_da);

void
disp_help(win_t               *win,
          term_t              *term,
          help_attr_entry_t ***help_da,
          int                  first_help_line);

int
tag_comp(void const *a, void const *b);

void
tag_swap(void **a, void **b);

int
isempty(const char *s);

void
clear_bitmap(word_t *word);

void
my_beep(toggle_t *toggles);

void
align_word(word_t *word, alignment_t alignment, size_t prerfix, char sp);

int
get_cursor_position(int * const r, int * const c);

void
get_terminal_size(int * const r, int * const c, term_t *term);

int
#ifdef __sun
outch(char c);
#else
outch(int c);
#endif

void
restore_term(int const fd, struct termios *old);

void
setup_term(int const fd, struct termios *old, struct termios *new);

void
strip_ansi_color(char *s, toggle_t *toggles, misc_t *misc);

int
tst_cb(void *elem);

int
tst_cb_cli(void *elem);

int
ini_parse(char     *filename,
          win_t    *win,
          term_t   *term,
          limit_t  *limits,
          ticker_t *timers,
          misc_t   *misc,
          mouse_t  *mouse);

int
ini_parse_entry(win_t      *win,
                term_t     *term,
                limit_t    *limits,
                ticker_t   *timers,
                misc_t     *misc,
                mouse_t    *mouse,
                const char *section,
                const char *parameter,
                char       *value);

char *
make_ini_path(char *name, char *base);

short
color_transcode(short color);

void
set_foreground_color(term_t *term, short color);

void
set_background_color(term_t *term, short color);

void
set_win_start_end(win_t *win, long current, long last);

long
build_metadata(term_t *term, long count, win_t *win);

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
           langinfo_t    *langinfo);

void
get_message_lines(char *message,
                  ll_t *message_lines_list,
                  long *message_max_width,
                  long *message_max_len);

void
disp_message(ll_t       *message_lines_list,
             long        width,
             long        max_len,
             term_t     *term,
             win_t      *win,
             langinfo_t *langinfo);

int
check_integer_constraint(int nb_args, char **args, char *value, char *par);

void
update_bitmaps(search_mode_t     search_mode,
               search_data_t    *search_data,
               bitmap_affinity_t affinity);

long
find_next_matching_word(long *array, long nb, long value, long *index);

long
find_prev_matching_word(long *array, long nb, long value, long *index);

void
clean_matches(search_data_t *search_data, long size);

void
disp_cursor_word(long pos, win_t *win, term_t *term, int err);

void
disp_matching_word(long pos, win_t *win, term_t *term, int is_cursor, int err);

void
disp_word(long           pos,
          search_mode_t  search_mode,
          search_data_t *search_data,
          term_t        *term,
          win_t         *win,
          toggle_t      *toggles,
          char          *tmp_word);

size_t
expand(char       *src,
       char       *dest,
       langinfo_t *langinfo,
       toggle_t   *toggles,
       misc_t     *misc);

int
read_bytes(FILE       *input,
           char       *utf8_buffer,
           ll_t       *ignored_glyphs_list,
           langinfo_t *langinfo,
           misc_t     *misc);

int
get_scancode(unsigned char *s, size_t max);

char *
read_word(FILE          *input,
          ll_t          *word_delims_list,
          ll_t          *line_delims_list,
          ll_t          *ignored_glyphs_list,
          char          *utf8_buffer,
          unsigned char *is_last,
          toggle_t      *toggles,
          langinfo_t    *langinfo,
          win_t         *win,
          limit_t       *limits,
          misc_t        *misc);

void
left_margin_putp(char *s, term_t *term, win_t *win);

void
right_margin_putp(char       *s1,
                  char       *s2,
                  langinfo_t *langinfo,
                  term_t     *term,
                  win_t      *win,
                  long        line,
                  long        offset);

void
sig_handler(int s);

void
set_new_first_column(win_t *win, term_t *term);

int
parse_sed_like_string(sed_t *sed);

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
                alignment_t *al_default,
                win_t       *win,
                misc_t      *misc,
                term_t      *term);

void
parse_al_selectors(char        *str,
                   char       **unparsed,
                   ll_t       **al_regex_list,
                   ll_t       **ar_regex_list,
                   ll_t       **ac_regex_list,
                   alignment_t *default_alignment,
                   misc_t      *misc);
int
replace(char *orig, sed_t *sed);

int
decode_attr_toggles(char *s, attrib_t *attr);

int
parse_attr(char *str, attrib_t *attr, short max_color);

void
apply_attr(term_t *term, attrib_t attr);

long
get_line_last_word(long line, long last_line);

void
move_left(win_t         *win,
          term_t        *term,
          toggle_t      *toggles,
          search_data_t *search_data,
          langinfo_t    *langinfo,
          long          *nl,
          long           last_line,
          char          *tmp_word);

void
move_right(win_t         *win,
           term_t        *term,
           toggle_t      *toggles,
           search_data_t *search_data,
           langinfo_t    *langinfo,
           long          *nl,
           long           last_line,
           char          *tmp_word);

int
find_best_word_upwards(long last_word, long s, long e);

int
find_best_word_downwards(long last_word, long s, long e);

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
        char          *tmp_word);

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
          char          *tmp_word);

void
init_main_ds(attrib_t  *init_attr,
             win_t     *win,
             limit_t   *limits,
             ticker_t  *timers,
             toggle_t  *toggles,
             misc_t    *misc,
             mouse_t   *mouse,
             timeout_t *timeout,
             daccess_t *daccess);

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
                    long           word_real_max_size);
#endif

long
get_clicked_index(win_t  *win,
                  term_t *term,
                  int     line_click,
                  int     column_click,
                  int    *error);
