/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

#ifndef SMENU_H
#define SMENU_H

#define CHARSCHUNK 8
#define WORDSCHUNK 8
#define COLSCHUNK 16

#define TPARM1(p) tparm(p, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define TPARM2(p, q) tparm(p, q, 0, 0, 0, 0, 0, 0, 0, 0)
#define TPARM3(p, q, r) tparm(p, q, r, 0, 0, 0, 0, 0, 0, 0)

#define _XOPEN_SOURCE 700

/* Used for timers management */
/* """""""""""""""""""""""""" */
#define SECOND 1000000
#define FREQ 10
#define TCK (SECOND / FREQ)

/* Bit array management */
/* """""""""""""""""""" */
#define MASK (CHAR_BIT - 1)
#define SHIFT ((CHAR_BIT == 8) ? 3 : (CHAR_BIT == 16) ? 4 : 8)

#define BIT_OFF(a, x) ((void)((a)[(x) >> SHIFT] &= ~(1 << ((x)&MASK))))
#define BIT_ON(a, x) ((void)((a)[(x) >> SHIFT] |= (1 << ((x)&MASK))))
#define BIT_FLIP(a, x) ((void)((a)[(x) >> SHIFT] ^= (1 << ((x)&MASK))))
#define BIT_ISSET(a, x) ((a)[(x) >> SHIFT] & (1 << ((x)&MASK)))

/* ******** */
/* Typedefs */
/* ******** */

typedef struct charsetinfo_s charsetinfo_t;
typedef struct term_s        term_t;
typedef struct toggle_s      toggle_t;
typedef struct win_s         win_t;
typedef struct word_s        word_t;
typedef struct attrib_s      attrib_t;
typedef struct limit_s       limit_t;
typedef struct ticker_s      ticker_t;
typedef struct misc_s        misc_t;
typedef struct sed_s         sed_t;
typedef struct timeout_s     timeout_t;
typedef struct output_s      output_t;
typedef struct daccess_s     daccess_t;
typedef struct search_data_s search_data_t;

/* ***** */
/* Enums */
/* ***** */

/* Various filter pseudo constants */
/* """"""""""""""""""""""""""""""" */
typedef enum filter_types
{
  UNKNOWN_FILTER,
  INCLUDE_FILTER,
  EXCLUDE_FILTER
} filters_t;

/* Used by the -N -F and -D options */
/* """""""""""""""""""""""""""""""" */
typedef enum daccess_modes
{
  DA_TYPE_NONE = 0, /* must be 0 (boolean value) */
  DA_TYPE_AUTO = 1,
  DA_TYPE_POS  = 2
} da_mode_t;

/* Various timeout mode used by the -x/-X option */
/* """"""""""""""""""""""""""""""""""""""""""""" */
typedef enum timeout_modes
{
  CURRENT, /* on timeout, outputs the selected word       */
  QUIT,    /* on timeout, quit without selecting anything */
  WORD     /* on timeout , outputs the specified word     */
} to_mode_t;

/* Constants used to set the color attributes */
/* """""""""""""""""""""""""""""""""""""""""" */
typedef enum attribute_settings
{
  UNSET = 0, /* must be 0 for future testings */
  SET,
  FORCED /* an attribute setting has been given in the command line */
} attr_set_t;

/* Constant to distinguish between the various search modes */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
typedef enum search_modes
{
  NONE,
  PREFIX,
  FUZZY,
  SUBSTRING
} search_mode_t;

/* Constants used in search mode to orient the bit-mask building */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
typedef enum bitmap_affinities
{
  NO_AFFINITY,
  END_AFFINITY,
  START_AFFINITY
} bitmap_affinity_t;

/* Used when managing the -R option */
/* """""""""""""""""""""""""""""""" */
enum
{
  ROW_REGEX_EXCLUDE = 0, /* must be 0 (boolean value) */
  ROW_REGEX_INCLUDE = 1
};

