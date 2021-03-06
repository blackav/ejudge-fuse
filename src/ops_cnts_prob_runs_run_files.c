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

#include "ops_cnts_prob_runs_run_files.h"

#include "settings.h"
#include "ejfuse.h"
#include "ops_generic.h"
#include "contests_state.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

typedef void (*unlocker_t)(void *ptr);

static void
run_info_unlocker(void *ptr)
{
    run_info_read_unlock((struct EjRunInfo *) ptr);
}
static void
run_source_unlocker(void *ptr)
{
    run_source_read_unlock((struct EjRunSource *) ptr);
}
static void
run_messages_unlocker(void *ptr)
{
    run_messages_read_unlock((struct EjRunMessages *) ptr);
}

static int
get_info(
        struct EjFuseRequest *efr,
        unsigned char **p_data,
        off_t *p_size,
        long long *p_mtime_us,
        unlocker_t *p_unlocker,
        void **p_unlock_data)
{
    switch (efr->file_name_code) {
    case FILE_NAME_INFO:
    case FILE_NAME_INFO_JSON:
    case FILE_NAME_COMPILER_TXT:
    case FILE_NAME_VALUER_TXT: {
        struct EjRunInfo *eri = run_info_read_lock(efr->ers);
        if (!eri || !eri->ok) {
            run_info_read_unlock(eri);
            return -ENOENT;
        }
        if (efr->file_name_code == FILE_NAME_INFO) {
            if (!eri->info_text || !eri->info_size) {
                run_info_read_unlock(eri);
                return -ENOENT;
            }
            if (p_data) *p_data = eri->info_text;
            if (p_size) *p_size = eri->info_size;
            if (p_mtime_us) *p_mtime_us = eri->update_time_us;
        } else if (efr->file_name_code == FILE_NAME_INFO_JSON) {
            if (!eri->info_json_size || !eri->info_json_size) {
                run_info_read_unlock(eri);
                return -ENOENT;
            }
            if (p_data) *p_data = eri->info_json_text;
            if (p_size) *p_size = eri->info_json_size;
            if (p_mtime_us) *p_mtime_us = eri->update_time_us;
        } else if (efr->file_name_code == FILE_NAME_COMPILER_TXT) {
            if (!eri->compiler_text || !eri->compiler_size) {
                run_info_read_unlock(eri);
                return -ENOENT;
            }
            if (p_data) *p_data = eri->compiler_text;
            if (p_size) *p_size = eri->compiler_size;
            if (p_mtime_us) *p_mtime_us = eri->update_time_us;
        } else if (efr->file_name_code == FILE_NAME_VALUER_TXT) {
            if (!eri->valuer_text || !eri->valuer_size) {
                run_info_read_unlock(eri);
                return -ENOENT;
            }
            if (p_data) *p_data = eri->valuer_text;
            if (p_size) *p_size = eri->valuer_size;
            if (p_mtime_us) *p_mtime_us = eri->update_time_us;
        } else {
            abort();
        }
        // in the data mode the data must remain locked
        if (!p_data) {
            run_info_read_unlock(eri);
        } else {
            *p_unlocker = run_info_unlocker;
            *p_unlock_data = eri;
        }
    }
        break;
    case FILE_NAME_SOURCE: {
        efr->file_name = "source";
        if (p_data) {
            // need data, so download the run
            run_source_maybe_update(efr->efs, efr->ecs, efr->ers, efr->current_time_us);
        }
        struct EjRunSource *ert = run_source_read_lock(efr->ers);
        if (!ert && !p_data) {
            // report run info before the first download
            run_source_read_unlock(ert);
            struct EjRunInfo *eri = run_info_read_lock(efr->ers);
            if (!eri || !eri->ok) {
                run_info_read_unlock(eri);
                return -ENOENT;
            }
            if (p_size) *p_size = eri->size;
            if (p_mtime_us) *p_mtime_us = eri->run_time_us;
            run_info_read_unlock(eri);
            return 0;
        }
        if (!ert->ok) {
            run_source_read_unlock(ert);
            return -ENOENT;
        }
        if (p_data) *p_data = ert->data;
        if (p_size) *p_size = ert->size;
        if (p_mtime_us) {
            // FIXME: avoid accessing run_info?
            struct EjRunInfo *eri = run_info_read_lock(efr->ers);
            if (eri && eri->ok) {
                *p_mtime_us = eri->run_time_us;
            } else {
                *p_mtime_us = ert->update_time_us;
            }
            run_info_read_unlock(eri);
        }
        if (!p_data) {
            run_source_read_unlock(ert);
        } else {
            *p_unlocker = run_source_unlocker;
            *p_unlock_data = ert;
        }
        return 0;
    }
        break;
    case FILE_NAME_MESSAGES_TXT: {
        struct EjRunInfo *eri = run_info_read_lock(efr->ers);
        if (!eri || !eri->ok || eri->message_count <= 0) {
            run_info_read_unlock(eri);
            return -ENOENT;
        }
        int message_count = eri->message_count;
        long long run_time_us = eri->run_time_us;
        run_info_read_unlock(eri);
        if (p_data) {
            run_messages_maybe_update(efr->efs, efr->ecs, efr->ers, efr->current_time_us);
        }
        struct EjRunMessages *erms = run_messages_read_lock(efr->ers);
        if (!erms && !p_data) {
            if (p_size) *p_size = message_count;
            if (p_mtime_us) *p_mtime_us = run_time_us;
            return 0;
        }
        if (!erms->ok) {
            run_messages_read_unlock(erms);
            return -ENOENT;
        }
        if (p_data) *p_data = erms->text;
        if (p_size) *p_size = erms->size;
        if (p_mtime_us) *p_mtime_us = erms->latest_time_us;
        if (!p_data) {
            run_messages_read_unlock(erms);
        } else {
            *p_unlocker = run_messages_unlocker;
            *p_unlock_data = erms;
        }
        return 0;
    }
        break;
    default:
        return -ENOENT;
    }
    return 0;
}

