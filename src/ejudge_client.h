#pragma once

struct EjFuseState;
struct EjContestState;
struct EjProblemInfo;
struct EjSessionValue;

void
ejudge_client_problem_info_request(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemInfo *epi, // output
        const struct EjSessionValue *esv,
        int prob_id);
