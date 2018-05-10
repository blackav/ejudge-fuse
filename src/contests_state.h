#pragma once

#include <pthread.h>

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

struct EjContestInfo
{
    _Atomic int reader_count;

    int cnts_id;
    _Bool ok;
    long long      recheck_time_us;
    unsigned char *log_s;

    unsigned char *info_json_text;
    size_t info_json_size;
};

struct EjContestLog
{
    _Atomic int reader_count;

    size_t size;
    unsigned char *text;
};

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

struct EjContestInfo *contest_info_create(int cnts_id);
void contest_info_free(struct EjContestInfo *eci);
struct EjContestInfo *contest_info_read_lock(struct EjContestState *ecs);
void contest_info_read_unlock(struct EjContestInfo *eci);
int contest_info_try_write_lock(struct EjContestState *ecs);
void contest_info_set(struct EjContestState *ecs, struct EjContestInfo *ecd);
