//#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <stdarg.h>
#include <string.h>
#include "ctxopt.h"

errors ctxopterrno = CTXOPTNOERR;

/* *********************** */
/* Static global variables */
/* *********************** */
static void * contexts_bst;
static void * options_bst;

/* Prototypes */

/* ****************** */
/* Messages interface */
/* ****************** */

static void (**err_functions)(char *, char *, char *, char *, char *);

static void
fatal(errors e, char * opt_par, char * opt_params, char * opt_name,
      char * ctx_par, char * ctx_name, const char * format, ...);

static void
error(const char * format, ...);

static int    user_rc;      /* Used by various callback functions */
static int    user_value;   /* Used by various callback functions */
static char * user_string;  /* Used by various callback functions */
static char * user_string2; /* Used by various callback functions */
static void * user_object;  /* Used by various callback functions */

/* *************************** */
/* Memory management interface */
/* *************************** */

static void *
xmalloc(size_t size);

static void *
xcalloc(size_t num, size_t size);

static void *
xrealloc(void * ptr, size_t size);

static char *
xstrdup(const char * p);

static char *
xstrndup(const char * str, size_t len);

/* ************* */
/* BST interface */
/* ************* */
typedef struct bst_s bst_t;

typedef enum
{
  preorder,
  postorder,
  endorder,
  leaf
} walk_order_e;

static void *
bst_delete(const void * vkey, void ** vrootp,
           int (*compar)(const void *, const void *));

static void
bst_destroy_recurse(bst_t * root, void (*free_action)(void *));

static void
bst_destroy(void * vrootp, void (*freefct)(void *));

static void *
bst_find(const void * vkey, void * const * vrootp,
         int (*compar)(const void *, const void *));

static void *
bst_search(const void * vkey, void ** vrootp,
           int (*compar)(const void *, const void *));

static void
bst_walk_recurse(const bst_t * root,
                 void (*action)(const void *, walk_order_e, int), int level);

static void
bst_walk(const void * vroot, void (*action)(const void *, walk_order_e, int));

/* ********************* */
/* Linked list Interface */
/* ********************* */

typedef struct ll_node_s ll_node_t;
typedef struct ll_s      ll_t;

static void
ll_append(ll_t * const list, void * const data);

static void
ll_prepend(ll_t * const list, void * const data);

static void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data);

static void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data);

static int
ll_delete(ll_t * const list, ll_node_t * node);

static ll_node_t *
ll_find(ll_t * const, void * const, int (*)(const void *, const void *));

static void
ll_init(ll_t * list);

static ll_node_t *
ll_new_node(void);

static ll_t *
ll_new(void);

static int
ll_strarray(ll_t * list, ll_node_t * start_node, int * count, char *** array);

/* ****************** */
/* various interfaces */
/* ****************** */

static void
ltrim(char * str, const char * trim_str);

static void
rtrim(char * str, const char * trim_str, size_t min);

static int
strchrcount(char * str, char c);

int
strpref(char * s1, char * s2);

static char *
xstrtok_r(char * str, const char * delim, char ** end);

/* **************** */
/* ctxopt interface */
/* **************** */

typedef struct opt_s        opt_t;
typedef struct par_s        par_t;
typedef struct ctx_s        ctx_t;
typedef struct constraint_s constraint_t;
typedef struct ctx_inst_s   ctx_inst_t;
typedef struct opt_inst_s   opt_inst_t;
typedef struct seen_opt_s   seen_opt_t;

static char *
strtoken(char * s, char * token, size_t tok_len, char * pattern, int * pos);

static int
ctx_compare(const void * c1, const void * c2);

static int
opt_compare(const void * o1, const void * o2);

static int
par_compare(const void * a1, const void * a2);

static int
seen_opt_compare(const void * so1, const void * so2);

static int
str_compare(const void * s1, const void * s2);

static ctx_t *
locate_ctx(char * name);

static opt_t *
locate_opt(char * name);

static par_t *
locate_par(char * name, ctx_t * ctx);

static void
bst_seen_opt_cb(const void * node, walk_order_e kind, int level);

static void
bst_seen_opt_seen_cb(const void * node, walk_order_e kind, int level);

static void
bst_print_ctx_cb(const void * node, walk_order_e kind, int level);

static void
match_prefix_cb(const void * node, walk_order_e kind, int level);

static int
has_unseen_mandatory_opt(ctx_inst_t * ctx_inst);

static int
opt_parse(char * s, opt_t ** opt);

static int
init_opts(char * spec, ctx_t * ctx);

static int
ctxopt_build_cmdline_list(int nb_words, char ** words);

static int
opt_set_parms(char * opt_name, char * par_str);

static ctx_inst_t *
new_ctx_inst(ctx_t * ctx, ctx_inst_t * prev_ctx_inst);

static void
evaluate_ctx_inst(ctx_inst_t * ctx_inst);

/* ====================== */
/* Message implementation */
/* ====================== */
static void
fatal(errors e, char * opt_par, char * opt_params, char * opt_name,
      char * ctx_par, char * ctx_name, const char * format, ...)
{
  va_list args;

  ctxopterrno = e;

  if (e == CTXOPTINTERNAL)
    fprintf(stderr, "ctxopt internal: ");

  if (err_functions[e] != NULL)
    err_functions[e](opt_par, opt_params, opt_name, ctx_par, ctx_name);
  else
  {
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
  }

  if (ctx_name != NULL)
    ctxopt_ctx_disp_usage(ctx_name, continue_after);

  va_end(args);

  exit(EXIT_FAILURE);
}

static void
error(const char * format, ...)
{
  va_list args;

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");

  exit(EXIT_FAILURE);
}

/* ******************************** */
/* Memory management implementation */
/* ******************************** */

/* ================= */
/* Customized malloc */
/* ================= */
static void *
xmalloc(size_t size)
{
  void * allocated;
  size_t real_size;

  real_size = (size > 0) ? size : 1;
  allocated = malloc(real_size);
  if (allocated == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Insufficient memory (attempt to malloc %lu bytes)\n",
          (unsigned long int)size);

  return allocated;
}

/* ================= */
/* Customized calloc */
/* ================= */
static void *
xcalloc(size_t n, size_t size)
{
  void * allocated;

  n         = (n > 0) ? n : 1;
  size      = (size > 0) ? size : 1;
  allocated = calloc(n, size);
  if (allocated == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Insufficient memory (attempt to calloc %lu bytes)\n",
          (unsigned long int)size);

  return allocated;
}

/* ================== */
/* Customized realloc */
/* ================== */
static void *
xrealloc(void * p, size_t size)
{
  void * allocated;

  allocated = realloc(p, size);
  if (allocated == NULL && size > 0)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Insufficient memory (attempt to xrealloc %lu bytes)\n",
          (unsigned long int)size);

  return allocated;
}

/* =================================== */
/* strdup implementation using xmalloc */
/* =================================== */
static char *
xstrdup(const char * p)
{
  char * allocated;

  allocated = xmalloc(strlen(p) + 1);
  strcpy(allocated, p);

  return allocated;
}

/* ================================================== */
/* strndup implementation using xmalloc               */
/* This version guarantees that there is a final '\0' */
/* ================================================== */
static char *
xstrndup(const char * str, size_t len)
{
  char * p;

  p = memchr(str, '\0', len);

  if (p)
    len = p - str;

  p = xmalloc(len + 1);
  memcpy(p, str, len);
  p[len] = '\0';

  return p;
}

/* ************************** */
/* Linked list implementation */
/* ************************** */

/* Linked list node structure */
/* """""""""""""""""""""""""" */
struct ll_node_s
{
  void *             data;
  struct ll_node_s * next;
  struct ll_node_s * prev;
};

/* Linked List structure */
/* """"""""""""""""""""" */
struct ll_s
{
  ll_node_t * head;
  ll_node_t * tail;
  long        len;
};

/* ======================== */
/* Create a new linked list */
/* ======================== */
static ll_t *
ll_new(void)
{
  ll_t * ret = xmalloc(sizeof(ll_t));
  ll_init(ret);

  return ret;
}

/* ======================== */
/* Initialize a linked list */
/* ======================== */
static void
ll_init(ll_t * list)
{
  list->head = NULL;
  list->tail = NULL;
  list->len  = 0;
}

/* ==================================================== */
/* Allocate the space for a new node in the linked list */
/* ==================================================== */
static ll_node_t *
ll_new_node(void)
{
  ll_node_t * ret = xmalloc(sizeof(ll_node_t));

  return ret;
}

/* ==================================================================== */
/* Append a new node filled with its data at the end of the linked list */
/* The user is responsible for the memory management of the data        */
/* ==================================================================== */
static void
ll_append(ll_t * const list, void * const data)
{
  ll_node_t * node;

  node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                         | uses xmalloc which does not return if there *
                         | is an allocation error.                     */

  node->data = data;
  node->next = NULL;

  node->prev = list->tail;
  if (list->tail)
    list->tail->next = node;
  else
    list->head = node;

  list->tail = node;

  ++list->len;
}

/* =================================================================== */
/* Put a new node filled with its data at the beginning of the linked  */
/* list. The user is responsible for the memory management of the data */
/* =================================================================== */
static void
ll_prepend(ll_t * const list, void * const data)
{
  ll_node_t * node;

  node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                         | uses xmalloc which does not return if there *
                         | is an allocation error.                     */

  node->data = data;
  node->prev = NULL;

  node->next = list->head;
  if (list->head)
    list->head->prev = node;
  else
    list->tail = node;

  list->head = node;

  ++list->len;
}

/* ======================================================= */
/* Insert a new node before the specified node in the list */
/* ======================================================= */
static void
ll_insert_before(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (node->prev == NULL)
    ll_prepend(list, data);
  else
  {
    new_node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                               | uses xmalloc which does not return if there *
                               | is an allocation error. */

    new_node->data   = data;
    new_node->next   = node;
    new_node->prev   = node->prev;
    node->prev->next = new_node;
    node->prev       = new_node;

    ++list->len;
  }
}

/* ====================================================== */
/* Insert a new node after the specified node in the list */
/* ====================================================== */
void
ll_insert_after(ll_t * const list, ll_node_t * node, void * const data)
{
  ll_node_t * new_node;

  if (node->next == NULL)
    ll_append(list, data);
  else
  {
    new_node = ll_new_node(); /* ll_new_node cannot return NULL because it   *
                               | uses xmalloc which does not return if there *
                               | is an allocation error.                    */

    new_node->data   = data;
    new_node->prev   = node;
    new_node->next   = node->next;
    node->next->prev = new_node;
    node->next       = new_node;

    ++list->len;
  }
}

/* ================================================================ */
/* Remove a node from a linked list                                 */
/* The memory taken by the deleted node must be freed by the caller */
/* ================================================================ */
static int
ll_delete(ll_t * const list, ll_node_t * node)
{
  if (list->head == list->tail)
  {
    if (list->head != NULL)
      list->head = list->tail = NULL;
    else
      return 0;
  }
  else if (node->prev == NULL)
  {
    list->head       = node->next;
    list->head->prev = NULL;
  }
  else if (node->next == NULL)
  {
    list->tail       = node->prev;
    list->tail->next = NULL;
  }
  else
  {
    node->next->prev = node->prev;
    node->prev->next = node->next;
  }

  --list->len;

  return 1;
}

/* =========================================================================*/
/* Find a node in the list containing data. Return the node pointer or NULL */
/* if not found.                                                            */
/* A comparison function must be provided to compare a and b (strcmp like). */
/* =========================================================================*/
static ll_node_t *
ll_find(ll_t * const list, void * const data,
        int (*cmpfunc)(const void * a, const void * b))
{
  ll_node_t * node;

  if (NULL == (node = list->head))
    return NULL;

  do
  {
    if (0 == cmpfunc(node->data, data))
      return node;
  } while (NULL != (node = node->next));

  return NULL;
}

