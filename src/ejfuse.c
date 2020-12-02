/* -*- mode: c; c-basic-offset: 4 -*- */
/* Copyright (C) 2018-2020 Alexander Chernov <cher@ejudge.ru> */

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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <termios.h>

#include "cJSON.h"
#include "inode_hash.h"
#include "contests_state.h"
#include "ejfuse.h"
#include "ops_generic.h"
#include "ops_root.h"
#include "ops_cnts.h"
#include "ops_cnts_info.h"
#include "ops_cnts_log.h"
#include "ops_fuse.h"
#include "ops_cnts_probs.h"
#include "ops_cnts_prob_dir.h"
#include "ops_cnts_prob_files.h"
#include "ops_cnts_prob_submit.h"
#include "ops_cnts_prob_submit_comp.h"
#include "ops_cnts_prob_submit_comp_dir.h"
#include "ejudge_client.h"
#include "ejfuse_file.h"
#include "submit_thread.h"
#include "ops_cnts_prob_runs.h"
#include "ops_cnts_prob_runs_run.h"
#include "ops_cnts_prob_runs_run_files.h"
#include "ops_cnts_prob_runs_run_tests.h"
#include "ops_cnts_prob_runs_run_tests_test.h"
#include "ops_cnts_prob_runs_run_tests_test_files.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <curl/curl.h>

enum { MAX_PROB_SHORT_NAME_SIZE = 32 };

enum { NODE_QUOTA = 1024 };
enum { SIZE_QUOTA = 16 * 1024 * 1024 };

int
ejf_generic_fgetattr(struct EjFuseRequest *efr, const char *path, struct stat *stb, struct fuse_file_info *ffi)
{
    return efr->ops->getattr(efr, path, stb);
}

long long
get_current_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long v1;
    if (__builtin_mul_overflow((long long) tv.tv_sec, 1000000LL, &v1)) {
        abort();
    }
    if (__builtin_add_overflow(v1, (long long) tv.tv_usec, &v1)) {
        abort();
    }
    return v1;
}

void
top_session_free(struct EjTopSession *tls)
{
    if (tls) {
        free(tls->log_s);
        free(tls->session_id);
        free(tls->client_key);
        free(tls);
    }
}

static void
contest_list_free(struct EjContestList *cl)
{
    if (cl) {
        free(cl->log_s);
        free(cl);
    }
}

int
request_free(struct EjFuseRequest *rq, int retval)
{
    return retval;
}

struct EjTopSession *
top_session_read_lock(struct EjFuseState *efs)
{
    struct EjTopSession *top_session = NULL;

    atomic_fetch_add_explicit(&efs->top_session_guard, 1, memory_order_acquire);
    top_session = atomic_load_explicit(&efs->top_session, memory_order_relaxed);
    atomic_fetch_add_explicit(&top_session->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&efs->top_session_guard, 1, memory_order_release);
    return top_session;
}

void
top_session_read_unlock(struct EjTopSession *tls)
{
    if (tls) {
        atomic_fetch_sub_explicit(&tls->reader_count, 1, memory_order_relaxed);
    }
}

int
top_session_try_write_lock(struct EjFuseState *efs)
{
    return atomic_exchange_explicit(&efs->top_session_update, 1, memory_order_relaxed);
}

void
top_session_set(struct EjFuseState *efs, struct EjTopSession *top_session)
{
    struct EjTopSession *old_session = atomic_exchange_explicit(&efs->top_session, top_session, memory_order_acquire);
    int expected = 0;
    // spinlock
    while (!atomic_compare_exchange_weak_explicit(&efs->top_session_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&efs->top_session_update, 0, memory_order_release);

    if (old_session) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old_session->reader_count, &expected, 0, memory_order_acquire, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        top_session_free(old_session);
    }
}

struct EjContestList *
contest_list_read_lock(struct EjFuseState *efs)
{
    struct EjContestList *contests = NULL;

    atomic_fetch_add_explicit(&efs->contests_guard, 1, memory_order_acquire);
    contests = atomic_load_explicit(&efs->contests, memory_order_relaxed);
    atomic_fetch_add_explicit(&contests->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&efs->contests_guard, 1, memory_order_release);
    return contests;
}

void
contest_list_read_unlock(struct EjContestList *contests)
{
    if (contests) {
        atomic_fetch_sub_explicit(&contests->reader_count, 1, memory_order_relaxed);
    }
}

int
contest_list_try_write_lock(struct EjFuseState *efs)
{
    return atomic_exchange_explicit(&efs->contests_update, 1, memory_order_relaxed);
}

