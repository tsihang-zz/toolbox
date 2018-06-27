#ifndef MPOOL_H
#define MPOOL_H

extern void mpool_init(void ** mp, const char *mp_name,
			int nb_steps, int nb_bytes_per_element,
			struct element_fn_t *efn);

#endif

