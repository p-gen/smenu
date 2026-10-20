#ifndef BUF_H
#define BUF_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Hidden metadata and alignment for ordinary C99 object types. */
/* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
union buf_hdr
{
  struct
  {
    size_t len, cap;
  } x;

  long double ld;
  void       *p;
  void (*fp)(void);
};

/* Recover metadata and query a possibly null buffer. */
/* """""""""""""""""""""""""""""""""""""""""""""""""" */
#define BUF_HDR(b) ((union buf_hdr *)(void *)(b) - 1)
#define BUF_LEN(b) ((b) ? BUF_HDR(b)->x.len : 0)
#define BUF_CAP(b) ((b) ? BUF_HDR(b)->x.cap : 0)
#define BUF_END(b) ((b) ? (b) + BUF_LEN(b) : NULL)

/* Customizable fatal allocation interface. */
/* """""""""""""""""""""""""""""""""""""""" */
#ifndef BUF_MALLOC
#define BUF_MALLOC malloc
#endif

#ifndef BUF_REALLOC
#define BUF_REALLOC realloc
#endif

#ifndef BUF_FREE_MEM
#define BUF_FREE_MEM free
#endif

#ifndef BUF_DIE
#define BUF_DIE() abort()
#endif

/* =========================================================== */
/* Add sizes, terminating instead of allowing size_t overflow. */
/* =========================================================== */
static size_t
buf_sum(size_t a, size_t b)
{
  if (b > SIZE_MAX - a)
    BUF_DIE();
  return a + b;
}

/* ======================================= */
/* Validate and return an insertion index. */
/* ======================================= */
static size_t
buf_index(size_t i, size_t len)
{
  if (i > len)
    BUF_DIE();
  return i;
}

/* ==================================================== */
/* Allocate or enlarge storage for at least n elements. */
/* ==================================================== */
static void *
buf_grow(void *b, size_t n, size_t es)
{
  union buf_hdr *h;
  size_t         cap = b ? BUF_HDR(b)->x.cap : 0;

  if (n > (SIZE_MAX - sizeof *h) / es)
    BUF_DIE();

  cap = cap > SIZE_MAX / 2 ? n : cap * 2;
  if (cap < n)
    cap = n;
  if (cap < 16)
    cap = 16;
  if (cap > (SIZE_MAX - sizeof *h) / es)
    cap = n;

  h = b ? BUF_REALLOC(BUF_HDR(b), sizeof *h + cap * es)
        : BUF_MALLOC(sizeof *h + cap * es);
  if (!b)
    h->x.len = 0;
  h->x.cap = cap;
  return h + 1;
}

/* ========================================================== */
/* Extend the logical length and return the former end index. */
/* ========================================================== */
static size_t
buf_add(void *b, size_t n)
{
  union buf_hdr *h = BUF_HDR(b);
  size_t         i = h->x.len;

  h->x.len += n;
  return i;
}

/* ============================================================ */
/* Move a suffix right, extend the length and return gap index. */
/* ============================================================ */
static size_t
buf_gap(void *b, size_t i, size_t n, size_t es)
{
  union buf_hdr *h = BUF_HDR(b);

  memmove((char *)b + (i + n) * es, (char *)b + i * es, (h->x.len - i) * es);
  h->x.len += n;
  return i;
}

/* Internal typed growth expression. */
/* """"""""""""""""""""""""""""""""" */
#define BUF_FIT_EXPR(b, n)   \
  ((size_t)(n) <= BUF_CAP(b) \
     ? (b)                   \
     : ((b) = buf_grow((b), (size_t)(n), sizeof *(b))))

/* Public buffer operations. */
/* """"""""""""""""""""""""" */
#define BUF_FREE(b) ((b) ? (BUF_FREE_MEM(BUF_HDR(b)), (b) = NULL, 0) : 0)

#define BUF_FIT(b, n) ((void)BUF_FIT_EXPR((b), (size_t)(n)))

#define BUF_PUSH(b, v) \
  (BUF_FIT_EXPR((b), buf_sum(BUF_LEN(b), 1)), (b)[BUF_HDR(b)->x.len++] = (v))

#define BUF_POP(b) (BUF_LEN(b) ? (b)[--BUF_HDR(b)->x.len] : (BUF_DIE(), (b)[0]))

#define BUF_RESIZE(b, n) \
  (BUF_FIT_EXPR((b), (size_t)(n)), BUF_HDR(b)->x.len = (size_t)(n))

#define BUF_CLEAR(b) ((b) ? (BUF_HDR(b)->x.len = 0) : 0)

#define BUF_REMOVE(b, i)                                                      \
  ((size_t)(i) < BUF_LEN(b) ? (memmove((b) + (i),                             \
                                       (b) + (i) + 1,                         \
                                       (BUF_LEN(b) - (i) - 1) * sizeof *(b)), \
                               --BUF_HDR(b)->x.len)                           \
                            : (BUF_DIE(), (size_t)0))

#define BUF_SWAPREMOVE(b, i)                               \
  ((size_t)(i) < BUF_LEN(b)                                \
     ? ((b)[i] = (b)[BUF_LEN(b) - 1], --BUF_HDR(b)->x.len) \
     : (BUF_DIE(), (size_t)0))

#define BUF_MAKEGAP(b, i, n)                                                 \
  ((size_t)(n) ? (buf_index((size_t)(i), BUF_LEN(b)),                        \
                  BUF_FIT_EXPR((b), buf_sum(BUF_LEN(b), (size_t)(n))),       \
                  (b) + buf_gap((b), (size_t)(i), (size_t)(n), sizeof *(b))) \
               : ((b) ? (b) + buf_index((size_t)(i), BUF_LEN(b))             \
                      : (buf_index((size_t)(i), 0), (b))))

#define BUF_INSERT(b, i, v) (*BUF_MAKEGAP((b), (i), 1) = (v))

#define BUF_ADD(b, n)                                                  \
  ((size_t)(n) ? (BUF_FIT_EXPR((b), buf_sum(BUF_LEN(b), (size_t)(n))), \
                  (b) + buf_add((b), (size_t)(n)))                     \
               : BUF_END(b))

#define BUF_ADDZEROED(b, n)                                               \
  ((size_t)(n) ? (BUF_FIT_EXPR((b), buf_sum(BUF_LEN(b), (size_t)(n))),    \
                  memset((b) + BUF_LEN(b), 0, (size_t)(n) * sizeof *(b)), \
                  (b) + buf_add((b), (size_t)(n)))                        \
               : BUF_END(b))

#endif