/* ==================================================================== */
/* Allocates and fills an array of strings from a list                  */
/* WARNINGS:                                                            */
/*   1) The list node must contain strings (char *)                     */
/*   2) The strings in the resulting array MUST NOT be freed as the are */
/*      NOT copied from the strings of the list.                        */
/*                                                                      */
/* IN list       : The list from which the array is generated           */
/* IN start_node : The node of the list which will be the first node to */
/*                 consider to create the array                         */
/* OUT: count    : The number of elements of the resulting array.       */
/* OUT: array    : The resulting array.                                 */
/* RC :          : The number of elements of the resulting array.       */
/* ==================================================================== */
static int
ll_strarray(ll_t * list, ll_node_t * start_node, int * count, char *** array)
{
  int         n = 0;
  ll_node_t * node;

  *count = 0;

  node = start_node;

  if (list == NULL || node == NULL)
    return 0;

  *array = xmalloc((list->len + 1) * sizeof(char **));
  while (node != NULL)
  {
    (*array)[n++] = (char *)(node->data);
    (*count)++;

    node = node->next;
  }

  (*array)[*count] = NULL;
  *array           = xrealloc(*array, ((*count) + 1) * sizeof(char **));

  return *count;
}

/* ******************************************************************* */
/* BST (search.h compatible) implementation                            */
/*                                                                     */
/* Tree search generalized from Knuth (6.2.2) Algorithm T just like    */
/* the AT&T man page says.                                             */
/*                                                                     */
/* Written by reading the System V Interface Definition, not the code. */
/*                                                                     */
/* Totally public domain.                                              */
/* ******************************************************************* */

struct bst_s
{
  void *         key;
  struct bst_s * llink;
  struct bst_s * rlink;
};

/* ========================== */
/* delete node with given key */
/* ========================== */
static void *
bst_delete(const void * vkey, void ** vrootp,
           int (*compar)(const void *, const void *))
{
  bst_t ** rootp = (bst_t **)vrootp;
  bst_t *  p, *q, *r;
  int      cmp;

  if (rootp == NULL || (p = *rootp) == NULL)
    return NULL;

  while ((cmp = (*compar)(vkey, (*rootp)->key)) != 0)
  {
    p     = *rootp;
    rootp = (cmp < 0) ? &(*rootp)->llink  /* follow llink branch */
                      : &(*rootp)->rlink; /* follow rlink branch */
    if (*rootp == NULL)
      return NULL; /* key not found */
  }
  r = (*rootp)->rlink;               /* D1: */
  if ((q = (*rootp)->llink) == NULL) /* Left NULL? */
    q = r;
  else if (r != NULL)
  { /* Right link is NULL? */
    if (r->llink == NULL)
    { /* D2: Find successor */
      r->llink = q;
      q        = r;
    }
    else
    { /* D3: Find NULL link */
      for (q = r->llink; q->llink != NULL; q = r->llink)
        r = q;
      r->llink = q->rlink;
      q->llink = (*rootp)->llink;
      q->rlink = (*rootp)->rlink;
    }
  }
  if (p != *rootp)
    free(*rootp); /* D4: Free node */
  *rootp = q;     /* link parent to new node */
  return p;
}

/* ============== */
/* destroy a tree */
/* ============== */
static void
bst_destroy_recurse(bst_t * root, void (*free_action)(void *))
{
  if (root->llink != NULL)
    bst_destroy_recurse(root->llink, free_action);
  if (root->rlink != NULL)
    bst_destroy_recurse(root->rlink, free_action);

  (*free_action)((void *)root->key);
  free(root);
}

static void
bst_destroy(void * vrootp, void (*freefct)(void *))
{
  bst_t * root = (bst_t *)vrootp;

  if (root != NULL)
    bst_destroy_recurse(root, freefct);
}

/* ======================== */
/* find a node, or return 0 */
/* ======================== */
static void *
bst_find(const void * vkey, void * const * vrootp,
         int (*compar)(const void *, const void *))
{
  bst_t * const * rootp = (bst_t * const *)vrootp;

  if (rootp == NULL)
    return NULL;

  while (*rootp != NULL)
  { /* T1: */
    int r;

    if ((r = (*compar)(vkey, (*rootp)->key)) == 0) /* T2: */
      return *rootp;                               /* key found */
    rootp = (r < 0) ? &(*rootp)->llink             /* T3: follow left branch */
                    : &(*rootp)->rlink;            /* T4: follow right branch */
  }
  return NULL;
}

/* ===================================== */
/* find or insert datum into search tree */
/* ===================================== */
static void *
bst_search(const void * vkey, void ** vrootp,
           int (*compar)(const void *, const void *))
{
  bst_t *  q;
  bst_t ** rootp = (bst_t **)vrootp;

  if (rootp == NULL)
    return NULL;

  while (*rootp != NULL)
  { /* Knuth's T1: */
    int r;

    if ((r = (*compar)(vkey, (*rootp)->key)) == 0) /* T2: */
      return *rootp;                               /* we found it! */

    rootp = (r < 0) ? &(*rootp)->llink  /* T3: follow left branch */
                    : &(*rootp)->rlink; /* T4: follow right branch */
  }

  q = xmalloc(sizeof(bst_t)); /* T5: key not found */
  if (q != 0)
  {                          /* make new node */
    *rootp   = q;            /* link new node to old */
    q->key   = (void *)vkey; /* initialize new node */
    q->llink = q->rlink = NULL;
  }
  return q;
}

/* ======================== */
/* Walk the nodes of a tree */
/* ======================== */
static void
bst_walk_recurse(const bst_t * root,
                 void (*action)(const void *, walk_order_e, int), int level)
{
  if (root->llink == NULL && root->rlink == NULL)
    (*action)(root, leaf, level);
  else
  {
    (*action)(root, preorder, level);
    if (root->llink != NULL)
      bst_walk_recurse(root->llink, action, level + 1);
    (*action)(root, postorder, level);
    if (root->rlink != NULL)
      bst_walk_recurse(root->rlink, action, level + 1);
    (*action)(root, endorder, level);
  }
}

static void
bst_walk(const void * vroot, void (*action)(const void *, walk_order_e, int))
{
  if (vroot != NULL && action != NULL)
    bst_walk_recurse(vroot, action, 0);
}

/* *********************** */
/* various implementations */
/* *********************** */

/* ======================= */
/* Trim leading characters */
/* ======================= */
static void
ltrim(char * str, const char * trim_str)
{
  size_t len   = strlen(str);
  size_t begin = strspn(str, trim_str);
  size_t i;

  if (begin > 0)
    for (i = begin; i <= len; ++i)
      str[i - begin] = str[i];
}

/* ================================================= */
/* Trim trailing characters                          */
/* The resulting string will have at least min bytes */
/* even if trailing spaces remain.                   */
/* ================================================= */
static void
rtrim(char * str, const char * trim_str, size_t min)
{
  size_t len = strlen(str);
  while (len > min && strchr(trim_str, str[len - 1]))
    str[--len] = '\0';
}

/* ================================================== */
/* Count the number of occurrences of the character c */
/* in the string str.                                 */
/* The str pointer is assumed to be not NULL          */
/* ================================================== */
static int
strchrcount(char * str, char c)
{
  int count = 0;

  while (*str)
    if (*str++ == c)
      count++;

  return count;
}

/* =============================================== */
/* Is the string str2 a prefix of the string str1? */
/* =============================================== */
int
strpref(char * str1, char * str2)
{
  while (*str1 != '\0' && *str1 == *str2)
  {
    str1++;
    str2++;
  }

  return *str2 == '\0';
}

/*===================================================================== */
/* Allocate memory and safely concatenate strings. Stolen from a public */
/* domain implementation which can be found here:                       */
/* http://openwall.info/wiki/people/solar/software/\                    */
/* public-domain-source-code                                            */
/* ==================================================================== */
static char *
concat(const char * s1, ...)
{
  va_list      args;
  const char * s;
  char *       p, *result;
  size_t       l, m, n;

  m = n = strlen(s1);
  va_start(args, s1);
  while ((s = va_arg(args, char *)))
  {
    l = strlen(s);
    if ((m += l) < l)
      break;
  }
  va_end(args);
  if (s || m >= INT_MAX)
    return NULL;

  result = (char *)xmalloc(m + 1);

  memcpy(p = result, s1, n);
  p += n;
  va_start(args, s1);
  while ((s = va_arg(args, char *)))
  {
    l = strlen(s);
    if ((n += l) < l || n > m)
      break;
    memcpy(p, s, l);
    p += l;
  }
  va_end(args);
  if (s || m != n || p != result + n)
  {
    free(result);
    return NULL;
  }

  *p = 0;
  return result;
}

/* ====================================================================== */
/*  public domain strtok_r() by Charlie Gordon                            */
/*   from comp.lang.c  9/14/2007                                          */
/*      http://groups.google.com/group/comp.lang.c/msg/2ab1ecbb86646684   */
/*                                                                        */
/*     (Declaration that it's public domain):                             */
/*      http://groups.google.com/group/comp.lang.c/msg/7c7b39328fefab9c   */
/*                                                                        */
/* Also, fixed by Fletcher T. Penney --- added the "return NULL" when     */
/* *end == NULL                                                           */
/* ====================================================================== */
static char *
xstrtok_r(char * str, const char * delim, char ** end)
{
  char * ret;

  if (str == NULL)
    str = *end;

  if (str == NULL)
    return NULL;

  str += strspn(str, delim);

  if (*str == '\0')
    return NULL;

  ret = str;

  str += strcspn(str, delim);

  if (*str)
    *str++ = '\0';

  *end = str;

  return ret;
}

/* =========================================================== */
/* Fills an array of strings from the words composing a string */
/*                                                             */
/* str: initial string which will be altered                   */
/* args: array of pointers to the start of the words in str    */
/* max: maximum number of words used before giving up          */
/* return: the number of words (<=max)                         */
/* =========================================================== */
static int
str2argv(char * str, char ** args, int max)
{
  int nb_args = 0;

  while (*str)
  {
    if (nb_args >= max)
      return nb_args;

    while (*str == ' ' || *str == '\t')
      *(str++) = '\0';

    if (!*str)
      return nb_args;

    args[nb_args] = str;
    nb_args++;

    while (*str && (*str != ' ') && (*str != '\t'))
      str++;
  }

  return nb_args;
}

/* ********************* */
/* ctxopt implementation */
/* ********************* */

static int ctxopt_initialized = 0; /* cap_init has not yet been called */

/* context structure */
/* """"""""""""""""" */
struct ctx_s
{
  char * name;
  ll_t * opt_list;    /* list of options allowed in this context       */
  ll_t * incomp_list; /* list of strings containing incompatible names *
                       | of options separated by spaces or tabs        */
  int (*action)(char * name, int type, char * new_ctx, int ctx_nb_data,
                void ** ctx_data);
  void *  par_bst;
  int     nb_data;
  void ** data;
};

/* https://textik.com/#488ce3649b6c60f5                          */
/*                                                               */
/*       +--------------+                                        */
/*       |first_ctx_inst|                                        */
/*       +---+----------+                                        */
/*           |                                                   */
/*        +--v-----+      +--------+     +--------+      +-----+ */
/* +---+-->ctx_inst+------>opt_inst+----->opt_inst+------> ... | */
/* |   |  +-+------+      +----+---+     +----+---+      +-----+ */
/* |   |    |                  |              |                  */
/* |   |  +-v------+           |              |                  */
/* |   +--+ctx_inst<-----------+              |                  */
/* |      +-+------+                          |                  */
/* |        |                                 |                  */
/* |      +-v------+                          |                  */
/* +------+ctx_inst<--------------------------+                  */
/*        +-+------+                                             */
/*          |                                                    */
/*        +-v---+                                                */
/*        | ... |                                                */
/*        +-----+                                                */

/* option structure */
/* """""""""""""""" */
struct opt_s
{
  char * name;     /* option name                                            */
  char * next_ctx; /* new context this option may lead to */
  ll_t * ctx_list; /* list of contexts allowing this option */
  char * params;   /* string containing all the parameters of the option     */

