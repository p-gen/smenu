/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef UTF8_H
#define UTF8_H

#include <stddef.h> /* for size_t.   */
#include <stdint.h> /* for uint32_t. */

typedef struct langinfo_s langinfo_t;

/* Locale information. */
/* """"""""""""""""""" */
struct langinfo_s
{
  int utf8; /* charset is UTF-8              */
  int bits; /* number of bits in the charset */
};

int
utf8_get_length(unsigned char c);

size_t
utf8_offset(char const *, size_t);

char *
utf8_strhead(char *d, char const *s, long n, long *pos);

size_t
utf8_strlen(char const *str);

int
cptoutf8(char *utf8_str, uint32_t c);

uint32_t
utf8tocp(char *utf8_str);

uint32_t *
utf8stocps(char *utf8_str);

int
utf8_interpret(char *s, int is_utf8, char sci, int latin1_compatible);

char *
utf8_validate(unsigned char *str);

char *
utf8_prev(const char *str, const char *p);

char *
utf8_next(char *p);

void
utf8_strtolower(char *dst, char *src);

int
utf8_width(const unsigned char *s);

long
utf8_strwidth(const unsigned char *s, size_t n);

int
terminal_width_utf8_dfa(const unsigned char *utf8, size_t length);
#endif
