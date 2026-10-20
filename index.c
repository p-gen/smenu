/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

/* ************************************************************************ */
/* Ternary Search Tree and sorted array creation functions.                 */
/* Inspired by a code described in "Ternary Search Trees" by Jon            */
/* Bentley and Robert Sedgewick in the April, 1998, Dr. Dobb's Journal.     */
/* A Ternary Search Tree pretty printing functions has also been added.     */
/* Links:                                                                   */
/*   https://www.drdobbs.com/database/ternary-search-trees/184410528?pgno=1 */
/*   https://www.cs.princeton.edu/~rs/strings/tstdemo.c.                    */
/* ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "xmalloc.h"
#include "list.h"
#include "utf8.h"
#include "index.h"

/* Global list holding the intermediate candidate subtree sets used */
/* by substring and fuzzy searches.                                 */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
ll_t *tst_search_list;

/* ============================================ */
/* Fold an uppercase ASCII letter to lowercase. */
/* Return all other code points unchanged.      */
/* ============================================ */
static int32_t
fold_ascii(int32_t c)
{
  return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* ============================================================= */
/* Insert UTF-8 string s and its associated data d into the TST. */
/* A node with a zero code point terminates a stored string.     */
/* ============================================================= */
tst_node_t *
tst_insert(tst_node_t *p, char *s, void *d)
{
  int32_t c = utf8tocp(s);

  /* Reject an invalid UTF-8 sequence. */
  /* """"""""""""""""""""""""""""""""" */
  if (c < 0)
    return p;

  /* Allocate and initialize a node when the subtree is empty. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!p)
  {
    p            = xmalloc(sizeof(*p));
    p->codepoint = c;
    p->low = p->equal = p->high = NULL;
    p->data                     = NULL;
  }

  /* Follow a side branch without advancing s because the current */
  /* code point has not yet been matched.                         */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (c < p->codepoint)
    p->low = tst_insert(p->low, s, d);
  else if (c == p->codepoint)
  {
    if (!c)
    {
      /* Store the associated data in the terminal node. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      p->data  = d;
      p->equal = NULL;
    }
    else
    {
      /* Advance past the matched UTF-8 code point. */
      /* """""""""""""""""""""""""""""""""""""""""" */
      int n    = utf8_get_length((unsigned char)*s);
      p->equal = tst_insert(p->equal, s + n, d);
    }
  }
  else
    p->high = tst_insert(p->high, s, d);

  return p;
}

/* ====================================================== */
/* Recursively release all nodes in the tree rooted at p. */
/* Stored data pointers are not released.                 */
/* ====================================================== */
void
tst_cleanup(tst_node_t *p)
{
  if (p)
  {
    tst_cleanup(p->low);

    /* Terminal nodes do not have equal subtrees. */
    /* """""""""""""""""""""""""""""""""""""""""" */
    if (p->codepoint)
      tst_cleanup(p->equal);

    tst_cleanup(p->high);
    free(p);
  }
}

/* ============================================================== */
/* Traverse all terminal nodes and call cb for each stored value. */
/* A nonzero f resets the persistent callback-result accumulator. */
/* Return nonzero if at least one callback returns nonzero.       */
/* ============================================================== */
int
tst_traverse(tst_node_t *p, int (*cb)(void *), int f)
{
  static int r;

  /* Reset the accumulator when starting an outermost traversal. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (f)
    r = 0;

  if (!p)
    return 0;

  tst_traverse(p->low, cb, 0);

  if (p->codepoint)
    tst_traverse(p->equal, cb, 0);
  else if (cb)
    r += cb(p->data);

  tst_traverse(p->high, cb, 0);

  return !!r;
}

/* ==================================================== */
/* Allocate an empty candidate-subtree container.       */
/* Its pointer array initially has room for 64 entries. */
/* ==================================================== */
sub_tst_t *
sub_tst_new(void)
{
  sub_tst_t *e = xmalloc(sizeof(*e));
  e->size      = 64;
  e->count     = 0;
  e->array     = xmalloc((size_t)e->size * sizeof(*e->array));
  return e;
}

/* ============================================================= */
/* Traverse the tree and locate nodes whose code point equals c. */
/* Add each matching equal subtree to the current candidate set. */
/* Call cb for every terminal node encountered by the traversal. */
/* ============================================================= */
int
tst_substring_traverse(tst_node_t *p, int (*cb)(void *), int f, int32_t c)
{
  static int r;

  /* Reset the accumulator when starting an outermost traversal. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (f)
    r = 0;

  if (!p)
    return 0;

  /* Add the continuation of a matching node to the current set. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (p->codepoint == c)
  {
    sub_tst_t *d = tst_search_list->tail->data;
    if (p->equal)
      insert_sorted_ptr(&d->array, &d->size, &d->count, p->equal);
    r = 1;
  }

  tst_substring_traverse(p->low, cb, 0, c);

  if (p->codepoint)
    tst_substring_traverse(p->equal, cb, 0, c);
  else if (cb)
    r += cb(p->data);

  tst_substring_traverse(p->high, cb, 0, c);

  return !!r;
}

/* =============================================================== */
/* Traverse the tree and locate nodes matching c after ASCII fold. */
/* Non-ASCII code points are compared without case conversion.     */
/* Add each matching equal subtree to the current candidate set.   */
/* =============================================================== */
int
tst_fuzzy_traverse(tst_node_t *p, int (*cb)(void *), int f, int32_t c)
{
  static int r;

  /* Reset the accumulator when starting an outermost traversal. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (f)
    r = 0;

  if (!p)
    return 0;

  /* Compare the code points after folding ASCII letter case. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (fold_ascii(p->codepoint) == fold_ascii(c))
  {
    sub_tst_t *d = tst_search_list->tail->data;
    if (p->equal)
      insert_sorted_ptr(&d->array, &d->size, &d->count, p->equal);
    r = 1;
  }

  tst_fuzzy_traverse(p->low, cb, 0, c);

  if (p->codepoint)
    tst_fuzzy_traverse(p->equal, cb, 0, c);
  else if (cb)
    r += cb(p->data);

  tst_fuzzy_traverse(p->high, cb, 0, c);

  return !!r;
}

/* =============================================================== */
/* Search for the exact UTF-8 string s in the tree rooted at p.    */
/* Return its associated data pointer, or NULL if it is not found. */
/* =============================================================== */
void *
tst_search(tst_node_t *p, char *s)
{
  while (p)
  {
    int32_t c = utf8tocp(s);

    /* Stop when an invalid UTF-8 sequence is encountered. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (c < 0)
      return NULL;

    if (c < p->codepoint)
      p = p->low;
    else if (c == p->codepoint)
    {
      /* Return the data when the terminal node is reached. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""" */
      if (!c)
        return p->data;

      s += utf8_get_length((unsigned char)*s);
      p = p->equal;
    }
    else
      p = p->high;
  }

  return NULL;
}

/* ============================================================== */
/* Locate the subtree whose keys begin with the UTF-8 prefix s.   */
/* Traverse its values with cb after the whole prefix is matched. */
/* ============================================================== */
void *
tst_prefix_search(tst_node_t *p, char *s, int (*cb)(void *))
{
  /* Reject an empty prefix. */
  /* """""""""""""""""""""""" */
  if (!*s)
    return NULL;

  while (p)
  {
    int32_t c = utf8tocp(s);

    /* Stop when an invalid UTF-8 sequence is encountered. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (c < 0)
      return NULL;

    if (c < p->codepoint)
      p = p->low;
    else if (c == p->codepoint)
    {
      s += utf8_get_length((unsigned char)*s);

      /* Traverse all keys below the completely matched prefix. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (!*s)
        return (void *)(long)tst_traverse(p->equal, cb, 1);

      p = p->equal;
    }
    else
      p = p->high;
  }

  return NULL;
}

/* =============================================================== */
/* Search for code point c along one equal branch of a stored key. */
/* Return the matching node, or NULL if no such node is found.     */
/* =============================================================== */
void *
tst_search_in_word(tst_node_t *p, int32_t c)
{
  while (p && p->codepoint)
  {
    if (p->codepoint == c)
      return p;
    p = p->equal;
  }

  return NULL;
}

/* ========================================================== */
/* Decode the first code point in g and search for it in each */
/* equal chain contained in the current candidate set.        */
/* Append the resulting candidate set to tst_search_list.     */
/* ========================================================== */
void
append_tst_search_list(char *g, long l)
{
  int        n = utf8_get_length((unsigned char)*g);
  int32_t    c;
  sub_tst_t *o, *w;
  long       i;

  /* Reject an invalid, empty, or incomplete UTF-8 sequence. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!n || n > l || (c = utf8tocp(g)) < 0)
    return;

  /* The tail contains the candidate set from the previous step. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  o = tst_search_list->tail->data;
  w = sub_tst_new();

  for (i = 0; i < o->count; i++)
  {
    tst_node_t *q = tst_search_in_word(o->array[i], c);
    if (q)
      insert_sorted_ptr(&w->array, &w->size, &w->count, q);
  }

  ll_append(tst_search_list, w);
}

/* =========================================================== */
/* Insert p into an array sorted by increasing pointer value.  */
/* Ignore duplicate pointers and grow the array by 64 entries. */
/* =========================================================== */
void
insert_sorted_ptr(tst_node_t ***a, long *s, long *n, tst_node_t *p)
{
  long pos;
  long l = 0;
  long r = *n;

  /* Locate the insertion position using a binary search. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  while (l < r)
  {
    long m;

    m = (l + r) / 2;

    if ((*a)[m] == p)
      return;
    if ((uintptr_t)p < (uintptr_t)(*a)[m])
      r = m;
    else
      l = m + 1;
  }

  pos = l;

  /* Expand the array when no unused entry remains. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (*n == *s)
  {
    *s += 64;
    *a = xrealloc(*a, (size_t)*s * sizeof(**a));
  }

  /* Move the following entries one position to the right. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (*n > pos)
    memmove(*a + pos + 1, *a + pos, (size_t)(*n - pos) * sizeof(**a));

  (*a)[pos] = p;
  (*n)++;
}

/* The following functions allow you to draw a ternary search tree as         */
/* multi-line ASCII art.                                                      */
/*                                                                            */
/* Algorithm:                                                                 */
/*                                                                            */
/* The tree is rendered recursively from the leaves toward the root. Each     */
/* recursive call to build() produces a rectangular block containing an       */
/* array of fixed-width text lines, the dimensions of the block, and the      */
/* horizontal position of the subtree root.                                   */
/*                                                                            */
/* For each node, build() first constructs the blocks for its low, equal,     */
/* and high children. The low block is placed to the left, the equal block    */
/* directly below the node, and the high block to the right.                  */
/*                                                                            */
/* The current node is aligned with the root of its equal child whenever      */
/* an equal child exists.                                                     */
/* This keeps equal branches, which represent successive characters of a key, */
/* vertically uninterrupted.                                                  */
/* Without an equal child, the root position is derived from the low and      */
/* high blocks that exist.                                                    */
/*                                                                            */
/* Low and high branches are connected with '/', '\\', and '_' characters.    */
/* An ASCII split character occupies one column. A non-ASCII character uses   */
/* two columns and is rendered as a code point in hexadecimal between upper   */
/* and lower "::" delimiters. A zero split character is rendered as '*'.      */
/*                                                                            */
/* After the root block has been assembled, trailing spaces are removed from  */
/* each line before the result is written to standard output.                 */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

/* Minimum horizontal gap between adjacent subtree blocks. */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define GAP 1

/* Rectangular block containing a rendered subtree. */
/* """""""""""""""""""""""""""""""""""""""""""""""" */
typedef struct block_s
{
  char **line;
  int    width;
  int    height;
  int    root;
} block_t;

/* ======================================================================== */
/* Encode a Unicode code point to its ASCII representation in hexadecimal.  */
/* If the code point argument c is > 0, return the number of lines required */
/* to print it using two columns, as shown in the following examples.       */
/*                                                                          */
/* LATIN SMALL LETTER E WITH ACUTE (00e9) -> 1 line containing  e9          */
/*                                                                          */
/* EURO SIGN (20ac)                       -> 2 lines containing 20          */
/*                                                              ac          */
/*                                                                          */
/* MUSICAL SYMBOL HALF NOTE (1d15e)       -> 3 lines containing 1d          */
/*                                                              15          */
/*                                                               e          */
/* ======================================================================== */
static int
encode(int32_t c, char cell[9])
{
  int len;

  if (c < 0)
    return 0;

  if (c <= 0x7f)
  {
    cell[0] = (char)c;
    return 1;
  }

  if (c <= 0x10ffff)
  {
    /* Get the length needed to print c. */
    /* """""""""""""""""""""""""""""""""" */
    len = snprintf(NULL, 0, "%x", c);

    /* Fill cell, adding a leading zero if len is odd. */
    /* """"""""""""""""""""""""""""""""""""""""""""""" */
    sprintf(cell, "%0*x", (len + 1) & ~1, c);
  }
  else
  {
    /* Represent a value outside the Unicode range as "??". */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
    cell[0] = '?';
    cell[1] = '?';
    len     = 2;
  }

  cell[(len + 1) & ~1] = '\0';

  return ((len + 1) & ~1) >> 1;
}

/* ================================================================ */
/* Return the number of lines required to display a node character. */
/* ================================================================ */
static int
character_height(const tst_node_t *node)
{
  char character[9];
  int  length;

  if (!node->codepoint)
    return 1;

  length = encode(node->codepoint, character);

  /* Non-ASCII characters have upper and lower delimiter lines. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  return node->codepoint >= 0x80 ? length + 2 : length;
}

/* ================================================================== */
/* Return the number of columns required to display a node character. */
/* ================================================================== */
static int
character_width(const tst_node_t *node)
{
  return node->codepoint >= 0x80 ? 2 : 1;
}

/* =================================== */
/* Allocate a blank rectangular block. */
/* =================================== */
static block_t
block_new(int width, int height, int root)
{
  block_t block;
  int     y;

  /* Ensure that every block has at least one row and one column. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  block.width  = width > 0 ? width : 1;
  block.height = height > 0 ? height : 1;
  block.root   = root;
  block.line   = xmalloc((size_t)block.height * sizeof(*block.line));

  for (y = 0; y < block.height; y++)
  {
    block.line[y] = xmalloc((size_t)block.width + 1);
    memset(block.line[y], ' ', (size_t)block.width);
    block.line[y][block.width] = '\0';
  }

  return block;
}

/* ===================================== */
/* Release all storage owned by a block. */
/* ===================================== */
static void
block_free(block_t *block)
{
  int y;

  for (y = 0; y < block->height; y++)
    free(block->line[y]);

  free(block->line);
}

/* ================================================================ */
/* Copy a source block into a destination block at position (x, y). */
/* ================================================================ */
static void
block_copy(block_t *destination, const block_t *source, int x, int y)
{
  int row;

  for (row = 0; row < source->height; row++)
    memcpy(destination->line[y + row] + x,
           source->line[row],
           (size_t)source->width);
}

/* ========================================================= */
/* Draw the current node character at horizontal position x. */
/* ========================================================= */
static void
draw_character(block_t *block, const tst_node_t *node, int x)
{
  char character[9];
  int  length;
  int  i;

  /* Draw the terminal zero code point as an asterisk. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  if (!node->codepoint)
  {
    block->line[0][x] = '*';
    return;
  }

  /* Draw an ASCII code point directly in one column. */
  /* """""""""""""""""""""""""""""""""""""""""""""""" */
  if (node->codepoint < 0x80)
  {
    block->line[0][x] = (char)node->codepoint;
    return;
  }

  length = encode(node->codepoint, character);

  /* Draw a replacement marker if encoding fails. */
  /* """""""""""""""""""""""""""""""""""""""""""" */
  if (!length)
  {
    block->line[0][x] = '?';
    return;
  }

  /* Draw the upper delimiter of a non-ASCII character. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  block->line[0][x]     = '=';
  block->line[0][x + 1] = '=';

  /* Draw one hexadecimal pair on each intermediate line. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
  for (i = 0; i < length; i++)
  {
    block->line[i + 1][x]     = character[2 * i];
    block->line[i + 1][x + 1] = character[2 * i + 1];
  }

  /* Draw the lower delimiter of a non-ASCII character. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  block->line[length + 1][x]     = '=';
  block->line[length + 1][x + 1] = '=';
}

/* ====================================================== */
/* Recursively construct the display block for a subtree. */
/* ====================================================== */
static block_t
build(const tst_node_t *node)
{
  block_t low   = { 0 };
  block_t equal = { 0 };
  block_t high  = { 0 };
  block_t output;

  int has_low   = node->low != NULL;
  int has_equal = node->equal != NULL;
  int has_high  = node->high != NULL;

  int node_height = character_height(node);
  int node_width  = character_width(node);

  int low_x   = 0;
  int equal_x = 0;
  int high_x  = 0;
  int root;
  int width;
  int height  = node_height;
  int equal_y = node_height;
  int side_y  = node_height + 1;
  int x;

  /* Recursively build all child blocks that exist. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (has_low)
    low = build(node->low);

  if (has_equal)
    equal = build(node->equal);

  if (has_high)
    high = build(node->high);

  /* Place the equal block after the low block when one exists. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (has_low)
    equal_x = low.width + GAP;

  /* Place the high block after the equal block or current node. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (has_equal)
    high_x = equal_x + equal.width + GAP;
  else
    high_x = equal_x + node_width + GAP;

  /* Align the current node with the root of its equal subtree.      */
  /* Without an equal subtree, derive its position from the existing */
  /* low and high subtrees.                                          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (has_equal)
    root = equal_x + equal.root;
  else if (has_low && has_high)
    root = (low.root + high_x + high.root) / 2;
  else if (has_low)
    root = low.width + 1;
  else
    root = 0;

  /* Start with the width required by the current node. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  width = root + node_width;

  /* Expand the width to include all child blocks. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  if (has_low)
    width = MAX(width, low_x + low.width);

  if (has_equal)
    width = MAX(width, equal_x + equal.width);

  if (has_high)
    width = MAX(width, high_x + high.width);

  /* Expand the height to include all child blocks. */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  if (has_equal)
    height = MAX(height, equal_y + equal.height);

  if (has_low)
    height = MAX(height, side_y + low.height);

  if (has_high)
    height = MAX(height, side_y + high.height);

  /* Allocate the output block and draw its root character. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
  output = block_new(width, height, root);
  draw_character(&output, node, root);

  /* Copy each child block into its calculated position. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  if (has_equal)
    block_copy(&output, &equal, equal_x, equal_y);

  if (has_low)
    block_copy(&output, &low, low_x, side_y);

  if (has_high)
    block_copy(&output, &high, high_x, side_y);

  /* Connect the root to its low child. */
  /* """""""""""""""""""""""""""""""""" */
  if (has_low)
  {
    int child_root = low_x + low.root;

    output.line[equal_y][root - 1] = '/';

    for (x = child_root + 1; x < root - 1; x++)
      output.line[equal_y][x] = '_';
  }

  /* Connect the root to its high child. */
  /* """"""""""""""""""""""""""""""""""" */
  if (has_high)
  {
    int child_root = high_x + high.root;

    output.line[equal_y][root + node_width] = '\\';

    for (x = root + node_width + 1; x < child_root; x++)
      output.line[equal_y][x] = '_';
  }

  /* Release child blocks after copying their contents. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""" */
  if (has_low)
    block_free(&low);

  if (has_equal)
    block_free(&equal);

  if (has_high)
    block_free(&high);

  return output;
}

/* ============================================================ */
/* Render the complete ternary search tree on standard output.  */
/* Remove trailing spaces independently from every output line. */
/* ============================================================ */
void
tst_pretty_print(tst_node_t *root)
{
  block_t block;
  int     y;

  /* Handle an empty tree without constructing a display block. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!root)
  {
    puts("(empty)");
    return;
  }

  block = build(root);

  for (y = 0; y < block.height; y++)
  {
    int end = block.width;

    /* Locate the final non-space character on the line. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    while (end > 0 && block.line[y][end - 1] == ' ')
      end--;

    /* Terminate the line immediately after its visible content. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    block.line[y][end] = '\0';
    puts(block.line[y]);
  }

  block_free(&block);
}
