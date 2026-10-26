/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef SAFE_H
#define SAFE_H

#include <stdio.h>     /* for size_t, FILE.    */
#include <sys/types.h> /* for ssize_t, mode_t. */

struct termios;

/* Retry only failures caused by EINTR.                             */
/* Preserve successful results and all other errors unchanged.      */
/* No argument validation or full-transfer loops are performed.     */
/* Repeated interruptions can cause indefinite retries.             */
/* Do not use these wrappers when interruption should end the wait. */
/* errno is meaningful on failure; it is not reset on success.      */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* ================================ */
/* Read up to count bytes into buf. */
/* ================================ */
ssize_t
read_safe(int fd, void *buf, size_t count);

/* ================================= */
/* Write up to count bytes from buf. */
/* ================================= */
ssize_t
write_safe(int fd, const void *buf, size_t count);

/* ================================================================== */
/* Open pathname using flags and possibly the explicit creation mode. */
/* ================================================================== */
int
open_safe_impl(const char *pathname, int flags, mode_t mode);

#define SAFE_OPEN_2(pathname, flags) \
  open_safe_impl((pathname), (flags), (mode_t)0)

#define SAFE_OPEN_3(pathname, flags, mode) \
  open_safe_impl((pathname), (flags), (mode))

#define SAFE_OPEN_SELECT(a, b, c, selected, ...) selected

#define open_safe(...) \
  SAFE_OPEN_SELECT(__VA_ARGS__, SAFE_OPEN_3, SAFE_OPEN_2, unused)(__VA_ARGS__)

/* ================================================= */
/* Open pathname using the fopen() mode string mode. */
/* ================================================= */
FILE *
fopen_safe(const char * restrict pathname, const char * restrict mode);

/* ==================================================== */
/* Apply attributes to the terminal associated with fd. */
/* ==================================================== */
int
tcsetattr_safe(int fd, int actions, const struct termios *attributes);

#endif /* SAFE_H */
