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

#include "ejfuse.h"
#include "contests_state.h"

static const unsigned char *
size_to_string(unsigned char *buf, size_t size, unsigned long long value)
{
    if (!(value % (1024 * 1024 * 1024))) {
        snprintf(buf, size, "%lluG", value / (1024 * 1024 * 1024));
    } else if (!(value % (1024 * 1024))) {
        snprintf(buf, size, "%lluM", value / (1024 * 1024));
    } else if (!(value % 1024)) {
        snprintf(buf, size, "%lluK", value / (1024 * 1024));
    } else {
        snprintf(buf, size, "%llu", value);
    }
    return buf;
}

void
ejfuse_contest_info_text(struct EjContestInfo *eci)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);

    fclose(text_f);
    eci->info_text = text_s;
    eci->info_size = text_z;
}

void
ejfuse_problem_info_text(struct EjProblemInfo *epi)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);
    time_t ltt;
    struct tm ltm;

    fprintf(text_f, "Problem information:\n");
    if (epi->short_name && epi->short_name[0]) {
        fprintf(text_f, "\tShort name:\t\t%s\n", epi->short_name);
    }
    if (epi->long_name && epi->long_name[0]) {
        fprintf(text_f, "\tLong name:\t\t%s\n", epi->long_name);
    }
        /*
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

    time_t start_date;
    time_t deadline;

    int compiler_size;
    unsigned char *compilers;
         */
    if ((long long) epi->max_vm_size > 0) {
        unsigned char buf[128];
        fprintf(text_f, "\tMax VM Size:\t\t%s\n", size_to_string(buf, sizeof(buf), epi->max_vm_size));
    }
    if ((long long) epi->max_stack_size > 0) {
        unsigned char buf[128];
        fprintf(text_f, "\tMax Stack Size:\t\t%s\n", size_to_string(buf, sizeof(buf), epi->max_stack_size));
    } else if (epi->enable_max_stack_size > 0) {
        unsigned char buf[128];
        fprintf(text_f, "\tMax Stack Size:\t\t%s\n", size_to_string(buf, sizeof(buf), epi->max_vm_size));
    }
    
    fprintf(text_f, "Your statistics:\n");
        /*

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

    time_t effective_time;
         */

    fprintf(text_f, "Server information:\n");
    ltt = epi->server_time;
    localtime_r(&ltt, &ltm);
    fprintf(text_f, "\tServer time:\t\t%04d-%02d-%02d %02d:%02d:%02d\n",
            ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

    fclose(text_f);
    epi->info_text = text_s;
    epi->info_size = text_z;
}

void
ejfuse_run_info_text(struct EjRunInfo *eri)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);

    fclose(text_f);
    eri->info_text = text_s;
    eri->info_size = text_z;
}
