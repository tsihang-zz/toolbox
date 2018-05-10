#ifndef __RANGE_H__
#define __RANGE_H__

struct range_t {
	u32 ul_start;
	u32 ul_end;

	u8 uc_ready;
};

extern void
range_get (char *, struct range_t *);

#endif
