/* egetopt.c -- Extended 'getopt'.
 *
 * A while back, a public-domain version of getopt() was posted to the
 * net.  A bit later, a gentleman by the name of Keith Bostic made some
 * enhancements and reposted it.
 *
 * In recent weeks (i.e., early-to-mid 1988) there's been some
 * heated discussion in comp.lang.c about the merits and drawbacks
 * of getopt(), especially with regard to its handling of '?'.
 *
 * In light of this, I have taken Mr. Bostic's public-domain getopt()
 * and have made some changes that I hope will be considered to be
 * improvements.  I call this routine 'egetopt' ("Extended getopt").
 * The default behavior of this routine is the same as that of getopt(),
 * but it has some optional features that make it more useful.  These
 * options are controlled by the settings of some global variables.
 * By not setting any of these extra global variables, you will have
 * the same functionality as getopt(), which should satisfy those
 * purists who believe getopt() is perfect and can never be improved.
 * If, on the other hand, you are someone who isn't satisfied with the
 * status quo, egetopt() may very well give you the added capabilities
 * you want.
 *
 * Look at the enclosed README file for a description of egetopt()'s
 * new features.
 *
 * The code was originally posted to the net as getopt.c by ...
 *
 * Keith Bostic
 * ARPA: keith@seismo
 * UUCP: seismo!keith
 *
 * Current version: added enhancements and comments, reformatted code.
 *
 * Lloyd Zusman
 * Master Byte Software
 * Los Gatos, California
 * Internet: ljz@fx.com
 * UUCP:     ...!ames!fxgrp!ljz
 *
 * May, 1988
 *
 * This version is slightly adapted for use by smenu.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "getopt.h"

/* Here are all the pertinent global variables. */
/* """""""""""""""""""""""""""""""""""""""""""" */
int    eopterr = 1;          /* if true, output error message */
int    eoptind = 1;          /* index into parent argv vector */
int    eoptopt;              /* character checked for validity */
int    eoptbad   = BADCH;    /* character returned on error */
int    eoptchar  = 0;        /* character that begins returned option */
int    eoptneed  = NEEDSEP;  /* flag for mandatory argument */
int    eoptmaybe = MAYBESEP; /* flag for optional argument */
int    eopterrfd = ERRFD;    /* file descriptor for error text */
char * eoptarg   = NULL;     /* argument associated with option */
char * eoptstart = START;    /* list of characters that start options */

/* Here it is: */
/* """"""""""" */
int
egetopt(int nargc, char ** nargv, char * ostr)
{
  static char *   place = EMSG; /* option letter processing */
  register char * oli;          /* option letter list index */
  register char * osi = NULL;   /* option start list index  */

  if (nargv == (char **)NULL)
    return (EOF);

  if (nargc <= eoptind || nargv[eoptind] == NULL)
    return (EOF);

  if (place == NULL)
    place = EMSG;

  /* Update scanning pointer. */
  /* """""""""""""""""""""""" */
  if (*place == '\0')
  {
    place = nargv[eoptind];
    if (place == NULL)
      return (EOF);
    else if (*place == '\0')
      return (EOF);

    osi = strchr(eoptstart, *place);
    if (osi != NULL)
      eoptchar = (int)*osi;

    if (eoptind >= nargc || osi == NULL || *++place == '\0')
      return (EOF);

    /* Two adjacent, identical flag characters were found. */
    /* This takes care of "--", for example.               */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == place[-1])
    {
      ++eoptind;
      return (EOF);
    }
  }

  /* If the option is a separator or the option isn't in the list, */
  /* we've got an error.                                           */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  eoptopt = (int)*place++;
  oli     = strchr(ostr, eoptopt);
  if (eoptopt == eoptneed || eoptopt == (int)eoptmaybe || oli == NULL)
  {
    /* If we're at the end of the current argument, bump the */
    /* argument index.                                       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == '\0')
      ++eoptind;

    TELL("Illegal option -- "); /* bye bye */
  }

  /* If there is no argument indicator, then we don't even try to */
  /* return an argument.                                          */
  /* """""""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  ++oli;
  if (*oli == '\0' || (*oli != (char)eoptneed && *oli != (char)eoptmaybe))
  {
    /* If we're at the end of the current argument, bump the */
    /* argument index.                                       */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place == '\0')
      ++eoptind;

    eoptarg = NULL;
  }
  /* If we're here, there's an argument indicator.  It's handled */
  /* differently depending on whether it's a mandatory or an     */
  /* optional argument.                                          */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  else
  {
    /* If there's no white space, use the rest of the    */
    /* string as the argument.  In this case, it doesn't */
    /* matter if the argument is mandatory or optional.  */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if (*place != '\0')
      eoptarg = place;

    /* If we're here, there's whitespace after the option. */
    /*                                                     */
    /* Is it a mandatory argument?  If so, return the      */
    /* next command-line argument if there is one.         */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
    else if (*oli == (char)eoptneed)
    {
      /* If we're at the end of the argument list, there  */
      /* isn't an argument and hence we have an error.    */
      /* Otherwise, make 'eoptarg' point to the argument. */
      /* """""""""""""""""""""""""""""""""""""""""""""""" */
      if (nargc <= ++eoptind)
      {
        place = EMSG;
        TELL("Option requires an argument -- ");
      }
      else
        eoptarg = nargv[eoptind];
    }
    /* If we're here it must have been an optional argument. */
    /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
    else
    {
      if (nargc <= ++eoptind)
      {
        place   = EMSG;
        eoptarg = NULL;
      }
      else
      {
        eoptarg = nargv[eoptind];
        if (eoptarg == NULL)
          place = EMSG;

        /* If the next item begins with a flag  */
        /* character, we treat it like a new    */
        /* argument.  This is accomplished by   */
        /* decrementing 'eoptind' and returning */
        /* a null argument.                     */
        /* """""""""""""""""""""""""""""""""""" */
        else if (strchr(eoptstart, *eoptarg) != NULL)
        {
          --eoptind;
          eoptarg = NULL;
        }
      }
    }
    place = EMSG;
    ++eoptind;
  }

  /* Return option letter. */
  /* """"""""""""""""""""" */
  return (eoptopt);
}
