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

#include "ops_fuse.h"
#include "ejfuse.h"

#include <errno.h>

/*
Generic handling of FUSE requests
 */
static int
ejf_entry_getattr(const char *path, struct stat *stb)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->getattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->getattr(&rq, path, stb));
}
static int __attribute__((unused))
ejf_entry_readlink(const char *path, char *buf, size_t size)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->readlink) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->readlink(&rq, path, buf, size));
}
static int
ejf_entry_mknod(const char *path, mode_t mode, dev_t dev)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->mknod) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->mknod(&rq, path, mode, dev));
}
static int
ejf_entry_mkdir(const char *path, mode_t mode)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->mkdir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->mkdir(&rq, path, mode));
}
static int
ejf_entry_unlink(const char *path)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->unlink) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->unlink(&rq, path));
}
static int
ejf_entry_rmdir(const char *path)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->rmdir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->rmdir(&rq, path));
}
static int __attribute__((unused))
ejf_entry_symlink(const char *target, const char *linkpath)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(target, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->symlink) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->symlink(&rq, target, linkpath));
}
static int
ejf_entry_rename(const char *oldpath, const char *newpath)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(oldpath, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->rename) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->rename(&rq, oldpath, newpath));
}
static int __attribute__((unused))
ejf_entry_link(const char *oldpath, const char *newpath)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(oldpath, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->link) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->link(&rq, oldpath, newpath));
}
static int
ejf_entry_chmod(const char *path, mode_t mode)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->chmod) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->chmod(&rq, path, mode));
}
static int __attribute__((unused))
ejf_entry_chown(const char *path, uid_t uid, gid_t gid)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->chown) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->chown(&rq, path, uid, gid));
}
static int
ejf_entry_truncate(const char *path, off_t newsize)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->truncate) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->truncate(&rq, path, newsize));
}
static int
ejf_entry_open(const char *path, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->open) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->open(&rq, path, ffi));
}
static int
ejf_entry_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->read) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->read(&rq, path, buf, size, offset, ffi));
}
static int
ejf_entry_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->write) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->write(&rq, path, buf, size, offset, ffi));
}
static int __attribute__((unused))
ejf_entry_statfs(const char *path, struct statvfs *sfs)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->statfs) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->statfs(&rq, path, sfs));
}
static int
ejf_entry_flush(const char *path, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->flush) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->flush(&rq, path, ffi));
}
static int
ejf_entry_release(const char *path, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->release) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->release(&rq, path, ffi));
}
static int __attribute__((unused))
ejf_entry_fsync(const char *path, int datasync, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->fsync) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->fsync(&rq, path, datasync, ffi));
}
static int __attribute__((unused))
ejf_entry_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->setxattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->setxattr(&rq, path, name, value, size, flags));
}
static int __attribute__((unused))
ejf_entry_getxattr(const char *path, const char *name, char *value, size_t size)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->getxattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->getxattr(&rq, path, name, value, size));
}
static int __attribute__((unused))
ejf_entry_listxattr(const char *path, char *list, size_t size)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->listxattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->listxattr(&rq, path, list, size));
}
static int __attribute__((unused))
ejf_entry_removexattr(const char *path, const char *name)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->removexattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->removexattr(&rq, path, name));
}
static int
ejf_entry_opendir(const char *path, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->opendir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->opendir(&rq, path, ffi));
}
static int
ejf_entry_readdir(
        const char *path,
        void *arg2,
        fuse_fill_dir_t arg3,
        off_t offset,
        struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->readdir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->readdir(&rq, path, arg2, arg3, offset, ffi));
}
static int
ejf_entry_releasedir(const char *path, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->releasedir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->releasedir(&rq, path, ffi));
}
static int __attribute__((unused))
ejf_entry_fsyncdir(const char *path, int datasync, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->fsyncdir) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->fsyncdir(&rq, path, datasync, ffi));
}
static void *
ejf_entry_init(struct fuse_conn_info *conn)
{
    // WTF?
    return fuse_get_context()->private_data;
}
static void
ejf_entry_destroy(void *user)
{
}
static int
ejf_entry_access(const char *path, int mode)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->access) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->access(&rq, path, mode));
}
static int __attribute__((unused))
ejf_entry_create(const char *path, mode_t mode, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->create) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->create(&rq, path, mode, ffi));
}
static int
ejf_entry_ftruncate(const char *path, off_t offset, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->ftruncate) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->ftruncate(&rq, path, offset, ffi));
}
static int
ejf_entry_fgetattr(const char *path, struct stat *stb, struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->fgetattr) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->fgetattr(&rq, path, stb, ffi));
}
static int __attribute__((unused))
ejf_entry_lock(const char *path, struct fuse_file_info *ffi, int cmd, struct flock *fl)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->lock) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->lock(&rq, path, ffi, cmd, fl));
}
static int
ejf_entry_utimens(const char *path, const struct timespec tv[2])
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->utimens) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->utimens(&rq, path, tv));
}
static int __attribute__((unused))
ejf_entry_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->bmap) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->bmap(&rq, path, blocksize, idx));
}
static int __attribute__((unused))
ejf_entry_ioctl(
        const char *path,
        int cmd,
        void *arg,
        struct fuse_file_info *ffi,
        unsigned int flags,
        void *data)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->ioctl) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->ioctl(&rq, path, cmd, arg, ffi, flags, data));
}
static int
ejf_entry_poll(
        const char *path,
        struct fuse_file_info *ffi,
        struct fuse_pollhandle *ph,
        unsigned *reventsp)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->poll) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->poll(&rq, path, ffi, ph, reventsp));
}
static int __attribute__((unused))
ejf_entry_write_buf(
        const char *path,
        struct fuse_bufvec *buf,
        off_t off,
        struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->write_buf) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->write_buf(&rq, path, buf, off, ffi));
}
static int __attribute__((unused))
ejf_entry_read_buf(
        const char *path,
        struct fuse_bufvec **bufp,
        size_t size,
        off_t off,
        struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->read_buf) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->read_buf(&rq, path, bufp, size, off, ffi));
}
static int __attribute__((unused))
ejf_entry_flock(const char *path, struct fuse_file_info *ffi, int op)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->flock) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->flock(&rq, path, ffi, op));
}
static int __attribute__((unused))
ejf_entry_fallocate(
        const char *path,
        int arg2,
        off_t arg3,
        off_t arg4,
        struct fuse_file_info *ffi)
{
    struct EjFuseRequest rq;
    int r = ejf_process_path(path, &rq);
    if (r < 0) {
        return request_free(&rq, r);
    }
    if (!rq.ops || !rq.ops->fallocate) {
        return request_free(&rq, -ENOSYS);
    }
    return request_free(&rq, rq.ops->fallocate(&rq, path, arg2, arg3, arg4, ffi));
}

