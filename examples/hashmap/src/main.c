#include "oryx.h"

void *hashmap;
int nr_max_elements = 64, nr_elements = 4;
#define HM_KEY_LENGTH	64

static __oryx_always_inline__
uint32_t hashmap_hashkey
(
	IN hm_key_t k
)
{
	uint8_t* p, *k0 = (uint8_t *)k;
	uint32_t hv = 0;

	for (p = k0; *p != '\0'; ++ p) {
		hv *= 12345;
		hv += *p;
	}
	
	return hv;
}

static __oryx_always_inline__
int hashmap_cmpkey
(
	IN hm_key_t k0,
	IN hm_key_t k1
)
{
	return !strcmp(k0, k1);
}

static __oryx_always_inline__
void hashmap_freekey
(
	IN hm_key_t k,
	IN hm_val_t v
)
{
	free(k);
	free(v);
}

static void hashmap_test_create(void)
{
	void *hashmap;

	oryx_hashmap_new("HASHMAP", nr_max_elements,
		nr_elements,
		HASHMAP_ENABLE_UPDATE,
		hashmap_hashkey,
		hashmap_cmpkey,
		hashmap_freekey,
		&hashmap);

	oryx_hashmap_print(hashmap);
	oryx_hashmap_destroy(hashmap);
}


static void hashmap_test_put_get(void)
{
	int i, err;
	char *key, keys[HM_KEY_LENGTH];
	char *val, *evicted;
	void *hashmap;

	oryx_hashmap_new("HASHMAP", nr_max_elements,
		nr_elements,
		HASHMAP_ENABLE_UPDATE,
		hashmap_hashkey,
		hashmap_cmpkey,
		hashmap_freekey,
		&hashmap);
	
	for (i = 0; i < nr_elements; i ++) {
		key = (char *)malloc(HM_KEY_LENGTH);
		val = (char *)malloc(HM_KEY_LENGTH);
		sprintf(key, "%d", i);
		sprintf(val, "value%d", i); 
		err = oryx_hashmap_put(hashmap, (hm_key_t)key, (hm_val_t)val, (hm_val_t *)&evicted);
		if (err) {
			fprintf(stdout, "line%d, put(%s) err(%s), %s\n",
				__LINE__, key, oryx_hashmap_strerror(err), val);
			free(key);
			free(val);
		}
	}
	oryx_hashmap_print(hashmap);

	/* hit */
	for (i = 0; i < nr_elements; i ++) {
		sprintf(keys, "%d", i);
		err = oryx_hashmap_get(hashmap, (hm_key_t)keys, (hm_val_t *)&val);
		if (err) {
			fprintf(stdout, "line%d, get(%s) err(%s)\n",
				__LINE__, key, oryx_hashmap_strerror(err));
		}
	}

	oryx_hashmap_destroy(hashmap);
}


static void hashmap_test_del(void)
{
	int i, err;
	char *key, keys[HM_KEY_LENGTH];
	char *val, *evicted;
	void *hashmap;

	oryx_hashmap_new("HASHMAP", nr_max_elements,
		nr_elements,
		HASHMAP_ENABLE_UPDATE,
		hashmap_hashkey,
		hashmap_cmpkey,
		hashmap_freekey,
		&hashmap);

	for (i = 0; i < nr_elements; i ++) {
		key = (char *)malloc(HM_KEY_LENGTH);
		val = (char *)malloc(HM_KEY_LENGTH);
		sprintf(key, "%d", i);
		sprintf(val, "value%d", i); 
		err = oryx_hashmap_put(hashmap, (hm_key_t)key, (hm_val_t)val, (hm_val_t *)&evicted);
		if (err) {
			fprintf(stdout, "line%d, put(%s) err(%s), %s\n",
				__LINE__, key, oryx_hashmap_strerror(err), val);
			free(key);
			free(val);
		}
	}
	oryx_hashmap_print(hashmap);
	
	for (i = 0; i < nr_elements; i ++) {
		sprintf(keys, "%d", i);
		err = oryx_hashmap_del(hashmap, keys);
		if (err) {
			fprintf(stdout, "line%d, del(%s) err(%s)\n",
				__LINE__, key, oryx_hashmap_strerror(err));
		}
	}
	oryx_hashmap_print(hashmap);

	oryx_hashmap_destroy(hashmap);
}

static void hashmap_test_resize(void)
{
	int i, err;
	char *key, keys[HM_KEY_LENGTH];
	char *val, *evicted;
	void *hashmap;

	oryx_hashmap_new("HASHMAP", nr_max_elements,
		nr_elements,
		HASHMAP_ENABLE_UPDATE,
		hashmap_hashkey,
		hashmap_cmpkey,
		hashmap_freekey,
		&hashmap);

	for (i = 0; i < nr_elements; i ++) {
		key = (char *)malloc(HM_KEY_LENGTH);
		val = (char *)malloc(HM_KEY_LENGTH);
		sprintf(key, "%d", i);
		sprintf(val, "value%d", i); 

		err = oryx_hashmap_put(hashmap, (hm_key_t)key, (hm_val_t)val, (hm_val_t *)&evicted);
		if (err) {
			fprintf(stdout, "line%d, put(%s) err(%s), %s\n",
				__LINE__, key, oryx_hashmap_strerror(err), val);
			free(key);
			free(val);
		}
	}
	oryx_hashmap_resize(hashmap, 2 * oryx_hashmap_curr_buckets(hashmap));

	/* hit */
	for (i = 0; i < nr_elements; i ++) {
		sprintf(keys, "%d", i);
		err = oryx_hashmap_get(hashmap, (hm_key_t)keys, (hm_val_t *)&val);
		if (err) {
			fprintf(stdout, "line%d, get(%s) err(%s)\n",
				__LINE__, key, oryx_hashmap_strerror(err));
		}
	}

	oryx_hashmap_print(hashmap);
	
	oryx_hashmap_destroy(hashmap);
}

int main (
	int 	__oryx_unused__	argc,
	char	__oryx_unused__	** argv
)
{
	oryx_initialize();

	hashmap_test_create();
	hashmap_test_put_get();
	hashmap_test_del();
	hashmap_test_resize();
}

