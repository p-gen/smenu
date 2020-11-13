/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

/* ********************************************************************* */
/* Tiny linked list implementation.                                      */
/*                                                                       */
/* Each node contain a void pointer to some opaque data, these functions */
/* will not try to allocate or free this data pointer.                   */
/*                                                                       */
/* Also accessors are not provided, the user has to directly manipulate  */
/* the structure members (head, tail, len, data, prev, next).             */
/* ********************************************************************* */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "xmalloc.h"
#include "list.h"

static ll_node_t *
ll_partition(ll_node_t * l, ll_node_t * h, int (*comp)(void *, void *),
             void (*swap)(void *, void *));

/* ========================== */
/* Creates a new linked list. */
/* ========================== */
ll_t *
ll_new(void)
{
  ll_t * ret = xmalloc(sizeof(ll_t));
  ll_init(ret);

  return ret;
}

/* ========================== */
/* Initializes a linked list. */
/* ========================== */
void
ll_init(ll_t * list)
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
  ll_node_t * ret = xmalloc(sizeof(ll_node_t));

  return ret;
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
  ll_node_t * node;

  node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                         * uses xmalloc which does not return if there *
                         * is an allocation error.                     */

  node->data = data;
  node->next = NULL;

  node->prev = list->tail;
  if (list->tail)
    list->tail->next = node;
  else
    list->head = node;

  list->tail = node;

  ++list->len;
}

#if 0
/* ==================================================================== */
/* Puts a new node filled with its data at the beginning of the linked  */
/* list. The user is responsible for the memory management of the data. */
/*                                                                      */
/* Note: list is assumed to be initialized by ll_new().                 */
/* ==================================================================== */
void
ll_prepend(ll_t * const list, void * const data)
{
  ll_node_t * node;

  node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                         * uses xmalloc which does not return if there *
                         * is an allocation error.                     */

  node->data = data;
  node->prev = NULL;

  node->next = list->head;
  if (list->head)
    list->head->prev = node;
  else
    list->tail = node;

  list->head = node;

  ++list->len;
}
#endif

#if 0
/* ========================================================= */
/* Inserts a new node before the specified node in the list. */
/* ========================================================= */
void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (node->prev == NULL)
    ll_prepend(list, data);
  else
  {
    new_node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                               * uses xmalloc which does not return if there *
                               * is an allocation error.                     */

    new_node->data   = data;
    new_node->next   = node;
    new_node->prev   = node->prev;
    node->prev->next = new_node;
    node->prev       = new_node;

    ++list->len;
  }
}
#endif

#if 0
/* ======================================================== */
/* Inserts a new node after the specified node in the list. */
/* ======================================================== */
void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (node->next == NULL)
    ll_append(list, data);
  else
  {
    new_node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                               * uses xmalloc which does not return if there *
                               * is an allocation error.                     */

    new_node->data   = data;
    new_node->prev   = node;
    new_node->next   = node->next;
    node->next->prev = new_node;
    node->next       = new_node;

    ++list->len;
  }
}
#endif

/* ====================================================== */
/* Partition code for the quicksort function.             */
/* Based on code found here:                              */
/* http://www.geeksforgeeks.org/quicksort-for-linked-list */
/* ====================================================== */
static ll_node_t *
ll_partition(ll_node_t * l, ll_node_t * h, int (*comp)(void *, void *),
             void (*swap)(void *, void *))
{
  /* Considers last element as pivot, places the pivot element at its       */
  /* correct position in sorted array, and places all smaller (smaller than */
  /* pivot) to left of pivot and all greater elements to right of pivot.    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  /* Set pivot as h element. */
  /* """"""""""""""""""""""" */
  void * x = h->data;

  ll_node_t * i = l->prev;
  ll_node_t * j;

  for (j = l; j != h; j = j->next)
  {
    if (comp(j->data, x) < 1)
    {
      i = (i == NULL) ? l : i->next;

      swap(i->data, j->data);
    }
  }

  i = (i == NULL) ? l : i->next;
  swap(i->data, h->data);

  return i;
}

/* ======================================================== */
/* A recursive implementation of quicksort for linked list. */
/* ======================================================== */
void
ll_quicksort(ll_node_t * l, ll_node_t * h, int (*comp)(void *, void *),
             void (*swap)(void * a, void *))
{
  if (h != NULL && l != h && l != h->next)
  {
    ll_node_t * p = ll_partition(l, h, comp, swap);
    ll_quicksort(l, p->prev, comp, swap);
    ll_quicksort(p->next, h, comp, swap);
  }
}

/* ============================ */
/* A linked list sort function. */
/* ============================ */
void
ll_sort(ll_t * list, int (*comp)(void *, void *),
        void (*swap)(void * a, void *))
{
  /* Call the recursive ll_quicksort function. */
  /* """"""""""""""""""""""""""""""""""""""""" */
  ll_quicksort(list->head, list->tail, comp, swap);
}

/* ================================== */
/* Removes a node from a linked list. */
/* ================================== */
int
ll_delete(ll_t * const list, ll_node_t * node)
{
  if (list->head == list->tail)
  {
    if (list->head != NULL)
      list->head = list->tail = NULL;
    else
      return 0;
  }
  else if (node->prev == NULL)
  {
    list->head       = node->next;
    list->head->prev = NULL;
  }
  else if (node->next == NULL)
  {
    list->tail       = node->prev;
    list->tail->next = NULL;
  }
  else
  {
    node->next->prev = node->prev;
    node->prev->next = node->next;
  }

  free(node);

  --list->len;

  return 1;
}

/* ==========================================================================*/
/* Finds a node in the list containing data. Return the node pointer or NULL */
/* if not found.                                                             */
/* A comparison function must be provided to compare a and b (strcmp like).  */
/* ==========================================================================*/
ll_node_t *
ll_find(ll_t * const list, void * const data,
        int (*cmpfunc)(const void * a, const void * b))
{
  ll_node_t * node;

  if (NULL == (node = list->head))
    return NULL;

  do
  {
    if (0 == cmpfunc(node->data, data))
      return node;
  } while (NULL != (node = node->next));

  return NULL;
}
