/*!
 * @file hashmap.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

#define HM_LOCK(hm)\
		oryx_sys_mutex_lock(&(hm)->mtx);

#define HM_UNLOCK(hm)\
		oryx_sys_mutex_unlock(&(hm)->mtx);

extern unsigned long clac_crc32
(
	IN const unsigned char *s,
	IN unsigned int len
);

/*
 * Hashing function for a string
 */
static __oryx_always_inline__
unsigned int hashmap_hashkey
(
	IN hm_key_t key
)
{
	unsigned long k = clac_crc32((unsigned char*)(key), strlen(key));

	/* Robert Jenkins' 32 bit Mix Function */
	k += (k << 12);
	k ^= (k >> 22);
	k += (k << 4);
	k ^= (k >> 9);
	k += (k << 10);
	k ^= (k >> 2);
	k += (k << 7);
	k ^= (k >> 12);

	/* Knuth's Multiplicative Method */
	k = (k >> 3) * 2654435761;

	return k;
}

/*
 * Hashing function for a string
 */
static __oryx_always_inline__
int hashmap_cmpkey
(
	IN hm_key_t k0,
	IN hm_key_t k1
)
{
	k0 = k0;
	k1 = k1;

	return !strcmp(k0, k1);
}

/*
 * Hashing function for a string
 */
static __oryx_always_inline__
void hashmap_freekey
(
	IN hm_key_t k,
	IN hm_val_t v
)
{
	k = k;
	v = v;
}

static __oryx_always_inline__
void **hashmap_alloc_table(int nr_max_buckets)
{
	void **new;
	
	new = (void **)malloc(
			sizeof(struct oryx_hashmap_bucket_t *) * nr_max_buckets);
	if (unlikely(!new)) {
		return NULL;
	}
	
	memset(new, 0,
		sizeof(struct oryx_hashmap_bucket_t *) * nr_max_buckets);
	return new;
}

static __oryx_always_inline__
void hashmap_insert_entry
(
	IN struct oryx_hashmap_t *hashmap,
	IN int hash,
	IN hm_key_t key,
	IN hm_val_t val
)
{
	struct oryx_hashmap_bucket_t *b;
	
	b = (struct oryx_hashmap_bucket_t *)malloc(sizeof(struct oryx_hashmap_bucket_t));
	if (unlikely(!b))
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));
	memset (b, 0, sizeof(struct oryx_hashmap_bucket_t));
	b->key = key;
	b->value = val;

	if (hashmap->array[hash] == NULL) {
		hashmap->array[hash] = b;
	} else {
		b->next = hashmap->array[hash];
		hashmap->array[hash] = b;
	}
	hashmap->nr_elements ++;
}

/**
 *  \brief Create a new HashMap
 *
 *  \param hm_name, the HashMap alias
 *  \param hm_max_elements, supported maximum elements
 *  \param hm_max_chains, default is HASHMAP_DEFAULT_CHAINS
 *  \param hm_cfg, configurations
 *  \param hashkey_fn, user specified @key hash function
 *  \param cmpkey_fn, user specified @key compare function
 *  \param hashmap, the HashMap context
 */
__oryx_always_extern__
void oryx_hashmap_new
(
	IN const char *hm_name,
	IN uint32_t hm_max_elements,
	IN uint32_t hm_max_buckets,
	IN uint32_t hm_cfg,
	IN uint32_t (*hashkey_fn)(const hm_key_t),
	IN int (*cmpkey_fn)(const hm_key_t, const hm_key_t),
	IN void (*freekey_fn)(const hm_key_t, const hm_val_t),
	OUT void ** hashmap
)
{
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)malloc(sizeof(struct oryx_hashmap_t));

	if (unlikely(!hmctx))
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));

	memset(hmctx, 0, sizeof(struct oryx_hashmap_t));
	hmctx->name		=	hm_name;
	hmctx->nr_max_elements	=	hm_max_elements;
	hmctx->nr_max_buckets	=	hm_max_buckets ? hm_max_buckets : HASHMAP_DEFAULT_CHAINS;
	hmctx->ul_flags		=	hm_cfg;
	hmctx->hashkey_fn	=	hashkey_fn ? hashkey_fn : hashmap_hashkey;
	hmctx->cmpkey_fn	=	cmpkey_fn  ? cmpkey_fn  : hashmap_cmpkey;
	hmctx->freekey_fn	=	freekey_fn ? freekey_fn : hashmap_freekey;
	hmctx->nr_elements	=	0;

	hmctx->array = (struct oryx_hashmap_bucket_t **)hashmap_alloc_table(hmctx->nr_max_buckets);
	if (unlikely(!hmctx->array)) {
		/* recycle hmctx */
		free(hmctx);
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));
	}
	
