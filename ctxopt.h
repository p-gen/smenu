/* ########################################################### */
/* This Software is licensed under the GPL licensed Version 2, */
/* please read http://www.gnu.org/copyleft/gpl.html            */
/* ########################################################### */

#ifndef CTXOPT_H
#define CTXOPT_H

typedef enum
{
  parameters,
  constraints,
  actions,
  incompatibilities,
  error_functions
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
  char * prog_name;        /* base name of the program name.        */
  char * ctx_name;         /* current context name.                 */
  char * ctx_par_name;     /* parameter which led to this context.  */
  char * opt_name;         /* current option name.                  */
  char * opt_params;       /* parameters of the current option.     */
  int    opts_count;       /* limit of the number of occurrences of *
                           |  the current option.                   */
  int opt_args_count;      /* limit of the number of parameters of  *
                           |  the current option.                   */
  char * pre_opt_par_name; /* parameter before the current one.     */
  char * cur_opt_par_name; /* current parameter.                    */
} state_t;

void
ctxopt_init(char * prog_name);

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
ctxopt_format_constraint(int nb_args, char ** args, char * value);

int
ctxopt_re_constraint(int nb_args, char ** args, char * value);

int
ctxopt_range_constraint(int nb_args, char ** args, char * value);

#endif
