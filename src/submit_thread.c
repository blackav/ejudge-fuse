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

#include "submit_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

struct EjSubmitListItem
{
    struct EjSubmitListItem *prev, *next;
    struct EjSubmitItem *item;
};

struct EjSubmitThread
{
    pthread_t id;
    struct EjFuseState *efs;

    pthread_mutex_t qm;
    pthread_cond_t qc;
    struct EjSubmitListItem *qhead, *qtail;
};

struct EjSubmitThread *
submit_thread_create(void)
{
    struct EjSubmitThread *st = calloc(1, sizeof(*st));
    pthread_mutex_init(&st->qm, NULL);
    pthread_cond_init(&st->qc, NULL);
    return st;
}

void
submit_thread_free(struct EjSubmitThread *st)
{
    if (st) {
        pthread_cond_destroy(&st->qc);
        pthread_mutex_destroy(&st->qm);
        free(st);
    }
}

struct EjSubmitItem *
submit_item_create(
        long long submit_time_us,
        int cnts_id,
        int prob_id,
        int lang_id,
        int fnode,
        const unsigned char *fname)
{
    struct EjSubmitItem *si = calloc(1, sizeof(*si));
    si->submit_time_us = submit_time_us;
    si->cnts_id = cnts_id;
    si->prob_id = prob_id;
    si->lang_id = lang_id;
    si->fnode = fnode;
    if (fname && *fname) {
        si->fname = strdup(fname);
    }
    return si;
}

void
submit_item_free(struct EjSubmitItem *si)
{
    if (si) {
        free(si->fname);
        free(si);
    }
}

void
submit_thread_enqueue(struct EjSubmitThread *st, struct EjSubmitItem *si)
{
    struct EjSubmitListItem *sli = calloc(1, sizeof(*sli));
    sli->item = si;

    pthread_mutex_lock(&st->qm);
    sli->prev = st->qtail;
    if (st->qtail) {
        st->qtail->next = sli;
    } else {
        st->qhead = sli;
    }
    st->qtail = sli;
    pthread_cond_signal(&st->qc);
    pthread_mutex_unlock(&st->qm);
}

static void
thread_submit(struct EjSubmitThread *st, struct EjSubmitItem *si)
{
    long long current_time_us;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    current_time_us = tv.tv_sec * 1000000LL + tv.tv_usec;

    fprintf(stderr, "SUBMIT: %lld, %lld, %d, %d, %d, %d, %s\n",
            si->submit_time_us, current_time_us, si->cnts_id, si->prob_id, si->lang_id, si->fnode, si->fname);
}

static void *
thread_func(void *arg)
{
    struct EjSubmitThread *st = (struct EjSubmitThread *) arg;

    st->id = pthread_self(); // both parent and child do this

    while (1) {
        pthread_mutex_lock(&st->qm);
        while (!st->qhead) {
            pthread_cond_wait(&st->qc, &st->qm);
        }
        struct EjSubmitListItem *sli = st->qhead;
        st->qhead = sli->next;
        if (st->qhead) {
            st->qhead->prev = NULL;
        } else {
            st->qtail = NULL;
        }
        sli->next = NULL;
        pthread_mutex_unlock(&st->qm);
        struct EjSubmitItem *si = sli->item;
        free(sli); sli = NULL;
        if (si) {
            thread_submit(st, si);
        }
        submit_item_free(si);

        // rate limiter: 5s timeout
        nanosleep(&(struct timespec) { .tv_sec = 5, .tv_nsec = 0 }, NULL);
    }

    return NULL;
}

int
submit_thread_start(struct EjSubmitThread *st, struct EjFuseState *efs)
{
    pthread_attr_t pa;
    pthread_t id;

    st->efs = efs;

    pthread_attr_init(&pa);
    pthread_attr_setstacksize(&pa, 1024 * 1024);
    int res = pthread_create(&id, &pa, thread_func, st);
    pthread_attr_destroy(&pa);
    if (res) {
        return -res;
    }

    st->id = id; // both parent and child to this

    return 0;
}