void
contest_list_set(struct EjFuseState *efs, struct EjContestList *contests)
{
    struct EjContestList *old_contests = atomic_exchange_explicit(&efs->contests, contests, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&efs->contests_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&efs->contests_update, 0, memory_order_release);

    // spinlock
    if (old_contests) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old_contests->reader_count, &expected, 0, memory_order_acquire, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        contest_list_free(old_contests);
    }
}

struct EjContestListItem *
contest_list_find(const struct EjContestList *contests, int cnts_id)
{
    int low = 0, high = contests->count;
    while (low < high) {
        int med = (low + high) / 2;
        struct EjContestListItem *fcntx = &contests->entries[med];

        if (fcntx->id == cnts_id) {
            return fcntx;
        } else if (fcntx->id < cnts_id) {
            low = med + 1;
        } else {
            high = med;
        }
    }
    /*
    for (int i = 0; i < contests->count; ++i) {
        if (contests->entries[i].id == efr->contest_id) {
            return &contests->entries[i];
        }
    }
    */
    return NULL;
}

static _Bool
contests_is_valid(struct EjFuseState *efs, int cnts_id)
{
    struct EjContestList *contests = contest_list_read_lock(efs);
    struct EjContestListItem *cnts = contest_list_find(contests, cnts_id);
    contest_list_read_unlock(contests);
    return cnts != NULL;
}

void
top_session_maybe_update(struct EjFuseState *efs, long long current_time_us)
{
    int update_needed = 0;
    struct EjTopSession *top_session = top_session_read_lock(efs);
    if (top_session->ok) {
        if (top_session->expire_us > 0 && current_time_us >= top_session->expire_us - 100000000) { // 100s
            update_needed = 1;
        }
        if (top_session->recheck_time_us > 0 && current_time_us >= top_session->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (top_session->recheck_time_us > 0 && current_time_us < top_session->recheck_time_us) {
            update_needed = 0;
        }
    }
    top_session_read_unlock(top_session);

    if (update_needed) {
        top_session = calloc(1, sizeof(*top_session));
        ejudge_client_get_top_session_request(efs, current_time_us, top_session);
        top_session_set(efs, top_session);
    }
}

int
top_session_copy_session(struct EjFuseState *efs, struct EjSessionValue *esv)
{
    esv->ok = 0;
    struct EjTopSession *ets = top_session_read_lock(efs);
    if (!ets || !ets->ok) {
        top_session_read_unlock(ets);
        return 0;
    }
    esv->ok = 1;
    if (snprintf(esv->session_id, sizeof(esv->session_id), "%s", ets->session_id) >= sizeof(esv->session_id)) {
        abort();
    }
    if (snprintf(esv->client_key, sizeof(esv->client_key), "%s", ets->client_key) >= sizeof(esv->client_key)) {
        abort();
    }
    top_session_read_unlock(ets);
    return 1;
}


void
ej_get_contest_list(struct EjFuseState *efs, long long current_time_us)
{
    struct EjSessionValue esv = {};
    struct EjContestList *contests = NULL;

    if (!top_session_copy_session(efs, &esv)) return;
    if (contest_list_try_write_lock(efs)) return;

    contests = calloc(1, sizeof(*contests));

    ejudge_client_get_contest_list_request(efs, &esv, current_time_us, contests);

    contest_list_set(efs, contests);
    return;

}

void
contest_log_format(
        long long current_time_us,
        struct EjContestState *ecs,
        const char *action,
        int success,
        const char *format, ...)
    __attribute__((format(printf, 5, 6)));
void
contest_log_format(
        long long current_time_us,
        struct EjContestState *ecs,
        const char *action,
        int success,
        const char *format, ...)
{
    va_list args;
    unsigned char buf1[1024];
    unsigned char buf2[1024];

    buf1[0] = 0;
    if (format) {
        va_start(args, format);
        if (vsnprintf(buf1, sizeof(buf1), format, args) >= sizeof(buf1)) {
            abort();
        }
        va_end(args);
    }

    time_t tt = current_time_us / 1000000;
    struct tm ltm;
    localtime_r(&tt, &ltm);
    int res = snprintf(buf2, sizeof(buf2), "%04d-%02d-%02d %02d:%02d:%02d %s %s %s\n",
             ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec,
             action, success?"ok":"fail", buf1);
    if (res >= sizeof(buf2)) { abort(); }
    contest_log_append(ecs, buf2);
}

void
ejudge_client_enter_contest(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us)
{
    struct EjSessionValue esv = {};

    int already = contest_session_try_write_lock(ecs);
    if (already) return;

    if (!top_session_copy_session(efs, &esv)) return;

    struct EjContestSession *ecc = calloc(1, sizeof(*ecc));
    ecc->cnts_id = ecs->cnts_id;
    ejudge_client_enter_contest_request(efs, ecs, &esv, current_time_us, ecc);
    contest_session_set(ecs, ecc);
}

void
contest_session_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjContestSession *ecc = contest_session_read_lock(ecs);
    if (ecc->ok) {
        if (ecc->expire_us > 0 && current_time_us >= ecc->expire_us - 10000000) {
            update_needed = 1;
        }
        if (ecc->recheck_time_us > 0 && current_time_us >= ecc->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (ecc->recheck_time_us > 0 && current_time_us < ecc->recheck_time_us) {
            update_needed = 0;
        }
    }
    contest_session_read_unlock(ecc);
    if (!update_needed) return;

    top_session_maybe_update(efs, current_time_us);
    ejudge_client_enter_contest(efs, ecs, current_time_us);
}

void
ejudge_client_contest_info(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us)
{
    int already = contest_info_try_write_lock(ecs);
    if (already) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    struct EjContestInfo *eci = contest_info_create(ecs->cnts_id);
    ejudge_client_contest_info_request(efs, ecs, &esv, current_time_us, eci);
    ejfuse_contest_info_text(eci);
    contest_info_set(ecs, eci);
}

void
contest_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us)
{
    // contest session must be updated before
    int update_needed = 0;
    struct EjContestInfo *eci = contest_info_read_lock(ecs);
    if (eci->ok) {
        if (eci->recheck_time_us > 0 && current_time_us >= eci->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eci->recheck_time_us > 0 && current_time_us < eci->recheck_time_us){
            update_needed = 0;
        }
    }
    contest_info_read_unlock(eci);
    if (!update_needed) return;

    ejudge_client_contest_info(efs, ecs, current_time_us);
}

