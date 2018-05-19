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

static int
ejf_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *ejs = efr->ejs;
    unsigned char fullpath[PATH_MAX];
    struct EjProblemInfo *epi = NULL;
    size_t name_len = strlen(efr->file_name);

    if (name_len > NAME_MAX) return -ENAMETOOLONG;

    epi = problem_info_read_lock(efr->eps);
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

    struct EjProblemCompilerSubmits *epcs = problem_submits_get(efr->eps->submits, efr->lang_id);
    if (!epcs) {
        problem_info_read_unlock(epi);
        return -ENOENT;
    }

    struct EjDirectoryNode dn;
    int ret = dir_nodes_get_node(epcs->dir_nodes, efr->file_name, name_len, &dn);
    if (ret < 0) {
        problem_info_read_unlock(epi);
        return ret;
    }

    struct EjFileNode *efn = file_nodes_get_node(ejs->file_nodes, dn.fnode);
    if (!efn) {
        problem_info_read_unlock(epi);
        return ret;
    }
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
    problem_info_read_unlock(epi);
    return 0;
}

const struct EjFuseOperations ejfuse_contest_problem_submit_compiler_dir_operations =
{
    ejf_getattr, //int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
    ejf_generic_readlink, //int (*readlink)(struct EjFuseRequest *, const char *, char *, size_t);
    NULL, //int (*mknod)(struct EjFuseRequest *, const char *, mode_t, dev_t);
    ejf_generic_mkdir, //int (*mkdir)(struct EjFuseRequest *, const char *, mode_t);
    NULL, //int (*unlink)(struct EjFuseRequest *, const char *);
    ejf_generic_rmdir, //int (*rmdir)(struct EjFuseRequest *, const char *);
    ejf_generic_symlink, //int (*symlink)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_rename, //int (*rename)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_link, //int (*link)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_chmod, //int (*chmod)(struct EjFuseRequest *, const char *, mode_t);
    ejf_generic_chown, //int (*chown)(struct EjFuseRequest *, const char *, uid_t, gid_t);
    NULL, //int (*truncate)(struct EjFuseRequest *, const char *, off_t);
    NULL, //int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    NULL, //int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    NULL, //int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_statfs, //int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    NULL, //int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    NULL, //int (*release)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsync, //int (*fsync)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_generic_setxattr, //int (*setxattr)(struct EjFuseRequest *, const char *, const char *, const char *, size_t, int);
    ejf_generic_getxattr, //int (*getxattr)(struct EjFuseRequest *, const char *, const char *, char *, size_t);
    ejf_generic_listxattr, //int (*listxattr)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_generic_removexattr, //int (*removexattr)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_opendir, //int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_readdir, //int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    ejf_generic_releasedir, //int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsyncdir, //int (*fsyncdir)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    NULL, //int (*access)(struct EjFuseRequest *, const char *, int);
    NULL, //int (*create)(struct EjFuseRequest *, const char *, mode_t, struct fuse_file_info *);
    NULL, //int (*ftruncate)(struct EjFuseRequest *, const char *, off_t, struct fuse_file_info *);
    NULL, //int (*fgetattr)(struct EjFuseRequest *, const char *, struct stat *, struct fuse_file_info *);
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
