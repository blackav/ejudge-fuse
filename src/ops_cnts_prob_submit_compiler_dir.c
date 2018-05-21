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

#include "ops_cnts_prob_submit_compiler_dir.h"

#include "ejfuse.h"
#include "ops_generic.h"
#include "contests_state.h"
#include "ejfuse_file.h"

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>

static int
check_perms(struct EjFuseRequest *efr, int file_perms, int req_perms)
{
    int retval = -EACCES;
    req_perms &= 7;

    if (efr->ejs->owner_uid == efr->fx->uid) {
        file_perms >>= 6;
    } else if (efr->ejs->owner_gid == efr->fx->gid) {
        file_perms >>= 3;
    } else {
        // nothing
    }
    if (((file_perms & 07) & req_perms) == req_perms) {
        retval = 0;
    }
    return retval;
}

static int
check_lang(struct EjFuseRequest *efr)
{
    struct EjProblemInfo *epi = problem_info_read_lock(efr->eps);
    if (!epi || !epi->ok || !epi->is_submittable) {
        problem_info_read_unlock(epi);
        return -ENOENT;
    }

    if (efr->lang_id <= 0) {
        problem_info_read_unlock(epi);
        return -ENOENT;
    }
    if (epi->compiler_size && epi->compilers) {
        if (efr->lang_id >= epi->compiler_size || !epi->compilers[efr->lang_id]) {
            problem_info_read_unlock(epi);
            return -ENOENT;
        }
    }
    problem_info_read_unlock(epi);
    return 0;
}

static int
ejf_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *ejs = efr->ejs;
    unsigned char fullpath[PATH_MAX];
    size_t name_len = strlen(efr->file_name);

    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    if (efr->ejs->owner_uid != efr->fx->uid) return -EPERM;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) {
        return -ENOENT;
    }

    struct EjDirectoryNode dn;
    res = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_rdlock(&efn->rwl);

    memset(stb, 0, sizeof(*stb));
    snprintf(fullpath, sizeof(fullpath), "/fnode/%d", dn.fnode);
    stb->st_ino = get_inode(ejs, fullpath);
    stb->st_mode = S_IFREG | (efn->mode & 07777);
    stb->st_nlink = efn->nlink;
    stb->st_uid = ejs->owner_uid;
    stb->st_gid = ejs->owner_gid;
    stb->st_size = efn->size;
    stb->st_atim.tv_sec = efn->atime_us / 1000000;
    stb->st_atim.tv_nsec = (efn->atime_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = efn->mtime_us / 1000000;
    stb->st_mtim.tv_nsec = (efn->mtime_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = efn->ctime_us / 1000000;
    stb->st_ctim.tv_nsec = (efn->ctime_us % 1000000) * 1000;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return 0;
}

static int
ejf_fgetattr(struct EjFuseRequest *efr, const char *path, struct stat *stb, struct fuse_file_info *ffi)
{
    struct EjFuseState *ejs = efr->ejs;
    unsigned char fullpath[PATH_MAX];

    struct EjFileNode *efn = file_nodes_get_node(ejs->file_nodes, ffi->fh);
    if (!efn) return -ENOENT;
    pthread_rwlock_rdlock(&efn->rwl);

    memset(stb, 0, sizeof(*stb));
    snprintf(fullpath, sizeof(fullpath), "/fnode/%d", (int) ffi->fh);
    stb->st_ino = get_inode(ejs, fullpath);
    stb->st_mode = S_IFREG | (efn->mode & 07777);
    stb->st_nlink = efn->nlink;
    stb->st_uid = ejs->owner_uid;
    stb->st_gid = ejs->owner_gid;
    stb->st_size = efn->size;
    stb->st_atim.tv_sec = efn->atime_us / 1000000;
    stb->st_atim.tv_nsec = (efn->atime_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = efn->mtime_us / 1000000;
    stb->st_mtim.tv_nsec = (efn->mtime_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = efn->ctime_us / 1000000;
    stb->st_ctim.tv_nsec = (efn->ctime_us % 1000000) * 1000;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return 0;
}

static int
ejf_mknod(struct EjFuseRequest *efr, const char *path, mode_t mode, dev_t dev)
{
    // only regular files are supported
    int what = mode & S_IFMT;
    if (what && what != S_IFREG) return -EINVAL;
    mode &= 07777;
    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    if (efr->ejs->owner_uid != efr->fx->uid) return -EPERM;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_open_node(epcs->dir_nodes, efr->ejs->file_nodes, efr->file_name, name_len, 1, 1, mode, efr->ejs->current_time_us, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return 0;
}

static int
ejf_access(struct EjFuseRequest *efr, const char *path, int mode)
{
    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);
    int perms = efn->mode;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);

    return check_perms(efr, perms, mode);
}

static int
ejf_open(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    int open_mode = (ffi->flags & O_ACCMODE);
    int req_bits = 0;
    if (open_mode == O_RDONLY) {
        req_bits = 4;
    } else if (open_mode == O_WRONLY) {
        req_bits = 2;
    } else if (open_mode == O_RDWR) {
        req_bits = 6;
    } else {
        return -EINVAL;
    }

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);  // because of atime

    if ((res = check_perms(efr, efn->mode, req_bits)) < 0) {
        pthread_rwlock_unlock(&efn->rwl);
        atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
        return res;
    }

    if ((open_mode == O_WRONLY || open_mode == O_RDWR) && (ffi->flags & O_TRUNC)) {
        // truncate output
        efn->size = 0;
        efn->mtime_us = efr->ejs->current_time_us;
    }

    efn->atime_us = efr->ejs->current_time_us;
    atomic_fetch_add_explicit(&efn->opencnt, 1, memory_order_relaxed);
    ffi->fh = dn.fnode;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);

    return 0;
}

