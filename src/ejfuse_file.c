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

struct EjFileNode *
file_node_create(int fnode)
{
    struct EjFileNode *efn = calloc(1, sizeof(*efn));
    efn->fnode = fnode;
    pthread_rwlock_init(&efn->rwl, NULL);
    return efn;
}

void
file_node_free(struct EjFileNode *efn)
{
    if (efn) {
        pthread_rwlock_destroy(&efn->rwl);
        free(efn);
    }
}

struct EjFileNodes *
file_nodes_create(void)
{
    struct EjFileNodes *efns = calloc(1, sizeof(*efns));
    pthread_rwlock_init(&efns->rwl, NULL);
    efns->serial = 1;
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

struct EjFileNode *
file_nodes_create_node(struct EjFileNodes *efns)
{
    struct EjFileNode *retval = NULL;
    pthread_rwlock_wrlock(&efns->rwl);
    if (efns->size == efns->reserved) {
        if (!(efns->reserved *= 2)) efns->reserved = 32;
        efns->nodes = realloc(efns->nodes, efns->reserved * sizeof(efns->nodes[0]));
    }
    efns->nodes[efns->size++] = retval = file_node_create(efns->serial++);
    pthread_rwlock_unlock(&efns->rwl);
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
file_nodes_remove_node(struct EjFileNodes *efns, int fnode)
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
dir_nodes_open_node(
        struct EjDirectoryNodes *edns,
        struct EjFileNodes *efns,
        const unsigned char *name,
        size_t len,
        int excl_mode,
        int create_mode,
        struct EjDirectoryNode *res)
{
    if (len < NAME_MAX) return -ENAMETOOLONG;

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

    if (edns->size == edns->reserved) {
        if (!(edns->reserved *= 2)) edns->reserved = 16;
        edns->nodes = realloc(edns->nodes, edns->reserved * sizeof(edns->nodes[0]));
    }
    if (low < edns->size) {
        memmove(&edns->nodes[low + 1], &edns->nodes[low], (edns->size - low) * sizeof(edns->nodes[0]));
    }
    edns->nodes[low] = dir_node_create(efn->fnode, name, len);
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
    if (len < NAME_MAX) return -ENAMETOOLONG;

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