  void (*action)(char * ctx_name, char * opt_name, char * par, int nb_args,
                 char ** args, int nb_opt_data, void ** opt_data,
                 int     nb_ctx_data,
                 void ** ctx_data); /* The option associated action */
  int     nb_data; /* number of the data passed as argument to action        */
  void ** data;    /* void data passed as argument to action                 */

  int args; /* pointer to the data passed to the action function */

  int optional; /* 1 if the option is optional, else 0 */
  int multiple; /* 1 if the option can appear more than one time in a        *
                 | context, else 0 */

  int  opt_count_matter; /* 1 if we must restrict the count, else 0          */
  int  occurrences;      /* Number of occurrences of the option in a context */
  char opt_count_oper;   /* <, = or >                                        */
  int  opt_count_mark;   /* Value to be compared to with opt_count_oper      */
  char * arg;            /* text represent the argument in in description of *
                          | the option.                                      */
  int optional_args;     /* 1 of option is optional else 0                   */
  int multiple_args;     /* 1 is option can appear more than once in a       *
                          | context instance                                 */

  int  opt_args_count_matter; /* 1 if we must restrict the count, else 0     */
  char opt_args_count_oper;   /* <, = or >                                   */
  int  opt_args_count_mark;   /* Value to be compared to with opt_count_oper */
  int  eval_first;            /* 1 if this option must be evaluated before   *
                               |the options without this mark                */

  ll_t * constraints_list; /* List of constraints checking functions         *
                            | pointers. */
};

/* context instance structure */
/* """""""""""""""""""""""""" */
struct ctx_inst_s
{
  ctx_t *      ctx;           /* the context whose this is an instance of  */
  ctx_inst_t * prev_ctx_inst; /* ctx_inst of the opt_inst which led to the *
                               | creation of this ctx_inst structure.      */
  opt_inst_t * gen_opt_inst;  /* opt_inst which led to the creation of a   *
                               | instance of this structure                */
  ll_t * incomp_bst_list;     /* list of seen_opt_t bst                    */
  void * seen_opt_bst;        /* tree of seen_opt_t                        */
  ll_t * opt_inst_list;       /* The list of option instances in this      *
                               | context instance                          */
  char * par_name;            /* parameter which created this instance     */
};

/* Option instance structure */
/* """"""""""""""""""""""""" */
struct opt_inst_s
{
  opt_t *      opt;           /* The option this is an instance of        */
  char *       opt_name;      /* The option which led to this creation    */
  char *       par;           /* The parameter which led to this creation */
  ll_t *       values_list;   /* The list of arguments of this option     */
  ctx_inst_t * next_ctx_inst; /* The new context instance this option     *
                               | instance may create                      */
};

/* Structure used to check if an option has bee seen or not */
/* in a context instance                                    */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct seen_opt_s
{
  opt_t * opt;   /* The concerned option                                */
  char *  par;   /* Parameter which led to the making of this structure */
  char *  count; /* Number of seen occurrences of this parameter        */
  int     seen;  /* 1 if seen in the context instances, else 0          */
};

/* parameter structure which links a parameter to the option it belongs to */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
struct par_s
{
  char *  name; /* Parameter name (with the leading - */
  opt_t * opt;  /* Attached option                    */
};

/* Constraint structure */
/* """""""""""""""""""" */
struct constraint_s
{
  int (*constraint)(int nb_args, char ** args, char * value);
  int     nb_args;
  char ** args;
};

static ll_t *       cmdline_list;
static ctx_t *      main_ctx       = NULL;
static ctx_inst_t * first_ctx_inst = NULL; /* Pointer to the fist context *
                                            | instance which holds the    *
                                            | options instances           */
static ll_t * ctx_inst_list;

/* ====================================================== */
/* Parse a string for the next matching token.            */
/*                                                        */
/* s:       string to parse.                              */
/* token:   pre_allocated array of max tok_len characters */
/* pattern: scanf type pattern token must match           */
/* pos:     number of characters successfully parsed in s */
/*                                                        */
/* Returns: a pointer to the first unread character or    */
/*          to he terminating \0.                         */
/* ====================================================== */
static char *
strtoken(char * s, char * token, size_t tok_len, char * pattern, int * pos)
{
  char * full_pattern;
  char   len[3];
  int    n;

  *pos = 0;

  n = snprintf(len, 3, "%zu", tok_len);
  if (n < 0)
    return NULL;

  full_pattern = xmalloc(strlen(pattern) + n + 4);

  strcpy(full_pattern, "%");
  strcat(full_pattern, len);
  strcat(full_pattern, pattern);
  strcat(full_pattern, "%n");

  n = sscanf(s, full_pattern, token, pos);

  free(full_pattern);

  if (n != 1)
    return NULL;

  return s + *pos;
}

/* **************************** */
/* Various comparison functions */
/* **************************** */

static int
ctx_compare(const void * c1, const void * c2)
{
  return strcmp(((ctx_t *)c1)->name, ((ctx_t *)c2)->name);
}

static int
opt_compare(const void * o1, const void * o2)
{
  return strcmp(((opt_t *)o1)->name, ((opt_t *)o2)->name);
}

static int
par_compare(const void * a1, const void * a2)
{
  return strcmp(((par_t *)a1)->name, ((par_t *)a2)->name);
}

static int
seen_opt_compare(const void * so1, const void * so2)
{
  opt_t *o1, *o2;

  o1 = ((seen_opt_t *)so1)->opt;
  o2 = ((seen_opt_t *)so2)->opt;

  return strcmp(o1->name, o2->name);
}

static int
str_compare(const void * s1, const void * s2)
{
  return strcmp((char *)s1, (char *)s2);
}

/* ******************************************************************* */
/* Helper function to locate contexts, options and parameters in a bst */
/* by their names.                                                     */
/* ******************************************************************* */

static ctx_t *
locate_ctx(char * name)
{
  bst_t * node;
  ctx_t   ctx;

  ctx.name = name;

  if ((node = bst_find(&ctx, &contexts_bst, ctx_compare)) == NULL)
    return NULL;
  else
    return node->key;
}

static opt_t *
locate_opt(char * name)
{
  bst_t * node;
  opt_t   opt;

  opt.name = name;

  if ((node = bst_find(&opt, &options_bst, opt_compare)) == NULL)
    return NULL;
  else
    return node->key;
}

static par_t *
locate_par(char * name, ctx_t * ctx)
{
  bst_t * node;
  par_t   par;
  void *  bst = ctx->par_bst;

  par.name = name;

  if ((node = bst_find(&par, &bst, par_compare)) == NULL)
    return NULL;
  else
    return node->key;
}

void
print_options(ll_t * list, int * has_optional, int * has_ellipsis,
              int * has_rule, int * has_generic_arg, int * has_ctx_change,
              int * has_early_eval)
{
  ll_node_t * node = list->head;
  opt_t *     opt;
  char *      line;
  char *      option;

  line = "  ";
  while (node != NULL)
  {
    option = "";
    opt    = node->data;

    if (opt->optional)
    {
      option        = concat(option, "[", NULL);
      *has_optional = 1;
    }

    if (opt->eval_first)
    {
      option          = concat(option, "*", NULL);
      *has_early_eval = 1;
    }

    option = concat(option, opt->params, NULL);

    if (opt->next_ctx != NULL)
    {
      option          = concat(option, ">", opt->next_ctx, NULL);
      *has_ctx_change = 1;
    }

    if (opt->multiple)
    {
      if (opt->opt_count_oper != '\0')
      {
        char m[4];
        char o[2];
        o[0] = opt->opt_count_oper;
        o[1] = '\0';
        snprintf(m, 3, "%u", opt->opt_count_mark);
        option    = concat(option, "...", o, m, NULL);
        *has_rule = 1;
      }

      option        = concat(option, "...", NULL);
      *has_ellipsis = 1;
    }

    if (opt->args)
    {

      if (strcmp(opt->arg, "#") == 0)
        *has_generic_arg = 1;

      option = concat(option, " ", NULL);
      if (opt->optional_args)
      {
        option        = concat(option, "[", opt->arg, NULL);
        *has_optional = 1;
      }
      else
        option = concat(option, opt->arg, NULL);

      if (opt->multiple_args)
      {
        if (opt->opt_args_count_oper != '\0')
        {
          char m[4];
          char o[2];
          o[0] = opt->opt_args_count_oper;
          o[1] = '\0';
          snprintf(m, 3, "%u", opt->opt_args_count_mark);
          option    = concat(option, "...", o, m, NULL);
          *has_rule = 1;
        }
        else
          option = concat(option, "...", NULL);
        *has_ellipsis = 1;
      }
      if (opt->optional_args)
        option = concat(option, "]", NULL);
    }
    if (opt->optional)
      option = concat(option, "]", NULL);

    if (strlen(line) + 1 + strlen(option) < 80)
      line = concat(line, option, " ", NULL);
    else
    {
      printf("%s\n", line);
      line[2] = '\0';
      line    = concat(line, option, " ", NULL);
    }
    free(option);

    node = node->next;
  }
  printf("%s\n", line);
  free(line);
}

/* ******************************************************* */
/* Various utility and callback function call when walking */
/* through a bst.                                          */
/* ******************************************************* */

static void
bst_seen_opt_cb(const void * node, walk_order_e kind, int level)
{
  seen_opt_t * seen_opt = ((bst_t *)node)->key;

  if (kind == postorder || kind == leaf)
  {
    if ((!seen_opt->opt->optional) && seen_opt->seen == 0)
    {
      user_rc     = 1;
      user_string = concat(user_string, seen_opt->opt->params, " ", NULL);
    }
  }
}

static void
bst_seen_opt_seen_cb(const void * node, walk_order_e kind, int level)
{
  seen_opt_t * seen_opt = ((bst_t *)node)->key;

  if (kind == postorder || kind == leaf)
    if (seen_opt->seen == 1)
    {
      user_rc     = 1;
      user_object = seen_opt->par;
    }
}

static void
bst_print_ctx_cb(const void * node, walk_order_e kind, int level)
{
  ctx_t * ctx     = main_ctx;
  ctx_t * cur_ctx = ((bst_t *)node)->key;

  ll_t * list;

  int has_optional    = 0;
  int has_ellipsis    = 0;
  int has_rule        = 0;
  int has_generic_arg = 0;
  int has_ctx_change  = 0;
  int has_early_eval  = 0;

  if (kind == postorder || kind == leaf)
    if (strcmp(ctx->name, cur_ctx->name) != 0)
    {
      list = cur_ctx->opt_list;

      printf("\nAllowed options in the context %s:\n", cur_ctx->name);
      print_options(list, &has_optional, &has_ellipsis, &has_rule,
                    &has_generic_arg, &has_ctx_change, &has_early_eval);
    }
}

static void
bst_match_par_cb(const void * node, walk_order_e kind, int level)
{
  ctx_t * ctx = ((bst_t *)node)->key;
  par_t * par;

  if (kind == postorder || kind == leaf)
  {
    char * str = xstrdup(user_string);

    while (*str != '\0')
    {
      if ((par = locate_par(str, ctx)) != NULL)
      {
        user_string2 = concat(user_string2, " ", ctx->name, NULL);
        break;
      }
      str[strlen(str) - 1] = '\0';
    }
    free(str);
  }
}

static void
match_prefix_cb(const void * node, walk_order_e kind, int level)
{
  par_t * par = ((bst_t *)node)->key;

  if (kind == postorder || kind == leaf)
    if (strpref(par->name, (char *)user_object))
    {
      user_rc++;
      user_string = concat(user_string, par->name, " ", NULL);
    }
}

void
bst_null_action(void * data)
{
  ;
}

