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

#include "ejfuse_file.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>

struct EjFileNode *
file_node_create(int fnode)
{
    struct EjFileNode *efn = calloc(1, sizeof(*efn));
    efn->fnode = fnode;
    pthread_mutex_init(&efn->m, NULL);
    return efn;
}

void
file_node_free(struct EjFileNode *efn)
{
    if (efn) {
        pthread_mutex_destroy(&efn->m);
        free(efn->data);
        free(efn);
    }
}

struct EjFileNodes *
file_nodes_create(int node_quota, int size_quota)
{
    struct EjFileNodes *efns = calloc(1, sizeof(*efns));
    pthread_rwlock_init(&efns->rwl, NULL);
    efns->serial = 1;
    efns->node_quota = node_quota;
    efns->size_quota = size_quota;
    return efns;
}

void
file_nodes_free(struct EjFileNodes *efns)
{
    if (efns) {
        pthread_rwlock_destroy(&efns->rwl);
        for (int i = 0; i < efns->size; ++i) {
            file_node_free(efns->nodes[i]);
        }
        free(efns->nodes);
        {
            struct EjFileNode *p, *q;
            for (p = efns->reclaim_first; p; p = q) {
                q = p->reclaim_next;
                file_node_free(p);
            }
        }
        free(efns);
    }
}

static struct EjFileNode *
file_nodes_create_node(struct EjFileNodes *efns)
{
    struct EjFileNode *retval = NULL;
    pthread_rwlock_wrlock(&efns->rwl);
    if (efns->node_quota <= 0 || efns->size < efns->node_quota) {
        if (efns->size == efns->reserved) {
            if (!(efns->reserved *= 2)) efns->reserved = 32;
            efns->nodes = realloc(efns->nodes, efns->reserved * sizeof(efns->nodes[0]));
        }
        efns->nodes[efns->size++] = retval = file_node_create(efns->serial++);
        atomic_fetch_add_explicit(&retval->refcnt, 1, memory_order_relaxed);
        pthread_rwlock_unlock(&efns->rwl);
    }
    return retval;
}