void
problem_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjProblemInfo *epi = problem_info_read_lock(eps);
    if (epi && epi->ok) {
        if (epi->recheck_time_us > 0 && current_time_us >= epi->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (epi && epi->recheck_time_us > 0 && current_time_us < epi->recheck_time_us) {
            update_needed = 0;
        }
    }
    problem_info_read_unlock(epi);
    if (!update_needed) return;

    int already = problem_info_try_write_lock(eps);
    if (already) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    epi = problem_info_create(eps->prob_id);
    ejudge_client_problem_info_request(efs, ecs, &esv, eps->prob_id, current_time_us, epi);
    ejfuse_problem_info_text(epi, ecs);
    problem_info_set(eps, epi);
}

void
problem_statement_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us)
{
    struct EjProblemInfo *epi = problem_info_read_lock(eps);
    if (!epi || !epi->ok || !epi->is_viewable || !epi->is_statement_avaiable) {
        problem_info_read_unlock(epi);
        return;
    }
    problem_info_read_unlock(epi);

    int update_needed = 0;
    struct EjProblemStatement *eph = problem_statement_read_lock(eps);
    if (eph && eph->ok) {
        if (eph->recheck_time_us > 0 && current_time_us >= eph->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eph && eph->recheck_time_us > 0 && current_time_us < eph->recheck_time_us) {
            update_needed = 0;
        }
    }
    problem_statement_read_unlock(eph);
    if (!update_needed) return;

    int already = problem_statement_try_write_lock(eps);
    if (already) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    eph = problem_statement_create(eps->prob_id);
    ejudge_client_problem_statement_request(efs, ecs, &esv, eps->prob_id, current_time_us, eph);
    problem_statement_set(eps, eph);
}

void
problem_runs_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjProblemRuns *eprs = problem_runs_read_lock(eps);
    if (eprs && eprs->ok) {
        if (eprs->recheck_time_us > 0 && current_time_us >= eprs->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eprs && eprs->recheck_time_us > 0 && current_time_us < eprs->recheck_time_us) {
            update_needed = 0;
        }
    }
    problem_runs_read_unlock(eprs);
    if (!update_needed) return;

    int already = problem_runs_try_write_lock(eps);
    if (already) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    eprs = problem_runs_create(eps->prob_id);
    ejudge_client_problem_runs_request(efs, ecs, &esv, eps->prob_id, current_time_us, eprs);
    problem_runs_set(eps, eprs);
}

void
run_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjRunState *ers,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjRunInfo *eri = run_info_read_lock(ers);
    if (eri && eri->ok) {
        if (eri->recheck_time_us > 0 && current_time_us >= eri->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eri && eri->recheck_time_us > 0 && current_time_us < eri->recheck_time_us) {
            update_needed = 0;
        }
    }
    run_info_read_unlock(eri);
    if (!update_needed) return;

    if (run_info_try_write_lock(ers)) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    eri = run_info_create(ers->run_id);
    ejudge_client_run_info_request(efs, ecs, &esv, ers->run_id, current_time_us, eri);
    ejfuse_run_info_text(eri, ecs);
    run_info_set(ers, eri);
}

