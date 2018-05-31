#pragma once

/* Copyright (C) 2018 Alexander Chernov <cher@ejudge.ru> */

/*
 * This file is part of ejudge-fuse.
 *
 * Ejudge-fuse is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Ejudge-fuse is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ejudge-fuse.  If not, see <http://www.gnu.org/licenses/>.
 */

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

    long long update_time_us;  // last update time (in case of success)

    unsigned char *info_json_text;
    size_t info_json_size;

    unsigned char *info_text;
    size_t info_size;

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

    long long update_time_us;  // last update time (in case of success)

    unsigned char *info_json_text;
    size_t info_json_size;

    unsigned char *info_text;
    size_t info_size;

    time_t server_time;
    unsigned char *short_name;
    unsigned char *long_name;
    int type;
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
    unsigned char enable_max_stack_size;

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
    int ok_status;

    unsigned char *stand_name;
    unsigned char *stand_column;
    unsigned char *group_name;
    unsigned char *input_file;
    unsigned char *output_file;

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
    long long update_time_us;  // last update time (in case of success)

    unsigned char *stmt_text;
    size_t stmt_size;
};

struct EjDirectoryNodes;

struct EjProblemCompilerSubmits
{
    int lang_id;

    struct EjDirectoryNodes *dir_nodes;
};

struct EjProblemSubmits;

// brief info about run to display as directory listing
struct EjProblemRun
{
    long long run_time_us;
    int run_id;
    int prob_id;
    int status;
    int score;
};

struct EjProblemRuns
{
    _Atomic int reader_count;

    int prob_id;
    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;
    long long update_time_us;  // last update time (in case of success)

    unsigned char *info_json_text;
    size_t info_json_size;

    int size;
    struct EjProblemRun *runs;
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

    _Atomic int runs_guard;
    struct EjProblemRuns *runs;
    _Atomic _Bool runs_update;

    struct EjProblemSubmits *submits;
};

struct EjRunInfoTestResult
{
    int num;
    int status;
    int time_ms;
    int score;
    int max_score;
    unsigned char is_visibility_exists;
};

struct EjRunInfo
{
    _Atomic int reader_count;

    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;
    long long update_time_us;

    unsigned char *info_json_text;
    size_t info_json_size;

    unsigned char *info_text;
    size_t info_size;

    int run_id;

    time_t server_time;
    // run
    int prob_id;
    long long run_time_us;
    time_t run_time;
    time_t duration;
    int lang_id;
    int user_id;
    int size;
    int status;
    time_t effective_time;
    unsigned char *src_sfx;
    int failed_test;
    int passed_tests;
    int score;
    unsigned char *score_str;
    int message_count;

    unsigned char is_imported;
    unsigned char is_hidden;
    unsigned char is_with_duration;
    unsigned char is_standard_problem;
    unsigned char is_minimal_report;
    unsigned char is_with_effective_time;
    unsigned char is_src_enabled;
    unsigned char is_report_enabled;
    unsigned char is_failed_test_available;
    unsigned char is_passed_tests_available;
    unsigned char is_score_available;
    unsigned char is_compiler_output_available;
    unsigned char is_report_available;

    // compiler output
    unsigned char *compiler_text;
    size_t compiler_size;

    // testing report
    unsigned char *valuer_text;
    size_t valuer_size;

    int test_count;
    struct EjRunInfoTestResult *tests;
};

struct EjRunSource
{
    _Atomic int reader_count;

    int run_id;

    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;
    long long update_time_us;

    long long mtime;
    unsigned char *data;
    size_t size;
};

struct EjRunMessage
{
    int clar_id;
    long long time_us;
    int from;
    int to;
    unsigned char *subject;

    unsigned char *data;
    size_t size;
};

struct EjRunMessages
{
    _Atomic int reader_count;

    int run_id;

    _Bool ok;
    long long recheck_time_us;
    unsigned char *log_s;
    long long update_time_us;

    unsigned char *json_text;
    size_t json_size;

    unsigned char *text;
    size_t size;

    time_t server_time;

    long long latest_time_us;

    int count;
    struct EjRunMessage *messages;
};

// run state is a container for updateable run structures
struct EjRunState
{
    int run_id;

    _Atomic int info_guard;
    struct EjRunInfo *info;
    _Atomic _Bool info_update;

    _Atomic int src_guard;
    struct EjRunSource *src;
    _Atomic _Bool src_update;

    _Atomic int msg_guard;
    struct EjRunMessages *msg;
    _Atomic _Bool msg_update;
};

struct EjProblemStates;
struct EjRunStates;

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

    // PIMPL to the runs
    struct EjRunStates *run_states;
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

struct EjRunState *run_state_create(int run_id);
void run_state_free(struct EjRunState *ejr);

struct EjRunStates *run_states_create(void);
void run_states_free(struct EjRunStates *ejrs);
struct EjRunState *run_states_get(struct EjRunStates *erss, int run_id);

struct EjProblemCompilerSubmits *problem_compiler_submits_create(int lang_id);
void problem_compiler_submits_free(struct EjProblemCompilerSubmits *epcs);

struct EjProblemSubmits *problem_submits_create(void);
void problem_submits_free(struct EjProblemSubmits *epss);
struct EjProblemCompilerSubmits *problem_submits_get(struct EjProblemSubmits *epss, int lang_id);

struct EjProblemRuns *problem_runs_create(int prob_id);
void problem_runs_free(struct EjProblemRuns *eprs);

struct EjProblemRuns *problem_runs_read_lock(struct EjProblemState *eps);
void problem_runs_read_unlock(struct EjProblemRuns *eprs);
int problem_runs_try_write_lock(struct EjProblemState *eps);
void problem_runs_set(struct EjProblemState *eps, struct EjProblemRuns *eprs);
struct EjProblemRun *problem_runs_find_unlocked(struct EjProblemRuns *eprs, int run_id);

struct EjRunInfo *run_info_create(int run_id);
void run_info_free(struct EjRunInfo *eri);

struct EjRunInfo *run_info_read_lock(struct EjRunState *ers);
void run_info_read_unlock(struct EjRunInfo *eri);
int run_info_try_write_lock(struct EjRunState *ers);
void run_info_set(struct EjRunState *ers, struct EjRunInfo *eri);

struct EjRunSource *run_source_create(int run_id);
void run_source_free(struct EjRunSource *ert);

struct EjRunSource *run_source_read_lock(struct EjRunState *ers);
void run_source_read_unlock(struct EjRunSource *ert);
int run_source_try_write_lock(struct EjRunState *ers);
void run_source_set(struct EjRunState *ers, struct EjRunSource *eri);

struct EjRunMessages *run_messages_create(int run_id);
void run_messages_free(struct EjRunMessages *erms);

struct EjRunMessages *run_messages_read_lock(struct EjRunState *ers);
void run_messages_read_unlock(struct EjRunMessages *erms);
int run_messages_try_write_lock(struct EjRunState *ers);
void run_messages_set(struct EjRunState *ers, struct EjRunMessages *eri);
