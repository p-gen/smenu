/* ----------------------------------------------------------- */
/* darray.c                                                    */
/* Inspired by dynarray.c from Bob Dondero used for the COS217 */
/* course at www.cs.princeton.edu                              */
/* ----------------------------------------------------------- */

#include <stdlib.h>
#include "darray.h"
#include "xmalloc.h"

enum
{
  MIN_PHYS_LENGTH = 2
};

enum
{
  GROWTH_FACTOR = 2
};

/* A darray consists of an array, along with its logical and */
/* physical lengths.                                         */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct darray
{
  int    length;   /* The number of elements in the darray     *
                    | from the client's point of view.         */
  int    capacity; /* The number of elements in the array that *
                    | underlies the darray.                    */
  void **array;    /* The array that underlies the darray.     */
};

/* Notes:                                                              */
/* - As all memory allocations defined in xmalloc.h are fatal          */
/*   no checking is done when using these functions.                   */
/*   It is also assumed that the user will not use out of bound        */
/*   indexes or lengths when using function implemented in this file.  */
/* - All the sizes are in int which should be enough for our use case. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* ======================================================== */
/* Return a new darray_t * whose length is length.          */
/* ======================================================== */
darray_t *
darray_new(int length)
{
  darray_t *darray;

  darray         = (struct darray *)xmalloc(sizeof(*darray));
  darray->length = length;
  if (length > MIN_PHYS_LENGTH)
    darray->capacity = length;
  else
    darray->capacity = MIN_PHYS_LENGTH;
  darray->array = (void **)xcalloc((size_t)darray->capacity, sizeof(void *));

  return darray;
}

/* ============ */
/* Free darray. */
/* ============ */
void
darray_free(darray_t *darray, void (*free_item)(void *item))
{
  int i;

  if (free_item != NULL)
    for (i = 0; i < darray->length; i++)
      (*free_item)((void *)darray->array[i]);

  free(darray->array);
  free(darray);
}

/* ==================================================== */
/* Return the length of darray.                         */
/* ==================================================== */
int
darray_get_length(darray_t *darray)
{
  return darray->length;
}

/* ======================================================== */
/* Expose the internet array pointer of pointer to optimize */
/* read-only access.                                        */
/* ======================================================== */
void **
darray_get_array(darray_t *darray)
{
  return darray->array;
}

/* ====================================== */
/* Return the index'th element of darray. */
/* ====================================== */
void *
darray_get(darray_t *darray, int index)
{
  return (void *)(darray->array)[index];
}

/* ============================================== */
/* Assign item to the index'th element of darray. */
/* The old item is returned.                      */
/* ============================================== */
void *
darray_put(darray_t *darray, int index, void *item)
{
  void *old_item;

  old_item             = darray->array[index];
  darray->array[index] = item;

  return (void *)old_item;
}

/* ===================================== */
/* Double the physical length of darray. */
/* ===================================== */
static void
darray_grow(darray_t *darray)
{
  darray->capacity *= GROWTH_FACTOR;
  darray->array = (void **)xrealloc(darray->array,
                                    sizeof(void *) * darray->capacity);
}

/* ============================================================ */
/* Add item to the end of darray, thus incrementing its length. */
/* ============================================================ */
void
darray_add(darray_t *darray, void *item)
{
  if (darray->length == darray->capacity)
    darray_grow(darray);

  darray->array[darray->length] = item;
  darray->length++;
}

/* ======================================= */
/* Fill array with the elements of darray. */
/* ======================================= */
void
darray_to_array(darray_t *darray, void **array)
{
  int i;

  for (i = 0; i < darray->length; i++)
    array[i] = (void *)darray->array[i];
}

/* =========================================================== */
/* Apply function *apply to each element of darray, passing    */
/* extra as an extra argument.  That is, for each element item */
/* of darray, call (*apply)(item, extra).                      */
/* =========================================================== */
void
darray_map(darray_t *darray,
           void (*apply)(void *item, void *extra),
           void *extra)
{
  int i;

  for (i = 0; i < darray->length; i++)
    (*apply)((void *)darray->array[i], (void *)extra);
}

/* ================================================================= */
/* Sort darray in the order determined by *compare.                  */
/* *compare should return <0, 0, or >0 depending upon whether *item1 */
/* is less than, equal to, or greater than *item2, respectively.     */
/* ================================================================= */
void
darray_sort(darray_t *darray, int (*compare)(void *item1, void *item2))
{
  int   outer;
  int   inner;
  int   comp;
  void *temp;

  for (outer = 1; outer < darray->length; outer++)
    for (inner = outer; inner > 0; inner--)
    {
      comp = (*compare)(darray->array[inner], darray->array[inner - 1]);
      if (comp < 0)
      {
        temp                     = darray->array[inner];
        darray->array[inner]     = darray->array[inner - 1];
        darray->array[inner - 1] = temp;
      }
    }
}

/* =========================================================== */
/* Linear search darray for *sought_item using *compare to     */
/* determine equality.  Return the index at which *sought_item */
/* is found, or -1 if there is no such index.                  */
/* *compare should return 0 if *item1 is equal to item2,       */
/* and non-0 otherwise.                                        */
/* =========================================================== */
int
darray_search(darray_t *darray,
              void     *sought_item,
              int (*compare)(void *item1, void *item2))
{
  int i;

  for (i = 0; i < darray->length; i++)
    if ((*compare)(darray->array[i], sought_item) == 0)
      return i;
  return -1;
}

/* ================================================================= */
/* Binary search darray for *sought_item using *compare to           */
/* determine equality.  Return the index at which *sought_item       */
/* is found, or -1 if there is no such index.                        */
/* *compare should return <0, 0, or >0 depending upon whether *item1 */
/* is less than, equal to, or greater than *item2, respectively.     */
/* ================================================================= */
int
darray_bsearch(darray_t *darray,
               void     *sought_item,
               int (*compare)(void *item1, void *item2))
{
  int left  = 0;
  int right = darray->length - 1;
  int mid;
  int comp;

  while (left <= right)
  {
    mid  = (left + right) / 2;
    comp = (*compare)(sought_item, darray->array[mid]);
    if (comp == 0)
      return mid;
    if (comp < 0)
      right = mid - 1;
    else
      left = mid + 1;
  }

  return -1;
}
