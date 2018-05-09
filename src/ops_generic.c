#include "ops_generic.h"

#include <string.h>
#include <errno.h>

int
ejf_generic_readlink(struct EjFuseRequest *efr, const char *path, char *buf, size_t size)
{
    return -EINVAL;
}
int
ejf_generic_mknod(struct EjFuseRequest *efr, const char *path, mode_t mode, dev_t dev)
{
    // disallow creating new files
    return -EPERM;
}
int
ejf_generic_mkdir(struct EjFuseRequest *efr, const char *path, mode_t mode)
{
    // disallow creating new directories
    return -EPERM;
}
int
ejf_generic_unlink(struct EjFuseRequest *efr, const char *path)
{
    // disallow file removal
    return -EPERM;
}
int
ejf_generic_rmdir(struct EjFuseRequest *efr, const char *path)
{
    // disallow directory removal
    return -EPERM;
}
int
ejf_generic_symlink(struct EjFuseRequest *efr, const char *oldpath, const char *newpath)
{
    return -EPERM;
}
int
ejf_generic_rename(struct EjFuseRequest *efr, const char *oldpath, const char *newpath)
{
    return -EPERM;
}
int
ejf_generic_link(struct EjFuseRequest *efr, const char *oldpath, const char *newpath)
{
    return -EPERM;
}
int
ejf_generic_chmod(struct EjFuseRequest *efr, const char *path, mode_t mode)
{
    return -EPERM;
}
int
ejf_generic_chown(struct EjFuseRequest *efr, const char *path, uid_t uid, gid_t gid)
{
    return -EPERM;
}
int
ejf_generic_truncate(struct EjFuseRequest *efr, const char *path, off_t size)
{
    return -EPERM;
}
int
ejf_generic_open(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return -EPERM;
}
int
ejf_generic_read(struct EjFuseRequest *efr, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    return -EIO;
}
int
ejf_generic_write(struct EjFuseRequest *efr, const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    return -EIO;
}
int
ejf_generic_statfs(struct EjFuseRequest *efr, const char *path, struct statvfs *sfs)
{

    memset(sfs, 0, sizeof(*sfs));
    sfs->f_bsize = 4096;
    sfs->f_blocks = 0;
    sfs->f_bfree = 0;
    sfs->f_bavail = 0;
    sfs->f_files = 0;
    sfs->f_ffree = 0;
    sfs->f_namemax = 255;
    sfs->f_frsize = 0;
    sfs->f_flag = ST_NOATIME | ST_NODEV | ST_NODIRATIME | ST_NOEXEC | ST_NOSUID;
    return 0;
}
int
ejf_generic_flush(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return 0;
}
int
ejf_generic_release(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return -ENOSYS;
}
int
ejf_generic_fsync(struct EjFuseRequest *efr, const char *path, int datasync, struct fuse_file_info *ffi)
{
    // always success?
    return 0;
}
int
ejf_generic_setxattr(struct EjFuseRequest *efr, const char *path, const char *name, const char *buf, size_t size, int flags)
{
    return -ENOTSUP;
}
int
ejf_generic_getxattr(struct EjFuseRequest *efr, const char *path, const char *name, char *buf, size_t size)
{
    return -ENOTSUP;
}
int
ejf_generic_listxattr(struct EjFuseRequest *efr, const char *path, char *args, size_t size)
{
    return -ENOTSUP;
}
int
ejf_generic_removexattr(struct EjFuseRequest *efr, const char *path, const char *name)
{
    return -ENOTSUP;
}
int
ejf_generic_opendir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return -ENOTDIR;
}
int
ejf_generic_readdir(struct EjFuseRequest *efr, const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *ffi)
{
    return -EIO;
}
int
ejf_generic_releasedir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi)
{
    return -ENOSYS;
}
int
ejf_generic_fsyncdir(struct EjFuseRequest *efr, const char *path, int datasync, struct fuse_file_info *ffi)
{
    // always success?
    return 0;
}
int
ejf_generic_create(struct EjFuseRequest *efr, const char *path, mode_t mode, struct fuse_file_info *ffi)
{
    return -EPERM;
}
int
ejf_generic_ftruncate(struct EjFuseRequest *efr, const char *path, off_t mode, struct fuse_file_info *ffi)
{
    return -EPERM;
}
/*
int
ejf_generic_fgetattr(struct EjFuseRequest *efr, const char *path, struct stat *stb, struct fuse_file_info *ffi)
{
    return efr->ops->getattr(efr, path, stb);
}
*/
int
ejf_generic_lock(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, int cmd, struct flock *fl)
{
    return -ENOSYS;
}
int
ejf_generic_utimens(struct EjFuseRequest *efr, const char *path, const struct timespec tv[2])
{
    return -EPERM;
}
int
ejf_generic_bmap(struct EjFuseRequest *efr, const char *path, size_t blocksize, uint64_t *idx)
{
    return -ENOSYS;
}
int
ejf_generic_ioctl(struct EjFuseRequest *efr, const char *path, int cmd, void *arg, struct fuse_file_info *ffi, unsigned int flags, void *data)
{
    return -EINVAL;
}
int
ejf_generic_poll(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, struct fuse_pollhandle *ph, unsigned *reventsp)
{
    return -ENOSYS;
}
int
ejf_generic_write_buf(struct EjFuseRequest *efr, const char *path, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *ffi)
{
    return -EIO;
}
int
ejf_generic_read_buf(struct EjFuseRequest *efr, const char *path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *ffi)
{
    return -EIO;
}
int
ejf_generic_flock(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, int op)
{
    return -ENOSYS;
}
int
ejf_generic_fallocate(struct EjFuseRequest *efr, const char *path, int arg3, off_t arg4, off_t arg5, struct fuse_file_info *ffi)
{
    return -EOPNOTSUPP;
}