#if defined(HAVE_HASHMAP_DEBUG)	
	hmctx->memory = sizeof (struct oryx_hashmap_t) + (hmctx->nr_max_buckets * (sizeof(struct oryx_hashmap_bucket_t*)));
#endif

	oryx_sys_mutex_create(&hmctx->mtx);

	(*hashmap) = hmctx;
	fprintf(stdout, "hashmap handler, %p\n", (*hashmap));
	fprintf(stdout, "   max elements, %u\n", hmctx->nr_max_elements);
	fprintf(stdout, "    max buckets, %u\n", hmctx->nr_max_buckets);
	fprintf(stdout, "       elements, %u\n", hmctx->nr_elements);
}

static __oryx_always_inline__
void oryx_hashmap_entry_free
(
	IN struct oryx_hashmap_t* hashmap
)
{
	int i;
	struct oryx_hashmap_bucket_t *b, *next;
	
	for (i = 0; i < (int)hashmap->nr_max_buckets; i ++) {
		next = b = hashmap->array[i];
		while(b) {
			next = b->next;
			hashmap->freekey_fn(b->key, b->value);
			free(b);
			b = next;
		}
	}
}

__oryx_always_extern__
const char * oryx_hashmap_strerror
(
	IN int err
)
{
	switch(err) {
		case HM_SUCCESS: return "HM_SUCCESS";
		case HM_FAILURE: return "HM_FAILURE";
		case HM_HIT    : return "HM_HIT";
		case HM_MISS   : return "HM_MISS";
		case HM_FULL   : return "HM_FULL";
		default        : return "Unknown error";
	}
}

__oryx_always_extern__
void oryx_hashmap_print
(
	IN void *hashmap
)
{
	int i, n;
	struct oryx_hashmap_bucket_t *b;
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)hashmap;

	fprintf (stdout, "\n----------- HashMap Summary (Usage %.2f%%) ------------\n", oryx_hashmap_usage(hmctx));
	fprintf (stdout, "Buckets:	%d\n", hmctx->nr_max_buckets);	
	for (i = 0; i < (int)hmctx->nr_max_buckets; i ++) {
		n = 0;
		b = hmctx->array[i];
		while(b) {
			if (b) n ++;
			b = b->next;
		}
		//fprintf (stdout, "  bucket[%d], %4d (%.2f%%)\n", i, n, ratio_of(n, hmctx->nr_max_elements));
	}
	fprintf (stdout, "-----------------------------------------\n");
}

/**
 *  \brief Allocate additional memory to accomodate a hash table with more
 *         buckets and a reduced load factor.
 *
 *  This is triggered when the current load factor reaches above 75%. That is,
 *  when the number of entries consumes most of the available buckets, it's
 *  time to rehash to avoid excessive chaining within each bucket. 
 *
 *  The existing entries are rehashed into the new buckets, and the previous
 *  bucket memory is released.
 *
 *  \param hashmap, the HashMap context
 *  \param nr_max_buckets, the new bucket number
 */
static __oryx_always_inline__
int oryx_hashmap_resize
(
	IN struct oryx_hashmap_t *hashmap,
	uint32_t nr_max_buckets
)
{
	int i;
	uint32_t hash;
	struct oryx_hashmap_t *newhashmap;
	struct oryx_hashmap_bucket_t *next, *b;

	newhashmap = (struct oryx_hashmap_t *)malloc(sizeof(struct oryx_hashmap_t));
	if (unlikely(!newhashmap))
		oryx_panic(0,
			"malloc: %s", oryx_safe_strerror(errno));
	
	newhashmap->array = (struct oryx_hashmap_bucket_t **)hashmap_alloc_table(nr_max_buckets);
	if (unlikely(!newhashmap))
		oryx_panic(0,
			"hashmap alloc table: %s", oryx_safe_strerror(errno));		

	/* remap entries to new hashmap */
	for (i = 0; i < (int)hashmap->nr_max_buckets; i ++) {
		b = hashmap->array[i];
		while (b) {
			/* rehash */
			hash = (hashmap->hashkey_fn(b->key) % nr_max_buckets);
			hashmap_insert_entry(newhashmap, hash, b->key, b->value);
			b = b->next;
		}
	}

	/* release old entries */
	for (i = 0; i < (int)hashmap->nr_max_buckets; i ++) {
		next = b = hashmap->array[i];
		while(b) {
			next = b->next;
			hashmap->freekey_fn(b->key, b->value);
			free(b);
			b = next;
		}
	}
	
	fprintf(stdout, "hashmap resize %4u -> %4u (%4u)\n", hashmap->nr_max_buckets, nr_max_buckets, hashmap->nr_elements);
	free(hashmap->array);
	hashmap->array = newhashmap->array;
	hashmap->nr_max_buckets = nr_max_buckets;
	hashmap->nr_elements = newhashmap->nr_elements;
	
	return HM_SUCCESS;
}

