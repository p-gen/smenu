/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* ************************************************************************ */
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
#include "xmalloc.h"

/* List of words matching the current search. */
/* """""""""""""""""""""""""""""""""""""""""" */
ll_t *tst_search_list; /* Must be initialized by ll_new() before use. */

/* ======================================= */
/* Ternary search tree insertion function. */
/* ======================================= */
tst_node_t *
tst_insert(tst_node_t *p, wchar_t *w, void *data)
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
      p->eqkid = tst_insert(p->eqkid, w + 1, data);
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
tst_cleanup(tst_node_t *p)
{
  if (p != NULL)
  {
    tst_cleanup(p->lokid);
    if (p->splitchar != L'\0')
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
tst_traverse(tst_node_t *p, int (*callback)(void *), int first_call)
{
  static int rc;

  if (first_call)
    rc = 0;

  if (p == NULL)
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
/* next wchar of the search string.                                        */
/* ======================================================================= */
int
tst_substring_traverse(tst_node_t *p,
                       int (*callback)(void *),
                       int     first_call,
                       wchar_t w)
{
  static int rc;

  if (first_call)
    rc = 0;

  if (p == NULL)
    return 0;

  if (p->splitchar == w)
  {
    ll_node_t *node;
    sub_tst_t *sub_tst_data;

    node         = tst_search_list->tail;
    sub_tst_data = (sub_tst_t *)(node->data);

    if (p->eqkid != NULL)
      insert_sorted_ptr(&(sub_tst_data->array),
                        &(sub_tst_data->size),
                        &(sub_tst_data->count),
                        p->eqkid);

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
tst_fuzzy_traverse(tst_node_t *p,
                   int (*callback)(void *),
                   int     first_call,
                   wchar_t w)
{
  static int rc;
  wchar_t    w1s[2];
  wchar_t    w2s[2];

  w1s[1] = w2s[1] = L'\0';

  if (first_call)
    rc = 0;

  if (p == NULL)
    return 0;

  w1s[0] = p->splitchar;
  w2s[0] = w;

  if (my_wcscasecmp(w1s, w2s) == 0)
  {
    ll_node_t *node;
    sub_tst_t *sub_tst_data;

    node         = tst_search_list->tail;
    sub_tst_data = (sub_tst_t *)(node->data);

    if (p->eqkid != NULL)
      insert_sorted_ptr(&(sub_tst_data->array),
                        &(sub_tst_data->size),
                        &(sub_tst_data->count),
                        p->eqkid);

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
tst_search(tst_node_t *root, wchar_t *w)
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

/* ============================================================== */
/* Searches all strings beginning with the same prefix.           */
/* the callback function will be applied to each of these strings */
/* returns NULL if no string matched the prefix.                  */
/* ============================================================== */
void *
tst_prefix_search(tst_node_t *root, wchar_t *w, int (*callback)(void *))
{
  tst_node_t *p   = root;
  size_t      len = wcslen(w);
  size_t      rc;

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

/* =============================================================== */
/* Follows the eqkid list from a note in the tst until w if found. */
/* Returns a pointer to the node if found, else returns NULL.      */
/*                                                                 */
/* Example with words "abcde", "bacdef" and "cda" in the tst_word  */
/*         tree.                                                   */
/*         if p points to the node (a), and w contains 'e' then    */
/*         tst_search_in_word(p, w) will follow the starred nodes  */
/*         until (e) and return q following the aqkid of the TST   */
/*         from (a)                                                */
/*                                                                 */
/*    b                                                            */
/*  / | \                                                          */
/* a (a) c                                                         */
/* |  |  |                                                         */
/* b  c  d                                                         */
/* |  |  |                                                         */
/* c  d  a                                                         */
/* |  |                                                            */
/* d (e) <- q                                                      */
/* |  |                                                            */
/* e  f                                                            */
/* =============================================================== */
void *
tst_search_in_word(tst_node_t *root, wchar_t w)
{
  tst_node_t *p  = root;
  size_t      rc = 0;

  while (p && p->splitchar != L'\0')
  {
    if (p->splitchar == w)
    {
      rc = 1;
      break;
    }
    p = p->eqkid;
  }

  if (rc)
    return p;
  else
    return NULL;
}

/* ===================================================================== */
/* Helper function used only in fuzzy mode to append a glyph in the      */
/* search_list.                                                          */
/* Takes all the level n pointers to nodes in tst_words to determine the */
/* level n+1 pointers by following the eqkids pointers from these nodes. */
/* ===================================================================== */
void
append_tst_search_list(char *glyph, long l)
{
  tst_node_t *tst_node, *node;
  sub_tst_t  *sub_tst, *new_sub_tst;
  wchar_t     w;

  sub_tst = (sub_tst_t *)(tst_search_list->tail->data);
  mbtowc(&w, glyph, l);

  /* Create a new empty sub_tst_t node. */
  /* """""""""""""""""""""""""""""""""" */
  new_sub_tst = sub_tst_new();

  /* For each eqkids of the level n */
  /* """""""""""""""""""""""""""""" */
  for (long i = 0; i < sub_tst->count; i++)
  {
    tst_node = ((tst_node_t **)(sub_tst->array))[i];
    if (tst_node->splitchar == L'\0')
      continue;

    /* If the search glyph exists as on of its eqkid. */
    /* build a level n+1 sub_tst node.                */
    /* """""""""""""""""""""""""""""""""""""""""""""" */
    if ((node = tst_search_in_word(tst_node, w)) != NULL)
    {
      new_sub_tst->count++;
      new_sub_tst->size  = new_sub_tst->count / 64 + 64;
      new_sub_tst->array = xrealloc(new_sub_tst->array,
                                    sizeof(tst_node_t *) * new_sub_tst->size);

      new_sub_tst->array[new_sub_tst->count - 1] = node;
    }
  }

  ll_append(tst_search_list, new_sub_tst);
}

/* ========================================================= */
/* Insertion of an pointer in a already sorted pointer array */
/* without duplications.                                     */
/* ========================================================= */
void
insert_sorted_ptr(tst_node_t ***array_ptr,
                  long         *size,
                  long         *nb,
                  tst_node_t   *ptr)
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
      if ((intptr_t)((*array_ptr)[middle]) == (intptr_t)ptr)
        return; /* Value already in array. */

      if ((intptr_t)ptr < (intptr_t)((*array_ptr)[middle]))
        right = middle;
      else
        left = middle + 1;
    }
    pos = left;
  }

  if (*nb == *size)
  {
    *size += 64;
    *array_ptr = xrealloc(*array_ptr, *size * sizeof(long));
  }

  if (*nb > pos)
    memmove((*array_ptr) + pos + 1,
            (*array_ptr) + pos,
            sizeof(ptr) * (*nb - pos));

  (*nb)++;

  (*array_ptr)[pos] = ptr;
}
