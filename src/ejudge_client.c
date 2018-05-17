#include "ejudge_client.h"

#include "contests_state.h"
#include "ejfuse.h"
#include "cJSON.h"

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

void
contest_log_format(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const char *action,
        int success,
        const char *format, ...)
    __attribute__((format(printf, 5, 6)));

/*
{
  "ok" : true,
  "result": {
    "server_time": 1526308302,
    "problem": {
      "id": 1,
      "short_name": "A",
      "long_name": "Sum 1",
      "type": "standard",
      "full_score": 100,
      "use_stdin": true,
      "use_stdout": true,
      "real_time_limit_ms": 5000,
      "time_limit_ms": 1000,
      "max_vm_size": "67108864",
      "max_stack_size": "67108864"
    },
    "problem_status": {
      "is_viewable" : true,
      "is_submittable" : true,
      "is_tabable" : true
    }
  }
}
 */
void
ejudge_client_problem_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemInfo *epi, // output
        const struct EjSessionValue *esv,
        int prob_id)
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
        fprintf(url_f, "%sclient/problem-status-json?SID=%s&EJSID=%s&problem=%d&json=1",
                ejs->url,
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
        epi->info_json_text = strdup(resp_s);
        epi->info_json_size = strlen(resp_s);

        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jj = cJSON_GetObjectItem(jresult, "server_time");
        if (jj) {
            if (jj->type != cJSON_Number || jj->valueint <= 0) goto invalid_json;
            epi->server_time = jj->valueint;
        }

        cJSON *jp = cJSON_GetObjectItem(jresult, "problem");
        if (!jp || jp->type != cJSON_Object) goto invalid_json;
        if ((jj = cJSON_GetObjectItem(jp, "short_name"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->short_name = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "long_name"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->long_name = strdup(jj->valuestring);
        }
        // FIXME: parse type
        /*
  fprintf(fout, ",\n      \"type\": \"%s\"", json_armor_buf(&ab, problem_unparse_type(prob->type)));
        */
        if ((jj = cJSON_GetObjectItem(jp, "full_score"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->full_score = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "full_user_score"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->full_user_score = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "min_score_1"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->min_score_1 = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "min_score_2"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->min_score_2 = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "use_stdin"))) {
            epi->use_stdin = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "use_stdout"))) {
            epi->use_stdout = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "combined_stdin"))) {
            epi->combined_stdin = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "combined_stdout"))) {
            epi->combined_stdout = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "use_ac_not_ok"))) {
            epi->use_ac_not_ok = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "ignore_prev_ac"))) {
            epi->ignore_prev_ac = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "team_enable_rep_view"))) {
            epi->team_enable_rep_view = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "team_enable_ce_view"))) {
            epi->team_enable_ce_view = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "ignore_compile_errors"))) {
            epi->ignore_compile_errors = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_user_submit"))) {
            epi->disable_user_submit = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_tab"))) {
            epi->disable_tab = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "enable_submit_after_reject"))) {
            epi->enable_submit_after_reject = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "enable_tokens"))) {
            epi->enable_tokens = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "tokens_for_user_ac"))) {
            epi->tokens_for_user_ac = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_submit_after_ok"))) {
            epi->disable_submit_after_ok = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_auto_testing"))) {
            epi->disable_auto_testing = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_testing"))) {
            epi->disable_testing = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "enable_compilation"))) {
            epi->enable_compilation = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "skip_testing"))) {
            epi->skip_testing = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "hidden"))) {
            epi->hidden = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "stand_hide_time"))) {
            epi->stand_hide_time = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "stand_ignore_score"))) {
            epi->stand_ignore_score = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "stand_last_column"))) {
            epi->stand_last_column = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "disable_stderr"))) {
            epi->disable_stderr = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "is_statement_avaiable"))) {
            epi->is_statement_avaiable = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(jp, "real_time_limit_ms"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->real_time_limit_ms = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "time_limit_ms"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->time_limit_ms = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "acm_run_penalty"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->acm_run_penalty = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "test_score"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->test_score = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "run_penalty"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->run_penalty = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "disqualified_penalty"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->disqualified_penalty = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "compile_error_penalty"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->compile_error_penalty = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "tests_to_accept"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->tests_to_accept = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "min_tests_to_accept"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->min_tests_to_accept = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "score_multiplier"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->score_multiplier = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "max_user_run_count"))) {
            if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
            epi->max_user_run_count = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "stand_name"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->stand_name = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "stand_column"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->stand_column = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "group_name"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->group_name = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "input_file"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->input_file = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "output_file"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->output_file = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "ok_status"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            epi->ok_status = strdup(jj->valuestring);
        }
        if ((jj = cJSON_GetObjectItem(jp, "start_date"))) {
            if (jj->type != cJSON_Number || jj->valueint <= 0) goto invalid_json;
            epi->start_date = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(jp, "compilers"))) {
            if (jj->type != cJSON_Array) goto invalid_json;
            int max_lang_id = 0;
            int arr_size = cJSON_GetArraySize(jj);
            for (int i = 0; i < arr_size; ++i) {
                cJSON *jl = cJSON_GetArrayItem(jj, i);
                if (!jl || jl->type != cJSON_Number || jl->valueint <= 0 || jl->valueint > 1024) goto invalid_json;
                if (jl->valueint > max_lang_id) max_lang_id = jl->valueint;
            }
            epi->compilers_size = max_lang_id + 1;
            epi->compilers = calloc(epi->compilers_size, sizeof(epi->compilers[0]));
            for (int i = 0; i < arr_size; ++i) {
                cJSON *jl = cJSON_GetArrayItem(jj, i);
                epi->compilers[jl->valueint] = 1;
            }
        }
        if ((jj = cJSON_GetObjectItem(jp, "max_vm_size"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            errno = 0;
            char *eptr = NULL;
            unsigned long long val = strtoull(jj->valuestring, &eptr, 10);
            if (errno || *eptr || jj->valuestring == eptr) goto invalid_json;
            epi->max_vm_size = val;
        }
        if ((jj = cJSON_GetObjectItem(jp, "max_stack_size"))) {
            if (jj->type != cJSON_String) goto invalid_json;
            errno = 0;
            char *eptr = NULL;
            unsigned long long val = strtoull(jj->valuestring, &eptr, 10);
            if (errno || *eptr || jj->valuestring == eptr) goto invalid_json;
            epi->max_stack_size = val;
        }
        if ((jj = cJSON_GetObjectItem(jp, "est_stmt_size"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->est_stmt_size = jj->valueint;
        }

        cJSON *js = cJSON_GetObjectItem(jresult, "problem_status");
        if ((jj = cJSON_GetObjectItem(js, "is_viewable"))) {
            epi->is_viewable = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_submittable"))) {
            epi->is_submittable = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_tabable"))) {
            epi->is_tabable = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_solved"))) {
            epi->is_solved = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_accepted"))) {
            epi->is_accepted = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_pending"))) {
            epi->is_pending = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_pending_review"))) {
            epi->is_pending_review = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_transient"))) {
            epi->is_transient = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_last_untokenized"))) {
            epi->is_last_untokenized = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_marked"))) {
            epi->is_marked = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_autook"))) {
            epi->is_autook = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "is_eff_time_needed"))) {
            epi->is_eff_time_needed = (jj->type == cJSON_True);
        }
        if ((jj = cJSON_GetObjectItem(js, "best_run"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->best_run = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "attempts"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->attempts = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "disqualified"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->disqualified = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "ce_attempts"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->ce_attempts = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "best_score"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->best_score = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "prev_successes"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->prev_successes = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "all_attempts"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->all_attempts = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "eff_attempts"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->eff_attempts = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "token_count"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->token_count = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "deadline"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->deadline = jj->valueint;
        }
        if ((jj = cJSON_GetObjectItem(js, "effective_time"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->effective_time = jj->valueint;
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    // normal return
    //contest_log_format(ejs, ecs, "contest-status-json", 1, NULL);
    epi->log_s = NULL;
    epi->recheck_time_us = ejs->current_time_us + 10000000; // +10s
    epi->ok = 1;

cleanup:
    if (root) {
        cJSON_Delete(root);
    }
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

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);

failed:
    if (err_f) {
        fclose(err_f); err_f = NULL;
    }
    epi->log_s = err_s; err_s = NULL;
    epi->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    contest_log_format(ejs, ecs, "contest-problem-json", 0, NULL);
    goto cleanup;
}

void
ejudge_client_problem_statement_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemStatement *eph, // output
        const struct EjSessionValue *esv,
        int prob_id)
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
        fprintf(url_f, "%sclient/problem-statement-json?SID=%s&EJSID=%s&problem=%d&json=1",
                ejs->url,
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

    // normal return
    //contest_log_format(ejs, ecs, "problem-statement-json", 1, NULL);
    eph->stmt_text = resp_s; resp_s = NULL;
    eph->stmt_size = strlen(eph->stmt_text);

    eph->log_s = NULL;
    eph->recheck_time_us = ejs->current_time_us + 10000000; // +10s
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
    eph->recheck_time_us = ejs->current_time_us + 10000000; // 10s
    contest_log_format(ejs, ecs, "problem-statement-json", 0, NULL);
    goto cleanup;
}
