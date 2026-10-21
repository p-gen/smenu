/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* ******************************** */
/* Various small utility functions. */
/* ******************************** */

#include "config.h" /* for HAVE_STRCASECMP                  */
#include <unistd.h> /* for close, getpgid, tcgetpgrp        */
#include <stdio.h>  /* for fprintf, sprintf, FILE           */
#include <stdlib.h> /* for free, strtol                     */
#include <stddef.h> /* for size_t, NULL, ptrdiff_t          */
#include <limits.h> /* for INT_MAX, INT_MIN                 */
#include <safe.h>   /* for INT_MAX, INT_MIN                 */

#ifdef HAVE_STRCASECMP
#include <strings.h> /* for strcasecmp */
#endif

#include <string.h>  /* for strlen, memset, memcpy, memmove, strcat, strchr */
#include <stdarg.h>  /* for va_arg, va_end, va_start, va_list               */
#include <ctype.h>   /* for isprint                                         */
#include <fcntl.h>   /* for open, O_RDONLY                                  */
#include <errno.h>   /* for ERANGE, errno                                   */
#include "xmalloc.h" /* for xmalloc, xcalloc, xrealloc                      */
#include "list.h"    /* for ll_node_s, ll_delete, ll_node_t, ll_s, ll_sort  */
#include "utils.h"

/* ******************* */
/* Interval functions. */
/* ******************* */

/* ======================= */
/* Creates a new interval. */
/* ======================= */
interval_t *
interval_new(void)
{
  return xmalloc(sizeof(interval_t));
}

/* ======================================= */
/* Compares 2 intervals as integer couples */
/* same return values as for strcmp.       */
/* ======================================= */
int
interval_comp(void const *a, void const *b)
{
  interval_t const *ia = (interval_t *)a;
  interval_t const *ib = (interval_t *)b;

  if (ia->low < ib->low)
    /* ia: [...      */
    /* ib:      [... */
    return -1;
  if (ia->low > ib->low)
    /* ia:      [... */
    /* ib: [...      */
    return 1;
  if (ia->high < ib->high)
    /* ia: ...]      */
    /* ib:      ...] */
    return -1;
  if (ia->high > ib->high)
    /* ia:      ...] */
    /* ib: ...]      */
    return 1;

  return 0;
}

/* ================================== */
/* Swaps the values of two intervals. */
/* ================================== */
void
interval_swap(void **a, void **b)
{
  interval_t *ia = (interval_t *)*a;
  interval_t *ib = (interval_t *)*b;
  long        tmp;

  tmp     = ia->low;
  ia->low = ib->low;
  ib->low = tmp;

  tmp      = ia->high;
  ia->high = ib->high;
  ib->high = tmp;
}

/* ====================================================================== */
/* Merges the intervals from an interval list in order to get the minimum */
/* number of intervals to consider.                                       */
/* ====================================================================== */
void
optimize_an_interval_list(ll_t *list)
{
  ll_node_t  *node1, *node2;
  interval_t *data1, *data2;

  if (!list || list->len < 2)
    return;

  /* Step 1: sort the intervals list. */
  /* """""""""""""""""""""""""""""""" */
  ll_sort(list, interval_comp, interval_swap);

  /* Step 2: merge the list by merging the consecutive intervals. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node1 = list->head;
  node2 = node1->next;

  while (node2)
  {
    data1 = (interval_t *)(node1->data);
    data2 = (interval_t *)(node2->data);

    if (data1->high >= data2->low - 1)
    {
      /* Interval 1 overlaps interval 2. */
      /* ''''''''''''''''''''''''''''''' */
      if (data2->high >= data1->high)
        data1->high = data2->high;
      ll_delete(list, node2);
      free(data2);
      node2 = node1->next;
    }
    else
    {
      /* No overlap. */
      /* ''''''''''' */
      node1 = node2;
      node2 = node2->next;
    }
  }
}

/* ***************** */
/* String functions. */
/* ***************** */

