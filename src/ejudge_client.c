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

#include "ejudge_client.h"

#include "settings.h"
#include "contests_state.h"
#include "ejfuse.h"

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

void
contest_log_format(
        long long current_time_us,
        struct EjContestState *ecs,
        const char *action,
        int success,
        const char *format, ...)
    __attribute__((format(printf, 5, 6)));

void
ejudge_client_get_top_session_request(
        struct EjFuseState *efs,
        long long current_time_us,
        struct EjTopSession *tls) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *post_s = NULL;
    char *resp_s = NULL;
    CURLcode res;
    CURL *curl = NULL;

    err_f = open_memstream(&err_s, &err_z);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        fprintf(url_f, "%sregister/login-json", efs->url);
        fclose(url_f); url_f = NULL;
    }

    {
        size_t post_z = 0;
        FILE *post_f = open_memstream(&post_s, &post_z);
        char *s = curl_easy_escape(curl, efs->login, 0);
        fprintf(post_f, "login=%s", s);
        free(s);
        s = curl_easy_escape(curl, efs->password, 0);
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    free(post_s); post_s = NULL;
    free(url_s); url_s = NULL;
    curl_easy_cleanup(curl); curl = NULL;

    fprintf(stdout, ">%s<\n", resp_s);

    if (ejudge_json_parse_top_session(err_f, resp_s, tls) < 0) {
        goto failed;
    }

    // normal return
    tls->log_s = NULL;
    tls->recheck_time_us = 0;
    tls->ok = 1;

 cleanup:
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
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    tls->log_s = err_s; err_s = NULL;
    tls->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    goto cleanup;
}

void
ejudge_client_get_contest_list_request(
        struct EjFuseState *efs,
        const struct EjSessionValue *esv,
        long long current_time_us,
        struct EjContestList *contests)
{
    CURL *curl = NULL;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;

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
        fprintf(url_f, "%sregister/user-contests-json?SID=%s&EJSID=%s&json=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)));
        free(s1);
        free(s2);
        fclose(url_f);
    }

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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);

    if (ejudge_json_parse_contest_list(err_f, resp_s, contests) < 0) {
        goto failed;
    }

    for (int i = 0; i < contests->count; ++i) {
        printf("%d: %s\n", contests->entries[i].id, contests->entries[i].name);
    }

    // normal return
    contests->log_s = NULL;
    contests->update_time_us = current_time_us;
    contests->recheck_time_us = 0;
    contests->ok = 1;

cleanup:
    free(resp_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    free(url_s);
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    contests->log_s = err_s; err_s = NULL;
    contests->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    goto cleanup;
}

void
ejudge_client_enter_contest_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        long long current_time_us,
        struct EjContestSession *ecc) // output
{
    CURL *curl = NULL;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    char *url_s = NULL;
    char *post_s = NULL;
    char *resp_s = NULL;
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
        fprintf(url_f, "%sregister/enter-contest-json", efs->url);
        fclose(url_f); url_f = NULL;
    }

    {
        size_t post_z = 0;
        FILE *post_f = open_memstream(&post_s, &post_z);
        char *s = curl_easy_escape(curl, esv->session_id, 0);
        fprintf(post_f, "SID=%s", s);
        free(s);
        s = curl_easy_escape(curl, esv->client_key, 0);
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);

    if (ejudge_json_parse_contest_session(err_f, resp_s, ecc) < 0) {
        goto failed;
    }

    // normal return
    contest_log_format(current_time_us, ecs, "enter-contest-json", 1, "%s %s %lld", ecc->session_id, ecc->client_key, ecc->expire_us);
    ecc->log_s = NULL;
    ecc->recheck_time_us = 0;
    ecc->ok = 1;

cleanup:
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

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    ecc->log_s = err_s; err_s = NULL;
    ecc->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "enter-contest-json", 0, NULL);
    goto cleanup;
}