/* Used when managing the -C option */
/* """""""""""""""""""""""""""""""" */
enum
{
  EXCLUDE_MARK      = 0, /* must be 0 because used in various tests     *
                          * these words cannot be re-included           */
  INCLUDE_MARK      = 1, /* to forcibly include a word, these words can *
                          * be excluded later                           */
  SOFT_EXCLUDE_MARK = 2, /* word with this mark are excluded by default *
                          * but can be included later                   */
  SOFT_INCLUDE_MARK = 3  /* word with this mark are included by default *
                          * but can be excluded later                   */
};

/* ******* */
/* Structs */
/* ******* */

/* Used to store the different allowed charsets data */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
struct charsetinfo_s
{
  char * name; /* canonical name of the allowed charset */
  int    bits; /* number of bits in this charset        */
};

/* Various toggles which can be set with command line options */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct toggle_s
{
  int del_line;            /* 1 if the clean option is set (-d) else 0   */
  int enter_val_in_search; /* 1 if ENTER validates in search mode else 0 */
  int no_scrollbar;        /* 1 to disable the scrollbar display else 0  */
  int blank_nonprintable;  /* 1 to try to display non-blanks in          *
                            * symbolic form else 0                       */
  int keep_spaces;         /* 1 to keep the trailing spaces in columns   *
                            * and tabulate mode.                         */
  int taggable;            /* 1 if tagging is enabled                    */
  int pinable;             /* 1 if pinning is selected                   */
  int autotag;             /* 1 if tagging is selected and pinning is    *
                            * not and we do no want an automatic tagging *
                            * when the users presses <ENTER>             */
  int visual_bell;         /* 1 to flash the window, 0 to make a sound   */
};

/* Structure to store the default or imposed smenu limits */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct limit_s
{
  long word_length; /* maximum number of bytes in a word */
  long words;       /* maximum number of words           */
  long cols;        /* maximum number of columns         */
};

/* Structure to store the default or imposed timers */
/* """""""""""""""""""""""""""""""""""""""""""""""" */
struct ticker_s
{
  int search;
  int help;
  int winch;
  int direct_access;
};

/* Structure to store miscellaneous informations */
/* """"""""""""""""""""""""""""""""""""""""""""" */
struct misc_s
{
  search_mode_t default_search_method;
  char          invalid_char_substitute;
  int           ignore_quotes;
};

/* Terminal setting variables */
/* """""""""""""""""""""""""" */
struct termios new_in_attrs;
struct termios old_in_attrs;

/* Interval timers used */
/* """""""""""""""""""" */
struct itimerval periodic_itv; /* refresh rate for the timeout counter. */

int help_timer    = -1;
int winch_timer   = -1;
int daccess_timer = -1;
int search_timer  = -1;

/* Structure containing the attributes components */
/* """""""""""""""""""""""""""""""""""""""""""""" */
struct attrib_s
{
  attr_set_t  is_set;
  short       fg;
  short       bg;
  signed char bold;
  signed char dim;
  signed char reverse;
  signed char standout;
  signed char underline;
  signed char italic;
};

/* Structure containing some terminal characteristics */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
struct term_s
{
  int   ncolumns;     /* number of columns                      */
  int   nlines;       /* number of lines                        */
  int   curs_column;  /* current cursor column                  */
  int   curs_line;    /* current cursor line                    */
  short colors;       /* number of available colors             */
  short color_method; /* color method (0=classic (0-7), 1=ANSI) */

  char has_cursor_up;         /* has cuu1 terminfo capability           */
  char has_cursor_down;       /* has cud1 terminfo capability           */
  char has_cursor_left;       /* has cub1 terminfo capability           */
  char has_cursor_right;      /* has cuf1 terminfo capability           */
  char has_parm_right_cursor; /* has cuf terminfo capability            */
  char has_cursor_address;    /* has cup terminfo capability            */
  char has_save_cursor;       /* has sc terminfo capability             */
  char has_restore_cursor;    /* has rc terminfo capability             */
  char has_setf;              /* has set_foreground terminfo capability */
  char has_setb;              /* has set_background terminfo capability */
  char has_setaf;             /* idem for set_a_foreground (ANSI)       */
  char has_setab;             /* idem for set_a_background (ANSI)       */
  char has_hpa;               /* has column_address terminfo capability */
  char has_bold;              /* has bold mode                          */
  char has_dim;               /* has dim mode                           */
  char has_reverse;           /* has reverse mode                       */
  char has_underline;         /* has underline mode                     */
  char has_standout;          /* has standout mode                      */
  char has_italic;            /* has italic mode                        */
};

