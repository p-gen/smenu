/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef LIST_H
#define LIST_H

typedef struct ll_node_s ll_node_t;
typedef struct ll_s      ll_t;

/* ******************************* */
/* Linked list specific structures */
/* ******************************* */

/* Linked list node structure */
/* """""""""""""""""""""""""" */
struct ll_node_s
{
  void *             data;
  struct ll_node_s * next;
  struct ll_node_s * prev;
};

/* Linked List structure */
/* """"""""""""""""""""" */
struct ll_s
{
  ll_node_t * head;
  ll_node_t * tail;
  long        len;
};

void
ll_append(ll_t * const list, void * const data);

#if 0
void
ll_prepend(ll_t * const list, void * const data);
#endif

#if 0
void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data);
#endif

#if 0
void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data);
#endif

void
ll_sort(ll_t * list, int (*comp)(void *, void *),
        void (*swap)(void **, void **));

int
ll_delete(ll_t * const list, ll_node_t * node);

ll_node_t *
ll_find(ll_t * const, void * const, int (*)(const void *, const void *));

void
ll_init(ll_t * list);

ll_node_t *
ll_new_node(void);

ll_t *
ll_new(void);

#endif
