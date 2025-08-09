/* **************************** */
/* Memory management functions. */
/* **************************** */

/* ***************************************************************** */
/* Created by Kevin Locke (from numerous canonical examples).        */
/*                                                                   */
/* Adapted for use by smenu.                                         */
/*                                                                   */
/* I hereby place this file in the public domain.  It may be freely  */
/* reproduced, distributed, used, modified, built upon, or otherwise */
/* employed by anyone for any purpose without restriction.           */
/* ***************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "xmalloc.h"

/* The following rpl_* function are necessary for AIX which doesn't     */
/* provide 'GNU compatible' allocation functions.                       */
/* Every call to malloc()/realloc() is then replaced by a call to       */
/* rpl_malloc()/rpl_realloc() as defined in the GNU generated config.h. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

#ifdef malloc

#undef malloc
extern void *malloc(size_t);

void *
rpl_malloc(size_t size)
{
  if (!size)
    size++;
  return malloc(size);
}

#undef realloc
extern void *
realloc(void *, size_t);

void *
rpl_realloc(void *ptr, size_t size)
{
  if (!size)
    size++;
  return (ptr ? realloc(ptr, size) : malloc(size));
}

#endif

/* ================================================================== */
/* Customized malloc.                                                 */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xmalloc(size_t size)
{
  void  *allocated;
  size_t real_size;

  real_size = (size > 0) ? size : 1;
  allocated = malloc(real_size);
  if (allocated == NULL)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to malloc %zu bytes)\n",
            size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================================================================== */
/* Customized calloc.                                                 */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xcalloc(size_t n, size_t size)
{
  void *allocated;

  n         = (n > 0) ? n : 1;
  size      = (size > 0) ? size : 1;
  allocated = calloc(n, size);
  if (allocated == NULL)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to calloc %zu bytes)\n",
            size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================================================================== */
/* Customized realloc.                                                */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xrealloc(void *p, size_t size)
{
  void *allocated;

  allocated = realloc(p, size);
  if (allocated == NULL && size > 0)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to realloc %zu bytes)\n",
            size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================================================================== */
/* strdup implementation using xmalloc.                               */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
char *
xstrdup(const char *str)
{
  char *p;

  p = malloc(strlen(str) + 1);

  if (p == NULL)
  {
    fprintf(stderr, "Error: Insufficient memory for xstrdup.\n");
    exit(EXIT_FAILURE);
  }

  strcpy(p, str);

  return p;
}

/* ================================================================== */
/* strndup implementation using xmalloc.                              */
/* This version guarantees that there is a final '\0'.                */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
char *
xstrndup(const char *str, size_t len)
{
  char *p;

  p = memchr(str, '\0', len);

  if (p != NULL)
    len = p - str;

  p = malloc(len + 1);

  if (p == NULL)
  {
    fprintf(stderr, "Error: Insufficient memory for xstrndup.\n");
    exit(EXIT_FAILURE);
  }

  memcpy(p, str, len);
  p[len] = '\0';

  return p;
}