/* Structure describing a word */
/* """"""""""""""""""""""""""" */
struct word_s
{
  long          start, end;    /* start/end absolute horiz. word positions *
                                * on the screen                            */
  size_t        mb;            /* number of UTF-8 glyphs to display        */
  long          tag_order;     /* each time a word is tagged, this value   *
                                * is increased                             */
  size_t        special_level; /* can vary from 0 to 5; 0 meaning normal   */
  char *        str;           /* display string associated with this word */
  size_t        len;           /* number of bytes of str (for trimming)    */
  char *        orig;          /* NULL or original string if is had been   *
                                * shortened for being displayed or altered *
                                * by is expansion.                         */
  char *        bitmap;        /* used to store the position of the        *
                                * currently searched chars in a word. The  *
                                * objective is to speed their display      */
  unsigned char is_matching;
  unsigned char is_tagged;     /* 1 if the word is tagged, 0 if not        */
  unsigned char is_last;       /* 1 if the word is the last of a line      */
  unsigned char is_selectable; /* word is selectable                       */
  unsigned char is_numbered;   /* word has a direct access index           */
};

/* Structure describing the window in which the user */
/* will be able to select a word                     */
/* """"""""""""""""""""""""""""""""""""""""""""""""" */
struct win_s
{
  long    start, end;      /* index of the first and last word        */
  long    first_column;    /* number of the first character displayed */
  long    cur_line;        /* relative number of the cursor line (1+) */
  long    asked_max_lines; /* requested number of lines in the window */
  long    max_lines;       /* effective number of lines in the window */
  long    max_cols;        /* max number of words in a single line    */
  long    real_max_width;  /* max line length. In column, tab or line *
                            * mode it can be greater than the         *
                            * terminal width                          */
  long    message_lines;   /* number of lines taken by the messages   *
                            * (updated by disp_message                */
  long    max_width;       /* max usable line width or the terminal   */
  long    offset;          /* window offset user when centered        */
  char *  sel_sep;         /* output separator when tags are enabled  */
  char ** gutter_a;        /* array of UTF-8 gutter glyphs            */
  long    gutter_nb;       /* number of UTF-8 gutter glyphs           */

  unsigned char tab_mode;  /* -t */
  unsigned char col_mode;  /* -c */
  unsigned char line_mode; /* -l */
  unsigned char col_sep;   /* -g */
  unsigned char wide;      /* -w */
  unsigned char center;    /* -M */

  attrib_t cursor_attr;           /* current cursor attributes          */
  attrib_t cursor_on_tag_attr;    /* current cursor on tag attributes   */
  attrib_t bar_attr;              /* scrollbar attributes               */
  attrib_t shift_attr;            /* shift indicator attributes         */
  attrib_t message_attr;          /* message (title) attributes         */
  attrib_t search_field_attr;     /* search mode field attributes       */
  attrib_t search_text_attr;      /* search mode text attributes        */
  attrib_t search_err_field_attr; /* bad search mode field attributes   */
  attrib_t search_err_text_attr;  /* bad search mode text attributes    */
  attrib_t match_field_attr;      /* matching word field attributes     */
  attrib_t match_text_attr;       /* matching word text attributes      */
  attrib_t match_err_field_attr;  /* bad matching word field attributes */
  attrib_t match_err_text_attr;   /* bad matching word text attributes  */
  attrib_t include_attr;          /* selectable words attributes        */
  attrib_t exclude_attr;          /* non-selectable words attributes    */
  attrib_t tag_attr;              /* non-selectable words attributes    */
  attrib_t daccess_attr;          /* direct access tag attributes       */
  attrib_t special_attr[5];       /* special (-1,...) words attributes  */
};

/* Sed like node structure */
/* """"""""""""""""""""""" */
struct sed_s
{
  char *        pattern;      /* pattern to be matched             */
  char *        substitution; /* substitution string               */
  unsigned char visual;       /* visual flag: alterations are only *
                               *              visual               */
  unsigned char global;       /* global flag: alterations can      *
                               *              occur more than once */
  unsigned char stop;         /* stop flag:   only one alteration  *
                               *              per word is allowed  */
  regex_t       re;           /* compiled regular expression       */
};

