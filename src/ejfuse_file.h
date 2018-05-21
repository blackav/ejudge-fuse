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

#include <limits.h>
#include <pthread.h>
#include <sys/types.h>

// hold read-write file info (similar to inode)
struct EjFileNode
{
    int fnode;   // serial number, a component to "/fnone/<NUM>" path to generate an inode
    struct EjFileNode *reclaim_next; // the list of nodes for reclaim

    pthread_rwlock_t rwl;
    _Atomic int refcnt;   // reference counter for pointers outside of EjFileNodes
    _Atomic int opencnt;  // open file counter
    int nlink;            // refcounter

    int mode;       // just permission bits

    long long atime_us;
    long long mtime_us;
    long long ctime_us;
    long long dname_us;

    int size;      // int - intentionally, we don't want too big files (>= 2G)
    int reserved;  // int - intentionally, we don't want too big files (>= 2G)
    unsigned char *data;
};

struct EjFileNodes
{
    pthread_rwlock_t rwl;

    int serial;
    int reserved;
    int size;
    struct EjFileNode **nodes;
    struct EjFileNode *reclaim_first;
};

struct EjDirectoryNode
{
    int fnode;
    unsigned char name[NAME_MAX + 1];
};

struct EjDirectoryNodes
{
    pthread_rwlock_t rwl;

    int reserved;
    int size;
    struct EjDirectoryNode **nodes;
};

struct EjFileNode *file_node_create(int fnode);
void file_node_free(struct EjFileNode *efn);

struct EjFileNodes *file_nodes_create(void);
void file_nodes_free(struct EjFileNodes *efns);

struct EjFileNode *file_nodes_get_node(struct EjFileNodes *efns, int fnode);
void file_nodes_remove_node(struct EjFileNodes *efns, int fnode);

struct EjDirectoryNode *
dir_node_create(
        int fnode,
        const unsigned char *name,
        size_t len);
void dir_node_free(struct EjDirectoryNode *edn);

struct EjDirectoryNodes *dir_nodes_create(void);
void dir_nodes_free(struct EjDirectoryNodes *edns);

int
dir_nodes_get_node(
        struct EjDirectoryNodes *edns,
        const unsigned char *name,
        size_t len,
        struct EjDirectoryNode *res);
int
dir_nodes_open_node(
        struct EjDirectoryNodes *edns,
        struct EjFileNodes *efns,
        const unsigned char *name,
        size_t len,
        int excl_mode,
        int create_mode,
        struct EjDirectoryNode *res);
int
dir_nodes_unlink_node(
        struct EjDirectoryNodes *edns,
        const unsigned char *name,
        size_t len,
        struct EjDirectoryNode *res);

void dir_nodes_lock(struct EjDirectoryNodes *edns);
void dir_nodes_unlock(struct EjDirectoryNodes *edns);
int dir_nodes_size(struct EjDirectoryNodes *edns);
int dir_nodes_read(struct EjDirectoryNodes *edns, int index, struct EjDirectoryNode *res);

int file_node_reserve_unlocked(struct EjFileNode *efn, off_t offset);
int file_node_truncate_unlocked(struct EjFileNode *efn, off_t offset);

void file_nodes_list(struct EjFileNodes *efns);
