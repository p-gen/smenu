/* ------------------------------------------------------------- */
/* darray.h                                                      */
/* Inspired by dynarray.h from Bob Dondero used for the COS217   */
/* course at www.cs.princeton.edu                                */
/* ------------------------------------------------------------- */

#ifndef DARRAY_H
#define DARRAY_H

/* A darray_t * is an array whose length can expand dynamically. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
typedef struct darray darray_t;

/* Return a new darray_t * whose length is length.          */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
darray_t *
darray_new(int length);

/* Free darray. */
/* """""""""""" */
void
darray_free(darray_t *darray, void (*free_item)(void *item));

/* Return the internal array of pointer of the darray.              */
/* This array must not be altered and must be considered read-only. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
void **
darray_get_array(darray_t *darray);

/* Return the length of darray. */
/* """""""""""""""""""""""""""" */
int
darray_get_length(darray_t *darray);

/* Return the index'th element of darray. */
/* """""""""""""""""""""""""""""""""""""" */
void *
darray_get(darray_t *darray, int index);

/* Assign item to the index'th element of darray. */
/* Returns the old item.                          */
/* """""""""""""""""""""""""""""""""""""""""""""" */
void *
darray_put(darray_t *darray, int index, void *item);

/* Add item to the end of darray, thus incrementing its length. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
void
darray_add(darray_t *darray, void *item);

/* Fill array with the elements of darray. */
/* """"""""""""""""""""""""""""""""""""""" */
void
darray_to_array(darray_t *darray, void **array);

/* Apply function *apply to each element of darray, passing extra   */
/* as an extra argument.  That is, for each element item of darray, */
/* call (*apply)(item, extra).                                      */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
void
darray_map(darray_t *darray,
           void (*apply)(void *item, void *extra),
           void *extra);

/* Sort darray in the order determined by *compare.                   */
/* *compare should return -<0, 0, or >0 depending upon whether *item1 */
/* is less than, equal to, or greater than *item2, respectively.      */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
void
darray_sort(darray_t *darray, int (*compare)(void *item1, void *item2));

/* Linear search darray for *sought_item using *compare to     */
/* determine equality.  Return the index at which *sought_item */
/* is found, or -1 if there is no such index.                  */
/* *compare should return 0 if *item1 is equal to item2,       */
/* and non-0 otherwise.                                        */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
int
darray_search(darray_t *darray,
              void     *sought_item,
              int (*compare)(void *item1, void *item2));

/* Binary search darray for *sought_item using *compare to           */
/* determine equality.  Return the index at which *sought_item       */
/* is found, or -1 if there is no such index.                        */
/* *compare should return <0, 0, or >0 depending upon whether *item1 */
/* is less than, equal to, or greater than *item2, respectively.     */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
int
darray_bsearch(darray_t *darray,
               void     *sought_item,
               int (*compare)(void *item1, void *item2));

#endif
