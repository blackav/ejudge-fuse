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

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <curl/curl.h>

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
  "user_id": 2,
  "user_login": "user01",
  "user_name": "user01",
  "session": "1cca7131d51a2b96-78453be8c6aa2ea1",
  "session_id": "1cca7131d51a2b96",
  "client_key": "78453be8c6aa2ea1",
  "expire": 1525379733
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
        cJSON *jsession_id = cJSON_GetObjectItem(root, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            tls->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(root, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            tls->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(root, "expire");
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

void
ej_get_contest_list(struct EjFuseState *ejs)
{
    struct EjTopSession *top_session = NULL; // reader RCU lock for top_session
    struct EjContestList *contests = NULL;
    CURL *curl = NULL;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
    cJSON *root = NULL;

    // do not want parallel updates
    if (contest_list_try_write_lock(ejs)) return;

    // take under spinlock, ejs->top_session is never NULL
    top_session = top_session_read_lock(ejs);

    err_f = open_memstream(&err_s, &err_z);
    contests = calloc(1, sizeof(*contests));
    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        char *s1, *s2;
        fprintf(url_f, "%sregister/user-contests-json?SID=%s&EJSID=%s&json=1",
                ejs->url,
                (s1 = curl_easy_escape(curl, top_session->session_id, 0)),
                (s2 = curl_easy_escape(curl, top_session->client_key, 0)));
        free(s1);
        free(s2);
        fclose(url_f);
    }

    // release as early as possible
    top_session_read_unlock(top_session); top_session = NULL;

    CURLcode res = 0;
    {
        size_t resp_z = 0;
        FILE *resp_f = open_memstream(&resp_s, &resp_z);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url_s);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_f);
        res = curl_easy_perform(curl);
        fclose(resp_f);
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

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
>{
  "ok": true,
  "contests": [
    {
      "id": 2,
      "name": "Test contest (Tokens)"
    },
    {
      "id": 3,
      "name": "Test contest (variants)"
    }
  ]
}
     */

    cJSON *jok = cJSON_GetObjectItem(root, "ok");
    if (!jok) {
        goto invalid_json;
    }
    if (jok->type == cJSON_True) {
        cJSON *jcontests = cJSON_GetObjectItem(root, "contests");
        if (!jcontests || jcontests->type != cJSON_Array) {
            goto invalid_json;
        }
        contests->count = cJSON_GetArraySize(jcontests);
        if (contests->count > 0) {
            contests->entries = calloc(contests->count, sizeof(contests->entries[0]));
            for (int j = 0; j < contests->count; ++j) {
                cJSON *jc = cJSON_GetArrayItem(jcontests, j);
                if (!jc || jc->type != cJSON_Object) {
                    goto invalid_json;
                }
                cJSON *jid = cJSON_GetObjectItem(jc, "id");
                if (!jid || jid->type != cJSON_Number) {
                    goto invalid_json;
                }
                contests->entries[j].id = jid->valueint;
                cJSON *jname = cJSON_GetObjectItem(jc, "name");
                if (!jname || jname->type != cJSON_String) {
                    goto invalid_json;
                }
                contests->entries[j].name = strdup(jname->valuestring);
            }
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    for (int i = 0; i < contests->count; ++i) {
        printf("%d: %s\n", contests->entries[i].id, contests->entries[i].name);
    }

    // normal return
    contests->log_s = NULL;
    contests->recheck_time_us = 0;
    contests->ok = 1;

cleanup:
    if (root) {
        cJSON_Delete(root);
    }
    free(resp_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    free(url_s);
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    free(err_s);

    // release RCU reader lock
    top_session_read_unlock(top_session);

    // setup contests pointer
    contest_list_set(ejs, contests);
    return;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
    
failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    contests->log_s = err_s; err_s = NULL;
    contests->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    goto cleanup;
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

static void
ejudge_client_enter_contest_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjContestSession *ecc,
        const unsigned char *session_id,
        const unsigned char *client_key)
{
    CURL *curl = NULL;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *post_s = NULL;
    char *resp_s = NULL;
    cJSON *root = NULL;
    CURLcode res = 0;

    err_f = open_memstream(&err_s, &err_z);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        fprintf(url_f, "%sregister/enter-contest-json", ejs->url);
        fclose(url_f); url_f = NULL;
    }

    {
        size_t post_z = 0;
        FILE *post_f = open_memstream(&post_s, &post_z);
        char *s = curl_easy_escape(curl, session_id, 0);
        fprintf(post_f, "SID=%s", s);
        free(s);
        s = curl_easy_escape(curl, client_key, 0);
        fprintf(post_f, "&EJSID=%s", s);
        free(s); s = NULL;
        fprintf(post_f, "&contest_id=%d", ecs->cnts_id);
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
  "contest_id": 2,
  "session": "2044993ba5cef38e-c0048f20bb50e070",
  "SID": "2044993ba5cef38e",
  "EJSID": "c0048f20bb50e070",
  "expire": 1525894365,
  "user_id": 2,
  "user_login": "user01",
  "user_name": "user01",
  "cntsreg": {
    "status": "ok"
  }
}
     */

    cJSON *jok = cJSON_GetObjectItem(root, "ok");
    if (!jok) {
        goto invalid_json;
    }
    if (jok->type == cJSON_True) {
        cJSON *jsession_id = cJSON_GetObjectItem(root, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            ecc->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(root, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            ecc->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(root, "expire");
        if (jexpire && jexpire->type == cJSON_Number) {
            ecc->expire_us = (time_t) jexpire->valuedouble * 1000000LL;
        } else {
            goto invalid_json;
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    // normal return
    contest_log_format(ejs, ecs, "enter-contest-json", 1, "%s %s %lld", ecc->session_id, ecc->client_key, ecc->expire_us);
    ecc->log_s = NULL;
    ecc->recheck_time_us = 0;
    ecc->ok = 1;

cleanup:
    if (root) {
        cJSON_Delete(root);
    }
    free(resp_s);
    free(url_s);
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    free(post_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    return;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    ecc->log_s = err_s; err_s = NULL;
    ecc->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    contest_log_format(ejs, ecs, "enter-contest-json", 1, NULL);
    goto cleanup;
}

void
ejudge_client_enter_contest(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    int already = contest_session_try_write_lock(ecs);
    if (already) return;

    struct EjTopSession *top_session = top_session_read_lock(ejs);
    if (!top_session || !top_session->ok) {
        top_session_read_unlock(top_session);
        return;
    }

    unsigned char session_id[128];
    unsigned char client_key[128];
    snprintf(session_id, sizeof(session_id), "%s", top_session->session_id);
    snprintf(client_key, sizeof(client_key), "%s", top_session->client_key);
    top_session_read_unlock(top_session);

    struct EjContestSession *ecc = calloc(1, sizeof(*ecc));
    ecc->cnts_id = ecs->cnts_id;
    ejudge_client_enter_contest_request(ejs, ecs, ecc, session_id, client_key);
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

static void
ejudge_client_contest_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjContestInfo *eci,
        const unsigned char *session_id,
        const unsigned char *client_key)
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
    CURLcode res = 0;
    cJSON *root = NULL;

    err_f = open_memstream(&err_s, &err_z);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        char *s1, *s2;
        fprintf(url_f, "%sclient/contest-status-json?SID=%s&EJSID=%s&json=1",
                ejs->url,
                (s1 = curl_easy_escape(curl, session_id, 0)),
                (s2 = curl_easy_escape(curl, client_key, 0)));
        free(s1);
        free(s2);
        fclose(url_f);
    }

    {
        size_t resp_z = 0;
        FILE *resp_f = open_memstream(&resp_s, &resp_z);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url_s);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_f);
        res = curl_easy_perform(curl);
        fclose(resp_f);
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);
    root = cJSON_Parse(resp_s);
    if (!root) {
        fprintf(err_f, "json parse failed\n");
        goto failed;
    }
    if (root->type != cJSON_Object) {
        goto invalid_json;
    }
    cJSON *jok = cJSON_GetObjectItem(root, "ok");
    if (!jok) {
        goto invalid_json;
    }

    if (jok->type == cJSON_True) {
        eci->info_json = strdup(resp_s);
    /*

        cJSON *jsession_id = cJSON_GetObjectItem(root, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            ecc->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(root, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            ecc->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(root, "expire");
        if (jexpire && jexpire->type == cJSON_Number) {
            ecc->expire_us = (time_t) jexpire->valuedouble * 1000000LL;
        } else {
            goto invalid_json;
        }
    */
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    // normal return
    //contest_log_format(ejs, ecs, "contest-status-json", 1, NULL);
    eci->log_s = NULL;
    eci->recheck_time_us = ejs->current_time_us + 10000000; // +10s
    eci->ok = 1;

cleanup:
    if (root) {
        cJSON_Delete(root);
    }
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) fclose(err_f);
    free(err_s);
    return;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    eci->log_s = err_s; err_s = NULL;
    eci->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    contest_log_format(ejs, ecs, "contest-status-json", 0, NULL);
    goto cleanup;
}


void
ejudge_client_contest_info(struct EjFuseState *ejs, struct EjContestState *ecs)
{
    int already = contest_info_try_write_lock(ecs);
    if (already) return;

    struct EjContestSession *ecc = contest_session_read_lock(ecs);
    if (!ecc || !ecc->ok) {
        contest_session_read_unlock(ecc);
        return;
    }

    unsigned char session_id[128];
    unsigned char client_key[128];
    snprintf(session_id, sizeof(session_id), "%s", ecc->session_id);
    snprintf(client_key, sizeof(client_key), "%s", ecc->client_key);
    contest_session_read_unlock(ecc);

    struct EjContestInfo *eci = contest_info_create(ecs->cnts_id);
    ejudge_client_contest_info_request(ejs, ecs, eci, session_id, client_key);
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

int
ejf_process_path(const char *path, struct EjFuseRequest *rq)
{
    memset(rq, 0, sizeof(*rq));
    rq->fx = fuse_get_context();
    rq->ejs = (struct EjFuseState *) rq->fx->private_data;
    update_current_time(rq->ejs);
    // safety
    if (!path || path[0] != '/') {
        return -ENOENT;
    }
    // then process the path
    if (!strcmp(path, "/")) {
        rq->ops = &ejfuse_root_operations;
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
        rq->contest_id = cnts_id;
        rq->ops = &ejfuse_contest_operations;
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
        rq->contest_id = cnts_id;
    }
    if (!contests_is_valid(rq->ejs, rq->contest_id)) {
        return -ENOENT;
    }
    if (!(rq->ecs = contests_state_get(rq->ejs->contests_state, rq->contest_id))) {
        return -ENOENT;
    }
    const char *p2 = strchr(p1 + 1, '/');
    if (!p2) {
        if (!strcmp(p1 + 1, "INFO")) {
            contest_session_maybe_update(rq->ejs, rq->ecs);
            contest_info_maybe_update(rq->ejs, rq->ecs);
            rq->ops = &ejfuse_contest_info_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "LOG")) {
            rq->ops = &ejfuse_contest_log_operations;
            return 0;
        }
        if (!strcmp(p1 + 1, "problems")) {
            contest_session_maybe_update(rq->ejs, rq->ecs);
            contest_info_maybe_update(rq->ejs, rq->ecs);
            rq->ops = &ejfuse_contest_problems_operations;
            return 0;
        }
        return -ENOENT;
    }

    return -ENOENT;
}

int main(int argc, char *argv[])
{
    CURLcode curle = curl_global_init(CURL_GLOBAL_ALL);
    if (curle != CURLE_OK) {
        fprintf(stderr, "failed to initialize CURL\n");
        return 1;
    }
    
    struct EjFuseState *ejs = calloc(1, sizeof(*ejs));
    ejs->url = strdup("http://localhost/ej/");
    ejs->login = strdup("user01");
    ejs->password = strdup("aaa");
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
