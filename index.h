#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>

#define GAP 1

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct tst_node_s         tst_node_t;
typedef struct sub_tst_s          sub_tst_t;
typedef struct tst_pretty_block_s tst_pretty_block_t;

struct tst_node_s
{
  tst_node_t *low, *equal, *high;
  void       *data;
  int32_t     codepoint;
};

struct sub_tst_s
{
  tst_node_t **array;
  long         size;
  long         count;
};

/* Temporary render block for pretty-printing.           */
/* line   : array of text lines                          */
/* width  : width  of the block                          */
/* height : height of the block                          */
/* root   : x-coordinate of the node drawn in this block */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
struct tst_pretty_block_s
{
  char **line;
  int    width;
  int    height;
  int    root;
};

int
utf8_get_length(unsigned char c);

tst_node_t *
tst_insert(tst_node_t *p, char *s, void *data);

void
tst_cleanup(tst_node_t *p);

void *
tst_search(tst_node_t *root, char *s);

void *
tst_prefix_search(tst_node_t *root, char *s, int (*callback)(void *));

int
tst_traverse(tst_node_t *p, int (*callback)(void *), int first_call);

int
tst_substring_traverse(tst_node_t *p,
                       int (*callback)(void *),
                       int     first_call,
                       int32_t codepoint);

int
tst_fuzzy_traverse(tst_node_t *p,
                   int (*callback)(void *),
                   int     first_call,
                   int32_t codepoint);

void *
tst_search_in_word(tst_node_t *root, int32_t codepoint);

void
tst_pretty_print(tst_node_t *root);

sub_tst_t *
sub_tst_new(void);

void
append_tst_search_list(char *character, long length);

void
insert_sorted_ptr(tst_node_t ***array,
                  long         *size,
                  long         *filled,
                  tst_node_t   *ptr);

#endif
