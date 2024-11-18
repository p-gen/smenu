/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct interval_s interval_t;
typedef struct range_s    range_t;

struct interval_s
{
  long low;
  long high;
};

/* Structure used by the replace function to delimit matches */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct range_s
{
  size_t start;
  size_t end;
};

interval_t *
interval_new(void);

int
interval_comp(void const *a, void const *b);

void
interval_swap(void **a, void **b);

void
optimize_an_interval_list(ll_t *list);

char *
concat(const char *str1, ...);

int
strprefix(char *str1, char *str2);

void
ltrim(char *str, const char *trim);

void
rtrim(char *str, const char *trim, size_t min_len);

int
my_strcasecmp(const char *str1, const char *str2);

char *
my_strcpy(char *dst, char *src);

int
isprint7(int i);

int
isprint8(int i);

int
my_wcscasecmp(const wchar_t *w1s, const wchar_t *w2s);

int
is_integer(const char * const s);

int
swap_string_parts(char **s, size_t first);

void
strrep(char *s, const char c1, const char c2);

char *
strprint(char const *s);

void
hexdump(const char *buf, FILE *fp, const char *prefix, size_t size);

int
my_wcswidth(const wchar_t *s, size_t n);

long
get_sorted_array_target_pos(long *array, long nb, long value);

int
is_in_foreground_process_group(void);

int
isempty_non_utf8(const unsigned char *s);

int
isempty_utf8(const unsigned char *s);

#endif
