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
#include "ejudge.h"

#include <string.h>
#include <stdarg.h>

struct KeyValueItem
{
    int indent;
    unsigned char *key;
    unsigned char *str;
    int int_value;
    _Bool owns_key;
    _Bool has_str;
    _Bool owns_str;
    _Bool has_int;
};

struct KeyValueVector
{
    int reserved;
    int size;
    struct KeyValueItem *items;
};

static void
kvv_free(struct KeyValueVector *kvv)
{
    if (kvv) {
        for (int i = 0; i < kvv->size; ++i) {
            struct KeyValueItem *kvi = &kvv->items[i];
            if (kvi->owns_key) free(kvi->key);
            if (kvi->has_str && kvi->owns_str) free(kvi->str);
        }
    }
}

static struct KeyValueItem *
kvv_extend(struct KeyValueVector *kvv)
{
    if (kvv->size == kvv->reserved) {
        if (!(kvv->reserved *= 2)) kvv->reserved = 32;
        kvv->items = realloc(kvv->items, kvv->reserved * sizeof(kvv->items[0]));
    }
    memset(&kvv->items[kvv->size], 0, sizeof(kvv->items[0]));
    return &kvv->items[kvv->size++];
}

static void
kvv_push_back_key(struct KeyValueVector *kvv, int indent, const unsigned char *key)
{
    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
}

static void
kvv_push_back_const_str(struct KeyValueVector *kvv, int indent, const unsigned char *key, const unsigned char *value)
{
    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
    kvi->has_str = 1;
    kvi->str = (unsigned char *) value;

}

static void
kvv_push_back_copy_str(struct KeyValueVector *kvv, int indent, const unsigned char *key, const unsigned char *value)
{
    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
    kvi->has_str = 1;
    kvi->owns_str = 1;
    kvi->str = (unsigned char *) strdup(value);
}

static void
kvv_push_back_move_str(struct KeyValueVector *kvv, int indent, const unsigned char *key, unsigned char *value)
{
    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
    kvi->has_str = 1;
    kvi->owns_str = 1;
    kvi->str = value;
}

static void
kvv_push_back_int(struct KeyValueVector *kvv, int indent, const unsigned char *key, int value)
{
    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
    kvi->has_int = 1;
    kvi->int_value = value;

}

static void __attribute__((format(printf, 4, 5)))
kvv_push_back_format(
        struct KeyValueVector *kvv,
        int indent,
        const unsigned char *key,
        const char *format, ...)
{
    char *p = NULL;
    va_list args;
    va_start(args, format);
    vasprintf(&p, format, args);
    va_end(args);

    struct KeyValueItem *kvi = kvv_extend(kvv);
    kvi->indent = indent;
    kvi->key = (unsigned char *) key;
    kvi->has_str = 1;
    kvi->owns_str = 1;
    kvi->str = (unsigned char *) p;

}

static void
kvv_generate(struct KeyValueVector *kvv, FILE *fout)
{
    // compute value column
    int max_key_width = -1;
    for (int i = 0; i < kvv->size; ++i) {
        struct KeyValueItem *kvi = &kvv->items[i];
        int width = kvi->indent * 4;
        width += strlen(kvi->key);
        if (width > max_key_width) max_key_width = width;
    }
    for (int i = 0; i < kvv->size; ++i) {
        struct KeyValueItem *kvi = &kvv->items[i];
        int rem_width = max_key_width;
        if (kvi->indent > 0) {
            int w = kvi->indent * 4;
            rem_width -= w;
            for (; w; --w) putc_unlocked(' ', fout);
        }
        fprintf(fout, "%s: ", kvi->key);
        if (kvi->has_int || kvi->has_str) {
            rem_width -= strlen(kvi->key);
            for (; rem_width; --rem_width) putc_unlocked(' ', fout);
            if (kvi->has_int) {
                fprintf(fout, "%d", kvi->int_value);
            } else if (kvi->has_str) {
                fprintf(fout, "%s", kvi->str);
            }
        }
        fprintf(fout, "\n");
    }
}

static const unsigned char *
size_to_str(unsigned char *buf, size_t size, unsigned long long value)
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

