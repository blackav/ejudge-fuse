/* Copyright (C) 2018-2023 Alexander Chernov <cher@ejudge.ru> */

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

#include "contests_state.h"
#include "ejfuse_file.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

struct EjContestsState
{
    pthread_rwlock_t rwl;

    int reserved;
    int size;
    struct EjContestState **entries;
};

struct EjProblemStates
{
    pthread_rwlock_t rwl;

    int size;
    struct EjProblemState **entries;
};

struct EjRunStates
{
    pthread_rwlock_t rwl;

    // sorted by increasing run_id
    int reserved;
    int size;
    struct EjRunState **runs;
};

struct EjRunTests
{
    pthread_rwlock_t rwl;

    // indexed by (num - 1)
    int reserved;
    int size;
    struct EjRunTest **tests;
};

struct EjProblemSubmits
{
    pthread_rwlock_t rwl;

    // sorted by increasing lang_id
    int reserved;
    int size;
    struct EjProblemCompilerSubmits **submits;
};

struct EjContestSession *
contest_session_create(int cnts_id)
{
    struct EjContestSession *ecc = calloc(1, sizeof(*ecc));
    ecc->cnts_id = cnts_id;
    return ecc;
}

void
contest_session_free(struct EjContestSession *ecc)
{
    if (ecc) {
        free(ecc);
    }
}

struct EjContestProblem *
contest_problem_create(int prob_id)
{
    struct EjContestProblem *ecp = calloc(1, sizeof(*ecp));
    ecp->id = prob_id;
    return ecp;
}

void
contest_problem_free(struct EjContestProblem *ecp)
{
    if (ecp) {
        free(ecp->short_name);
        free(ecp->long_name);
        free(ecp);
    }
}

struct EjContestCompiler *
contest_language_create(int lang_id)
{
    struct EjContestCompiler *ecl = calloc(1, sizeof(*ecl));
    ecl->id = lang_id;
    return ecl;
}

void
contest_language_free(struct EjContestCompiler *ecl)
{
    if (ecl) {
        free(ecl->short_name);
        free(ecl->long_name);
        free(ecl->src_suffix);
        free(ecl);
    }
}

struct EjContestInfo *
contest_info_create(int cnts_id)
{
    struct EjContestInfo *eci = calloc(1, sizeof(*eci));
    eci->cnts_id = cnts_id;
    return eci;
}

void
contest_info_free(struct EjContestInfo *eci)
{
    if (eci) {
        free(eci->log_s);
        for (int i = 0; i < eci->prob_size; ++i) {
            contest_problem_free(eci->probs[i]);
        }
        free(eci->probs);
        for (int i = 0; i < eci->compiler_size; ++i) {
            contest_language_free(eci->compilers[i]);
        }
        free(eci->compilers);
        free(eci->info_json_text);
        free(eci->info_text);
        free(eci->name);
        free(eci);
    }
}

struct EjContestLog *
contest_log_create(const unsigned char *init_str)
{
    struct EjContestLog *ecl = calloc(1, sizeof(*ecl));
    if (init_str) {
        ecl->text = strdup(init_str);
    }
    return ecl;
}

void
contest_log_free(struct EjContestLog *ecl)
{
    if (ecl) {
        free(ecl->text);
        free(ecl);
    }
}

struct EjContestState *
contest_state_create(int cnts_id)
{
    struct EjContestState *ecs = calloc(1, sizeof(*ecs));
    ecs->cnts_id = cnts_id;
    atomic_store_explicit(&ecs->info, contest_info_create(cnts_id), memory_order_relaxed);
    pthread_mutex_init(&ecs->log_mutex, NULL);
    atomic_store_explicit(&ecs->log, contest_log_create(""), memory_order_relaxed);
    atomic_store_explicit(&ecs->session, contest_session_create(cnts_id), memory_order_relaxed);
    ecs->prob_states = problem_states_create();
    ecs->run_states = run_states_create();
    return ecs;
}

void
contest_state_free(struct EjContestState *ecs)
{
    if (ecs) {
        contest_info_free(ecs->info);
        contest_log_free(ecs->log);
        pthread_mutex_destroy(&ecs->log_mutex);
        contest_session_free(ecs->session);
        problem_states_free(ecs->prob_states);
        run_states_free(ecs->run_states);
        free(ecs);
    }
}

