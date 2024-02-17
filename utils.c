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

#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <wctype.h>
#include "xmalloc.h"
#include "wchar.h"
#include "list.h"
#include "utf8.h"
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
/* Allocates memory and safely concatenate strings. Stolen from a public     */
/* domain implementation which can be found here:                            */
/* http://openwall.info/wiki/people/solar/software/public-domain-source-code */
/* ========================================================================= */
char *
concat(const char *s1, ...)
{
  va_list     args;
  const char *s;
  char       *p, *result;
  size_t      l, m, n;

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

/* ================================ */
/* 8 bits aware version of isprint. */
/* ================================ */
int
isprint8(int i)
{
  unsigned char c = i & (unsigned char)0xff;

  return (c >= 0x20 && c < 0x7f) || (c >= (unsigned char)0xa0);
}

/* ==================================================== */
/* Private implementation of wcscasecmp missing in c99. */
/* ==================================================== */
int
my_wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
  wchar_t c1, c2;

  while (*s1)
  {
    c1 = towlower(*s1);
    c2 = towlower(*s2);

    if (c1 != c2)
      return (int)(c1 - c2);

    s1++;
    s2++;
  }
  return -*s2;
}

/* ================================================================ */
/* Returns 1 if s can be converted into an int otherwise returns 0. */
/* ================================================================ */
int
is_integer(const char * const s)
{
  long int n;
  char    *endptr;

  n = strtol(s, &endptr, 10);

  if (errno != ERANGE && n >= INT_MIN && n <= INT_MAX && *endptr == '\0')
    return 1;

  return 0;
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
  char  *tmp;
  size_t size;

  if (*s == NULL || **s == '\0')
    return 0;

  tmp  = xmalloc(strlen(*s) * 2 + 1);
  size = strlen(*s);

  if (first > size)
    return 0;

  strcpy(tmp, *s);
  strcat(tmp, *s);
  strncpy(*s, tmp + first, size);

  free(tmp);
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
    if (isprint(*s))
      *(p++) = *s++;
    else
    {
      sprintf(p, "\\x%02X", (unsigned char)*s++);
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

    d[b % 16] = isprint(buf[b]) ? (unsigned char)buf[b] : '.';

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

/* ===================================================================== */
/* Version of wcswidth which tries to support extended grapheme clusters */
/* by taking into zero width characters.                                 */
/* ===================================================================== */
int
my_wcswidth(const wchar_t *s, size_t n)
{
  int len = 0;
  int l   = 0;
  int m   = 0;

  if (s == NULL || *s == L'\0')
    return 0;

  while (*s && m < n)
  {
    if ((l = wcwidth(*s)) >= 0)
    {
      /* Do not count zero-width-length glyphs. */
      /* """""""""""""""""""""""""""""""""""""" */
      if (*s != L'\x200d' && *(s + 1) != L'\x200d' && *(s + 1) != L'\xfe0f'
          && *(s + 1) != L'\x20e3')
        len += l;
    }
    else
      return -1; /* wcwidth returned -1. */

    s++;
    m++;
  }

  return len;
}