enum
{
    SEC_PER_YEAR = 365 * 24 * 60 * 60,
    SEC_PER_MONTH = 30 * 24 * 60 * 60,
    SEC_PER_WEEK = 7 * 24 * 60 * 60,
    SEC_PER_DAY = 24 * 60 * 60,
    SEC_PER_HOUR = 60 * 60,
    SEC_PER_MIN = 60,
};

static unsigned char *
dur_to_str(int sec)
{
    char *s = NULL;
    size_t z = 0;
    FILE *f = open_memstream(&s, &z);
    if (sec > 0) {
        if (sec >= SEC_PER_YEAR) {
            fprintf(f, "%dy", sec / SEC_PER_YEAR);
            sec %= SEC_PER_YEAR;
        }
        if (sec > 0) {
            if (sec >= SEC_PER_MONTH) {
                fprintf(f, "%dn", sec / SEC_PER_MONTH);
                sec %= SEC_PER_MONTH;
            }
            if (sec > 0) {
                if (sec >= SEC_PER_WEEK) {
                    fprintf(f, "%dw", sec / SEC_PER_WEEK);
                    sec %= SEC_PER_WEEK;
                }
                if (sec > 0) {
                    if (sec >= SEC_PER_DAY) {
                        fprintf(f, "%dd", sec / SEC_PER_DAY);
                        sec %= SEC_PER_DAY;
                    }
                    if (sec > 0) {
                        if (sec >= SEC_PER_HOUR) {
                            fprintf(f, "%dh", sec / SEC_PER_HOUR);
                            sec %= SEC_PER_HOUR;
                        }
                        if (sec > 0) {
                            if (sec >= SEC_PER_MIN) {
                                fprintf(f, "%dm", sec / SEC_PER_MIN);
                                sec %= SEC_PER_MIN;
                            }
                            if (sec > 0) {
                                fprintf(f, "%ds", sec);
                            }
                        }
                    }
                }
            }
        }
    } else {
        fprintf(f, "N/A");
    }
    fclose(f);
    return s;
}

static unsigned char *
time_to_str(time_t t)
{
    char *s = NULL;
    size_t z = 0;
    FILE *f = open_memstream(&s, &z);
    if (t <= 0) {
        fprintf(f, "N/A");
    } else {
        struct tm tt;
        localtime_r(&t, &tt);
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d",
                tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday,
                tt.tm_hour, tt.tm_min, tt.tm_sec);
    }
    fclose(f);
    return s;
}

static unsigned char *
utime_to_str(long long ut)
{
    char *s = NULL;
    size_t z = 0;
    FILE *f = open_memstream(&s, &z);
    if (ut <= 0) {
        fprintf(f, "N/A");
    } else {
        long long llt = ut / 1000000;
        if ((time_t) llt != llt) {
            fprintf(f, "N/A");
        } else {
            time_t t = llt;
            struct tm tt;
            localtime_r(&t, &tt);
            fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d.%lld",
                    tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday,
                    tt.tm_hour, tt.tm_min, tt.tm_sec, ut % 1000000);
        }
    }
    fclose(f);
    return s;
}