struct EjContestsState *
contests_state_create(void)
{
    struct EjContestsState *ecss = calloc(1, sizeof(*ecss));
    pthread_rwlock_init(&ecss->rwl, NULL);
    return ecss;
}

void
contests_state_free(struct EjContestsState *ecss)
{
    if (ecss) {
        pthread_rwlock_destroy(&ecss->rwl);
        free(ecss);
    }
}

struct EjContestState *
contests_state_get(struct EjContestsState *ecss, int cnts_id)
{
    pthread_rwlock_rdlock(&ecss->rwl);
    if (ecss->size > 0) {
        int low = 0, high = ecss->size;
        while (low < high) {
            int mid = (low + high) / 2;
            struct EjContestState *ecs = ecss->entries[mid];
            if (ecs->cnts_id == cnts_id) {
                pthread_rwlock_unlock(&ecss->rwl);
                return ecs;
            } else if (ecs->cnts_id < cnts_id) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }
    pthread_rwlock_unlock(&ecss->rwl);
    pthread_rwlock_wrlock(&ecss->rwl);
    struct EjContestState *retval = NULL;
    if (ecss->reserved <= 0) {
        ecss->reserved = 16;
        ecss->entries = calloc(ecss->reserved, sizeof(ecss->entries[0]));
    }
    if (ecss->size > 0) {
        int low = 0, high = ecss->size;
        while (low < high) {
            int mid = (low + high) / 2;
            struct EjContestState *ecs = ecss->entries[mid];
            if (ecs->cnts_id == cnts_id) {
                pthread_rwlock_unlock(&ecss->rwl);
                return ecs;
            } else if (ecs->cnts_id < cnts_id) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        if (ecss->size == ecss->reserved) {
            ecss->entries = realloc(ecss->entries, (ecss->reserved *= 2) * sizeof(ecss->entries[0]));
        }
        if (low < ecss->size) {
            memmove(&ecss->entries[low + 1], &ecss->entries[low], (ecss->size - low) * sizeof(ecss->entries[0]));
            retval = ecss->entries[low] = contest_state_create(cnts_id);
            ++ecss->size;
        } else {
            retval = ecss->entries[ecss->size++] = contest_state_create(cnts_id);
        }
    } else {
        ecss->size = 1;
        retval = ecss->entries[0] = contest_state_create(cnts_id);
    }
    pthread_rwlock_unlock(&ecss->rwl);
    return retval;
}

// return false if there's no session and true if session valid
int
contest_state_copy_session(struct EjContestState *ecs, struct EjSessionValue *esv)
{
    esv->ok = 0;
    struct EjContestSession *ecc = contest_session_read_lock(ecs);
    if (!ecc || !ecc->ok) {
        contest_session_read_unlock(ecc);
        return 0;
    }
    esv->ok = 1;
    if (snprintf(esv->session_id, sizeof(esv->session_id), "%s", ecc->session_id) >= sizeof(esv->session_id)) {
        abort();
    }
    if (snprintf(esv->client_key, sizeof(esv->client_key), "%s", ecc->client_key) >= sizeof(esv->client_key)) {
        abort();
    }
    contest_session_read_unlock(ecc);
    return 1;
}

struct EjContestInfo *
contest_info_read_lock(struct EjContestState *ecs)
{
    atomic_fetch_add_explicit(&ecs->info_guard, 1, memory_order_acquire);
    struct EjContestInfo *data = atomic_load_explicit(&ecs->info, memory_order_relaxed);
    atomic_fetch_add_explicit(&data->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ecs->info_guard, 1, memory_order_release);
    return data;
}

struct EjContestInfo *
contests_info_read_lock(struct EjContestsState *ecss, int cnts_id)
{
    return contest_info_read_lock(contests_state_get(ecss, cnts_id));
}

void
contest_info_read_unlock(struct EjContestInfo *eci)
{
    if (eci) {
        atomic_fetch_sub_explicit(&eci->reader_count, 1, memory_order_relaxed);
    }
}

int
contest_info_try_write_lock(struct EjContestState *ecs)
{
    return atomic_exchange_explicit(&ecs->info_update, 1, memory_order_acquire);
}

void
contest_info_set(struct EjContestState *ecs, struct EjContestInfo *ecd)
{
    struct EjContestInfo *old = atomic_exchange_explicit(&ecs->info, ecd, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ecs->info_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ecs->info_update, 0, memory_order_release);

    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_acquire, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        contest_info_free(old);
    }
}

struct EjContestLog *
contest_log_read_lock(struct EjContestState *ecs)
{
    atomic_fetch_add_explicit(&ecs->log_guard, 1, memory_order_acquire);
    struct EjContestLog *log = atomic_load_explicit(&ecs->log, memory_order_relaxed);
    atomic_fetch_add_explicit(&log->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ecs->log_guard, 1, memory_order_release);
    return log;
}

struct EjContestLog *
contests_log_read_lock(struct EjContestsState *ecss, int cnts_id)
{
    return contest_log_read_lock(contests_state_get(ecss, cnts_id));
}

void
contests_log_read_unlock(struct EjContestLog *ecl)
{
    if (ecl) {
        atomic_fetch_sub_explicit(&ecl->reader_count, 1, memory_order_relaxed);
    }
}

struct EjContestState *
contests_log_write_lock(struct EjContestsState *ecss, int cnts_id)
{
    struct EjContestState *ecs = contests_state_get(ecss, cnts_id);
    if (!ecs) abort();

    pthread_mutex_lock(&ecs->log_mutex);
    return ecs;
}

void
contest_log_set(struct EjContestState *ecs, struct EjContestLog *ecl)
{
    struct EjContestLog *old = atomic_exchange_explicit(&ecs->log, ecl, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ecs->log_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    pthread_mutex_unlock(&ecs->log_mutex);

    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_acquire, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        contest_log_free(old);
    }
}

void
contest_log_append(struct EjContestState *ecs, const unsigned char *text)
{
    if (!text || !*text) return;

    pthread_mutex_lock(&ecs->log_mutex);
    struct EjContestLog *nl = contest_log_create(NULL);
    size_t tz = strlen(text);
    size_t nz = ecs->log->size + tz;
    unsigned char *nt = malloc(nz + 1);
    unsigned char *p = stpcpy(nt, ecs->log->text);
    stpcpy(p, text);
    free(nl->text);
    nl->size = nz;
    nl->text = nt;
    contest_log_set(ecs, nl);
}

struct EjContestSession *
contest_session_read_lock(struct EjContestState *ecs)
{
    atomic_fetch_add_explicit(&ecs->session_guard, 1, memory_order_acquire);
    struct EjContestSession *ecc = atomic_load_explicit(&ecs->session, memory_order_relaxed);
    atomic_fetch_add_explicit(&ecc->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ecs->session_guard, 1, memory_order_release);
    return ecc;
}

struct EjContestSession *
contests_session_read_lock(struct EjContestsState *ecss, int cnts_id)
{
    return contest_session_read_lock(contests_state_get(ecss, cnts_id));
}

void
contest_session_read_unlock(struct EjContestSession *ecc)
{
    if (ecc) {
        atomic_fetch_sub_explicit(&ecc->reader_count, 1, memory_order_relaxed);
    }
}

int
contest_session_try_write_lock(struct EjContestState *ecs)
{
    return atomic_exchange_explicit(&ecs->session_update, 1, memory_order_acquire);
}

void
contest_session_set(struct EjContestState *ecs, struct EjContestSession *ecc)
{
    struct EjContestSession *old = atomic_exchange_explicit(&ecs->session, ecc, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ecs->session_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ecs->session_update, 0, memory_order_release);

    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_acquire, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        contest_session_free(old);
    }
}

