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

#include <ctype.h>   /* for isxdigit, tolower       */
#include <stdint.h>  /* for uint32_t                */
#include <stdio.h>   /* for sscanf                  */
#include <stdlib.h>  /* for mbtowc                  */
#include <string.h>  /* for memmove, strlen, strstr */
#include "xmalloc.h" /* for xmalloc                 */
#include "utils.h"
#include "utf8.h"
#include "iso8859_tables.h"

/* ============================================================ */
/* UTF-8 byte sequence generation from a given UCS-4 code point */
/* utf8_str must be preallocated with a size of at least 5      */
/* bytes.                                                       */
/* return the length of the generated sequence or 0 if c is     */
/* not a valid code point.                                      */
/* ============================================================ */
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
    utf8_str[0] = 0xc0 | ((c >> 6) & 0x1f);
    utf8_str[1] = 0x80 | (c & 0x3f);
    len         = 2;
  }
  else if (c >= 0xd800 && c <= 0xdfff)
  {
    utf8_str[0] = '\0';
    len         = 0;
  }
  else if (c < 0x10000)
  {
    utf8_str[0] = 0xe0 | ((c >> 12) & 0x0f);
    utf8_str[1] = 0x80 | ((c >> 6) & 0x3f);
    utf8_str[2] = 0x80 | (c & 0x3f);
    len         = 3;
  }
  else if (c < 0x110000)
  {
    utf8_str[0] = 0xf0 | ((c >> 18) & 0x07);
    utf8_str[1] = 0x80 | ((c >> 12) & 0x3f);
    utf8_str[2] = 0x80 | ((c >> 6) & 0x3f);
    utf8_str[3] = 0x80 | (c & 0x3f);
    len         = 4;
  }

  utf8_str[len] = '\0';

  return len;
}

/* ============================================================= */
/* Convert a UTF-8 byte sequence to a Unicode code point.        */
/* Returns (uint32_t)-1 for invalid leading bytes.               */
/* ============================================================= */
uint32_t
utf8tocp(char *s)
{
  const unsigned char *p = (const unsigned char *)s;

  if (p[0] < 0x80)
    return p[0];

  if (p[0] < 0xe0)
    return ((uint32_t)(p[0] & 0x1f) << 6) | (uint32_t)(p[1] & 0x3f);

  if (p[0] < 0xf0)
    return ((uint32_t)(p[0] & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6)
           | (uint32_t)(p[2] & 0x3f);

  if (p[0] < 0xf8)
    return ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3f) << 12)
           | ((uint32_t)(p[2] & 0x3f) << 6) | (uint32_t)(p[3] & 0x3f);

  return -1;
}

/* ======================================================================= */
/* Converts a UTF-8 character string to a  pointer to an array of int32_t. */
/* The array of int32_t string must be freed by the caller.                */
/* ======================================================================= */
uint32_t *
utf8stocps(char *s)
{
  int            converted = 0;
  unsigned char *ch;
  uint32_t      *iptr, *i;
  size_t         size;

  size = (long)strlen(s);
  i    = xmalloc((size + 1) * sizeof(uint32_t));
  iptr = i;

  for (ch = (unsigned char *)s; *ch; ch += converted)
  {
    if ((converted = mblen((char *)ch, MB_CUR_MAX)) >= 0)
    {
      *iptr = utf8tocp((char *)ch);
      iptr++;
    }
    else
    {
      *iptr++   = (uint32_t)*ch;
      converted = 1;
    }
  }

  *iptr = 0;

  return i;
}

/* ===================================================== */
/* Convert one hexadecimal digit to its numerical value. */
/*                                                       */
/* Return -1 when c is not a hexadecimal digit.          */
/* ===================================================== */
static int
hex_value(unsigned char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}

/* ================================================================= */
/* Convert two hexadecimal digits to one byte.                       */
/*                                                                   */
/* Return 1 on success and 0 if either character is not hexadecimal. */
/* ================================================================= */
static int
hex_byte(const char *s, unsigned char *byte)
{
  int high;
  int low;

  high = hex_value((unsigned char)s[0]);
  low  = hex_value((unsigned char)s[1]);

  if (high < 0 || low < 0)
    return 0;

  *byte = (unsigned char)((high << 4) | low);

  return 1;
}

/* =============================================================== */
/* Look up one Unicode value in preselected ISO-8859 mapping data. */
/*                                                                 */
/* Return 1 when byte belongs to an unchanged ISO-8859 range.      */
/* =============================================================== */
static int
is_identity_byte(const struct iso8859_charset *charset, unsigned char byte)
{
  size_t i;

  for (i = 0; i < charset->identity_count; i++)
  {
    if (byte < charset->identity[i].first)
      return 0;

    if (byte <= charset->identity[i].last)
      return 1;
  }

  return 0;
}

/* ====================================================================== */
/* Dichotomous search: the sorted array containing only the non-identical */
/* requests.                                                              */
/* ====================================================================== */
static int
find_remapped_byte(const struct iso8859_charset *charset,
                   uint32_t                      codepoint,
                   unsigned char                *byte)
{
  size_t low  = 0;
  size_t high = charset->map_count;

  while (low < high)
  {
    size_t middle = low + (high - low) / 2;

    if (codepoint < charset->map[middle].codepoint)
      high = middle;
    else if (codepoint > charset->map[middle].codepoint)
      low = middle + 1;
    else
    {
      *byte = charset->map[middle].byte;

      return 1;
    }
  }

  return 0;
}

/* ================================================================= */
/* Convert one Unicode value to the selected ISO-8859 character set. */
/* ================================================================= */
static int
encode_single_byte(uint32_t codepoint, int charset_number, unsigned char *byte)
{
  const struct iso8859_charset *charset;

  /* ISO-8859-1 is by far the most common ISO-8859 variant in current  */
  /* web usage. Its complete mapping is the identity mapping, so avoid */
  /* all table accesses and searches on this hot path.                 */
  /* ="""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (charset_number == 1)
  {
    if (codepoint <= 0xff)
    {
      *byte = (unsigned char)codepoint;

      return 1;
    }

    return 0;
  }

  /* Otherwise. */
  /* """""""""" */
  if (charset_number <= 0 || (size_t)charset_number >= ISO8859_CHARSET_COUNT)
    return 0;

  charset = iso8859_charsets[charset_number];

  if (charset == NULL)
    return 0;

  /* Every supported ISO-8859 character set preserves ASCII. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (codepoint < 0x80)
  {
    *byte = (unsigned char)codepoint;

    return 1;
  }

  /* Unchanged high-byte mappings are stored as readable ranges. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (codepoint <= 0xff && is_identity_byte(charset, (unsigned char)codepoint))
  {
    *byte = (unsigned char)codepoint;

    return 1;
  }

  return find_remapped_byte(charset, codepoint, byte);
}

/* ====================================================================== */
/* Replace the Unicode and UTF-8 hexadecimal escape sequences contained   */
/* in `s` with their interpreted values in the current character set,     */
/* when possible.                                                         */
/* The content of s is not preserved.                                     */
/*                                                                        */
/* Supported input formats for sequences to be interpreted:               */
/*   \\UXXXXXX   Exactly six hexadecimal digits containing a code point   */
/*               whit leading 0.                                          */
/*                                                                        */
/*   \\uXX...    One UTF-8 sequence represented by two hexadecimal digits */
/*               per byte. utf8_expected_length() determines the number   */
/*               of bytes from the first decoded byte.                    */
/*                                                                        */
/* Algorithm:                                                             */
/*   is_utf8 != 0                                                         */
/*     Emit validated UTF-8 bytes. charset is ignored.                    */
/*                                                                        */
/*   is_utf8 == 0, charset == 0                                           */
/*     No escaped character is assumed representable.                     */
/*                                                                        */
/*   is_utf8 == 0, charset in 1..11 or 13..16                             */
/*     Encode representable characters using the corresponding ISO-8859   */
/*     part.                                                              */
/*     ISO-8859-12 is undefined and therefore unsupported.                */
/*                                                                        */
/* A valid but unrepresentable character is replaced by one substitution  */
/* byte for each byte in its UTF-8 representation.                        */
/*                                                                        */
/* A malformed escape is replaced by one substitution byte.               */
/*                                                                        */
/* When substitute is '\0', byte 0x15 is used as subtitution character.   */
/*                                                                        */
/* Return 1 if every escape was converted successfully. Return 0 if s     */
/* is NULL, the locale is unsupported, or at least one escape was invalid */
/* or could not be represented in the selected output character set.      */
/* ====================================================================== */
int
utf8_interpret(char *s, int is_utf8, char substitute, int charset)
{
  char *read;
  char *write;
  char *end;
  int   rc = 1;

  if (s == NULL)
    return 0;

  /* Multibyte character sets other than UTF-8 are not supported. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!is_utf8 && MB_CUR_MAX != 1)
    return 0;

  if (substitute == '\0')
    substitute = (char)0x15;

  /* Read and write within the buffer supplied by the caller. */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  read  = s;
  write = s;
  end   = s + strlen(s);

  while (read < end)
  {
    unsigned char bytes[5];
    uint32_t      cp            = 0;
    size_t        utf8_length   = 0;
    size_t        consumed      = 0;
    size_t        produced      = 0;
    int           valid         = 0;
    int           representable = 0;

    /* Copy ordinary bytes without interpreting them. */
    /* """""""""""""""""""""""""""""""""""""""""""""" */
    if (*read != '\\' || end - read < 2 || (read[1] != 'U' && read[1] != 'u'))
    {
      if (!is_utf8 && (unsigned char)*read >= 0x80)
      {
        utf8_length = utf8_get_length((unsigned char)*read);

        if (utf8_length >= 2 && utf8_length <= 4
            && (size_t)(end - read) >= utf8_length)
        {
          memcpy(bytes, read, utf8_length);
          bytes[utf8_length] = '\0';

          if (utf8_validate(bytes) == NULL)
          {
            cp = utf8tocp((char *)bytes);

            if (!encode_single_byte(cp, charset, &bytes[0]))
            {
              memset(write, (unsigned char)substitute, utf8_length);

              write += utf8_length;
              read += utf8_length;
              rc = 0;

              continue;
            }
          }
        }
      }

      *write++ = *read++;
      continue;
    }

    if (read[1] == 'U')
    {
      size_t digits = 0;
      size_t i;

      /* Count at most the six digits belonging to the \\U escape. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      while (digits < 6 && read + 2 + digits < end
             && hex_value((unsigned char)read[2 + digits]) >= 0)
        digits++;

      consumed = 2 + digits;

      if (digits == 6)
      {
        /* Decode the six-digit Unicode value. */
        /* """"""""""""""""""""""""""""""""""" */
        for (i = 0; i < 6; i++)
          cp = (cp << 4) | (uint32_t)hex_value((unsigned char)read[2 + i]);

        /* Reject values outside the Unicode scalar-value range. */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
        if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
          valid = 0;
        else
        {
          /* Encode and validate the Unicode value as UTF-8. */
          /* """"""""""""""""""""""""""""""""""""""""""""""" */
          utf8_length = (size_t)cptoutf8((char *)bytes, cp);

          if (utf8_length == 0)
            valid = 0;
          else
          {
            bytes[utf8_length] = '\0';
            valid              = utf8_validate(bytes) == NULL;
          }
        }
      }
    }
    else
    {
      unsigned char first;
      size_t        required_digits  = 0;
      size_t        available_digits = 0;
      size_t        i;

      /* Decode the first UTF-8 byte and obtain its expected length. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      if (end - read >= 4 && hex_byte(read + 2, &first))
      {
        utf8_length     = utf8_get_length(first);
        required_digits = utf8_length * 2;
      }

      if (utf8_length == 0)
      {
        /* Consume at most the invalid first hexadecimal byte. */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
        while (available_digits < 2 && read + 2 + available_digits < end
               && hex_value((unsigned char)read[2 + available_digits]) >= 0)
          available_digits++;

        consumed = 2 + available_digits;
      }
      else
      {
        /* Count only the digits required by the announced UTF-8 length. */
        /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
        while (available_digits < required_digits
               && read + 2 + available_digits < end
               && hex_value((unsigned char)read[2 + available_digits]) >= 0)
          available_digits++;

        consumed = 2 + available_digits;

        if (available_digits == required_digits)
        {
          valid = 1;

          /* Rebuild the UTF-8 sequence from its hexadecimal byte pairs. */
          /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
          for (i = 0; i < utf8_length; i++)
          {
            if (!hex_byte(read + 2 + 2 * i, &bytes[i]))
            {
              valid = 0;
              break;
            }
          }

          if (valid)
          {
            bytes[utf8_length] = '\0';
            valid              = utf8_validate(bytes) == NULL;

            if (valid)
              cp = utf8tocp((char *)bytes);
          }
        }
      }
    }

    /* Never insert U+0000 into the middle of a C string. */
    /* """""""""""""""""""""""""""""""""""""""""""""""""" */
    if (valid && cp == 0)
      valid = 0;

    if (valid && is_utf8)
    {
      produced      = utf8_length;
      representable = 1;
    }
    else if (valid && encode_single_byte(cp, charset, &bytes[0]))
    {
      produced      = 1;
      representable = 1;
    }

    if (!representable)
    {
      /* Preserve the UTF-8 display width in bytes when substitution */
      /* occurs.                                                     */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
      produced = valid ? utf8_length : 1;
      memset(bytes, (unsigned char)substitute, produced);
      rc = 0;
    }

    memcpy(write, bytes, produced);
    write += produced;
    read += consumed;
  }

  *write = '\0';

  return rc;
}

/* ========================================================= */
/* Decodes the number of bytes taken by a UTF-8 character.   */
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

/* ======================================================== */
/* Returns the byte offset of the nth UTF-8 character in s. */
/* ======================================================== */
size_t
utf8_offset(char const *s, size_t n)
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