/* Structure used to keep track of the different timeout values */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct timeout_s
{
  to_mode_t mode;          /* timeout mode: current/quit/word */
  unsigned  initial_value; /* 0: no timeout else value in sec */
  unsigned  remain;        /* remaining seconds               */
  unsigned  reached;       /* 1: timeout has expired, else 0  */
};

/* Structure used during the construction of the pinned words list */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct output_s
{
  long   order;      /* this field is incremented each time a word is pinned */
  char * output_str; /* The pinned word itself                               */
};

/* Structure describing the formatting of the automatic direct access entries */
/* """""""""""""""""""""""""""""""'"""""""""""""""""""""""""""""""""""""""""" */
struct daccess_s
{
  da_mode_t mode; /* DA_TYPE_NONE (0), DA_TYPE_AUTO, DA_TYPE_POS          */

  char * left;  /* character to put before the direct access selector     */
  char * right; /* character to put after the direct access selector      */

  char   alignment;  /* l: left; r: right                                 */
  char   padding;    /* a: all; i: only included words are padded         */
  char   head;       /* What to do with chars before the embedded number  */
  int    length;     /* selector size (5 max)                             */
  int    flength;    /* 0 or length + 3 (full prefix length               */
  size_t offset;     /* offset to the start of the selector               */
  char   missing;    /* y: number missing embedded numbers                */
  int    plus;       /* 1 if we can look for the number to extract after  *
                      * the offset, else 0. (a '+' follows the offset)    */
  int    size;       /* size in bytes of the selector to extract          */
  size_t ignore;     /* number of UTF-8 glyphs to ignore after the number */
  char   follow;     /* y: the numbering follows the last number set      */
  char * num_sep;    /* character to separate number and selection        */
  int    def_number; /* 1: the numbering is on by default 0: it is not    */
};

/* Structure used in search mod to store the current buffer and various   */
/* related values.                                                        */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct search_data_s
{
  char * buf;        /* search buffer                            */
  long   len;        /* current position in the search buffer    */
  long   utf8_len;   /* current position in the search buffer in *
                      * UTF-8 units                              */
  long * utf8_off_a; /* array of mb offsets in buf               */
  long * utf8_len_a; /* array of mb lengths in buf               */

  int  fuzzy_err;     /* fuzzy match error indicator             */
  long fuzzy_err_pos; /* last good position in search buffer     */

  int only_ending;   /* only searches giving a result with the   *
                      * pattern at the end of the word will be   *
                      * selected                                 */
  int only_starting; /* same with the pattern at the beginning   */
};

/* ********** */
/* Prototypes */
/* ********** */

void
help(win_t * win, term_t * term, long last_line, toggle_t * toggles);

int
tag_comp(void * a, void * b);

void
tag_swap(void * a, void * b);

int
isempty(const char * s);

void
my_beep(toggle_t * toggles);

int
get_cursor_position(int * const r, int * const c);

void
get_terminal_size(int * const r, int * const c, term_t * term);

int
#ifdef __sun
outch(char c);
#else
outch(int c);
#endif

void
restore_term(int const fd);

void
setup_term(int const fd);

void
strip_ansi_color(char * s, toggle_t * toggles, misc_t * misc);

int
tst_cb(void * elem);

int
tst_cb_cli(void * elem);

int
ini_load(const char * filename, win_t * win, term_t * term, limit_t * limits,
         ticker_t * timers, misc_t * misc, langinfo_t * langinfo,
         int (*report)(win_t * win, term_t * term, limit_t * limits,
                       ticker_t * timers, misc_t * misc, langinfo_t * langinfo,
                       const char * section, const char * name, char * value));

int
ini_cb(win_t * win, term_t * term, limit_t * limits, ticker_t * timers,
       misc_t * misc, langinfo_t * langinfo, const char * section,
       const char * name, char * value);

char *
make_ini_path(char * name, char * base);

short
color_transcode(short color);