void
ejfuse_contest_info_text(struct EjContestInfo *eci)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);

    fprintf(text_f, "Contest information:\n");
    if (eci->name && eci->name[0]) {
        fprintf(text_f, "\tName:\t\t%s\n", eci->name);
    }
    const unsigned char *type = NULL;
    if (eci->score_system == SCORE_ACM) {
        if (eci->is_virtual) {
            type = "Virtual ACM";
        } else {
            type = "ACM";
        }
    } else if (eci->score_system == SCORE_KIROV) {
        if (eci->is_virtual) {
            type = "Virtual Kirov";
        } else {
            type = "Kirov";
        }
    } else if (eci->score_system == SCORE_OLYMPIAD) {
        if (eci->is_virtual) {
            type = "Virtual Olympiad";
        } else {
            type = "Olympiad";
        }
    } else if (eci->score_system == SCORE_MOSCOW) {
        if (eci->is_virtual) {
        } else {
            type = "Moscow";
        }
    }
    if (type) {
        fprintf(text_f, "\tType:\t\t%s\n", type);
    }
    if (eci->is_testing_finished) {
        fprintf(text_f, "\tOlympiad:\t%s\n", "TESTING FINISHED");
    } else if (eci->is_olympiad_accepting_mode) {
        fprintf(text_f, "\tOlympiad:\t%s\n", "ACCEPTING SOLUTIONS");
    }
    if (eci->is_clients_suspended) {
        fprintf(text_f, "\tClients:\t\t%s\n", "SUSPENDED");
    }
    if (eci->is_testing_suspended) {
        fprintf(text_f, "\tTesting:\t\t%s\n", "SUSPENDED");
    }
    if (eci->is_printing_suspended) {
        fprintf(text_f, "\tPrinting:\t\t%s\n", "SUSPENDED");
    }
    if (eci->is_upsolving) {
        fprintf(text_f, "\tUpsolving:\t%s\n", "ACTIVATED");
    }
    if (eci->is_restartable) {
        fprintf(text_f, "\tVirtual Restart:\t%s\n", "ENABLED");
    }
    if (eci->is_unlimited) {
        fprintf(text_f, "\tDuration:\tUnlimited\n");
    } else {
        unsigned char *s = dur_to_str(eci->duration);
        fprintf(text_f, "\tDuration:\t%s\n", s);
        free(s);
    }
    if (eci->is_started && eci->is_stopped) {
        fprintf(text_f, "\tStatus:\t\tSTOPPED\n");
        unsigned char *s = time_to_str(eci->start_time);
        fprintf(text_f, "\tStart Time:\t%s\n", s);
        free(s);
        s = time_to_str(eci->stop_time);
        fprintf(text_f, "\tStop Time:\t%s\n", s);
        free(s);
        if (eci->is_frozen) {
            fprintf(text_f, "\tStandings:\t%s\n", "FROZEN");
            s = time_to_str(eci->unfreeze_time);
            fprintf(text_f, "\tUnfreeze Time:\t%s\n", s);
            free(s);
        }
    } else if (eci->is_started) {
        fprintf(text_f, "\tStatus:\t\tRUNNING\n");
        unsigned char *s = time_to_str(eci->start_time);
        fprintf(text_f, "\tStart Time:\t%s\n", s);
        free(s);
        s = dur_to_str((int)(eci->server_time - eci->start_time));
        fprintf(text_f, "\tElapsed Time:\t%s\n", s);
        free(s);
        if (eci->scheduled_finish_time > 0) {
            s = time_to_str(eci->scheduled_finish_time);
            fprintf(text_f, "\tScheduled Stop:\t%s\n", s);
            free(s);
            s = dur_to_str((int)(eci->scheduled_finish_time - eci->server_time));
            fprintf(text_f, "\tRemaining:\t%s\n", s);
            free(s);
        }
        if (eci->expected_stop_time > 0) {
            s = time_to_str(eci->expected_stop_time);
            fprintf(text_f, "\tExpected Stop:\t%s\n", s);
            free(s);
            s = dur_to_str((int)(eci->expected_stop_time - eci->server_time));
            fprintf(text_f, "\tRemaining:\t%s\n", s);
            free(s);
        }
        if (eci->is_freezable) {
            if (eci->is_frozen) {
                fprintf(text_f, "\tStandings:\t\t%s\n", "FROZEN");
            }
            s = time_to_str(eci->freeze_time);
            fprintf(text_f, "\tFreeze Time:\t\t%s\n", s);
            free(s);
        }
    } else {
        fprintf(text_f, "\tStatus:\t\tNOT STARTED\n");
        if (eci->scheduled_start_time > 0) {
            unsigned char *s = time_to_str(eci->scheduled_start_time);
            fprintf(text_f, "\tScheduled:\t%s\n", s);
            free(s);
            s = dur_to_str((int)(eci->scheduled_start_time - eci->server_time));
            fprintf(text_f, "\tBefore Start:\t%s\n", s);
            free(s);
        }
        if (eci->open_time > 0) {
            unsigned char *s = time_to_str(eci->open_time);
            fprintf(text_f, "\tOpen Time:\t%s\n", s);
            free(s);
        }
        if (eci->close_time > 0) {
            unsigned char *s = time_to_str(eci->close_time);
            fprintf(text_f, "\tClose Time:\t%s\n", s);
            free(s);
        }
    }

    fprintf(text_f, "Server statistics:\n");
    if (eci->server_time > 0) {
        unsigned char *s = time_to_str(eci->server_time);
        fprintf(text_f, "\tServer time:\t%s\n", s);
        free(s);
    }
    if (eci->user_count > 0) {
        fprintf(text_f, "\tOn-line users:\t%d\n", eci->user_count);
    }
    if (eci->max_online_count > 0 && eci->max_online_time > 0) {
        unsigned char *s = time_to_str(eci->max_online_time);
        fprintf(text_f, "\tMax On-line:\t%d (%s)\n", eci->max_online_count, s);
        free(s);
    }
    if (eci->update_time_us > 0) {
        unsigned char *s = utime_to_str(eci->update_time_us);
        fprintf(text_f, "\tStatus updated:\t%s\n", s);
        free(s);
    }

    fclose(text_f);
    eci->info_text = text_s;
    eci->info_size = text_z;
}

