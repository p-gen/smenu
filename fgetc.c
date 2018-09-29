#include <stdio.h>
#include "fgetc.h"

/* ************************************************************************ */
/* Custom fgetc/ungetc implementation able to unget more than one character */
/* ************************************************************************ */

enum
{
  GETC_BUFF_SIZE = 16
};

static char getc_buffer[GETC_BUFF_SIZE] = { '\0' };

static long next_buffer_pos = 0; /* next free position in the getc buffer */

/* ====================================== */
/* Get a (possibly pushed-back) character */
/* ====================================== */
int
my_fgetc(FILE * input)
{
  return (next_buffer_pos > 0) ? getc_buffer[--next_buffer_pos] : fgetc(input);
}

/* ============================ */
/* Push character back on input */
/* ============================ */
void
my_ungetc(int c)
{
  if (next_buffer_pos >= GETC_BUFF_SIZE)
    fprintf(stderr, "Error: cannot push back more than %d characters\n",
            GETC_BUFF_SIZE);
  else
    getc_buffer[next_buffer_pos++] = c;
}
