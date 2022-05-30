/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef SAFE_H
#define SAFE_H

int
fputs_safe(const char * restrict s, FILE * restrict stream);

int
fputc_safe(int c, FILE * stream);

int
tcsetattr_safe(int fildes, int optional_actions,
               const struct termios * termios_p);

FILE *
fopen_safe(const char * restrict stream, const char * restrict mode);

#endif
