/* This file was automatically generated.  Do not edit! */
/* This file was automatically generated.  Do not edit! */

typedef enum
{
  parameters,
  constraints,
  actions,
  incompatibilities,
  error_functions,
} settings;

typedef enum
{
  entering,
  exiting,
} direction;

typedef enum
{
  CTXOPTNOERR = 0,
  CTXOPTMISPAR,
  CTXOPTMISARG,
  CTXOPTDUPOPT,
  CTXOPTUNKPAR,
  CTXOPTINCTXOPTT,
  CTXOPTINTERNAL,
  CTXOPTERRSIZE,
} errors;

typedef enum
{
  continue_after,
  exit_after,
} usage_behaviour;

char * ctxoptopt;
errors ctxopterrno;

void
ctxopt_init(void);

void
ctxopt_analyze(int nb_words, char ** words, int * rem_count, char *** rem_args);

void
ctxopt_evaluate(void);

void
ctxopt_new_ctx(char * name, char * opts_specs);

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
