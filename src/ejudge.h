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

#include <stdlib.h>

enum
{
  PROB_TYPE_STANDARD = 0,       /* standard problem */
  PROB_TYPE_OUTPUT_ONLY,        /* output-only problem */
  PROB_TYPE_SHORT_ANSWER,       /* output-only with short answer */
  PROB_TYPE_TEXT_ANSWER,        /* output-only with textarea input */
  PROB_TYPE_SELECT_ONE,         /* select one answer from the list */
  PROB_TYPE_SELECT_MANY,        /* select many answers from the list */
  PROB_TYPE_CUSTOM,             /* custom form (part of prob. stmt) */
  PROB_TYPE_TESTS,              /* test suite submit */

  PROB_TYPE_LAST,
};

enum
{
  RUN_OK               = 0,
  RUN_COMPILE_ERR      = 1,
  RUN_RUN_TIME_ERR     = 2,
  RUN_TIME_LIMIT_ERR   = 3,
  RUN_PRESENTATION_ERR = 4,
  RUN_WRONG_ANSWER_ERR = 5,
  RUN_CHECK_FAILED     = 6,
  RUN_PARTIAL          = 7,
  RUN_ACCEPTED         = 8,
  RUN_IGNORED          = 9,
  RUN_DISQUALIFIED     = 10,
  RUN_PENDING          = 11,
  RUN_MEM_LIMIT_ERR    = 12,
  RUN_SECURITY_ERR     = 13,
  RUN_STYLE_ERR        = 14,
  RUN_WALL_TIME_LIMIT_ERR = 15,
  RUN_PENDING_REVIEW   = 16,
  RUN_REJECTED         = 17,
  RUN_SKIPPED          = 18,
  RUN_SYNC_ERR         = 19,
  OLD_RUN_MAX_STATUS   = 19, // obsoleted
  RUN_NORMAL_LAST      = 19, // may safely overlap pseudo statuses

  RUN_PSEUDO_FIRST     = 20,
  RUN_VIRTUAL_START    = 20,
  RUN_VIRTUAL_STOP     = 21,
  RUN_EMPTY            = 22,
  RUN_PSEUDO_LAST      = 22,

  RUN_SUMMONED         = 23, // summoned for oral defence
  RUN_LOW_LAST         = 23, // will be == RUN_NORMAL_LAST later

  RUN_TRANSIENT_FIRST  = 95,
  RUN_RUNNING          = 96,
  RUN_COMPILED         = 97,
  RUN_COMPILING        = 98,
  RUN_AVAILABLE        = 99,
  RUN_TRANSIENT_LAST   = 99,

  RUN_STATUS_SIZE      = 100
};

const char *
run_status_str(
        int status,
        char *out,
        size_t len,
        int prob_type,
        int var_score);

/* scoring systems */
enum
{
    SCORE_ACM,
    SCORE_KIROV,
    SCORE_OLYMPIAD,
    SCORE_MOSCOW,

    SCORE_TOTAL,
};

const unsigned char *problem_unparse_type(int val);