void
ejfuse_problem_info_text(struct EjProblemInfo *epi, struct EjContestState *ecs)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);
    unsigned char status_str[128];
    struct KeyValueVector kvv = {};

    kvv_push_back_key(&kvv, 0, "Problem information");
    if (epi->disable_user_submit) {
        kvv_push_back_const_str(&kvv, 1, "Problem information", "YES");
    }
    if (epi->disable_testing && epi->enable_compilation) {
        kvv_push_back_const_str(&kvv, 1, "Just Compile", "YES");
    } else if (epi->disable_testing) {
        kvv_push_back_const_str(&kvv, 1, "Disable Testing", "YES");
    }
    if (epi->short_name && epi->short_name[0]) {
        kvv_push_back_const_str(&kvv, 1, "Short name", epi->short_name);
    }
    if (epi->long_name && epi->long_name[0]) {
        kvv_push_back_const_str(&kvv, 1, "Long name", epi->long_name);
    }
    const unsigned char *problem_type = problem_unparse_type(epi->type);
    if (problem_type) {
        kvv_push_back_const_str(&kvv, 1, "Type", problem_type);
    }
    if (epi->full_score >= 0) {
        kvv_push_back_int(&kvv, 1, "Full Score", epi->full_score);
    }
    if (epi->full_user_score >= 0) {
        kvv_push_back_int(&kvv, 1, "Full User Score", epi->full_user_score);
    }
    if (epi->min_score_1 >= 0) {
        kvv_push_back_int(&kvv, 1, "Min Score 1", epi->min_score_1);
    }
    if (epi->min_score_2 >= 0) {
        kvv_push_back_int(&kvv, 1, "Min Score 2", epi->min_score_2);
    }
    if (epi->use_stdin && epi->combined_stdin) {
        if (epi->input_file && epi->input_file[0]) {
            kvv_push_back_format(&kvv, 1, "Input", "'%s' or standard input", epi->input_file);
        } else {
            kvv_push_back_const_str(&kvv, 1, "Input", "file or standard input");
        }
    } else if (epi->use_stdin) {
        kvv_push_back_const_str(&kvv, 1, "Input", "standard input");
    } else if (epi->input_file && epi->input_file[0]) {
        kvv_push_back_format(&kvv, 1, "Input", "'%s'", epi->input_file);
    } else {
    }
    if (epi->use_stdout && epi->combined_stdout) {
        if (epi->input_file && epi->input_file[0]) {
            kvv_push_back_format(&kvv, 1, "Output", "'%s' or standard output", epi->output_file);
        } else {
            kvv_push_back_const_str(&kvv, 1, "Output", "file or standard output");
        }
    } else if (epi->use_stdout) {
        kvv_push_back_const_str(&kvv, 1, "Output", "standard output");
    } else if (epi->input_file && epi->input_file[0]) {
        kvv_push_back_format(&kvv, 1, "Output", "'%s'", epi->output_file);
    } else {
    }
    if (epi->ok_status >= 0) {
        kvv_push_back_copy_str(&kvv, 1, "Success Status", run_status_str(epi->ok_status, status_str, sizeof(status_str), 0, 0));
    } else if (epi->use_ac_not_ok) {
        kvv_push_back_copy_str(&kvv, 1, "Success Status", run_status_str(RUN_PENDING_REVIEW, status_str, sizeof(status_str), 0, 0));
    }
    if (epi->ignore_prev_ac) {
        kvv_push_back_const_str(&kvv, 1, "Ignore Previous PR", "yes");
    }
    if (epi->team_enable_rep_view) {
        kvv_push_back_const_str(&kvv, 1, "Enable Report View", "yes");
    }
    if (epi->team_enable_ce_view) {
        kvv_push_back_const_str(&kvv, 1, "Enable Compile Log View", "yes");
    }
    if (epi->ignore_compile_errors) {
        kvv_push_back_const_str(&kvv, 1, "Don't Penalize CE", "yes");
    }
    if (epi->enable_submit_after_reject) {
        kvv_push_back_const_str(&kvv, 1, "Allow Resubmit After Reject", "yes");
    }
        /*
    unsigned char enable_tokens;
    unsigned char tokens_for_user_ac;
        */
    if (epi->disable_submit_after_ok) {
        kvv_push_back_const_str(&kvv, 1, "Disable Resubmit After OK", "yes");
    }
    if (epi->hidden) {
        kvv_push_back_const_str(&kvv, 1, "Hidden in Standings", "yes");
    }
    if (epi->stand_hide_time) {
        kvv_push_back_const_str(&kvv, 1, "Time Hidden in Standings", "yes");
    }
    if (epi->stand_ignore_score) {
        kvv_push_back_const_str(&kvv, 1, "Score Ignored in Standings", "yes");
    }
    if (epi->stand_last_column) {
        kvv_push_back_const_str(&kvv, 1, "Last Column in Standings", "yes");
    }
    if (epi->stand_name && epi->stand_name[0]) {
        kvv_push_back_const_str(&kvv, 1, "Name in Standings", epi->stand_name);
    }
    if (epi->stand_column && epi->stand_column[0]) {
        kvv_push_back_const_str(&kvv, 1, "Column in Standings", epi->stand_column);
    }
    if (epi->disable_stderr) {
        kvv_push_back_const_str(&kvv, 1, "Output to stderr Prohibited", "yes");
    }
    if (epi->time_limit_ms > 0) {
        kvv_push_back_int(&kvv, 1, "Time Limit (ms)", epi->time_limit_ms);
    }
    if (epi->real_time_limit_ms > 0) {
        kvv_push_back_int(&kvv, 1, "Real Time Limit (ms)", epi->real_time_limit_ms);
    }
    if (epi->acm_run_penalty >= 0 && epi->acm_run_penalty != 20) {
        kvv_push_back_int(&kvv, 1, "ACM Run Penalty", epi->acm_run_penalty);
    }
    if (epi->test_score >= 0) {
        kvv_push_back_int(&kvv, 1, "Test Score", epi->test_score);
    }
    if (epi->run_penalty >= 0) {
        kvv_push_back_int(&kvv, 1, "Run Penalty", epi->run_penalty);
    }
    if (epi->disqualified_penalty >= 0) {
        kvv_push_back_int(&kvv, 1, "Disqualified Penalty", epi->disqualified_penalty);
    }
    if (epi->compile_error_penalty >= 0) {
        kvv_push_back_int(&kvv, 1, "Compile Error Penalty", epi->compile_error_penalty);
    }
    if (epi->tests_to_accept >= 0) {
        kvv_push_back_int(&kvv, 1, "Tests To Accept", epi->tests_to_accept);
    }
    if (epi->min_tests_to_accept >= 0) {
        kvv_push_back_int(&kvv, 1, "Min Tests To Accept", epi->min_tests_to_accept);
    }
    if (epi->score_multiplier > 1) {
        kvv_push_back_int(&kvv, 1, "Score Multiplier", epi->score_multiplier);
    }
    if (epi->max_user_run_count > 0) {
        kvv_push_back_int(&kvv, 1, "Max User Run Count", epi->max_user_run_count);
    }
    if (epi->start_date > 0) {
        kvv_push_back_move_str(&kvv, 1, "Problem Open Date", time_to_str(epi->start_date));
    }
    if (epi->deadline > 0) {
        kvv_push_back_move_str(&kvv, 1, "Deadline", time_to_str(epi->deadline));
    }
    if (epi->compiler_size > 0 && epi->compilers) {
    /*
FIXME: need contest state structure
    int compiler_size;
    unsigned char *compilers;
         */
    }
    if ((long long) epi->max_vm_size > 0) {
        unsigned char buf[128];
        kvv_push_back_copy_str(&kvv, 1, "Max VM Size", size_to_str(buf, sizeof(buf), epi->max_vm_size));
    }
    if ((long long) epi->max_stack_size > 0) {
        unsigned char buf[128];
        kvv_push_back_copy_str(&kvv, 1, "Max Stack Size", size_to_str(buf, sizeof(buf), epi->max_stack_size));
    } else if (epi->enable_max_stack_size > 0) {
        unsigned char buf[128];
        kvv_push_back_copy_str(&kvv, 1, "Max Stack Size", size_to_str(buf, sizeof(buf), epi->max_vm_size));
    }

    kvv_push_back_key(&kvv, 0, "Your statistics");
        /*

    unsigned char is_statement_avaiable;

    unsigned char is_viewable;
    unsigned char is_submittable;
    unsigned char is_tabable;
        */
    if (epi->is_solved) {
        kvv_push_back_const_str(&kvv, 1, "Solved", "yes");
    }
    if (epi->is_accepted) {
        kvv_push_back_const_str(&kvv, 1, "Accepted for Testing", "yes");
    }
    if (epi->is_pending) {
        kvv_push_back_const_str(&kvv, 1, "Pending Testing", "yes");
    }
    if (epi->is_pending_review) {
        kvv_push_back_const_str(&kvv, 1, "Pending Review", "yes");
    }
    if (epi->is_transient) {
        kvv_push_back_const_str(&kvv, 1, "Compiling or Running", "yes");
    }
    /*
    unsigned char is_last_untokenized;
    unsigned char is_marked;
    */
    if (epi->is_autook) {
        kvv_push_back_const_str(&kvv, 1, "OKed Automatically", "yes");
    }
    if (epi->is_rejected) {
        kvv_push_back_const_str(&kvv, 1, "Rejected", "yes");
    }
    /*
    unsigned char is_eff_time_needed;
    */
    if (epi->best_run >= 0) {
        kvv_push_back_int(&kvv, 1, "Best RunId", epi->best_run);
    }
    if (epi->attempts > 0) {
        kvv_push_back_int(&kvv, 1, "Failed Attempts", epi->attempts);
    }
    if (epi->disqualified > 0) {
        kvv_push_back_int(&kvv, 1, "Disqualified Attempts", epi->disqualified);
    }
    if (epi->ce_attempts > 0) {
        kvv_push_back_int(&kvv, 1, "Compile Errors", epi->ce_attempts);
    }
    if (epi->best_score > 0) {
        kvv_push_back_int(&kvv, 1, "Best Score", epi->best_score);
    }
    if (epi->prev_successes > 0) {
        kvv_push_back_int(&kvv, 1, "Previous Successes by Others", epi->prev_successes);
    }
    if (epi->all_attempts > 0) {
        kvv_push_back_int(&kvv, 1, "All Attempts", epi->all_attempts);
    }

    /*
    int eff_attempts;
    int token_count;
    */
    if (epi->effective_time > 0) {
        kvv_push_back_move_str(&kvv, 1, "Effective Time", time_to_str(epi->effective_time));
    }

    kvv_push_back_key(&kvv, 0, "Server information");
    kvv_push_back_move_str(&kvv, 1, "Server time", time_to_str(epi->server_time));

    kvv_generate(&kvv, text_f);
    fclose(text_f);
    epi->info_text = text_s;
    epi->info_size = text_z;
    kvv_free(&kvv);
}

void
ejfuse_run_info_text(struct EjRunInfo *eri)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);

    fprintf(text_f, "Problem information:\n");
    fprintf(text_f, "\tRun Id:\t\t%d\n", eri->run_id);

    fclose(text_f);
    eri->info_text = text_s;
    eri->info_size = text_z;
}

void
ejfuse_run_messages_text(struct EjRunMessages *erms)
{
    char *text_s = NULL;
    size_t text_z = 0;
    FILE *text_f = open_memstream(&text_s, &text_z);

    fprintf(text_f, "%s", erms->json_text);

    fclose(text_f);
    erms->text = text_s;
    erms->size = text_z;
}
