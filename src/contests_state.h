#pragma once

#include <pthread.h>

struct EjSessionValue
{
    _Bool ok;
    unsigned char session_id[128];
    unsigned char client_key[128];
};

struct EjContestSession
{
    _Atomic int reader_count;

    int cnts_id;
    _Bool ok;
    long long      recheck_time_us;
    unsigned char *log_s;

    unsigned char *session_id;
    unsigned char *client_key;
    long long expire_us;
};

struct EjContestProblem
{
    int id;
    unsigned char *short_name;
    unsigned char *long_name;
};

struct EjContestCompiler
{
    int id;
    unsigned char *short_name;
    unsigned char *long_name;
    unsigned char *src_suffix;
};

struct EjContestInfo
{
    _Atomic int reader_count;

    int cnts_id;
    _Bool ok;
    long long      recheck_time_us;
    unsigned char *log_s;

    unsigned char *info_json_text;
    size_t info_json_size;

    int prob_size;
    struct EjContestProblem **probs;

    int compiler_size;
    struct EjContestCompiler **compilers;
};

struct EjContestLog
{
    _Atomic int reader_count;

    size_t size;
    unsigned char *text;
};

struct EjProblemInfo
{
    _Atomic int reader_count;

    int prob_id;
    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;

    unsigned char *info_json_text;
    size_t info_json_size;

    time_t server_time;
    unsigned char *short_name;
    unsigned char *long_name;
    int prob_type;
    int full_score;
    int full_user_score;
    int min_score_1;
    int min_score_2;
    unsigned char use_stdin;
    unsigned char use_stdout;
    unsigned char combined_stdin;
    unsigned char combined_stdout;
    unsigned char use_ac_not_ok;
    unsigned char ignore_prev_ac;
    unsigned char team_enable_rep_view;
    unsigned char team_enable_ce_view;
    unsigned char ignore_compile_errors;
    unsigned char disable_user_submit;
    unsigned char disable_tab;
    unsigned char enable_submit_after_reject;
    unsigned char enable_tokens;
    unsigned char tokens_for_user_ac;
    unsigned char disable_submit_after_ok;
    unsigned char disable_auto_testing;
    unsigned char disable_testing;
    unsigned char enable_compilation;
    unsigned char skip_testing;
    unsigned char hidden;
    unsigned char stand_hide_time;
    unsigned char stand_ignore_score;
    unsigned char stand_last_column;
    unsigned char disable_stderr;

    int real_time_limit_ms;
    int time_limit_ms;
    int acm_run_penalty;
    int test_score;
    int run_penalty;
    int disqualified_penalty;
    int compile_error_penalty;
    int tests_to_accept;
    int min_tests_to_accept;
    int score_multiplier;
    int max_user_run_count;

    unsigned char *stand_name;
    unsigned char *stand_column;
    unsigned char *group_name;
    unsigned char *input_file;
    unsigned char *output_file;
    unsigned char *ok_status;

    time_t start_date;

    int compiler_size;
    unsigned char *compilers;

    unsigned long long max_vm_size;
    unsigned long long max_stack_size;

    unsigned char is_statement_avaiable;

    unsigned char is_viewable;
    unsigned char is_submittable;
    unsigned char is_tabable;
    unsigned char is_solved;
    unsigned char is_accepted;
    unsigned char is_pending;
    unsigned char is_pending_review;
    unsigned char is_transient;
    unsigned char is_last_untokenized;
    unsigned char is_marked;
    unsigned char is_autook;
    unsigned char is_rejected;
    unsigned char is_eff_time_needed;

    int best_run;
    int attempts;
    int disqualified;
    int ce_attempts;
    int best_score;
    int prev_successes;
    int all_attempts;
    int eff_attempts;
    int token_count;

    time_t deadline;
    time_t effective_time;

    // estimate statement size
    int est_stmt_size;
};

struct EjProblemStatement
{
    _Atomic int reader_count;

    int prob_id;
    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;

    unsigned char *stmt_text;
    size_t stmt_size;
};

