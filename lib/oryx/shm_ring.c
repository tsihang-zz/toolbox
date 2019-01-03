/*!
 * @file shm_ring.c
 * @date 2017/08/29
 *
 * TSIHANG (haechime@gmail.com)
 */

#include "oryx.h"

#define SHMRING_BASE	0x123456

static __oryx_always_inline__
key_t shmring_key
(
	IN const char *v,
	IN uint32_t s
) 
{
     uint8_t *d = (uint8_t *)v;
     uint32_t i;
     uint16_t hv = 0;

     for (i = 0; i < s; i++)
	 	hv += d[i];

	 return (SHMRING_BASE + hv);
}

__oryx_always_extern__
int oryx_shmring_create
(
	IN const char *shmring_name,
	IN int __oryx_unused__ nr_elements,
	IN int __oryx_unused__ nr_element_size,
	IN uint32_t flags,
	OUT struct oryx_shmring_t **shmring
)
{
	BUG_ON(shmring_name == NULL);
	BUG_ON(shmring == NULL);
	int		err;
	key_t	key;	
	vlib_shm_t shm = {
		.addr	= 0,
		.shmid	= 0,
	};

	key	= shmring_key(shmring_name, strlen(shmring_name));
	err	= oryx_shm_get(key, sizeof(struct oryx_shmring_t), &shm);
	if (err) {
		fprintf(stdout, "Cannot create share ring handler\n");
		return -1;
	} else {
		(*shmring) = (struct oryx_shmring_t *)shm.addr;
		nr_elements = NR_SHMRING_ELEMENTS;
		if ((*shmring)->name[0] == '\0') {
			//r->name			= shmring_name;
			memcpy((*shmring)->name, shmring_name, strlen(shmring_name));
			(*shmring)->rp			= 0;
			(*shmring)->key 			= key;
			(*shmring)->key0			= shm.shmid;
			(*shmring)->wp			= nr_elements;
			(*shmring)->max_elements = nr_elements;
			(*shmring)->ul_flags 	= flags;
			fprintf(stdout, "Try init share ring \"%s\" done\n", (*shmring)->name);
		} else {
			if (strcmp(shmring_name, (*shmring)->name)) {
				fprintf(stdout, "error: expected %s, but %s\n", shmring_name, (*shmring)->name);
				exit(0);
			}
		}
	}
	return 0;	
}

__oryx_always_extern__
void oryx_shmring_dump
(
	IN struct oryx_shmring_t *shmring
)
{
	
	fprintf (stdout, "shmring %s\n", shmring->name);
	fprintf (stdout, "%16s%32d\n", "shmid: ",		shmring->key0);
	fprintf (stdout, "%16s%32d\n", "nb_data: ",		shmring->max_elements);
	fprintf (stdout, "%16s%32lu\n", "rp_times: ",	shmring->nr_times_rd);
	fprintf (stdout, "%16s%32lu\n", "wp_times: ",	shmring->nr_times_wr);
	fprintf (stdout, "%16s%32lu\n", "full_times: ",	shmring->nr_times_f);		
	fprintf (stdout, "%16s%32lu\n", "rp: ",			shmring->rp);
	fprintf (stdout, "%16s%32lu\n", "wp: ",			shmring->wp);
}

__oryx_always_extern__
int oryx_shmring_destroy
(
	IN struct oryx_shmring_t *shmring
)
{
	int err;
	vlib_shm_t shm = {
		.addr = (uint64_t)shmring,
		.shmid = shmring->key0,
	};

	err = oryx_shm_destroy(&shm);
	if (err) {
		fprintf(stdout, "destroy shmring \"%s\"\n", shmring->name);
		return -1;
	}	
	return 0;
}

