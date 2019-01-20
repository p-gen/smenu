/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

#ifndef INDEX_H
#define INDEX_H

/* *************************************** */
/* Ternary Search Tree specific structures */
/* *************************************** */

typedef struct tst_node_s tst_node_t;
typedef struct sub_tst_s  sub_tst_t;

#if 0 /* here for coherency but not used. */
void tst_cleanup(tst_node_t * p);
#endif

tst_node_t *
tst_insert(tst_node_t * p, wchar_t * w, void * data);

void *
tst_prefix_search(tst_node_t * root, wchar_t * w, int (*callback)(void *));

void *
tst_search(tst_node_t * root, wchar_t * w);

int
tst_traverse(tst_node_t * p, int (*callback)(void *), int first_call);

int
tst_substring_traverse(tst_node_t * p, int (*callback)(void *), int first_call,
                       wchar_t w);
int
tst_fuzzy_traverse(tst_node_t * p, int (*callback)(void *), int first_call,
                   wchar_t w);

sub_tst_t *
sub_tst_new(void);

void
insert_sorted_index(long ** array, long * size, long * filled, long value);

void
insert_sorted_ptr(tst_node_t *** array, unsigned long long * size,
                  unsigned long long * filled, tst_node_t * ptr);

/* Ternary node structure */
/* """""""""""""""""""""" */
struct tst_node_s
{
  wchar_t splitchar;

  tst_node_t *lokid, *eqkid, *hikid;
  void *      data;
};

/* Structure to contain data and metadata attached to a fuzzy/substring. */
/* search step.                                                          */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct sub_tst_s
{
  tst_node_t **      array;
  unsigned long long size;
  unsigned long long count;
};

#endif