const struct fuse_operations ejf_fuse_operations =
{
    /**
     * Get file attributes.
     *
     * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
     * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
     * mount option is given.
     */
    ejf_entry_getattr,

    /**
     * Read the target of a symbolic link
     *
     * The buffer should be filled with a null terminated string.  The
     * buffer size argument includes the space for the terminating
     * null character.  If the linkname is too long to fit in the
     * buffer, it should be truncated.  The return value should be 0
     * for success.
     */
    NULL, //ejf_entry_readlink,

    /* Deprecated, use readdir() instead */
    NULL, //int (*getdir) (const char *, fuse_dirh_t, fuse_dirfil_t);

    /**
     * Create a file node
     *
     * This is called for creation of all non-directory, non-symlink
     * nodes.  If the filesystem defines a create() method, then for
     * regular files that will be called instead.
     */
    ejf_entry_mknod,

    /**
     * Create a directory 
     *
     * Note that the mode argument may not have the type specification
     * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
     * correct directory type bits use  mode|S_IFDIR
     * */
    ejf_entry_mkdir,

    /** Remove a file */
    ejf_entry_unlink,

    /** Remove a directory */
    ejf_entry_rmdir,

    /** Create a symbolic link */
    NULL, //ejf_entry_symlink,

    /** Rename a file */
    ejf_entry_rename,

    /** Create a hard link to a file */
    NULL, //ejf_entry_link,

    /** Change the permission bits of a file */
    ejf_entry_chmod,

    /** Change the owner and group of a file */
    NULL, //ejf_entry_chown,

    /** Change the size of a file */
    ejf_entry_truncate,

    /** Change the access and/or modification times of a file
     *
     * Deprecated, use utimens() instead.
     */
    NULL,

    /**
     * File open operation
     *
     * No creation (O_CREAT, O_EXCL) and by default also no
     * truncation (O_TRUNC) flags will be passed to open(). If an
     * application specifies O_TRUNC, fuse first calls truncate()
     * and then open(). Only if 'atomic_o_trunc' has been
     * specified and kernel version is 2.6.24 or later, O_TRUNC is
     * passed on to open.
     *
     * Unless the 'default_permissions' mount option is given,
     * open should check if the operation is permitted for the
     * given flags. Optionally open may also return an arbitrary
     * filehandle in the fuse_file_info structure, which will be
     * passed to all file operations.
     *
     * Changed in version 2.2
     */
    ejf_entry_open,

    /**
     * Read data from an open file
     *
     * Read should return exactly the number of bytes requested except
     * on EOF or error, otherwise the rest of the data will be
     * substituted with zeroes.  An exception to this is when the
     * 'direct_io' mount option is specified, in which case the return
     * value of the read system call will reflect the return value of
     * this operation.
     *
     * Changed in version 2.2
     */
    ejf_entry_read,

    /**
     * Write data to an open file
     *
     * Write should return exactly the number of bytes requested
     * except on error.  An exception to this is when the 'direct_io'
     * mount option is specified (see read operation).
     *
     * Changed in version 2.2
     */
    ejf_entry_write,

    /**
     * Get file system statistics
     *
     * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
     *
     * Replaced 'struct statfs' parameter with 'struct statvfs' in
     * version 2.5
     */
    NULL, //ejf_entry_statfs,

    /**
     * Possibly flush cached data
     *
     * BIG NOTE: This is not equivalent to fsync().  It's not a
     * request to sync dirty data.
     *
     * Flush is called on each close() of a file descriptor.  So if a
     * filesystem wants to return write errors in close() and the file
     * has cached dirty data, this is a good place to write back data
     * and return any errors.  Since many applications ignore close()
     * errors this is not always useful.
     *
     * NOTE: The flush() method may be called more than once for each
     * open().  This happens if more than one file descriptor refers
     * to an opened file due to dup(), dup2() or fork() calls.  It is
     * not possible to determine if a flush is final, so each flush
     * should be treated equally.  Multiple write-flush sequences are
     * relatively rare, so this shouldn't be a problem.
     *
     * Filesystems shouldn't assume that flush will always be called
     * after some writes, or that if will be called at all.
     *
     * Changed in version 2.2
     */
    ejf_entry_flush,

    /**
     * Release an open file
     *
     * Release is called when there are no more references to an open
     * file: all file descriptors are closed and all memory mappings
     * are unmapped.
     *
     * For every open() call there will be exactly one release() call
     * with the same flags and file descriptor.  It is possible to
     * have a file opened more than once, in which case only the last
     * release will mean, that no more reads/writes will happen on the
     * file.  The return value of release is ignored.
     *
     * Changed in version 2.2
     */
    ejf_entry_release,

    /**
     * Synchronize file contents
     *
     * If the datasync parameter is non-zero, then only the user data
     * should be flushed, not the meta data.
     *
     * Changed in version 2.2
     */
    NULL, //ejf_entry_fsync,

    /** Set extended attributes */
    NULL, //ejf_entry_setxattr,

    /** Get extended attributes */
    NULL, //ejf_entry_getxattr,

    /** List extended attributes */
    NULL, //ejf_entry_listxattr,

    /** Remove extended attributes */
    NULL, //ejf_entry_removexattr,

    /**
     * Open directory
     *
     * Unless the 'default_permissions' mount option is given,
     * this method should check if opendir is permitted for this
     * directory. Optionally opendir may also return an arbitrary
     * filehandle in the fuse_file_info structure, which will be
     * passed to readdir, closedir and fsyncdir.
     *
     * Introduced in version 2.3
     */
    ejf_entry_opendir,

    /**
     * Read directory
     *
     * This supersedes the old getdir() interface.  New applications
     * should use this.
     *
     * The filesystem may choose between two modes of operation:
     *
     * 1) The readdir implementation ignores the offset parameter, and
     * passes zero to the filler function's offset.  The filler
     * function will not return '1' (unless an error happens), so the
     * whole directory is read in a single readdir operation.  This
     * works just like the old getdir() method.
     *
     * 2) The readdir implementation keeps track of the offsets of the
     * directory entries.  It uses the offset parameter and always
     * passes non-zero offset to the filler function.  When the buffer
     * is full (or an error happens) the filler function will return
     * '1'.
     *
     * Introduced in version 2.3
     */
    ejf_entry_readdir,

    /**
     * Release directory
     *
     * Introduced in version 2.3
     */
    ejf_entry_releasedir,

    /**
     * Synchronize directory contents
     *
     * If the datasync parameter is non-zero, then only the user data
     * should be flushed, not the meta data
     *
     * Introduced in version 2.3
     */
    NULL, //ejf_entry_fsyncdir,

    /**
     * Initialize filesystem
     *
     * The return value will passed in the private_data field of
     * fuse_context to all file operations and as a parameter to the
     * destroy() method.
     *
     * Introduced in version 2.3
     * Changed in version 2.6
     */
    ejf_entry_init,

    /**
     * Clean up filesystem
     *
     * Called on filesystem exit.
     *
     * Introduced in version 2.3
     */
    ejf_entry_destroy,

    /**
     * Check file access permissions
     *
     * This will be called for the access() system call.  If the
     * 'default_permissions' mount option is given, this method is not
     * called.
     *
     * This method is not called under Linux kernel versions 2.4.x
     *
     * Introduced in version 2.5
     */
    ejf_entry_access,

    /**
     * Create and open a file
     *
     * If the file does not exist, first create it with the specified
     * mode, and then open it.
     *
     * If this method is not implemented or under Linux kernel
     * versions earlier than 2.6.15, the mknod() and open() methods
     * will be called instead.
     *
     * Introduced in version 2.5
     */
    NULL, //ejf_entry_create,

    /**
     * Change the size of an open file
     *
     * This method is called instead of the truncate() method if the
     * truncation was invoked from an ftruncate() system call.
     *
     * If this method is not implemented or under Linux kernel
     * versions earlier than 2.6.15, the truncate() method will be
     * called instead.
     *
     * Introduced in version 2.5
     */
    ejf_entry_ftruncate,

    /**
     * Get attributes from an open file
     *
     * This method is called instead of the getattr() method if the
     * file information is available.
     *
     * Currently this is only called after the create() method if that
     * is implemented (see above).  Later it may be called for
     * invocations of fstat() too.
     *
     * Introduced in version 2.5
     */
    ejf_entry_fgetattr,

    /**
     * Perform POSIX file locking operation
     *
     * The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
     *
     * For the meaning of fields in 'struct flock' see the man page
     * for fcntl(2).  The l_whence field will always be set to
     * SEEK_SET.
     *
     * For checking lock ownership, the 'fuse_file_info->owner'
     * argument must be used.
     *
     * For F_GETLK operation, the library will first check currently
     * held locks, and if a conflicting lock is found it will return
     * information without calling this method.  This ensures, that
     * for local locks the l_pid field is correctly filled in.  The
     * results may not be accurate in case of race conditions and in
     * the presence of hard links, but it's unlikely that an
     * application would rely on accurate GETLK results in these
     * cases.  If a conflicting lock is not found, this method will be
     * called, and the filesystem may fill out l_pid by a meaningful
     * value, or it may leave this field zero.
     *
     * For F_SETLK and F_SETLKW the l_pid field will be set to the pid
     * of the process performing the locking operation.
     *
     * Note: if this method is not implemented, the kernel will still
     * allow file locking to work locally.  Hence it is only
     * interesting for network filesystems and similar.
     *
     * Introduced in version 2.6
     */
    NULL, //ejf_entry_lock,

    /**
     * Change the access and modification times of a file with
     * nanosecond resolution
     *
     * This supersedes the old utime() interface.  New applications
     * should use this.
     *
     * See the utimensat(2) man page for details.
     *
     * Introduced in version 2.6
     */
    ejf_entry_utimens,

    /**
     * Map block index within file to block index within device
     *
     * Note: This makes sense only for block device backed filesystems
     * mounted with the 'blkdev' option
     *
     * Introduced in version 2.6
     */
    NULL, //ejf_entry_bmap,

    /**
     * Flag indicating that the filesystem can accept a NULL path
     * as the first argument for the following operations:
     *
     * read, write, flush, release, fsync, readdir, releasedir,
     * fsyncdir, ftruncate, fgetattr, lock, ioctl and poll
     *
     * If this flag is set these operations continue to work on
     * unlinked files even if "-ohard_remove" option was specified.
     */
    //unsigned int flag_nullpath_ok:1;
    0,

    /**
     * Flag indicating that the path need not be calculated for
     * the following operations:
     *
     * read, write, flush, release, fsync, readdir, releasedir,
     * fsyncdir, ftruncate, fgetattr, lock, ioctl and poll
     *
     * Closely related to flag_nullpath_ok, but if this flag is
     * set then the path will not be calculaged even if the file
     * wasn't unlinked.  However the path can still be non-NULL if
     * it needs to be calculated for some other reason.
     */
    //unsigned int flag_nopath:1;
    0,

    /**
     * Flag indicating that the filesystem accepts special
     * UTIME_NOW and UTIME_OMIT values in its utimens operation.
     */
    //unsigned int flag_utime_omit_ok:1;
    0,

    /**
     * Reserved flags, don't set
     */
    //unsigned int flag_reserved:29;
    0,

    /**
     * Ioctl
     *
     * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
     * 64bit environment.  The size and direction of data is
     * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
     * data will be NULL, for _IOC_WRITE data is out area, for
     * _IOC_READ in area and if both are set in/out area.  In all
     * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
     *
     * If flags has FUSE_IOCTL_DIR then the fuse_file_info refers to a
     * directory file handle.
     *
     * Introduced in version 2.8
     */
    NULL, //ejf_entry_ioctl,

    /**
     * Poll for IO readiness events
     *
     * Note: If ph is non-NULL, the client should notify
     * when IO readiness events occur by calling
     * fuse_notify_poll() with the specified ph.
     *
     * Regardless of the number of times poll with a non-NULL ph
     * is received, single notification is enough to clear all.
     * Notifying more times incurs overhead but doesn't harm
     * correctness.
     *
     * The callee is responsible for destroying ph with
     * fuse_pollhandle_destroy() when no longer in use.
     *
     * Introduced in version 2.8
     */
    ejf_entry_poll,

    /**
     * Write contents of buffer to an open file
     *
     * Similar to the write() method, but data is supplied in a
     * generic buffer.  Use fuse_buf_copy() to transfer data to
     * the destination.
     *
     * Introduced in version 2.9
     */
    NULL, //ejf_entry_write_buf,

    /**
     * Store data from an open file in a buffer
     *
     * Similar to the read() method, but data is stored and
     * returned in a generic buffer.
     *
     * No actual copying of data has to take place, the source
     * file descriptor may simply be stored in the buffer for
     * later data transfer.
     *
     * The buffer must be allocated dynamically and stored at the
     * location pointed to by bufp.  If the buffer contains memory
     * regions, they too must be allocated using malloc().  The
     * allocated memory will be freed by the caller.
     *
     * Introduced in version 2.9
     */
    NULL, //ejf_entry_read_buf,

    /**
     * Perform BSD file locking operation
     *
     * The op argument will be either LOCK_SH, LOCK_EX or LOCK_UN
     *
     * Nonblocking requests will be indicated by ORing LOCK_NB to
     * the above operations
     *
     * For more information see the flock(2) manual page.
     *
     * Additionally fi->owner will be set to a value unique to
     * this open file.  This same value will be supplied to
     * ->release() when the file is released.
     *
     * Note: if this method is not implemented, the kernel will still
     * allow file locking to work locally.  Hence it is only
     * interesting for network filesystems and similar.
     *
     * Introduced in version 2.9
     */
    NULL, //ejf_entry_flock,

    /**
     * Allocates space for an open file
     *
     * This function ensures that required space is allocated for specified
     * file.  If this function returns success then any subsequent write
     * request to specified range is guaranteed not to fail because of lack
     * of space on the file system media.
     *
     * Introduced in version 2.9.1
     */
    NULL, //ejf_entry_fallocate,
};
