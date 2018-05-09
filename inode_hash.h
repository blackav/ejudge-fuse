#pragma once

struct EjInodeHashEntry
{
    unsigned inode;
    unsigned char digest[32]; // SHA256_DIGEST_LENGTH
};

struct EjInodeHash;

struct EjInodeHash *inode_hash_create(void);
void inode_hash_free(struct EjInodeHash *ejh);

struct EjInodeHashEntry *inode_hash_find(struct EjInodeHash *ejh, const unsigned char *digest);
struct EjInodeHashEntry *inode_hash_insert(struct EjInodeHash *ejh, const unsigned char *digest);
void inode_hash_delete(struct EjInodeHash *ejh, const unsigned char *digest);