/* ========================================================================= */
/* Allocates memory and safely concatenate strings. Inspired by a public     */
/* domain implementation which can be found here:                            */
/* http://openwall.info/wiki/people/solar/software/public-domain-source-code */
/* ========================================================================= */
char *
concat(const char *s1, ...)
{
  va_list     args;
  const char *s;
  char       *p;
  char       *result;
  size_t      len;
  size_t      total;
  size_t      first_len;

  first_len = strlen(s1);
  total     = first_len + 1;

  if (total == 0)
    return NULL;

  va_start(args, s1);

  while ((s = va_arg(args, const char *)) != NULL)
  {
    len = strlen(s);

    if ((total += len) < len)
    {
      va_end(args);
      return NULL;
    }
  }

  va_end(args);

  result = xmalloc(total);
  p      = result;

  memcpy(p, s1, first_len);
  p += first_len;

  va_start(args, s1);

  while ((s = va_arg(args, const char *)) != NULL)
  {
    len = strlen(s);
    memcpy(p, s, len);
    p += len;
  }

  va_end(args);

  *p = '\0';

  return result;
}

/* =============================================== */
/* Is the string str2 a prefix of the string str1? */
/* Returns 1 if true, else 0.                      */
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

/* ========================= */
/* Trims leading characters. */
/* ========================= */
void
ltrim(char *str, const char *trim_str)
{
  size_t len   = strlen(str);
  size_t begin = strspn(str, trim_str);

  if (begin > 0)
    for (size_t i = begin; i <= len; ++i)
      str[i - begin] = str[i];
}

/* ==================================================================== */
/* Trims trailing characters.                                           */
/* All (ASCII) characters in trim_str will be removed.                  */
/* The min argument guarantees that the length of the resulting string  */
/* will not be smaller than this size if it was larger before, 0 is the */
/* usual value here.                                                    */
/* Note that when min is greater than 0, tail characters intended to be */
/* deleted may remain.                                                  */
/* ==================================================================== */
void
rtrim(char *str, const char *trim_str, size_t min)
{
  size_t len = strlen(str);
  while (len > min && strchr(trim_str, str[len - 1]))
    str[--len] = '\0';
}