// problem state is a container for updateable data structures
struct EjProblemState
{
    int prob_id;

    _Atomic int info_guard;
    struct EjProblemInfo *info;
    _Atomic _Bool info_update;

    _Atomic int stmt_guard;
    struct EjProblemStatement *stmt;
    _Atomic _Bool stmt_update;
};

struct EjProblemStates;

struct EjContestState
{
    int cnts_id;

    _Atomic int info_guard;
    struct EjContestInfo * _Atomic info;
    _Atomic _Bool info_update;

    _Atomic int log_guard;
    struct EjContestLog * _Atomic log;
    pthread_mutex_t log_mutex;

    _Atomic int session_guard;
    struct EjContestSession * _Atomic session;
    _Atomic _Bool session_update;

    // PIMPL pointer to the problem list
    struct EjProblemStates *prob_states;
};

struct EjContestsState;

struct EjContestsState *contests_state_create(void);
void contests_state_free(struct EjContestsState *ecss);

struct EjContestState *contests_state_get(struct EjContestsState *ecss, int cnts_id);

struct EjContestLog *contest_log_create(const unsigned char *init_str);
void contest_log_free(struct EjContestLog *ecl);
struct EjContestLog *contest_log_read_lock(struct EjContestState *ecs);
struct EjContestLog *contests_log_read_lock(struct EjContestsState *ecss, int cnts_id);
void contests_log_read_unlock(struct EjContestLog *ecl);

struct EjContestState * contests_log_write_lock(struct EjContestsState *ecss, int cnts_id);
void contest_log_set(struct EjContestState *ecs, struct EjContestLog *ecl);
void contest_log_append(struct EjContestState *ecs, const unsigned char *text);

struct EjContestSession *contest_session_create(int cnts_id);
void contest_session_free(struct EjContestSession *ecc);
struct EjContestSession *contest_session_read_lock(struct EjContestState *ecs);
void contest_session_read_unlock(struct EjContestSession *ecc);
int contest_session_try_write_lock(struct EjContestState *ecs);
void contest_session_set(struct EjContestState *ecs, struct EjContestSession *ecc);
int contest_state_copy_session(struct EjContestState *ecs, struct EjSessionValue *esv);

struct EjContestProblem *contest_problem_create(int prob_id);
void contest_problem_free(struct EjContestProblem *ecp);

struct EjContestCompiler *contest_language_create(int lang_id);
void contest_language_free(struct EjContestCompiler *ecl);

struct EjContestInfo *contest_info_create(int cnts_id);
void contest_info_free(struct EjContestInfo *eci);
struct EjContestInfo *contest_info_read_lock(struct EjContestState *ecs);
void contest_info_read_unlock(struct EjContestInfo *eci);
int contest_info_try_write_lock(struct EjContestState *ecs);
void contest_info_set(struct EjContestState *ecs, struct EjContestInfo *ecd);

struct EjProblemStates *problem_states_create(void);
void problem_states_free(struct EjProblemStates *epss);
struct EjProblemState *problem_states_get(struct EjProblemStates *epss, int prob_id);

struct EjProblemState *problem_state_create(int prob_id);
void problem_state_free(struct EjProblemState *eps);

struct EjProblemInfo *problem_info_create(int prob_id);
void problem_info_free(struct EjProblemInfo *epi);
struct EjProblemInfo *problem_info_read_lock(struct EjProblemState *eps);
void problem_info_read_unlock(struct EjProblemInfo *epi);
int problem_info_try_write_lock(struct EjProblemState *eps);
void problem_info_set(struct EjProblemState *ecs, struct EjProblemInfo *epi);

struct EjProblemStatement *problem_statement_create(int prob_id);
void problem_statement_free(struct EjProblemStatement *eph);
struct EjProblemStatement *problem_statement_read_lock(struct EjProblemState *eps);
void problem_statement_read_unlock(struct EjProblemStatement *eph);
int problem_statement_try_write_lock(struct EjProblemState *eps);
void problem_statement_set(struct EjProblemState *eps, struct EjProblemStatement *eph);
