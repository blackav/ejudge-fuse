#include "contests_state.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

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
    return eps;
}

void
problem_state_free(struct EjProblemState *eps)
{
    if (eps) {
        free(eps);
    }
}
