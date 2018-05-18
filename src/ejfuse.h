#pragma once

#include <stdio.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

enum { EJFUSE_DIR_PERMS = 0500 };
enum { EJFUSE_FILE_PERMS = 0400 };

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

    int count;
    struct EjContestListItem *entries;
};

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
    _Atomic long long current_time_us;

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
};

struct EjFuseRequest
{
    const struct EjFuseOperations *ops;
    struct EjFuseState *ejs;
    struct fuse_context *fx;

    int contest_id;
    struct EjContestState *ecs;
    const unsigned char *file_name;
    int prob_id;
    struct EjProblemState *eps;
    int lang_id;
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

unsigned get_inode(struct EjFuseState *ejs, const char *path);

struct EjContestList *contest_list_read_lock(struct EjFuseState *ejs);
void contest_list_read_unlock(struct EjContestList *contests);

struct EjContestListItem *contest_list_find(const struct EjContestList *contests, int cnts_id);

struct EjFuseState;
struct EjContestState;
struct EjProblemState;
void problem_info_maybe_update(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemState *eps);
void
problem_statement_maybe_update(
        struct EjFuseState *ejs,
        struct EjContestState *ecs,
        struct EjProblemState *eps);

// directory structure
#define FN_CONTEST_PROBLEM_INFO           "INFO"
#define FN_CONTEST_PROBLEM_INFO_JSON      "info.json"
#define FN_CONTEST_PROBLEM_STATEMENT_HTML "statement.html"
