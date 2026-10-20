/* ini.c : implementation of the INI file parser */
/* Jon Mayo - PUBLIC DOMAIN - July 13, 2007      */
/* initial: 2007-07-13                           */
/* Modified: Pierre Gentile - 2016               */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "xmalloc.h"
#include "safe.h"
#include "utils.h"
#include "ini.h"

struct parameter
{
  char             *name;
  char             *value;
  struct parameter *next;
};

struct section
{
  char             *name;
  struct parameter *head;
  struct section   *next;
};

struct ini_info
{
  struct section *head;

  /* Sequential-access state. */
  /* """""""""""""""""""""""" */
  char             *filename;
  unsigned          line;
  struct section   *curr_sect;
  struct parameter *curr_param;
};

/* ========================================================== */
/* Release the complete INI data structure. NULL is accepted. */
/* ========================================================== */
void
ini_free(struct ini_info *ii)
{
  struct section   *curr_sect, *next_sect;
  struct parameter *curr, *next;

  if (!ii)
    return;

  for (curr_sect = ii->head; curr_sect; curr_sect = next_sect)
  {
    for (curr = curr_sect->head; curr; curr = next)
    {
      next = curr->next;
      free(curr->name);
      free(curr->value);
      free(curr);
    }

    next_sect = curr_sect->next;
    free(curr_sect->name);
    free(curr_sect);
  }

  free(ii->filename);
  free(ii);
}

/* ================================================================ */
/* Load and parse an INI file. Return NULL on failure.              */
/* error and error_line may be NULL.                                */
/*                                                                  */
/* Error codes:                                                     */
/*   0: no error.                                                   */
/*   1: invalid filename or file could not be opened.               */
/*   2: unterminated section name.                                  */
/*   3: parameter without an '=' sign.                              */
/*   4: parameter outside any section.                              */
/*   5: line exceeds 511 bytes, excluding the final LF.             */
/*   6: file read failure.                                          */
/*                                                                  */
/* Empty section names, parameter names and values remain accepted. */
/* Allocation failure follows the policy of xmalloc and xstrdup.    */
/* ================================================================ */
struct ini_info *
ini_load(char *filename, unsigned *error, unsigned *error_line)
{
  char              buf[512], *s;
  struct ini_info  *ret;
  struct section   *curr_sect, **prev_sect;
  struct parameter *curr, **prev;
  unsigned          line, code;
  FILE             *f;

  if (error)
    *error = 0;

  if (error_line)
    *error_line = 0;

  if (!filename)
  {
    if (error)
      *error = 1;

    return NULL;
  }

  f = fopen_safe(filename, "r");
  if (!f)
  {
    if (error)
      *error = 1;

    return NULL;
  }

  /* Initialize every field before any possible cleanup. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""" */
  ret             = xmalloc(sizeof(*ret));
  ret->head       = NULL;
  ret->filename   = xstrdup(filename);
  ret->line       = 0;
  ret->curr_sect  = NULL;
  ret->curr_param = NULL;

  prev_sect = &ret->head;
  prev      = NULL;
  line      = 0;
  code      = 0;

  while (fgets(buf, sizeof(buf), f))
  {
    line++;

    /* A full buffer without LF may be an overlong line. */
    /* Accept exactly 511 bytes followed by LF or EOF.   */
    /* """"""""""""""""""""""""""""""""""""""""""""""""" */
    if (!strchr(buf, '\n') && strlen(buf) == sizeof(buf) - 1)
    {
      int ch;

      ch = fgetc(f);
      if (ch == EOF)
      {
        if (ferror(f))
        {
          code = 6;
          break;
        }
      }
      else if (ch != '\n')
      {
        code = 5;
        break;
      }
    }

    s = buf;
    while (isspace((unsigned char)*s))
      s++;

    if (*s == ';' || *s == '#' || *s == '\0')
    {
      /* Comment or blank line. */
      /* """""""""""""""""""""" */
      continue;
    }

    if (*s == '[')
    {
      char *tmp;

      /* Advance from the actual '[' after leading whitespace. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""""""" */
      s++;
      tmp = strchr(s, ']');
      if (!tmp)
      {
        code = 2;
        break;
      }
      *tmp = '\0';

      /* Create and append the section. */
      /* """""""""""""""""""""""""""""" */
      curr_sect       = xmalloc(sizeof(*curr_sect));
      curr_sect->name = xstrdup(s);
      curr_sect->head = NULL;
      curr_sect->next = NULL;

      *prev_sect = curr_sect;
      prev_sect  = &curr_sect->next;
      prev       = &curr_sect->head;
    }
    else if (prev)
    {
      char *value, *tmp;

      /* Preserve the original semicolon-comment syntax. */
      /* """"""""""""""""""""""""""""""""""""""""""""""" */
      tmp = strchr(s, ';');
      if (tmp)
        *tmp = '\0';

      /* Trim trailing whitespace from the complete entry. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
      tmp = s + strlen(s);
      while (tmp != s && isspace((unsigned char)tmp[-1]))
        *--tmp = '\0';

      /* Split the parameter name and value at the first '='. */
      /* """""""""""""""""""""""""""""""""""""""""""""""""""" */
      value = strchr(s, '=');
      if (!value)
      {
        code = 3;
        break;
      }

      *value++ = '\0';

      while (isspace((unsigned char)*value))
        value++;

      /* Trim trailing whitespace from the parameter name. */
      /* """"""""""""""""""""""""""""""""""""""""""""""""" */
      tmp = s + strlen(s);
      while (tmp != s && isspace((unsigned char)tmp[-1]))
        *--tmp = '\0';

      /* Create and append the parameter. */
      /* """""""""""""""""""""""""""""""" */
      curr        = xmalloc(sizeof(*curr));
      curr->name  = xstrdup(s);
      curr->value = xstrdup(value);
      curr->next  = NULL;

      *prev = curr;
      prev  = &curr->next;
    }
    else
    {
      code = 4;
      break;
    }
  }

  /* Do not return a partially parsed file after a read failure. */
  /* """"""""""""""""""""""""""""""""""""""""""""""""""""""""""" */
  if (!code && ferror(f))
  {
    code = 6;
    line++;
  }

  fclose(f);

  if (code)
  {
    if (error)
      *error = code;

    if (error_line)
      *error_line = line;

    ini_free(ret);

    return NULL;
  }

  ret->line = line;

  return ret;
}

