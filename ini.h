/* ini.h : header file for INI file parser */
/* Jon Mayo - PUBLIC DOMAIN - July 13, 2007
 * $Id: ini.h 159 2007-07-13 09:48:56Z orange $ */
#ifndef INI_H
#define INI_H

struct ini_info;

void
ini_free(struct ini_info *ii);

struct ini_info *
ini_load(char *filename, unsigned *error, unsigned *line);

char *
ini_next_section(struct ini_info *ii);

char *
ini_next_parameter(struct ini_info *ii, char **parameter);

void
ini_rewind(struct ini_info *ii);

char *
ini_get(struct ini_info *ii, char *section, char *parameter);

#endif
