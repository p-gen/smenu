/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

#ifndef SAFE_H
#define SAFE_H

int
fputs_safe(const char * restrict s, FILE * restrict stream);

int
fputc_safe(int c, FILE * stream);

int
tcsetattr_safe(int fildes, int optional_actions,
               const struct termios * termios_p);

FILE *
fopen_safe(const char * restrict stream, const char * restrict mode);

#endif
