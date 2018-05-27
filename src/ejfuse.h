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

#define FUSE_USE_VERSION 26
#include <fuse.h>

struct EjTopSession
{
    _Atomic int reader_count;   // readers count for RCU

    _Bool          ok;          // status is valid
    long long      recheck_time_us;
    unsigned char *log_s;

    unsigned char *session_id;
    unsigned char *client_key;
    long long expire_us;
};

struct EjContestListItem
{
    int id;
    unsigned char *name;
};

struct EjContestList
{
    _Atomic int reader_count;   // readers count for RCU

    _Bool          ok;          // status is valid
    long long      recheck_time_us;
    unsigned char *log_s;

    long long update_time_us;  // last update time (in case of success)

    int count;
    struct EjContestListItem *entries;
};

struct EjFileNodes;
struct EjSubmitThread;
struct EjRunState;

struct EjFuseState
{
    // settings
    unsigned char *url;
    unsigned char *login;
    unsigned char *password;
    int owner_uid;
    int owner_gid;
    long long start_time_us;

    // the current time (microseconds)
    //_Atomic long long current_time_us;

    // top-level session info
    _Atomic _Bool top_session_update;
    _Atomic int top_session_guard;
    struct EjTopSession *_Atomic top_session;

    // top-level contest info
    _Atomic _Bool contests_update;
    _Atomic int contests_guard;
    struct EjContestList *_Atomic contests;

    struct EjInodeHash *inode_hash;

    struct EjContestsState *contests_state;

    struct EjFileNodes *file_nodes;

    struct EjSubmitThread *submit_thread;
};

struct EjFuseRequest
{
    long long current_time_us;

    const struct EjFuseOperations *ops;
    struct EjFuseState *efs;
    struct fuse_context *fx;

    int contest_id;
    struct EjContestState *ecs;
    const unsigned char *file_name;
    int file_name_code;
    int prob_id;
    struct EjProblemState *eps;
    int lang_id;
    int run_id;
    struct EjRunState *ers;
};

struct EjFuseRequest;

// replica of the corresponding fuse_operations, the first parameter is request structure
struct EjFuseOperations
{
    int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
    int (*readlink)(struct EjFuseRequest *, const char *, char *, size_t);
    int (*mknod)(struct EjFuseRequest *, const char *, mode_t, dev_t);
    int (*mkdir)(struct EjFuseRequest *, const char *, mode_t);
    int (*unlink)(struct EjFuseRequest *, const char *);
    int (*rmdir)(struct EjFuseRequest *, const char *);
    int (*symlink)(struct EjFuseRequest *, const char *, const char *);
    int (*rename)(struct EjFuseRequest *, const char *, const char *);
    int (*link)(struct EjFuseRequest *, const char *, const char *);
    int (*chmod)(struct EjFuseRequest *, const char *, mode_t);
    int (*chown)(struct EjFuseRequest *, const char *, uid_t, gid_t);
    int (*truncate)(struct EjFuseRequest *, const char *, off_t);
    int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    int (*release)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    int (*fsync)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    int (*setxattr)(struct EjFuseRequest *, const char *, const char *, const char *, size_t, int);
    int (*getxattr)(struct EjFuseRequest *, const char *, const char *, char *, size_t);
    int (*listxattr)(struct EjFuseRequest *, const char *, char *, size_t);
    int (*removexattr)(struct EjFuseRequest *, const char *, const char *);
    int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    int (*fsyncdir)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    int (*access)(struct EjFuseRequest *, const char *, int);
    int (*create)(struct EjFuseRequest *, const char *, mode_t, struct fuse_file_info *);
    int (*ftruncate)(struct EjFuseRequest *, const char *, off_t, struct fuse_file_info *);
    int (*fgetattr)(struct EjFuseRequest *, const char *, struct stat *, struct fuse_file_info *);
    int (*lock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int cmd, struct flock *);
    int (*utimens)(struct EjFuseRequest *, const char *, const struct timespec tv[2]);
    int (*bmap)(struct EjFuseRequest *, const char *, size_t blocksize, uint64_t *idx);
    int (*ioctl)(struct EjFuseRequest *, const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    int (*poll)(struct EjFuseRequest *, const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
    int (*write_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
    int (*read_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
    int (*flock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int op);
    int (*fallocate)(struct EjFuseRequest *, const char *, int, off_t, off_t, struct fuse_file_info *);
};

int ejf_process_path(const char *path, struct EjFuseRequest *rq);

int request_free(struct EjFuseRequest *rq, int retval);

unsigned get_inode(struct EjFuseState *efs, const char *path);

struct EjContestList *contest_list_read_lock(struct EjFuseState *efs);
void contest_list_read_unlock(struct EjContestList *contests);

struct EjContestListItem *contest_list_find(const struct EjContestList *contests, int cnts_id);

struct EjContestState;
struct EjFuseState;
struct EjProblemState;
struct EjRunState;

void
contest_session_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us);
void
contest_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        long long current_time_us);
void
problem_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us);
void
problem_statement_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us);
void
problem_runs_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjProblemState *eps,
        long long current_time_us);
void
run_info_maybe_update(
        struct EjFuseState *efs,
        struct EjContestState *ecs,
        struct EjRunState *ers,
        long long current_time_us);

/* special file names */
enum
{
    FILE_NAME_INFO = 1,
    FILE_NAME_INFO_JSON = 2,
    FILE_NAME_STATEMENT_HTML = 3
};

struct EjContestInfo;
struct EjProblemInfo;
struct EjRunInfo;
void ejfuse_contest_info_text(struct EjContestInfo *eci);
void ejfuse_problem_info_text(struct EjProblemInfo *epi);
void ejfuse_run_info_text(struct EjRunInfo *eri);
