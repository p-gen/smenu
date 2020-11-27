/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html.           */
/* ########################################################### */

/* Ternary Search Tree and sorted array creation functions.                 */
/* Inspired by a code described in "Ternary Search Trees" by Jon            */
/* Bentley and Robert Sedgewick in the April, 1998, Dr. Dobb's Journal.     */
/* Links:                                                                   */
/*   https://www.drdobbs.com/database/ternary-search-trees/184410528?pgno=1 */
/*   https://www.cs.princeton.edu/~rs/strings/tstdemo.c.                    */
/* ************************************************************************ */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <wchar.h>
#include <string.h>

#include "xmalloc.h"
#include "list.h"
#include "utils.h"
#include "index.h"

/* List of words matching the current search. */
/* """""""""""""""""""""""""""""""""""""""""" */
ll_t * tst_search_list; /* Must be initialized by ll_new() before use. */

/* ======================================= */
/* Ternary search tree insertion function. */
/* ======================================= */
tst_node_t *
tst_insert(tst_node_t * p, wchar_t * w, void * data)
{
  if (p == NULL)
  {
    p            = (tst_node_t *)xmalloc(sizeof(tst_node_t));
    p->splitchar = *w;
    p->lokid = p->eqkid = p->hikid = NULL;
    p->data                        = NULL;
  }

  if (*w < p->splitchar)
    p->lokid = tst_insert(p->lokid, w, data);
  else if (*w == p->splitchar)
  {
    if (*w == L'\0')
    {
      p->data  = data;
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
/* Ternary search tree deletion function. */
/* User data area not cleaned.            */
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
/* Recursive traversal of a ternary tree. A callback function */
/* is also called when a complete string is found.            */
/* Returns 1 if the callback function succeed (returned 1) at */
/* least once.                                                */
/* The first_call argument is for initializing the static     */
/* variable.                                                  */
/* ========================================================== */
int
tst_traverse(tst_node_t * p, int (*callback)(void *), int first_call)
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
    rc += (*callback)(p->data);
  tst_traverse(p->hikid, callback, 0);

  return !!rc;
}

/* ======================================================================= */
/* Traverses the word tst looking for a wchar and build a list of pointers */
/* containing all the sub-tst potentially leading to words containing the  */
/* next wchar os the search string.                                        */
/* ======================================================================= */
int
tst_substring_traverse(tst_node_t * p, int (*callback)(void *), int first_call,
                       wchar_t w)
{
  static int rc;

  if (first_call)
    rc = 0;

  if (!p)
    return 0;

  if (p->splitchar == w)
  {
    ll_node_t * node;
    sub_tst_t * sub_tst_data;

    node         = tst_search_list->tail;
    sub_tst_data = (sub_tst_t *)(node->data);

    if (p->eqkid)
      insert_sorted_ptr(&(sub_tst_data->array), &(sub_tst_data->size),
                        &(sub_tst_data->count), p->eqkid);

    rc = 1;
  }

  tst_substring_traverse(p->lokid, callback, 0, w);
  if (p->splitchar != L'\0')
    tst_substring_traverse(p->eqkid, callback, 0, w);
  else if (callback != NULL)
    rc += (*callback)(p->data);
  tst_substring_traverse(p->hikid, callback, 0, w);

  return !!rc;
}

/* ======================================================================== */
/* Traverses the word tst looking for a wchar and build a list of pointers  */
/* containing all the sub-tst nodes potentially leading to words containing */
/* the next wchar os the search string.                                     */
/* ======================================================================== */
int
tst_fuzzy_traverse(tst_node_t * p, int (*callback)(void *), int first_call,
                   wchar_t w)
{
  static int rc;
  wchar_t    w1s[2];
  wchar_t    w2s[2];

  w1s[1] = w2s[1] = L'\0';

  if (first_call)
    rc = 0;

  if (!p)
    return 0;

  w1s[0] = p->splitchar;
  w2s[0] = w;

  if (xwcscasecmp(w1s, w2s) == 0)
  {
    ll_node_t * node;
    sub_tst_t * sub_tst_data;

    node         = tst_search_list->tail;
    sub_tst_data = (sub_tst_t *)(node->data);

    if (p->eqkid != NULL)
      insert_sorted_ptr(&(sub_tst_data->array), &(sub_tst_data->size),
                        &(sub_tst_data->count), p->eqkid);

    rc += 1;
  }

  tst_fuzzy_traverse(p->lokid, callback, 0, w);
  if (p->splitchar != L'\0')
    tst_fuzzy_traverse(p->eqkid, callback, 0, w);
  else if (callback != NULL)
    rc += (*callback)(p->data);
  tst_fuzzy_traverse(p->hikid, callback, 0, w);

  return !!rc;
}

/* ======================================================================= */
/* Searches a complete string in a ternary tree starting from a root node. */
/* ======================================================================= */
void *
tst_search(tst_node_t * root, wchar_t * w)
{
  tst_node_t * p;

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

/* ================================================================= */
/* Searches all strings beginning with the same prefix.              */
/* the callback function will be applied to each of theses strings   */
/* returns NULL if no string matched the prefix.                     */
/* ================================================================= */
void *
tst_prefix_search(tst_node_t * root, wchar_t * w, int (*callback)(void *))
{
  tst_node_t * p   = root;
  size_t       len = wcslen(w);
  size_t       rc;

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
        return (void *)(long)rc;
      }
      p = p->eqkid;
    }
    else
      p = p->hikid;
  }

  return NULL;
}

/* ========================================================= */
/* Insertion of an integer in a already sorted integer array */
/* without duplications.                                     */
/* ========================================================= */
void
insert_sorted_index(long ** array, long * size, long * nb, long value)
{
  long pos  = *nb;
  long left = 0, right = *nb, middle;

  if (*nb > 0)
  {
    /* Bisection search. */
    /* """"""""""""""""" */
    while (left < right)
    {
      middle = (left + right) / 2;
      if ((*array)[middle] == value)
        return; /* Value already in array. */

      if (value < (*array)[middle])
        right = middle;
      else
        left = middle + 1;
    }
    pos = left;
  }

  if (*nb == *size)
  {
    *size += 64;
    *array = xrealloc(*array, *size * sizeof(long));
  }

  if (*nb > pos)
    memmove((*array) + pos + 1, (*array) + pos, sizeof(value) * (*nb - pos));

  (*nb)++;

  (*array)[pos] = value;
}

/* ========================================================= */
/* Insertion of an pointer in a already sorted pointer array */
/* without duplications.                                     */
/* ========================================================= */
void
insert_sorted_ptr(tst_node_t *** array, unsigned long long * size,
                  unsigned long long * nb, tst_node_t * ptr)
{
  unsigned long long pos  = *nb;
  unsigned long long left = 0, right = *nb, middle;

  if (*nb > 0)
  {
    /* Bisection search. */
    /* """"""""""""""""" */
    while (left < right)
    {
      middle = (left + right) / 2;
      if ((intptr_t)((*array)[middle]) == (intptr_t)ptr)
        return; /* Value already in array. */

      if ((intptr_t)ptr < (intptr_t)((*array)[middle]))
        right = middle;
      else
        left = middle + 1;
    }
    pos = left;
  }

  if (*nb == *size)
  {
    *size += 64;
    *array = xrealloc(*array, *size * sizeof(long));
  }

  if (*nb > pos)
    memmove((*array) + pos + 1, (*array) + pos, sizeof(ptr) * (*nb - pos));

  (*nb)++;

  (*array)[pos] = ptr;
}