static int
ejf_create(struct EjFuseRequest *efr, const char *path, mode_t mode, struct fuse_file_info *ffi)
{
    int what = mode & S_IFMT;
    if (what && what != S_IFREG) return -EINVAL;
    mode &= 0777;

    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    if (efr->ejs->owner_uid != efr->fx->uid) return -EPERM;

    int open_mode = (ffi->flags & O_ACCMODE);
    int req_bits = 0;
    if (open_mode == O_RDONLY) {
        req_bits = 4;
    } else if (open_mode == O_WRONLY) {
        req_bits = 2;
    } else if (open_mode == O_RDWR) {
        req_bits = 6;
    } else {
        return -EINVAL;
    }

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_open_node(epcs->dir_nodes, efr->ejs->file_nodes, efr->file_name, name_len, 1, 0, mode, efr->ejs->current_time_us, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    if ((res = check_perms(efr, efn->mode, req_bits)) < 0) {
        pthread_rwlock_unlock(&efn->rwl);
        atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
        return res;
    }

    if ((open_mode == O_WRONLY || open_mode == O_RDWR) && (ffi->flags & O_TRUNC)) {
        // truncate output
        efn->size = 0;
        efn->mtime_us = efr->ejs->current_time_us;
    }

    efn->atime_us = efr->ejs->current_time_us;
    atomic_fetch_add_explicit(&efn->opencnt, 1, memory_order_relaxed);
    ffi->fh = dn.fnode;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);

    return 0;
}

static int
ejf_flush(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return 0;
}

static int
ejf_truncate(struct EjFuseRequest *efr, const char *path, off_t offset)
{
    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    if ((res = check_perms(efr, efn->mode, 2)) < 0) goto out;
    if ((res = file_node_truncate_unlocked(efr->ejs->file_nodes, efn, offset)) < 0) goto out;

    efn->mtime_us = efr->ejs->current_time_us;
    res = 0;

out:
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return res;
}

static int
ejf_ftruncate(struct EjFuseRequest *efr, const char *path, off_t offset, struct fuse_file_info *ffi)
{
    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, ffi->fh);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    if ((res = check_perms(efr, efn->mode, 2)) < 0) goto out;
    if ((res = file_node_truncate_unlocked(efr->ejs->file_nodes, efn, offset)) < 0) goto out;

    efn->mtime_us = efr->ejs->current_time_us;
    res = 0;

out:
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return res;
}

static int
ejf_unlink(struct EjFuseRequest *efr, const char *path)
{
    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_unlink_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    atomic_fetch_sub_explicit(&efn->nlink, 1, memory_order_relaxed);
    file_nodes_maybe_remove(efr->ejs->file_nodes, efn, efr->ejs->current_time_us);

    return 0;
}

static int
ejf_read(
        struct EjFuseRequest *efr,
        const char *path,
        char *buf,
        size_t size,
        off_t offset,
        struct fuse_file_info *ffi)
{
    int res = check_lang(efr);
    if (res < 0) return res;

    int open_mode = (ffi->flags & O_ACCMODE);
    if (open_mode != O_RDONLY && open_mode != O_RDWR) return -EBADF;

    if (offset < 0) return -EINVAL;
    int ioff = offset;
    if (ioff != offset) return -EINVAL;
    int isize = size;
    if (isize != size) return -EINVAL;
    if (isize < 0) return -EINVAL;
    if (!isize) return 0;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, ffi->fh);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl); // for atime

    res = 0;
    if (ioff >= efn->size) goto out;
    if (efn->size - ioff < isize) isize = efn->size - ioff;
    if (isize <= 0) goto out;
    memcpy(buf, efn->data + ioff, isize);
    res = isize;

out:
    efn->atime_us = efr->ejs->current_time_us;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return res;
}

static int
ejf_write(
        struct EjFuseRequest *efr,
        const char *path,
        const char *buf,
        size_t size,
        off_t offset,
        struct fuse_file_info *ffi)
{
    int res = check_lang(efr);
    if (res < 0) return res;

