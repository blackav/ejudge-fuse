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

struct EjFuseState;

struct EjSubmitItem
{
    long long submit_time_us;
    int cnts_id;
    int prob_id;
    int lang_id;
    int fnode;
    unsigned char *fname;
};

struct EjSubmitThread;

struct EjSubmitThread *submit_thread_create(void);
void submit_thread_free(struct EjSubmitThread *st);

struct EjSubmitItem *
submit_item_create(
        long long submit_time_us,
        int cnts_id,
        int prob_id,
        int lang_id,
        int fnode,
        const unsigned char *fname);

int submit_thread_start(struct EjSubmitThread *st, struct EjFuseState *);

void submit_thread_enqueue(struct EjSubmitThread *st, struct EjSubmitItem *si);