// generic operations
const struct EjFuseOperations __attribute__((unused)) ejfuse_generic_operations =
{
    NULL, //int (*getattr)(struct EjFuseRequest *, const char *, struct stat *);
    ejf_generic_readlink, //int (*readlink)(struct EjFuseRequest *, const char *, char *, size_t);
    NULL, //int (*mknod)(struct EjFuseRequest *, const char *, mode_t, dev_t);
    NULL, //int (*mkdir)(struct EjFuseRequest *, const char *, mode_t);
    NULL, //int (*unlink)(struct EjFuseRequest *, const char *);
    NULL, //int (*rmdir)(struct EjFuseRequest *, const char *);
    ejf_generic_symlink, //int (*symlink)(struct EjFuseRequest *, const char *, const char *);
    NULL, //int (*rename)(struct EjFuseRequest *, const char *, const char *);
    ejf_generic_link, //int (*link)(struct EjFuseRequest *, const char *, const char *);
    NULL, //int (*chmod)(struct EjFuseRequest *, const char *, mode_t);
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
    NULL, //int (*opendir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    NULL, //int (*readdir)(struct EjFuseRequest *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    NULL, //int (*releasedir)(struct EjFuseRequest *, const char *, struct fuse_file_info *);
    ejf_generic_fsyncdir, //int (*fsyncdir)(struct EjFuseRequest *, const char *, int, struct fuse_file_info *);
    NULL, //int (*access)(struct EjFuseRequest *, const char *, int);
    NULL, //int (*create)(struct EjFuseRequest *, const char *, mode_t, struct fuse_file_info *);
    NULL, //int (*ftruncate)(struct EjFuseRequest *, const char *, off_t, struct fuse_file_info *);
    NULL, //int (*fgetattr)(struct EjFuseRequest *, const char *, struct stat *, struct fuse_file_info *);
    ejf_generic_lock, //int (*lock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int cmd, struct flock *);
    NULL, //int (*utimens)(struct EjFuseRequest *, const char *, const struct timespec tv[2]);
    ejf_generic_bmap, //int (*bmap)(struct EjFuseRequest *, const char *, size_t blocksize, uint64_t *idx);
    ejf_generic_ioctl, //int (*ioctl)(struct EjFuseRequest *, const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    NULL, //int (*poll)(struct EjFuseRequest *, const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
    NULL, //int (*write_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
    NULL, //int (*read_buf)(struct EjFuseRequest *, const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
    ejf_generic_flock, //int (*flock)(struct EjFuseRequest *, const char *, struct fuse_file_info *, int op);
    ejf_generic_fallocate, //int (*fallocate)(struct EjFuseRequest *, const char *, int, off_t, off_t, struct fuse_file_info *);
};