struct EjProblemStates *
problem_states_create(void)
{
    struct EjProblemStates *epss = calloc(1, sizeof(*epss));
    pthread_rwlock_init(&epss->rwl, NULL);
    return epss;
}

void
problem_states_free(struct EjProblemStates *epss)
{
    if (epss) {
        pthread_rwlock_destroy(&epss->rwl);
        free(epss);
    }
}

struct EjProblemState *
problem_states_get(struct EjProblemStates *epss, int prob_id)
{
    struct EjProblemState *retval = NULL;
    if (prob_id <= 0 || prob_id > 10000) return NULL;

    pthread_rwlock_rdlock(&epss->rwl);
    if (prob_id < epss->size) {
        retval = epss->entries[prob_id];
    }
    pthread_rwlock_unlock(&epss->rwl);
    if (retval) return retval;

    pthread_rwlock_wrlock(&epss->rwl);
    if (prob_id >= epss->size) {
        int nz = epss->size * 2;
        if (!nz) nz = 16;
        while (nz <= prob_id) nz *= 2;
        struct EjProblemState **ee = calloc(nz, sizeof(*ee));
        if (epss->size > 0) {
            memcpy(ee, epss->entries, epss->size * sizeof(ee[0]));
        }
        free(epss->entries);
        epss->size = nz;
        epss->entries = ee;
    }
    retval = epss->entries[prob_id];
    if (!retval) {
        retval = epss->entries[prob_id] = problem_state_create(prob_id);
    }
    pthread_rwlock_unlock(&epss->rwl);

    return retval;
}

