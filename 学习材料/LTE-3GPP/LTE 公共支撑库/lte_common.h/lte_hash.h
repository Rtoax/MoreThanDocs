/*******************************************************************************
 **
 ** Copyright (c) 2005-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_hash.h
 ** Description:
 **
 ** Current Version: 0.0
 ** Revision: 0.0.0.0
 ** Author: zhanglei(zhanglei02@ict.ac.cn)
 ** Date: 2010-4-11
 **
 ******************************************************************************/

#ifndef LTE_HASH_H
#define LTE_HASH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lte_type.h"
#include "lte_lock.h"
#include "lte_task.h"
#include "lte_log.h"
#include "lte_malloc.h"

/*
 * This code is a fairly straightforward hash
 * table implementation using Bob Jenkins'
 * hash function.
 *
 * Hash tables are used in OpenVPN to keep track of
 * client instances over various key spaces.
 */

/* define this to enable special list test mode */
/*#define LIST_TEST*/

#define hashsize(n) ((UINT32)1<<(n))
#define hashmask(n) (hashsize(n)-1)

#define ALLOC_OBJ(dptr, type) \
	{ \
	(dptr) = (type *) calloc (1, sizeof (type)); \
	}
#define ALLOC_ARRAY(dptr, type, n) \
{ \
  (dptr) = (type *) calloc ((n), sizeof (type)); \
}


struct hash_element
{
    void *value;
    const void *key;
    unsigned int hash_value;
    struct hash_element *next;
};

struct hash_bucket
{
    RWLOCK_DEFINE (mutex);
    struct hash_element *list;
};

struct hash
{
    int n_buckets;
    int n_elements;
    int mask;
    UINT32 iv;
    UINT32 (*hash_function)(const void *key, UINT32 iv);
    int (*compare_function)(const void *key1, const void *key2); /* return true if equal */
    struct hash_bucket *buckets;
};

struct hash *hash_init(const int n_buckets, const UINT32 iv,
        UINT32(*hash_function)(const void *key, UINT32 iv),
        int(*compare_function)(const void *key1, const void *key2));

void hash_free(struct hash *hash);
int hash_add(struct hash *hash, const void *key, void *value, int replace);

struct hash_element *hash_lookup_fast(struct hash *hash,
        struct hash_bucket *bucket, const void *key, UINT32 hv);
int hash_remove_fast(struct hash *hash, struct hash_bucket *bucket,
        const void *key, UINT32 hv);

void hash_remove_by_value(struct hash *hash, void *value, int autolock);

struct hash_iterator
{
    struct hash *hash;
    int bucket_index;
    struct hash_bucket *bucket;
    struct hash_element *elem;
    struct hash_element *last;
    int bucket_marked;
    int autolock;
    int bucket_index_start;
    int bucket_index_end;
};

void hash_iterator_init_range(struct hash *hash, struct hash_iterator *hi,
        int autolock, int start_bucket, int end_bucket);

void hash_iterator_init(struct hash *hash, struct hash_iterator *iter,
        int autolock);
struct hash_element *hash_iterator_next(struct hash_iterator *hi);
void hash_iterator_delete_element(struct hash_iterator *hi);
void hash_iterator_free(struct hash_iterator *hi);
UINT32 hash_func(const UINT8 *k, UINT32 length, UINT32 initval);
UINT32 void_ptr_hash_function(const void *key, UINT32 iv);
int void_ptr_compare_function(const void *key1, const void *key2);

static inline UINT32 hash_value(const struct hash *hash, const void *key)
{
    return (*hash->hash_function)(key, hash->iv);
}

static inline int hash_n_elements(const struct hash *hash)
{
    return hash->n_elements;
}

static inline int hash_n_buckets(const struct hash *hash)
{
    return hash->n_buckets;
}

static inline struct hash_bucket *
hash_bucket(struct hash *hash, UINT32 hv)
{
    return &hash->buckets[hv & hash->mask];
}


static inline void *
hash_lookup_lock(struct hash *hash, const void *key, UINT32 hv)
{
    void *ret = NULL;
    struct hash_element *he;
    struct hash_bucket *bucket = &hash->buckets[hv & hash->mask];

    rwlock_rdlock (&bucket->mutex);
    he = hash_lookup_fast(hash, bucket, key, hv);
    if (he)
        ret = he->value;
    rwlock_unlock (&bucket->mutex);

    return ret;
}

static inline void *
hash_lookup(struct hash *hash, const void *key)
{
    return hash_lookup_lock(hash, key, hash_value(hash, key));
}

/* NOTE: assumes that key is not a duplicate */
static inline void hash_add_fast(struct hash *hash, struct hash_bucket *bucket,
        const void *key, UINT32 hv, void *value)
{
    struct hash_element *he;

    ALLOC_OBJ (he, struct hash_element);
    he->value = value;
    he->key = key;
    he->hash_value = hv;
    he->next = bucket->list;
    bucket->list = he;
    ++hash->n_elements;
}

static inline int hash_remove(struct hash *hash, const void *key)
{
    UINT32 hv;
    struct hash_bucket *bucket;
    int ret;

    hv = hash_value(hash, key);
    bucket = &hash->buckets[hv & hash->mask];
    rwlock_wrlock (&bucket->mutex);
    ret = hash_remove_fast(hash, bucket, key, hv);
    rwlock_unlock (&bucket->mutex);
    return ret;
}

#endif /* LTE_HASH_H */
