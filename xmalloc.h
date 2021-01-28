#ifndef XMALLOC_H
#define XMALLOC_H

void *
rpl_malloc(size_t size);

void *
rpl_realloc(void * ptr, size_t size);

void *
xmalloc(size_t size);

void *
xcalloc(size_t num, size_t size);

void *
xrealloc(void * ptr, size_t size);

char *
xstrdup(const char * p);

char *
xstrndup(const char * str, size_t len);

#endif