struct EjFileNode *
file_nodes_get_node(struct EjFileNodes *efns, int fnode)
{
    struct EjFileNode *retval = NULL;
    pthread_rwlock_rdlock(&efns->rwl);
    if (efns->size > 0 && fnode >= efns->nodes[0]->fnode && fnode <= efns->nodes[efns->size - 1]->fnode) {
        int low = 0, high = efns->size;
        while (low < high) {
            int mid = (low + high) / 2;
            struct EjFileNode *tmp = efns->nodes[mid];
            if (tmp->fnode == fnode) {
                retval = tmp;
                atomic_fetch_add_explicit(&retval->refcnt, 1, memory_order_relaxed);
                break;
            } else if (tmp->fnode < fnode) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }
    pthread_rwlock_unlock(&efns->rwl);
    return retval;
}

void
unused_file_nodes_remove_node(struct EjFileNodes *efns, int fnode)
{
    pthread_rwlock_wrlock(&efns->rwl);
    if (efns->size > 0 && fnode >= efns->nodes[0]->fnode && fnode <= efns->nodes[efns->size - 1]->fnode) {
        int low = 0, high = efns->size;
        while (low < high) {
            int mid = (low + high) / 2;
            struct EjFileNode *tmp = efns->nodes[mid];
            if (tmp->fnode == fnode) {
                tmp->reclaim_next = efns->reclaim_first;
                efns->reclaim_first = tmp;
                if (mid < efns->size - 1) {
                    memmove(&efns->nodes[mid], &efns->nodes[mid + 1], (efns->size - mid - 1) * sizeof(efns->nodes[0]));
                }
                --efns->size;
                break;
            } else if (tmp->fnode < fnode) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }
    pthread_rwlock_unlock(&efns->rwl);
}

// efn is an owned pointer
void
file_nodes_maybe_remove(struct EjFileNodes *efns, struct EjFileNode *efn, long long current_time_us)
{
    if (atomic_load_explicit(&efn->nlink, memory_order_relaxed) > 0
        || atomic_load_explicit(&efn->opencnt, memory_order_relaxed) > 0) return;

    pthread_rwlock_wrlock(&efns->rwl);
    int low = 0, high = efns->size;
    struct EjFileNode *rmn = NULL;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjFileNode *tmp = efns->nodes[mid];
        if (tmp->fnode == efn->fnode) {
            assert(tmp == efn);
            low = mid;
            rmn = tmp;
            break;
        } else if (tmp->fnode < efn->fnode) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (!rmn) {
        atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
        pthread_rwlock_unlock(&efns->rwl);
        return;
    }
    if (low < efns->size - 1) {
        memmove(&efns->nodes[low], &efns->nodes[low + 1], (efns->size - low - 1) * sizeof(efns->nodes[0]));
    }
    --efns->size;
    atomic_fetch_sub_explicit(&efns->total_size, rmn->size, memory_order_relaxed);
    if (atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed) <= 1) {
        file_node_free(rmn);
    } else {
        rmn->reclaim_next = efns->reclaim_first;
        efns->reclaim_first = rmn;
        rmn->dtime_us = current_time_us;
    }
    pthread_rwlock_unlock(&efns->rwl);
}

struct EjDirectoryNode *
dir_node_create(
        int fnode,
        const unsigned char *name,
        size_t len)
{
    if (len > NAME_MAX) return NULL;

    struct EjDirectoryNode *edn = calloc(1, sizeof(*edn));
    edn->fnode = fnode;
    memcpy(edn->name, name, len);
    return edn;
}

void
dir_node_free(struct EjDirectoryNode *edn)
{
    if (edn) {
        free(edn);
    }
}

struct EjDirectoryNodes *
dir_nodes_create(void)
{
    struct EjDirectoryNodes *edns = calloc(1, sizeof(*edns));
    pthread_rwlock_init(&edns->rwl, NULL);
    return edns;
}

void
dir_nodes_free(struct EjDirectoryNodes *edns)
{
    if (edns) {
        pthread_rwlock_destroy(&edns->rwl);
        for (int i = 0; i < edns->size; ++i) {
            dir_node_free(edns->nodes[i]);
        }
        free(edns->nodes);
        free(edns);
    }
}

int
dir_nodes_get_node(
        struct EjDirectoryNodes *edns,
        const unsigned char *name,
        size_t len,
        struct EjDirectoryNode *res)
{
    int retval = -ENOENT;
    if (len > NAME_MAX) return -ENAMETOOLONG;
    pthread_rwlock_rdlock(&edns->rwl);
    int low = 0, high = edns->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjDirectoryNode *tmp = edns->nodes[mid];
        int r = strcmp(tmp->name, name);
        if (!r) {
            memcpy(res, tmp, sizeof(*res));
            retval = 0;
            break;
        } else if (r < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    pthread_rwlock_unlock(&edns->rwl);
    return retval;
}

int
dir_nodes_get_node_by_fnode(
        struct EjDirectoryNodes *edns,
        int fnode,
        struct EjDirectoryNode *res)
{
    // only linear scan here :(
    int retval = -ENOENT;
    pthread_rwlock_rdlock(&edns->rwl);
    for (int i = 0; i < edns->size; ++i) {
        struct EjDirectoryNode *tmp = edns->nodes[i];
        if (tmp->fnode == fnode) {
            memcpy(res, tmp, sizeof(*res));
            retval = 0;
            break;
        }
    }
    pthread_rwlock_unlock(&edns->rwl);
    return retval;
}

int
dir_nodes_open_node(
        struct EjDirectoryNodes *edns,
        struct EjFileNodes *efns,
        const unsigned char *name,
        size_t len,
        int create_mode,
        int excl_mode,
        int perms,                    // for created files
        long long current_time_us,    // for timestamps for created files
        struct EjDirectoryNode *res)
{
    if (len > NAME_MAX) return -ENAMETOOLONG;

    int retval = -ENOENT;
    pthread_rwlock_wrlock(&edns->rwl);
    int low = 0, high = edns->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjDirectoryNode *tmp = edns->nodes[mid];
        int r = strcmp(tmp->name, name);
        if (!r) {
            if (excl_mode > 0) {
                retval = -EEXIST;
            } else {
                retval = 0;
                memcpy(res, tmp, sizeof(*res));
            }
            goto done;
        } else if (r < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (create_mode <= 0) {
        retval = -ENOENT;
        goto done;
    }

    struct EjFileNode *efn = file_nodes_create_node(efns);
    if (!efn) {
        retval = -EMFILE;
        goto done;
    }
    efn->mode = perms & 0777;
    efn->ctime_us = current_time_us;
    efn->mtime_us = current_time_us;

    if (edns->size == edns->reserved) {
        if (!(edns->reserved *= 2)) edns->reserved = 16;
        edns->nodes = realloc(edns->nodes, edns->reserved * sizeof(edns->nodes[0]));
    }
    if (low < edns->size) {
        memmove(&edns->nodes[low + 1], &edns->nodes[low], (edns->size - low) * sizeof(edns->nodes[0]));
    }
    edns->nodes[low] = dir_node_create(efn->fnode, name, len);
    ++edns->size;
    atomic_fetch_add_explicit(&efn->nlink, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&efn->refcnt, 1, memory_order_relaxed);
    memcpy(res, edns->nodes[low], sizeof(*res));
    retval = 0;

done:
    pthread_rwlock_unlock(&edns->rwl);
    return retval;
}

int
dir_nodes_unlink_node(
        struct EjDirectoryNodes *edns,
        const unsigned char *name,
        size_t len,
        struct EjDirectoryNode *res)
{
    if (len > NAME_MAX) return -ENAMETOOLONG;

    int retval = -ENOENT;
    pthread_rwlock_wrlock(&edns->rwl);
    int low = 0, high = edns->size;
    while (low < high) {
        int mid = (low + high) / 2;
        struct EjDirectoryNode *tmp = edns->nodes[mid];
        int r = strcmp(tmp->name, name);
        if (!r) {
            memcpy(res, tmp, sizeof(*res));
            free(tmp);
            edns->nodes[mid] = NULL;
            if (mid < edns->size - 1) {
                memmove(&edns->nodes[mid], &edns->nodes[mid + 1], (edns->size - mid - 1) * sizeof(edns->nodes[0]));
            }
            --edns->size;
            retval = 0;
            goto done;
        } else if (r < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

done:
    pthread_rwlock_unlock(&edns->rwl);
    return retval;
}

void
dir_nodes_lock(struct EjDirectoryNodes *edns)
{
    pthread_rwlock_rdlock(&edns->rwl);
}

void
dir_nodes_unlock(struct EjDirectoryNodes *edns)
{
    pthread_rwlock_unlock(&edns->rwl);
}

int
dir_nodes_size(struct EjDirectoryNodes *edns)
{
    return edns->size;
}

int
dir_nodes_read(struct EjDirectoryNodes *edns, int index, struct EjDirectoryNode *res)
{
    memcpy(res, edns->nodes[index], sizeof(*res));
    return 0;
}

int
file_node_reserve_unlocked(struct EjFileNode *efn, off_t offset)
{
    if (offset < 0) return -EINVAL;
    int ioff = offset;
    if (ioff != offset) return -EINVAL;
    if (ioff <= efn->reserved) return 0;

    int new_reserved = efn->reserved * 2;
    if (!new_reserved) new_reserved = 4096;
    while (new_reserved < ioff) new_reserved *= 2;
    unsigned char *new_data = realloc(efn->data, new_reserved);
    if (!new_data) return -EIO;
    efn->data = new_data;
    memset(efn->data + efn->reserved, 0, new_reserved - efn->reserved);
    efn->reserved = new_reserved;

    return 0;
}

int
file_node_truncate_unlocked(struct EjFileNodes *efns, struct EjFileNode *efn, off_t offset)
{
    if (offset < 0) return -EINVAL;
    int ioff = offset;
    if (ioff != offset) return -EINVAL;
    if (ioff == efn->size) return 0;
    if (ioff < efn->size) {
        int diff = efn->size - ioff;
        atomic_fetch_sub_explicit(&efns->total_size, diff, memory_order_relaxed);
        memset(efn->data + ioff, 0, efn->size - ioff);
        efn->size = ioff;
        return 0;
    }

    // race condition on total_size between check and add
    int diff = ioff - efn->size;
    int new_size;
    if (__builtin_add_overflow(efns->total_size, diff, &new_size)) {
        return -EIO;
    }
    if (efns->size_quota > 0 && new_size > efns->size_quota) {
        return -EIO;
    }

    int res = file_node_reserve_unlocked(efn, ioff);
    if (res < 0) return res;

    efn->size = ioff;
    atomic_fetch_add_explicit(&efns->total_size, diff, memory_order_relaxed);
    return 0;
}

void
file_nodes_list(struct EjFileNodes *efns)
{
    if (!efns) return;
    pthread_rwlock_rdlock(&efns->rwl);
    if (efns->size > 0 || efns->reclaim_first) {
        fprintf(stderr, "NODES: nodes: %d, size: %d\n", efns->size, efns->total_size);
    }
    for (int i = 0; i < efns->size; ++i) {
        struct EjFileNode *efn = efns->nodes[i];
        fprintf(stderr, "[%d]: %d, %d, %d, %d, %d\n", i, efn->fnode, efn->refcnt, efn->opencnt, efn->nlink, efn->size);
    }
    if (efns->reclaim_first) {
        int serial = 0;
        fprintf(stderr, "RECLAIM:\n");
        for (struct EjFileNode *efn = efns->reclaim_first; efn; efn = efn->reclaim_next) {
            fprintf(stderr, "[%d]: %d, %d, %d, %d, %d, %lld\n", serial++, efn->fnode, efn->refcnt, efn->opencnt, efn->nlink, efn->size, efn->dtime_us);
        }
    }
    pthread_rwlock_unlock(&efns->rwl);
}
