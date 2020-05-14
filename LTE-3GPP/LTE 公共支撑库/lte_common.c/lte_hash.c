/*******************************************************************************
 **
 ** Copyright (c) 2005-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: fg_hash.c
 ** Description:
 **
 ** Current Version: 0.0
 ** Revision: 0.0.0.0
 ** Author: zhang lei(zhanglei02@ict.ac.cn)
 ** Date: 2010-4-11
 **
 ******************************************************************************/
#include "lte_hash.h"

#define true     1
#define false    0

int adjust_power_of_2(int u)
{
	int ret = 1;

	while (ret < u) {
		ret <<= 1;
	}

	return ret;
}

struct hash *
hash_init(const int n_buckets, const UINT32 iv,
		UINT32(*hash_function)(const void *key, UINT32 iv),
		int(*compare_function)(const void *key1, const void *key2))
{
	struct hash *h;
	int i;

	if (n_buckets <= 0)
		return NULL;

	ALLOC_OBJ(h, struct hash);
	h->n_buckets = adjust_power_of_2(n_buckets);
	h->mask = h->n_buckets - 1;
	h->hash_function = hash_function;
	h->compare_function = compare_function;
	h->iv = iv;
	ALLOC_ARRAY(h->buckets, struct hash_bucket, h->n_buckets);
	for (i = 0; i < h->n_buckets; ++i) {
		struct hash_bucket *b = &h->buckets[i];
		b->list = NULL;
		rwlock_init(&b->mutex);
	}
	return h;
}

void hash_free(struct hash *hash)
{
	int i;
	for (i = 0; i < hash->n_buckets; ++i) {
		struct hash_bucket *b = &hash->buckets[i];
		struct hash_element *he = b->list;

		rwlock_destroy(&b->mutex);
		while (he) {
			struct hash_element *next = he->next;
			free(he);
			he = next;
		}
	}
	free(hash->buckets);
	free(hash);
}

struct hash_element *
hash_lookup_fast(struct hash *hash, struct hash_bucket *bucket, const void *key,
		UINT32 hv)
{
	struct hash_element *he;
	struct hash_element *prev = NULL;

	he = bucket->list;

	while (he) {
		if (hv == he->hash_value
				&& (*hash->compare_function)(key, he->key)) {
			/* move to head of list */
			if (prev) {
				prev->next = he->next;
				he->next = bucket->list;
				bucket->list = he;
			}
			return he;
		}
		prev = he;
		he = he->next;
	}

	return NULL;
}

int hash_remove_fast(struct hash *hash, struct hash_bucket *bucket,
		const void *key, UINT32 hv)
{
	struct hash_element *he;
	struct hash_element *prev = NULL;

	he = bucket->list;

	while (he) {
		if (hv == he->hash_value
				&& (*hash->compare_function)(key, he->key)) {
			if (prev)
				prev->next = he->next;
			else
				bucket->list = he->next;
			free(he);
			--hash->n_elements;
			return true;
		}
		prev = he;
		he = he->next;
	}
	return false;
}

int hash_add(struct hash *hash, const void *key, void *value, int replace)
{
	UINT32 hv;
	struct hash_bucket *bucket;
	struct hash_element *he;
	int ret = false;

	hv = hash_value(hash, key);
	bucket = &hash->buckets[hv & hash->mask];
	rwlock_wrlock(&bucket->mutex);

	if ((he = hash_lookup_fast(hash, bucket, key, hv))) /* already exists? */
	{
		if (replace) {
			he->value = value;
			ret = true;
		}
	} else {
		hash_add_fast(hash, bucket, key, hv, value);
		ret = true;
	}

	rwlock_unlock(&bucket->mutex);

	return ret;
}

void hash_remove_by_value(struct hash *hash, void *value, int autolock)
{
	struct hash_iterator hi;
	struct hash_element *he;

	hash_iterator_init(hash, &hi, autolock);
	while ((he = hash_iterator_next(&hi))) {
		if (he->value == value)
			hash_iterator_delete_element(&hi);
	}
	hash_iterator_free(&hi);
}