void
set_foreground_color(term_t * term, short color);

void
set_background_color(term_t * term, short color);

void
set_win_start_end(win_t * win, long current, long last);

long
build_metadata(term_t * term, long count, win_t * win);

long
disp_lines(win_t * win, toggle_t * toggles, long current, long count,
           search_mode_t search_mode, search_data_t * search_data,
           term_t * term, long last_line, char * tmp_word,
           langinfo_t * langinfo);

void
get_message_lines(char * message, ll_t * message_lines_list,
                  long * message_max_width, long * message_max_len);

void
disp_message(ll_t * message_lines_list, long width, long max_len, term_t * term,
             win_t * win, langinfo_t * langinfo);

int
check_integer_constraint(int nb_args, char ** args, char * value, char * par);

void
update_bitmaps(search_mode_t search_mode, search_data_t * search_data,
               bitmap_affinity_t affinity);

long
find_next_matching_word(long * array, long nb, long value, long * index);

long
find_prev_matching_word(long * array, long nb, long value, long * index);

void
clean_matches(search_data_t * search_data, long size);

void
disp_cursor_word(long pos, win_t * win, term_t * term, int err);

void
disp_matching_word(long pos, win_t * win, term_t * term, int is_cursor,
                   int err);

void
disp_word(long pos, search_mode_t search_mode, search_data_t * search_data,
          term_t * term, win_t * win, char * tmp_word);

size_t
expand(char * src, char * dest, langinfo_t * langinfo, toggle_t * toggles,
       misc_t * misc);

int
get_bytes(FILE * input, char * utf8_buffer, ll_t * ignored_glyphs_list,
          langinfo_t * langinfo, misc_t * misc);

int
get_scancode(unsigned char * s, size_t max);

char *
get_word(FILE * input, ll_t * word_delims_list, ll_t * record_delims_list,
         ll_t * ignored_glyphs_list, char * utf8_buffer,
         unsigned char * is_last, toggle_t * toggles, langinfo_t * langinfo,
         win_t * win, limit_t * limits, misc_t * misc);

void
left_margin_putp(char * s, term_t * term, win_t * win);

void
right_margin_putp(char * s1, char * s2, langinfo_t * langinfo, term_t * term,
                  win_t * win, long line, long offset);

void
sig_handler(int s);

void
set_new_first_column(win_t * win, term_t * term);

int
parse_sed_like_string(sed_t * sed);

void
parse_selectors(char * str, filters_t * filter, char * unparsed,
                ll_t ** inc_interval_list, ll_t ** inc_regex_list,
                ll_t ** exc_interval_list, ll_t ** exc_regex_list,
                langinfo_t * langinfo, misc_t * misc);

int
replace(char * orig, sed_t * sed);

int
decode_attr_toggles(char * s, attrib_t * attr);

int
parse_attr(char * str, attrib_t * attr, short max_color);

void
apply_attr(term_t * term, attrib_t attr);

int
delims_cmp(const void * a, const void * b);

long
get_line_last_word(long line, long last_line);

void
move_left(win_t * win, term_t * term, toggle_t * toggles,
          search_data_t * search_data, langinfo_t * langinfo, long * nl,
          long last_line, char * tmp_word);

void
move_right(win_t * win, term_t * term, toggle_t * toggles,
           search_data_t * search_data, langinfo_t * langinfo, long * nl,
           long last_line, char * tmp_word);

int
find_best_word_upward(long last_word, long s, long e);

int
find_best_word_downward(long last_word, long s, long e);

void
move_up(win_t * win, term_t * term, toggle_t * toggles,
        search_data_t * search_data, langinfo_t * langinfo, long * nl,
        long page, long first_selectable, long last_line, char * tmp_word);

void
move_down(win_t * win, term_t * term, toggle_t * toggles,
          search_data_t * search_data, langinfo_t * langinfo, long * nl,
          long page, long last_selectable, long last_line, char * tmp_word);

void
init_main_ds(attrib_t * init_attr, win_t * win, limit_t * limits,
             ticker_t * timers, toggle_t * toggles, misc_t * misc,
             timeout_t * timeout, daccess_t * daccess);
#endif
