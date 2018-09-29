#ifndef GETOPT_H
#define GETOPT_H

/* None of these constants are referenced in the executable portion of */
/* the code ... their sole purpose is to initialize global variables.  */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define BADCH (int)'@'
#define NEEDSEP (int)':'
#define MAYBESEP (int)'%'
#define ERRFD 2
#define EMSG ""
#define START "-"

/* Conditionally print out an error message and return (depends on the */
/* setting of 'opterr' and 'opterrfd').  Note that this version of     */
/* TELL() doesn't require the existence of stdio.h.                    */
/* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
#define TELL(S)                                      \
  {                                                  \
    ssize_t dummy_rc;                                \
    if (opterr && opterrfd >= 0)                     \
    {                                                \
      char option = (char)optopt;                    \
      dummy_rc    = write(opterrfd, (S), strlen(S)); \
      dummy_rc    = write(opterrfd, &option, 1);     \
      dummy_rc    = write(opterrfd, "\n", 1);        \
    }                                                \
    return (optbad);                                 \
  }

#endif
