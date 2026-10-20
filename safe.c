/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#include "safe.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

/* Private helper for functions reporting failure using -1.           */
/* result must be a plain local variable without side effects.        */
/* expression is evaluated once per attempt.                          */
/* errno is inspected only after failure and is not reset on success. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define SAFE_RETRY_MINUS_ONE(result, expression) \
  do                                             \
  {                                              \
    do                                           \
    {                                            \
      (result) = (expression);                   \
    } while ((result) == -1 && errno == EINTR);  \
  } while (0)

/* ==================================================== */
/* Read bytes, preserving short reads and zero results. */
/* ==================================================== */
ssize_t
read_safe(int fd, void *buf, size_t count)
{
  ssize_t res;

  SAFE_RETRY_MINUS_ONE(res, read(fd, buf, count));
  return res;
}

/* ====================================================== */
/* Write bytes, preserving short writes and zero results. */
/* ====================================================== */
ssize_t
write_safe(int fd, const void *buf, size_t count)
{
  ssize_t res;

  SAFE_RETRY_MINUS_ONE(res, write(fd, buf, count));
  return res;
}

/* ================================================================= */
/* Open a file using the mode supplied by the public dispatch macro. */
/* ================================================================= */
int
open_safe_impl(const char *pathname, int flags, mode_t mode)
{
  int res;

  SAFE_RETRY_MINUS_ONE(res, open(pathname, flags, mode));
  return res;
}

/* ============================================================= */
/* Open a stream, retrying only failures caused by interruption. */
/* ============================================================= */
FILE *
fopen_safe(const char * restrict pathname, const char * restrict mode)
{
  FILE *file;

  do
  {
    file = fopen(pathname, mode);
  } while (file == NULL && errno == EINTR);

  return file;
}

/* ========================================================= */
/* Apply terminal attributes, retrying interrupted attempts. */
/* ========================================================= */
int
tcsetattr_safe(int fd, int actions, const struct termios *attributes)
{
  int res;

  SAFE_RETRY_MINUS_ONE(res, tcsetattr(fd, actions, attributes));
  return res;
}

#undef SAFE_RETRY_MINUS_ONE

/* =================================================================== */
/* Write a complete string using descriptor I/O and progress tracking. */
/* =================================================================== */
int
puts_fd_safe(const char *s, int fd)
{
  size_t  remaining;
  size_t  count;
  ssize_t res;

  remaining = strlen(s);

  while (remaining != 0)
  {
    count = remaining;
    if (count > (size_t)SSIZE_MAX)
      count = (size_t)SSIZE_MAX;

    res = write_safe(fd, s, count);
    if (res == -1)
      return -1;

    if (res == 0)
    {
      errno = EIO;
      return -1;
    }

    s += (size_t)res;
    remaining -= (size_t)res;
  }

  return 0;
}
