/* ini.c : header file for INI file parser  */
/* Jon Mayo - PUBLIC DOMAIN - July 13, 2007 */
/* initial: 2007-07-13                      */
/* Modified: Pierre Gentile - 2016          */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "xmalloc.h"
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
  /* the rest is used for parsing */
  char             *filename;
  unsigned          line;
  struct section   *curr_sect;
  struct parameter *curr_param;
};

void
ini_free(struct ini_info *ii)
{
  struct section   *curr_sect, *next_sect;
  struct parameter *curr, *next;
  if (ii)
  {
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
}

/* ================================================================== */
/* loads an INI file, parses it and sticks it in the data structure.  */
/* return a pointer on a struct ini_info or NULL in case of failure.  */
/* On failure set error with a coded value and error_line with the    */
/* line number where the error has been detected                      */
/* the error codes are: 0: no error                                   */
/*                      1: the ini file could not have been opened.   */
/*                      2: an unterminated section name was found.    */
/*                      3: an entry without value found found (no =). */
/*                      4: an entry outside any section was found.    */
/* ================================================================== */
struct ini_info *
ini_load(char *filename, unsigned *error, unsigned *error_line)
{
  char              buf[512], *s;
  struct ini_info  *ret;
  struct section   *curr_sect, **prev_sect;
  struct parameter *curr, **prev;
  unsigned          line;
  FILE             *f;

  *error      = 0; /* No error so far. */
  *error_line = 0;

  f = fopen(filename, "r");
  if (!f)
  {
    *error = 1; /* Open failure. */
    return NULL;
  }

  /* Create a new ini_info */
  ret             = xmalloc(sizeof *ret);
  ret->filename   = xstrdup(filename);
  ret->line       = 0;
  ret->curr_sect  = 0;
  ret->curr_param = 0;

  prev_sect = &ret->head;
  curr      = 0;
  prev      = 0;
  line      = 0;
  while (fgets(buf, sizeof buf, f))
  {
    line++;
    s = buf;
    while (isspace(*s))
      s++; /* Ignore leading whitespaces. */

    if (*s == ';' || *s == '#' || *s == 0)
    { /* Comment or blank line. */
    }
    else if (*s == '[')
    { /* New section. */
      char *tmp;

      /* Parse the section. */
      s   = buf + 1;
      tmp = strchr(s, ']');
      if (tmp)
        *tmp = 0;
      else
      { /* Error: unterminated section names. */
        *error      = 2;
        *error_line = line;
        ini_free(ret);
        ret = 0; /* failure */
        break;
      }

      /* Create section. */
      curr_sect       = xmalloc(sizeof *curr_sect);
      curr_sect->next = 0;
      curr_sect->head = 0;
      curr_sect->name = xstrdup(s);

      /* Append it to the end. */
      *prev_sect = curr_sect;
      prev_sect  = &curr_sect->next;

      prev = &curr_sect->head;
    }
    else if (prev)
    { /* Parameter inside section. */
      char *value, *tmp;

      /* Trim trailing comment. */
      tmp = strchr(s, ';');
      if (tmp)
      {
        *tmp = 0;
      }

      /* Trim trailing whitespaces. */
      tmp = s + strlen(s);
      while (tmp != s)
      {
        tmp--;
        if (!isspace(*tmp))
          break;
        *tmp = 0;
      }

      /* Look for '=' sign to mark the start of the value. */
      value = s;
      while (*value && *value != '=')
        value++;
      if (*value)
      {
        /* Build the value string. */
        *value = 0; /* Chop parameter string - now s is parameter. */
        value++;
        while (isspace(*value))
          value++; /* Ignore leading whitespaces. */

        /* Build the parameter string. */
        /* Trim trailing whitespaces. */
        tmp = s + strlen(s);
        while (tmp != s)
        {
          tmp--;
          if (!isspace(*tmp))
            break;
          *tmp = 0;
        }

        /* Create parameter. */
        curr        = xmalloc(sizeof *curr);
        curr->name  = xstrdup(s);
        curr->value = xstrdup(value);
        curr->next  = 0;
        /* Append it to the end. */
        *prev = curr;
        prev  = &curr->next;
      }
      else
      { /* Error: no value found. */
        *error      = 3;
        *error_line = line;
        ini_free(ret);
        ret = 0; /* failure */
        break;
      }
    }
    else
    { /* Error: parameters outside section. */
      *error      = 4;
      *error_line = line;
      /* it's important to always keep the structure in a consistent *
       | state so that we can use the standard free utility.         */
      ini_free(ret);
      ret = 0; /* failure */
      break;
    }
  }

  fclose(f);
  return ret;
}

/* Sequential i/o.                           */
/* Return section name, NULL on end of file. */
char *
ini_next_section(struct ini_info *ii)
{
  ii->curr_param = 0; /* Always reset the parameter. */

  if (ii->curr_sect)
  {
    ii->curr_sect = ii->curr_sect->next;
  }
  else
  {
    ii->curr_sect = ii->head;
  }

  if (ii->curr_sect)
  {
    return ii->curr_sect->name;
  }
  else
  {
    return NULL; /* End. */
  }
}

/* Sequential i/o.                                    */
/* Return value of parameter. NULL on end of section. */
char *
ini_next_parameter(struct ini_info *ii, char **parameter)
{
  /* go to next parameter, or the first if set to 0 */
  if (ii->curr_param)
  {
    ii->curr_param = ii->curr_param->next;
  }
  else
  {
    ii->curr_param = ii->curr_sect->head;
  }

  if (ii->curr_param)
  {
    if (parameter)
      *parameter = ii->curr_param->name;
    return ii->curr_param->value;
  }
  else
  {
    return NULL; /* end */
  }
}

/* Rewind the pointers so the ini_next_xxx functions can start from the top. */
void
ini_rewind(struct ini_info *ii)
{
  if (ii)
  {
    ii->curr_param = 0;
    ii->curr_sect  = 0;
  }
}

/* Random access function.       */
/* Return value for a parameter. */
char *
ini_get(struct ini_info *ii, char *section, char *parameter)
{
  struct section   *curr_sect;
  struct parameter *curr_param;

  for (curr_sect = ii->head; curr_sect; curr_sect = curr_sect->next)
  {
    if (my_strcasecmp(curr_sect->name, section) == 0)
    {
      break;
    }
  }
  if (!curr_sect)
    return NULL;
  for (curr_param = curr_sect->head; curr_param; curr_param = curr_param->next)
  {
    if (my_strcasecmp(curr_param->name, parameter) == 0)
    {
      return curr_param->value;
    }
  }
  return NULL;
}

#if 0
/** Test Code **/
static void ini_dump(struct ini_info *ii) {
   char *section, *parameter, *value;
  while((section=ini_next_section(ii))) {
    printf("[%s]\n", section);
    while((value=ini_next_parameter(ii, &parameter))) {
      printf("%s=%s; buh\n", parameter, value);
    }
  }
}

int main(int argc, char **argv) {
  struct ini_info *ii;
  unsigned error, error_line;

  ii=ini_load("zomg.ini", &error, &error_line);
  if(!ii) {
    return EXIT_FAILURE;
  }
  ini_dump(ii);
  printf("DB=\"%s\"\n", ini_get(ii, "Server", "DBfile"));
  ini_free(ii);
  return 1;
}

/* vim: ts=4 sts=0 noet sw=4
*/
#endif
