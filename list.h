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

ll_t *
ll_new(void);

void
ll_init(ll_t * list);

ll_node_t *
ll_new_node(void);

void
ll_append(ll_t * const list, void * const data);

void
ll_sort(ll_t * list, int (*comp)(void const *, void const *),
        void (*swap)(void **, void **));

int
ll_delete(ll_t * const list, ll_node_t * node);

ll_node_t *
ll_find(ll_t * const, void * const, int (*)(void const *, void const *));

void
ll_free(ll_t * const list, void (*clean)(void *));

void
ll_destroy(ll_t * list, void (*clean)(void *));

#endif
