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

#include "ops_cnts_prob_dir.h"

#include "settings.h"
#include "ejfuse.h"
#include "ops_generic.h"
#include "contests_state.h"

#include <limits.h>
#include <errno.h>
#include <string.h>

static int
ejf_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *efs = efr->efs;
    unsigned char fullpath[PATH_MAX];
    struct EjContestInfo *eci = NULL;

    eci = contest_info_read_lock(efr->ecs);
    if (!eci || !eci->ok || efr->prob_id <= 0 || efr->prob_id >= eci->prob_size || !eci->probs[efr->prob_id]) {
        contest_info_read_unlock(eci);
        return -ENOENT;
    }

    memset(stb, 0, sizeof(*stb));
    snprintf(fullpath, sizeof(fullpath), "/%d/problems/%d", efr->contest_id, efr->prob_id);
    stb->st_ino = get_inode(efs, fullpath);
    stb->st_mode = S_IFDIR | EJFUSE_DIR_PERMS;
    stb->st_nlink = 2;
    stb->st_uid = efs->owner_uid;
    stb->st_gid = efs->owner_gid;
    stb->st_size = EJFUSE_DIR_SIZE;
    long long current_time_us = efr->current_time_us;
    stb->st_atim.tv_sec = current_time_us / 1000000;
    stb->st_atim.tv_nsec = (current_time_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = efs->start_time_us / 1000000;
    stb->st_mtim.tv_nsec = (efs->start_time_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = efs->start_time_us / 1000000;
    stb->st_ctim.tv_nsec = (efs->start_time_us % 1000000) * 1000;

    contest_info_read_unlock(eci);
    return 0;
}

static int
ejf_access(struct EjFuseRequest *efr, const char *path, int mode)
{
    struct EjFuseState *efs = efr->efs;
    int retval = -ENOENT;
    int perms = EJFUSE_DIR_PERMS;
    mode &= 07;

    struct EjContestInfo *eci = contest_info_read_lock(efr->ecs);
    if (!eci || !eci->ok) {
        contest_info_read_unlock(eci);
        goto done;
    }
    contest_info_read_unlock(eci);

    if (efs->owner_uid == efr->fx->uid) {
        perms >>= 6;
    } else if (efs->owner_gid == efr->fx->gid) {
        perms >>= 3;
    } else {
        // nothing
    }
    if (((perms & 07) & mode) == mode) {
        retval = 0;
    } else {
        retval = -EPERM;
    }

done:
    return retval;
}

static int
ejf_opendir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    problem_info_maybe_update(efr->efs, efr->ecs, efr->eps, efr->current_time_us);

    struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
    if (!epi || !epi->ok) {
        problem_info_read_unlock(epi);
        return -ENOENT;
    }
    problem_info_read_unlock(epi);
    if (efr->efs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }
    // no op, actual work is done by readdir
    return 0;
}

static int
ejf_releasedir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return 0;
}

static int
ejf_readdir(
        struct EjFuseRequest *efr,
        const char *path,
        void *buf,
        fuse_fill_dir_t filler,
        off_t offset,
        struct fuse_file_info *ffi)
{
    struct EjFuseState *efs = efr->efs;
    problem_info_maybe_update(efr->efs, efr->ecs, efr->eps, efr->current_time_us);

    struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
    if (!epi || !epi->ok) {
        problem_info_read_unlock(epi);
        return -EIO;
    }

    unsigned char p_path[PATH_MAX];
    snprintf(p_path, sizeof(p_path), "/%d/problems/%d", efr->contest_id, efr->prob_id);
    struct stat es;
    memset(&es, 0, sizeof(es));
    es.st_ino = get_inode(efs, p_path);
    filler(buf, ".", &es, 0);

    unsigned char up_path[PATH_MAX];
    snprintf(up_path, sizeof(up_path), "/%d/problems", efr->contest_id);
    es.st_ino = get_inode(efs, up_path);
    filler(buf, "..", &es, 0);