/* ================================================== */
/* Points to the previous UTF-8 character in a string */
/* from the given position.                           */
/* ================================================== */
char *
utf8_prev(const char *str, const char *p)
{
  if (str == NULL || p == NULL || p <= str)
    return NULL;

  do
  {
    p--;
  } while (p > str && ((unsigned char)*p & 0xc0) == 0x80);

  return (char *)p;
}

/* ============================================== */
/* Points to the next UTF-8 character in a string */
/* from the current position.                     */
/* ============================================== */
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
utf8_validate(unsigned char *s)
{
  /* clang-format off */
  while (*s)
  {
    if (*s < 0x80)
      /* 0xxxxxxx */
      s++;
    else if ((s[0] & 0xe0) == 0xc0)
    {
      /* 110XXXXx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) /* overlong? */
        return (char *)s;

      s += 2;
    }
    else if ((s[0] & 0xf0) == 0xe0)
    {
      /* 1110XXXX 10Xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
          (s[2] & 0xc0) != 0x80 ||
          (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) || /* overlong?         */
          (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) || /* surrogate?        */
          (s[0] == 0xef && s[1] == 0xbf &&
            (s[2] & 0xfe) == 0xbe))                   /* U+FFFE or U+FFFF? */
        return (char *)s;

      s += 3;
    }
    else if ((s[0] & 0xf8) == 0xf0)
    {
      /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
      if ((s[1] & 0xc0) != 0x80 ||
          (s[2] & 0xc0) != 0x80 ||
          (s[3] & 0xc0) != 0x80 ||
          (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||     /* overlong?   */
          (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
        return (char *)s;

      s += 4;
    }
    else
      return (char *)s;
  }
  /* clang-format on */

  return NULL;
}

size_t
utf8_strlen(const char *str)
{
  unsigned i   = 0;
  unsigned ret = 0;

  while (str[i] != '\0')
  {
    char c = str[i];

    if ((c & 0x80) == 0)
      i += 1;
    else if ((c & 0xe0) == 0xc0)
      i += 2;
    else if ((c & 0xf0) == 0xe0)
      i += 3;
    else if ((c & 0xf8) == 0xf0)
      i += 4;
    else
      return 0;

    ret++;
  }
  return ret;
}

/* ======================================================================== */
/* Multibytes extraction of the prefix of n UTF-8 characters from a string. */
/* The destination string d must have been allocated before.                */
/* pos is updated to reflect the position AFTER the prefix.                 */
/* ======================================================================== */
char *
utf8_strhead(char *d, char const *s, long n, long *pos)
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
      *dst = tolower((unsigned char)c);

    src++;
    dst++;
  }

  *dst = '\0';
}