/* ========================================= */
/* Case insensitive strcmp.                  */
/* from http://c.snippets.org/code/stricmp.c */
/* ========================================= */
int
my_strcasecmp(const char *str1, const char *str2)
{
#ifdef HAVE_STRCASECMP
  return strcasecmp(str1, str2);
#else
  int retval = 0;

  while (1)
  {
    retval = tolower((unsigned char)*str1++) - tolower((unsigned char)*str2++);

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

/* ===================================================================== */
/* Construct a lowercase version of an ASCII string.                     */
/*                                                                       */
/* d: destination character string (must be pre-allocated and freed when */
/*    no more useful).                                                   */
/* s: original character string.                                         */
/* ===================================================================== */
void
strtolower(char *d, char *s)
{
  unsigned char c;

  while ((c = (unsigned char)*s++) != '\0')
    *d++ = (char)tolower(c);

  *d = '\0';
}

/* ============================================= */
/* memmove based strcpy (tolerates overlapping). */
/* ============================================= */
char *
my_strcpy(char *str1, char *str2)
{
  if (str1 == NULL || str2 == NULL)
    return NULL;

  memmove(str1, str2, strlen(str2) + 1);

  return str1;
}

/* ================================ */
/* 7 bits aware version of isprint. */
/* ================================ */
int
isprint7(int i)
{
  return i >= 0x20 && i <= 0x7e;
}

/* ================================================================ */
/* Returns 1 if s can be converted into an int otherwise returns 0. */
/* ================================================================ */
int
is_integer(const char * const s)
{
  long int n;
  char    *endptr;

  errno = 0;
  n     = strtol(s, &endptr, 10);

  return (errno != ERANGE && n >= INT_MIN && n <= INT_MAX && endptr != s
          && *endptr == '\0');
}

/* ======================================== */
/* Helper function for swap_string_parts(). */
/* ======================================== */
static void
reverse(char *s, size_t n)
{
  char *e = s + n - 1;

  while (s < e)
  {
    char c = *s;
    *s++   = *e;
    *e--   = c;
  }
}

/* ===================================================== */
/* Exchanges the start and end part of a string.         */
/* The first part goes from char 0 to size-1.            */
/* The second part goes from char size to the end of *s. */
/* Returns 1 on success.                                 */
/* ===================================================== */
int
swap_string_parts(char **s, size_t first)
{
  size_t size;

  if (*s == NULL || **s == '\0')
    return 0;

  size = strlen(*s);

  if (first > size)
    return 0;

  if (first == 0 || first == size)
    return 1;

  reverse(*s, first);
  reverse(*s + first, size - first);
  reverse(*s, size);

  return 1;
}

/* ================================================================ */
/* Substitute all the characters c1 by c2 in the string s in place. */
/* ================================================================ */
void
strrep(char *s, const char c1, const char c2)
{
  if (s != NULL)
    while (*s)
    {
      if (*s == c1)
        *s = c2;
      s++;
    }
}

/* ================================================================== */
/* Allocates and returns a string similar to s but with non printable */
/* character changed by their ASCII hexadecimal notation.             */
/* ================================================================== */
char *
strprint(char const *s)
{
  size_t l  = strlen(s);
  char *new = xcalloc(1, 4 * l + 1);
  char *p   = new;

  while (*s)
  {
    if (isprint((unsigned char)*s))
      *(p++) = *s++;
    else
    {
      sprintf(p, "\\x%02x", (unsigned char)*s++);
      p += 4;
    }
  }

  if (p - new > (ptrdiff_t)l)
    new = xrealloc(new, p - new + 1);

  return new;
}

/* =============================================== */
/* Hexadecimal dump of part of a buffer to a file. */
/*                                                 */
/* buf   : buffer to dump.                         */
/* fp    : file to dump to.                        */
/* prefix: string to be printed before each line.  */
/* size  : length of the buffer to consider.       */
/* =============================================== */
void
hexdump(const char *buf, FILE *fp, const char *prefix, size_t size)
{
  unsigned int  b;
  unsigned char d[17];
  unsigned int  o, mo;
  size_t        l;

  o = mo = 0;
  l      = strlen(prefix);

  memset(d, '\0', 17);
  for (b = 0; b < size; b++)
  {

    d[b % 16] = isprint((unsigned char)buf[b]) ? (unsigned char)buf[b] : '.';

    if ((b % 16) == 0)
    {
      o = l + 7;
      if (o > mo)
        mo = o;
      fprintf(fp, "%s: %04x:", prefix, b);
    }

    o += 3;
    if (o > mo)
      mo = o;
    fprintf(fp, " %02x", (unsigned char)buf[b]);

    if ((b % 16) == 15)
    {
      mo = o;
      o  = 0;
      fprintf(fp, " |%s|", d);
      memset(d, '\0', 17);
      fprintf(fp, "\n");
    }
  }
  if ((b % 16) != 0)
  {
    for (unsigned int i = 0; i < mo - o; i++)
      fprintf(fp, "%c", ' ');

    fprintf(fp, " |%s", d);
    if (mo > o)
      for (unsigned int i = 0; i < 16 - strlen((char *)d); i++)
        fprintf(fp, "%c", ' ');
    fprintf(fp, "%c", '|');
    memset(d, '\0', 17);
    fprintf(fp, "\n");
  }
}

/* ==================================================================== */
/* Get the target index of the number to be inserted in a sorted array. */
/*                                                                      */
/* IN array : an already empty or sorted array.                         */
/* IN value : the value to be inserted in the array.                    */
/* IN nb    : the number of existing elements in the array.             */
/*                                                                      */
/* RETURN   : -1 if the value is already in the array of the future     */
/*            index of value in the array.                              */
/* ==================================================================== */
long
get_sorted_array_target_pos(long *array, long nb, long value)
{
  long left  = 0;
  long right = nb;

  /* Bisection search. */
  /* """"""""""""""""" */
  while (left < right)
  {
    long middle = left + ((right - left) >> 1);
    long midval = array[middle];

    if (value < midval)
      right = middle;
    else if (value > midval)
      left = middle + 1;
    else
      return -1;
  }

  return left;
}

/* =============================================================== */
/* Detect if the current terminal belongs to the foreground group. */
/* returns 1 if yes else returns 0.                                */
/* =============================================================== */
int
is_in_foreground_process_group(void)
{
  int fd, fg;

  fd = open_safe("/dev/tty", O_RDONLY);
  if (fd < 0)
    return 0;

  fg = (tcgetpgrp(fd) == getpgid(0));

  close(fd);

  return fg;
}

/* ====================================================== */
/* Returns 1 if a string is empty or only made of spaces. */
/* Non UTF-8 version.                                     */
/* ====================================================== */
int
isempty_non_utf8(const unsigned char *s)
{
  while (*s != '\0')
  {
    if (*s != ' ' && *s != '\t')
      return 0;
    s++;
  }
  return 1;
}

/* ======================================================= */
/* Returns 1 if the string is empty or contains only       */
/* characters having the Unicode White_Space property.     */
/*                                                         */
/* The input must be a valid, NUL-terminated UTF-8 string. */
/* ======================================================= */
int
isempty_utf8(const unsigned char *s)
{
  while (*s != '\0')
  {
    /* U+0009..U+000d: TAB, LF, VT, FF and CR */
    /* U+0020:         SPACE                  */
    /* """""""""""""""""""""""""""""""""""""" */
    if (*s == 0x20 || (unsigned int)(*s - 0x09) <= 0x04)
    {
      s++;
      continue;
    }

    switch (*s)
    {
      /* c2 85: U+0085 NEXT LINE      */
      /* c2 a0: U+00A0 NO-BREAK SPACE */
      /* """""""""""""""""""""""""""" */
      case 0xc2:
        if (s[1] == 0x85 || s[1] == 0xa0)
        {
          s += 2;
          continue;
        }
        break;

      /* e1 9a 80: U+1680 OGHAM SPACE MARK */
      /* """"""""""""""""""""""""""""""""" */
      case 0xe1:
        if (s[1] == 0x9a && s[2] == 0x80)
        {
          s += 3;
          continue;
        }
        break;

      case 0xe2:
        if (s[1] == 0x80)
        {
          /* e2 80 80..8a: U+2000..U+200a */
          /* e2 80 a8:     U+2028         */
          /* e2 80 a9:     U+2029         */
          /* e2 80 af:     U+202f         */
          /* """""""""""""""""""""""""""" */
          if ((unsigned int)(s[2] - 0x80) <= 0x0a || s[2] == 0xa8
              || s[2] == 0xa9 || s[2] == 0xaf)
          {
            s += 3;
            continue;
          }
        }
        else if (s[1] == 0x81 && s[2] == 0x9f)
        {
          /* e2 81 9f: U+205f MEDIUM MATHEMATICAL SPACE */
          /* """""""""""""""""""""""""""""""""""""""""" */
          s += 3;
          continue;
        }
        break;

      /* e3 80 80: U+3000 IDEOGRAPHIC SPACE */
      /* """""""""""""""""""""""""""""""""" */
      case 0xe3:
        if (s[1] == 0x80 && s[2] == 0x80)
        {
          s += 3;
          continue;
        }
        break;
    }

    return 0;
  }

  return 1;
}

#if 0
/* ================================================================= */
/* Replaces any multibyte or invalid character in the current locale */
/* in s by a substitution character in-place.                        */
/* s will be modified but its address in memory will not change.     */
/* ================================================================= */
void
sanitize(char *s, char substitute)
{
  char  *p = s;
  size_t len;

  len = strlen(s);
  while (*p)
  {
    int n;

    n = mblen(p, MB_CUR_MAX);

    /* In case of error, substitute just one byte. */
    /* """"""""""""""""""""""""""""""""""""""""""" */
    if (n < 0)
    {
      *p = substitute;
      len -= 1;
    }
    else if (n > 1)
    {
      *p = substitute;
      memmove(p + 1, p + n, len - (p - s) - n + 1);
      len -= (n - 1);
    }

    p++;
  }
}
#endif
