#pragma once

struct EjContestInfo;
struct EjContestList;
struct EjContestSession;
struct EjContestState;
struct EjFuseState;
struct EjProblemInfo;
struct EjProblemStatement;
struct EjSessionValue;

void
ejudge_client_get_contest_list_request(
        struct EjFuseState *ejs,
        const struct EjSessionValue *esv,
        struct EjContestList *contests); // output

void
ejudge_client_enter_contest_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        struct EjContestSession *ecc); // output

void
ejudge_client_contest_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        struct EjContestInfo *eci); // output

void
ejudge_client_problem_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        struct EjProblemInfo *epi); // output

void
ejudge_client_problem_statement_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        const struct EjSessionValue *esv,
        int prob_id,
        struct EjProblemStatement *eph); // output
