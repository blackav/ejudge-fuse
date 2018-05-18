#pragma once

struct EjFuseState;
struct EjContestState;
struct EjProblemInfo;
struct EjSessionValue;
struct EjContestInfo;

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
        struct EjProblemInfo *epi, // output
        const struct EjSessionValue *esv,
        int prob_id);

struct EjProblemStatement;
void
ejudge_client_problem_statement_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemStatement *eph, // output
        const struct EjSessionValue *esv,
        int prob_id);
