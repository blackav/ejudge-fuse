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

struct EjContestInfo;
struct EjContestList;
struct EjContestSession;
struct EjContestState;
struct EjFuseState;
struct EjProblemInfo;
struct EjProblemStatement;
struct EjSessionValue;
struct EjProblemRuns;

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