/* =========================================================== */
/* Determine the width occupied on the screen by a valid UTF-8 */
/* character.                                                  */
/*                                                             */
/* s contains the sequence of hexadecimal bytes representing   */
/* this character.                                             */
/*                                                             */
/* The function assumes s has been previously validated.       */
/*                                                             */
/* The values used here are from Unicode 18.0 official files   */
/* (2026 September 16).                                        */
/*                                                             */
/* Note: this function is not generic (seed the special case   */
/*       for 0x05 at its beginning but can be made generic by  */
/*       removing these 4 lines.                               */
/* =========================================================== */
int
utf8_width(const unsigned char *s)
{
  if (s[0] < 0x80)
  {
    /* U+0005 */
    /* """""" */
    if ((s[0] == 0x05 || (s[0] >= 0x20 && s[0] <= 0x7e)))
      return 1;

    /* U+0000..U+0004, U+0006..U+001f, U+007f */
    /* """""""""""""""""""""""""""""""""""""" */
    if ((s[0] <= 0x04 || (s[0] >= 0x06 && s[0] <= 0x1f) || s[0] == 0x7f))
      return 0;
  }
  else if (s[0] < 0xe0)
  {
    /* U+00c0..U+00ff */
    /* """""""""""""" */
    if (((s[0] >= 0xc3 && s[0] <= 0xcb) || (s[0] >= 0xce && s[0] <= 0xd1)
         || (s[0] >= 0xd3 && s[0] <= 0xd5) || s[0] == 0xda))
      return 1;

    /* U+0080..U+00bf */
    /* """""""""""""" */
    if (s[0] == 0xc2)
    {
      if (((s[1] >= 0xa0 && s[1] <= 0xac) || s[1] >= 0xae))
        return 1;

      if (((s[1] >= 0x80 && s[1] <= 0x9f) || s[1] == 0xad))
        return 0;
    }

    /* U+0300..U+033f */
    /* """""""""""""" */
    if (s[0] == 0xcc)
      return 0;

    /* U+0340..U+037f */
    /* """""""""""""" */
    if (s[0] == 0xcd)
    {
      if (s[1] >= 0xb0)
        return 1;

      if ((s[1] >= 0x80 && s[1] <= 0xaf))
        return 0;
    }

    /* U+0480..U+04bf */
    /* """""""""""""" */
    if (s[0] == 0xd2)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x82) || s[1] >= 0x8a))
        return 1;

      if ((s[1] >= 0x83 && s[1] <= 0x89))
        return 0;
    }

    /* U+0580..U+05bf */
    /* """""""""""""" */
    if (s[0] == 0xd6)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x90) || s[1] == 0xbe))
        return 1;

      if (((s[1] >= 0x91 && s[1] <= 0xbd) || s[1] == 0xbf))
        return 0;
    }

    /* U+05c0..U+05ff */
    /* """""""""""""" */
    if (s[0] == 0xd7)
    {
      if ((s[1] == 0x80 || s[1] == 0x83 || s[1] == 0x86 || s[1] >= 0x8a))
        return 1;

      if (((s[1] >= 0x81 && s[1] <= 0x82) || (s[1] >= 0x84 && s[1] <= 0x85)
           || (s[1] >= 0x87 && s[1] <= 0x89)))
        return 0;
    }

    /* U+0600..U+063f */
    /* """""""""""""" */
    if (s[0] == 0xd8)
    {
      if (((s[1] >= 0x86 && s[1] <= 0x8f) || s[1] == 0x9b || s[1] >= 0x9d))
        return 1;

      if (((s[1] >= 0x80 && s[1] <= 0x85) || (s[1] >= 0x90 && s[1] <= 0x9a)
           || s[1] == 0x9c))
        return 0;
    }

    /* U+0640..U+067f */
    /* """""""""""""" */
    if (s[0] == 0xd9)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x8a) || (s[1] >= 0xa0 && s[1] <= 0xaf)
           || s[1] >= 0xb1))
        return 1;

      if (((s[1] >= 0x8b && s[1] <= 0x9f) || s[1] == 0xb0))
        return 0;
    }

    /* U+06c0..U+06ff */
    /* """""""""""""" */
    if (s[0] == 0xdb)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x95) || s[1] == 0x9e
           || (s[1] >= 0xa5 && s[1] <= 0xa6) || s[1] == 0xa9 || s[1] >= 0xae))
        return 1;

      if (((s[1] >= 0x96 && s[1] <= 0x9d) || (s[1] >= 0x9f && s[1] <= 0xa4)
           || (s[1] >= 0xa7 && s[1] <= 0xa8) || (s[1] >= 0xaa && s[1] <= 0xad)))
        return 0;
    }

    /* U+0700..U+073f */
    /* """""""""""""" */
    if (s[0] == 0xdc)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x8e) || s[1] == 0x90
           || (s[1] >= 0x92 && s[1] <= 0xaf)))
        return 1;

      if ((s[1] == 0x8f || s[1] == 0x91 || s[1] >= 0xb0))
        return 0;
    }

    /* U+0740..U+077f */
    /* """""""""""""" */
    if (s[0] == 0xdd)
    {
      if (s[1] >= 0x8b)
        return 1;

      if ((s[1] >= 0x80 && s[1] <= 0x8a))
        return 0;
    }

    /* U+0780..U+07bf */
    /* """""""""""""" */
    if (s[0] == 0xde)
    {
      if (((s[1] >= 0x80 && s[1] <= 0xa5) || s[1] >= 0xb1))
        return 1;

      if ((s[1] >= 0xa6 && s[1] <= 0xb0))
        return 0;
    }

    /* U+07c0..U+07ff */
    /* """""""""""""" */
    if (s[0] == 0xdf)
    {
      if (((s[1] >= 0x80 && s[1] <= 0xaa) || (s[1] >= 0xb4 && s[1] <= 0xbc)
           || s[1] >= 0xbe))
        return 1;

      if (((s[1] >= 0xab && s[1] <= 0xb3) || s[1] == 0xbd))
        return 0;
    }
  }
  else if (s[0] < 0xf0)
  {
    /* U+e000..U+efff */
    /* """""""""""""" */
    if (s[0] == 0xee)
      return 1;

    /* U+0800..U+0fff */
    /* """""""""""""" */
    if (s[0] == 0xe0)
    {
      if (s[1] == 0xa0)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x95) || s[2] == 0x9a || s[2] == 0xa4
             || s[2] == 0xa8 || s[2] >= 0xae))
          return 1;

        if (((s[2] >= 0x96 && s[2] <= 0x99) || (s[2] >= 0x9b && s[2] <= 0xa3)
             || (s[2] >= 0xa5 && s[2] <= 0xa7)
             || (s[2] >= 0xa9 && s[2] <= 0xad)))
          return 0;
      }
      if (s[1] == 0xa1)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x98) || s[2] >= 0x9c))
          return 1;

        if ((s[2] >= 0x99 && s[2] <= 0x9b))
          return 0;
      }
      if (s[1] == 0xa2)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8f) || (s[2] >= 0x92 && s[2] <= 0x96)
             || s[2] >= 0xa0))
          return 1;

        if (((s[2] >= 0x90 && s[2] <= 0x91) || (s[2] >= 0x97 && s[2] <= 0x9f)))
          return 0;
      }
      if (s[1] == 0xa3)
      {
        if ((s[2] >= 0x80 && s[2] <= 0x89))
          return 1;

        if (s[2] >= 0x8a)
          return 0;
      }
      if (s[1] == 0xa4)
      {
        if (((s[2] >= 0x83 && s[2] <= 0xb9) || s[2] == 0xbb || s[2] >= 0xbd))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x82) || s[2] == 0xba || s[2] == 0xbc))
          return 0;
      }
      if (s[1] == 0xa5)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x89 && s[2] <= 0x8c)
             || (s[2] >= 0x8e && s[2] <= 0x90) || (s[2] >= 0x98 && s[2] <= 0xa1)
             || s[2] >= 0xa4))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x88) || s[2] == 0x8d
             || (s[2] >= 0x91 && s[2] <= 0x97)
             || (s[2] >= 0xa2 && s[2] <= 0xa3)))
          return 0;
      }
      if (s[1] == 0xa6)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x82 && s[2] <= 0xbb) || s[2] >= 0xbd))
          return 1;

        if ((s[2] == 0x81 || s[2] == 0xbc))
          return 0;
      }
      if (s[1] == 0xa7)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x85 && s[2] <= 0x8c)
             || (s[2] >= 0x8e && s[2] <= 0xa1) || (s[2] >= 0xa4 && s[2] <= 0xbd)
             || s[2] == 0xbf))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x84) || s[2] == 0x8d
             || (s[2] >= 0xa2 && s[2] <= 0xa3) || s[2] == 0xbe))
          return 0;
      }
      if ((s[1] == 0xa8 || s[1] == 0xaa))
      {
        if ((s[2] == 0x80 || (s[2] >= 0x83 && s[2] <= 0xbb) || s[2] >= 0xbd))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x82) || s[2] == 0xbc))
          return 0;
      }
      if (s[1] == 0xa9)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x83 && s[2] <= 0x86)
             || (s[2] >= 0x89 && s[2] <= 0x8a) || (s[2] >= 0x8e && s[2] <= 0x90)
             || (s[2] >= 0x92 && s[2] <= 0xaf) || (s[2] >= 0xb2 && s[2] <= 0xb4)
             || s[2] >= 0xb6))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x82) || (s[2] >= 0x87 && s[2] <= 0x88)
             || (s[2] >= 0x8b && s[2] <= 0x8d) || s[2] == 0x91
             || (s[2] >= 0xb0 && s[2] <= 0xb1) || s[2] == 0xb5))
          return 0;
      }
      if (s[1] == 0xab)
      {
        if ((s[2] == 0x80 || s[2] == 0x86 || (s[2] >= 0x89 && s[2] <= 0x8c)
             || (s[2] >= 0x8e && s[2] <= 0xa1)
             || (s[2] >= 0xa4 && s[2] <= 0xb9)))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x85) || (s[2] >= 0x87 && s[2] <= 0x88)
             || s[2] == 0x8d || (s[2] >= 0xa2 && s[2] <= 0xa3) || s[2] >= 0xba))
          return 0;
      }
      if ((s[1] == 0xac || s[1] == 0xb2))
      {
        if ((s[2] == 0x80 || (s[2] >= 0x82 && s[2] <= 0xbb)
             || (s[2] >= 0xbd && s[2] <= 0xbe)))
          return 1;

        if ((s[2] == 0x81 || s[2] == 0xbc || s[2] == 0xbf))
          return 0;
      }
      if (s[1] == 0xad)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x85 && s[2] <= 0x8c)
             || (s[2] >= 0x8e && s[2] <= 0x92) || (s[2] >= 0x97 && s[2] <= 0xa1)
             || s[2] >= 0xa4))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x84) || s[2] == 0x8d
             || (s[2] >= 0x93 && s[2] <= 0x96)
             || (s[2] >= 0xa2 && s[2] <= 0xa3)))
          return 0;
      }
      if (s[1] == 0xae)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x81) || s[2] >= 0x83))
          return 1;

        if (s[2] == 0x82)
          return 0;
      }
      if (s[1] == 0xaf)
      {
        if (((s[2] >= 0x81 && s[2] <= 0x8c) || s[2] >= 0x8e))
          return 1;

        if ((s[2] == 0x80 || s[2] == 0x8d))
          return 0;
      }
      if (s[1] == 0xb0)
      {
        if (((s[2] >= 0x81 && s[2] <= 0x83) || (s[2] >= 0x85 && s[2] <= 0xbb)
             || s[2] == 0xbd))
          return 1;

        if ((s[2] == 0x80 || s[2] == 0x84 || s[2] == 0xbc || s[2] >= 0xbe))
          return 0;
      }
      if (s[1] == 0xb1)
      {
        if (((s[2] >= 0x81 && s[2] <= 0x85) || s[2] == 0x89
             || (s[2] >= 0x8e && s[2] <= 0x94) || (s[2] >= 0x97 && s[2] <= 0xa1)
             || s[2] >= 0xa4))
          return 1;

        if ((s[2] == 0x80 || (s[2] >= 0x86 && s[2] <= 0x88)
             || (s[2] >= 0x8a && s[2] <= 0x8d) || (s[2] >= 0x95 && s[2] <= 0x96)
             || (s[2] >= 0xa2 && s[2] <= 0xa3)))
          return 0;
      }
      if (s[1] == 0xb3)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x85) || (s[2] >= 0x87 && s[2] <= 0x8b)
             || (s[2] >= 0x8e && s[2] <= 0xa1) || s[2] >= 0xa4))
          return 1;

        if ((s[2] == 0x86 || (s[2] >= 0x8c && s[2] <= 0x8d)
             || (s[2] >= 0xa2 && s[2] <= 0xa3)))
          return 0;
      }
      if (s[1] == 0xb4)
      {
        if (((s[2] >= 0x82 && s[2] <= 0xba) || s[2] >= 0xbd))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x81) || (s[2] >= 0xbb && s[2] <= 0xbc)))
          return 0;
      }
      if (s[1] == 0xb5)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x85 && s[2] <= 0x8c)
             || (s[2] >= 0x8e && s[2] <= 0xa1) || s[2] >= 0xa4))
          return 1;

        if (((s[2] >= 0x81 && s[2] <= 0x84) || s[2] == 0x8d
             || (s[2] >= 0xa2 && s[2] <= 0xa3)))
          return 0;
      }
      if (s[1] == 0xb6)
      {
        if ((s[2] == 0x80 || s[2] >= 0x82))
          return 1;

        if (s[2] == 0x81)
          return 0;
      }
      if (s[1] == 0xb7)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x89) || (s[2] >= 0x8b && s[2] <= 0x91)
             || s[2] == 0x95 || s[2] >= 0x97))
          return 1;

        if ((s[2] == 0x8a || (s[2] >= 0x92 && s[2] <= 0x94) || s[2] == 0x96))
          return 0;
      }
      if (s[1] == 0xb8)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xb0) || (s[2] >= 0xb2 && s[2] <= 0xb3)
             || s[2] >= 0xbb))
          return 1;

        if ((s[2] == 0xb1 || (s[2] >= 0xb4 && s[2] <= 0xba)))
          return 0;
      }
      if (s[1] == 0xb9)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x86) || s[2] >= 0x8f))
          return 1;

        if ((s[2] >= 0x87 && s[2] <= 0x8e))
          return 0;
      }
      if (s[1] == 0xba)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xb0) || (s[2] >= 0xb2 && s[2] <= 0xb3)
             || s[2] >= 0xbd))
          return 1;

        if ((s[2] == 0xb1 || (s[2] >= 0xb4 && s[2] <= 0xbc)))
          return 0;
      }
      if (s[1] == 0xbb)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x87) || s[2] >= 0x8f))
          return 1;

        if ((s[2] >= 0x88 && s[2] <= 0x8e))
          return 0;
      }
      if (s[1] == 0xbc)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x97) || (s[2] >= 0x9a && s[2] <= 0xb4)
             || s[2] == 0xb6 || s[2] == 0xb8 || s[2] >= 0xba))
          return 1;

        if (((s[2] >= 0x98 && s[2] <= 0x99) || s[2] == 0xb5 || s[2] == 0xb7
             || s[2] == 0xb9))
          return 0;
      }
      if (s[1] == 0xbd)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xb0) || s[2] == 0xbf))
          return 1;

        if ((s[2] >= 0xb1 && s[2] <= 0xbe))
          return 0;
      }
      if (s[1] == 0xbe)
      {
        if ((s[2] == 0x85 || (s[2] >= 0x88 && s[2] <= 0x8c) || s[2] == 0x98
             || s[2] >= 0xbd))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x84) || (s[2] >= 0x86 && s[2] <= 0x87)
             || (s[2] >= 0x8d && s[2] <= 0x97)
             || (s[2] >= 0x99 && s[2] <= 0xbc)))
          return 0;
      }
      if (s[1] == 0xbf)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x85) || s[2] >= 0x87))
          return 1;

        if (s[2] == 0x86)
          return 0;
      }
    }

    /* U+1000..U+1fff */
    /* """""""""""""" */
    if (s[0] == 0xe1)
    {
      if ((s[1] == 0x83 || (s[1] >= 0x86 && s[1] <= 0x8c)
           || (s[1] >= 0x8e && s[1] <= 0x9b) || s[1] == 0xa1 || s[1] == 0xa3
           || (s[1] >= 0xa5 && s[1] <= 0xa7) || (s[1] >= 0xb1 && s[1] <= 0xb2)
           || (s[1] >= 0xb4 && s[1] <= 0xb6) || s[1] >= 0xb8))
        return 1;

      if (s[1] == 0x80)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xac) || s[2] == 0xb1 || s[2] == 0xb8
             || (s[2] >= 0xbb && s[2] <= 0xbc) || s[2] == 0xbf))
          return 1;

        if (((s[2] >= 0xad && s[2] <= 0xb0) || (s[2] >= 0xb2 && s[2] <= 0xb7)
             || (s[2] >= 0xb9 && s[2] <= 0xba)
             || (s[2] >= 0xbd && s[2] <= 0xbe)))
          return 0;
      }
      if (s[1] == 0x81)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x97) || (s[2] >= 0x9a && s[2] <= 0x9d)
             || (s[2] >= 0xa1 && s[2] <= 0xb0) || s[2] >= 0xb5))
          return 1;

        if (((s[2] >= 0x98 && s[2] <= 0x99) || (s[2] >= 0x9e && s[2] <= 0xa0)
             || (s[2] >= 0xb1 && s[2] <= 0xb4)))
          return 0;
      }
      if (s[1] == 0x82)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x81) || (s[2] >= 0x83 && s[2] <= 0x84)
             || (s[2] >= 0x87 && s[2] <= 0x8c) || (s[2] >= 0x8e && s[2] <= 0x9c)
             || s[2] >= 0x9e))
          return 1;

        if ((s[2] == 0x82 || (s[2] >= 0x85 && s[2] <= 0x86) || s[2] == 0x8d
             || s[2] == 0x9d))
          return 0;
      }
      if (s[1] == 0x84)
        return 2;

      if (s[1] == 0x85)
      {
        if (s[2] >= 0xa0)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0x9f))
          return 2;
      }
      if (s[1] == 0x8d)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9c) || s[2] >= 0xa0))
          return 1;

        if ((s[2] >= 0x9d && s[2] <= 0x9f))
          return 0;
      }
      if (s[1] == 0x9c)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x91) || (s[2] >= 0x95 && s[2] <= 0xb1)
             || s[2] >= 0xb4))
          return 1;

        if (((s[2] >= 0x92 && s[2] <= 0x94) || (s[2] >= 0xb2 && s[2] <= 0xb3)))
          return 0;
      }
      if (s[1] == 0x9d)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x91) || (s[2] >= 0x94 && s[2] <= 0xb1)
             || s[2] >= 0xb4))
          return 1;

        if (((s[2] >= 0x92 && s[2] <= 0x93) || (s[2] >= 0xb2 && s[2] <= 0xb3)))
          return 0;
      }
      if (s[1] == 0x9e)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xb3) || s[2] == 0xb6 || s[2] >= 0xbe))
          return 1;

        if (((s[2] >= 0xb4 && s[2] <= 0xb5) || (s[2] >= 0xb7 && s[2] <= 0xbd)))
          return 0;
      }
      if (s[1] == 0x9f)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x85) || (s[2] >= 0x87 && s[2] <= 0x88)
             || (s[2] >= 0x94 && s[2] <= 0x9c) || s[2] >= 0x9e))
          return 1;

        if ((s[2] == 0x86 || (s[2] >= 0x89 && s[2] <= 0x93) || s[2] == 0x9d))
          return 0;
      }
      if (s[1] == 0xa0)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8a) || s[2] >= 0x90))
          return 1;

        if ((s[2] >= 0x8b && s[2] <= 0x8f))
          return 0;
      }
      if (s[1] == 0xa2)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x84) || (s[2] >= 0x87 && s[2] <= 0xa8)
             || s[2] >= 0xaa))
          return 1;

        if (((s[2] >= 0x85 && s[2] <= 0x86) || s[2] == 0xa9))
          return 0;
      }
      if (s[1] == 0xa4)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9f) || (s[2] >= 0xa3 && s[2] <= 0xa6)
             || (s[2] >= 0xa9 && s[2] <= 0xb1) || (s[2] >= 0xb3 && s[2] <= 0xb8)
             || s[2] >= 0xbc))
          return 1;

        if (((s[2] >= 0xa0 && s[2] <= 0xa2) || (s[2] >= 0xa7 && s[2] <= 0xa8)
             || s[2] == 0xb2 || (s[2] >= 0xb9 && s[2] <= 0xbb)))
          return 0;
      }
      if (s[1] == 0xa8)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x96) || (s[2] >= 0x99 && s[2] <= 0x9a)
             || s[2] >= 0x9c))
          return 1;

        if (((s[2] >= 0x97 && s[2] <= 0x98) || s[2] == 0x9b))
          return 0;
      }
      if (s[1] == 0xa9)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x95) || s[2] == 0x97 || s[2] == 0x9f
             || s[2] == 0xa1 || (s[2] >= 0xa3 && s[2] <= 0xa4)
             || (s[2] >= 0xad && s[2] <= 0xb2)
             || (s[2] >= 0xbd && s[2] <= 0xbe)))
          return 1;

        if ((s[2] == 0x96 || (s[2] >= 0x98 && s[2] <= 0x9e) || s[2] == 0xa0
             || s[2] == 0xa2 || (s[2] >= 0xa5 && s[2] <= 0xac)
             || (s[2] >= 0xb3 && s[2] <= 0xbc) || s[2] == 0xbf))
          return 0;
      }
      if (s[1] == 0xaa)
      {
        if ((s[2] >= 0x80 && s[2] <= 0xaf))
          return 1;

        if (s[2] >= 0xb0)
          return 0;
      }
      if (s[1] == 0xab)
      {
        if (s[2] >= 0xb1)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0xb0))
          return 0;
      }
      if (s[1] == 0xac)
      {
        if (((s[2] >= 0x84 && s[2] <= 0xb3) || s[2] == 0xb5 || s[2] == 0xbb
             || s[2] >= 0xbd))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x83) || s[2] == 0xb4
             || (s[2] >= 0xb6 && s[2] <= 0xba) || s[2] == 0xbc))
          return 0;
      }
      if (s[1] == 0xad)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x81) || (s[2] >= 0x83 && s[2] <= 0xaa)
             || s[2] >= 0xb4))
          return 1;

        if ((s[2] == 0x82 || (s[2] >= 0xab && s[2] <= 0xb3)))
          return 0;
      }
      if (s[1] == 0xae)
      {
        if (((s[2] >= 0x82 && s[2] <= 0xa1) || (s[2] >= 0xa6 && s[2] <= 0xa7)
             || s[2] == 0xaa || s[2] >= 0xae))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x81) || (s[2] >= 0xa2 && s[2] <= 0xa5)
             || (s[2] >= 0xa8 && s[2] <= 0xa9)
             || (s[2] >= 0xab && s[2] <= 0xad)))
          return 0;
      }
      if (s[1] == 0xaf)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa5) || s[2] == 0xa7
             || (s[2] >= 0xaa && s[2] <= 0xac) || s[2] == 0xae || s[2] >= 0xb2))
          return 1;

        if ((s[2] == 0xa6 || (s[2] >= 0xa8 && s[2] <= 0xa9) || s[2] == 0xad
             || (s[2] >= 0xaf && s[2] <= 0xb1)))
          return 0;
      }
      if (s[1] == 0xb0)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xab) || (s[2] >= 0xb4 && s[2] <= 0xb5)
             || s[2] >= 0xb8))
          return 1;

        if (((s[2] >= 0xac && s[2] <= 0xb3) || (s[2] >= 0xb6 && s[2] <= 0xb7)))
          return 0;
      }
      if (s[1] == 0xb3)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8f) || s[2] == 0x93 || s[2] == 0xa1
             || (s[2] >= 0xa9 && s[2] <= 0xac) || (s[2] >= 0xae && s[2] <= 0xb3)
             || (s[2] >= 0xb5 && s[2] <= 0xb7) || s[2] >= 0xba))
          return 1;

        if (((s[2] >= 0x90 && s[2] <= 0x92) || (s[2] >= 0x94 && s[2] <= 0xa0)
             || (s[2] >= 0xa2 && s[2] <= 0xa8) || s[2] == 0xad || s[2] == 0xb4
             || (s[2] >= 0xb8 && s[2] <= 0xb9)))
          return 0;
      }
      if (s[1] == 0xb7)
        return 0;
    }

    /* U+2000..U+2fff */
    /* """""""""""""" */
    if (s[0] == 0xe2)
    {
      if ((s[1] == 0x82 || (s[1] >= 0x84 && s[1] <= 0x8b)
           || (s[1] >= 0x8d && s[1] <= 0x8e) || (s[1] >= 0x90 && s[1] <= 0x96)
           || (s[1] >= 0x9f && s[1] <= 0xab) || (s[1] >= 0xae && s[1] <= 0xb2)
           || s[1] == 0xb4 || s[1] == 0xb6))
        return 1;

      if ((s[1] >= 0xb8 && s[1] <= 0xb9))
        return 1;

      if (s[1] == 0x80)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8a) || (s[2] >= 0x90 && s[2] <= 0xa9)
             || s[2] >= 0xaf))
          return 1;

        if (((s[2] >= 0x8b && s[2] <= 0x8f) || (s[2] >= 0xaa && s[2] <= 0xae)))
          return 0;
      }
      if (s[1] == 0x81)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9f) || s[2] == 0xa5 || s[2] >= 0xb0))
          return 1;

        if (((s[2] >= 0xa0 && s[2] <= 0xa4) || (s[2] >= 0xa6 && s[2] <= 0xaf)))
          return 0;
      }
      if (s[1] == 0x83)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8f) || s[2] >= 0xb1))
          return 1;

        if ((s[2] >= 0x90 && s[2] <= 0xb0))
          return 0;
      }
      if (s[1] == 0x8c)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x99) || (s[2] >= 0x9c && s[2] <= 0xa8)
             || s[2] >= 0xab))
          return 1;

        if (((s[2] >= 0x9a && s[2] <= 0x9b) || (s[2] >= 0xa9 && s[2] <= 0xaa)))
          return 2;
      }
      if (s[1] == 0x8f)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa8) || (s[2] >= 0xad && s[2] <= 0xaf)
             || (s[2] >= 0xb1 && s[2] <= 0xb2) || s[2] >= 0xb4))
          return 1;

        if (((s[2] >= 0xa9 && s[2] <= 0xac) || s[2] == 0xb0 || s[2] == 0xb3))
          return 2;
      }
      if (s[1] == 0x97)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xbc) || s[2] == 0xbf))
          return 1;

        if ((s[2] >= 0xbd && s[2] <= 0xbe))
          return 2;
      }
      if (s[1] == 0x98)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x93) || (s[2] >= 0x96 && s[2] <= 0xaf)
             || s[2] >= 0xb8))
          return 1;

        if (((s[2] >= 0x94 && s[2] <= 0x95) || (s[2] >= 0xb0 && s[2] <= 0xb7)))
          return 2;
      }
      if (s[1] == 0x99)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x87) || (s[2] >= 0x94 && s[2] <= 0xbe)))
          return 1;

        if (((s[2] >= 0x88 && s[2] <= 0x93) || s[2] == 0xbf))
          return 2;
      }
      if (s[1] == 0x9a)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x89) || (s[2] >= 0x90 && s[2] <= 0x92)
             || (s[2] >= 0x94 && s[2] <= 0xa0) || (s[2] >= 0xa2 && s[2] <= 0xa9)
             || (s[2] >= 0xac && s[2] <= 0xbc) || s[2] == 0xbf))
          return 1;

        if (((s[2] >= 0x8a && s[2] <= 0x8f) || s[2] == 0x93 || s[2] == 0xa1
             || (s[2] >= 0xaa && s[2] <= 0xab)
             || (s[2] >= 0xbd && s[2] <= 0xbe)))
          return 2;
      }
      if (s[1] == 0x9b)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x83) || (s[2] >= 0x86 && s[2] <= 0x8d)
             || (s[2] >= 0x8f && s[2] <= 0x93) || (s[2] >= 0x95 && s[2] <= 0xa9)
             || (s[2] >= 0xab && s[2] <= 0xb1) || s[2] == 0xb4))
          return 1;

        if (((s[2] >= 0xb6 && s[2] <= 0xb9) || (s[2] >= 0xbb && s[2] <= 0xbc)
             || s[2] >= 0xbe))
          return 1;

        if (((s[2] >= 0x84 && s[2] <= 0x85) || s[2] == 0x8e || s[2] == 0x94
             || s[2] == 0xaa || (s[2] >= 0xb2 && s[2] <= 0xb3) || s[2] == 0xb5
             || s[2] == 0xba || s[2] == 0xbd))
          return 2;
      }
      if (s[1] == 0x9c)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x84) || (s[2] >= 0x86 && s[2] <= 0x89)
             || (s[2] >= 0x8c && s[2] <= 0xa7) || s[2] >= 0xa9))
          return 1;

        if ((s[2] == 0x85 || (s[2] >= 0x8a && s[2] <= 0x8b) || s[2] == 0xa8))
          return 2;
      }
      if (s[1] == 0x9d)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8b) || s[2] == 0x8d
             || (s[2] >= 0x8f && s[2] <= 0x92) || s[2] == 0x96 || s[2] >= 0x98))
          return 1;

        if ((s[2] == 0x8c || s[2] == 0x8e || (s[2] >= 0x93 && s[2] <= 0x95)
             || s[2] == 0x97))
          return 2;
      }
      if (s[1] == 0x9e)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x94) || (s[2] >= 0x98 && s[2] <= 0xaf)
             || (s[2] >= 0xb1 && s[2] <= 0xbe)))
          return 1;

        if (((s[2] >= 0x95 && s[2] <= 0x97) || s[2] == 0xb0 || s[2] == 0xbf))
          return 2;
      }
      if (s[1] == 0xac)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9a) || s[2] >= 0x9d))
          return 1;

        if ((s[2] >= 0x9b && s[2] <= 0x9c))
          return 2;
      }
      if (s[1] == 0xad)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8f) || (s[2] >= 0x91 && s[2] <= 0x94)
             || s[2] >= 0x96))
          return 1;

        if ((s[2] == 0x90 || s[2] == 0x95))
          return 2;
      }
      if (s[1] == 0xb3)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xae) || s[2] >= 0xb2))
          return 1;

        if ((s[2] >= 0xaf && s[2] <= 0xb1))
          return 0;
      }
      if (s[1] == 0xb5)
      {
        if ((s[2] >= 0x80 && s[2] <= 0xbe))
          return 1;

        if (s[2] == 0xbf)
          return 0;
      }
      if (s[1] == 0xb7)
      {
        if ((s[2] >= 0x80 && s[2] <= 0x9f))
          return 1;

        if (s[2] >= 0xa0)
          return 0;
      }
      if (s[1] == 0xba)
      {
        if (s[2] == 0x9a)
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x99) || s[2] >= 0x9b))
          return 2;
      }
      if (s[1] == 0xbb)
      {
        if (s[2] >= 0xb4)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0xb3))
          return 2;
      }
      if ((s[1] >= 0xbc && s[1] <= 0xbe))
        return 2;

      if (s[1] == 0xbf)
      {
        if ((s[2] >= 0x96 && s[2] <= 0xaf))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x95) || s[2] >= 0xb0))
          return 2;
      }
    }

    /* U+3000..U+3fff */
    /* """""""""""""" */
    if (s[0] == 0xe3)
    {
      if (s[1] == 0x80)
      {
        if (s[2] == 0xbf)
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0xa9) || (s[2] >= 0xae && s[2] <= 0xbe)))
          return 2;

        if ((s[2] >= 0xaa && s[2] <= 0xad))
          return 0;
      }
      if (s[1] == 0x81)
      {
        if (s[2] == 0x80)
          return 1;

        if (s[2] >= 0x81)
          return 2;
      }
      if (s[1] == 0x82)
      {
        if ((s[2] >= 0x97 && s[2] <= 0x98))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x96) || s[2] >= 0x9b))
          return 2;

        if ((s[2] >= 0x99 && s[2] <= 0x9a))
          return 0;
      }
      if ((s[1] == 0x83 || s[1] == 0x85 || s[1] >= 0x8a))
        return 2;

      if (s[1] == 0x84)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x84) || s[2] == 0xb0))
          return 1;

        if (((s[2] >= 0x85 && s[2] <= 0xaf) || s[2] >= 0xb1))
          return 2;
      }
      if (s[1] == 0x86)
      {
        if (s[2] == 0x8f)
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x8e) || s[2] >= 0x90))
          return 2;
      }
      if (s[1] == 0x87)
      {
        if ((s[2] >= 0xa6 && s[2] <= 0xae))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0xa5) || s[2] >= 0xaf))
          return 2;
      }
      if (s[1] == 0x88)
      {
        if (s[2] == 0x9f)
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x9e) || s[2] >= 0xa0))
          return 2;
      }
      if (s[1] == 0x89)
      {
        if ((s[2] >= 0x88 && s[2] <= 0x8f))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x87) || s[2] >= 0x90))
          return 2;
      }
    }

    /* U+4000..U+4fff */
    /* """""""""""""" */
    if (((s[0] >= 0xe4 && s[0] <= 0xe9) || (s[0] >= 0xeb && s[0] <= 0xec)))
      return 2;

    /* U+a000..U+afff */
    /* """""""""""""" */
    if (s[0] == 0xea)
    {
      if (((s[1] >= 0x94 && s[1] <= 0x98) || (s[1] >= 0x9c && s[1] <= 0x9f)
           || (s[1] >= 0xa1 && s[1] <= 0xa2) || (s[1] >= 0xac && s[1] <= 0xae)))
        return 1;

      if (((s[1] >= 0x80 && s[1] <= 0x91) || s[1] >= 0xb0))
        return 2;

      if (s[1] == 0x92)
      {
        if ((s[2] >= 0x8d && s[2] <= 0x8f))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x8c) || s[2] >= 0x90))
          return 2;
      }
      if (s[1] == 0x93)
      {
        if (s[2] >= 0x87)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0x86))
          return 2;
      }
      if (s[1] == 0x99)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xae) || s[2] == 0xb3 || s[2] >= 0xbe))
          return 1;

        if (((s[2] >= 0xaf && s[2] <= 0xb2) || (s[2] >= 0xb4 && s[2] <= 0xbd)))
          return 0;
      }
      if (s[1] == 0x9a)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9d) || s[2] >= 0xa0))
          return 1;

        if ((s[2] >= 0x9e && s[2] <= 0x9f))
          return 0;
      }
      if (s[1] == 0x9b)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xaf) || s[2] >= 0xb2))
          return 1;

        if ((s[2] >= 0xb0 && s[2] <= 0xb1))
          return 0;
      }
      if (s[1] == 0xa0)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x81) || (s[2] >= 0x83 && s[2] <= 0x85)
             || (s[2] >= 0x87 && s[2] <= 0x8a) || (s[2] >= 0x8c && s[2] <= 0xa4)
             || (s[2] >= 0xa7 && s[2] <= 0xab) || s[2] >= 0xad))
          return 1;

        if ((s[2] == 0x82 || s[2] == 0x86 || s[2] == 0x8b
             || (s[2] >= 0xa5 && s[2] <= 0xa6) || s[2] == 0xac))
          return 0;
      }
      if (s[1] == 0xa3)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x83) || (s[2] >= 0x86 && s[2] <= 0x9f)
             || (s[2] >= 0xb2 && s[2] <= 0xbe)))
          return 1;

        if (((s[2] >= 0x84 && s[2] <= 0x85) || (s[2] >= 0xa0 && s[2] <= 0xb1)
             || s[2] == 0xbf))
          return 0;
      }
      if (s[1] == 0xa4)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa5) || s[2] >= 0xae))
          return 1;

        if ((s[2] >= 0xa6 && s[2] <= 0xad))
          return 0;
      }
      if (s[1] == 0xa5)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x86) || (s[2] >= 0x92 && s[2] <= 0x9f)
             || s[2] >= 0xbd))
          return 1;

        if ((s[2] >= 0x87 && s[2] <= 0x91))
          return 0;

        if ((s[2] >= 0xa0 && s[2] <= 0xbc))
          return 2;
      }
      if (s[1] == 0xa6)
      {
        if (((s[2] >= 0x83 && s[2] <= 0xb2) || (s[2] >= 0xb4 && s[2] <= 0xb5)
             || (s[2] >= 0xba && s[2] <= 0xbb) || s[2] >= 0xbe))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x82) || s[2] == 0xb3
             || (s[2] >= 0xb6 && s[2] <= 0xb9)
             || (s[2] >= 0xbc && s[2] <= 0xbd)))
          return 0;
      }
      if (s[1] == 0xa7)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa4) || s[2] >= 0xa6))
          return 1;

        if (s[2] == 0xa5)
          return 0;
      }
      if (s[1] == 0xa8)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa8) || (s[2] >= 0xaf && s[2] <= 0xb0)
             || (s[2] >= 0xb3 && s[2] <= 0xb4) || s[2] >= 0xb7))
          return 1;

        if (((s[2] >= 0xa9 && s[2] <= 0xae) || (s[2] >= 0xb1 && s[2] <= 0xb2)
             || (s[2] >= 0xb5 && s[2] <= 0xb6)))
          return 0;
      }
      if (s[1] == 0xa9)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x82) || (s[2] >= 0x84 && s[2] <= 0x8b)
             || (s[2] >= 0x8d && s[2] <= 0xbb) || s[2] >= 0xbd))
          return 1;

        if ((s[2] == 0x83 || s[2] == 0x8c || s[2] == 0xbc))
          return 0;
      }
      if (s[1] == 0xaa)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xaf) || s[2] == 0xb1
             || (s[2] >= 0xb5 && s[2] <= 0xb6)
             || (s[2] >= 0xb9 && s[2] <= 0xbd)))
          return 1;

        if ((s[2] == 0xb0 || (s[2] >= 0xb2 && s[2] <= 0xb4)
             || (s[2] >= 0xb7 && s[2] <= 0xb8) || s[2] >= 0xbe))
          return 0;
      }
      if (s[1] == 0xab)
      {
        if ((s[2] == 0x80 || (s[2] >= 0x82 && s[2] <= 0xab)
             || (s[2] >= 0xae && s[2] <= 0xb5) || s[2] >= 0xb7))
          return 1;

        if ((s[2] == 0x81 || (s[2] >= 0xac && s[2] <= 0xad) || s[2] == 0xb6))
          return 0;
      }
      if (s[1] == 0xaf)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xa4) || (s[2] >= 0xa6 && s[2] <= 0xa7)
             || (s[2] >= 0xa9 && s[2] <= 0xac) || s[2] >= 0xae))
          return 1;

        if ((s[2] == 0xa5 || s[2] == 0xa8 || s[2] == 0xad))
          return 0;
      }
    }

    /* U+d000..U+d7ff */
    /* """""""""""""" */
    if (s[0] == 0xed)
    {
      if (s[1] == 0x9f)
        return 1;

      if ((s[1] >= 0x80 && s[1] <= 0x9d))
        return 2;

      if (s[1] == 0x9e)
      {
        if (s[2] >= 0xa4)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0xa3))
          return 2;
      }
    }

    /* U+f000..U+ffff */
    /* """""""""""""" */
    if (s[0] == 0xef)
    {
      if (((s[1] >= 0x80 && s[1] <= 0xa3) || (s[1] >= 0xad && s[1] <= 0xb7)
           || s[1] == 0xba || s[1] == 0xbe))
        return 1;

      if ((s[1] >= 0xa4 && s[1] <= 0xab))
        return 2;

      if (s[1] == 0xac)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9d) || s[2] >= 0x9f))
          return 1;

        if (s[2] == 0x9e)
          return 0;
      }
      if (s[1] == 0xb8)
      {
        if ((s[2] >= 0x9a && s[2] <= 0x9f))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x8f) || (s[2] >= 0xa0 && s[2] <= 0xaf)))
          return 0;

        if (((s[2] >= 0x90 && s[2] <= 0x99) || s[2] >= 0xb0))
          return 2;
      }
      if (s[1] == 0xb9)
      {
        if ((s[2] == 0x93 || s[2] == 0xa7 || s[2] >= 0xac))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x92) || (s[2] >= 0x94 && s[2] <= 0xa6)
             || (s[2] >= 0xa8 && s[2] <= 0xab)))
          return 2;
      }
      if (s[1] == 0xbb)
      {
        if ((s[2] >= 0x80 && s[2] <= 0xbe))
          return 1;

        if (s[2] == 0xbf)
          return 0;
      }
      if (s[1] == 0xbc)
      {
        if (s[2] == 0x80)
          return 1;

        if (s[2] >= 0x81)
          return 2;
      }
      if (s[1] == 0xbd)
      {
        if (s[2] >= 0xa1)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0xa0))
          return 2;
      }
      if (s[1] == 0xbf)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x9f) || (s[2] >= 0xa7 && s[2] <= 0xb8)
             || s[2] >= 0xbc))
          return 1;

        if ((s[2] >= 0xa0 && s[2] <= 0xa6))
          return 2;

        if ((s[2] >= 0xb9 && s[2] <= 0xbb))
          return 0;
      }
    }
  }
  else
  {
    /* U+40000..U+7ffff */
    /* """""""""""""""" */
    if ((s[0] >= 0xf1 && s[0] <= 0xf2))
      return 1;

    /* U+100000..U+10ffff */
    /* """""""""""""""""" */
    if (s[0] == 0xf4)
    {
      if ((s[1] >= 0x80 && s[1] <= 0x8f))
        return 1;
    }

    /* U+10000..U+3ffff */
    /* """""""""""""""" */
    if (s[0] == 0xf0)
    {
      if ((s[1] == 0x92 || (s[1] >= 0x94 && s[1] <= 0x95)))
        return 1;

      if (s[1] == 0x90)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x86) || (s[2] >= 0x88 && s[2] <= 0x8a)
             || s[2] == 0x8c || (s[2] >= 0x8e && s[2] <= 0xa7)
             || (s[2] >= 0xa9 && s[2] <= 0xaa)
             || (s[2] >= 0xac && s[2] <= 0xb3)))
          return 1;

        if (((s[2] >= 0xb6 && s[2] <= 0xb9) || s[2] == 0xbc || s[2] == 0xbf))
          return 1;

        if (s[2] == 0x87)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xbc) || s[3] >= 0xbe))
            return 1;

          if (s[3] == 0xbd)
            return 0;
        }
        if (s[2] == 0x8b)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9f) || s[3] >= 0xa1))
            return 1;

          if (s[3] == 0xa0)
            return 0;
        }
        if (s[2] == 0x8d)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb5) || s[3] >= 0xbb))
            return 1;

          if ((s[3] >= 0xb6 && s[3] <= 0xba))
            return 0;
        }
        if (s[2] == 0xa8)
        {
          if ((s[3] == 0x80 || s[3] == 0x84 || (s[3] >= 0x87 && s[3] <= 0x8b)
               || (s[3] >= 0x90 && s[3] <= 0xb7)
               || (s[3] >= 0xbb && s[3] <= 0xbe)))
            return 1;

          if (((s[3] >= 0x81 && s[3] <= 0x83) || (s[3] >= 0x85 && s[3] <= 0x86)
               || (s[3] >= 0x8c && s[3] <= 0x8f)
               || (s[3] >= 0xb8 && s[3] <= 0xba) || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0xab)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa4) || s[3] >= 0xa7))
            return 1;

          if ((s[3] >= 0xa5 && s[3] <= 0xa6))
            return 0;
        }
        if (s[2] == 0xb4)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa3) || s[3] >= 0xa8))
            return 1;

          if ((s[3] >= 0xa4 && s[3] <= 0xa7))
            return 0;
        }
        if (s[2] == 0xb5)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa8) || s[3] >= 0xae))
            return 1;

          if ((s[3] >= 0xa9 && s[3] <= 0xad))
            return 0;
        }
        if (s[2] == 0xba)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaa) || s[3] >= 0xad))
            return 1;

          if ((s[3] >= 0xab && s[3] <= 0xac))
            return 0;
        }
        if (s[2] == 0xbb)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8a)
               || (s[3] >= 0x90 && s[3] <= 0xaf)))
            return 1;

          if (((s[3] >= 0x8b && s[3] <= 0x8f) || s[3] >= 0xb0))
            return 0;
        }
        if (s[2] == 0xbd)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x85) || s[3] >= 0x91))
            return 1;

          if ((s[3] >= 0x86 && s[3] <= 0x90))
            return 0;
        }
        if (s[2] == 0xbe)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x81) || s[3] >= 0x86))
            return 1;

          if ((s[3] >= 0x82 && s[3] <= 0x85))
            return 0;
        }
      }
      if (s[1] == 0x91)
      {
        if ((s[2] == 0x8a || (s[2] >= 0x94 && s[2] <= 0x95) || s[2] == 0x9b
             || (s[2] >= 0x9d && s[2] <= 0x9f) || (s[2] >= 0xa1 && s[2] <= 0xa3)
             || s[2] == 0xa6 || (s[2] >= 0xab && s[2] <= 0xac)
             || (s[2] >= 0xae && s[2] <= 0xaf) || s[2] == 0xb1 || s[2] == 0xb3))
          return 1;

        if (((s[2] >= 0xb8 && s[2] <= 0xba) || s[2] >= 0xbe))
          return 1;

        if (s[2] == 0x80)
        {
          if ((s[3] == 0x80 || (s[3] >= 0x82 && s[3] <= 0xb7)))
            return 1;

          if ((s[3] == 0x81 || s[3] >= 0xb8))
            return 0;
        }
        if (s[2] == 0x81)
        {
          if (((s[3] >= 0x87 && s[3] <= 0xaf) || (s[3] >= 0xb1 && s[3] <= 0xb2)
               || (s[3] >= 0xb5 && s[3] <= 0xbe)))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x86) || s[3] == 0xb0
               || (s[3] >= 0xb3 && s[3] <= 0xb4) || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0x82)
        {
          if (((s[3] >= 0x82 && s[3] <= 0xb2) || (s[3] >= 0xb7 && s[3] <= 0xb8)
               || (s[3] >= 0xbb && s[3] <= 0xbc) || s[3] >= 0xbe))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x81) || (s[3] >= 0xb3 && s[3] <= 0xb6)
               || (s[3] >= 0xb9 && s[3] <= 0xba) || s[3] == 0xbd))
            return 0;
        }
        if (s[2] == 0x83)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x81) || (s[3] >= 0x83 && s[3] <= 0x8c)
               || s[3] >= 0x8e))
            return 1;

          if ((s[3] == 0x82 || s[3] == 0x8d))
            return 0;
        }
        if (s[2] == 0x84)
        {
          if (((s[3] >= 0x83 && s[3] <= 0xa6) || s[3] == 0xac || s[3] >= 0xb5))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x82) || (s[3] >= 0xa7 && s[3] <= 0xab)
               || (s[3] >= 0xad && s[3] <= 0xb4)))
            return 0;
        }
        if (s[2] == 0x85)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb2) || s[3] >= 0xb4))
            return 1;

          if (s[3] == 0xb3)
            return 0;
        }
        if (s[2] == 0x86)
        {
          if (((s[3] >= 0x82 && s[3] <= 0xb5) || s[3] == 0xbf))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x81)
               || (s[3] >= 0xb6 && s[3] <= 0xbe)))
            return 0;
        }
        if (s[2] == 0x87)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x88) || (s[3] >= 0x8d && s[3] <= 0x8e)
               || s[3] >= 0x90))
            return 1;

          if (((s[3] >= 0x89 && s[3] <= 0x8c) || s[3] == 0x8f))
            return 0;
        }
        if (s[2] == 0x88)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xae) || (s[3] >= 0xb2 && s[3] <= 0xb3)
               || s[3] == 0xb5 || (s[3] >= 0xb8 && s[3] <= 0xbd)
               || s[3] == 0xbf))
            return 1;

          if (((s[3] >= 0xaf && s[3] <= 0xb1) || s[3] == 0xb4
               || (s[3] >= 0xb6 && s[3] <= 0xb7) || s[3] == 0xbe))
            return 0;
        }
        if (s[2] == 0x89)
        {
          if ((s[3] == 0x80 || s[3] >= 0x82))
            return 1;

          if (s[3] == 0x81)
            return 0;
        }
        if (s[2] == 0x8b)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9e) || (s[3] >= 0xa0 && s[3] <= 0xa2)
               || s[3] >= 0xab))
            return 1;

          if ((s[3] == 0x9f || (s[3] >= 0xa3 && s[3] <= 0xaa)))
            return 0;
        }
        if (s[2] == 0x8c)
        {
          if (((s[3] >= 0x82 && s[3] <= 0xba) || s[3] >= 0xbd))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x81)
               || (s[3] >= 0xbb && s[3] <= 0xbc)))
            return 0;
        }
        if (s[2] == 0x8d)
        {
          if (((s[3] >= 0x81 && s[3] <= 0xa5) || (s[3] >= 0xad && s[3] <= 0xaf)
               || s[3] >= 0xb5))
            return 1;

          if ((s[3] == 0x80 || (s[3] >= 0xa6 && s[3] <= 0xac)
               || (s[3] >= 0xb0 && s[3] <= 0xb4)))
            return 0;
        }
        if (s[2] == 0x8e)
        {
          if ((s[3] >= 0x80 && s[3] <= 0xba))
            return 1;

          if (s[3] >= 0xbb)
            return 0;
        }
        if (s[2] == 0x8f)
        {
          if (((s[3] >= 0x81 && s[3] <= 0x8d) || s[3] == 0x8f || s[3] == 0x91
               || (s[3] >= 0x93 && s[3] <= 0xa0) || s[3] >= 0xa3))
            return 1;

          if ((s[3] == 0x80 || s[3] == 0x8e || s[3] == 0x90 || s[3] == 0x92
               || (s[3] >= 0xa1 && s[3] <= 0xa2)))
            return 0;
        }
        if (s[2] == 0x90)
        {
          if ((s[3] >= 0x80 && s[3] <= 0xb7))
            return 1;

          if (s[3] >= 0xb8)
            return 0;
        }
        if (s[2] == 0x91)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x81) || s[3] == 0x85
               || (s[3] >= 0x87 && s[3] <= 0x9d) || s[3] >= 0x9f))
            return 1;

          if (((s[3] >= 0x82 && s[3] <= 0x84) || s[3] == 0x86 || s[3] == 0x9e))
            return 0;
        }
        if (s[2] == 0x92)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb2) || s[3] == 0xb9
               || (s[3] >= 0xbb && s[3] <= 0xbe)))
            return 1;

          if (((s[3] >= 0xb3 && s[3] <= 0xb8) || s[3] == 0xba || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0x93)
        {
          if ((s[3] == 0x81 || s[3] >= 0x84))
            return 1;

          if ((s[3] == 0x80 || (s[3] >= 0x82 && s[3] <= 0x83)))
            return 0;
        }
        if (s[2] == 0x96)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb1) || (s[3] >= 0xb6 && s[3] <= 0xbb)
               || s[3] == 0xbe))
            return 1;

          if (((s[3] >= 0xb2 && s[3] <= 0xb5) || (s[3] >= 0xbc && s[3] <= 0xbd)
               || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0x97)
        {
          if (((s[3] >= 0x81 && s[3] <= 0x9b) || s[3] >= 0x9e))
            return 1;

          if ((s[3] == 0x80 || (s[3] >= 0x9c && s[3] <= 0x9d)))
            return 0;
        }
        if (s[2] == 0x98)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb2) || (s[3] >= 0xbb && s[3] <= 0xbc)
               || s[3] == 0xbe))
            return 1;

          if (((s[3] >= 0xb3 && s[3] <= 0xba) || s[3] == 0xbd || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0x99)
        {
          if (s[3] >= 0x81)
            return 1;

          if (s[3] == 0x80)
            return 0;
        }
        if (s[2] == 0x9a)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaa) || s[3] == 0xac
               || (s[3] >= 0xae && s[3] <= 0xaf) || s[3] == 0xb6
               || s[3] >= 0xb8))
            return 1;

          if ((s[3] == 0xab || s[3] == 0xad || (s[3] >= 0xb0 && s[3] <= 0xb5)
               || s[3] == 0xb7))
            return 0;
        }
        if (s[2] == 0x9c)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9c) || s[3] == 0x9e
               || (s[3] >= 0xa0 && s[3] <= 0xa1) || s[3] == 0xa6
               || s[3] >= 0xac))
            return 1;

          if ((s[3] == 0x9d || s[3] == 0x9f || (s[3] >= 0xa2 && s[3] <= 0xa5)
               || (s[3] >= 0xa7 && s[3] <= 0xab)))
            return 0;
        }
        if (s[2] == 0xa0)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xae) || s[3] == 0xb8 || s[3] >= 0xbb))
            return 1;

          if (((s[3] >= 0xaf && s[3] <= 0xb7)
               || (s[3] >= 0xb9 && s[3] <= 0xba)))
            return 0;
        }
        if (s[2] == 0xa4)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xba) || s[3] == 0xbd || s[3] == 0xbf))
            return 1;

          if (((s[3] >= 0xbb && s[3] <= 0xbc) || s[3] == 0xbe))
            return 0;
        }
        if (s[2] == 0xa5)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x82) || s[3] >= 0x84))
            return 1;

          if (s[3] == 0x83)
            return 0;
        }
        if (s[2] == 0xa7)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x93) || (s[3] >= 0x98 && s[3] <= 0x99)
               || (s[3] >= 0x9c && s[3] <= 0x9f) || s[3] >= 0xa1))
            return 1;

          if (((s[3] >= 0x94 && s[3] <= 0x97) || (s[3] >= 0x9a && s[3] <= 0x9b)
               || s[3] == 0xa0))
            return 0;
        }
        if (s[2] == 0xa8)
        {
          if ((s[3] == 0x80 || (s[3] >= 0x8b && s[3] <= 0xb2)
               || (s[3] >= 0xb9 && s[3] <= 0xba) || s[3] == 0xbf))
            return 1;

          if (((s[3] >= 0x81 && s[3] <= 0x8a) || (s[3] >= 0xb3 && s[3] <= 0xb8)
               || (s[3] >= 0xbb && s[3] <= 0xbe)))
            return 0;
        }
        if (s[2] == 0xa9)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x86) || (s[3] >= 0x88 && s[3] <= 0x90)
               || (s[3] >= 0x97 && s[3] <= 0x98) || s[3] >= 0x9c))
            return 1;

          if ((s[3] == 0x87 || (s[3] >= 0x91 && s[3] <= 0x96)
               || (s[3] >= 0x99 && s[3] <= 0x9b)))
            return 0;
        }
        if (s[2] == 0xaa)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x89) || s[3] == 0x97 || s[3] >= 0x9a))
            return 1;

          if (((s[3] >= 0x8a && s[3] <= 0x96)
               || (s[3] >= 0x98 && s[3] <= 0x99)))
            return 0;
        }
        if (s[2] == 0xad)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9f) || s[3] == 0xa1 || s[3] == 0xa5
               || s[3] >= 0xa7))
            return 1;

          if ((s[3] == 0xa0 || (s[3] >= 0xa2 && s[3] <= 0xa4) || s[3] == 0xa6))
            return 0;
        }
        if (s[2] == 0xb0)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] == 0xb7 || s[3] == 0xbe))
            return 1;

          if (((s[3] >= 0xb0 && s[3] <= 0xb6) || (s[3] >= 0xb8 && s[3] <= 0xbd)
               || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0xb2)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x91) || (s[3] >= 0xa8 && s[3] <= 0xa9)
               || s[3] == 0xb1 || s[3] == 0xb4 || s[3] >= 0xb7))
            return 1;

          if (((s[3] >= 0x92 && s[3] <= 0xa7) || (s[3] >= 0xaa && s[3] <= 0xb0)
               || (s[3] >= 0xb2 && s[3] <= 0xb3)
               || (s[3] >= 0xb5 && s[3] <= 0xb6)))
            return 0;
        }
        if (s[2] == 0xb4)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb0) || (s[3] >= 0xb7 && s[3] <= 0xb9)
               || s[3] == 0xbb || s[3] == 0xbe))
            return 1;

          if (((s[3] >= 0xb1 && s[3] <= 0xb6) || s[3] == 0xba
               || (s[3] >= 0xbc && s[3] <= 0xbd) || s[3] == 0xbf))
            return 0;
        }
        if (s[2] == 0xb5)
        {
          if ((s[3] == 0x86 || s[3] >= 0x88))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x85) || s[3] == 0x87))
            return 0;
        }
        if (s[2] == 0xb6)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8f) || (s[3] >= 0x92 && s[3] <= 0x94)
               || s[3] == 0x96 || s[3] >= 0x98))
            return 1;

          if (((s[3] >= 0x90 && s[3] <= 0x91) || s[3] == 0x95 || s[3] == 0x97))
            return 0;
        }
        if (s[2] == 0xb7)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] >= 0xb1))
            return 1;

          if (s[3] == 0xb0)
            return 0;
        }
        if (s[2] == 0xbb)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xb2) || s[3] >= 0xb5))
            return 1;

          if ((s[3] >= 0xb3 && s[3] <= 0xb4))
            return 0;
        }
        if (s[2] == 0xbc)
        {
          if (((s[3] >= 0x82 && s[3] <= 0xb5) || s[3] >= 0xbb))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x81)
               || (s[3] >= 0xb6 && s[3] <= 0xba)))
            return 0;
        }
        if (s[2] == 0xbd)
        {
          if ((s[3] == 0x81 || (s[3] >= 0x83 && s[3] <= 0x99) || s[3] >= 0x9b))
            return 1;

          if ((s[3] == 0x80 || s[3] == 0x82 || s[3] == 0x9a))
            return 0;
        }
      }
      if (s[1] == 0x93)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x8f) || s[2] >= 0x92))
          return 1;

        if (s[2] == 0x90)
        {
          if ((s[3] >= 0x80 && s[3] <= 0xaf))
            return 1;

          if (s[3] >= 0xb0)
            return 0;
        }
        if (s[2] == 0x91)
        {
          if (((s[3] >= 0x81 && s[3] <= 0x86) || s[3] >= 0x96))
            return 1;

          if ((s[3] == 0x80 || (s[3] >= 0x87 && s[3] <= 0x95)))
            return 0;
        }
      }
      if (s[1] == 0x96)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x83) || (s[2] >= 0x85 && s[2] <= 0xaa)
             || (s[2] >= 0xad && s[2] <= 0xbc)))
          return 1;

        if (s[2] == 0x84)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9d) || (s[3] >= 0xaa && s[3] <= 0xac)
               || s[3] >= 0xb0))
            return 1;

          if (((s[3] >= 0x9e && s[3] <= 0xa9)
               || (s[3] >= 0xad && s[3] <= 0xaf)))
            return 0;
        }
        if (s[2] == 0xab)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] >= 0xb5))
            return 1;

          if ((s[3] >= 0xb0 && s[3] <= 0xb4))
            return 0;
        }
        if (s[2] == 0xac)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] >= 0xb7))
            return 1;

          if ((s[3] >= 0xb0 && s[3] <= 0xb6))
            return 0;
        }
        if (s[2] == 0xbd)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8e) || s[3] >= 0x90))
            return 1;

          if (s[3] == 0x8f)
            return 0;
        }
        if (s[2] == 0xbe)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8e) || s[3] >= 0x93))
            return 1;

          if ((s[3] >= 0x8f && s[3] <= 0x92))
            return 0;
        }
        if (s[2] == 0xbf)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9f) || (s[3] >= 0xa5 && s[3] <= 0xaf)
               || s[3] >= 0xb7))
            return 1;

          if (((s[3] >= 0xa0 && s[3] <= 0xa3)
               || (s[3] >= 0xb0 && s[3] <= 0xb6)))
            return 2;

          if (s[3] == 0xa4)
            return 0;
        }
      }
      if ((s[1] == 0x97 || (s[1] >= 0xa0 && s[1] <= 0xae)
           || (s[1] >= 0xb0 && s[1] <= 0xbe)))
        return 2;

      if (s[1] == 0x98)
      {
        if (s[2] == 0xb5)
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0xb2) || s[2] == 0xb6 || s[2] >= 0xb8))
          return 2;

        if (s[2] == 0xb3)
        {
          if ((s[3] >= 0x9b && s[3] <= 0xbe))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x9a) || s[3] == 0xbf))
            return 2;
        }
        if (s[2] == 0xb4)
        {
          if (s[3] >= 0xa1)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xa0))
            return 2;
        }
        if (s[2] == 0xb7)
        {
          if (s[3] >= 0xb3)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xb2))
            return 2;
        }
      }
      if (s[1] == 0x99)
      {
        if (s[2] >= 0x88)
          return 1;

        if ((s[2] >= 0x80 && s[2] <= 0x85))
          return 2;

        if (s[2] == 0x86)
        {
          if ((s[3] >= 0x92 && s[3] <= 0x9f))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x91) || s[3] >= 0xa0))
            return 2;
        }
        if (s[2] == 0x87)
        {
          if (s[3] >= 0x93)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0x92))
            return 2;
        }
      }
      if (s[1] == 0x9a)
      {
        if ((s[2] >= 0x80 && s[2] <= 0xbe))
          return 1;

        if (s[2] == 0xbf)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] == 0xb4 || s[3] == 0xbc
               || s[3] == 0xbf))
            return 1;

          if (((s[3] >= 0xb0 && s[3] <= 0xb3) || (s[3] >= 0xb5 && s[3] <= 0xbb)
               || (s[3] >= 0xbd && s[3] <= 0xbe)))
            return 2;
        }
      }
      if (s[1] == 0x9b)
      {
        if (((s[2] >= 0x8c && s[2] <= 0xb1) || s[2] >= 0xb3))
          return 1;

        if (((s[2] >= 0x80 && s[2] <= 0x83) || (s[2] >= 0x86 && s[2] <= 0x8a)))
          return 2;

        if (s[2] == 0x84)
        {
          if (((s[3] >= 0xa9 && s[3] <= 0xb1) || s[3] >= 0xb3))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xa8) || s[3] == 0xb2))
            return 2;
        }
        if (s[2] == 0x85)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8f) || (s[3] >= 0x93 && s[3] <= 0x94)
               || (s[3] >= 0x96 && s[3] <= 0xa3)
               || (s[3] >= 0xa9 && s[3] <= 0xaf)))
            return 1;

          if (((s[3] >= 0x90 && s[3] <= 0x92) || s[3] == 0x95
               || (s[3] >= 0xa4 && s[3] <= 0xa8) || s[3] >= 0xb0))
            return 2;
        }
        if (s[2] == 0x8b)
        {
          if (s[3] >= 0xbc)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xbb))
            return 2;
        }
        if (s[2] == 0xb2)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x9c) || s[3] == 0x9f || s[3] >= 0xa4))
            return 1;

          if (((s[3] >= 0x9d && s[3] <= 0x9e)
               || (s[3] >= 0xa0 && s[3] <= 0xa3)))
            return 0;
        }
      }
      if (s[1] == 0x9c)
      {
        if (((s[2] >= 0x80 && s[2] <= 0xbb) || s[2] >= 0xbe))
          return 1;

        if (s[2] == 0xbc)
        {
          if ((s[3] >= 0xae && s[3] <= 0xaf))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xad) || s[3] >= 0xb0))
            return 0;
        }
        if (s[2] == 0xbd)
        {
          if (s[3] >= 0x87)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0x86))
            return 0;
        }
      }
      if (s[1] == 0x9d)
      {
        if (((s[2] >= 0x80 && s[2] <= 0x83) || (s[2] >= 0x87 && s[2] <= 0x88)
             || (s[2] >= 0x8a && s[2] <= 0x8b) || (s[2] >= 0x8e && s[2] <= 0xa7)
             || s[2] >= 0xab))
          return 1;

        if (s[2] == 0x84)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa6) || s[3] >= 0xa9))
            return 1;

          if ((s[3] >= 0xa7 && s[3] <= 0xa8))
            return 0;
        }
        if (s[2] == 0x85)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa6)
               || (s[3] >= 0xaa && s[3] <= 0xb2)))
            return 1;

          if (((s[3] >= 0xa7 && s[3] <= 0xa9) || s[3] >= 0xb3))
            return 0;
        }
        if (s[2] == 0x86)
        {
          if (((s[3] >= 0x83 && s[3] <= 0x84) || (s[3] >= 0x8c && s[3] <= 0xa9)
               || s[3] >= 0xae))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x82) || (s[3] >= 0x85 && s[3] <= 0x8b)
               || (s[3] >= 0xaa && s[3] <= 0xad)))
            return 0;
        }
        if (s[2] == 0x89)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x81) || (s[3] >= 0x85 && s[3] <= 0x9a)
               || s[3] >= 0x9d))
            return 1;

          if (((s[3] >= 0x82 && s[3] <= 0x84)
               || (s[3] >= 0x9b && s[3] <= 0x9c)))
            return 0;
        }
        if (s[2] == 0x8c)
          return 2;

        if (s[2] == 0x8d)
        {
          if (((s[3] >= 0x97 && s[3] <= 0x9f) || s[3] >= 0xb7))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x96)
               || (s[3] >= 0xa0 && s[3] <= 0xb6)))
            return 2;
        }
        if (s[2] == 0xa8)
        {
          if ((s[3] >= 0xb7 && s[3] <= 0xba))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xb6) || s[3] >= 0xbb))
            return 0;
        }
        if (s[2] == 0xa9)
        {
          if (((s[3] >= 0xad && s[3] <= 0xb4) || s[3] >= 0xb6))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xac) || s[3] == 0xb5))
            return 0;
        }
        if (s[2] == 0xaa)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x83) || (s[3] >= 0x85 && s[3] <= 0x9a)
               || s[3] == 0xa0 || s[3] >= 0xb0))
            return 1;

          if ((s[3] == 0x84 || (s[3] >= 0x9b && s[3] <= 0x9f)
               || (s[3] >= 0xa1 && s[3] <= 0xaf)))
            return 0;
        }
      }
      if (s[1] == 0x9e)
      {
        if ((s[2] == 0x81 || s[2] == 0x83 || (s[2] >= 0x85 && s[2] <= 0x89)
             || (s[2] >= 0x8c && s[2] <= 0x92) || (s[2] >= 0x94 && s[2] <= 0x96)
             || (s[2] >= 0x98 && s[2] <= 0x9a) || (s[2] >= 0x9c && s[2] <= 0xa2)
             || s[2] == 0xa4 || s[2] >= 0xa6))
          return 1;

        if (s[2] == 0x80)
        {
          if ((s[3] == 0x87 || (s[3] >= 0x99 && s[3] <= 0x9a) || s[3] == 0xa2
               || s[3] == 0xa5 || s[3] >= 0xab))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x86) || (s[3] >= 0x88 && s[3] <= 0x98)
               || (s[3] >= 0x9b && s[3] <= 0xa1)
               || (s[3] >= 0xa3 && s[3] <= 0xa4)
               || (s[3] >= 0xa6 && s[3] <= 0xaa)))
            return 0;
        }
        if (s[2] == 0x82)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8e) || s[3] >= 0x90))
            return 1;

          if (s[3] == 0x8f)
            return 0;
        }
        if (s[2] == 0x84)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] >= 0xb7))
            return 1;

          if ((s[3] >= 0xb0 && s[3] <= 0xb6))
            return 0;
        }
        if (s[2] == 0x8a)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xad) || s[3] >= 0xaf))
            return 1;

          if (s[3] == 0xae)
            return 0;
        }
        if ((s[2] == 0x8b || s[2] == 0x93))
        {
          if (((s[3] >= 0x80 && s[3] <= 0xab) || s[3] >= 0xb0))
            return 1;

          if ((s[3] >= 0xac && s[3] <= 0xaf))
            return 0;
        }
        if (s[2] == 0x97)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xad) || s[3] >= 0xb0))
            return 1;

          if ((s[3] >= 0xae && s[3] <= 0xaf))
            return 0;
        }
        if (s[2] == 0x9b)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xa2) || (s[3] >= 0xa4 && s[3] <= 0xa5)
               || (s[3] >= 0xa7 && s[3] <= 0xad)
               || (s[3] >= 0xb0 && s[3] <= 0xb4) || s[3] >= 0xb6))
            return 1;

          if ((s[3] == 0xa3 || s[3] == 0xa6 || (s[3] >= 0xae && s[3] <= 0xaf)
               || s[3] == 0xb5))
            return 0;
        }
        if (s[2] == 0xa3)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8f) || s[3] >= 0x97))
            return 1;

          if ((s[3] >= 0x90 && s[3] <= 0x96))
            return 0;
        }
        if (s[2] == 0xa5)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x83) || s[3] >= 0x8b))
            return 1;

          if ((s[3] >= 0x84 && s[3] <= 0x8a))
            return 0;
        }
      }
      if (s[1] == 0x9f)
      {
        if (((s[2] >= 0x81 && s[2] <= 0x82) || (s[2] >= 0x84 && s[2] <= 0x85)
             || (s[2] >= 0x8a && s[2] <= 0x8b) || (s[2] >= 0x9c && s[2] <= 0x9e)
             || (s[2] >= 0xa0 && s[2] <= 0xa3) || s[2] == 0xa8 || s[2] >= 0xac))
          return 1;

        if (s[2] == 0x80)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x83) || s[3] >= 0x85))
            return 1;

          if (s[3] == 0x84)
            return 2;
        }
        if (s[2] == 0x83)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8e) || s[3] >= 0x90))
            return 1;

          if (s[3] == 0x8f)
            return 2;
        }
        if (s[2] == 0x86)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8d) || (s[3] >= 0x8f && s[3] <= 0x90)
               || (s[3] >= 0x9b && s[3] <= 0xad) || s[3] >= 0xaf))
            return 1;

          if ((s[3] == 0x8e || (s[3] >= 0x91 && s[3] <= 0x9a) || s[3] == 0xae))
            return 2;
        }
        if (s[2] == 0x87)
        {
          if ((s[3] >= 0x80 && s[3] <= 0xa5))
            return 1;

          if (s[3] >= 0xa6)
            return 2;
        }
        if (s[2] == 0x88)
        {
          if (((s[3] >= 0x83 && s[3] <= 0x8f) || s[3] >= 0xbc))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x82)
               || (s[3] >= 0x90 && s[3] <= 0xbb)))
            return 2;
        }
        if (s[2] == 0x89)
        {
          if (((s[3] >= 0x89 && s[3] <= 0x8f) || (s[3] >= 0x92 && s[3] <= 0x9f)
               || s[3] >= 0xa6))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x88) || (s[3] >= 0x90 && s[3] <= 0x91)
               || (s[3] >= 0xa0 && s[3] <= 0xa5)))
            return 2;
        }
        if (s[2] == 0x8c)
        {
          if (((s[3] >= 0xa1 && s[3] <= 0xac) || s[3] == 0xb6))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xa0) || (s[3] >= 0xad && s[3] <= 0xb5)
               || s[3] >= 0xb7))
            return 2;
        }
        if (s[2] == 0x8d)
        {
          if (s[3] == 0xbd)
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xbc) || s[3] >= 0xbe))
            return 2;
        }
        if (s[2] == 0x8e)
        {
          if ((s[3] >= 0x94 && s[3] <= 0x9f))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x93) || s[3] >= 0xa0))
            return 2;
        }
        if (s[2] == 0x8f)
        {
          if (((s[3] >= 0x8b && s[3] <= 0x8e) || (s[3] >= 0x94 && s[3] <= 0x9f)
               || (s[3] >= 0xb1 && s[3] <= 0xb3)
               || (s[3] >= 0xb5 && s[3] <= 0xb7)))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x8a) || (s[3] >= 0x8f && s[3] <= 0x93)
               || (s[3] >= 0xa0 && s[3] <= 0xb0) || s[3] == 0xb4
               || (s[3] >= 0xb8 && s[3] <= 0xba)))
            return 2;

          if (s[3] >= 0xbb)
            return 0;
        }
        if (s[2] == 0x90)
        {
          if (s[3] == 0xbf)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xbe))
            return 2;
        }
        if (s[2] == 0x91)
        {
          if (s[3] == 0x81)
            return 1;

          if ((s[3] == 0x80 || s[3] >= 0x82))
            return 2;
        }
        if ((s[2] == 0x92 || s[2] == 0x98 || s[2] == 0x9a
             || (s[2] >= 0xa6 && s[2] <= 0xa7) || s[2] == 0xaa))
          return 2;

        if (s[2] == 0x93)
        {
          if ((s[3] >= 0xbd && s[3] <= 0xbe))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0xbc) || s[3] == 0xbf))
            return 2;
        }
        if (s[2] == 0x94)
        {
          if (s[3] >= 0xbe)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xbd))
            return 2;
        }
        if (s[2] == 0x95)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8a) || s[3] == 0x8f
               || (s[3] >= 0xa8 && s[3] <= 0xb9) || s[3] >= 0xbb))
            return 1;

          if (((s[3] >= 0x8b && s[3] <= 0x8e) || (s[3] >= 0x90 && s[3] <= 0xa7)
               || s[3] == 0xba))
            return 2;
        }
        if (s[2] == 0x96)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x94) || (s[3] >= 0x97 && s[3] <= 0xa3)
               || s[3] >= 0xa5))
            return 1;

          if (((s[3] >= 0x95 && s[3] <= 0x96) || s[3] == 0xa4))
            return 2;
        }
        if (s[2] == 0x97)
        {
          if ((s[3] >= 0x80 && s[3] <= 0xba))
            return 1;

          if (s[3] >= 0xbb)
            return 2;
        }
        if (s[2] == 0x99)
        {
          if (s[3] >= 0x90)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0x8f))
            return 2;
        }
        if (s[2] == 0x9b)
        {
          if (((s[3] >= 0x86 && s[3] <= 0x8b) || (s[3] >= 0x8d && s[3] <= 0x8f)
               || (s[3] >= 0x93 && s[3] <= 0x94)
               || (s[3] >= 0x9a && s[3] <= 0x9b)
               || (s[3] >= 0xa0 && s[3] <= 0xaa)))
            return 1;

          if (((s[3] >= 0xad && s[3] <= 0xb3) || s[3] >= 0xbd))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x85) || s[3] == 0x8c
               || (s[3] >= 0x90 && s[3] <= 0x92)
               || (s[3] >= 0x95 && s[3] <= 0x99)
               || (s[3] >= 0x9c && s[3] <= 0x9f)
               || (s[3] >= 0xab && s[3] <= 0xac)))
            return 2;

          if ((s[3] >= 0xb4 && s[3] <= 0xbc))
            return 2;
        }
        if (s[2] == 0x9f)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x99) || (s[3] >= 0x9b && s[3] <= 0x9f)
               || (s[3] >= 0xac && s[3] <= 0xaf) || s[3] >= 0xb1))
            return 1;

          if ((s[3] == 0x9a || (s[3] >= 0xa0 && s[3] <= 0xab) || s[3] == 0xb0))
            return 2;
        }
        if (s[2] == 0xa4)
        {
          if (((s[3] >= 0x80 && s[3] <= 0x8b) || s[3] == 0xbb))
            return 1;

          if (((s[3] >= 0x8c && s[3] <= 0xba) || s[3] >= 0xbc))
            return 2;
        }
        if (s[2] == 0xa5)
        {
          if (s[3] == 0x86)
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x85) || s[3] >= 0x87))
            return 2;
        }
        if (s[2] == 0xa9)
        {
          if (((s[3] >= 0x80 && s[3] <= 0xaf) || s[3] >= 0xbd))
            return 1;

          if ((s[3] >= 0xb0 && s[3] <= 0xbc))
            return 2;
        }
        if (s[2] == 0xab)
        {
          if ((s[3] == 0x87 || (s[3] >= 0x89 && s[3] <= 0x8b) || s[3] == 0x9e
               || (s[3] >= 0xac && s[3] <= 0xae) || s[3] >= 0xbb))
            return 1;

          if (((s[3] >= 0x80 && s[3] <= 0x86) || s[3] == 0x88
               || (s[3] >= 0x8c && s[3] <= 0x9d)
               || (s[3] >= 0x9f && s[3] <= 0xab)
               || (s[3] >= 0xaf && s[3] <= 0xba)))
            return 2;
        }
      }
      if ((s[1] == 0xaf || s[1] == 0xbf))
      {
        if ((s[2] >= 0x80 && s[2] <= 0xbe))
          return 2;

        if (s[2] == 0xbf)
        {
          if (s[3] >= 0xbe)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xbd))
            return 2;
        }
      }
    }

    /* U+c0000..U+fffff */
    /* """""""""""""""" */
    if (s[0] == 0xf3)
    {
      if (((s[1] >= 0x80 && s[1] <= 0x9f) || s[1] >= 0xa1))
        return 1;

      if (s[1] == 0xa0)
      {
        if (((s[2] >= 0x82 && s[2] <= 0x83) || s[2] >= 0x88))
          return 1;

        if (s[2] == 0x80)
        {
          if ((s[3] == 0x80 || (s[3] >= 0x82 && s[3] <= 0x9f)))
            return 1;

          if ((s[3] == 0x81 || s[3] >= 0xa0))
            return 0;
        }
        if ((s[2] == 0x81 || (s[2] >= 0x84 && s[2] <= 0x86)))
          return 0;

        if (s[2] == 0x87)
        {
          if (s[3] >= 0xb0)
            return 1;

          if ((s[3] >= 0x80 && s[3] <= 0xaf))
            return 0;
        }
      }
    }
  }
  return 1;
}