static void hash_remove_marked(struct hash *hash, struct hash_bucket *bucket)
{
	struct hash_element *prev = NULL;
	struct hash_element *he = bucket->list;

	while (he) {
		if (!he->key) /* marked? */
		{
			struct hash_element *newhe;
			if (prev)
				newhe = prev->next = he->next;
			else
				newhe = bucket->list = he->next;
			free(he);
			--hash->n_elements;
			he = newhe;
		} else {
			prev = he;
			he = he->next;
		}
	}
}

UINT32 void_ptr_hash_function(const void *key, UINT32 iv)
{
	return hash_func((const void *) &key, sizeof(key), iv);
}

int void_ptr_compare_function(const void *key1, const void *key2)
{
	return key1 == key2;
}

void hash_iterator_init_range(struct hash *hash, struct hash_iterator *hi,
		int autolock, int start_bucket, int end_bucket)
{
	if (end_bucket > hash->n_buckets)
		end_bucket = hash->n_buckets;

	hi->hash = hash;
	hi->elem = NULL;
	hi->bucket = NULL;
	hi->autolock = autolock;
	hi->last = NULL;
	hi->bucket_marked = false;
	hi->bucket_index_start = start_bucket;
	hi->bucket_index_end = end_bucket;
	hi->bucket_index = hi->bucket_index_start - 1;
}

void hash_iterator_init(struct hash *hash, struct hash_iterator *hi,
		int autolock)
{
	hash_iterator_init_range(hash, hi, autolock, 0, hash->n_buckets);
}

static inline void hash_iterator_lock(struct hash_iterator *hi,
		struct hash_bucket *b)
{
	if (hi->autolock) {
		rwlock_wrlock(&b->mutex);
	}
	hi->bucket = b;
	hi->last = NULL;
	hi->bucket_marked = false;
}

static inline void hash_iterator_unlock(struct hash_iterator *hi)
{
	if (hi->bucket) {
		if (hi->bucket_marked) {
			hash_remove_marked(hi->hash, hi->bucket);
			hi->bucket_marked = false;
		}
		if (hi->autolock) {
			rwlock_unlock(&hi->bucket->mutex);
		}
		hi->bucket = NULL;
		hi->last = NULL;
	}
}

static inline void hash_iterator_advance(struct hash_iterator *hi)
{
	hi->last = hi->elem;
	hi->elem = hi->elem->next;
}

void hash_iterator_free(struct hash_iterator *hi)
{
	hash_iterator_unlock(hi);
}

struct hash_element *
hash_iterator_next(struct hash_iterator *hi)
{
	struct hash_element *ret = NULL;
	if (hi->elem) {
		ret = hi->elem;
		hash_iterator_advance(hi);
	} else {
		while (++hi->bucket_index < hi->bucket_index_end) {
			struct hash_bucket *b;
			hash_iterator_unlock(hi);
			b = &hi->hash->buckets[hi->bucket_index];
			if (b->list) {
				hash_iterator_lock(hi, b);
				hi->elem = b->list;
				if (hi->elem) {
					ret = hi->elem;
					hash_iterator_advance(hi);
					break;
				}
			}
		}
	}
	return ret;
}

void hash_iterator_delete_element(struct hash_iterator *hi)
{
	hi->last->key = NULL;
	hi->bucket_marked = true;
}