void
run_source_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjRunState *ers,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjRunSource *ert = run_source_read_lock(ers);
    if (ert && ert->ok) {
        if (ert->recheck_time_us > 0 && current_time_us >= ert->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (ert && ert->recheck_time_us > 0 && current_time_us < ert->recheck_time_us) {
            update_needed = 0;
        }
    }
    run_source_read_unlock(ert);
    if (!update_needed) return;

    if (run_source_try_write_lock(ers)) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    ert = run_source_create(ers->run_id);
    ejudge_client_run_source_request(efs, ecs, &esv, ers->run_id, current_time_us, ert);
    run_source_set(ers, ert);
}

void
run_messages_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjRunState *ers,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjRunMessages *erms = run_messages_read_lock(ers);
    if (erms && erms->ok) {
        if (erms->recheck_time_us > 0 && current_time_us >= erms->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (erms && erms->recheck_time_us > 0 && current_time_us < erms->recheck_time_us) {
            update_needed = 0;
        }
    }
    run_messages_read_unlock(erms);
    if (!update_needed) return;

    if (run_messages_try_write_lock(ers)) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    erms = run_messages_create(ers->run_id);
    ejudge_client_run_messages_request(efs, ecs, &esv, ers->run_id, current_time_us, erms);
    ejfuse_run_messages_text(erms);
    run_messages_set(ers, erms);
}

void
run_test_data_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjRunTest *ert,
        int run_id,
        int index,
        long long current_time_us)
{
    int update_needed = 0;
    struct EjRunTestData *ertd = run_test_data_read_lock(ert, index);
    if (ertd && ertd->ok) {
        if (ertd->recheck_time_us > 0 && current_time_us >= ertd->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (ertd && ertd->recheck_time_us > 0 && current_time_us < ertd->recheck_time_us) {
            update_needed = 0;
        }
    }
    run_test_data_read_unlock(ertd);
    if (!update_needed) return;

    if (run_test_data_try_write_lock(ert, index)) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    ertd = run_test_data_create();
    ejudge_client_run_test_request(efs, ecs, &esv, run_id, ert->num, index, current_time_us, ertd);
    run_test_data_set(ert, index, ertd);
}

unsigned
get_inode(struct EjFuseState *efs, const char *path)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    size_t len = strlen(path);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, path, len);
    SHA256_Final(digest, &ctx);

    return inode_hash_insert(efs->inode_hash, digest)->inode;
}

unsigned char *
fix_name(unsigned char *str)
{
    for (unsigned char *p = str; *p; ++p) {
        if (*p < ' ' || *p == 0x7f || *p == '/') {
            *p = '_';
        }
    }
    return str;
}

static int
find_problem(struct EjFuseRequest *efr, const unsigned char *name_or_id)
{
    struct EjContestInfo *eci = contest_info_read_lock(efr->ecs);
    for (int prob_id = 1; prob_id < eci->prob_size; ++prob_id) {
        struct EjContestProblem *tmp = eci->probs[prob_id];
        if (tmp && tmp->short_name && !strcmp(name_or_id, tmp->short_name)) {
            efr->prob_id = prob_id;
            contest_info_read_unlock(eci);
            efr->eps = problem_states_get(efr->ecs->prob_states, efr->prob_id);
            return prob_id;
        }
    }

    errno = 0;
    char *eptr = NULL;
    long val = strtol(name_or_id, &eptr, 10);
    if (*eptr || errno || (unsigned char *) eptr == name_or_id || (int) val != val || val <= 0 || val >= eci->prob_size || !eci->probs[val]) {
        contest_info_read_unlock(eci);
        return -ENOENT;
    }
    efr->prob_id = val;
    contest_info_read_unlock(eci);
    efr->eps = problem_states_get(efr->ecs->prob_states, efr->prob_id);
    return val;
}

static int
find_compiler(struct EjFuseRequest *efr, const unsigned char *name_or_id)
{
    struct EjContestInfo *eci = contest_info_read_lock(efr->ecs);
    for (int lang_id = 1; lang_id < eci->compiler_size; ++lang_id) {
        struct EjContestCompiler *ecl = eci->compilers[lang_id];
        if (ecl && ecl->short_name && !strcmp(name_or_id, ecl->short_name)) {
            efr->lang_id = lang_id;
            contest_info_read_unlock(eci);
            return lang_id;
        }
    }

    errno = 0;
    char *eptr = NULL;
    long val = strtol(name_or_id, &eptr, 10);
    if (*eptr || errno || (unsigned char *) eptr == name_or_id || (int) val != val || val < 0) {
        contest_info_read_unlock(eci);
        return -ENOENT;
    }
    if (val > 0) {
        if (val >= eci->compiler_size || !eci->compilers[val]) {
            contest_info_read_unlock(eci);
            return -ENOENT;
        }
    }
    efr->lang_id = val;
    contest_info_read_unlock(eci);
    return val;
}

