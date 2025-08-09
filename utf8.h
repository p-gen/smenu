/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

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
utf8_strprefix(char *d, char const *s, long n, long *pos);

size_t
utf8_strlen(char const *str);

wchar_t *
utf8_strtowcs(char *s);

void
utf8_sanitize(char *s, char sc);

int
cptoutf8(char *utf8_str, uint32_t c);

int
utf8_interpret(char *s, char sc);

char *
utf8_validate(char *str);

char *
utf8_prev(const char *str, const char *p);

char *
utf8_next(char *p);

void
utf8_strtolower(char *dst, char *src);

#endif