struct EjProblemState *
problem_states_try(struct EjProblemStates *epss, int prob_id)
{
    struct EjProblemState *retval = NULL;
    pthread_rwlock_rdlock(&epss->rwl);
    if (prob_id > 0 && prob_id < epss->size) {
        retval = epss->entries[prob_id];
    }
    pthread_rwlock_unlock(&epss->rwl);

    return retval;
}

struct EjProblemState *
problem_state_create(int prob_id)
{
    struct EjProblemState *eps = calloc(1, sizeof(*eps));
    eps->prob_id = prob_id;
    eps->submits = problem_submits_create();
    return eps;
}

void
problem_state_free(struct EjProblemState *eps)
{
    if (eps) {
        free(eps);
    }
}

struct EjProblemInfo *
problem_info_create(int prob_id)
{
    struct EjProblemInfo *epi = calloc(1, sizeof(*epi));
    epi->acm_run_penalty = 20;
    epi->test_score = 1;
    epi->best_run = -1;
    epi->full_score = -1;
    epi->full_user_score = -1;
    epi->min_score_1 = -1;
    epi->min_score_2 = -1;
    epi->date_penalty = -1;
    return epi;
}

void
problem_info_free(struct EjProblemInfo *epi)
{
    if (epi) {
        free(epi->info_json_text);
        free(epi->info_text);
        free(epi->short_name);
        free(epi->long_name);
        free(epi->stand_name);
        free(epi->stand_column);
        free(epi->input_file);
        free(epi->output_file);
        free(epi->compilers);
        free(epi->log_s);
        free(epi->penalty_formula);
        free(epi);
    }
}

