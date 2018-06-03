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

#include "ops_cnts_prob_runs_run_tests_test_files.h"

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

    if (efs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }

    unsigned char fullpath[PATH_MAX];
    if (snprintf(fullpath, sizeof(fullpath), "/%d/problems/%d/runs/%d/tests/%d/%s",
                 efr->contest_id, efr->prob_id, efr->run_id, efr->num, efr->file_name) >= sizeof(fullpath)) {
        return -ENAMETOOLONG;
    }

    struct EjRunInfo *eri = run_info_read_lock(efr->ers);
    struct EjRunInfoTestResult *eritr = run_info_get_test_result_unlocked(eri, efr->num);
    struct EjRunInfoTestResultData *eritrd = &eritr->data[efr->test_file_index];
    long long mtime_us = 0;
    mtime_us = eri->run_time_us;
    if (!eritrd->is_defined) {
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    int perms = 0;
    if (!eritrd->is_too_big) {
        perms = EJFUSE_FILE_PERMS;
    }
    off_t file_size = eritrd->size;
    struct EjRunTestData *ertd = run_test_data_read_lock(efr->ert, efr->test_file_index);
    if (ertd && !ertd->ok) {
        run_test_data_read_unlock(ertd);
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    if (ertd) {
        file_size = ertd->size;
        run_test_data_read_unlock(ertd);
    }
    run_info_read_unlock(eri);

    memset(stb, 0, sizeof(*stb));

    stb->st_ino = get_inode(efs, fullpath);
    stb->st_mode = S_IFREG | perms;
    stb->st_nlink = 1;
    stb->st_uid = efs->owner_uid;
    stb->st_gid = efs->owner_gid;
    stb->st_size = file_size;
    long long current_time_us = efr->current_time_us;
    if (mtime_us <= 0) mtime_us = efs->start_time_us;
    stb->st_atim.tv_sec = current_time_us / 1000000;
    stb->st_atim.tv_nsec = (current_time_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = mtime_us / 1000000;
    stb->st_mtim.tv_nsec = (mtime_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = efs->start_time_us / 1000000;
    stb->st_ctim.tv_nsec = (efs->start_time_us % 1000000) * 1000;

    return 0;
}

static int
ejf_access(struct EjFuseRequest *efr, const char *path, int mode)
{
    int perms = 0;
    mode &= 07;

    struct EjRunInfo *eri = run_info_read_lock(efr->ers);
    struct EjRunInfoTestResult *eritr = run_info_get_test_result_unlocked(eri, efr->num);
    struct EjRunInfoTestResultData *eritrd = &eritr->data[efr->test_file_index];
    if (!eritrd->is_defined) {
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    if (!eritrd->is_too_big) {
        perms = EJFUSE_FILE_PERMS;
    }
    struct EjRunTestData *ertd = run_test_data_read_lock(efr->ert, efr->test_file_index);
    if (ertd && !ertd->ok) {
        run_test_data_read_unlock(ertd);
        run_info_read_unlock(eri);
        return -ENOENT;
    }
    if (ertd) {
        run_test_data_read_unlock(ertd);
    }
    run_info_read_unlock(eri);

    if (efr->efs->owner_uid == efr->fx->uid) {
        perms >>= 6;
    } else if (efr->efs->owner_gid == efr->fx->gid) {
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
    if (efr->efs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }
    if ((ffi->flags & O_ACCMODE) != O_RDONLY) {
        return -EPERM;
    }

    run_test_data_maybe_update(efr->efs, efr->ecs, efr->ert, efr->run_id, efr->test_file_index, efr->current_time_us);
    struct EjRunTestData *ertd = run_test_data_read_lock(efr->ert, efr->test_file_index);
    if (!ertd || !ertd->ok) {
        run_test_data_read_unlock(ertd);
        return -ENOENT;
    }

    run_test_data_read_unlock(ertd);
    return 0;
}

static int
ejf_read(struct EjFuseRequest *efr, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    int retval = -EIO;

    struct EjRunTestData *ertd = run_test_data_read_lock(efr->ert, efr->test_file_index);
    if (!ertd->ok) goto done;
    if (!ertd->size || offset < 0 || offset >= ertd->size || (int) size <= 0) {
        retval = 0;
        goto done;
    }
    if (ertd->size - offset < size) {
        size = ertd->size - offset;
    }
    memcpy(buf, ertd->data + offset, size);
    retval = size;

done:
    run_test_data_read_unlock(ertd);
    return retval;
}

const struct EjFuseOperations ejfuse_contest_problem_runs_run_tests_test_files_operations =
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
