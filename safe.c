/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

/* ************************************ */
/* Some wrappers to manage EINTR errors */
/* ************************************ */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "safe.h"

FILE *
fopen_safe(const char * restrict stream, const char * restrict mode)
{
  FILE * file;

  while ((file = fopen(stream, mode)) == NULL && errno == EINTR)
    ;

  return file;
}

int
tcsetattr_safe(int fildes, int optional_actions,
               const struct termios * termios_p)
{
  int res;

  while ((res = tcsetattr(fildes, optional_actions, termios_p)) == -1
         && errno == EINTR)
    ;

  return res;
}

int
fputc_safe(int c, FILE * stream)
{
  int res;

  while ((res = fputc(c, stream)) == -1 && errno == EINTR)
    ;

  return res;
}

int
fputs_safe(const char * restrict s, FILE * restrict stream)
{
  int res;

  while ((res = fputs(s, stream)) == -1 && errno == EINTR)
    ;

  return res;
}