/**
 *  \brief Get element from HashMap by a given @key
 *
 *  \param hashmap, the HashMap context
 *  \param key, the key of the element
 *  \param val, the element
 */
__oryx_always_extern__
int oryx_hashmap_get
(
	IN void * hashmap,
	IN hm_key_t key,
	IN hm_val_t *val
)
{
	int hash;
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)hashmap;
	struct oryx_hashmap_bucket_t *b;
	
	/* Find the best index */
	hash = (hmctx->hashkey_fn(key) % hmctx->nr_max_buckets);

	HM_LOCK(hmctx);
    if (unlikely(!hmctx->array[hash])) {
		HM_UNLOCK(hmctx);
        return HM_MISS;
    }
	
	b = hmctx->array[hash];
	while(b) {
		if (hmctx->cmpkey_fn(b->key, key)) {
			(*val) = b->value;
			HM_UNLOCK(hmctx);
			return HM_SUCCESS;
		}
		b = b->next;
	}
	HM_UNLOCK(hmctx);

	return HM_MISS;
}


/**
 *  \brief Put element to HashMap by a given key
 *
 *  \param hashmap, the HashMap context
 *  \param key, the key of the element
 *  \param val, the element
 *  \param evicted, the evicted element used holding @value before updating it.
 */
__oryx_always_extern__
int oryx_hashmap_put
(
	IN void * hashmap,
	IN hm_key_t key,
	IN hm_val_t val,
	IO hm_val_t *evicted
)
{
	int err, hash;
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)hashmap;
	struct oryx_hashmap_bucket_t *b;

	/* Find the best index */
	hash = (hmctx->hashkey_fn(key) % hmctx->nr_max_buckets);

	b = hmctx->array[hash];
	while(b) {
		if (hmctx->cmpkey_fn(b->key, key)) {
			/* should we update the value here ? */
			if(hmctx->ul_flags & HASHMAP_ENABLE_UPDATE) {
				ASSERT(*evicted != NULL);
				(*evicted) = b->value;
				b->value = val;
			}
			return HM_SUCCESS;
		}
		b = b->next;
	}

	HM_LOCK(hmctx);
	hashmap_insert_entry(hmctx, hash, key, val);
	if (0 /**< not ready */) {
		if (oryx_hashmap_load(hmctx) > HASHMAP_MAX_LOAD_FACTOR) {
			err = oryx_hashmap_resize(hmctx, hmctx->nr_max_buckets * 2);
			if (err) {
				oryx_panic(0,
					"resize: %s", oryx_safe_strerror(errno));
			}
		}
	}
	HM_UNLOCK(hmctx);

	return HM_SUCCESS;
}

/**
 *  \brief Delete element from HashMap by a given @key
 *
 *  \param hashmap, the HashMap context
 *  \param key, the key of the element
 */
__oryx_always_extern__
int oryx_hashmap_del
(
	IN void * hashmap,
	IN hm_key_t key
)
{
	int hash;
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)hashmap;

	/* Find the best index */
	hash = (hmctx->hashkey_fn(key) % hmctx->nr_max_buckets);

	HM_LOCK(hmctx);
	if (hmctx->array[hash] == NULL) {
		HM_UNLOCK(hmctx);
		return HM_MISS;
	}

	if (hmctx->array[hash]->next == NULL) {
		if (hmctx->freekey_fn)
			hmctx->freekey_fn(hmctx->array[hash]->key, hmctx->array[hash]->value);
		free(hmctx->array[hash]);
		hmctx->array[hash] = NULL;
		hmctx->nr_elements --;
		HM_UNLOCK(hmctx);
		return HM_SUCCESS;
	}

	struct oryx_hashmap_bucket_t *b = hmctx->array[hash], *next = NULL;
	do {
		if (hmctx->cmpkey_fn(b->key, key)) {
			if (next == NULL) {
				/* root bucket */
				hmctx->array[hash] = b->next;
			} else {
				/* child bucket */
				next->next = b->next;
			}
			/* remove this */
			hmctx->freekey_fn(b->key, b->value);
			free(b);
			hmctx->nr_elements --;
			HM_UNLOCK(hmctx);
			return HM_SUCCESS;
		}

		next = b;
		b = b->next;

	} while (next != NULL);
	HM_UNLOCK(hmctx);

	return HM_MISS;
}

/**
 *  \brief Destroy a new HashMap
 *
 *  \param hashmap, the HashMap context
 */
__oryx_always_extern__
void oryx_hashmap_destroy
(
	IN void ** hashmap
)
{
	struct oryx_hashmap_t *hmctx = (struct oryx_hashmap_t *)hashmap;
	fprintf(stdout, "Destroy hashmap %s\n", hmctx->name);
	oryx_hashmap_entry_free(hmctx);	
	free(hmctx->array);
	oryx_sys_mutex_destroy(&hmctx->mtx);
	free(hmctx);
}


