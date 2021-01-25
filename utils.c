/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html.           */
/* ########################################################### */

/* ******************************** */
/* Various small utility functions. */
/* ******************************** */

#include "config.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <wctype.h>
#include "xmalloc.h"
#include "list.h"
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
  interval_t * ret = xmalloc(sizeof(interval_t));

  return ret;
}

/* ======================================= */
/* Compares 2 intervals as integer couples */
/* same return values as for strcmp.       */
/* ======================================= */
int
interval_comp(void * a, void * b)
{
  interval_t * ia = (interval_t *)a;
  interval_t * ib = (interval_t *)b;

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

/* ====================================================================== */
/* Merges the intervals from an interval list in order to get the minimum */
/* number of intervals to consider.                                       */
/* ====================================================================== */
void
merge_intervals(ll_t * list)
{
  ll_node_t * node1, *node2;
  interval_t *data1, *data2;

  if (!list || list->len < 2)
    return;
  else
  {
    /* Step 1: sort the intervals list. */
    /* """""""""""""""""""""""""""""""" */
    ll_sort(list, interval_comp, interval_swap);

    /* Step 2: merge the list by merging the consecutive intervals. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
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
}

/* ****************** */
/* Strings functions. */
/* ****************** */

/* ========================================================================= */
/* Allocates memory and safely concatenate strings. Stolen from a public     */
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
/* Returns 1 if true, else 0.                      */
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

/* ========================= */
/* Trims leading characters. */
/* ========================= */
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
/* Trims trailing characters.                        */
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

/* ========================================= */
/* Case insensitive strcmp.                  */
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

/* ============================================= */
/* memmove based strcpy (tolerates overlapping). */
/* ============================================= */
char *
my_strcpy(char * str1, char * str2)
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
  return (i >= 0x20 && i <= 0x7e);
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
xwcscasecmp(const wchar_t * s1, const wchar_t * s2)
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
  return (-*s2);
}

/* ====================================== */
/* Returns 1 if s represents an integer   */
/* else 0.                                */
/* s != NULL is assumed.                  */
/* 0 and 0x prefixes are not understood.  */
/* ====================================== */
int
is_integer(const char * const s)
{
  char * end;

  strtol(s, &end, 10);
  return (*end == '\0');
}