/* ============================================================ */
/* Return the next section name, or NULL at the end.            */
/* A call after reaching the end restarts at the first section, */
/* preserving the original behavior.                            */
/* ============================================================ */
char *
ini_next_section(struct ini_info *ii)
{
  if (!ii)
    return NULL;

  ii->curr_param = NULL;

  if (ii->curr_sect)
    ii->curr_sect = ii->curr_sect->next;
  else
    ii->curr_sect = ii->head;

  if (ii->curr_sect)
    return ii->curr_sect->name;

  return NULL;
}

/* =================================================================== */
/* Return the next parameter value, or NULL at the end of the section. */
/* If parameter is non-NULL, store the parameter name there.           */
/* No selected section returns NULL without dereferencing it.          */
/* A call after reaching the end restarts at the first parameter,      */
/* preserving the original behavior.                                   */
/* =================================================================== */
char *
ini_next_parameter(struct ini_info *ii, char **parameter)
{
  if (parameter)
    *parameter = NULL;

  if (!ii || !ii->curr_sect)
    return NULL;

  if (ii->curr_param)
    ii->curr_param = ii->curr_param->next;
  else
    ii->curr_param = ii->curr_sect->head;

  if (ii->curr_param)
  {
    if (parameter)
      *parameter = ii->curr_param->name;

    return ii->curr_param->value;
  }

  return NULL;
}

/* =============================================================== */
/* Reset sequential access so iteration starts from the beginning. */
/* =============================================================== */
void
ini_rewind(struct ini_info *ii)
{
  if (ii)
  {
    ii->curr_param = NULL;
    ii->curr_sect  = NULL;
  }
}

/* ================================================================= */
/* Return a parameter value using case-insensitive name comparisons. */
/* Return NULL for invalid arguments or when no match exists.        */
/* Returned strings remain owned by the INI data structure.          */
/* ================================================================= */
char *
ini_get(struct ini_info *ii, char *section, char *parameter)
{
  struct section   *curr_sect;
  struct parameter *curr_param;

  if (!ii || !section || !parameter)
    return NULL;

  for (curr_sect = ii->head; curr_sect; curr_sect = curr_sect->next)
  {
    if (my_strcasecmp(curr_sect->name, section) == 0)
      break;
  }

  if (!curr_sect)
    return NULL;

  for (curr_param = curr_sect->head; curr_param; curr_param = curr_param->next)
  {
    if (my_strcasecmp(curr_param->name, parameter) == 0)
      return curr_param->value;
  }

  return NULL;
}

#if 0
/* ==================================== */
/* Display all sections and parameters. */
/* ==================================== */
static void
ini_dump(struct ini_info *ii)
{
  char *section, *parameter, *value;

  ini_rewind(ii);

  while ((section = ini_next_section(ii)) != NULL)
  {
    printf("[%s]\n", section);

    while ((value = ini_next_parameter(ii, &parameter)) != NULL)
      printf("%s=%s\n", parameter, value);
  }
}
#endif