void
ejudge_client_contest_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        long long current_time_us,
        struct EjContestInfo *eci) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
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
        char *s1, *s2;
        fprintf(url_f, "%sclient/contest-status-json?SID=%s&EJSID=%s&json=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)));
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);
    eci->info_json_text = strdup(resp_s);
    eci->info_json_size = strlen(resp_s);

    if (ejudge_json_parse_contest_info(err_f, resp_s, eci) < 0) {
        goto failed;
    }

    // normal return
    //contest_log_format(efs, ecs, "contest-status-json", 1, NULL);
    eci->log_s = NULL;
    eci->update_time_us = current_time_us;
    eci->recheck_time_us = current_time_us + EJFUSE_CACHING_TIME;
    eci->ok = 1;

cleanup:
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) fclose(err_f);
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    eci->log_s = err_s; err_s = NULL;
    eci->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "contest-status-json", 0, NULL);
    goto cleanup;
}

void
ejudge_client_problem_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemInfo *epi) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
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
        char *s1, *s2;
        fprintf(url_f, "%sclient/problem-status-json?SID=%s&EJSID=%s&problem=%d&json=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)),
                prob_id);
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);
    epi->info_json_text = strdup(resp_s);
    epi->info_json_size = strlen(resp_s);

    if (ejudge_json_parse_problem_info(err_f, resp_s, epi) < 0) {
        goto failed;
    }

    // normal return
    //contest_log_format(efs, ecs, "problem-status-json", 1, NULL);
    epi->log_s = NULL;
    epi->update_time_us = current_time_us;
    epi->recheck_time_us = current_time_us + EJFUSE_CACHING_TIME;
    epi->ok = 1;

cleanup:
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    epi->log_s = err_s; err_s = NULL;
    epi->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "problem-status-json", 0, NULL);
    goto cleanup;
}

void
ejudge_client_problem_statement_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemStatement *eph) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
    CURLcode res = 0;
    char *stmt_s = NULL;

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
        fprintf(url_f, "%sclient/problem-statement-json?SID=%s&EJSID=%s&problem=%d&json=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)),
                prob_id);
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

    {
        size_t stmt_z = 0;
        FILE *stmt_f = open_memstream(&stmt_s, &stmt_z);
        fprintf(stmt_f,
                "<html>\n"
                "<head>\n"
                "<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\"/>\n"
                "<title>Problem statement</title>\n"
                "</head>\n"
                "<body>\n"
                "%s\n"
                "</body>\n"
                "</html>\n",
                resp_s);
        fclose(stmt_f);

        eph->stmt_text = stmt_s; stmt_s = NULL;
        eph->stmt_size = stmt_z;
    }

    // normal return
    //contest_log_format(efs, ecs, "problem-statement-json", 1, NULL);

    eph->log_s = NULL;
    eph->update_time_us = current_time_us;
    eph->recheck_time_us = current_time_us + EJFUSE_CACHING_TIME;
    eph->ok = 1;

cleanup:
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    eph->log_s = err_s; err_s = NULL;
    eph->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "problem-statement-json", 0, NULL);
    goto cleanup;
}

int
ejudge_client_submit_run_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        int lang_id,
        const unsigned char *data,
        int size,
        long long current_time_us)
{
    int retval = -1;
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    struct curl_httppost *post_head = NULL;
    struct curl_httppost *post_tail = NULL;
    char *resp_s = NULL;
    CURLcode res = 0;
    int run_id = 0;

    err_f = open_memstream(&err_s, &err_z);

    curl = curl_easy_init();
    if (!curl) {
        fprintf(err_f, "curl_easy_init failed\n");
        goto failed;
    }

    {
        size_t url_z = 0;
        FILE *url_f = open_memstream(&url_s, &url_z);
        fprintf(url_f, "%sclient/submit-run", efs->url);
        fclose(url_f);
    }

    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "SID",
                 CURLFORM_COPYCONTENTS, esv->session_id,
                 CURLFORM_END);
    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "EJSID",
                 CURLFORM_COPYCONTENTS, esv->client_key,
                 CURLFORM_END);
    unsigned char buf[64];
    snprintf(buf, sizeof(buf), "%d", prob_id);
    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "prob_id",
                 CURLFORM_COPYCONTENTS, buf,
                 CURLFORM_END);
    snprintf(buf, sizeof(buf), "%d", lang_id);
    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "lang_id",
                 CURLFORM_COPYCONTENTS, buf,
                 CURLFORM_END);
    snprintf(buf, sizeof(buf), "%d", 1);
    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "json",
                 CURLFORM_COPYCONTENTS, buf,
                 CURLFORM_END);
    curl_formadd(&post_head, &post_tail,
                 CURLFORM_COPYNAME, "file",
                 CURLFORM_PTRCONTENTS, data,
                 CURLFORM_CONTENTSLENGTH, (long) size,
                 CURLFORM_END);

    {
        size_t resp_z = 0;
        FILE *resp_f = open_memstream(&resp_s, &resp_z);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url_s);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_f);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, post_head);
        res = curl_easy_perform(curl);
        fclose(resp_f);
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);

    if (ejudge_json_parse_submit_run_reply(err_f, resp_s, &run_id) < 0) {
        goto failed;
    }

    contest_log_format(current_time_us, ecs, "submit-run", 1, "%d %d %d %d -> %d",
                       ecs->cnts_id, prob_id, lang_id, (int) size, run_id);
    retval = 0;