/*
 --------------------------------------------------------------------
 hash() -- hash a variable-length key into a 32-bit value
 k     : the key (the unaligned variable-length array of bytes)
 len   : the length of the key, counting by bytes
 level : can be any 4-byte value
 Returns a 32-bit value.  Every bit of the key affects every bit of
 the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 About 36+6len instructions.

 The best hash table sizes are powers of 2.  There is no need to do
 mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 use a bitmask.  For example, if you need only 10 bits, do
 h = (h & hashmask(10));
 In which case, the hash table should have hashsize(10) elements.

 If you are hashing n strings (uint8_t **)k, do it like this:
 for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

 By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
 code any way you wish, private, educational, or commercial.  It's free.

 See http://burlteburtle.net/bob/hash/evahash.html
 Use for hash table lookup, or anything where one collision in 2^32 is
 acceptable.  Do NOT use for cryptographic purposes.

 --------------------------------------------------------------------

 mix -- mix 3 32-bit values reversibly.
 For every delta with one or two bit set, and the deltas of all three
 high bits or all three low bits, whether the original value of a,b,c
 is almost all zero or is uniformly distributed,
 * If mix() is run forward or backward, at least 32 bits in a,b,c
 have at least 1/4 probability of changing.
 * If mix() is run forward, every bit of c will change between 1/3 and
 2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 mix() was built out of 36 single-cycle latency instructions in a
 structure that could supported 2x parallelism, like so:
 a -= b;
 a -= c; x = (c>>13);
 b -= c; a ^= x;
 b -= a; x = (a<<8);
 c -= a; b ^= x;
 c -= b; x = (b>>13);
 ...
 Unfortunately, superscalar Pentiums and Sparcs can't take advantage
 of that parallelism.  They've also turned some of those single-cycle
 latency instructions into multi-cycle latency instructions.  Still,
 this is the fastest good hash I could find.  There were about 2^^68
 to choose from.  I only looked at a billion or so.

 James Yonan Notes:

 * This function is faster than it looks, and appears to be
 appropriate for our usage in OpenVPN which is primarily
 for hash-table based address lookup (IPv4, IPv6, and Ethernet MAC).
 NOTE: This function is never used for cryptographic purposes, only
 to produce evenly-distributed indexes into hash tables.

 * Benchmark results: 11.39 machine cycles per byte on a P2 266Mhz,
 and 12.1 machine cycles per byte on a
 2.2 Ghz P4 when hashing a 6 byte string.
 --------------------------------------------------------------------
 */

#define mix(a,b,c)               \
{                                \
  a -= b; a -= c; a ^= (c>>13);  \
  b -= c; b -= a; b ^= (a<<8);   \
  c -= a; c -= b; c ^= (b>>13);  \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16);  \
  c -= a; c -= b; c ^= (b>>5);   \
  a -= b; a -= c; a ^= (c>>3);   \
  b -= c; b -= a; b ^= (a<<10);  \
  c -= a; c -= b; c ^= (b>>15);  \
}

UINT32 hash_func(const UINT8 *k, UINT32 length, UINT32 initval)
{
	UINT32 a, b, c, len;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9; /* the golden ratio; an arbitrary value */
	c = initval; /* the previous hash value */

	/*---------------------------------------- handle most of the key */
	while (len >= 12) {
		a += (k[0] + ((UINT32) k[1] << 8) + ((UINT32) k[2] << 16)
				+ ((UINT32) k[3] << 24));
		b += (k[4] + ((UINT32) k[5] << 8) + ((UINT32) k[6] << 16)
				+ ((UINT32) k[7] << 24));
		c += (k[8] + ((UINT32) k[9] << 8) + ((UINT32) k[10] << 16)
				+ ((UINT32) k[11] << 24));
		mix(a, b, c);
		k += 12;
		len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	c += length;
	switch (len)
	/* all the case statements fall through */
	{
	case 11:
		c += ((UINT32) k[10] << 24);
	case 10:
		c += ((UINT32) k[9] << 16);
	case 9:
		c += ((UINT32) k[8] << 8);
		/* the first byte of c is reserved for the length */
	case 8:
		b += ((UINT32) k[7] << 24);
	case 7:
		b += ((UINT32) k[6] << 16);
	case 6:
		b += ((UINT32) k[5] << 8);
	case 5:
		b += k[4];
	case 4:
		a += ((UINT32) k[3] << 24);
	case 3:
		a += ((UINT32) k[2] << 16);
	case 2:
		a += ((UINT32) k[1] << 8);
	case 1:
		a += k[0];
		/* case 0: nothing left to add */
	}mix(a, b, c);
	/*-------------------------------------- report the result */
	return c;
}

