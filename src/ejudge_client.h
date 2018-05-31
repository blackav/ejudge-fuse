#pragma once

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

#include <stdio.h>

struct EjContestInfo;
struct EjContestList;
struct EjContestSession;
struct EjContestState;
struct EjFuseState;
struct EjProblemInfo;
struct EjProblemRuns;
struct EjProblemStatement;
struct EjRunInfo;
struct EjRunMessages;
struct EjRunSource;
struct EjSessionValue;
struct EjTopSession;

void
ejudge_client_get_top_session_request(
        struct EjFuseState *efs,
        long long current_time_us,
        struct EjTopSession *tls); // output
void
ejudge_client_get_contest_list_request(
        struct EjFuseState *efs,
        const struct EjSessionValue *esv,
        long long current_time_us, 
        struct EjContestList *contests); // output

void
ejudge_client_enter_contest_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        long long current_time_us,
        struct EjContestSession *ecc); // output

void
ejudge_client_contest_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        long long current_time_us,
        struct EjContestInfo *eci); // output

void
ejudge_client_problem_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemInfo *epi); // output

void
ejudge_client_problem_statement_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemStatement *eph); // output
int
ejudge_client_submit_run_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        int lang_id,
        const unsigned char *data,
        int size,
        long long current_time_us);
void
ejudge_client_problem_runs_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        long long current_time_us,
        struct EjProblemRuns *eprs); // output
void
ejudge_client_run_info_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int run_id,
        long long current_time_us,
        struct EjRunInfo *eri); // output
void
ejudge_client_run_source_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int run_id,
        long long current_time_us,
        struct EjRunSource *ert); // output
void
ejudge_client_run_messages_request(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int run_id,
        long long current_time_us,
        struct EjRunMessages *erm); // output

int
ejudge_json_parse_top_session(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjTopSession *ets); // out
int
ejudge_json_parse_contest_list(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestList *ecl); // out
int
ejudge_json_parse_contest_session(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestSession *ecc); // out
int
ejudge_json_parse_contest_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjContestInfo *eci); // out
int
ejudge_json_parse_problem_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjProblemInfo *epi); // out
int
ejudge_json_parse_submit_run_reply(
        FILE *err_f,
        const unsigned char *resp_s,
        int *p_run_id);
int
ejudge_json_parse_problem_runs(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjProblemRuns *eprs);
int
ejudge_json_parse_run_info(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjRunInfo *eri); // out
int
ejudge_json_parse_run_messages(
        FILE *err_f,
        const unsigned char *resp_s,
        struct EjRunMessages *erms); // out
