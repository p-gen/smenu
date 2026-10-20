/* **************************** */
/* Memory management functions. */
/* **************************** */

/* ***************************************************************** */
/* Created by Kevin Locke (from numerous canonical examples).        */
/*                                                                   */
/* I hereby place this file in the public domain.  It may be freely  */
/* reproduced, distributed, used, modified, built upon, or otherwise */
/* employed by anyone for any purpose without restriction.           */
/*                                                                   */
/* Adapted for use by smenu.                                         */
/* ***************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmalloc.h"

/* The following rpl_* function are necessary for AIX which doesn't     */
/* provide 'GNU compatible' allocation functions.                       */
/* Every call to malloc()/realloc() is then replaced by a call to       */
/* rpl_malloc()/rpl_realloc() as defined in the GNU generated config.h. */
/* Zero-size allocations are converted to one-byte allocations.         */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

#ifdef malloc

#undef malloc
extern void *
malloc(size_t);

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
/* Allocates at least one byte when size is 0.                        */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xmalloc(size_t size)
{
  void *allocated;

  if (size == 0)
    size = 1;

  allocated = malloc(size);

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
/* Allocates at least one element and one byte per element.           */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xcalloc(size_t n, size_t size)
{
  void *allocated;

  if (n == 0)
    n = 1;

  if (size == 0)
    size = 1;

  allocated = calloc(n, size);

  if (allocated == NULL)
  {
    fprintf(stderr,
            "Error: Insufficient memory (attempt to calloc %zu elements "
            "of %zu bytes)\n",
            n,
            size);

    exit(EXIT_FAILURE);
  }

  return allocated;
}

/* ================================================================== */
/* Customized realloc.                                                */
/* Reallocates at least one byte when size is 0.                      */
/* Displays an error message and exits gracefully if an error occurs. */
/* ================================================================== */
void *
xrealloc(void *p, size_t size)
{
  void *allocated;

  if (size == 0)
    size = 1;

  allocated = realloc(p, size);

  if (allocated == NULL)
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

  p = xmalloc(strlen(str) + 1);

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
  char  *p;
  size_t n;

  for (n = 0; n < len && str[n] != '\0'; n++)
    ;

  len = n;

  p = xmalloc(len + 1);

  memcpy(p, str, len);
  p[len] = '\0';

  return p;
}
