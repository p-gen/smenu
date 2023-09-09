/* ################################################################### */
/* Copyright 2015, Pierre Gentile (p.gen.progs@gmail.com)              */
/*                                                                     */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

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
cptoutf8(char *utf8_str, uint32_t c)
{
  int len = 0;

  if (c < 0x80)
  {
    utf8_str[0] = c;
    len         = 1;
  }
  else if (c < 0x800)
  {
    utf8_str[0] = 0xC0 | ((c >> 6) & 0x1F);
    utf8_str[1] = 0x80 | (c & 0x3F);
    len         = 2;
  }
  else if (c < 0x10000)
  {
    utf8_str[0] = 0xE0 | ((c >> 12) & 0x0F);
    utf8_str[1] = 0x80 | ((c >> 6) & 0x3F);
    utf8_str[2] = 0x80 | (c & 0x3F);
    len         = 3;
  }
  else if (c < 0x110000)
  {
    utf8_str[0] = 0xF0 | ((c >> 18) & 0x07);
    utf8_str[1] = 0x80 | ((c >> 12) & 0x3F);
    utf8_str[2] = 0x80 | ((c >> 6) & 0x3F);
    utf8_str[3] = 0x80 | (c & 0x3F);
    len         = 4;
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
utf8_interpret(char *s, char substitute)
{
  char  *utf8_str;          /* \uxx...                                     */
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
  while ((utf8_str = strstr(s,
                            "\\"
                            "U"))
         != NULL)
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
               tmp,
               &utf8_str_len);

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
      memmove(utf8_str + 1,
              utf8_str + 2 + utf8_str_len,
              utf8_to_eos_len - (utf8_str_len + 2 - 1));
      len_to_remove += utf8_str_len + 2 - 1;
    }
  }

  /* Make sure that the string is well terminated. */
  /* """"""""""""""""""""""""""""""""""""""""""""" */
  *(s + init_len - len_to_remove) = '\0';

  /* Manage \u UTF-8 byte sequences. */
  /* """"""""""""""""""""""""""""""" */
  while ((utf8_str = strstr(s,
                            "\\"
                            "u"))
         != NULL)
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
      char    *utf8_seq_offset = utf8_str + 2;

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

          if (good)
            *(tmp + i) = byte;
          else
            utf8_ascii_len = 2 * i; /* Force the new length according to the *
                                     | number of valid UTF-8 bytes read.     */
        }
        tmp[utf8_ascii_len / 2] = '\0';

        /* Does they form a valid UTF-8 char? */
        /* '''''''''''''''''''''''''''''''''' */
        if (utf8_validate(tmp) == NULL)
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
            memmove(utf8_str + 1,
                    utf8_seq_offset + utf8_ascii_len,
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

  return 4;
}

/* ==================================================== */
/* Returns the byte offset of the nth UTF-8 glyph in s. */
/* ==================================================== */
size_t
utf8_offset(char *s, size_t n)
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
utf8_prev(const char *str, const char *p)
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
utf8_next(char *p)
{
  if (*p)
  {
    for (++p; (*p & 0xc0) == 0x80; ++p)
      ;
  }

  return *p == '\0' ? NULL : p;
}

/* ============================================================= */
/* Replaces any UTF-8 glyph present in s by a substitution       */
/* character in-place.                                           */
/* s will be modified but its address in memory will not change. */
/* ============================================================= */
void
utf8_sanitize(char *s, char substitute)
{
  char  *p = s;
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

/* ======================================================================= */
/* This function scans the '\0'-terminated string starting at s.           */
/* It returns a pointer to the first byte of the first malformed           */
/* or overlong UTF-8 sequence found, or NULL if the string contains only   */
/* correct UTF-8.                                                          */
/* It also spots UTF-8 sequences that could cause trouble if converted to  */
/* UTF-16, namely surrogate characters (U+D800..U+DFFF) and non-Unicode    */
/* positions (U+FFFE..U+FFFF).                                             */
/* This routine is very likely to find a malformed sequence if the input   */
/* uses any other encoding than UTF-8.                                     */
/* It therefore can be used as a very effective heuristic for              */
/* distinguishing between UTF-8 and other encodings.                       */
/*                                                                         */
/* I wrote this code mainly as a specification of functionality; there     */
/* are no doubt performance optimizations possible for certain CPUs.       */
/*                                                                         */
/* Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> -- 2005-03-30             */
/* License: http://www.cl.cam.ac.uk/~mgk25/short-license.html              */
/* ======================================================================= */
char *
utf8_validate(char *s)
{
  unsigned char *us = (unsigned char *)s;

  /* clang-format off */
  while (*us)
  {
    if (*us < 0x80)
      /* 0xxxxxxx */
      us++;
    else if ((us[0] & 0xe0) == 0xc0)
    {
      /* 110XXXXx 10xxxxxx */
      if ((us[1] & 0xc0) != 0x80 || (us[0] & 0xfe) == 0xc0) /* overlong? */
        return (char *)us;

      us += 2;
    }
    else if ((us[0] & 0xf0) == 0xe0)
    {
      /* 1110XXXX 10Xxxxxx 10xxxxxx */
      if ((us[1] & 0xc0) != 0x80 ||
          (us[2] & 0xc0) != 0x80 ||
          (us[0] == 0xe0 && (us[1] & 0xe0) == 0x80) || /* overlong?         */
          (us[0] == 0xed && (us[1] & 0xe0) == 0xa0) || /* surrogate?        */
          (us[0] == 0xef && us[1] == 0xbf &&
            (us[2] & 0xfe) == 0xbe))                   /* U+FFFE or U+FFFF? */
        return (char *)us;

      us += 3;
    }
    else if ((us[0] & 0xf8) == 0xf0)
    {
      /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
      if ((us[1] & 0xc0) != 0x80 ||
          (us[2] & 0xc0) != 0x80 ||
          (us[3] & 0xc0) != 0x80 ||
          (us[0] == 0xf0 && (us[1] & 0xf0) == 0x80) ||     /* overlong?   */
          (us[0] == 0xf4 && us[1] > 0x8f) || us[0] > 0xf4) /* > U+10FFFF? */
        return (char *)us;

      us += 4;
    }
    else
      return (char *)us;
  }
  /* clang-format on */

  return NULL;
}

/* ======================= */
/* Multibyte UTF-8 strlen. */
/* ======================= */
size_t
utf8_strlen(char *str)
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

/* ==================================================================== */
/* Multibytes extraction of the prefix of n UTF-8 glyphs from a string. */
/* The destination string d must have been allocated before.            */
/* pos is updated to reflect the position AFTER the prefix.             */
/* ==================================================================== */
char *
utf8_strprefix(char *d, char *s, long n, long *pos)
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
utf8_strtowcs(char *s)
{
  int            converted = 0;
  unsigned char *ch;
  wchar_t       *wptr, *w;
  size_t         size;

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
/* Poor man UTF-8 aware strtolower version.                       */
/* Replaces all ASCII characters in src by its lowercase version. */
/* dst must be preallocated before the call.                      */
/* ============================================================== */
void
utf8_strtolower(char *dst, char *src)
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
