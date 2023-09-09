/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* ********************************************************************* */
/* Tiny linked list implementation.                                      */
/*                                                                       */
/* Each node contain a void pointer to some opaque data, these functions */
/* will not try to allocate or free this data pointer.                   */
/*                                                                       */
/* Also accessors are not provided, the user has to directly manipulate  */
/* the structure members (head, tail, len, data, prev, next).            */
/* ********************************************************************* */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "xmalloc.h"
#include "list.h"

static ll_node_t *
ll_partition(ll_node_t *l,
             ll_node_t *h,
             int (*comp)(void const *, void const *),
             void (*swap)(void **, void **));

static void
ll_quicksort(ll_node_t *l,
             ll_node_t *h,
             int (*comp)(void const *, void const *),
             void (*swap)(void **, void **));

/* ========================== */
/* Creates a new linked list. */
/* ========================== */
ll_t *
ll_new(void)
{
  ll_t *ret = xmalloc(sizeof(ll_t));
  ll_init(ret);

  return ret;
}

/* ========================== */
/* Initializes a linked list. */
/* ========================== */
void
ll_init(ll_t *list)
{
  list->head = NULL;
  list->tail = NULL;
  list->len  = 0;
}

/* ====================================================== */
/* Allocates the space for a new node in the linked list. */
/* ====================================================== */
ll_node_t *
ll_new_node(void)
{
  return xmalloc(sizeof(ll_node_t));
}

/* ====================================================================== */
/* Appends a new node filled with its data at the end of the linked list. */
/* The user is responsible for the memory management of the data.         */
/*                                                                        */
/* Note: list is assumed to be initialized by ll_new().                   */
/* ====================================================================== */
void
ll_append(ll_t * const list, void * const data)
{
  ll_node_t *node;

  node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                         | uses xmalloc which does not return if there *
                         | is an allocation error.                     */

  node->data = data;
  node->next = NULL;       /* This node will be the last. */
  node->prev = list->tail; /* NULL if it is a new list.   */

  if (list->tail)
    list->tail->next = node;
  else
    list->head = node;

  list->tail = node;

  ++list->len; /* One more node in the list. */
}

/* ================================== */
/* Removes a node from a linked list. */
/* ================================== */
int
ll_delete(ll_t * const list, ll_node_t *node)
{
  if (list->head == list->tail)
  {
    /* We delete the last remaining element from the list. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (list->head == NULL)
      return 0;

    list->head = list->tail = NULL;
  }
  else if (node->prev == NULL)
  {
    /* We delete the first element from the list. */
    /* """""""""""""""""""""""""""""""""""""""""" */
    list->head       = node->next;
    list->head->prev = NULL;
  }
  else if (node->next == NULL)
  {
    /* We delete the last element from the list. */
    /* """"""""""""""""""""""""""""""""""""""""" */
    list->tail       = node->prev;
    list->tail->next = NULL;
  }
  else
  {
    /* We delete an element from the list. */
    /* """"""""""""""""""""""""""""""""""" */
    node->next->prev = node->prev;
    node->prev->next = node->next;
  }

  free(node);

  --list->len; /* One less node in the list. */

  return 1;
}

/* ====================================================== */
/* Partition code for the quicksort function.             */
/* Based on code found here:                              */
/* http://www.geeksforgeeks.org/quicksort-for-linked-list */
/* ====================================================== */
static ll_node_t *
ll_partition(ll_node_t *l,
             ll_node_t *h,
             int (*comp)(void const *, void const *),
             void (*swap)(void **, void **))
{
  /* Considers last element as pivot, places the pivot element at its       */
  /* correct position in sorted array, and places all smaller (smaller than */
  /* pivot) to left of pivot and all greater elements to right of pivot.    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  /* Set pivot as h element. */
  /* """"""""""""""""""""""" */
  void *x = h->data;

  ll_node_t *i = l->prev;
  ll_node_t *j;

  for (j = l; j != h; j = j->next)
  {
    if (comp(j->data, x) < 1)
    {
      i = (i == NULL) ? l : i->next;

      swap(&(i->data), &(j->data));
    }
  }

  i = (i == NULL) ? l : i->next;
  swap(&(i->data), &(h->data));

  return i;
}

/* ======================================================== */
/* A recursive implementation of quicksort for linked list. */
/* Based on code found here:                                */
/* http://www.geeksforgeeks.org/quicksort-for-linked-list   */
/* ======================================================== */
static void
ll_quicksort(ll_node_t *l,
             ll_node_t *h,
             int (*comp)(void const *, void const *),
             void (*swap)(void **, void **))
{
  if (h != NULL && l != h && l != h->next)
  {
    ll_node_t *p = ll_partition(l, h, comp, swap);
    ll_quicksort(l, p->prev, comp, swap);
    ll_quicksort(p->next, h, comp, swap);
  }
}

/* ============================ */
/* A linked list sort function. */
/* ============================ */
void
ll_sort(ll_t *list,
        int (*comp)(void const *, void const *),
        void (*swap)(void **, void **))
{
  /* Call the recursive ll_quicksort function. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  ll_quicksort(list->head, list->tail, comp, swap);
}

/* ==========================================================================*/
/* Finds a node in the list containing data. Return the node pointer or NULL */
/* if not found.                                                             */
/* A comparison function must be provided to compare a and b (strcmp like).  */
/* ==========================================================================*/
ll_node_t *
ll_find(ll_t * const list,
        void * const data,
        int (*cmpfunc)(const void *, const void *))
{
  ll_node_t *node;

  if (NULL == (node = list->head))
    return NULL;

  do
  {
    if (0 == cmpfunc(node->data, data))
      return node;
  } while (NULL != (node = node->next));

  return NULL;
}

/* =============================================== */
/* Free all the elements of a list (make it empty) */
/* NULL or a custom function may be used to free   */
/* the sub components of the elements.             */
/* =============================================== */
void
ll_free(ll_t * const list, void (*clean)(void *))
{
  if (list)
  {
    ll_node_t *node = list->head;

    while (node)
    {
      /* Apply a custom cleaner if not NULL. */
      /* """"""""""""""""""""""""""""""""""" */
      if (clean)
        clean(node->data);

      ll_delete(list, node);

      node = list->head;
    }
  }
}

/* ==================================== */
/* Destroy a list and all its elements. */
/* ==================================== */
void
ll_destroy(ll_t *list, void (*clean)(void *))
{
  if (list)
  {
    ll_free(list, clean);
    free(list);
  }
}
