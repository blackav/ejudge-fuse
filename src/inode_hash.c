#include "inode_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <openssl/sha.h>

enum { HASH_TABLE_OFFSET = 5 };

struct EjInodeHash
{
    struct EjInodeHashEntry *entries;
    pthread_rwlock_t rwl;
    int serial;
    int size;
    int used;
    int size_idx;
};

static const int primes[] =
{
    4099,
    8209,
    16411,
    32771,
    65537,
    131101,
    262147,
    524309,
    1048583,
    2097169,
    4194319,
    8388617,
    16777259,
  0,
};

struct EjInodeHash *
inode_hash_create(void)
{
    struct EjInodeHash *ejh = calloc(1, sizeof(*ejh));
    ejh->size = primes[ejh->size_idx++];
    ejh->serial = 1;
    pthread_rwlock_init(&ejh->rwl, NULL);
    ejh->entries = calloc(ejh->size, sizeof(ejh->entries[0]));
    return ejh;
}

void
inode_hash_free(struct EjInodeHash *ejh)
{
    if (ejh) {
        free(ejh->entries);
        pthread_rwlock_destroy(&ejh->rwl);
        free(ejh);
    }
}

struct EjInodeHashEntry *
inode_hash_find(struct EjInodeHash *ejh, const unsigned char *digest)
{
    unsigned long long val;
    memcpy(&val, digest, sizeof(val));
    struct EjInodeHashEntry *retval = NULL;

    pthread_rwlock_rdlock(&ejh->rwl);
    int i = (int)(val % ejh->size);
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[i];
        if (!entry->inode) {
            // not found
            break;
        }
        if (!memcmp(entry->digest, digest, SHA256_DIGEST_LENGTH)) {
            retval = entry;
            break;
        }
        i = (i + HASH_TABLE_OFFSET) % ejh->size;
    }
    pthread_rwlock_unlock(&ejh->rwl);
    return retval;
}

struct EjInodeHashEntry *
inode_hash_insert(struct EjInodeHash *ejh, const unsigned char *digest)
{
    unsigned long long val;
    memcpy(&val, digest, sizeof(val));
    struct EjInodeHashEntry *retval = NULL;

    pthread_rwlock_rdlock(&ejh->rwl);
    int i = (int)(val % ejh->size);
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[i];
        if (!entry->inode) {
            // not found
            break;
        }
        if (!memcmp(entry->digest, digest, SHA256_DIGEST_LENGTH)) {
            retval = entry;
            break;
        }
        i = (i + HASH_TABLE_OFFSET) % ejh->size;
    }
    pthread_rwlock_unlock(&ejh->rwl);
    if (retval) return retval;
    // insert an entry
    pthread_rwlock_wrlock(&ejh->rwl);
    if ((ejh->used + 1) * 2 >= ejh->size) {
        // create new hashtable and copy its content
        int new_size = primes[ejh->size_idx++];
        if (new_size <= 0) abort(); // oops
        struct EjInodeHashEntry *entries = calloc(new_size, sizeof(entries[0]));
        for (int i = 0; i < ejh->size; ++i) {
            struct EjInodeHashEntry *src_entry = &ejh->entries[i];
            if (src_entry->inode) {
                unsigned long long src_val;
                memcpy(&src_val, src_entry->digest, sizeof(src_val));

                int j = (int)(src_val % new_size);
                while (1) {
                    struct EjInodeHashEntry *dst_entry = &entries[j];
                    if (!dst_entry->inode) {
                        *dst_entry = *src_entry;
                        break;
                    }
                    // check
                    if (!memcmp(src_entry->digest, dst_entry->digest, SHA256_DIGEST_LENGTH)) abort();
                    j = (j + HASH_TABLE_OFFSET) % new_size;
                }
            }
        }
        free(ejh->entries);
        ejh->entries = entries;
        ejh->size = new_size;
    }
    // now insert new hash
    i = (int)(val % ejh->size);
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[i];
        if (!entry->inode) {
            memcpy(entry->digest, digest, SHA256_DIGEST_LENGTH);
            entry->inode = ejh->serial++;
            retval = entry;
            ++ejh->used;
            break;
        }
        if (!memcmp(entry->digest, digest, SHA256_DIGEST_LENGTH)) {
            retval = entry;
            break;
        }
        i = (i + HASH_TABLE_OFFSET) % ejh->size;
    }
    pthread_rwlock_unlock(&ejh->rwl);
    return retval;
}

void
inode_hash_delete(struct EjInodeHash *ejh, const unsigned char *digest)
{
    unsigned long long val;
    memcpy(&val, digest, sizeof(val));

    pthread_rwlock_wrlock(&ejh->rwl);
    int i = (int)(val % ejh->size);
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[i];
        if (!entry->inode) {
            goto done;
        }
        if (!memcmp(entry->digest, digest, SHA256_DIGEST_LENGTH)) {
            break;
        }
        i = (i + HASH_TABLE_OFFSET) % ejh->size;
    }
    // count the number of chained entries
    int j = (i + HASH_TABLE_OFFSET) % ejh->size;
    int chained_count = 0;
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[j];
        if (!entry->inode) {
            break;
        }
        ++chained_count;
        j = (j + HASH_TABLE_OFFSET) % ejh->size;
    }
    // clear entry
    memset(&ejh->entries[i], 0, sizeof(ejh->entries[0]));
    // save chained nodes
    if (chained_count > 100) abort(); // oops
    struct EjInodeHashEntry *saved = alloca(sizeof(*saved) * chained_count);
    int k = 0;
    j = (i + HASH_TABLE_OFFSET) % ejh->size;
    while (1) {
        struct EjInodeHashEntry *entry = &ejh->entries[j];
        if (!entry->inode) {
            break;
        }
        memcpy(&saved[k++], entry, sizeof(*entry));
        memset(entry, 0, sizeof(*entry));
        j = (j + HASH_TABLE_OFFSET) % ejh->size;
    }
    // rehash saved nodes
    for (k = 0; k < chained_count; ++k) {
        struct EjInodeHashEntry *src_entry = &saved[k];
        unsigned long long src_val;
        memcpy(&src_val, src_entry->digest, sizeof(src_val));
        j = (int)(src_val % ejh->size);
        while (1) {
            struct EjInodeHashEntry *dst_entry = &ejh->entries[j];
            if (!dst_entry->inode) {
                *dst_entry = *src_entry;
                break;
            }
            // check
            if (!memcmp(src_entry->digest, dst_entry->digest, SHA256_DIGEST_LENGTH)) abort();
            j = (j + HASH_TABLE_OFFSET) % ejh->size;
        }
    }
    
    --ejh->used;
done:
    pthread_rwlock_unlock(&ejh->rwl);
}