static int
recognize_special_file_names(const unsigned char *file_name)
{
    if (!file_name) return 0;
    if (!strcmp(file_name, "INFO")) {
        return FILE_NAME_INFO;
    }
    if (!strcmp(file_name, "info.json")) {
        return FILE_NAME_INFO_JSON;
    }
    if (!strcmp(file_name, "statement.html")) {
        return FILE_NAME_STATEMENT_HTML;
    }
    if (!strcmp(file_name, "compiler.txt")) {
        return FILE_NAME_COMPILER_TXT;
    }
    if (!strcmp(file_name, "valuer.txt")) {
        return FILE_NAME_VALUER_TXT;
    }
    if (!strcmp(file_name, "source")) {
        return FILE_NAME_SOURCE;
    }
    if (!strncmp(file_name, "source.", 7)) {
        return FILE_NAME_SOURCE;
    }
    if (!strcmp(file_name, "messages.txt")) {
        return FILE_NAME_MESSAGES_TXT;
    }
    if (!strcmp(file_name, "tests")) {
        return FILE_NAME_TESTS;
    }
    return 0;
}

/*
 * /<CNTS>/problems/<PROB>/runs/...
 *                             ^ path
 */
static int
ejf_process_path_runs(const char *path, struct EjFuseRequest *efr)
{
    unsigned char name_buf[NAME_MAX + 1];
    if (path[0] != '/') return -ENOENT;
    const char *p1 = strchr(path + 1, '/');
    const char *comma = strchr(path + 1, ',');
    int len = 0;
    if (!p1) {
        if (!comma) {
            len = strlen(path + 1);
        } else {
            len = comma - path - 1;
        }
    } else {
        if (!comma || comma > p1) {
            len = p1 - path - 1;
        } else {
            len = comma - path - 1;
        }
    }
    if (len > NAME_MAX) {
        return -ENOENT;
    }
    memcpy(name_buf, path + 1, len);
    name_buf[len] = 0;

    efr->file_name_code = recognize_special_file_names(name_buf);
    if (efr->file_name_code == FILE_NAME_INFO || efr->file_name_code == FILE_NAME_INFO_JSON) {
        if (p1) {
            return -ENOTDIR;
        }
        // FIXME: handle these files
        return -ENOENT;
    }

    errno = 0;
    char *eptr = NULL;
    long val = strtol(name_buf, &eptr, 10);
    if (errno || *eptr || (unsigned char *) eptr == name_buf || val < 0 || (int) val != val) {
        return -ENOENT;
    }

    problem_runs_maybe_update(efr->efs, efr->ecs, efr->eps, efr->current_time_us);
    struct EjProblemRuns *eprs = problem_runs_read_lock(efr->eps);
    if (!eprs || !eprs->ok || eprs->size <= 0) {
        problem_runs_read_unlock(eprs);
        return -ENOENT;
    }
    struct EjProblemRun *epr = problem_runs_find_unlocked(eprs, val);
    if (!epr) {
        problem_runs_read_unlock(eprs);
        return -ENOENT;
    }
    efr->run_id = epr->run_id;
    problem_runs_read_unlock(eprs);

    if (!p1) {
        efr->ops = &ejfuse_contest_problem_runs_run_operations;
        return 0;
    }

    efr->ers = run_states_get(efr->ecs->run_states, efr->run_id);
    if (!efr->ers) return -ENOENT;
    run_info_maybe_update(efr->efs, efr->ecs, efr->ers, efr->current_time_us);
    const char *p2 = strchr(p1 + 1, '/');
    if (!p2) {
        efr->file_name = p1 + 1;
        efr->file_name_code = recognize_special_file_names(efr->file_name);
        if (efr->file_name_code == FILE_NAME_TESTS) {
            efr->ops = &ejfuse_contest_problem_runs_run_tests_operations;
        } else {
            efr->ops = &ejfuse_contest_problem_runs_run_files_operations;
        }
        return 0;
    }

    len = p2 - p1 - 1;
    if (len > NAME_MAX) {
        return -ENOENT;
    }
    memcpy(name_buf, p1 + 1, len);
    name_buf[len] = 0;
    if (strcmp(name_buf, "tests") != 0) {
        return -ENOENT;
    }

    const char *p3 = strchr(p2 + 1, '/');
    if (!p3) {
        len = strlen(p2 + 1);
    } else {
        len = p3 - p2 - 1;
    }
    if (len > NAME_MAX) {
        return -ENOENT;
    }
    memcpy(name_buf, p2 + 1, len);
    name_buf[len] = 0;

    errno = 0;
    eptr = NULL;
    val = strtol(name_buf, &eptr, 10);
    if (errno || *eptr || (unsigned char *) eptr == name_buf || val <= 0 || (int) val != val) {
        return -ENOENT;
    }

    struct EjRunInfo *eri = run_info_read_lock(efr->ers);
    if (!eri || !eri->ok || !eri->is_test_available) {
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    struct EjRunInfoTestResult *eritr = run_info_get_test_result_unlocked(eri, val);
    if (!eritr || !eritr->is_visibility_full) {
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    efr->num = val;
    if (!(efr->ert = run_tests_get(efr->ers->tests, efr->num))) {
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    run_info_read_unlock(eri);

    if (!p3) {
        efr->ops = &ejfuse_contest_problem_runs_run_tests_test_operations;
        return 0;
    }

/*
 * /<CNTS>/problems/<PROB>/runs/RUN-ID/tests/NUM/file
 *                             ^ path
 *                                    ^ p1
 *                                          ^ p2
 *                                              ^ p3
 */
    const char *p4 = strchr(p3 + 1, '/');
    if (!p4) {
        len = strlen(p3 + 1);
        if (len > NAME_MAX) {
            return -ENOENT;
        }
        memcpy(name_buf, p3 + 1, len);
        name_buf[len] = 0;
        efr->file_name = p3 + 1;
        int index = testing_info_parse(name_buf);
        if (index < 0) return -ENOENT;
        efr->test_file_index = index;
        efr->ops = &ejfuse_contest_problem_runs_run_tests_test_files_operations;
        return 0;
    }

    return -ENOENT;
}

/*
 * /<CNTS>/problems/<PROB>/submit/<LANG>...
 *                               ^ path
 */
static int
ejf_process_path_submit(const char *path, struct EjFuseRequest *efr)
{
    //file_nodes_list(efr->efs->file_nodes);

    unsigned char lang_buf[NAME_MAX + 1];
    if (path[0] != '/') return -ENOENT;
    const char *p1 = strchr(path + 1, '/');
    const char *comma = strchr(path + 1, ',');
    int len = 0;
    if (!p1) {
        if (!comma) {
            len = strlen(path + 1);
        } else {
            len = comma - path - 1;
        }
    } else {
        if (!comma || comma > p1) {
            len = p1 - path - 1;
        } else {
            len = comma - path - 1;
        }
    }
    if (len > NAME_MAX) {
        return -ENOENT;
    }
    memcpy(lang_buf, path + 1, len);
    lang_buf[len] = 0;
    if (find_compiler(efr, lang_buf) < 0) {
        return -ENOENT;
    }

    if (!p1) {
        efr->ops = &ejfuse_contest_problem_submit_compiler_operations;
        return 0;
    }
/*
 * /<CNTS>/problems/<PROB>/submit/<LANG>/file
 *                               ^ path
 *                                      ^ p1
 */
    const char *p2 = strchr(p1 + 1, '/');
    if (!p2) {
        efr->file_name = p1 + 1;
        efr->ops = &ejfuse_contest_problem_submit_compiler_dir_operations;
        return 0;
    }
/*
 * /<CNTS>/problems/<PROB>/submit/<LANG>/file/...
 *                               ^ path
 *                                      ^ p1
 *                                           ^ p2
 */
    return -ENOENT;
}

int
ejf_process_path(const char *path, struct EjFuseRequest *efr)
{
    memset(efr, 0, sizeof(*efr));
    efr->fx = fuse_get_context();
    efr->efs = (struct EjFuseState *) efr->fx->private_data;
    efr->current_time_us = get_current_time();
    // safety
    if (!path || path[0] != '/') {
        return -ENOENT;
    }
    // then process the path
    if (!strcmp(path, "/")) {
        efr->ops = &ejfuse_root_operations;
        return 0;
    }
    int len = strlen(path);
    if (path[len - 1] == '/') {
        return -ENOENT;
    }
    const char *p1;
    if (!(p1 = strchr(path + 1, '/'))) {
        // parse the contest identifier
        char *eptr = NULL;
        errno = 0;
        long cnts_id = strtol(path + 1, &eptr, 10);
        if (path + 1 == eptr) return -ENOENT;
        if (errno) return -ENOENT;
        if (*eptr && *eptr != ',') return -ENOENT;
        if (cnts_id <= 0 || (int) cnts_id != cnts_id) return -ENOENT;
        efr->contest_id = cnts_id;
        efr->ops = &ejfuse_contest_operations;
        return 0;
    }
    {
        char *eptr = NULL;
        errno = 0;
        long cnts_id = strtol(path + 1, &eptr, 10);
        if (path + 1 == eptr) return -ENOENT;
        if (errno) return -ENOENT;
        if (*eptr != '/' && *eptr != ',') return -ENOENT;
        if (cnts_id <= 0 || (int) cnts_id != cnts_id) return -ENOENT;
        efr->contest_id = cnts_id;
    }
    if (!contests_is_valid(efr->efs, efr->contest_id)) {
        return -ENOENT;
    }
    if (!(efr->ecs = contests_state_get(efr->efs->contests_state, efr->contest_id))) {
        return -ENOENT;
    }
    const char *p2 = strchr(p1 + 1, '/');
    if (!p2) {
        efr->file_name = p1 + 1;
        efr->file_name_code = recognize_special_file_names(p1 + 1);
        if (efr->file_name_code == FILE_NAME_INFO || efr->file_name_code == FILE_NAME_INFO_JSON) {
            contest_session_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
            contest_info_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
            efr->ops = &ejfuse_contest_info_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "LOG")) {
            efr->ops = &ejfuse_contest_log_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "problems")) {
            contest_session_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
            contest_info_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
            efr->ops = &ejfuse_contest_problems_operations;
            return 0;
        }
        return -ENOENT;
    }
    contest_session_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
    contest_info_maybe_update(efr->efs, efr->ecs, efr->current_time_us);
    // next component: [p1 + 1, p2)
    // check for problems
    if (p2 - p1 - 1 != 8 || memcmp(p1 + 1, "problems", 8)) {
        // ENOENT or ENOTDIR, choose one
        return -ENOENT;
    }
    const char *p3 = strchr(p2 + 1, '/');
    if (!p3) {
        // problem directory
        const char *comma = strchr(p2 + 1, ',');
        unsigned char prob_name_buf[64];
        int len = 0;
        if (comma) {
            len = comma - p2 - 1;
        } else {
            len = strlen(p2 + 1);
        }
        if (len >= MAX_PROB_SHORT_NAME_SIZE) {
            return -ENOENT;
        }
        memcpy(prob_name_buf, p2 + 1, len);
        prob_name_buf[len] = 0;
        if (find_problem(efr, prob_name_buf) < 0) {
            return -ENOENT;
        }
        efr->ops = &ejfuse_contest_problem_operations;
        return 0;
    } else {
        const char *comma = strchr(p2 + 1, ',');
        unsigned char prob_name_buf[64];
        int len = 0;
        if (comma && comma < p3) {
            len = comma - p2 - 1;
        } else {
            len = p3 - p2 - 1;
        }
        if (len >= MAX_PROB_SHORT_NAME_SIZE) {
            return -ENOENT;
        }
        memcpy(prob_name_buf, p2 + 1, len);
        prob_name_buf[len] = 0;
        if (find_problem(efr, prob_name_buf) < 0) {
            return -ENOENT;
        }
        problem_info_maybe_update(efr->efs, efr->ecs, efr->eps, efr->current_time_us);
    }
    const char *p4 = strchr(p3 + 1, '/');
    if (!p4) {
        efr->file_name = p3 + 1;
        efr->file_name_code = recognize_special_file_names(efr->file_name);
        if (efr->file_name_code == FILE_NAME_INFO
            || efr->file_name_code == FILE_NAME_INFO_JSON
            || efr->file_name_code == FILE_NAME_STATEMENT_HTML) {
            efr->ops = &ejfuse_contest_problem_files_operations;
            return 0;
        } else if (!strcmp(p3 + 1, "runs")) {
            efr->ops = &ejfuse_contest_problem_runs_operations;
            return 0;
        } else if (!strcmp(p3 + 1, "submit")) {
            efr->ops = &ejfuse_contest_problem_submit_operations;
            return 0;
        }
        return -ENOENT;
    }
    // next component: [p3 + 1, p4)
    unsigned char pp3[NAME_MAX + 1];
    int pp3l = p4 - p3 - 1;
    if (pp3l > NAME_MAX) {
        return -ENOENT;
    }
    memcpy(pp3, p3 + 1, pp3l);
    pp3[pp3l] = 0;
    if (!strcmp(pp3, "submit")) {
        return ejf_process_path_submit(p4, efr);
    } else if (!strcmp(pp3, "runs")) {
        return ejf_process_path_runs(p4, efr);
    }

    return -ENOENT;
}

int main(int argc, char *argv[])
{
    unsigned char *ej_user = NULL;
    unsigned char *ej_password = NULL;
    const unsigned char *ej_url = NULL;

    int work = 0;
    do {
        work = 0;
        if (argc >= 3 && !strcmp(argv[1], "--user")) {
            if (ej_user) {
                fprintf(stderr, "--user specified more than once\n");
                return 1;
            }
            ej_user = strdup(argv[2]);
            char *ptr = argv[1];
            while (*ptr) *ptr++ = 0;
            ptr = argv[2];
            while (*ptr) *ptr++ = 0;
            memmove(&argv[1], &argv[3], (argc - 2) * sizeof(argv[0]));
            argc -= 2;
            work = 1;
        } else if (argc >= 3 && !strcmp(argv[1], "--password")) {
            if (ej_password) {
                fprintf(stderr, "--password specified more than once\n");
                return 1;
            }
            ej_password = strdup(argv[2]);
            char *ptr = argv[1];
            while (*ptr) *ptr++ = 0;
            ptr = argv[2];
            while (*ptr) *ptr++ = 0;
            memmove(&argv[1], &argv[3], (argc - 2) * sizeof(argv[0]));
            argc -= 2;
            work = 1;
        } else if (argc >= 3 && !strcmp(argv[1], "--url")) {
            if (ej_url) {
                fprintf(stderr, "--url specified more than once\n");
                return 1;
            }
            ej_url = argv[2];
            memmove(&argv[1], &argv[3], (argc - 2) * sizeof(argv[0]));
            argc -= 2;
            work = 1;
        }
    } while (work);
    if (!ej_user && isatty(0)) {
        fprintf(stdout, "Login: "); fflush(stdout);
        size_t n = 0;
        ssize_t sz = getline((char**) &ej_user, &n, stdin);
        if (sz < 0) {
            fprintf(stderr, "--user not specified\n");
            return 1;
        }
        while (sz > 0 && isspace(ej_user[sz - 1])) --sz;
        ej_user[sz] = 0;
        if (!sz) {
            free(ej_user); ej_user = NULL;
        }
        free(ej_password); ej_password = NULL;
    }
    if (!ej_user) {
        fprintf(stderr, "--user not specified\n");
        return 1;
    }
    if (!ej_password && isatty(0)) {
        struct termios old, new;
        if (tcgetattr(0, &old) >= 0) {
            new = old;
            new.c_lflag &= ~ECHO;
            if (tcsetattr(0, TCSAFLUSH, &new) >= 0) {
                fprintf(stdout, "Password: "); fflush(stdout);
                size_t n = 0;
                ssize_t sz = getline((char**) &ej_password, &n, stdin);
                if (sz > 0) {
                    while (sz > 0 && isspace(ej_password[sz - 1])) --sz;
                    ej_password[sz] = 0;
                    if (!sz) {
                        free(ej_password); ej_password = NULL;
                    }
                } else {
                    free(ej_password); ej_password = NULL;
                }
                tcsetattr(0, TCSAFLUSH, &old);
            }
        }
    }
    if (!ej_password) {
        fprintf(stderr, "--password not specified\n");
        return 1;
    }
    if (!ej_url) {
        fprintf(stderr, "--url not specified\n");
        return 1;
    }

    CURLcode curle = curl_global_init(CURL_GLOBAL_ALL);
    if (curle != CURLE_OK) {
        fprintf(stderr, "failed to initialize CURL\n");
        return 1;
    }

    struct EjFuseState *efs = calloc(1, sizeof(*efs));
    efs->url = strdup(ej_url);
    efs->login = strdup(ej_user);
    efs->password = strdup(ej_password);
    efs->owner_uid = getuid();
    efs->owner_gid = getgid();
    efs->inode_hash = inode_hash_create();
    efs->contests_state = contests_state_create();
    efs->file_nodes = file_nodes_create(NODE_QUOTA, SIZE_QUOTA);
    efs->submit_thread = submit_thread_create();

    //submit_thread_start(efs->submit_thread, efs);

    long long current_time_us = get_current_time();
    efs->start_time_us = current_time_us;

    efs->top_session = calloc(1, sizeof(*efs->top_session));
    ejudge_client_get_top_session_request(efs, current_time_us, efs->top_session);
    if (!efs->top_session->ok) {
        fprintf(stderr, "initial login failed: %s\n", efs->top_session->log_s);
        return 1;
    }
    ej_get_contest_list(efs, current_time_us);
    if (!efs->contests->ok) {
        fprintf(stderr, "initial contest list failed: %s\n", efs->top_session->log_s);
        return 1;
    }

    int retval = fuse_main(argc, argv, &ejf_fuse_operations, efs);
    free(efs);
    return retval;
}