/* ====================================================================== */
/* A parameter may not be separated from its first option by spaces, in   */
/* this case this function looks for a valid flag as a prefix and splits  */
/* the command line queue (eg: "-pa1" -> "-pa" "1" if "-pa" is a valid    */
/* option)                                                                */
/*                                                                        */
/* IN  word : the word to be checked                                      */
/* IN  ctx  : The context in which the flag indexed by the word is to be  */
/*            checked.                                                    */
/* OUT pos  : The offset in word pointing just after the matching prefix. */
/* OUT opt  : A pointer to the option associated with the new parameter   */
/*            or NULL if none is found                                    */
/*                                                                        */
/* The returned pointer must be freed by the caller                       */
/* ====================================================================== */
static char *
look_for_valid_prefix_in_word(char * word, ctx_t * ctx, int * pos, opt_t ** opt)
{
  char * new = NULL;
  int     len;
  par_t * par;
  par_t   tmp_par = { 0 };

  len = strlen(word);

  if (len > 2)
  {
    new = xstrdup(word);

    do
    {
      new[--len]   = '\0';
      tmp_par.name = new;
    } while ((par = locate_par(tmp_par.name, ctx)) == NULL && len > 2);

    if (par != NULL)
    {
      *pos = len;
      *opt = par->opt;
    }
    else
    {
      free(new);
      new = NULL;
    }
  }
  else
    *pos = 0;

  return new;
}

/* ============================================================= */
/* is par_name is an unique abbreviation of an exiting parameter */
/* in the context ctx, then return this parameter.               */
/* ============================================================= */
static char *
abbrev_expand(char * par_name, ctx_t * ctx)
{
  user_object = par_name;
  user_rc     = 0;

  *user_string = '\0';
  bst_walk(ctx->par_bst, match_prefix_cb);
  rtrim(user_string, " ", 0);

  if (user_rc == 1) /* The number of matching abbreviations */
    return xstrdup(user_string);
  else
  {
    char *  s, *first_s;
    par_t * par;
    opt_t * opt;
    int     opt_count   = 0;
    void *  tmp_opt_bst = NULL;

    /* for each word in the matching parameters return by walking the */
    /* parameter's tree of this context, do:                          */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    s = first_s = strtok(user_string, " "); /* first_s holds a copy of *
                                             | the first word.         */
    while (s != NULL)
    {
      par = locate_par(s, ctx);
      opt = par->opt;

      if (bst_find(opt, &tmp_opt_bst, opt_compare) == NULL)
      {
        /* This option as not already been seen   */
        /* store it and increase the seen counter */
        /* """""""""""""""""""""""""""""""""""""" */
        bst_search(opt, &tmp_opt_bst, opt_compare);
        opt_count++;
      }
      s = strtok(NULL, " ");
    }

    if (tmp_opt_bst != NULL)
      bst_destroy(tmp_opt_bst, bst_null_action);

    if (opt_count == 1)
      /* All th abbreviation lead to only one option   */
      /* We can just continue as in the previous case. */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      return xstrdup(first_s);
    else
      return NULL;
  }
}

/* ==================================================== */
/* Returns 1 if mandatory options required by a context */
/* are not present, else returns 0.                     */
/* ==================================================== */
static void
check_for_missing_mandatory_opt(ctx_inst_t * ctx_inst, char * opt_par)
{
  if (has_unseen_mandatory_opt(ctx_inst))
  {
    if (ctx_inst->par_name != NULL)
    {
      fatal(CTXOPTMISPAR, opt_par, user_string, NULL, ctx_inst->par_name,
            ctx_inst->ctx->name,
            "Mandatory parameter(s): %s are missing in the context "
            "introduced by %s.",
            user_string, ctx_inst->par_name);
    }
    else
      fatal(CTXOPTMISPAR, opt_par, user_string, NULL, ctx_inst->par_name,
            ctx_inst->ctx->name,
            "Mandatory parameter(s): %s are missing in the first context.",
            user_string);
  }
}

/* ====================================================== */
/* Return 1 if at least one mandatory option was not seen */
/* when quitting a context, else 0                        */
/* ====================================================== */
static int
has_unseen_mandatory_opt(ctx_inst_t * ctx_inst)
{
  user_rc      = 0;
  *user_string = '\0';

  bst_walk(ctx_inst->seen_opt_bst, bst_seen_opt_cb);
  rtrim(user_string, " ", 0);

  return user_rc ? 1 : 0;
}

void
check_for_occurrences_issues(ctx_inst_t * ctx_inst)
{
  ctx_t *      ctx = ctx_inst->ctx;
  opt_t *      opt;
  ll_node_t *  node;
  opt_inst_t * opt_inst;

  /* Check options */
  /* """"""""""""" */

  node = ctx->opt_list->head;
  while (node != NULL)
  {
    opt = node->data;
    switch (opt->opt_count_oper)
    {
      case '=':
        if (opt->occurrences > 0 && opt->opt_count_mark != opt->occurrences)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must appear exactly %u times in this context, seen %u.",
                opt->params, opt->opt_count_mark, opt->occurrences);
        break;

      case '<':
        if (opt->occurrences > 0 && opt->opt_count_mark <= opt->occurrences)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must appear less than %u times in this context, seen %u.",
                opt->params, opt->opt_count_mark, opt->occurrences);
        break;

      case '>':
        if (opt->occurrences > 0 && opt->opt_count_mark >= opt->occurrences)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must appear more than %u times in this context, seen %u.",
                opt->params, opt->opt_count_mark, opt->occurrences);
        break;
    }

    node = node->next;
  }

  /* Check arguments */
  /* """"""""""""""" */

  node = ctx_inst->opt_inst_list->head;
  while (node != NULL)
  {
    opt_inst = node->data;
    opt      = opt_inst->opt;

    int nb_values = opt_inst->values_list->len; /* Number of arguments of opt */

    switch (opt->opt_args_count_oper)
    {
      case '=':
        if (nb_values > 0 && opt->opt_args_count_mark != nb_values)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must have exactly %u arguments, not %u.", opt->params,
                opt->opt_args_count_mark, nb_values);
        break;

      case '<':
        if (nb_values > 0 && opt->opt_args_count_mark <= nb_values)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must have less than %u arguments, not %u.", opt->params,
                opt->opt_args_count_mark, nb_values);
        break;

      case '>':
        if (nb_values > 0 && opt->opt_args_count_mark >= nb_values)
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s must have more than %u arguments, not %u.", opt->params,
                opt->opt_args_count_mark, nb_values);
        break;
    }

    node = node->next;
  }
}

/* ======================================================================== */
/* Parse a strings describing options and some of their characteristics     */
/* The input string must have follow some rules like in the examples below: */
/*                                                                          */
/* "opt_name1 opt_name2"                                                    */
/* "[opt_name1] opt_name2"                                                  */
/* "[opt_name1] opt_name2..."                                               */
/* "[opt_name1 #...] opt_name2... [#]"                                      */
/* "[opt_name1 [#...]] opt_name2... [#...]"                                 */
/*                                                                          */
/* Where [ ] encloses an optional part, # means: has parameters and ...     */
/* means that  there can be more than one occurrence of the previous thing. */
/*                                                                          */
/* opt_name can be followed by a 'new context' change prefixed with the     */
/* symbol >, as in opt1>c2 by eg                                            */
/*                                                                          */
/* This function returns as soon as one (or no) option has been parsed and  */
/* return the offset to the next option to parse.                           */
/*                                                                          */
/* In case of successful parsing, an new option is allocated and its        */
/* pointer returned.                                                        */
/* ======================================================================== */
static int
opt_parse(char * s, opt_t ** opt)
{
  int      opt_optional     = 0;
  int      opt_multiple     = 0;
  int      opt_count_matter = 0;
  char     opt_count_oper   = '\0';
  unsigned opt_count_mark   = 0;
  int      opt_args         = 0;
  char     opt_arg[33];
  int      opt_multiple_args     = 0;
  int      opt_args_count_matter = 0;
  char     opt_args_count_oper   = '\0';
  unsigned opt_args_count_mark   = 0;
  int      opt_optional_args     = 0;
  int      opt_eval_first        = 0;

  int n;
  int pos;
  int count = 0;

  char * s_orig = s;

  char * p;
  char * opt_name;
  char * next_ctx;
  char   token[65];

  *opt = NULL;
  memset(opt_arg, '\0', 33);

  if (*s == '[') /* Start of an optional option */
  {
    opt_optional = 1;
    s++;
  }
  s = strtoken(s, token, sizeof(token), "[^] \n\t.]", &pos);
  if (s == NULL)
    return -1; /* empty string */

  /* Early EOS, only return success if the option is mandatory */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!*s)
    if (opt_optional == 1)
      return -(s - s_orig - 1);

  /* validate the option name */
  /* ALPHA+(ALPHANUM|_)*      */
  /* """""""""""""""""""""""" */
  p = token;
  if (!isalpha(*p) && *p != '*')
    return -(s - s_orig - 1); /* opt_name must start with a letter */

  if (*p == '*')
    opt_eval_first = 1;

  p++;
  while (*p)
  {
    if (!isalnum(*p) && *p != '_' && *p != '>')
      return -(s - s_orig - 1); /* opt_name must contain a letter, *
                                 * a number or a _                 */
    p++;
  }

  if (opt_eval_first)
    opt_name = xstrdup(token + 1); /* Ignore the first '*' in token */
  else
    opt_name = xstrdup(token);

  if (*s == ']')
  {
    s++;
    while (isblank(*s))
      s++;

    goto success;
  }

  /* Check if it can appear multiple times by looking for the dots */
  p = strtoken(s, token, 3, "[.]", &pos);
  if (p)
  {
    if (strcmp(token, "...") == 0)
    {
      opt_multiple = 1;
      s            = p;
      if (*s == '<' || *s == '=' || *s == '>')
      {
        unsigned count;
        int      offset;

        n = sscanf(s + 1, "%u%n", &count, &offset);
        if (n == 1)
        {
          opt_count_matter = 1;
          opt_count_oper   = *s;
          opt_count_mark   = count;
        }
        s += offset + 1;
      }
    }
    else
      return -(s - s_orig - 1);
  }

  /* A blank separates the option name and the argument tag */
  if (isblank(*s))
  {
    char dots[4];

    while (isblank(*s))
      s++;

    if (!*s)
      goto success;

    pos = 0;
    n   = sscanf(s, "[%32[^] .\t]%n%3[.]", opt_arg, &pos, dots);
    if (pos > 1) /* [# was read */
    {
      opt_args          = 1;
      opt_optional_args = 1;
      if (n == 2)
        opt_multiple_args = 1; /* there were dots */

      s += pos + !!(n == 2) * 3; /* skip the dots */

      if (*s == '<' || *s == '=' || *s == '>')
      {
        unsigned count;
        int      offset;

        n = sscanf(s + 1, "%u%n", &count, &offset);
        if (n == 1)
        {
          opt_args_count_matter = 1;
          opt_args_count_oper   = *s;
          opt_args_count_mark   = count;
        }
        s += offset + 1;
      }

      /* optional arg tag must end with a ] */
      if (*s != ']')
        return -(s - s_orig - 1);

      s++; /* skip the ] */
    }
    else
    {
      n = sscanf(s, "%32[^] .\t]%n%3[.]", opt_arg, &pos, dots);
      if (pos > 0) /* # was read */
      {
        opt_args = 1;
        if (n == 2) /* there were dots */
          opt_multiple_args = 1;

        s += pos + !!(n == 2) * 3; /* skip the dots */

        if (*s == '<' || *s == '=' || *s == '>')
        {
          unsigned count;
          int      offset;

          n = sscanf(s + 1, "%u%n", &count, &offset);
          if (n == 1)
          {
            opt_args_count_matter = 1;
            opt_args_count_oper   = *s;
            opt_args_count_mark   = count;
          }
          s += offset + 1;
        }
      }
    }
    if (*s == ']')
    {
      /* Abort on extraneous ] if the option is mandatory */
      if (!opt_optional)
        return -(s - s_orig - 1);

      s++; /* skip the ] */

      /* strip the following blanks */
      while (isblank(*s))
        s++;

      goto success;
    }
    else if (opt_optional == 0 && (!*s || isblank(*s)))
    {
      /* strip the following blanks */
      while (isblank(*s))
        s++;

      goto success;
    }
    else if (opt_args == 0) /* # was not read it is possibly the start *
                             * of another option                       */
      goto success;
    else
      return -(s - s_orig - 1);
  }

