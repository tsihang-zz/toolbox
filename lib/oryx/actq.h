#ifndef __ACTQ_H__
#define __ACTQ_H__


typedef uint32_t oryx_id_t;
typedef void *raw_t;

typedef struct oryx_actq_raw_t {
#define RAW(raw)	(raw)
#define RAW_DATA(raw) ((raw)->v)
#define RAW_SIZE(raw) ((raw)->l)
	u8 t;
	oryx_size_t l;
	raw_t v;

	uint32_t ul_flags;
	struct list_head node;
} oryx_actq_raw_t;

typedef enum ACTQ_OPTIONS {
	ACTQ_RQ,
	ACTQ_WQ,
	ACTQ_RW,
}ACTQ_OPTIONS;

typedef struct oryx_actq_prefix_t {
	/** Alias for this queue. */
	char sc_alias[32];

	uint32_t ul_alias_size;

	/** Priority for this queue. */
	uint32_t ul_prio;

	uint32_t ul_flags;

	/* maximum buffered elements. 
	 * effect while (ul_flags & ACTQ_ENABLE_FIXED_BUFFER) */
	uint32_t ul_buffered_refcnt;
	
} oryx_actq_prefix_t;

typedef struct oryx_actq_t {

/** readable queue. */
#define ACTQ_ENABLE_READ	(1 << 0)

/** writable queue. */
#define ACTQ_ENABLE_WRITE 	(1 << 1)

/** trigger method. */
#define ACTQ_ENABLE_TRIGGER	(1 << 2)

/* all message stored in a fixed buffer (ul_cache_size). 
 * if buffer full, new arrived element is dropped.
 * count for this case is record by ul_drop_refcnt. */
#define ACTQ_ENABLE_FIXED_BUFFER	(1 << 3)


#define ACTQ_ENABLE_DEFAULT		(ACTQ_ENABLE_READ | ACTQ_ENABLE_WRITE | ACTQ_ENABLE_TRIGGER)

	/** Alias for this queue. */
	char sc_alias[32];

	oryx_size_t ul_alias_size;

	/** Priority for this queue. */
	uint32_t ul_prio;

	/** Unique identify for this queue. */
	oryx_id_t ul_id;
	
	/** Raw data cache size. default is RAW_DATA_N_ELEMENTS
	 * inner raw unlimited, use whole RAM. 
	 * this case is simple. */
	uint32_t ul_cache_size;

	/** Up-limit threshold */
	uint32_t ul_threshold_max;

	/** Low-limit threshold */
	uint32_t ul_threshold_min;
	
	/** Critical lock for this queue. */
	os_mutex_t *ol_actq_lock;

	os_mutex_t *ol_lock;
	os_cond_t *ol_cond;
	
	uint32_t ul_peek_backlog;
	atomic_t ul_buffered_blocks;

	/** A raw data list for this queue. */
	struct list_head head;

	/** A IPC list for every raw elements. */
	struct list_head ipc_head;

	void (*fn_wakeup)(struct oryx_actq_t *);
	void (*fn_stupor) (struct oryx_actq_t *);

	int ul_eq_bytes, ul_eq_blocks, ul_eq_bytes_prev, ul_eq_blocks_prev;
	int ul_dq_bytes, ul_dq_blocks, ul_dq_bytes_prev, ul_dq_blocks_prev;

	/** errors. */
	uint32_t ul_write_error;
	
	uint32_t ul_drop_refcnt;

	/** A oryx_vector this queue stored in. */
	//oryx_vector v;

	uint32_t ul_flags;

}oryx_actq_t;

#define ACTQ_STAT_INC_EQ(byte,actq)\
		((actq)->ul_eq_bytes += byte);\
		((actq)->ul_eq_blocks += 1);

#define ACTQ_STAT_INC_DQ(byte,actq)\
		((actq)->ul_dq_bytes += byte);\
		((actq)->ul_dq_blocks += 1);


ORYX_DECLARE(
	oryx_status_t oryx_actq_new (struct oryx_actq_prefix_t *ap, 
						struct oryx_actq_t **actq)
);

ORYX_DECLARE(
	oryx_status_t oryx_actq_write (struct oryx_actq_t *actq, 
						struct oryx_actq_raw_t *raw)
);

ORYX_DECLARE(
	oryx_status_t oryx_actq_read (struct oryx_actq_t *actq, 
						struct oryx_actq_raw_t **raw)
);

ORYX_DECLARE(
	void actq_lookup (struct oryx_actq_prefix_t *ap, struct oryx_actq_t **v)
);

#endif