cleanup:
    free(resp_s);
    curl_formfree(post_head);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    return retval;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    contest_log_format(current_time_us, ecs, "submit-run", 0, "%d %d %d %d -> %d %s",
                       ecs->cnts_id, prob_id, lang_id, (int) size, (int) err_z, err_s);
    free(err_s); err_s = NULL;
    goto cleanup;
}

void
ejudge_client_problem_runs_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemRuns *eprs) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
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
        char *s1, *s2;
        fprintf(url_f, "%sclient/list-runs-json?SID=%s&EJSID=%s&prob_id=%d&json=1&mode=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)),
                prob_id);
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);
    eprs->info_json_text = strdup(resp_s);
    eprs->info_json_size = strlen(resp_s);

    if (ejudge_json_parse_problem_runs(err_f, resp_s, eprs) < 0) {
        goto failed;
    }

    // normal return
    //contest_log_format(efs, ecs, "list-runs-json", 1, NULL);
    eprs->log_s = NULL;
    eprs->update_time_us = current_time_us;
    eprs->recheck_time_us = current_time_us + EJFUSE_CACHING_TIME;
    eprs->ok = 1;

cleanup:
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    eprs->log_s = err_s; err_s = NULL;
    eprs->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "list-runs-json", 0, NULL);
    goto cleanup;
}

void
ejudge_client_run_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int run_id,
        long long current_time_us,
        struct EjRunInfo *eri) // output
{
    char *err_s = NULL;
    size_t err_z = 0;
    FILE *err_f = NULL;
    CURL *curl = NULL;
    char *url_s = NULL;
    char *resp_s = NULL;
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
        char *s1, *s2;
        fprintf(url_f, "%sclient/run-status-json?SID=%s&EJSID=%s&run_id=%d&json=1&mode=1",
                efs->url,
                (s1 = curl_easy_escape(curl, esv->session_id, 0)),
                (s2 = curl_easy_escape(curl, esv->client_key, 0)),
                run_id);
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
        if (strlen(resp_s) != resp_z) {
            fprintf(err_f, "server reply contains NUL byte\n");
            goto failed;
        }
    }
    if (res != CURLE_OK) {
        fprintf(err_f, "request failed: %s\n", curl_easy_strerror(res));
        goto failed;
    }

    fprintf(stdout, ">%s<\n", resp_s);
    eri->info_json_text = strdup(resp_s);
    eri->info_json_size = strlen(resp_s);

    if (ejudge_json_parse_run_info(err_f, resp_s, eri) < 0) {
        goto failed;
    }

    // normal return
    //contest_log_format(efs, ecs, "run-status-json", 1, NULL);
    eri->log_s = NULL;
    eri->update_time_us = current_time_us;
    eri->recheck_time_us = current_time_us + EJFUSE_CACHING_TIME;
    eri->ok = 1;

cleanup:
    free(resp_s);
    free(url_s);
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (err_f) {
        fclose(err_f);
    }
    free(err_s);
    return;

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    eri->log_s = err_s; err_s = NULL;
    eri->recheck_time_us = current_time_us + EJFUSE_RETRY_TIME;
    contest_log_format(current_time_us, ecs, "run-status-json", 0, NULL);
    goto cleanup;
}
