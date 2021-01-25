/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html.           */
/* ########################################################### */

/* ************************************* */
/* Various UTF-8 manipulation functions. */
/* ************************************* */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <langinfo.h>
#include "xmalloc.h"
#include "utf8.h"

/* =========================================================== */
/* UTF-8 byte sequence generation from a given UCS-4 codepoint */
/* utf8_str must be preallocated with a size of at least 5     */
/* bytes.                                                      */
/* return the length of the generated sequence or 0 if c is    */
/* not a valid codepoint.                                      */
/* =========================================================== */
int
cptoutf8(char * utf8_str, uint32_t c)
{
  int len   = 0;
  int first = 0;
  int i;

  if (c < 0x80)
  {
    first = 0;
    len   = 1;
  }
  else if (c < 0x800)
  {
    first = 0xc0;
    len   = 2;
  }
  else if (c < 0x10000)
  {
    first = 0xe0;
    len   = 3;
  }
  else if (c < 0x200000)
  {
    first = 0xf0;
    len   = 4;
  }

  if (utf8_str)
  {
    for (i = len - 1; i > 0; --i)
    {
      utf8_str[i] = (c & 0x3f) | 0x80;
      c >>= 6;
    }
    utf8_str[0] = c | first;
  }

  return len;
}

/* ======================================================================= */
/* Unicode (UTF-8) ASCII representation interpreter.                       */
/* The string passed will be altered but its address will not change.      */
/* All hexadecimal sequences of \uxx, \uxxxx, \uxxxxxx and \uxxxxxxxx will */
/* be replaced by the corresponding UTF-8 character when possible.         */
/* All hexadecimal sequences of \Uxxxxxx will be replaced with the UTF-8   */
/* sequence corresponding to the given UCS-4 codepoint.                    */
/* When not possible the substitution character is substituted in place.   */
/* Returns 0 if the conversion has failed else 1.                          */
/* ======================================================================= */
int
utf8_interpret(char * s, langinfo_t * langinfo, char substitute)
{
  char * utf8_str;          /* \uxx...                                     */
  size_t utf8_to_eos_len;   /* bytes in s starting from the first          *
                             * occurrence of \u.                           */
  size_t init_len;          /* initial lengths of the string to interpret  */
  size_t utf8_ascii_len;    /* 2,4,6 or 8 bytes.                           */
  size_t len_to_remove = 0; /* number of bytes to remove after the         *
                             | conversion.                                 */
  char   tmp[9];            /* temporary string.                           */
  int    rc = 1;            /* return code, 0: error, 1: fine.             */

  /* Guard against the case where s is NULL. */
  /* """"""""""""""""""""""""""""""""""""""" */
  if (s == NULL)
    return 0;

  init_len = strlen(s);

  /* Manage \U codepoints. */
  /* """"""""""""""""""""" */
  while ((utf8_str = strstr(s, "\\U")) != NULL)
  {
    char     str[7];
    int      utf8_str_len;
    int      len;
    int      n;
    uint32_t cp;
    int      subst; /* 0, the \U sequence is valid, else 1. */

    utf8_to_eos_len = strlen(utf8_str);
    utf8_str_len    = 0;

    n = sscanf(utf8_str + 2,
               "%6["
               "0123456789"
               "abcdef"
               "ABCDEF"
               "]%n",
               tmp, &utf8_str_len);

    subst = 0;

    if (n == 1 && utf8_str_len == 6)
    {
      sscanf(tmp, "%x", &cp);
      if (cp > 0x10FFFF)
        subst = 1; /* Invalid range. */
      else
      {
        len             = cptoutf8(str, cp);
        str[len]        = '\0';
        *(utf8_str + 1) = 'u';
        memmove(utf8_str, str, len);
        memmove(utf8_str + len, utf8_str + 8, utf8_to_eos_len - 8);
        len_to_remove += 8 - len;
      }
    }
    else
      subst = 1; /* Invalid sequence. */

    /* In case of invalid \U sequence, replace it with the */
    /* substitution character.                             */
    /* ''''''''''''''''''''''''''''''''''''''''''''''''''' */
    if (subst)
    {
      *utf8_str = substitute;
      memmove(utf8_str + 1, utf8_str + 2 + utf8_str_len,
              utf8_to_eos_len - (utf8_str_len + 2 - 1));
      len_to_remove += utf8_str_len + 2 - 1;
    }
  }

  /* Make sure that the string is well terminated. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  *(s + init_len - len_to_remove) = '\0';

  /* Manage \u UTF-8 byte sequences. */
  /* """"""""""""""""""""""""""""""" */
  while ((utf8_str = strstr(s, "\\u")) != NULL)
  {
    utf8_to_eos_len = strlen(utf8_str);
    if (utf8_to_eos_len < 4) /* string too short to contain *
                              | a valid UTF-8 char.         */
    {
      *utf8_str       = substitute;
      *(utf8_str + 1) = '\0';
      rc              = 0;
    }
    else /* s is long enough. */
    {
      unsigned byte;
      char *   utf8_seq_offset = utf8_str + 2;

      /* Get the first 2 UTF-8 bytes. */
      /* """""""""""""""""""""""""""" */
      *tmp       = *utf8_seq_offset;
      *(tmp + 1) = *(utf8_seq_offset + 1);
      *(tmp + 2) = '\0';

      /* If they are invalid, replace the \u sequence by the */
      /* substitute character.                               */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
      if (!isxdigit(tmp[0]) || !isxdigit(tmp[1]))
      {
        *utf8_str = substitute;
        if (4 >= utf8_to_eos_len)
          *(utf8_str + 1) = '\0';
        else
        {
          /* Do not forget the training \0. */
          /* """""""""""""""""""""""""""""" */
          memmove(utf8_str + 1, utf8_str + 4, utf8_to_eos_len - 4 + 1);
        }
        rc = 0;
      }
      else
      {
        int    n;
        char   end;
        size_t i;
        char   b[3] = { ' ', ' ', '\0' };

        /* They are valid, deduce from them the length of the sequence. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        sscanf(tmp, "%2x", &byte);

        utf8_ascii_len = utf8_get_length(byte) * 2;

        /* replace the \u sequence by the bytes forming the UTF-8 char. */
        /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */

        /* Put the bytes in the tmp string. */
        /* '''''''''''''''''''''''''''''''' */
        *tmp = byte; /* Reuse the tmp array. */

        for (i = 1; i < utf8_ascii_len / 2; i++)
        {
          int good = 1;

          n = sscanf(utf8_seq_offset + 2 * i, "%c%c", &b[0], &b[1]);

          if (n == 2)
          {
            byte = 0;
            end  = '\0';
            sscanf(b, "%x%c", &byte, &end);

            if (byte == 0 || end != '\0' || (byte & 0xc0) != 0x80)
              good = 0;
          }
          else
            good = 0;

          if (!good)
            utf8_ascii_len = 2 * i; /* Force the new length according to the *
                                     | number of valid UTF-8 bytes read.     */
          else
            *(tmp + i) = byte;
        }
        tmp[utf8_ascii_len / 2] = '\0';

        /* Does they form a valid UTF-8 char? */
        /* '''''''''''''''''''''''''''''''''' */
        if (utf8_validate(tmp, utf8_ascii_len / 2))
        {
          /* Put them back in the original string and move */
          /* the remaining bytes after them.               */
          /* ''''''''''''''''''''''''''''''''''''''''''''' */
          memmove(utf8_str, tmp, utf8_ascii_len / 2);

          if (utf8_to_eos_len < utf8_ascii_len)
            *(utf8_str + utf8_ascii_len / 2 + 1) = '\0';
          else
            memmove(utf8_str + utf8_ascii_len / 2,
                    utf8_seq_offset + utf8_ascii_len,
                    utf8_to_eos_len - utf8_ascii_len - 2 + 1);
        }
        else
        {
          /* The invalid sequence is replaced by a */
          /* substitution character.               */
          /* ''''''''''''''''''''''''''''''''''''' */
          *utf8_str = substitute;

          if (utf8_to_eos_len < utf8_ascii_len)
            *(utf8_str + 1) = '\0';
          else
            memmove(utf8_str + 1, utf8_seq_offset + utf8_ascii_len,
                    utf8_to_eos_len - utf8_ascii_len - 2 + 1);

          utf8_ascii_len = 2;
          rc             = 0;
        }

        /* Update the number of bytes to remove at the end */
        /* of the initial string.                          */
        /* """"""""""""""""""""""""""""""""""""""""""""""" */
        len_to_remove += 2 + utf8_ascii_len / 2;
      }
    }
  }

  /* Make sure that the string is well terminated. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  *(s + init_len - len_to_remove) = '\0';

  return rc;
}

/* ========================================================= */
/* Decodes the number of bytes taken by a UTF-8 glyph.       */
/* It is the length of the leading sequence of bits set to 1 */
/* in the first byte.                                        */
/* ========================================================= */
int
utf8_get_length(unsigned char c)
{
  if (c < 0x80)
    return 1;
  else if (c < 0xe0)
    return 2;
  else if (c < 0xf0)
    return 3;
  else
    return 4;
}

/* ==================================================== */
/* Returns the byte offset of the nth UTF-8 glyph in s. */
/* ==================================================== */
size_t
utf8_offset(char * s, size_t n)
{
  size_t i = 0;

  while (n > 0)
  {
    if (s[i++] & 0x80)
    {
      (void)(((s[++i] & 0xc0) != 0x80) || ((s[++i] & 0xc0) != 0x80) || ++i);
    }
    n--;
  }
  return i;
}

/* ============================================== */
/* Points to the previous UTF-8 glyph in a string */
/* from the given position.                       */
/* ============================================== */
char *
utf8_prev(const char * str, const char * p)
{
  while ((*p & 0xc0) == 0x80)
    p--;

  for (--p; p >= str; --p)
  {
    if ((*p & 0xc0) != 0x80)
      return (char *)p;
  }
  return NULL;
}

/* ========================================== */
/* Points to the next UTF-8 glyph in a string */
/* from the current position.                 */
/* ========================================== */
char *
utf8_next(char * p)
{
  if (*p)
  {
    for (++p; (*p & 0xc0) == 0x80; ++p)
      ;
  }
  return (*p == '\0' ? NULL : p);
}

/* ============================================================= */
/* Replaces any UTF-8 glyph present in s by a substitution       */
/* character in-place.                                           */
/* s will be modified but its address in memory will not change. */
/* ============================================================= */
void
utf8_sanitize(char * s, char substitute)
{
  char * p = s;
  int    n;
  size_t len;

  len = strlen(s);
  while (*p)
  {
    n = utf8_get_length(*p);
    if (n > 1)
    {
      *p = substitute;
      memmove(p + 1, p + n, len - (p - s) - n + 1);
      len -= (n - 1);
    }
    p++;
  }
}

static const char trailing_bytes_for_utf8[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

/* =================================================================== */
/* UTF-8 validation routine inspired by Jeff Bezanson                  */
/*   placed in the public domain Fall 2005                             */
/*   (https://github.com/JeffBezanson/cutef8).                         */
/*                                                                     */
/* Returns 1 if str contains a valid UTF-8 byte sequence, 0 otherwise. */
/* =================================================================== */
int
utf8_validate(const char * str, size_t length)
{
  const unsigned char *p, *pend = (const unsigned char *)str + length;
  unsigned char        c;
  size_t               ab;

  for (p = (const unsigned char *)str; p < pend; p++)
  {
    c = *p;
    if (c < 128)
      continue;
    if ((c & 0xc0) != 0xc0)
      return 0;
    ab = trailing_bytes_for_utf8[c];
    if (length < ab)
      return 0;
    length -= ab;

    p++;
    /* Check top bits in the second byte. */
    /* """""""""""""""""""""""""""""""""" */
    if ((*p & 0xc0) != 0x80)
      return 0;

    /* Check for overlong sequences for each different length. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
    switch (ab)
    {
      /* Check for xx00 000x. */
      /* """""""""""""""""""" */
      case 1:
        if ((c & 0x3e) == 0)
          return 0;
        continue; /* We know there aren't any more bytes to check. */

      /* Check for 1110 0000, xx0x xxxx. */
      /* """"""""""""""""""""""""""""""" */
      case 2:
        if (c == 0xe0 && (*p & 0x20) == 0)
          return 0;
        break;

      /* Check for 1111 0000, xx00 xxxx. */
      /* """"""""""""""""""""""""""""""" */
      case 3:
        if (c == 0xf0 && (*p & 0x30) == 0)
          return 0;
        break;

      /* Check for 1111 1000, xx00 0xxx. */
      /* """"""""""""""""""""""""""""""" */
      case 4:
        if (c == 0xf8 && (*p & 0x38) == 0)
          return 0;
        break;

      /* Check for leading 0xfe or 0xff,    */
      /* and then for 1111 1100, xx00 00xx. */
      /* """""""""""""""""""""""""""""""""" */
      case 5:
        if (c == 0xfe || c == 0xff || (c == 0xfc && (*p & 0x3c) == 0))
          return 0;
        break;
    }

    /* Check for valid bytes after the 2nd, if any; all must start with 10. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
    while (--ab > 0)
    {
      if ((*(++p) & 0xc0) != 0x80)
        return 0;
    }
  }

  return 1;
}

/* ======================= */
/* Multibyte UTF-8 strlen. */
/* ======================= */
size_t
utf8_strlen(char * str)
{
  size_t i = 0, j = 0;

  while (str[i])
  {
    if ((str[i] & 0xc0) != 0x80)
      j++;
    i++;
  }
  return j;
}

/* =================================================================== */
/* Multibytes extraction of the prefix of n UTF-8 glyphs from a string */
/* The destination string d must have been allocated before.           */
/* pos is updated to reflect the position AFTER the prefix.            */
/* =================================================================== */
char *
utf8_strprefix(char * d, char * s, long n, long * pos)
{
  long i = 0;
  long j = 0;

  *pos = 0;

  while (s[i] && j < n)
  {
    d[i] = s[i];
    i++;
    j++;
    while (s[i] && (s[i] & 0xC0) == 0x80)
    {
      d[i] = s[i];
      i++;
    }
  }

  *pos = i;

  d[i] = '\0';

  return d;
}

/* ================================================== */
/* Converts a UTF-8 glyph string to a wchar_t string. */
/* The returned string must be freed by the caller.   */
/* ================================================== */
wchar_t *
utf8_strtowcs(char * s)
{
  int             converted = 0;
  unsigned char * ch;
  wchar_t *       wptr, *w;
  size_t          size;

  size = (long)strlen(s);
  w    = xmalloc((size + 1) * sizeof(wchar_t));
  w[0] = L'\0';

  wptr = w;
  for (ch = (unsigned char *)s; *ch; ch += converted)
  {
    if ((converted = mbtowc(wptr, (char *)ch, 4)) > 0)
      wptr++;
    else
    {
      *wptr++   = (wchar_t)*ch;
      converted = 1;
    }
  }

  *wptr = L'\0';

  return w;
}

/* ============================================================== */
/* Replaces all ASCII characters in src by its lowercase version. */
/* dst must be preallocated before the call.                      */
/* ============================================================== */
void
utf8_strtolower(char * dst, char * src)
{
  unsigned char c;

  while ((c = *src))
  {
    if (c >= 0x80)
      *dst = c;
    else
      *dst = tolower(c);

    src++;
    dst++;
  }

  *dst = '\0';
}
