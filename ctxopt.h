/* ################################################################### */
/* This Source Code Form is subject to the terms of the Mozilla Public */
/* License, v. 2.0. If a copy of the MPL was not distributed with this */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.           */
/* ################################################################### */

#ifndef CTXOPT_H
#define CTXOPT_H

typedef enum
{
  parameters,
  constraints,
  actions,
  incompatibilities,
  requirements,
  error_functions,
  before,
  after,
} settings;

typedef enum
{
  entering,
  exiting
} direction;

typedef enum
{
  CTXOPTNOERR = 0,
  CTXOPTMISPAR,
  CTXOPTREQPAR,
  CTXOPTMISARG,
  CTXOPTDUPOPT,
  CTXOPTUNKPAR,
  CTXOPTINCOPT,
  CTXOPTCTEOPT,
  CTXOPTCTLOPT,
  CTXOPTCTGOPT,
  CTXOPTCTEARG,
  CTXOPTCTLARG,
  CTXOPTCTGARG,
  CTXOPTUNXARG,
  CTXOPTERRSIZ
} errors;

typedef enum
{
  continue_after,
  exit_after
} usage_behaviour;

typedef struct state_s
{
  char * prog_name;          /* base name of the program name.        */
  char * ctx_name;           /* current context name.                 */
  char * ctx_par_name;       /* parameter which led to this context.  */
  char * opt_name;           /* current option name.                  */
  int    opts_count;         /* limit of the number of occurrences of *
                             |  the current option.                   */
  int opt_args_count;        /* limit of the number of parameters of  *
                             |  the current option.                   */
  char * pre_opt_par_name;   /* parameter before the current one.     */
  char * cur_opt_par_name;   /* current parameter.                    */
  char * cur_opt_params;     /* All the option's parameters.          */
  char * req_opt_par_needed; /* Option's params in the missing        *
                             | required group of optrions.            */
  char * req_opt_par;        /* Option's params of the option which   *
                             | required one of the parameter in       *
                             | req_opt_par_needed to also be present  *
                             | in the current context.                */
} state_t;

void
ctxopt_init(char * prog_name, char * flags);

void
ctxopt_analyze(int nb_words, char ** words, int * rem_count, char *** rem_args);

void
ctxopt_evaluate(void);

void
ctxopt_new_ctx(char * name, char * opts_specs);

void
ctxopt_ctx_disp_usage(char * ctx_name, usage_behaviour action);

void
ctxopt_disp_usage(usage_behaviour action);

void
ctxopt_add_global_settings(settings s, ...);

void
ctxopt_add_ctx_settings(settings s, ...);

void
ctxopt_add_opt_settings(settings s, ...);

int
ctxopt_format_constraint(int nb_args, char ** args, char * value, char * par);

int
ctxopt_re_constraint(int nb_args, char ** args, char * value, char * par);

int
ctxopt_range_constraint(int nb_args, char ** args, char * value, char * par);

void
ctxopt_free_memory(void);

#endif
