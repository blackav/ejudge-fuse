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
        fprintf(text_f, "\tMax VM Size:\t\t%s\n", size_to_str(buf, sizeof(buf), epi->max_vm_size));
    }
    if ((long long) epi->max_stack_size > 0) {
        unsigned char buf[128];
        fprintf(text_f, "\tMax Stack Size:\t\t%s\n", size_to_str(buf, sizeof(buf), epi->max_stack_size));
    } else if (epi->enable_max_stack_size > 0) {
        unsigned char buf[128];
        fprintf(text_f, "\tMax Stack Size:\t\t%s\n", size_to_str(buf, sizeof(buf), epi->max_vm_size));
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