/* =================================================== */
/* Check if s contain the ZERO WIDTH JOINER character. */
/* Returns 1 if yes else returns 0.                    */
/* =================================================== */
static int
utf8_is_zwj(const unsigned char *s, size_t n)
{
  return n == 3 && s[0] == 0xe2 && s[1] == 0x80 && s[2] == 0x8d;
}

/* ======================================================= */
/* Check if s contain the VARIATION SELECTOR-16 character. */
/* Returns 1 if yes else returns 0.                        */
/* ======================================================= */
static int
utf8_is_vs16(const unsigned char *s, size_t n)
{
  return n == 3 && s[0] == 0xef && s[1] == 0xb8 && s[2] == 0x8f;
}

/* ================================================================ */
/* Check if s contain a REGIONAL INDICATOR SYMBOL LETTER character. */
/* Returns 1 if yes else returns 0.                                 */
/* ================================================================ */
static int
utf8_is_ri(const unsigned char *s, size_t n)
{
  return n == 4 && s[0] == 0xf0 && s[1] == 0x9f && s[2] == 0x87 && s[3] >= 0xa6
         && s[3] <= 0xbf;
}

int
utf8_is_keycap_mark(const unsigned char *s, size_t n)
{
  return n == 3 && s[0] == 0xe2 && s[1] == 0x83 && s[2] == 0xa3;
}