    unsigned char entry_path[PATH_MAX];
    int res = snprintf(entry_path, sizeof(entry_path), "%s/%s", p_path, "INFO");
    if (res >= sizeof(entry_path)) { abort(); }
    es.st_ino = get_inode(efs, entry_path);
    filler(buf, "INFO", &es, 0);
    res = snprintf(entry_path, sizeof(entry_path), "%s/%s", p_path, "info.json");
    if (res >= sizeof(entry_path)) { abort(); }
    es.st_ino = get_inode(efs, entry_path);
    filler(buf, "info.json", &es, 0);
    if (epi->is_viewable && epi->is_statement_avaiable) {
        res = snprintf(entry_path, sizeof(entry_path), "%s/%s", p_path, "statement.html");
        if (res >= sizeof(entry_path)) { abort(); }
        es.st_ino = get_inode(efs, entry_path);
        filler(buf, "statement.html", &es, 0);
    }
    res = snprintf(entry_path, sizeof(entry_path), "%s/%s", p_path, "runs");
    if (res >= sizeof(entry_path)) { abort(); }
    es.st_ino = get_inode(efs, entry_path);
    filler(buf, "runs", &es, 0);
    if (epi->is_submittable) {
        res = snprintf(entry_path, sizeof(entry_path), "%s/%s", p_path, "submit");
        if (res >= sizeof(entry_path)) { abort(); }
        es.st_ino = get_inode(efs, entry_path);
        filler(buf, "submit", &es, 0);
    }

    problem_info_read_unlock(epi);
    return 0;
}

const struct EjFuseOperations ejfuse_contest_problem_operations =
{
    ejf_getattr, //int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
    ejf_generic_readlink, //int (*readlink)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_generic_mknod, //int (*mknod)(struct EjFuseRequest *, const char *, mode_t, dev_t);
    ejf_generic_mkdir, //int (*mkdir)(struct EjFuseRequest *, const char *, mode_t);
    ejf_generic_unlink, //int (*unlink)(struct EjFuseRequest *, const char *);
    ejf_generic_rmdir, //int (*rmdir)(struct EjFuseRequest *, const char *);
    ejf_generic_symlink, //int (*symlink)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_rename, //int (*rename)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_link, //int (*link)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_chmod, //int (*chmod)(struct EjFuseRequest *, const char *, mode_t);
    ejf_generic_chown, //int (*chown)(struct EjFuseRequest *, const char *, uid_t, gid_t);
    ejf_generic_truncate, //int (*truncate)(struct EjFuseRequest *, const char *, off_t);
    ejf_generic_open, //int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_read, //int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_write, //int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_statfs, //int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    ejf_generic_flush, //int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_release, //int (*release)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsync, //int (*fsync)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_generic_setxattr, //int (*setxattr)(struct EjFuseRequest *, const char *, const char *, const char *, size_t, int);
    ejf_generic_getxattr, //int (*getxattr)(struct EjFuseRequest *, const char *, const char *, char *, size_t);
    ejf_generic_listxattr, //int (*listxattr)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_generic_removexattr, //int (*removexattr)(struct EjFuseRequest *, const char *, const char *);
    ejf_opendir, //int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_readdir, //int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    ejf_releasedir, //int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsyncdir, //int (*fsyncdir)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_access, //int (*access)(struct EjFuseRequest *, const char *, int);
    ejf_generic_create, //int (*create)(struct EjFuseRequest *, const char *, mode_t, struct fuse_file_info *);
    ejf_generic_ftruncate, //int (*ftruncate)(struct EjFuseRequest *, const char *, off_t, struct fuse_file_info *);
    ejf_generic_fgetattr, //int (*fgetattr)(struct EjFuseRequest *, const char *, struct stat *, struct fuse_file_info *);
    ejf_generic_lock, //int (*lock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int cmd, struct flock *);
    ejf_generic_utimens, //int (*utimens)(struct EjFuseRequest *, const char *, const struct timespec tv[2]);
    ejf_generic_bmap, //int (*bmap)(struct EjFuseRequest *, const char *, size_t blocksize, uint64_t *idx);
    ejf_generic_ioctl, //int (*ioctl)(struct EjFuseRequest *, const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    ejf_generic_poll, //int (*poll)(struct EjFuseRequest *, const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
    ejf_generic_write_buf, //int (*write_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
    ejf_generic_read_buf, //int (*read_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
    ejf_generic_flock, //int (*flock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int op);
    ejf_generic_fallocate, //int (*fallocate)(struct EjFuseRequest *, const char *, int, off_t, off_t, struct fuse_file_info *);
};
