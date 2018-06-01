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
#include "contests_state.h"
#include "ejfuse.h"
#include "cJSON.h"
#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
int
ejudge_json_parse_top_session(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjTopSession *ets) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jsession_id = cJSON_GetObjectItem(jresult, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            ets->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(jresult, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            ets->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(jresult, "expire");
        if (jexpire && jexpire->type == cJSON_Number) {
            ets->expire_us = (time_t) jexpire->valuedouble * 1000000LL;
        } else {
            goto invalid_json;
        }
        printf("session_id: %s\n", ets->session_id);
        printf("client_key: %s\n", ets->client_key);
        printf("expire: %lld\n", (long long) ets->expire_us);
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }
    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

    /*
{
  "ok": true,
  "result": {
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
}
     */
int
ejudge_json_parse_contest_list(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestList *ecl) // out
{
    int retval = -1;
    cJSON *root = NULL;
    retval = 0;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jcontests = cJSON_GetObjectItem(jresult, "contests");
        if (!jcontests || jcontests->type != cJSON_Array) {
            goto invalid_json;
        }
        ecl->count = cJSON_GetArraySize(jcontests);
        if (ecl->count > 0) {
            ecl->entries = calloc(ecl->count, sizeof(ecl->entries[0]));
            for (int j = 0; j < ecl->count; ++j) {
                cJSON *jc = cJSON_GetArrayItem(jcontests, j);
                if (!jc || jc->type != cJSON_Object) {
                    goto invalid_json;
                }
                cJSON *jid = cJSON_GetObjectItem(jc, "id");
                if (!jid || jid->type != cJSON_Number) {
                    goto invalid_json;
                }
                ecl->entries[j].id = jid->valueint;
                cJSON *jname = cJSON_GetObjectItem(jc, "name");
                if (!jname || jname->type != cJSON_String) {
                    goto invalid_json;
                }
                ecl->entries[j].name = strdup(jname->valuestring);
            }
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

    /*
{
  "ok": true,
  "result": {
    "session": "8d17dfe31ca46bc9-a312d850aecb7000",
    "SID": "8d17dfe31ca46bc9",
    "EJSID": "a312d850aecb7000",
    "contest_id": 3,
    "expire": 1526030072,
    "user_id": 2,
    "user_login": "user01",
    "user_name": "user01",
    "cntsreg": {
      "status": "ok"
    }
  }
}
     */
int
ejudge_json_parse_contest_session(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestSession *ecc) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jsession_id = cJSON_GetObjectItem(jresult, "SID");
        if (jsession_id && jsession_id->type == cJSON_String) {
            ecc->session_id = strdup(jsession_id->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jclient_key = cJSON_GetObjectItem(jresult, "EJSID");
        if (jclient_key && jclient_key->type == cJSON_String) {
            ecc->client_key = strdup(jclient_key->valuestring);
        } else {
            goto invalid_json;
        }
        cJSON *jexpire = cJSON_GetObjectItem(jresult, "expire");
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

    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

int
ejudge_json_parse_contest_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestInfo *eci) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jproblems = cJSON_GetObjectItem(jresult, "problems");
        if (jproblems) {
            if (jproblems->type != cJSON_Array) goto invalid_json;
        }
        int prob_count = cJSON_GetArraySize(jproblems);
        int max_prob_id = 0;
        for (int i = 0; i < prob_count; ++i) {
            cJSON *jp = cJSON_GetArrayItem(jproblems, i);
            if (!jp || jp->type != cJSON_Object) goto invalid_json;
            cJSON *jid = cJSON_GetObjectItem(jp, "id");
            if (!jid || jid->type != cJSON_Number) goto invalid_json;
            int id = jid->valueint;
            if (id <= 0 || id > 10000) goto invalid_json;
            if (id > max_prob_id) max_prob_id = id;
        }
        if (max_prob_id > 0) {
            eci->prob_size = max_prob_id + 1;
            eci->probs = calloc(eci->prob_size, sizeof(eci->probs[0]));
        }
        for (int i = 0; i < prob_count; ++i) {
            cJSON *jp = cJSON_GetArrayItem(jproblems, i);
            cJSON *jid = cJSON_GetObjectItem(jp, "id");
            int id = jid->valueint;
            struct EjContestProblem *ecp = contest_problem_create(id);
            eci->probs[id] = ecp;
            cJSON *jshort_name = cJSON_GetObjectItem(jp, "short_name");
            if (!jshort_name || jshort_name->type != cJSON_String) goto invalid_json;
            ecp->short_name = strdup(jshort_name->valuestring);
            cJSON *jlong_name = cJSON_GetObjectItem(jp, "long_name");
            if (jlong_name) {
                if (jlong_name->type != cJSON_String) goto invalid_json;
                ecp->long_name = strdup(jlong_name->valuestring);
            }
        }
        cJSON *jlangs = cJSON_GetObjectItem(jresult, "compilers");
        if (jlangs) {
            if (jlangs->type != cJSON_Array) goto invalid_json;
            int lang_count = cJSON_GetArraySize(jlangs);
            int max_lang_id = 0;
            for (int i = 0; i < lang_count; ++i) {
                cJSON *jl = cJSON_GetArrayItem(jlangs, i);
                if (!jl || jl->type != cJSON_Object) goto invalid_json;
                cJSON *jid = cJSON_GetObjectItem(jl, "id");
                if (!jid || jid->type != cJSON_Number) goto invalid_json;
                int id = jid->valueint;
                if (id <= 0 || id > 10000) goto invalid_json;
                if (id > max_lang_id) max_lang_id = id;
            }
            if (max_lang_id > 0) {
                eci->compiler_size = max_lang_id + 1;
                eci->compilers = calloc(eci->compiler_size, sizeof(eci->compilers[0]));
            }
            for (int i = 0; i < lang_count; ++i) {
                cJSON *jl = cJSON_GetArrayItem(jlangs, i);
                cJSON *jid = cJSON_GetObjectItem(jl, "id");
                int id = jid->valueint;
                struct EjContestCompiler *ecl = contest_language_create(id);
                eci->compilers[id] = ecl;
                cJSON *jshort_name = cJSON_GetObjectItem(jl, "short_name");
                if (!jshort_name || jshort_name->type != cJSON_String) goto invalid_json;
                ecl->short_name = strdup(jshort_name->valuestring);
                cJSON *jlong_name = cJSON_GetObjectItem(jl, "long_name");
                if (jlong_name) {
                    if (jlong_name->type != cJSON_String) goto invalid_json;
                    ecl->long_name = strdup(jlong_name->valuestring);
                }
                cJSON *jsrc_suffix = cJSON_GetObjectItem(jl, "src_suffix");
                if (jsrc_suffix) {
                    if (jsrc_suffix->type != cJSON_String) goto invalid_json;
                    ecl->src_suffix = strdup(jsrc_suffix->valuestring);
                }
            }
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }

    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

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
int
ejudge_json_parse_problem_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjProblemInfo *epi) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
        if ((jj = cJSON_GetObjectItem(jp, "type"))) {
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->type = jj->valueint;
        }
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
        if ((jj = cJSON_GetObjectItem(jp, "enable_max_stack_size"))) {
            epi->enable_max_stack_size = (jj->type == cJSON_True);
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
            if (jj->type != cJSON_Number) goto invalid_json;
            epi->ok_status = jj->valueint;
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
            epi->compiler_size = max_lang_id + 1;
            epi->compilers = calloc(epi->compiler_size, sizeof(epi->compilers[0]));
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

    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

static int
sort_runs_func(const void *p1, const void *p2)
{
    const struct EjProblemRun *r1 = (const struct EjProblemRun *) p1;
    const struct EjProblemRun *r2 = (const struct EjProblemRun *) p2;
    return (r1->run_id - r2->run_id);
}

int
ejudge_json_parse_problem_runs(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjProblemRuns *eprs) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        /*
        cJSON *jj = cJSON_GetObjectItem(jresult, "server_time");
        if (jj) {
            if (jj->type != cJSON_Number || jj->valueint <= 0) goto invalid_json;
            epi->server_time = jj->valueint;
        }
        */

        cJSON *jruns = cJSON_GetObjectItem(jresult, "runs");
        if (!jruns || jruns->type != cJSON_Array) goto invalid_json;

        eprs->size = cJSON_GetArraySize(jruns);
        if (eprs->size > 0) {
            eprs->runs = calloc(eprs->size, sizeof(eprs->runs[0]));
        }
        for (int i = 0; i < eprs->size; ++i) {
            struct EjProblemRun *epr = &eprs->runs[i];
            cJSON *jrun = cJSON_GetArrayItem(jruns, i);
            if (!jrun || jrun->type != cJSON_Object) goto invalid_json;

            cJSON *jj = cJSON_GetObjectItem(jrun, "run_id");
            if (!jj || jj->type != cJSON_Number) goto invalid_json;
            epr->run_id = jj->valueint;

            jj = cJSON_GetObjectItem(jrun, "prob_id");
            if (!jj || jj->type != cJSON_Number) goto invalid_json;
            epr->prob_id = jj->valueint;

            jj = cJSON_GetObjectItem(jrun, "status");
            if (!jj || jj->type != cJSON_Number) goto invalid_json;
            epr->status = jj->valueint;

            jj = cJSON_GetObjectItem(jrun, "score");
            if (jj) {
                if (jj->type != cJSON_Number || jj->valueint < 0) goto invalid_json;
                epr->score = jj->valueint;
            } else {
                epr->score = -1;
            }

            jj = cJSON_GetObjectItem(jrun, "run_time");
            if (!jj || jj->type != cJSON_Number) goto invalid_json;
            epr->run_time_us = jj->valueint * 1000000LL;
        }
        qsort(eprs->runs, eprs->size, sizeof(eprs->runs[0]), sort_runs_func);
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }
    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

int
ejudge_json_parse_submit_run_reply(
        FILE *err_f,
        const unsigned char *resp_s,
        int *p_run_id)
{
    int retval = -1;
    cJSON *root = NULL;

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
    int run_id = -1;
    if (jok->type == cJSON_True) {
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;
        cJSON *jrun_id = cJSON_GetObjectItem(jresult, "run_id");
        if (!jrun_id || jrun_id->type != cJSON_Number) goto invalid_json;
        run_id = jrun_id->valueint;
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }
    if (p_run_id) *p_run_id = run_id;
    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

static int
parse_json_content(
        cJSON *content,
        unsigned char **p_data,
        size_t *p_size)
{
    if (!content) return 0;
    if (content->type != cJSON_Object) return -1;
    cJSON *jmethod = cJSON_GetObjectItem(content, "method");
    if (!jmethod || jmethod->type != cJSON_Number || jmethod->valueint != 1) return -1;
    cJSON *jsize = cJSON_GetObjectItem(content, "size");
    if (!jsize || jsize->type != cJSON_Number || jsize->valueint < 0) return -1;
    cJSON *jdata = cJSON_GetObjectItem(content, "data");
    if (!jdata || jdata->type != cJSON_String) return -1;
    size_t b64_size = strlen(jdata->valuestring);

    if (!b64_size) {
        if (jsize->valueint != 0) return -1;
        *p_data = malloc(1);
        *p_size = 0;
        **p_data = 0;
        return 1;
    }

    unsigned char *buf = malloc(b64_size + 1);
    if (!buf) {
        return -1;
    }

    int err = 0;
    int outlen = base64_decode(jdata->valuestring, b64_size, buf, &err);
    if (err || outlen < 0 || outlen != jsize->valueint) {
        free(buf);
        return -1;
    }
    *p_data = buf;
    *p_size = outlen;
    return 1;
}

static int
parse_json_brief_test(
        cJSON *jnode,
        const unsigned char *name,
        struct EjRunInfoTestResultData *p_data)
{
    if (!jnode) return 0;
    cJSON *jd = cJSON_GetObjectItem(jnode, name);
    if (!jd) return 0;
    if (jd->type != cJSON_Object) return -1;

    cJSON *jj = cJSON_GetObjectItem(jd, "is_too_big");
    if (jj && jj->type == cJSON_True) p_data->is_too_big = 1;

    jj = cJSON_GetObjectItem(jd, "is_binary");
    if (jj && jj->type == cJSON_True) p_data->is_binary = 1;

    jj = cJSON_GetObjectItem(jd, "size");
    if (jj) {
        if (jj->type != cJSON_Number || jj->valueint != jj->valuedouble) return -1;
        p_data->size = jj->valueint;
    }
    p_data->is_defined = 1;
    return 1;
}

int
ejudge_json_parse_run_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjRunInfo *eri) // out
{
    int retval = -1;
    cJSON *root = NULL;

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
      cJSON *jresult = cJSON_GetObjectItem(root, "result");
      if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

      cJSON *jj = cJSON_GetObjectItem(jresult, "server_time");
      if (!jj || jj->type != cJSON_Number) goto invalid_json;
      eri->server_time = jj->valueint;

      cJSON *jrun = cJSON_GetObjectItem(jresult, "run");
      if (!jrun || jrun->type != cJSON_Object) goto invalid_json;

      jj = cJSON_GetObjectItem(jrun, "run_id");
      if (!jj || jj->type != cJSON_Number) goto invalid_json;

      jj = cJSON_GetObjectItem(jrun, "prob_id");
      if (!jj || jj->type != cJSON_Number) goto invalid_json;
      eri->prob_id = jj->valueint;

      jj = cJSON_GetObjectItem(jrun, "run_time_us");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->run_time_us = jj->valuedouble;
      }

      jj = cJSON_GetObjectItem(jrun, "run_time");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->run_time = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "duration");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->duration = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "lang_id");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->lang_id = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "user_id");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->user_id = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "size");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->size = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "status");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->status = jj->valueint;
      }

      jj = cJSON_GetObjectItem(jrun, "is_imported");
      if (jj && jj->type == cJSON_True) {
          eri->is_imported = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_hidden");
      if (jj && jj->type == cJSON_True) {
          eri->is_hidden = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_with_duration");
      if (jj && jj->type == cJSON_True) {
          eri->is_with_duration = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_standard_problem");
      if (jj && jj->type == cJSON_True) {
          eri->is_standard_problem = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_minimal_report");
      if (jj && jj->type == cJSON_True) {
          eri->is_minimal_report = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_with_effective_time");
      if (jj && jj->type == cJSON_True) {
          eri->is_with_effective_time = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "effective_time");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->effective_time = jj->valueint;
      }
      jj = cJSON_GetObjectItem(jrun, "is_src_enabled");
      if (jj && jj->type == cJSON_True) {
          eri->is_src_enabled = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "src_sfx");
      if (jj) {
        if (jj->type != cJSON_String) goto invalid_json;
        eri->src_sfx = strdup(jj->valuestring);
      }
      jj = cJSON_GetObjectItem(jrun, "is_report_enabled");
      if (jj && jj->type == cJSON_True) {
          eri->is_report_enabled = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_failed_test_available");
      if (jj && jj->type == cJSON_True) {
          eri->is_failed_test_available = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "failed_test");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->failed_test = jj->valueint;
      }
      jj = cJSON_GetObjectItem(jrun, "is_passed_tests_available");
      if (jj && jj->type == cJSON_True) {
          eri->is_passed_tests_available = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "passed_tests");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->passed_tests = jj->valueint;
      }
      jj = cJSON_GetObjectItem(jrun, "is_score_available");
      if (jj && jj->type == cJSON_True) {
          eri->is_score_available = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "score");
      if (jj) {
        if (jj->type != cJSON_Number) goto invalid_json;
        eri->score = jj->valueint;
      }
      jj = cJSON_GetObjectItem(jrun, "score_str");
      if (jj) {
        if (jj->type != cJSON_String) goto invalid_json;
        eri->score_str = strdup(jj->valuestring);
      }
      jj = cJSON_GetObjectItem(jrun, "is_compiler_output_available");
      if (jj && jj->type == cJSON_True) {
          eri->is_compiler_output_available = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "is_report_available");
      if (jj && jj->type == cJSON_True) {
          eri->is_report_available = 1;
      }
      jj = cJSON_GetObjectItem(jrun, "message_count");
      if (jj) {
          if (jj->type != cJSON_Number) goto invalid_json;
          eri->message_count = jj->valueint;
      }

      cJSON *jco = cJSON_GetObjectItem(jresult, "compiler_output");
      if (jco) {
          if (jco->type != cJSON_Object) goto invalid_json;
          if (parse_json_content(cJSON_GetObjectItem(jco, "content"), &eri->compiler_text, &eri->compiler_size) < 0) goto invalid_json;
      }

      cJSON *jtr = cJSON_GetObjectItem(jresult, "testing_report");
      if (jtr) {
          if (jtr->type != cJSON_Object) goto invalid_json;

          jj = cJSON_GetObjectItem(jtr, "valuer_comment");
          if (jj) {
              if (jj->type != cJSON_Object) goto invalid_json;
              if (parse_json_content(cJSON_GetObjectItem(jj, "content"), &eri->valuer_text, &eri->valuer_size) < 0) goto invalid_json;
          }

          cJSON *jtests = cJSON_GetObjectItem(jtr, "tests");
          if (jtests) {
              if (jtests->type != cJSON_Array) goto invalid_json;
              int count = cJSON_GetArraySize(jtests);
              eri->test_count = count;
              eri->tests = calloc(count, sizeof(eri->tests[0]));
              if (!eri->tests) goto invalid_json;
              for (int i = 0; i < count; ++i) {
                  cJSON *jtest = cJSON_GetArrayItem(jtests, i);
                  struct EjRunInfoTestResult *et = &eri->tests[i];
                  if (!jtest || jtest->type != cJSON_Object) goto invalid_json;

                  jj = cJSON_GetObjectItem(jtest, "num");
                  if (!jj || jj->type != cJSON_Number) goto invalid_json;
                  et->num = jj->valueint;

                  jj = cJSON_GetObjectItem(jtest, "is_visibility_exists");
                  if (!jj || jj->type == cJSON_False) {
                      jj = cJSON_GetObjectItem(jtest, "status");
                      if (jj) {
                          if (jj->type != cJSON_Number) goto invalid_json;
                          // FIXME: check status validity
                          et->status = jj->valueint;
                      }
                      jj = cJSON_GetObjectItem(jtest, "time_ms");
                      if (jj) {
                          if (jj->type != cJSON_Number) goto invalid_json;
                          et->time_ms = jj->valueint;
                      }
                      jj = cJSON_GetObjectItem(jtest, "score");
                      if (jj) {
                          if (jj->type != cJSON_Number) goto invalid_json;
                          et->score = jj->valueint;
                      }
                      jj = cJSON_GetObjectItem(jtest, "max_score");
                      if (jj) {
                          if (jj->type != cJSON_Number) goto invalid_json;
                          et->max_score = jj->valueint;
                      }
                      jj = cJSON_GetObjectItem(jtest, "is_visibility_full");
                      if (jj && jj->type == cJSON_True) {
                          et->is_visibility_full = 1;
                          eri->is_test_available = 1;
                      }

                      if (parse_json_brief_test(jtest, "args", &et->data[TESTING_REPORT_ARGS]) < 0) goto invalid_json;
                      if (parse_json_brief_test(jtest, "input", &et->data[TESTING_REPORT_INPUT]) < 0) goto invalid_json;
                      if (parse_json_brief_test(jtest, "output", &et->data[TESTING_REPORT_OUTPUT]) < 0) goto invalid_json;
                      if (parse_json_brief_test(jtest, "correct", &et->data[TESTING_REPORT_CORRECT]) < 0) goto invalid_json;
                      if (parse_json_brief_test(jtest, "error", &et->data[TESTING_REPORT_ERROR]) < 0) goto invalid_json;
                      if (parse_json_brief_test(jtest, "checker", &et->data[TESTING_REPORT_CHECKER]) < 0) goto invalid_json;
                  }
              }
          }
      }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }
    retval = 0;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}

int
ejudge_json_parse_run_messages(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjRunMessages *erms) // out
{
    int retval = -1;
    cJSON *root = NULL;
    long long latest_time_us = 0;

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
        cJSON *jresult = cJSON_GetObjectItem(root, "result");
        if (!jresult || jresult->type != cJSON_Object) goto invalid_json;

        cJSON *jj = cJSON_GetObjectItem(jresult, "server_time");
        if (!jj || jj->type != cJSON_Number) goto invalid_json;
        erms->server_time = jj->valueint;

        cJSON *jms = cJSON_GetObjectItem(jresult, "messages");
        if (jms) {
            if (jms->type != cJSON_Array) goto invalid_json;
            int count = cJSON_GetArraySize(jms);
            erms->count = count;
            if (count > 0) {
                erms->messages = calloc(count, sizeof(erms->messages[0]));
                for (int i = 0; i < count; ++i) {
                    cJSON *jm = cJSON_GetArrayItem(jms, i);
                    if (!jm || jm->type != cJSON_Object) goto invalid_json;
                    struct EjRunMessage *erm = &erms->messages[i];

                    jj = cJSON_GetObjectItem(jm, "clar_id");
                    if (!jj || jj->type != cJSON_Number || jj->valueint != jj->valuedouble) goto invalid_json;
                    erm->clar_id = jj->valueint;
                    jj = cJSON_GetObjectItem(jm, "time_us");
                    if (!jj || jj->type != cJSON_Number) goto invalid_json;
                    erm->time_us = jj->valuedouble;
                    if (erm->time_us > latest_time_us) latest_time_us = erm->time_us;
                    jj = cJSON_GetObjectItem(jm, "from");
                    if (!jj || jj->type != cJSON_Number || jj->valueint != jj->valuedouble) goto invalid_json;
                    erm->from = jj->valueint;
                    jj = cJSON_GetObjectItem(jm, "to");
                    if (!jj || jj->type != cJSON_Number || jj->valueint != jj->valuedouble) goto invalid_json;
                    erm->to = jj->valueint;
                    jj = cJSON_GetObjectItem(jm, "subject");
                    if (!jj || jj->type != cJSON_String) goto invalid_json;
                    erm->subject = strdup(jj->valuestring);
                    if (parse_json_content(cJSON_GetObjectItem(jj, "content"), &erm->data, &erm->size) < 0) goto invalid_json;
                }
            }
        }
    } else if (jok->type == cJSON_False) {
        fprintf(err_f, "request failed at server side: <%s>\n", resp_s);
        goto failed;
    } else {
        goto invalid_json;
    }
    retval = 0;
    erms->latest_time_us = latest_time_us;

cleanup:
    if (root) cJSON_Delete(root);
    return retval;

invalid_json:
    fprintf(err_f, "invalid JSON response: <%s>\n", resp_s);
failed:
    goto cleanup;
}
