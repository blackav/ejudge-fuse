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
        struct EjSessionValue *esv,
        struct EjContestList *contests);

void
ejudge_client_enter_contest_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjContestSession *ecc,
        const unsigned char *session_id,
        const unsigned char *client_key);

void
ejudge_client_contest_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjContestInfo *eci,
        const unsigned char *session_id,
        const unsigned char *client_key);

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
