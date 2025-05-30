/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef INDEX_H
#define INDEX_H

#include <wchar.h>

/* *************************************** */
/* Ternary Search Tree specific structures */
/* *************************************** */

typedef struct tst_node_s tst_node_t;
typedef struct sub_tst_s  sub_tst_t;

#if 0 /* here for coherency but not used. */
void tst_cleanup(tst_node_t * p);
#endif

tst_node_t *
tst_insert(tst_node_t *p, wchar_t *w, void *data);

int
my_wcscasecmp(const wchar_t *w1s, const wchar_t *w2s);

void *
tst_prefix_search(tst_node_t *root, wchar_t *w, int (*callback)(void *));

void *
tst_search(tst_node_t *root, wchar_t *w);

int
tst_traverse(tst_node_t *p, int (*callback)(void *), int first_call);

int
tst_substring_traverse(tst_node_t *p,
                       int (*callback)(void *),
                       int     first_call,
                       wchar_t w);
int
tst_fuzzy_traverse(tst_node_t *p,
                   int (*callback)(void *),
                   int     first_call,
                   wchar_t w);

void *
tst_search_in_word(tst_node_t *root, wchar_t w);

sub_tst_t *
sub_tst_new(void);

void
append_tst_search_list(char *glyph, long l);

void
insert_sorted_ptr(tst_node_t ***array,
                  long         *size,
                  long         *filled,
                  tst_node_t   *ptr);

/* Ternary node structure */
/* """""""""""""""""""""" */
struct tst_node_s
{
  tst_node_t *lokid, *eqkid, *hikid;
  void       *data;
  wchar_t     splitchar;
};

/* Structure to contain data and metadata attached to a fuzzy/substring. */
/* search step.                                                          */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct sub_tst_s
{
  tst_node_t **array;
  long         size;
  long         count;
};

#endif
