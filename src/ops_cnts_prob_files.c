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

#include "ops_cnts_prob_files.h"

#include "ejfuse.h"
#include "ops_generic.h"
#include "contests_state.h"

#include <string.h>
#include <errno.h>
#include <limits.h>

static int
ejf_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *ejs = efr->ejs;
    off_t file_size = 0;

    if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO) || !strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO_JSON)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        file_size = epi->info_json_size;
        problem_info_read_unlock(epi);
    } else if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_STATEMENT_HTML)) {
        // try to not request statement.html from server
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        if (!epi->is_viewable || !epi->is_statement_avaiable) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        file_size = epi->est_stmt_size;
        problem_info_read_unlock(epi);

        struct EjProblemStatement *eph = problem_statement_read_lock(efr->eps);
        if (eph && eph->ok) {
            file_size = eph->stmt_size;
        }
        problem_statement_read_unlock(eph);
    } else {
        return -ENOENT;
    }

    unsigned char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "/%d/problems/%d/%s", efr->contest_id, efr->prob_id, efr->file_name);

    memset(stb, 0, sizeof(*stb));

    snprintf(fullpath, sizeof(fullpath), "/%d/problems", efr->ecs->cnts_id);
    stb->st_ino = get_inode(ejs, fullpath);
    stb->st_mode = S_IFREG | EJFUSE_FILE_PERMS;
    stb->st_nlink = 2;
    stb->st_uid = ejs->owner_uid;
    stb->st_gid = ejs->owner_gid;
    stb->st_size = file_size;
    long long current_time_us = efr->current_time_us;
    stb->st_atim.tv_sec = current_time_us / 1000000;
    stb->st_atim.tv_nsec = (current_time_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = ejs->start_time_us / 1000000;
    stb->st_mtim.tv_nsec = (ejs->start_time_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = ejs->start_time_us / 1000000;
    stb->st_ctim.tv_nsec = (ejs->start_time_us % 1000000) * 1000;

    return 0;
}

static int
ejf_access(struct EjFuseRequest *efr, const char *path, int mode)
{
    int perms = EJFUSE_FILE_PERMS;
    mode &= 07;

    if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO) || !strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO_JSON)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        problem_info_read_unlock(epi);
    } else if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_STATEMENT_HTML)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok || !epi->is_viewable || !epi->is_statement_avaiable) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        problem_info_read_unlock(epi);
        struct EjProblemStatement *eph = problem_statement_read_lock(efr->eps);
        if (!eph || !eph->ok) {
            problem_statement_read_unlock(eph);
            return -ENOENT;
        }
        problem_statement_read_unlock(eph);
    } else {
        return -ENOENT;
    }
    if (efr->ejs->owner_uid == efr->fx->uid) {
        perms >>= 6;
    } else if (efr->ejs->owner_gid == efr->fx->gid) {
        perms >>= 3;
    } else {
        // nothing
    }
    if (((perms & 07) & mode) == mode) {
        return 0;
    } else {
        return -EPERM;
    }
}

static int
ejf_release(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return 0;
}

static int
ejf_open(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO) || !strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO_JSON)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        problem_info_read_unlock(epi);
    } else if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_STATEMENT_HTML)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok || !epi->is_viewable || !epi->is_statement_avaiable) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
        problem_info_read_unlock(epi);
        problem_statement_maybe_update(efr->ejs, efr->ecs, efr->eps, efr->current_time_us);
        struct EjProblemStatement *eph = problem_statement_read_lock(efr->eps);
        if (!eph || !eph->ok) {
            problem_statement_read_unlock(eph);
            return -ENOENT;
        }
        problem_statement_read_unlock(eph);
    } else {
        return -ENOENT;
    }
    if (efr->ejs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }
    if ((ffi->flags & O_ACCMODE) != O_RDONLY) {
        return -EPERM;
    }
    return 0;
}

static int
ejf_read(struct EjFuseRequest *efr, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    int retval = -EIO;
    if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO) || !strcmp(efr->file_name, FN_CONTEST_PROBLEM_INFO_JSON)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok) {
            problem_info_read_unlock(epi);
            return -EIO;
        }
        retval = 0;
        size_t len = epi->info_json_size;
        if (!size || offset < 0 || offset >= len) {
            retval = 0;
        } else {
            if (len - offset < size) {
                size = len - offset;
            }
            memcpy(buf, epi->info_json_text + offset, size);
            retval = size;
        }
        problem_info_read_unlock(epi);
    } else if (!strcmp(efr->file_name, FN_CONTEST_PROBLEM_STATEMENT_HTML)) {
        struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
        if (!epi || !epi->ok || !epi->is_viewable || !epi->is_statement_avaiable) {
            problem_info_read_unlock(epi);
            return -EIO;
        }
        problem_info_read_unlock(epi);
        problem_statement_maybe_update(efr->ejs, efr->ecs, efr->eps, efr->current_time_us);
        struct EjProblemStatement *eph = problem_statement_read_lock(efr->eps);
        if (!eph || !eph->ok) {
            problem_statement_read_unlock(eph);
            return -EIO;
        }
        size_t len = eph->stmt_size;
        if (!size || offset < 0 || offset >= len) {
            retval = 0;
        } else {
            if (len - offset < size) {
                size = len - offset;
            }
            memcpy(buf, eph->stmt_text + offset, size);
            retval = size;
        }
        problem_statement_read_unlock(eph);
    } else {
        return -EIO;
    }
    return retval;
}

// generic operations
const struct EjFuseOperations ejfuse_contest_problem_files_operations =
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
    ejf_open, //int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_read, //int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_write, //int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_statfs, //int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    ejf_generic_flush, //int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_release, //int (*release)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsync, //int (*fsync)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_generic_setxattr, //int (*setxattr)(struct EjFuseRequest *, const char *, const char *, const char *, size_t, int);
    ejf_generic_getxattr, //int (*getxattr)(struct EjFuseRequest *, const char *, const char *, char *, size_t);
    ejf_generic_listxattr, //int (*listxattr)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_generic_removexattr, //int (*removexattr)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_opendir, //int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_readdir, //int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    ejf_generic_releasedir, //int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
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