success:

  /* strip the following blanks */
  while (isblank(*s))
    s++;

  next_ctx = NULL;

  if (*opt_name == '>')
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "%s: option name is missing.", opt_name);

  count = strchrcount(opt_name, '>');
  if (count == 1)
  {
    char * tmp = strchr(opt_name, '>');
    next_ctx   = xstrdup(tmp + 1);
    *tmp       = '\0';
  }
  else if (count > 1)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "%s: only one occurrence of '>' is allowed.", opt_name);

  *opt = xmalloc(sizeof(opt_t));

  (*opt)->name                  = opt_name;
  (*opt)->optional              = opt_optional;
  (*opt)->multiple              = opt_multiple;
  (*opt)->opt_count_matter      = opt_count_matter;
  (*opt)->opt_count_oper        = opt_count_oper;
  (*opt)->opt_count_mark        = opt_count_mark;
  (*opt)->args                  = opt_args;
  (*opt)->arg                   = xstrdup(opt_arg);
  (*opt)->optional_args         = opt_optional_args;
  (*opt)->multiple_args         = opt_multiple_args;
  (*opt)->opt_args_count_matter = opt_args_count_matter;
  (*opt)->opt_args_count_oper   = opt_args_count_oper;
  (*opt)->opt_args_count_mark   = opt_args_count_mark;
  (*opt)->eval_first            = opt_eval_first;
  (*opt)->next_ctx              = next_ctx;
  (*opt)->ctx_list              = ll_new();
  (*opt)->constraints_list      = ll_new();
  (*opt)->action                = NULL;
  (*opt)->data                  = NULL;

  return s - s_orig;
}

/* ===================================================================== */
/* Try to initialize all the option in a given string                    */
/* Each parsed option are put in a bst tree with its name as index       */
/*                                                                       */
/* On collision, the arguments only the signature are required to be     */
/* the same else this is considered as an error. Options can be used in  */
/* more than one context and can be optional in one and mandatory in     */
/* another                                                               */
/* ===================================================================== */
static int
init_opts(char * spec, ctx_t * ctx)
{
  opt_t * opt, *bst_opt;
  bst_t * node;
  int     offset;

  ltrim(spec, " \n\t");
  while (*spec)
  {
    if ((offset = opt_parse(spec, &opt)) > 0)
    {
      spec += offset;

      if ((node = bst_find(opt, &options_bst, opt_compare)) != NULL)
      {
        int same_next_ctx = 0;

        bst_opt = node->key; /* node extracted from the BST */

        if (bst_opt->next_ctx == NULL && opt->next_ctx == NULL)
          same_next_ctx = 1;
        else if (bst_opt->next_ctx == NULL && opt->next_ctx != NULL)
          same_next_ctx = 0;
        else if (bst_opt->next_ctx != NULL && opt->next_ctx == NULL)
          same_next_ctx = 0;
        else
          same_next_ctx = strcmp(bst_opt->next_ctx, opt->next_ctx) == 0;

        if (bst_opt->optional_args != opt->optional_args
            || bst_opt->multiple_args != opt->multiple_args
            || bst_opt->args != opt->args || !same_next_ctx)
        {
          error("option %s already exists with "
                "a different arguments signature.\n",
                opt->name);

          free(opt->name);
          free(opt);

          return 0;
        }
        /* The new occurrence of the option option is legal */
        /* append the current context ptr in the list       */
        /* """""""""""""""""""""""""""""""""""""""""""""""" */
        ll_append(bst_opt->ctx_list, ctx);

        /* Append the new option to the context's options list */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
        ll_append(ctx->opt_list, bst_opt);
      }
      else
      {
        opt->params = NULL;

        /* Initialize the option's context list with the current context */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        ll_append(opt->ctx_list, ctx);

        /* Append the new option to the context's options list */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
        ll_append(ctx->opt_list, opt);

        /* Insert the new option in the BST */
        /* """""""""""""""""""""""""""""""" */
        bst_search(opt, &options_bst, opt_compare);
      }
    }
    else
    {
      char * s = xstrndup(spec, -offset);
      printf("%s <---\nSyntax error at or before offset %d\n", s, -offset);
      free(s);

      exit(EXIT_FAILURE);
    }
  }
  return 1;
}

/* ==================================================== */
/* ctxopt initialization function, must be called first */
/* ==================================================== */
void
ctxopt_init(void)
{
  int n;

  contexts_bst = NULL;
  options_bst  = NULL;

  user_rc      = 0;
  user_string  = xmalloc(8);
  user_string2 = xmalloc(8);
  user_object  = NULL;

  ctxopt_initialized = 1;

  /* Initialize custom error function pointers to NULL */
  /* """"""""""""""""""""""""""""""""""""""""""""""""" */
  err_functions = xmalloc(CTXOPTERRSIZE * sizeof(void *));
  for (n = 0; n < CTXOPTERRSIZE; n++)
    err_functions[n] = NULL;
}

static int
opt_set_parms(char * opt_name, char * par_str)
{
  char *  par_name, *ctx_name;
  char *  tmp_par_str, *end_tmp_par_str;
  ctx_t * ctx;
  opt_t * opt;
  bst_t * node;
  par_t * par, tmp_par;
  int     rc = 1; /* return code */

  ll_t *      list;
  ll_node_t * lnode;

  /* Look is the given option is defined */
  /* """"""""""""""""""""""""""""""""""" */
  opt = locate_opt(opt_name);
  if (opt == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL, "Unknown option %s",
          opt_name);

  /* For each context using this option */
  /* """""""""""""""""""""""""""""""""" */
  list = opt->ctx_list;

  lnode = list->head;
  while (lnode != NULL)
  {
    /* Locate the context in the contexts tree */
    /* """"""""""""""""""""""""""""""""""""""" */
    ctx_name = ((ctx_t *)(lnode->data))->name;

    ctx = locate_ctx(ctx_name);
    if (ctx == NULL)
      fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL, "Unknown context %s",
            ctx_name);
    else
    {
      void * par_bst = ctx->par_bst;

      tmp_par_str = xstrdup(par_str);
      par_name    = xstrtok_r(tmp_par_str, " \t,", &end_tmp_par_str);
      if (par_name == NULL)
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "Parameters are missing for option %s", opt_name);

      /* For each parameter given in par_str, create an par_t object and */
      /* insert it the in the parameters bst of the context.             */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      while (par_name != NULL)
      {
        tmp_par.name = par_name;

        node = bst_find(&tmp_par, &par_bst, par_compare);
        if (node != NULL)
        {
          error("The parameter %s is already defined in context %s", par_name,
                ctx->name);
          rc = 0;
        }
        else
        {
          par       = xmalloc(sizeof(par_t));
          par->name = xstrdup(par_name);
          par->opt  = opt; /* Link the option to this parameter */

          bst_search(par, &par_bst, par_compare);
        }
        par_name = xstrtok_r(NULL, " \t,", &end_tmp_par_str);
      }

      /* Update the value of the root of ctx->par_bst as it may have */
      /* been modified                                               */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      ctx->par_bst = par_bst;

      free(tmp_par_str);
    }
    lnode = lnode->next;
  }

  return rc;
}

/* ===================================================================== */
/* Creation of a new context instance                                    */
/* IN ctx           : a context pointer to allow this instance to access */
/*                    the context fields                                 */
/* IN prev_ctx_inst : the context instance whose option leading to the   */
/*                    creation of this new context instance is part of   */
/* ===================================================================== */
static ctx_inst_t *
new_ctx_inst(ctx_t * ctx, ctx_inst_t * prev_ctx_inst)
{
  opt_t *      opt;
  opt_inst_t * gen_opt_inst;
  ctx_inst_t * ctx_inst;
  seen_opt_t * seen_opt;
  char *       str, *opt_name;
  void *       bst;
  bst_t *      bst_node;

  /* Keep a trace of the opt_inst which was at the origin of the creation */
  /* of this context instance.                                            */
  /* This will serve during the evaluation of the option callbacks        */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (prev_ctx_inst != NULL)
    gen_opt_inst = (opt_inst_t *)(prev_ctx_inst->opt_inst_list->tail->data);
  else
    gen_opt_inst = NULL;

  /* Create and initialize the new context instance */
  /* """""""""""""""""""""""""""""""""""""""""""""" */
  ctx_inst                  = xmalloc(sizeof(ctx_inst_t));
  ctx_inst->ctx             = ctx;
  ctx_inst->prev_ctx_inst   = prev_ctx_inst;
  ctx_inst->gen_opt_inst    = gen_opt_inst;
  ctx_inst->incomp_bst_list = ll_new();
  ctx_inst->opt_inst_list   = ll_new();
  ctx_inst->seen_opt_bst    = NULL;

  ll_node_t * node;

  if (prev_ctx_inst == NULL)
    first_ctx_inst = ctx_inst;

  /* Initialize the occurrence counters of each opt allowed in the context */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = ctx->opt_list->head;
  while (node != NULL)
  {
    opt              = node->data;
    opt->occurrences = 0;

    node = node->next;
  }

  /* Initialize the bst containing the seen indicator for all the options */
  /* allowed in this context instance.                                    */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = ctx->opt_list->head;
  while (node != NULL)
  {
    opt            = node->data;
    seen_opt       = xmalloc(sizeof(seen_opt_t));
    seen_opt->opt  = opt;
    seen_opt->seen = 0;

    bst_search(seen_opt, &(ctx_inst->seen_opt_bst), seen_opt_compare);

    node = node->next;
  }

  /* Initialize the bst containing the incompatibles options               */
  /* Incompatibles option names are read from strings found in the list    */
  /* incomp_list present in each instance of ctx_t.                        */
  /* These names are then used to search for the object of type seen_opt_t */
  /* which is already present in the seen_opt_bst of the context instance. */
  /* in the bst.                                                           */
  /* Once found the seen_opt_t object in inserted in the new bst           */
  /* At the end the new bst in addes to the list incomp_bst_list.          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = ctx->incomp_list->head;
  while (node != NULL)
  {
    bst = NULL;
    seen_opt_t tmp_seen_opt;

    str      = xstrdup(node->data);
    opt_name = strtok(str, " \t"); /* extract the first option name */

    while (opt_name != NULL) /* for each option name */
    {
      if ((opt = locate_opt(opt_name)) != NULL)
      {
        /* The option found is searched in the tree of potential */
        /* seen options.                                         */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        tmp_seen_opt.opt = opt;

        bst_node = bst_find(&tmp_seen_opt, &(ctx_inst->seen_opt_bst),
                            seen_opt_compare);

        if (bst_node != NULL)
        {
          /* if found it, is added into the new bst tree */
          /* """"""""""""""""""""""""""""""""""""""""""" */
          seen_opt = bst_node->key;
          bst_search(seen_opt, &bst, seen_opt_compare);
        }
        else
          /* Not found! That means that the option is unknown in this */
          /* context as all options has have a seen_opt structure in  */
          /* seen_opt_bst                                             */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s is not known in the context %s", opt->name, ctx->name);
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "%s: unknown option.", opt_name);

      opt_name = strtok(NULL, " \t");
    }

    free(str);
    ll_append(ctx_inst->incomp_bst_list, bst);

    node = node->next;
  }

  return ctx_inst;
}

