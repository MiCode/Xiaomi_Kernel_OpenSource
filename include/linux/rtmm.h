#ifndef __RTMM_H__
#define __RTMM_H__

enum {
	RTMM_POOL_THREADINFO = 0,
	RTMM_POOL_PGD,
	RTMM_POOL_KMALLOC_ORDER2,
	RTMM_POOL_NR
};

#define KMALLOC_POOL_ORDER2 2
extern bool rtmm_reclaim(const char *name);
extern int rtmm_reclaim_swappiness(void);
#endif
