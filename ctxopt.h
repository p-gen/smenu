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
  CTXOPTERRSIZE
} errors;

typedef enum
{
  continue_after,
  exit_after
} usage_behaviour;

typedef struct state_s
{
  char * ctx_name;
  char * ctx_par_name;
  char * opt_name;
  char * opt_params;
  char * pre_opt_par_name;
  char * cur_opt_par_name;
} state_t;

void
ctxopt_init(void);

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
