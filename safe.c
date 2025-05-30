/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* ************************************ */
/* Some wrappers to manage EINTR errors */
/* ************************************ */

#include <errno.h>
#include "safe.h"

FILE *
fopen_safe(const char * restrict stream, const char * restrict mode)
{
  FILE *file;

  while ((file = fopen(stream, mode)) == NULL && errno == EINTR)
    ;

  return file;
}

int
tcsetattr_safe(int                   fildes,
               int                   optional_actions,
               const struct termios *termios_p)
{
  int res;

  while ((res = tcsetattr(fildes, optional_actions, termios_p)) == -1
         && errno == EINTR)
    ;

  return res;
}

int
fputc_safe(int c, FILE *stream)
{
  int res;

  while ((res = fputc(c, stream)) == -1 && errno == EINTR)
    ;

  return res;
}

int
fputs_safe(const char * restrict s, FILE * restrict stream)
{
  int res;

  while ((res = fputs(s, stream)) == -1 && errno == EINTR)
    ;

  return res;
}