int
utf8_is_extend(const unsigned char *s, size_t n)
{
  return (n == 2 && s[0] == 0xcc) || (n == 2 && s[0] == 0xcd && s[1] <= 0xaf)
         || utf8_is_vs16(s, n) || utf8_is_keycap_mark(s, n);
}

int
utf8_is_extended_pictographic(const unsigned char *s, size_t n)
{
  if (n == 2)
  {
    if (s[0] == 0xc2)
      return s[1] == 0xa9 || s[1] == 0xae;
    return 0;
  }
  else if (n == 3)
  {
    if (s[0] == 0xe2)
    {
      if (s[1] == 0x80)
        return s[2] == 0xbc;
      else if (s[1] == 0x81)
        return s[2] == 0x89;
      else if (s[1] == 0x84)
        return s[2] == 0xa2 || s[2] == 0xb9;
      else if (s[1] == 0x86)
        return (s[2] >= 0x94 && s[2] <= 0x99) || (s[2] >= 0xa9 && s[2] <= 0xaa);
      else if (s[1] == 0x8c)
        return (s[2] >= 0x9a && s[2] <= 0x9b) || s[2] == 0xa8;
      else if (s[1] == 0x8f)
        return s[2] == 0x8f || (s[2] >= 0xa9 && s[2] <= 0xb3)
               || (s[2] >= 0xb8 && s[2] <= 0xba);
      else if (s[1] == 0x93)
        return s[2] == 0x82;
      else if (s[1] == 0x96)
        return (s[2] >= 0xaa && s[2] <= 0xab) || s[2] == 0xb6;
      else if (s[1] == 0x97)
        return s[2] == 0x80 || (s[2] >= 0xbb && s[2] <= 0xbe);
      else if (s[1] == 0x98)
        return (s[2] >= 0x80 && s[2] <= 0x84) || s[2] == 0x8e || s[2] == 0x91
               || (s[2] >= 0x94 && s[2] <= 0x95) || s[2] == 0x98 || s[2] == 0x9d
               || s[2] == 0xa0 || (s[2] >= 0xa2 && s[2] <= 0xa3) || s[2] == 0xa6
               || s[2] == 0xaa || (s[2] >= 0xae && s[2] <= 0xaf)
               || (s[2] >= 0xb8 && s[2] <= 0xba);
      else if (s[1] == 0x99)
        return s[2] == 0x80 || s[2] == 0x82 || (s[2] >= 0x88 && s[2] <= 0x93)
               || (s[2] >= 0x9f && s[2] <= 0xa0) || s[2] == 0xa3
               || (s[2] >= 0xa5 && s[2] <= 0xa6) || s[2] == 0xa8 || s[2] == 0xbb
               || (s[2] >= 0xbe && s[2] <= 0xbf);
      else if (s[1] == 0x9a)
        return (s[2] >= 0x92 && s[2] <= 0x97) || s[2] == 0x99
               || (s[2] >= 0x9b && s[2] <= 0x9c)
               || (s[2] >= 0xa0 && s[2] <= 0xa1) || s[2] == 0xa7
               || (s[2] >= 0xaa && s[2] <= 0xab)
               || (s[2] >= 0xb0 && s[2] <= 0xb1)
               || (s[2] >= 0xbd && s[2] <= 0xbe);
      else if (s[1] == 0x9b)
        return (s[2] >= 0x84 && s[2] <= 0x85) || s[2] == 0x88
               || (s[2] >= 0x8e && s[2] <= 0x8f) || s[2] == 0x91
               || (s[2] >= 0x93 && s[2] <= 0x94)
               || (s[2] >= 0xa9 && s[2] <= 0xaa)
               || (s[2] >= 0xb0 && s[2] <= 0xb5)
               || (s[2] >= 0xb7 && s[2] <= 0xba) || s[2] == 0xbd;
      else if (s[1] == 0x9c)
        return s[2] == 0x82 || s[2] == 0x85 || (s[2] >= 0x88 && s[2] <= 0x8d)
               || s[2] == 0x8f || s[2] == 0x92 || s[2] == 0x94 || s[2] == 0x96
               || s[2] == 0x9d || s[2] == 0xa1 || s[2] == 0xa8
               || (s[2] >= 0xb3 && s[2] <= 0xb4);
      else if (s[1] == 0x9d)
        return s[2] == 0x84 || s[2] == 0x87 || s[2] == 0x8c || s[2] == 0x8e
               || (s[2] >= 0x93 && s[2] <= 0x95) || s[2] == 0x97
               || (s[2] >= 0xa3 && s[2] <= 0xa4);
      else if (s[1] == 0x9e)
        return (s[2] >= 0x95 && s[2] <= 0x97) || s[2] == 0xa1 || s[2] == 0xb0
               || s[2] == 0xbf;
      else if (s[1] == 0xa4)
        return (s[2] >= 0xb4 && s[2] <= 0xb5);
      else if (s[1] == 0xac)
        return (s[2] >= 0x85 && s[2] <= 0x87) || (s[2] >= 0x9b && s[2] <= 0x9c);
      else if (s[1] == 0xad)
        return s[2] == 0x90 || s[2] == 0x95;
      return 0;
    }
    else if (s[0] == 0xe3)
    {
      if (s[1] == 0x80)
        return s[2] == 0xb0 || s[2] == 0xbd;
      else if (s[1] == 0x8a)
        return s[2] == 0x97 || s[2] == 0x99;
      return 0;
    }
    return 0;
  }
  else if (n == 4)
  {
    if (s[0] == 0xf0)
    {
      if (s[1] == 0x9f)
      {
        if (s[2] == 0x80)
          return s[3] == 0x84 || (s[3] >= 0xac && s[3] <= 0xaf);
        else if (s[2] == 0x82)
          return (s[3] >= 0x94 && s[3] <= 0x9f)
                 || (s[3] >= 0xaf && s[3] <= 0xb0);
        else if (s[2] == 0x83)
          return s[3] == 0x80 || (s[3] >= 0x8f && s[3] <= 0x90)
                 || (s[3] >= 0xb6 && s[3] <= 0xbf);
        else if (s[2] == 0x85)
          return (s[3] >= 0xb0 && s[3] <= 0xb1)
                 || (s[3] >= 0xbe && s[3] <= 0xbf);
        else if (s[2] == 0x86)
          return s[3] == 0x8e || (s[3] >= 0x91 && s[3] <= 0x9a)
                 || (s[3] >= 0xae && s[3] <= 0xbf);
        else if (s[2] == 0x87)
          return (s[3] >= 0x80 && s[3] <= 0xa5);
        else if (s[2] == 0x88)
          return (s[3] >= 0x81 && s[3] <= 0x8f) || s[3] == 0x9a || s[3] == 0xaf
                 || (s[3] >= 0xb2 && s[3] <= 0xba)
                 || (s[3] >= 0xbc && s[3] <= 0xbf);
        else if (s[2] == 0x89)
          return (s[3] >= 0x89 && s[3] <= 0x9f)
                 || (s[3] >= 0xa6 && s[3] <= 0xbf);
        else if ((s[2] >= 0x8a && s[2] <= 0x8b) || s[2] == 0x8d
                 || (s[2] >= 0x90 && s[2] <= 0x92) || s[2] == 0x98
                 || s[2] == 0x9a || (s[2] >= 0xa6 && s[2] <= 0xa7)
                 || (s[2] >= 0xaa && s[2] <= 0xab)
                 || (s[2] >= 0xb0 && s[2] <= 0xbe))
          return (s[3] >= 0x80 && s[3] <= 0xbf);
        else if (s[2] == 0x8c)
          return (s[3] >= 0x80 && s[3] <= 0xa1)
                 || (s[3] >= 0xa4 && s[3] <= 0xbf);
        else if (s[2] == 0x8e)
          return (s[3] >= 0x80 && s[3] <= 0x93)
                 || (s[3] >= 0x96 && s[3] <= 0x97)
                 || (s[3] >= 0x99 && s[3] <= 0x9b)
                 || (s[3] >= 0x9e && s[3] <= 0xbf);
        else if (s[2] == 0x8f)
          return (s[3] >= 0x80 && s[3] <= 0xb0)
                 || (s[3] >= 0xb3 && s[3] <= 0xb5)
                 || (s[3] >= 0xb7 && s[3] <= 0xba);
        else if (s[2] == 0x93)
          return (s[3] >= 0x80 && s[3] <= 0xbd) || s[3] == 0xbf;
        else if (s[2] == 0x94 || s[2] == 0xbf)
          return (s[3] >= 0x80 && s[3] <= 0xbd);
        else if (s[2] == 0x95)
          return (s[3] >= 0x89 && s[3] <= 0x8e)
                 || (s[3] >= 0x90 && s[3] <= 0xa7)
                 || (s[3] >= 0xaf && s[3] <= 0xb0)
                 || (s[3] >= 0xb3 && s[3] <= 0xba);
        else if (s[2] == 0x96)
          return s[3] == 0x87 || (s[3] >= 0x8a && s[3] <= 0x8d) || s[3] == 0x90
                 || (s[3] >= 0x95 && s[3] <= 0x96)
                 || (s[3] >= 0xa4 && s[3] <= 0xa5) || s[3] == 0xa8
                 || (s[3] >= 0xb1 && s[3] <= 0xb2) || s[3] == 0xbc;
        else if (s[2] == 0x97)
          return (s[3] >= 0x82 && s[3] <= 0x84)
                 || (s[3] >= 0x91 && s[3] <= 0x93)
                 || (s[3] >= 0x9c && s[3] <= 0x9e) || s[3] == 0xa1
                 || s[3] == 0xa3 || s[3] == 0xa8 || s[3] == 0xaf || s[3] == 0xb3
                 || (s[3] >= 0xba && s[3] <= 0xbf);
        else if (s[2] == 0x99)
          return (s[3] >= 0x80 && s[3] <= 0x8f);
        else if (s[2] == 0x9b)
          return (s[3] >= 0x80 && s[3] <= 0x85)
                 || (s[3] >= 0x8b && s[3] <= 0x92)
                 || (s[3] >= 0x95 && s[3] <= 0xa5) || s[3] == 0xa9
                 || (s[3] >= 0xab && s[3] <= 0xb0)
                 || (s[3] >= 0xb3 && s[3] <= 0xbf);
        else if (s[2] == 0x9f)
          return (s[3] >= 0x9a && s[3] <= 0xbf);
        else if (s[2] == 0xa0)
          return (s[3] >= 0x8c && s[3] <= 0x8f);
        else if (s[2] == 0xa1)
          return (s[3] >= 0x88 && s[3] <= 0x8f)
                 || (s[3] >= 0x9a && s[3] <= 0x9f);
        else if (s[2] == 0xa2)
          return (s[3] >= 0x88 && s[3] <= 0x8f)
                 || (s[3] >= 0xae && s[3] <= 0xaf)
                 || (s[3] >= 0xbc && s[3] <= 0xbf);
        else if (s[2] == 0xa3)
          return (s[3] >= 0x82 && s[3] <= 0x8f)
                 || (s[3] >= 0x99 && s[3] <= 0xbf);
        else if (s[2] == 0xa4)
          return (s[3] >= 0x8c && s[3] <= 0xba)
                 || (s[3] >= 0xbc && s[3] <= 0xbf);
        else if (s[2] == 0xa5)
          return (s[3] >= 0x80 && s[3] <= 0x85)
                 || (s[3] >= 0x87 && s[3] <= 0xbf);
        else if (s[2] == 0xa9)
          return (s[3] >= 0x98 && s[3] <= 0x9f)
                 || (s[3] >= 0xae && s[3] <= 0xbf);
        return 0;
      }
      return 0;
    }
    return 0;
  }
  return 0;
}