struct EjProblemInfo *
problem_info_read_lock(struct EjProblemState *eps)
{
    atomic_fetch_add_explicit(&eps->info_guard, 1, memory_order_acquire);
    struct EjProblemInfo *epi = atomic_load_explicit(&eps->info, memory_order_relaxed);
    if (epi) {
        atomic_fetch_add_explicit(&epi->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&eps->info_guard, 1, memory_order_release);
    return epi;
}

void
problem_info_read_unlock(struct EjProblemInfo *epi)
{
    if (epi) {
        atomic_fetch_sub_explicit(&epi->reader_count, 1, memory_order_relaxed);
    }
}

int
problem_info_try_write_lock(struct EjProblemState *eps)
{
    return atomic_exchange_explicit(&eps->info_update, 1, memory_order_acquire);
}

void
problem_info_set(struct EjProblemState *eps, struct EjProblemInfo *epi)
{
    struct EjProblemInfo *old = atomic_exchange_explicit(&eps->info, epi, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&eps->info_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&eps->info_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        problem_info_free(old);
    }
}

struct EjProblemStatement *
problem_statement_create(int prob_id)
{
    struct EjProblemStatement *eph = calloc(1, sizeof(*eph));
    eph->prob_id = prob_id;
    return eph;
}

void
problem_statement_free(struct EjProblemStatement *eph)
{
    if (eph) {
        free(eph->log_s);
        free(eph->stmt_text);
        free(eph);
    }
}

struct EjProblemStatement *
problem_statement_read_lock(struct EjProblemState *eps)
{
    atomic_fetch_add_explicit(&eps->stmt_guard, 1, memory_order_acquire);
    struct EjProblemStatement *eph = atomic_load_explicit(&eps->stmt, memory_order_relaxed);
    if (eph) {
        atomic_fetch_add_explicit(&eph->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&eps->stmt_guard, 1, memory_order_release);
    return eph;
}

void
problem_statement_read_unlock(struct EjProblemStatement *eph)
{
    if (eph) {
        atomic_fetch_sub_explicit(&eph->reader_count, 1, memory_order_relaxed);
    }
}

int
problem_statement_try_write_lock(struct EjProblemState *eps)
{
    return atomic_exchange_explicit(&eps->stmt_update, 1, memory_order_acquire);
}

void
problem_statement_set(struct EjProblemState *eps, struct EjProblemStatement *eph)
{
    struct EjProblemStatement *old = atomic_exchange_explicit(&eps->stmt, eph, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&eps->stmt_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&eps->stmt_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        problem_statement_free(old);
    }
}

struct EjRunState *
run_state_create(int run_id)
{
    struct EjRunState *ejr = calloc(1, sizeof(*ejr));
    ejr->run_id = run_id;
    ejr->tests = run_tests_create();
    return ejr;
}

void
run_state_free(struct EjRunState *ejr)
{
    if (ejr) {
        run_tests_free(ejr->tests);
        free(ejr);
    }
}

struct EjRunStates *
run_states_create(void)
{
    struct EjRunStates *ejrs = calloc(1, sizeof(*ejrs));
    pthread_rwlock_init(&ejrs->rwl, NULL);
    return ejrs;
}

void
run_states_free(struct EjRunStates *ejrs)
{
    if (ejrs) {
        pthread_rwlock_destroy(&ejrs->rwl);
        for (int i = 0; i < ejrs->size; ++i) {
            free(ejrs->runs[i]);
        }
        free(ejrs->runs);
        free(ejrs);
    }
}

struct EjRunState *
run_states_get(struct EjRunStates *erss, int run_id)
{
    struct EjRunState *ers = NULL;
    int low, high;

    pthread_rwlock_rdlock(&erss->rwl);
    if (erss->size > 0 && run_id >= erss->runs[0]->run_id && run_id <= erss->runs[erss->size - 1]->run_id) {
        low = 0; high = erss->size;
        while (low < high) {
            int mid = (low + high) / 2;
            ers = erss->runs[mid];
            if (ers->run_id == run_id) {
                pthread_rwlock_unlock(&erss->rwl);
                return ers;
            } else if (ers->run_id < run_id) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }
    pthread_rwlock_unlock(&erss->rwl);
    pthread_rwlock_wrlock(&erss->rwl);
    if (erss->size <= 0 || run_id < erss->runs[0]->run_id) {
        low = high = 0;
        ers = NULL;
    } else if (run_id > erss->runs[erss->size - 1]->run_id) {
        low = high = erss->size;
        ers = NULL;
    } else {
        low = 0; high = erss->size;
        ers = NULL;
        while (low < high) {
            int mid = (low + high) / 2;
            struct EjRunState *tmp = erss->runs[mid];
            if (tmp->run_id == run_id) {
                ers = tmp;
                break;
            } else if (tmp->run_id < run_id) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }
    if (ers) {
        pthread_rwlock_unlock(&erss->rwl);
        return ers;
    }
    if (erss->size >= erss->reserved) {
        if (!erss->reserved) {
            erss->reserved = 16;
        } else {
            erss->reserved *= 2;
        }
        erss->runs = realloc(erss->runs, erss->reserved * sizeof(erss->runs[0]));
    }
    if (low < erss->size) {
        memmove(&erss->runs[low + 1], &erss->runs[low], (erss->size - low) * sizeof(erss->runs[0]));
    }
    ++erss->size;
    ers = erss->runs[low] = run_state_create(run_id);
    pthread_rwlock_unlock(&erss->rwl);
    return ers;
}

struct EjProblemCompilerSubmits *
problem_compiler_submits_create(int lang_id)
{
    struct EjProblemCompilerSubmits *epcs = calloc(1, sizeof(*epcs));
    epcs->lang_id = lang_id;
    epcs->dir_nodes = dir_nodes_create();
    return epcs;
}

void
problem_compiler_submits_free(struct EjProblemCompilerSubmits *epcs)
{
    if (epcs) {
        dir_nodes_free(epcs->dir_nodes);
        free(epcs);
    }
}

struct EjProblemSubmits *
problem_submits_create(void)
{
    struct EjProblemSubmits *epss = calloc(1, sizeof(*epss));
    pthread_rwlock_init(&epss->rwl, NULL);
    return epss;
}

void
problem_submits_free(struct EjProblemSubmits *epss)
{
    if (epss) {
        pthread_rwlock_destroy(&epss->rwl);
        for (int i = 0; i < epss->size; ++i) {
            problem_compiler_submits_free(epss->submits[i]);
        }
        free(epss->submits);
        free(epss);
    }
}

struct EjProblemCompilerSubmits *
problem_submits_get(struct EjProblemSubmits *epss, int lang_id)
{
    int low, high;
    struct EjProblemCompilerSubmits *epcs = NULL;

    pthread_rwlock_rdlock(&epss->rwl);
    low = 0; high = epss->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjProblemCompilerSubmits *tmp = epss->submits[mid];
        if (tmp->lang_id == lang_id) {
            epcs = tmp;
            break;
        } else if (tmp->lang_id < lang_id) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    pthread_rwlock_unlock(&epss->rwl);
    if (epcs) return epcs;

    pthread_rwlock_wrlock(&epss->rwl);
    low = 0; high = epss->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjProblemCompilerSubmits *tmp = epss->submits[mid];
        if (tmp->lang_id == lang_id) {
            epcs = tmp;
            break;
        } else if (tmp->lang_id < lang_id) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (!epcs) {
        if (epss->size >= epss->reserved) {
            if (!epss->reserved) {
                epss->reserved = 16;
            } else {
                epss->reserved *= 2;
            }
            epss->submits = realloc(epss->submits, epss->reserved * sizeof(epss->submits[0]));
        }
        if (low < epss->size) {
            memmove(&epss->submits[low + 1], &epss->submits[low], (epss->size - low) * sizeof(epss->submits[0]));
        }
        ++epss->size;
        epcs = epss->submits[low] = problem_compiler_submits_create(lang_id);
    }
    pthread_rwlock_unlock(&epss->rwl);
    return epcs;
}

struct EjProblemRuns *
problem_runs_create(int prob_id)
{
    struct EjProblemRuns *eprs = calloc(1, sizeof(*eprs));
    eprs->prob_id = prob_id;
    return eprs;
}

void
problem_runs_free(struct EjProblemRuns *eprs)
{
    if (eprs) {
        free(eprs->log_s);
        free(eprs->runs);
        free(eprs->info_json_text);
        free(eprs);
    }
}

struct EjProblemRuns *
problem_runs_read_lock(struct EjProblemState *eps)
{
    atomic_fetch_add_explicit(&eps->runs_guard, 1, memory_order_acquire);
    struct EjProblemRuns *eprs = atomic_load_explicit(&eps->runs, memory_order_relaxed);
    if (eprs) {
        atomic_fetch_add_explicit(&eprs->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&eps->runs_guard, 1, memory_order_release);
    return eprs;
}

void
problem_runs_read_unlock(struct EjProblemRuns *eprs)
{
    if (eprs) {
        atomic_fetch_sub_explicit(&eprs->reader_count, 1, memory_order_relaxed);
    }
}

int
problem_runs_try_write_lock(struct EjProblemState *eps)
{
    return atomic_exchange_explicit(&eps->runs_update, 1, memory_order_acquire);
}

void
problem_runs_set(struct EjProblemState *eps, struct EjProblemRuns *eprs)
{
    struct EjProblemRuns *old = atomic_exchange_explicit(&eps->runs, eprs, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&eps->runs_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&eps->runs_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        problem_runs_free(old);
    }
}

struct EjProblemRun *
problem_runs_find_unlocked(struct EjProblemRuns *eprs, int run_id)
{
    if (!eprs || !eprs->ok || eprs->size <= 0) return NULL;

    int low = 0, high = eprs->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjProblemRun *cur = &eprs->runs[mid];
        if (cur->run_id == run_id) {
            return cur;
        } else if (cur->run_id < run_id) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return NULL;
}

struct EjRunInfo *
run_info_create(int run_id)
{
    struct EjRunInfo *eri = calloc(1, sizeof(*eri));
    eri->run_id = run_id;
    return eri;
}

void
run_info_free(struct EjRunInfo *eri)
{
    if (eri) {
        free(eri->log_s);
        free(eri->info_json_text);
        free(eri->info_text);
        free(eri->compiler_text);
        free(eri->src_sfx);
        free(eri->score_str);
        free(eri->valuer_text);
        free(eri->tests);
        free(eri);
    }
}

struct EjRunInfo *
run_info_read_lock(struct EjRunState *ers)
{
    atomic_fetch_add_explicit(&ers->info_guard, 1, memory_order_acquire);
    struct EjRunInfo *eri = atomic_load_explicit(&ers->info, memory_order_relaxed);
    if (eri) {
        atomic_fetch_add_explicit(&eri->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&ers->info_guard, 1, memory_order_release);
    return eri;
}

void
run_info_read_unlock(struct EjRunInfo *eri)
{
    if (eri) {
        atomic_fetch_sub_explicit(&eri->reader_count, 1, memory_order_relaxed);
    }
}

int
run_info_try_write_lock(struct EjRunState *ers)
{
    return atomic_exchange_explicit(&ers->info_update, 1, memory_order_acquire);
}

void
run_info_set(struct EjRunState *ers, struct EjRunInfo *eri)
{
    struct EjRunInfo *old = atomic_exchange_explicit(&ers->info, eri, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ers->info_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ers->info_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        run_info_free(old);
    }
}

struct EjRunInfoTestResult *
run_info_get_test_result_unlocked(struct EjRunInfo *eri, int num)
{
    if (!eri || !eri->ok) return NULL;
    for (int i = 0; i < eri->test_count; ++i) {
        if (eri->tests[i].num == num) {
            return &eri->tests[i];
        }
    }
    return NULL;
}

struct EjRunSource *
run_source_create(int run_id)
{
    struct EjRunSource *ert = calloc(1, sizeof(*ert));
    ert->run_id = run_id;
    return ert;
}

void
run_source_free(struct EjRunSource *ert)
{
    if (ert) {
        free(ert->log_s);
        free(ert->data);
        free(ert);
    }
}

struct EjRunSource *
run_source_read_lock(struct EjRunState *ers)
{
    atomic_fetch_add_explicit(&ers->src_guard, 1, memory_order_acquire);
    struct EjRunSource *ert = atomic_load_explicit(&ers->src, memory_order_relaxed);
    if (ert) {
        atomic_fetch_add_explicit(&ert->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&ers->src_guard, 1, memory_order_release);
    return ert;
}

void
run_source_read_unlock(struct EjRunSource *ert)
{
    if (ert) {
        atomic_fetch_sub_explicit(&ert->reader_count, 1, memory_order_relaxed);
    }
}

int
run_source_try_write_lock(struct EjRunState *ers)
{
    return atomic_exchange_explicit(&ers->src_update, 1, memory_order_acquire);
}

void
run_source_set(struct EjRunState *ers, struct EjRunSource *eri)
{
    struct EjRunSource *old = atomic_exchange_explicit(&ers->src, eri, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ers->src_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ers->src_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        run_source_free(old);
    }
}

struct EjRunMessages *
run_messages_create(int run_id)
{
    struct EjRunMessages *erms = calloc(1, sizeof(*erms));
    erms->run_id = run_id;
    return erms;
}

void
run_messages_free(struct EjRunMessages *erms)
{
    if (erms) {
        for (int i = 0; i < erms->count; ++i) {
            struct EjRunMessage *erm = &erms->messages[i];
            free(erm->subject);
            free(erm->data);
        }
        free(erms->messages);
        free(erms->json_text);
        free(erms->text);
        free(erms->log_s);
        free(erms);
    }
}

struct EjRunMessages *
run_messages_read_lock(struct EjRunState *ers)
{
    atomic_fetch_add_explicit(&ers->msg_guard, 1, memory_order_acquire);
    struct EjRunMessages *ert = atomic_load_explicit(&ers->msg, memory_order_relaxed);
    if (ert) {
        atomic_fetch_add_explicit(&ert->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&ers->msg_guard, 1, memory_order_release);
    return ert;
}

void
run_messages_read_unlock(struct EjRunMessages *erms)
{
    if (erms) {
        atomic_fetch_sub_explicit(&erms->reader_count, 1, memory_order_relaxed);
    }
}

int
run_messages_try_write_lock(struct EjRunState *ers)
{
    return atomic_exchange_explicit(&ers->msg_update, 1, memory_order_acquire);
}

void
run_messages_set(struct EjRunState *ers, struct EjRunMessages *erms)
{
    struct EjRunMessages *old = atomic_exchange_explicit(&ers->msg, erms, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ers->msg_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ers->msg_update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        run_messages_free(old);
    }
}

struct EjRunTests *
run_tests_create(void)
{
    struct EjRunTests *erts = calloc(1, sizeof(*erts));
    pthread_rwlock_init(&erts->rwl, NULL);
    return erts;
}

void
run_tests_free(struct EjRunTests *erts)
{
    if (erts) {
        pthread_rwlock_destroy(&erts->rwl);
        for (int i = 0; i < erts->size; ++i) {
            run_test_free(erts->tests[i]);
        }
        free(erts);
    }
}

struct EjRunTest *
run_tests_get(struct EjRunTests *erts, int num)
{
    struct EjRunTest *ert = NULL;
    pthread_rwlock_rdlock(&erts->rwl);
    if (num < 1 || num > 10000) {
        pthread_rwlock_unlock(&erts->rwl);
        return NULL;
    }
    if (num <= erts->size) {
        ert = erts->tests[num - 1];
    }
    pthread_rwlock_unlock(&erts->rwl);
    if (!ert) {
        pthread_rwlock_wrlock(&erts->rwl);
        if (num > erts->size) {
            size_t new_size = erts->size;
            if (!new_size) new_size = 16;
            while (num > new_size) new_size *= 2;
            struct EjRunTest **new_tests = calloc(new_size, sizeof(new_tests[0]));
            if (erts->size > 0) {
                memcpy(new_tests, erts->tests, erts->size * sizeof(erts->tests[0]));
            }
            free(erts->tests);
            erts->tests = new_tests;
            erts->size = new_size;
        }
        ert = erts->tests[num - 1];
        if (!ert) {
            ert = erts->tests[num - 1] = run_test_create(num);
        }
        pthread_rwlock_unlock(&erts->rwl);
    }
    return ert;
}

struct EjRunTest *
run_test_create(int num)
{
    struct EjRunTest *ert = calloc(1, sizeof(*ert));
    ert->num = num;
    return ert;
}

void
run_test_free(struct EjRunTest *ert)
{
    if (ert) {
        free(ert);
    }
}

struct EjRunTestData *
run_test_data_create(void)
{
    struct EjRunTestData *ertd = calloc(1, sizeof(*ertd));
    return ertd;
}

void
run_test_data_free(struct EjRunTestData *ertd)
{
    if (ertd) {
        free(ertd->data);
        free(ertd);
    }
}

struct EjRunTestData *
run_test_data_read_lock(struct EjRunTest *ert, int index)
{
    if (index < 0 || index >= TESTING_REPORT_LAST) return NULL;
    struct EjRunTestPart *ertp = &ert->parts[index];
    atomic_fetch_add_explicit(&ertp->guard, 1, memory_order_acquire);
    struct EjRunTestData *ertd = atomic_load_explicit(&ertp->info, memory_order_relaxed);
    if (ertd) {
        atomic_fetch_add_explicit(&ertd->reader_count, 1, memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&ertp->guard, 1, memory_order_release);
    return ertd;
}

void
run_test_data_read_unlock(struct EjRunTestData *ertd)
{
    if (ertd) {
        atomic_fetch_sub_explicit(&ertd->reader_count, 1, memory_order_relaxed);
    }
}

int
run_test_data_try_write_lock(struct EjRunTest *ert, int index)
{
    if (index < 0 || index >= TESTING_REPORT_LAST) return 1;
    struct EjRunTestPart *ertp = &ert->parts[index];
    return atomic_exchange_explicit(&ertp->update, 1, memory_order_acquire);
}

void
run_test_data_set(struct EjRunTest *ert, int index, struct EjRunTestData *ertd)
{
    if (index < 0 || index >= TESTING_REPORT_LAST) return;
    struct EjRunTestPart *ertp = &ert->parts[index];
    struct EjRunTestData *old = atomic_exchange_explicit(&ertp->info, ertd, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ertp->guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ertp->update, 0, memory_order_release);
    if (old) {
        expected = 0;
        while (!atomic_compare_exchange_weak_explicit(&old->reader_count, &expected, 0, memory_order_release, memory_order_acquire)) {
            sched_yield();
            expected = 0;
        }
        run_test_data_free(old);
    }
}

static const unsigned char * const testing_info_file_names[] =
{
    [TESTING_REPORT_INPUT] = "input",
    [TESTING_REPORT_OUTPUT] = "output",
    [TESTING_REPORT_CORRECT] = "correct",
    [TESTING_REPORT_ERROR] = "error",
    [TESTING_REPORT_CHECKER] = "checker",
    [TESTING_REPORT_ARGS] = "args",
};

const unsigned char *
testing_info_unparse(int index)
{
    if (index >= 0 && index < TESTING_REPORT_LAST) {
        return testing_info_file_names[index];
    }
    abort();
    return "";
}

int
testing_info_parse(const unsigned char *str)
{
    if (!str) return -1;
    for (int i = 0; i < sizeof(testing_info_file_names) / sizeof(testing_info_file_names[0]); ++i) {
        if (!strcmp(testing_info_file_names[i], str)) {
            return i;
        }
    }
    return 0;
}