// INFO info.json compiler.txt valuer.txt messages.txt source*
static int
ejf_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *efs = efr->efs;

    if (efs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }

    off_t file_size = 0;
    long long mtime_us = 0;
    int err = get_info(efr, NULL, &file_size, &mtime_us, NULL, NULL);
    if (err < 0) return err;
    unsigned char fullpath[PATH_MAX];
    if (snprintf(fullpath, sizeof(fullpath), "/%d/problems/%d/runs/%d/%s", efr->contest_id, efr->prob_id, efr->run_id, efr->file_name) >= sizeof(fullpath)) {
        return -ENAMETOOLONG;
    }

    memset(stb, 0, sizeof(*stb));

    stb->st_ino = get_inode(efs, fullpath);
    stb->st_mode = S_IFREG | EJFUSE_FILE_PERMS;
    stb->st_nlink = 2;
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
    int perms = EJFUSE_FILE_PERMS;
    mode &= 07;

    int res = get_info(efr, NULL, NULL, NULL, NULL, NULL);
    if (res < 0) return res;

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
    unsigned char *file_data = NULL;
    off_t file_size;
    unlocker_t unlocker = NULL;
    void *unlock_data = NULL;

    int res = get_info(efr, &file_data, &file_size, NULL, &unlocker, &unlock_data);
    if (res < 0) return res;
    unlocker(unlock_data);

    if (efr->efs->owner_uid != efr->fx->uid) {
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
    unsigned char *file_data = NULL;
    off_t file_size = 0;
    unlocker_t unlocker = NULL;
    void *unlock_data = NULL;
    int res = get_info(efr, &file_data, &file_size, NULL, &unlocker, &unlock_data);
    if (res < 0) return res;

    if (!size || offset < 0 || offset >= file_size || (int) size <= 0) {
        unlocker(unlock_data);
        return 0;
    }

    if (file_size - offset < size) {
        size = file_size - offset;
    }
    memcpy(buf, file_data + offset, size);
    unlocker(unlock_data);
    return size;
}

const struct EjFuseOperations ejfuse_contest_problem_runs_run_files_operations =
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
