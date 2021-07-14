/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html.           */
/* ########################################################### */

/* ************************************************************************* */
/* Custom fgetc/ungetc implementation able to unget more than one character. */
/* ************************************************************************* */

#include <stdio.h>
#include "fgetc.h"

enum
{
  GETC_BUFF_SIZE = 16
};

static unsigned char getc_buffer[GETC_BUFF_SIZE] = { '\0' };

static long next_buffer_pos = 0; /* Next free position in the getc buffer. */

/* ======================================== */
/* Gets a (possibly pushed-back) character. */
/* ======================================== */
int
my_fgetc(FILE * input)
{
  return (next_buffer_pos > 0) ? getc_buffer[--next_buffer_pos] : fgetc(input);
}

/* =============================== */
/* Pushes character back on input. */
/* =============================== */
int
my_ungetc(int c, FILE * input)
{
  int rc;

  if (next_buffer_pos >= GETC_BUFF_SIZE)
  {
    fprintf(stderr, "Error: cannot push back more than %d characters\n",
            GETC_BUFF_SIZE);
    rc = EOF;
  }
  else
  {
    rc = getc_buffer[next_buffer_pos++] = (unsigned char)c;

    if (feof(input))
      clearerr(input); /* No more EOF. */
  }

  return rc;
}