/* ======================================================================== */
/* Create a list formed by all the significant command line words           */
/* Words beginning or ending with ^,{ or } are split. Each of these         */
/* symbols will get their own place in the list                             */
/*                                                                          */
/* - ^ forces the next option to be evaluated in the first context          */
/* - the {...} part delimits a context, the { will not appear in the list   */
/*   and the } will be replaced by a | in the resulting list (cmdline_list) */
/*   to facilitate the parsing phase. | must not be used by the end user    */
/*                                                                          */
/* IN nb_word : number of word to parse, this is typically argc-1 as the    */
/*              program name is not considered                              */
/* IN words   : is the array of strings constituting the command line to    */
/*              parse.                                                      */
/* Returns    : 1 on success, 0 if a { or } is missing.                     */
/* ======================================================================== */
static int
ctxopt_build_cmdline_list(int nb_words, char ** words)
{
  int        i;
  char *     prev_word = NULL;
  char *     word;
  int        level = 0;
  ll_node_t *node, *start_node;

  /* The analysis is divided into three passes, this is not optimal but  */
  /* must be done only one time ans so we can privilege the readability  */
  /*                                                                     */
  /* In the following, SG is the ascii character 1d (dec 29)             */
  /*                                                                     */
  /* The first pass creates the list, extract the leading an trailing    */
  /*  SG '{' and '}' of each word * and give them their own place in     */
  /* the list                                                            */
  /*                                                                     */
  /* The second pass transform the '{...}' blocks by a trailing SG       */
  /* ({...} -> ...|)                                                     */
  /*                                                                     */
  /* The last pass remove the duplicated SG and check for SG, '{' or '}' */
  /* in the middle in the remaining list elements                        */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  /* If the option list is not empty, clear it before going further */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

  if (cmdline_list != NULL)
  {
    node = cmdline_list->head;
    while (node != NULL)
    {
      word = node->data;
      ll_delete(cmdline_list, node);
      free(word);
      free(node);
      node = cmdline_list->head;
    }
  }
  else
    cmdline_list = ll_new();

  start_node = cmdline_list->head; /* in the following loop start_node will *
                                    * contain a pointer to the current      *
                                    * word stripped from its leading        *
                                    * sequence of {, } or ^                 */
  for (i = 0; i < nb_words; i++)
  {
    size_t len = strlen(words[i]);
    size_t start, end;
    char * str;

    str = words[i];

    /* the word {} is not a group, do not interpret it. */
    /* """""""""""""""""""""""""""""""""""""""""""""""" */
    if (strcmp(str, "{}") == 0)
    {
      ll_append(cmdline_list, xstrdup(str));
      start_node = cmdline_list->tail;
      continue;
    }

    if (len > 1) /* The word contains at least 2 characters */
    {
      start = 0;

      /* Interpret its beginning and look for the start of the real word */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      while (start <= len - 1 && (str[start] == '{' || str[start] == '}'))
      {
        ll_append(cmdline_list, xstrndup(str + start, 1));
        start++;
        start_node = cmdline_list->tail;
      }

      end = len - 1;
      if (str[end] == '{' || str[end] == '}')
      {
        if (end > 0 && str[end - 1] != '\\')
        {
          ll_append(cmdline_list, xstrndup(str + end, 1));
          end--;
          node = cmdline_list->tail;

          while (str[end] == '{' || str[end] == '}')
          {
            if (end > start && str[end - 1] == '\\')
              break;

            ll_insert_before(cmdline_list, node, xstrndup(str + end, 1));
            end--;
            node = node->prev;
          }
        }
      }

      if (start <= end)
      {
        if (start_node != NULL)
          ll_insert_after(cmdline_list, start_node,
                          xstrndup(str + start, end - start + 1));
        else
          ll_append(cmdline_list, xstrndup(str + start, end - start + 1));
        start_node = cmdline_list->tail;
      }
    }
    else if (len == 1)
    {
      ll_append(cmdline_list, xstrdup(str));
      start_node = cmdline_list->tail;
    }
  }

  /* 2nd pass */
  /* """""""" */
  node = cmdline_list->head;

  level = 0;
  while (node != NULL)
  {
    word = node->data;

    if (strcmp(word, "{") == 0)
    {
      ll_node_t * old_node = node;
      level++;
      node = node->next;
      ll_delete(cmdline_list, old_node);
      free(word);
      free(old_node);
    }
    else if (strcmp(word, "}") == 0)
    {
      level--;

      if (level < 0)
        return 0;
      else
        *word = 0x1d;
    }
    else
      node = node->next;
  }

  if (level != 0)
    return 0;

  /* 3rd pass */
  /* """""""" */
  node = cmdline_list->head;

  while (node != NULL)
  {
    word = node->data;

    /* {} is a legal word */
    /* """""""""""""""""" */
    if (strcmp(word, "{}") == 0)
      goto next;

    /* Remove a SG if the previous element is SG */
    /* """"""""""""""""""""""""""""""""""""""""" */
    if (strcmp(word, "\x1d") == 0)
      if (prev_word != NULL && (strcmp(prev_word, "\x1d") == 0))
      {
        ll_node_t * old_node = node;
        node                 = node->prev;
        ll_delete(cmdline_list, old_node);
        free(old_node->data);
        free(old_node);
        goto next;
      }

  next:
    prev_word = node->data;
    node      = node->next;
  }

  /* Clean useless and SG at the beginning and end of list */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = cmdline_list->head;

  if (node == NULL)
    return 1;

  word = node->data;

  if (strcmp(word, "\x1d") == 0)
  {
    ll_delete(cmdline_list, node);
    free(word);
    free(node);
  }

  node = cmdline_list->tail;
  if (node == NULL)
    return 1;

  word = node->data;

  if (strcmp(word, "\x1d") == 0)
  {
    ll_delete(cmdline_list, node);
    free(word);
    free(node);
  }

  return 1;
}

