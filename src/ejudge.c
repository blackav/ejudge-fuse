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

#include "ejudge.h"

#include <stdio.h>
#include <assert.h>

const char * const run_statuses[] =
{
    [RUN_OK] = "OK",
    [RUN_COMPILE_ERR] = "Compilation error",
    [RUN_RUN_TIME_ERR] = "Run-time error",
    [RUN_TIME_LIMIT_ERR] = "Time-limit exceeded",
    [RUN_PRESENTATION_ERR] = NULL, // filled up later
    [RUN_WRONG_ANSWER_ERR] = "Wrong answer",
    [RUN_CHECK_FAILED] = "Check failed",
    [RUN_PARTIAL] = NULL, // filled up later
    [RUN_ACCEPTED] = "Accepted for testing",
    [RUN_IGNORED] = "Ignored",
    [RUN_DISQUALIFIED] = "Disqualified",
    [RUN_PENDING] = "Pending check",
    [RUN_MEM_LIMIT_ERR] = "Memory limit exceeded",
    [RUN_SECURITY_ERR] = "Security violation",
    [RUN_SYNC_ERR] = "Synchronization error",
    [RUN_STYLE_ERR] = "Coding style violation",
    [RUN_REJECTED] = "Rejected",
    [RUN_WALL_TIME_LIMIT_ERR] = "Wall time-limit exceeded",
    [RUN_SKIPPED] = "Skipped",
    [RUN_PENDING_REVIEW] = "Pending review",
    [RUN_SUMMONED] = "Summoned for defence",
    [RUN_RUNNING] = "Running...",
    [RUN_COMPILED] = "Compiled",
    [RUN_COMPILING] = "Compiling...",
    [RUN_AVAILABLE] = "Available",
    [RUN_VIRTUAL_START] = "Virtual start",
    [RUN_VIRTUAL_STOP] = "Virtual stop",
    [RUN_EMPTY] = "EMPTY",
};

const char *
run_status_str(
        int status,
        char *out,
        size_t len,
        int prob_type,
        int var_score)
{
    char  zbuf[128];
    char const  *s;

    // ensure MT-safety
    assert(out);
    assert(len > 0);

    if (status < 0 || status >= sizeof(run_statuses) / sizeof(run_statuses[0])) {
        if (snprintf(zbuf, sizeof(zbuf), "Unknown: %d", status) >= sizeof(zbuf)) {
            abort();
        }
        s = zbuf;
    } else {
        s = run_statuses[status];
        if (!s) {
            if (status == RUN_PRESENTATION_ERR) {
                if (prob_type && prob_type != PROB_TYPE_TESTS) {
                    s = "Wrong output format";
                } else {
                    s = "Presentation error";
                }
            } else if (status == RUN_PARTIAL) {
                if (prob_type && !var_score && prob_type != PROB_TYPE_TESTS) {
                    s = "Wrong answer";
                } else {
                    s = "Partial solution";
                }
            } else {
                if (snprintf(zbuf, sizeof(zbuf), "Unknown: %d", status) >= sizeof(zbuf)) {
                    abort();
                }
                s = zbuf;
            }
        }
    }
    if (snprintf(out, len, "%s", s) >= len) {
        abort();
    }
    return out;
}

const unsigned char * const problem_type_str[] =
{
    [PROB_TYPE_STANDARD] = "standard",
    [PROB_TYPE_OUTPUT_ONLY] = "output-only",
    [PROB_TYPE_SHORT_ANSWER] = "short-answer",
    [PROB_TYPE_TEXT_ANSWER] = "text-answer",
    [PROB_TYPE_SELECT_ONE] = "select-one",
    [PROB_TYPE_SELECT_MANY] = "select-many",
    [PROB_TYPE_CUSTOM] = "custom",
    [PROB_TYPE_TESTS] = "tests",

    [PROB_TYPE_LAST] = 0,
};

const unsigned char *
problem_unparse_type(int val)
{
    if (val >= 0 && val < PROB_TYPE_LAST) {
        return problem_type_str[val];
    }
    return NULL;
}
