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

#include "cJSON.h"
#include "inode_hash.h"
#include "contests_state.h"
#include "ejfuse.h"
#include "ops_generic.h"
#include "ops_root.h"
#include "ops_contest.h"
#include "ops_contest_info.h"
#include "ops_contest_log.h"
#include "ops_fuse.h"
#include "ops_cnts_probs.h"
#include "ops_cnts_prob_dir.h"
#include "ops_cnts_prob_files.h"
#include "ops_cnts_prob_submit.h"
#include "ops_cnts_prob_submit_lang.h"
#include "ops_cnts_prob_submit_lang_dir.h"
#include "ejudge_client.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <curl/curl.h>

enum { MAX_PROB_SHORT_NAME_SIZE = 32 };

int
ejf_generic_fgetattr(struct EjFuseRequest *efr, const char *path, struct stat *stb, struct fuse_file_info *ffi)
{
    return efr->ops->getattr(efr, path, stb);
}

void
update_current_time(struct EjFuseState *ejs)
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
    ejs->current_time_us = v1;
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
top_session_read_lock(struct EjFuseState *ejs)
{
    struct EjTopSession *top_session = NULL;

    atomic_fetch_add_explicit(&ejs->top_session_guard, 1, memory_order_acquire);
    top_session = atomic_load_explicit(&ejs->top_session, memory_order_relaxed);
    atomic_fetch_add_explicit(&top_session->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ejs->top_session_guard, 1, memory_order_release);
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
top_session_try_write_lock(struct EjFuseState *ejs)
{
    return atomic_exchange_explicit(&ejs->top_session_update, 1, memory_order_relaxed);
}

void
top_session_set(struct EjFuseState *ejs, struct EjTopSession *top_session)
{
    struct EjTopSession *old_session = atomic_exchange_explicit(&ejs->top_session, top_session, memory_order_acquire);
    int expected = 0;
    // spinlock
    while (!atomic_compare_exchange_weak_explicit(&ejs->top_session_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ejs->top_session_update, 0, memory_order_release);

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
contest_list_read_lock(struct EjFuseState *ejs)
{
    struct EjContestList *contests = NULL;

    atomic_fetch_add_explicit(&ejs->contests_guard, 1, memory_order_acquire);
    contests = atomic_load_explicit(&ejs->contests, memory_order_relaxed);
    atomic_fetch_add_explicit(&contests->reader_count, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ejs->contests_guard, 1, memory_order_release);
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
contest_list_try_write_lock(struct EjFuseState *ejs)
{
    return atomic_exchange_explicit(&ejs->contests_update, 1, memory_order_relaxed);
}

void
contest_list_set(struct EjFuseState *ejs, struct EjContestList *contests)
{
    struct EjContestList *old_contests = atomic_exchange_explicit(&ejs->contests, contests, memory_order_acquire);
    int expected = 0;
    while (!atomic_compare_exchange_weak_explicit(&ejs->contests_guard, &expected, 0, memory_order_release, memory_order_acquire)) {
        expected = 0;
    }
    atomic_store_explicit(&ejs->contests_update, 0, memory_order_release);

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
contests_is_valid(struct EjFuseState *ejs, int cnts_id)
{
    struct EjContestList *contests = contest_list_read_lock(ejs);
    struct EjContestListItem *cnts = contest_list_find(contests, cnts_id);
    contest_list_read_unlock(contests);
    return cnts != NULL;
}

void
ej_get_top_level_session(struct EjFuseState *ejs)
{
    struct EjTopSession *tls = NULL;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *post_s = NULL;
    char *resp_s = NULL;
    CURLcode res;
    cJSON *root = NULL;
    CURL *curl = NULL;

    // don't want parallel updates
    if (top_session_try_write_lock(ejs)) return;

    err_f = open_memstream(&err_s, &err_z);
    tls = calloc(1, sizeof(*tls));
    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        fprintf(url_f, "%sregister/login-json", ejs->url);
        fclose(url_f); url_f = NULL;
    }

    {
        size_t post_z = 0;
        FILE *post_f = open_memstream(&post_s, &post_z);
        char *s = curl_easy_escape(curl, ejs->login, 0);
        fprintf(post_f, "login=%s", s);
        free(s);
        s = curl_easy_escape(curl, ejs->password, 0);
        fprintf(post_f, "&password=%s", s);
        free(s); s = NULL;
        fprintf(post_f, "&json=1");
        fclose(post_f); post_f = NULL;
    }

    {
        size_t resp_z = 0;
        FILE *resp_f = open_memstream(&resp_s, &resp_z);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url_s);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_f);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_s);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        res = curl_easy_perform(curl);
        fclose(resp_f);
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    free(post_s); post_s = NULL;
    free(url_s); url_s = NULL;
    curl_easy_cleanup(curl); curl = NULL;

    fprintf(stdout, ">%s<\n", resp_s);
    root = cJSON_Parse(resp_s);
    if (!root) {
        fprintf(err_f, "json parse failed\n");
        goto failed;
    }
    if (root->type != cJSON_Object) {
        goto invalid_json;
    }

    /*
{
  "ok": true,
  "result": {
    "user_id": 2,
    "user_login": "user01",
    "user_name": "user01",
    "session": "f2615a00b0baff33-d566e2c4cf762312",
    "SID": "f2615a00b0baff33",
    "EJSID": "d566e2c4cf762312",
    "expire": 1526029211
  }
}
{
  "ok": false,
  "error": {
    "num": 29,
    "symbol": "ERR_PERMISSION_DENIED",
    "message": "Permission denied",
    "log_id": "90c4a172"
  }
}
     */
    
    cJSON *jok = cJSON_GetObjectItem(root, "ok");
    if (!jok) {
        goto invalid_json;
    }
    if (jok->type == cJSON_True) {
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jsession_id = cJSON_GetObjectItem(jresult, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            tls->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(jresult, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            tls->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(jresult, "expire");
        if (jexpire && jexpire->type == cJSON_Number) {
            tls->expire_us = (time_t) jexpire->valuedouble * 1000000LL;
        } else {
            goto invalid_json;
        }
        printf("session_id: %s\n", tls->session_id);
        printf("client_key: %s\n", tls->client_key);
        printf("expire: %lld\n", (long long) tls->expire_us);
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    if (root) {
        cJSON_Delete(root);
        root = NULL;
    }
    free(resp_s); resp_s = NULL;

    // normal return
    tls->log_s = NULL;
    tls->recheck_time_us = 0;
    tls->ok = 1;

 cleanup:
    if (root) {
        cJSON_Delete(root);
    }
    free(resp_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    free(url_s);
    free(post_s);
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    free(err_s);
    return top_session_set(ejs, tls);

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
    
failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    tls->log_s = err_s; err_s = NULL;
    tls->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    goto cleanup;
}

void
top_session_maybe_update(struct EjFuseState *ejs)
{
    int update_needed = 0;
    struct EjTopSession *top_session = top_session_read_lock(ejs);
    if (top_session->ok) {
        if (top_session->expire_us > 0 && ejs->current_time_us >= top_session->expire_us - 100000000) { // 100s
            update_needed = 1;
        }
        if (top_session->recheck_time_us > 0 && ejs->current_time_us >= top_session->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (top_session->recheck_time_us > 0 && ejs->current_time_us < top_session->recheck_time_us) {
            update_needed = 0;
        }
    }
    top_session_read_unlock(top_session);
    if (update_needed) {
        ej_get_top_level_session(ejs);
    }
}

int
top_session_copy_session(struct EjFuseState *ejs, struct EjSessionValue *esv)
{
    esv->ok = 0;
    struct EjTopSession *ets = top_session_read_lock(ejs);
    if (!ets || !ets->ok) {
        top_session_read_unlock(ets);
        return 0;
    }
    esv->ok = 1;
    snprintf(esv->session_id, sizeof(esv->session_id), "%s", ets->session_id);
    snprintf(esv->client_key, sizeof(esv->client_key), "%s", ets->client_key);
    top_session_read_unlock(ets);
    return 1;
}


void
ej_get_contest_list(struct EjFuseState *ejs)
{
    struct EjSessionValue esv = {};
    struct EjContestList *contests = NULL;

    if (!top_session_copy_session(ejs, &esv)) return;
    if (contest_list_try_write_lock(ejs)) return;

    contests = calloc(1, sizeof(*contests));

    ejudge_client_get_contest_list_request(ejs, &esv, contests);

    contest_list_set(ejs, contests);
    return;

}

void
contest_log_format(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const char *action,
        int success,
        const char *format, ...)
    __attribute__((format(printf, 5, 6)));
void
contest_log_format(
        struct EjFuseState *ejs,
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
        vsnprintf(buf1, sizeof(buf1), format, args);
        va_end(args);
    }

    time_t tt = ejs->current_time_us / 1000000;
    struct tm ltm;
    localtime_r(&tt, &ltm);
    snprintf(buf2, sizeof(buf2), "%04d-%02d-%02d %02d:%02d:%02d %s %s %s\n",
             ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec,
             action, success?"ok":"fail", buf1);
    contest_log_append(ecs, buf2);
}

void
ejudge_client_enter_contest(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    struct EjSessionValue esv = {};

    int already = contest_session_try_write_lock(ecs);
    if (already) return;

    if (!top_session_copy_session(ejs, &esv)) return;

    struct EjContestSession *ecc = calloc(1, sizeof(*ecc));
    ecc->cnts_id = ecs->cnts_id;
    ejudge_client_enter_contest_request(ejs, ecs, &esv, ecc);
    contest_session_set(ecs, ecc);
}

void
contest_session_maybe_update(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    int update_needed = 0;
    struct EjContestSession *ecc = contest_session_read_lock(ecs);
    if (ecc->ok) {
        if (ecc->expire_us > 0 && ejs->current_time_us >= ecc->expire_us - 10000000) {
            update_needed = 1;
        }
        if (ecc->recheck_time_us > 0 && ejs->current_time_us >= ecc->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (ecc->recheck_time_us > 0 && ejs->current_time_us < ecc->recheck_time_us) {
            update_needed = 0;
        }
    }
    contest_session_read_unlock(ecc);
    if (!update_needed) return;

    top_session_maybe_update(ejs);
    ejudge_client_enter_contest(ejs, ecs);
}

void
ejudge_client_contest_info(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    int already = contest_info_try_write_lock(ecs);
    if (already) return;

    struct EjSessionValue esv;
    if (!contest_state_copy_session(ecs, &esv)) return;

    struct EjContestInfo *eci = contest_info_create(ecs->cnts_id);
    ejudge_client_contest_info_request(ejs, ecs, &esv, eci);
    contest_info_set(ecs, eci);
}

void
contest_info_maybe_update(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    // contest session must be updated before
    int update_needed = 0;
    struct EjContestInfo *eci = contest_info_read_lock(ecs);
    if (eci->ok) {
        if (eci->recheck_time_us > 0 && ejs->current_time_us >= eci->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eci->recheck_time_us > 0 && ejs->current_time_us < eci->recheck_time_us){
            update_needed = 0;
        }
    }
    contest_info_read_unlock(eci);
    if (!update_needed) return;

    ejudge_client_contest_info(ejs, ecs);
}

void
problem_info_maybe_update(struct EjFuseState *ejs, struct EjContestState *ecs, struct EjProblemState *eps)
{
    int update_needed = 0;
    struct EjProblemInfo *epi = problem_info_read_lock(eps);
    if (epi && epi->ok) {
        if (epi->recheck_time_us > 0 && ejs->current_time_us >= epi->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (epi && epi->recheck_time_us > 0 && ejs->current_time_us < epi->recheck_time_us) {
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
    ejudge_client_problem_info_request(ejs, ecs, &esv, eps->prob_id, epi);
    problem_info_set(eps, epi);
}

void
problem_statement_maybe_update(struct EjFuseState *ejs, struct EjContestState *ecs, struct EjProblemState *eps)
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
        if (eph->recheck_time_us > 0 && ejs->current_time_us >= eph->recheck_time_us) {
            update_needed = 1;
        }
    } else {
        update_needed = 1;
        if (eph && eph->recheck_time_us > 0 && ejs->current_time_us < eph->recheck_time_us) {
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
    ejudge_client_problem_statement_request(ejs, ecs, &esv, eps->prob_id, eph);
    problem_statement_set(eps, eph);
}

unsigned
get_inode(struct EjFuseState *ejs, const char *path)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    size_t len = strlen(path);
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, path, len);
    SHA256_Final(digest, &ctx);

    return inode_hash_insert(ejs->inode_hash, digest)->inode;
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
    if (*eptr || errno || (unsigned char *) eptr == name_or_id || (int) val != val || val <= 0 || val >= eci->compiler_size || !eci->compilers[val]) {
        contest_info_read_unlock(eci);
        return -ENOENT;
    }
    efr->lang_id = val;
    contest_info_read_unlock(eci);
    return val;
}

/*
 * /<CNTS>/problems/<PROB>/submit/<LANG>...
 *                               ^ path
 */
static int
ejf_process_path_submit(const char *path, struct EjFuseRequest *efr)
{
    unsigned char lang_buf[NAME_MAX + 1];
    if (path[0] != '/') return -ENOENT;
    const char *p1 = strchr(path + 1, '/');
    if (!p1) {
        const char *comma = strchr(path + 1, ',');
        int len = 0;
        if (!comma) {
            len = strlen(path + 1);
        } else {
            len = comma - path - 1;
        }
        if (len > NAME_MAX) {
            return -ENOENT;
        }
        memcpy(lang_buf, path + 1, len);
        lang_buf[len] = 0;
        if (find_compiler(efr, lang_buf) < 0) {
            return -ENOENT;
        }
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
    efr->ejs = (struct EjFuseState *) efr->fx->private_data;
    update_current_time(efr->ejs);
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
    if (!contests_is_valid(efr->ejs, efr->contest_id)) {
        return -ENOENT;
    }
    if (!(efr->ecs = contests_state_get(efr->ejs->contests_state, efr->contest_id))) {
        return -ENOENT;
    }
    const char *p2 = strchr(p1 + 1, '/');
    if (!p2) {
        if (!strcmp(p1 + 1, "INFO") || !strcmp(p1 + 1, "info.json")) {
            efr->file_name = p1 + 1;
            contest_session_maybe_update(efr->ejs, efr->ecs);
            contest_info_maybe_update(efr->ejs, efr->ecs);
            efr->ops = &ejfuse_contest_info_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "LOG")) {
            efr->ops = &ejfuse_contest_log_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "problems")) {
            contest_session_maybe_update(efr->ejs, efr->ecs);
            contest_info_maybe_update(efr->ejs, efr->ecs);
            efr->ops = &ejfuse_contest_problems_operations;
            return 0;
        }
        return -ENOENT;
    }
    contest_session_maybe_update(efr->ejs, efr->ecs);
    contest_info_maybe_update(efr->ejs, efr->ecs);
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
        problem_info_maybe_update(efr->ejs, efr->ecs, efr->eps);
    }
    const char *p4 = strchr(p3 + 1, '/');
    if (!p4) {
        if (!strcmp(p3 + 1, FN_CONTEST_PROBLEM_INFO)
            || !strcmp(p3 + 1, FN_CONTEST_PROBLEM_INFO_JSON)
            || !strcmp(p3 + 1, FN_CONTEST_PROBLEM_STATEMENT_HTML)) {
            efr->file_name = p3 + 1;
            efr->ops = &ejfuse_contest_problem_files_operations;
            return 0;
        } else if (!strcmp(p3 + 1, "runs")) {
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
    }

    return -ENOENT;
}

int main(int argc, char *argv[])
{
    const unsigned char *ej_user = NULL;
    const unsigned char *ej_password = NULL;
    const unsigned char *ej_url = NULL;

    int work = 0;
    do {
        work = 0;
        if (argc >= 3 && !strcmp(argv[1], "--user")) {
            if (ej_user) {
                fprintf(stderr, "--user specified more than once\n");
                return 1;
            }
            ej_user = argv[2];
            memmove(&argv[1], &argv[3], (argc - 2) * sizeof(argv[0]));
            argc -= 2;
            work = 1;
        } else if (argc >= 3 && !strcmp(argv[1], "--password")) {
            if (ej_password) {
                fprintf(stderr, "--password specified more than once\n");
                return 1;
            }
            ej_password = argv[2];
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
    if (!ej_user) {
        fprintf(stderr, "--user not specified\n");
        return 1;
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
    
    struct EjFuseState *ejs = calloc(1, sizeof(*ejs));
    ejs->url = strdup(ej_url);
    ejs->login = strdup(ej_user);
    ejs->password = strdup(ej_password);
    ejs->owner_uid = getuid();
    ejs->owner_gid = getgid();
    ejs->inode_hash = inode_hash_create();
    ejs->contests_state = contests_state_create();

    update_current_time(ejs);
    ejs->start_time_us = ejs->current_time_us;

    ej_get_top_level_session(ejs);
    if (!ejs->top_session->ok) {
        fprintf(stderr, "initial login failed: %s\n", ejs->top_session->log_s);
        return 1;
    }
    ej_get_contest_list(ejs);
    if (!ejs->contests->ok) {
        fprintf(stderr, "initial contest list failed: %s\n", ejs->top_session->log_s);
        return 1;
    }

    int retval = fuse_main(argc, argv, &ejf_fuse_operations, ejs);
    free(ejs);
    return retval;
}
