#pragma once

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

#include <stdlib.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "ejfuse.h"

struct EjFuseRequest;

/* some generic operation wrappers */
int ejf_generic_readlink(struct EjFuseRequest *efr, const char *path, char *buf, size_t size);
int ejf_generic_mknod(struct EjFuseRequest *efr, const char *path, mode_t mode, dev_t dev);
int ejf_generic_mkdir(struct EjFuseRequest *efr, const char *path, mode_t mode);
int ejf_generic_unlink(struct EjFuseRequest *efr, const char *path);
int ejf_generic_rmdir(struct EjFuseRequest *efr, const char *path);
int ejf_generic_symlink(struct EjFuseRequest *efr, const char *oldpath, const char *newpath);
int ejf_generic_rename(struct EjFuseRequest *efr, const char *oldpath, const char *newpath);
int ejf_generic_link(struct EjFuseRequest *efr, const char *oldpath, const char *newpath);
int ejf_generic_chmod(struct EjFuseRequest *efr, const char *path, mode_t mode);
int ejf_generic_chown(struct EjFuseRequest *efr, const char *path, uid_t uid, gid_t gid);
int ejf_generic_truncate(struct EjFuseRequest *efr, const char *path, off_t size);
int ejf_generic_open(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi);
int ejf_generic_read(struct EjFuseRequest *efr, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi);
int ejf_generic_write(struct EjFuseRequest *efr, const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *ffi);
int ejf_generic_statfs(struct EjFuseRequest *efr, const char *path, struct statvfs *sfs);
int ejf_generic_flush(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi);
int ejf_generic_release(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi);
int ejf_generic_fsync(struct EjFuseRequest *efr, const char *path, int datasync, struct fuse_file_info *ffi);
int ejf_generic_setxattr(struct EjFuseRequest *efr, const char *path, const char *name, const char *buf, size_t size, int flags);
int ejf_generic_getxattr(struct EjFuseRequest *efr, const char *path, const char *name, char *buf, size_t size);
int ejf_generic_listxattr(struct EjFuseRequest *efr, const char *path, char *args, size_t size);
int ejf_generic_removexattr(struct EjFuseRequest *efr, const char *path, const char *name);
int ejf_generic_opendir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi);
int ejf_generic_readdir(struct EjFuseRequest *efr, const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *ffi);
int ejf_generic_releasedir(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi);
int ejf_generic_fsyncdir(struct EjFuseRequest *efr, const char *path, int datasync, struct fuse_file_info *ffi);
int ejf_generic_create(struct EjFuseRequest *efr, const char *path, mode_t mode, struct fuse_file_info *ffi);
int ejf_generic_ftruncate(struct EjFuseRequest *efr, const char *path, off_t mode, struct fuse_file_info *ffi);
int ejf_generic_fgetattr(struct EjFuseRequest *efr, const char *path, struct stat *stb, struct fuse_file_info *ffi);
int ejf_generic_lock(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, int cmd, struct flock *fl);
int ejf_generic_utimens(struct EjFuseRequest *efr, const char *path, const struct timespec tv[2]);
int ejf_generic_bmap(struct EjFuseRequest *efr, const char *path, size_t blocksize, uint64_t *idx);
int ejf_generic_ioctl(struct EjFuseRequest *efr, const char *path, int cmd, void *arg, struct fuse_file_info *ffi, unsigned int flags, void *data);
int ejf_generic_poll(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, struct fuse_pollhandle *ph, unsigned *reventsp);
int ejf_generic_write_buf(struct EjFuseRequest *efr, const char *path, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *ffi);
int ejf_generic_read_buf(struct EjFuseRequest *efr, const char *path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *ffi);
int ejf_generic_flock(struct EjFuseRequest *efr, const char *path, struct fuse_file_info *ffi, int op);
int ejf_generic_fallocate(struct EjFuseRequest *efr, const char *path, int arg3, off_t arg4, off_t arg5, struct fuse_file_info *ffi);

// generic operations
extern const struct EjFuseOperations ejfuse_generic_operations;