/* ===================================================================== */
/* Build and analyze the command line list                               */
/* function and create the linked data structures whose data will be     */
/* evaluated later by ctxopt_evaluate.                                   */
/* This function identifies the following errors and creates an array of */
/* The remaining unanalyzed arguments.                                   */
/* - detect missing arguments                                            */
/* - detect too many arguments                                           */
/* - detect unknown parameters in a context                              */
/* - detect too many occurrences of a parameters in a context            */
/* - detect missing required arguments in a context                      */
/*                                                                       */
/* IN nb_word : number of word to parse, this is typically argc-1 as the */
/*              program name is not considered                           */
/* IN words   : is the array of strings constituting the command line to */
/*              parse.                                                   */
/* OUT nb_rem_args : nb of remaining command line arguments if a --      */
/*                   is present in the list.                             */
/* OUT rem_args    : array of remaining command line arguments if a --   */
/*                   is present in the list. This array must be free by  */
/*                   The caller as it is allocated here.                 */
/* ===================================================================== */
void
ctxopt_analyze(int nb_words, char ** words, int * nb_rem_args,
               char *** rem_args)
{
  ctx_t *      ctx;
  opt_t *      opt;
  par_t *      par;
  ctx_inst_t * ctx_inst;
  opt_inst_t * opt_inst;
  int          expect_par        = 0;
  int          expect_arg        = 0;
  int          expect_par_or_arg = 0;

  ll_node_t *  cli_node;
  bst_t *      bst_node;
  seen_opt_t * bst_seen_opt;
  char *       par_name;
  void *       bst;

  ll_node_t * node;

  if (!ctxopt_build_cmdline_list(nb_words, words))
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "The command line could not be parsed: missing { or } detected.");

  if (main_ctx == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "At least one context must have been created.");

  /* Create the first ctx_inst record */
  /* """""""""""""""""""""""""""""""" */
  ctx = main_ctx;

  ctx_inst_list      = ll_new();
  ctx_inst           = new_ctx_inst(ctx, NULL);
  ctx_inst->par_name = NULL;

  ll_append(ctx_inst_list, ctx_inst);

  /* For each node in the command line */
  /* """"""""""""""""""""""""""""""""" */
  cli_node   = cmdline_list->head;
  expect_par = 1;
  while (cli_node != NULL)
  {
    if (strcmp(cli_node->data, "--") == 0)
      break; /* No new parameter will be analyze after this point */

    par_name = cli_node->data;

    if (strcmp(par_name, "\x1d") == 0)
    {
      check_for_missing_mandatory_opt(ctx_inst, (char *)(cli_node->prev->data));
      check_for_occurrences_issues(ctx_inst);

      /* Forced backtracking to the previous context instance */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (ctx_inst->prev_ctx_inst != NULL)
      {
        ctx_inst = ctx_inst->prev_ctx_inst;
        ctx      = ctx_inst->ctx;
      }
    }
    else if (expect_par && *par_name == '-')
    {
      int    pos = 0;
      char * prefix;

      /* An expected parameter has been seen */
      /* """"""""""""""""""""""""""""""""""" */
      if ((par = locate_par(par_name, ctx)) == NULL)
      {
        opt_t * popt;
        char *  word;

        /* See if this parameter is an unique abbreviation of a longer   */
        /* parameter. If this is the case then just replace it with its  */
        /* full length version and try again.                            */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if ((word = abbrev_expand(par_name, ctx)) != NULL)
        {
          cli_node->data = word;
          continue;
        }

        /* Try to find a prefix which is a valid parameter in this context */
        /* If found, split the cli_node in two to build a new parameter    */
        /* node and followed by a node containing the remaining string     */
        /* If the new parameter corresponds to an option not taking        */
        /* argument then prefix the remaining string whit a dash as it may */
        /* contain a new parameter.                                        */
        /* The new parameter will be re-evaluated in the next iteration of */
        /* the loop.                                                       */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        prefix = look_for_valid_prefix_in_word(par_name, ctx, &pos, &popt);
        if (prefix != NULL && pos != 0)
        {
          cli_node->data = prefix; /* prefix contains le name of a valid *
                                    | parameter in this context.         */

          if (popt->args)
            /* The parameter may be followed by arguments */
            /* '''''''''''''''''''''''''''''''''''''''''' */
            word = xstrdup(par_name + pos);
          else
            /* The parameter does not take arguments, the    */
            /* following word must be a parameter or nothing */
            /* hence prefix it with a dash.                  */
            /* ''''''''''''''''''''''''''''''''''''''''''''' */
            word = concat("-", par_name + pos, NULL);

          /* Insert it after the current node in the list */
          /* """""""""""""""""""""""""""""""""""""""""""" */
          ll_insert_after(cmdline_list, cli_node, word);

          continue; /* loop */
        }
        else
        {
          check_for_missing_mandatory_opt(ctx_inst, par_name);
          check_for_occurrences_issues(ctx_inst);

          if (ctx_inst->prev_ctx_inst == NULL)
          {
            char * errmsg = "Unknown parameter: %s";

            *user_string  = '\0';
            *user_string2 = '\0';

            user_string = concat(user_string, par_name, NULL);
            bst_walk(contexts_bst, bst_match_par_cb);

            if (*user_string2 != '\0')
              errmsg = concat(errmsg,
                              "\nIt appears to be defined in the context(s):",
                              user_string2,
                              "\n'smenu -h' or 'smenu -H' can be helpful here.",
                              NULL);

            fatal(CTXOPTUNKPAR, par_name, NULL, NULL, ctx_inst->par_name,
                  ctx_inst->ctx->name, errmsg, par_name);
          }
          else
          {
            /* Try to backtrack and analyse the same parameter in the */
            /* previous context.                                      */
            /* """""""""""""""""""""""""""""""""""""""""""""""""""""" */
            ctx_inst = ctx_inst->prev_ctx_inst;
            ctx      = ctx_inst->ctx;

            cli_node = cli_node->prev;
          }
        }
      }
      else
      {
        seen_opt_t seen_opt;

        /* The parameter is legal in the context, create a opt_inst and */
        /* append it to the ctx_inst list options list                  */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        opt = par->opt;

        opt->occurrences++;

        opt_inst                = xmalloc(sizeof(opt_inst_t));
        opt_inst->opt           = opt;
        opt_inst->par           = par_name;
        opt_inst->values_list   = ll_new();
        opt_inst->next_ctx_inst = NULL;

        /* Priority option inserted at the start of the opt_inst list    */
        /* but their order of appearance in the context definition must  */
        /* be preserver so each new priority option will be placed after */
        /* the previous ones at the start of the opt_inst list.          */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (!opt->eval_first)
          ll_append(ctx_inst->opt_inst_list, opt_inst);
        else
        {
          ll_node_t *  node = ctx_inst->opt_inst_list->head;
          opt_inst_t * tmp_opt_inst;
          while (node != NULL)
          {
            tmp_opt_inst = node->data;
            if (!tmp_opt_inst->opt->eval_first)
            {
              ll_insert_before(ctx_inst->opt_inst_list, node, opt_inst);
              break;
            }
            else
              node = node->next;
          }
          if (node == NULL)
            ll_append(ctx_inst->opt_inst_list, opt_inst);
        }

        /* Check if an option was already seen in the */
        /* current context instance                   */
        /* """""""""""""""""""""""""""""""""""""""""" */
        seen_opt.opt = opt;

        bst_node = bst_find(&seen_opt, &(ctx_inst->seen_opt_bst),
                            seen_opt_compare);

        /* bst_node cannot be NULL here */

        bst_seen_opt = (seen_opt_t *)(bst_node->key);

        if (!opt->multiple && bst_seen_opt->seen == 1)
          fatal(CTXOPTDUPOPT, par_name, opt->params, opt->name,
                ctx_inst->par_name, ctx_inst->ctx->name,
                "The parameter(s) %s can only appear once in this context.",
                opt->params);

        /* Check if this option is compatible with the options already */
        /* seen in this context instance.                              */
        /* Look if the option is present in one on the bst present in  */
        /* the incomp_bst_list of the context instance                 */
        /* instance,                                                   */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        node = ctx_inst->incomp_bst_list->head;
        while (node != NULL)
        {
          bst         = node->data;
          user_object = NULL;

          /* They can only have one seen_opt object in the bst tree was */
          /* already seen, try to locate it, the result will be put in  */
          /* user_object by the bst_seen_opt_seen_cb function           */
          /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          bst_walk(bst, bst_seen_opt_seen_cb);

          /* It it is the case, look if the current option is also */
          /* in this bst.                                          */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
          if (user_object != NULL)
          {
            bst_node = bst_find(bst_seen_opt, &bst, seen_opt_compare);

            if (bst_node != NULL)
            {
              bst_seen_opt = (seen_opt_t *)(bst_node->key);
              if (bst_seen_opt->seen == 0)
                fatal(CTXOPTINCTXOPTT, par_name, (char *)user_object,
                      bst_seen_opt->opt->name, ctx_inst->par_name,
                      ctx_inst->ctx->name, "%s is incompatible with %s\n",
                      par_name, (char *)user_object);
            }
          }

          node = node->next;
        }

        /* Mark this option seen in the current context instance */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        bst_seen_opt->seen = 1;
        bst_seen_opt->par  = xstrdup(par_name);

        /* If this option leads to a next context, create a new ctx_inst */
        /* and switch to it for the analyse of the future parameter      */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (opt->next_ctx != NULL)
        {
          ctx = locate_ctx(opt->next_ctx);

          if (ctx == NULL)
            fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                  "%s: unknown context.", opt->next_ctx);

          opt_inst->next_ctx_inst = ctx_inst = new_ctx_inst(ctx, ctx_inst);
          ctx_inst->par_name                 = xstrdup(par_name);

          ll_append(ctx_inst_list, ctx_inst);
        }

        /* See is we must expect some arguments */
        /* """""""""""""""""""""""""""""""""""" */
        expect_par_or_arg = 0;
        expect_par        = 0;
        expect_arg        = 0;

        if (!opt->args)
          expect_par = 1; /* Parameter doesn't accept any argument */
        else
        {
          if (!opt->optional_args)
            expect_arg = 1; /* Parameter has mandatory arguments */
          else
            expect_par_or_arg = 1; /* Parameter has optional arguments */
        }
      }
    }
    else if (expect_par && *par_name != '-')
      break; /* a unexpected non parameter was seen, assume it is the end *
              | of parameters and the start of non ctxopt managed command *
              | line arguments.                                           */
    else if (expect_arg && *par_name != '-')
    {
      ll_node_t *    cstr_node;
      constraint_t * cstr;

      /* Check if the arguments of the option respects */
      /* the attached constraints if any               */
      /* """"""""""""""""""""""""""""""""""""""""""""" */
      cstr_node = opt->constraints_list->head;
      while (cstr_node != NULL)
      {
        cstr = cstr_node->data;
        if (!cstr->constraint(cstr->nb_args, cstr->args, par_name))
          exit(EXIT_FAILURE);

        cstr_node = cstr_node->next;
      }

      /* If the argument is valid, store it */
      /* """""""""""""""""""""""""""""""""" */
      ll_append(opt_inst->values_list, par_name);

      expect_arg        = 0;
      expect_par        = 0;
      expect_par_or_arg = 0;

      if (opt->multiple_args)
        expect_par_or_arg = 1;
      else
        expect_par = 1; /* Parameter takes only one argument */
    }
    else if (expect_arg && *par_name == '-')
    {
      char * prev_arg;

      prev_arg = (char *)(cli_node->prev->data);

      fatal(CTXOPTMISARG, prev_arg, opt->params, opt->name, ctx_inst->par_name,
            ctx_inst->ctx->name, "%s requires argument(s).", prev_arg);
    }
    else if (expect_par_or_arg)
    {
      expect_arg        = 0;
      expect_par        = 0;
      expect_par_or_arg = 0;

      if (*par_name != '-')
        expect_arg = 1; /* Consider this word as an argument and retry */
      else
        expect_par = 1; /* Consider this word as a parameter and retry */

      cli_node = cli_node->prev;
    }

    cli_node = cli_node->next;
  }

  if (cmdline_list->len > 0 && *par_name == '-')
  {
    if (expect_arg && !opt->optional_args)
      fatal(CTXOPTMISARG, par_name, opt->params, opt->name, ctx_inst->par_name,
            ctx_inst->ctx->name, "%s requires argument(s).", par_name);
  }

  /* Look if a context_instance has unseen mandatory options */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  node = ctx_inst_list->head;
  while (node != NULL)
  {
    ctx_inst = (ctx_inst_t *)(node->data);

    check_for_missing_mandatory_opt(ctx_inst, par_name);
    check_for_occurrences_issues(ctx_inst);

    node = node->next;
  }

  /* Allocate the array containing the remaining not analyzed */
  /* command line arguments.                                  */
  /* NOTE: The strings in the array are just pointer to the   */
  /*       data of the generating list and must not be freed. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (cli_node != NULL)
  {
    if (strcmp((char *)cli_node->data, "--") == 0)
      /* The special parameter -- was encountered, the -- argument is not */
      /* put in the remaining arguments.                                  */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      ll_strarray(cmdline_list, cli_node->next, nb_rem_args, rem_args);
    else
      /* A non parameter was encountered when a parameter was expected. We  */
      /* assume that the evaluation of the remaining command line  argument */
      /* are not the responsibility of the users code.                      */
      /* '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''' */
      ll_strarray(cmdline_list, cli_node, nb_rem_args, rem_args);
  }
  else
  {
    nb_rem_args    = 0;
    *rem_args      = xmalloc(sizeof(char *));
    (*rem_args)[0] = NULL;
  }
}

/* ===================================================================== */
/* Parses the options data structures and launches the callback function */
/* attached to each options instances.                                   */
/* This calls a recursive function which proceeds context per context.   */
/* ===================================================================== */
void
ctxopt_evaluate(void)
{
  evaluate_ctx_inst(first_ctx_inst);
}

/* =================================================================== */
/* Recursive function called by ctxopt_evaluate to process the list of */
/* the opt_inst present in a ctx_inst and attempt  to evaluate the     */
/* action attached to the context and its option instances             */
/* =================================================================== */
static void
evaluate_ctx_inst(ctx_inst_t * ctx_inst)
{
  opt_inst_t * opt_inst;
  ctx_t *      ctx;
  opt_t *      opt;
  ll_node_t *  opt_inst_node;
  char **      args;
  int          nb_args;
  int          i;

  if (ctx_inst == NULL)
    return;

  ctx = ctx_inst->ctx;

  /* Call the entering action attached to this context if any */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ctx->action != NULL)
  {
    if (ctx_inst->prev_ctx_inst != NULL)
      ctx->action(ctx->name, entering, ctx_inst->prev_ctx_inst->ctx->name,
                  ctx->nb_data, ctx->data);
    else
      ctx->action(ctx->name, entering, NULL, ctx->nb_data, ctx->data);
  }

  /* For each instance of options */
  /* """""""""""""""""""""""""""" */
  opt_inst_node = ctx_inst->opt_inst_list->head;
  while (opt_inst_node != NULL)
  {
    opt_inst = (opt_inst_t *)(opt_inst_node->data);
    ll_strarray(opt_inst->values_list, opt_inst->values_list->head, &nb_args,
                &args);
    opt = opt_inst->opt;

    /* Launch the attached action if any */
    /* """"""""""""""""""""""""""""""""" */
    if (opt->action != NULL)
      opt->action(ctx->name, opt->name, opt_inst->par, nb_args, args,
                  opt->nb_data, opt->data, ctx->nb_data, ctx->data);

    if (opt_inst->next_ctx_inst != NULL)
      evaluate_ctx_inst(opt_inst->next_ctx_inst);

    for (i = 0; i < nb_args; i++)
      free(args[i]);
    free(args);

    opt_inst_node = opt_inst_node->next;
  }

  /* Call the exiting action attached to this context if any */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (ctx->action != NULL)
  {
    if (ctx_inst->prev_ctx_inst != NULL)
      ctx->action(ctx->name, exiting, ctx_inst->prev_ctx_inst->ctx->name,
                  ctx->nb_data, ctx->data);
    else
      ctx->action(ctx->name, exiting, NULL, ctx->nb_data, ctx->data);
  }
}

/* =========================================================== */
/* Create and initialize a new context                         */
/* - allocate space                                            */
/* - name it                                                   */
/* - initialize its option with a few of their characteristics */
/* =========================================================== */
void
ctxopt_new_ctx(char * name, char * opts_specs)
{
  ctx_t * ctx;
  bst_t * node;

  if (!ctxopt_initialized)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Please call ctxopt_init first.");

  ctx = xmalloc(sizeof(ctx_t));

  /* The first created context is the main one */
  /* """"""""""""""""""""""""""""""""""""""""" */
  if (contexts_bst == NULL)
    main_ctx = ctx;

  ctx->name        = xstrdup(name);
  ctx->opt_list    = ll_new(); /* list of options legit in this context */
  ctx->incomp_list = ll_new(); /* list of incompatible options strings  */
  ctx->par_bst     = NULL;
  ctx->data        = NULL;
  ctx->action      = NULL;

  if (init_opts(opts_specs, ctx) == 0)
    exit(EXIT_FAILURE);
  if ((node = bst_find(ctx, &contexts_bst, ctx_compare)) != NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "The context %s already exists", name);
  else
    bst_search(ctx, &contexts_bst, ctx_compare);
}

/* ==================================================== */
/* Display a usage screen limited to a specific context */
/* IN: the context name.                                */
/* IN: what to do after (continue or exit the program)  */
/*     possible values: continue_after, exit_after.     */
/* ==================================================== */
void
ctxopt_ctx_disp_usage(char * ctx_name, usage_behaviour action)
{
  ctx_t * ctx;
  ll_t *  list;

  int has_optional    = 0;
  int has_ellipsis    = 0;
  int has_rule        = 0;
  int has_generic_arg = 0;
  int has_ctx_change  = 0;
  int has_early_eval  = 0;

  ctx = locate_ctx(ctx_name);
  if (ctx == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL, "%s: unknown context.",
          ctx_name);

  printf("\nSynopsis:\n");
  list = ctx->opt_list;
  print_options(list, &has_optional, &has_ellipsis, &has_rule, &has_generic_arg,
                &has_ctx_change, &has_early_eval);

  printf("\nExplanations:\n");
  if (has_early_eval)
    printf("*            : the parameters for this option will be "
           "evaluated first.\n");
  if (has_ctx_change)
    printf(">            : given after the parameters, sets the next "
           "context to go.\n");
  if (has_generic_arg)
    printf("#            : generic name for an argument.\n");
  if (has_optional)
    printf("[...]        : the object is square brackets is optional.\n");
  if (has_ellipsis)
    printf("...          : the previous object can be repeated more "
           "than one times.\n");
  if (has_rule)
    printf("[<|=|>]number: rules constraining the number of "
           "parameters/arguments.\n");

  if (action == exit_after)
    exit(EXIT_FAILURE);
}

