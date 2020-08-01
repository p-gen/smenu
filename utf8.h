/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

typedef struct langinfo_s langinfo_t;

/* Locale informations */
/* """"""""""""""""""" */
struct langinfo_s
{
  int utf8; /* charset is UTF-8              */
  int bits; /* number of bits in the charset */
};

int
utf8_get_length(unsigned char c);

size_t
utf8_offset(char *, size_t);

char *
utf8_strprefix(char * d, char * s, long n, long * pos);

size_t
utf8_strlen(char * str);

wchar_t *
utf8_strtowcs(char * s);

void
utf8_sanitize(char * s, char sc);

int
cptoutf8(char * utf8_str, uint32_t c);

int
utf8_interpret(char * s, langinfo_t * langinfo, char sc);

int
utf8_validate(const char * str, size_t length);

char *
utf8_prev(const char * str, const char * p);

char *
utf8_next(char * p);

void
utf8_strtolower(char * dst, char * src);

#endif