/* ====================================================================== */
/* Version of wcswidth which tries to support extended grapheme clusters. */
/* This version uses the hexadecimal representation of the characters.    */
/*                                                                        */
/* Grapheme clusters are merged when:                                     */
/*   - the current character has width 0,                                 */
/*   - the previous character is a ZWJ,                                   */
/*   - the current character is a ZWJ,                                    */
/*   - the character completes a Regional Indicator pair.                 */
/*                                                                        */
/* Grapheme clustering is evaluated only when necessary.                  */
/* The width of a grapheme cluster never exceeds 2 columns.               */
/*                                                                        */
/* s     is the start of the byte sequence to evaluate.                   */
/* limit is the maximum number of bytes to consider.                      */
/* ====================================================================== */
long
utf8_strwidth(const unsigned char *s, size_t limit)
{
  const unsigned char *last_base        = NULL;
  size_t               last_base_length = 0;
  long                 total            = 0;
  int                  cluster          = 0;
  int                  prev_ri          = 0;
  int                  prev_paired      = 0;
  int                  zwj_pending      = 0;

  if (s == NULL)
    return 0;

  while (limit && *s)
  {
    size_t n = utf8_get_length(*s);
    int    ri;
    int    zwj;
    int    vs16;
    int    keycap;
    int    extend;
    int    ri_pair;
    int    width;
    int    ep_after_zwj;
    int    join;

    if (n > limit)
      break;

    ri     = utf8_is_ri(s, n);
    zwj    = utf8_is_zwj(s, n);
    vs16   = utf8_is_vs16(s, n);
    keycap = utf8_is_keycap_mark(s, n);
    extend = utf8_is_extend(s, n);

    ri_pair = ri && prev_ri && !prev_paired;

    width = ri ? 2 : utf8_width(s);

    ep_after_zwj = zwj_pending && utf8_is_extended_pictographic(s, n);

    join = cluster && (extend || zwj || ri_pair || ep_after_zwj);

    if (join)
    {
      if (width == 2)
        cluster = 2;
    }
    else
    {
      total += cluster;
      cluster = width;
    }

    if (cluster == 1 && (vs16 || keycap))
      cluster = 2;

    if (zwj)
    {
      zwj_pending = last_base != NULL
                    && utf8_is_extended_pictographic(last_base,
                                                     last_base_length);
    }
    else if (extend)
    {
      zwj_pending = 0;
    }
    else if (width == 0)
    {
      zwj_pending      = 0;
      last_base        = NULL;
      last_base_length = 0;
    }
    else
    {
      zwj_pending      = 0;
      last_base        = s;
      last_base_length = n;
    }

    prev_paired = ri_pair;
    prev_ri     = ri;
    s += n;
    limit -= n;
  }

  return total + cluster;
}