    int open_mode = (ffi->flags & O_ACCMODE);
    if (open_mode != O_WRONLY && open_mode != O_RDWR) return -EBADF;

    if (offset < 0) return -EINVAL;
    int ioff = offset;
    if (ioff != offset) return -EINVAL;
    int isize = size;
    if (isize != size) return -EINVAL;
    if (isize < 0) return -EINVAL;
    if (!isize) return 0;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, ffi->fh);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    int new_size;
    if (__builtin_add_overflow(ioff, isize, &new_size)) {
        res = -EIO;
        goto out;
    }
    if (new_size > efn->size) {
        if ((res = file_node_truncate_unlocked(efr->ejs->file_nodes, efn, new_size)) < 0)
            goto out;
    }

    memcpy(efn->data + ioff, buf, isize);
    res = isize;

out:
    efn->mtime_us = efr->ejs->current_time_us;
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return res;
}

static int
ejf_release(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, ffi->fh);
    if (!efn) return -ENOENT;

    pthread_rwlock_rdlock(&efn->rwl);
    // send the submit!
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->opencnt, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return 0;
}

static int
ejf_utimens(struct EjFuseRequest *efr, const char *path, const struct timespec tv[2])
{
    long long atime_us = tv[0].tv_sec;
    long long mtime_us = tv[1].tv_sec;

    if (__builtin_mul_overflow(atime_us, 1000000LL, &atime_us)
        || __builtin_add_overflow(atime_us, tv[0].tv_nsec / 1000LL, &atime_us)
        || __builtin_mul_overflow(mtime_us, 1000000LL, &mtime_us)
        || __builtin_add_overflow(mtime_us, tv[1].tv_nsec / 1000LL, &mtime_us)) {
        return -EINVAL;
    }

    size_t name_len = strlen(efr->file_name);
    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    int res = check_lang(efr);
    if (res < 0) return res;

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) return -ENOENT;

    struct EjDirectoryNode dn;
    res = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (res < 0) return res;

    struct EjFileNode *efn = file_nodes_get_node(efr->ejs->file_nodes, dn.fnode);
    if (!efn) return -ENOENT;

    pthread_rwlock_wrlock(&efn->rwl);

    if ((res = check_perms(efr, efn->mode, 2)) < 0) goto out;

    efn->atime_us = atime_us;
    efn->mtime_us = mtime_us;
    res = 0;

out:
    pthread_rwlock_unlock(&efn->rwl);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    return res;
}

const struct EjFuseOperations ejfuse_contest_problem_submit_compiler_dir_operations =
{
    ejf_getattr, //int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
    ejf_generic_readlink, //int (*readlink)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_mknod, //int (*mknod)(struct EjFuseRequest *, const char *, mode_t, dev_t);
    ejf_generic_mkdir, //int (*mkdir)(struct EjFuseRequest *, const char *, mode_t);
    ejf_unlink, //int (*unlink)(struct EjFuseRequest *, const char *);
    ejf_generic_rmdir, //int (*rmdir)(struct EjFuseRequest *, const char *);
    ejf_generic_symlink, //int (*symlink)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_rename, //int (*rename)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_link, //int (*link)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_chmod, //int (*chmod)(struct EjFuseRequest *, const char *, mode_t);
    ejf_generic_chown, //int (*chown)(struct EjFuseRequest *, const char *, uid_t, gid_t);
    ejf_truncate, //int (*truncate)(struct EjFuseRequest *, const char *, off_t);
    ejf_open, //int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_read, //int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    ejf_write, //int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_statfs, //int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    ejf_flush, //int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
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
    ejf_create, //int (*create)(struct EjFuseRequest *, const char *, mode_t, struct fuse_file_info *);
    ejf_ftruncate, //int (*ftruncate)(struct EjFuseRequest *, const char *, off_t, struct fuse_file_info *);
    ejf_fgetattr, //int (*fgetattr)(struct EjFuseRequest *, const char *, struct stat *, struct fuse_file_info *);
    ejf_generic_lock, //int (*lock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int cmd, struct flock *);
    ejf_utimens, //int (*utimens)(struct EjFuseRequest *, const char *, const struct timespec tv[2]);
    ejf_generic_bmap, //int (*bmap)(struct EjFuseRequest *, const char *, size_t blocksize, uint64_t *idx);
    ejf_generic_ioctl, //int (*ioctl)(struct EjFuseRequest *, const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    ejf_generic_poll, //int (*poll)(struct EjFuseRequest *, const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
    ejf_generic_write_buf, //int (*write_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
    ejf_generic_read_buf, //int (*read_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
    ejf_generic_flock, //int (*flock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int op);
    ejf_generic_fallocate, //int (*fallocate)(struct EjFuseRequest *, const char *, int, off_t, off_t, struct fuse_file_info *);
};
