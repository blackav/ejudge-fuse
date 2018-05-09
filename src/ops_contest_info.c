#include "ops_contest_info.h"

#include "ejfuse.h"
#include "ops_generic.h"
#include "contests_state.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

static int
ejf_contest_info_getattr(struct EjFuseRequest *efr, const char *path, struct stat *stb)
{
    struct EjFuseState *ejs = efr->ejs;
    struct EjContestState *ecs = efr->ecs;
    int retval = -ENOENT;
    unsigned char fullpath[PATH_MAX];
    off_t size = 0;

    struct EjContestInfo *eci = contest_info_read_lock(ecs);
    if (!eci || !eci->ok || !eci->info_json) {
        contest_info_read_unlock(eci);
        goto done;
    }
    size = strlen(eci->info_json);
    contest_info_read_unlock(eci);

    memset(stb, 0, sizeof(*stb));
    snprintf(fullpath, sizeof(fullpath), "/%d/INFO", efr->contest_id);
    stb->st_ino = get_inode(ejs, fullpath);
    stb->st_mode = S_IFREG | EJFUSE_FILE_PERMS;
    stb->st_size = size;
    stb->st_nlink = 1;
    stb->st_uid = ejs->owner_uid;
    stb->st_gid = ejs->owner_gid;

    long long current_time_us = ejs->current_time_us;
    stb->st_atim.tv_sec = current_time_us / 1000000;
    stb->st_atim.tv_nsec = (current_time_us % 1000000) * 1000;
    stb->st_mtim.tv_sec = ejs->start_time_us / 1000000;
    stb->st_mtim.tv_nsec = (ejs->start_time_us % 1000000) * 1000;
    stb->st_ctim.tv_sec = ejs->start_time_us / 1000000;
    stb->st_ctim.tv_nsec = (ejs->start_time_us % 1000000) * 1000;

    retval = 0;
done:
    return retval;
}

static int
ejf_contest_info_access(struct EjFuseRequest *efr, const char *path, int mode)
{
    struct EjContestState *ecs = efr->ecs;
    int retval = -ENOENT;
    int perms = EJFUSE_FILE_PERMS;
    mode &= 07;

    struct EjContestInfo *eci = contest_info_read_lock(ecs);
    if (!eci || !eci->ok || !eci->info_json) {
        contest_info_read_unlock(eci);
        goto done;
    }
    contest_info_read_unlock(eci);

    if (efr->ejs->owner_uid == efr->fx->uid) {
        perms >>= 6;
    } else if (efr->ejs->owner_gid == efr->fx->gid) {
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
ejf_contest_info_open(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    struct EjContestInfo *eci = contest_info_read_lock(efr->ecs);
    if (!eci || !eci->ok || !eci->info_json) {
        contest_info_read_unlock(eci);
        return -ENOENT;
    }
    contest_info_read_unlock(eci);

    if (efr->ejs->owner_uid != efr->fx->uid) {
        return -EPERM;
    }
    if ((ffi->flags & O_ACCMODE) != O_RDONLY) {
        return -EPERM;
    }
    return 0;
}

static int
ejf_contest_info_read(struct EjFuseRequest *efr, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    int retval = 0;
    struct EjContestInfo *eci = contest_info_read_lock(efr->ecs);
    if (!eci || !eci->ok || !eci->info_json) {
        retval = -EIO;
        goto cleanup;
    }

    size_t len = strlen(eci->info_json);
    if (!size || offset < 0 || offset >= len) {
        goto cleanup;
    }
    if (len - offset < size) {
        size = len - offset;
    }
    memcpy(buf, eci->info_json + offset, size);
    retval = size;

cleanup:
    contest_info_read_unlock(eci);
    return retval;
}

static int
ejf_contest_info_release(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return 0;
}

const struct EjFuseOperations ejfuse_contest_info_operations =
{
    ejf_contest_info_getattr, //int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
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
    ejf_contest_info_open, //int (*open)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_contest_info_read, //int (*read)(struct EjFuseRequest *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_write, //int (*write)(struct EjFuseRequest *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    ejf_generic_statfs, //int (*statfs)(struct EjFuseRequest *, const char *, struct statvfs *);
    ejf_generic_flush, //int (*flush)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_contest_info_release, //int (*release)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsync, //int (*fsync)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_generic_setxattr, //int (*setxattr)(struct EjFuseRequest *, const char *, const char *, const char *, size_t, int);
    ejf_generic_getxattr, //int (*getxattr)(struct EjFuseRequest *, const char *, const char *, char *, size_t);
    ejf_generic_listxattr, //int (*listxattr)(struct EjFuseRequest *, const char *, char *, size_t);
    ejf_generic_removexattr, //int (*removexattr)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_opendir, //int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_readdir, //int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    ejf_generic_releasedir, //int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsyncdir, //int (*fsyncdir)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    ejf_contest_info_access, //int (*access)(struct EjFuseRequest *, const char *, int);
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