/* ==================================================== */
/* Display a full usage screen about all contexts.      */
/* IN: what to do after (continue or exit the program)  */
/*     possible values: continue_after, exit_after.     */
/* ==================================================== */
void
ctxopt_disp_usage(usage_behaviour action)
{
  ll_t * list;
  int    has_optional    = 0;
  int    has_ellipsis    = 0;
  int    has_rule        = 0;
  int    has_generic_arg = 0;
  int    has_ctx_change  = 0;
  int    has_early_eval  = 0;

  if (main_ctx == NULL)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "At least one context must have been created.");

  /* Usage for the first context */
  /* """"""""""""""""""""""""""" */
  printf("\nAllowed options in the default context:\n");
  list = main_ctx->opt_list;
  print_options(list, &has_optional, &has_ellipsis, &has_rule, &has_generic_arg,
                &has_ctx_change, &has_early_eval);

  /* Usage for the other contexts */
  /* """""""""""""""""""""""""""" */
  bst_walk(contexts_bst, bst_print_ctx_cb);

  /* Contextual syntactic explanations */
  /* """"""""""""""""""""""""""""""""" */
  printf("\nSyntactic explanations:\n");

  if (has_early_eval)
    printf("*            : the parameters for this option will be "
           "evaluated first.\n");

  if (has_ctx_change)
    printf(">            : given after the parameters, sets the next "
           "context to go.\n");

  if (has_generic_arg)
    printf("#            : generic name for an argument.\n");

  if (has_optional)
    printf("[...]        : the object is square brackets is optional.\n");

  if (has_ellipsis)
    printf("...          : the previous object can be repeated more "
           "than one times.\n");

  if (has_rule)
    printf("[<|=|>]number: rules constraining the number of "
           "parameters/arguments.\n");

  if (action == exit_after)
    exit(EXIT_FAILURE);
}

/* ************************************** */
/* Built-in constraint checking functions */
/* ************************************** */

/* ================================================================ */
/* This constraint checks if each arguments respects a format as    */
/* defined for the scanf function.                                  */
/* return 1 if yes and 0 if no                                      */
/* ================================================================ */
int
ctxopt_format_constraint(int nb_args, char ** args, char * value)
{
  int rc = 0;

  char   x[256];
  char   y;
  char * format;

  if (nb_args != 1)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Format constraint: invalid number of parameters.");

  if (strlen(value) > 255)
    value[255] = '\0';

  format = concat(args[0], "%c", NULL);

  rc = sscanf(value, format, x, &y);
  if (rc != 1)
    error("The argument %s does not respect the imposed format: %s", value,
          args[0]);

  free(format);

  return rc == 1;
}

/* ================================================================== */
/* This constraint checks if each arguments of the option instance is */
/* between a minimum and a maximum (inclusive).                       */
/* return 1 if yes and 0 if no                                        */
/* ================================================================== */
int
ctxopt_re_constraint(int nb_args, char ** args, char * value)
{
  regex_t re;

  if (nb_args != 1)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Regular expression constraint: invalid number of parameters.");

  if (regcomp(&re, args[0], REG_EXTENDED) != 0)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Invalid regular expression %s", args[0]);

  if (regexec(&re, value, (size_t)0, NULL, 0) != 0)
  {
    error("The argument %s doesn't match the constraining "
          "regular expression %s",
          value, args[0]);
    return 0;
  }

  regfree(&re);

  return 1;
}

/* ================================================================== */
/* This constraint checks if each arguments of the option instance is */
/* between a minimum and a maximum (inclusive).                       */
/* return 1 if yes and 0 if no                                        */
/* ================================================================== */
int
ctxopt_range_constraint(int nb_args, char ** args, char * value)
{
  long   min, max;
  char   c;
  char * ptr;
  int    n;
  long   v;
  int    min_only = 0;
  int    max_only = 0;

  if (nb_args != 2)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Range constraint: invalid number of parameters.");

  if (strcmp(args[0], "-") == 0)
    max_only = 1;
  else
    n = sscanf(args[0], "%ld%c", &min, &c);

  if (!max_only && n != 1)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Range constraint: min: invalid parameters.");

  if (strcmp(args[1], "-") == 0)
    min_only = 1;
  else
    n = sscanf(args[1], "%ld%c", &max, &c);

  if (!min_only && n != 1)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Range constraint: max: invalid parameters.");

  if (min_only && max_only)
    fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
          "Range constraint: invalid parameters.");

  v = strtol(value, &ptr, 10);
  if (ptr == value)
    return 0;

  if (min_only)
  {
    if (v < min)
    {
      error("%ld is not greater or equal to %ld as requested.", v, min);
      return 0;
    }
    else
      return 1;
  }
  else if (max_only)
  {
    if (v > max)
    {
      error("%ld is not lower or equal to %ld as requested.", v, max);
      return 0;
    }
    else
      return 1;
  }
  else if (v < min || v > max)
  {
    error("%ld is not between %ld and %ld as requested.", v, min, max);
    return 0;
  }

  return 1; /* check passed */
}

/* ============================================================== */
/* This function provides a way to set the behaviour of a context */
/* ============================================================== */
void
ctxopt_add_global_settings(settings s, ...)
{
  va_list(args);
  va_start(args, s);

  switch (s)
  {
    case error_functions:
    {
      void (*function)(char *, char *, char *, char *, char *);

      errors e;
      e        = va_arg(args, errors);
      function = va_arg(args, void (*)(char *, char *, char *, char *, char *));
      err_functions[e] = function;
      break;
    }

    default:
      break;
  }
  va_end(args);
}

/* ================================================================ */
/* This function provides a way to set the behaviour of an option   */
/* It can take a variable number of arguments according to its      */
/* first  argument:                                                 */
/* - parameter:                                                     */
/*   o a string containing an option name and all its possible      */
/*     parameters separates by spaces, tabs or commas (char *)      */
/*     (e.g: "help -h -help")                                       */
/* - actions:                                                       */
/*   o a string containing an option name                           */
/*   o a pointer to a function which will be called at evaluation   */
/*     time.                                                        */
/* - constraints:                                                   */
/*   o a string containing an option name                           */
/*   o a pointer to a function to check if an argument is valid.    */
/*   o a strings containing the arguments to this function.         */
/* ================================================================ */
void
ctxopt_add_opt_settings(settings s, ...)
{
  opt_t * opt;
  void *  ptr = NULL;

  va_list(args);
  va_start(args, s);

  switch (s)
  {
    /* This part associates some command line parameters to an option */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    case parameters:
    {
      char * opt_name;
      char * params;

      /* The second argument must be a string containing: */
      /* - The name of an existing option                 */
      /* - a list of parameters with a leading dash (-)   */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      ptr      = va_arg(args, char *);
      opt_name = ptr;

      if (opt_name != NULL)
      {
        if ((opt = locate_opt(opt_name)) != NULL)
        {
          ptr    = va_arg(args, char *);
          params = ptr;
          ltrim(params, " \t");
          rtrim(params, " \t", 0);

          if (!opt_set_parms(opt_name, params))
            fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                  "duplicates parameters or bad settings for the option (%s).",
                  params);
        }
        else
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "%s: unknown option.", opt_name);
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "ctxopt_opt_add_settings: parameters: not enough arguments.");

      if (opt->params != NULL)
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "Parameters are already set for %s", opt_name);
      else
      {
        size_t n;
        size_t l = strlen(params);

        opt->params = xstrdup(params);
        while ((n = strcspn(opt->params, " \t")) < l)
          opt->params[n] = '|';
      }

      break;
    }

    /* This part associates a callback function to an option         */
    /* This function will be called as when an instance of an option */
    /* is evaluated.                                                 */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    case actions:
    {
      void * data;
      void (*function)();
      int nb_data = 0;

      /* The second argument must be the name of an existing option */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      ptr = va_arg(args, char *);

      if ((opt = locate_opt(ptr)) != NULL)
      {
        /* The third argument must be the callback function */
        /* """""""""""""""""""""""""""""""""""""""""""""""" */
        function = va_arg(args, void (*)(char *, char *, char *, int, char **,
                                         int, void *, int, void **));
        opt->action = function;

        /* The fourth argument must be a pointer to an user's defined  */
        /* variable or structure that the previous function can manage */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        while ((data = va_arg(args, void *)) != NULL)
        {
          nb_data++;
          opt->data = xrealloc(opt->data, nb_data * sizeof(void *));
          opt->data[nb_data - 1] = data;
        }
        opt->nb_data = nb_data;
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "%s: unknown option.", ptr);
      break;
    }

    /* This part associates a list of functions to control some */
    /* characteristics of the arguments of an option.           */
    /* Each function will be called in order and must return 1  */
    /* to validate the arguments.                               */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    case constraints:
    {
      char *         value;
      constraint_t * cstr;
      int (*function)();

      /* The second argument must be a string */
      /* """""""""""""""""""""""""""""""""""" */
      ptr = va_arg(args, char *);

      if ((opt = locate_opt(ptr)) != NULL)
      {
        /* The third argument must be a function */
        /* """"""""""""""""""""""""""""""""""""" */
        function = va_arg(args, int (*)(int, char **, char *));

        cstr             = xmalloc(sizeof(constraint_t));
        cstr->constraint = function;

        /* The fourth argument must be a string containing the argument of */
        /* The previous function separated by spaces or tabs               */
        /* Theses arguments will be passed to the previous function        */
        /* max: 32 argument!                                               */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        value = xstrdup(va_arg(args, char *));

        cstr->args    = xcalloc(sizeof(char *), 32);
        cstr->nb_args = str2argv(value, cstr->args, 32);
        ll_append(opt->constraints_list, cstr);
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "%s: unknown option.", ptr);
      break;
    }

    default:
      break;
  }
  va_end(args);
}

/* ============================================================== */
/* This function provides a way to set the behaviour of a context */
/* ============================================================== */
void
ctxopt_add_ctx_settings(settings s, ...)
{
  ctx_t * ctx;

  va_list(args);
  va_start(args, s);

  switch (s)
  {
    case incompatibilities:
    {
      void * ptr;
      ll_t * list;
      size_t n;
      char * str;

      ptr = va_arg(args, char *);
      if ((ctx = locate_ctx(ptr)) != NULL)
      {
        ptr  = va_arg(args, char *);
        list = ctx->incomp_list;

        str = xstrdup(ptr);
        ltrim(str, " \t");
        rtrim(str, " \t", 0);

        n = strcspn(str, " \t");
        if (n > 0 && n < strlen(str))
          ll_append(list, str);
        else
          fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
                "Not enough incompatibles options in the string: \"%s\"", str);
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "%s: unknown context.", ptr);
      break;
    }
    case actions:
    {
      void * ptr;
      void * data;
      int (*function)();
      int nb_data = 0;

      ptr = va_arg(args, char *);
      if ((ctx = locate_ctx(ptr)) != NULL)
      {
        function = va_arg(args, int (*)(char *, direction, char *, char *, int,
                                        void **));
        ctx->action = function;

        while ((data = va_arg(args, void *)) != NULL)
        {
          nb_data++;
          ctx->data = xrealloc(ctx->data, nb_data * sizeof(void *));
          ctx->data[nb_data - 1] = data;
        }
        ctx->nb_data = nb_data;
      }
      else
        fatal(CTXOPTINTERNAL, NULL, NULL, NULL, NULL, NULL,
              "%s: unknown context.", ptr);
      break;
    }

    default:
      break;
  }
}
