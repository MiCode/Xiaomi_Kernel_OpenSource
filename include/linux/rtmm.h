#ifndef __RTMM_H__
#define __RTMM_H__

enum {
	RTMM_POOL_THREADINFO = 0,
	RTMM_POOL_PGD,
	RTMM_POOL_KMALLOC_ORDER2,
	RTMM_POOL_NR
};

#define KMALLOC_POOL_ORDER2 2

int __init rtmm_pool_init(struct dentry *dir);
void *rtmm_alloc(int pool_type);
void rtmm_free(void *addr, int pool_type);

int __init rtmm_reclaim_init(struct dentry *dir);

#ifdef CONFIG_RTMM
bool rtmm_pool(const char *name);
bool rtmm_reclaim(const char *name);
int rtmm_reclaim_swappiness(void);

#else
static inline bool rtmm_pool(const char *name)
{
        return false;
}

static inline bool rtmm_reclaim(const char *name)
{
	return false;
}

static inline int rtmm_reclaim_swappiness(void)
{
	return 60;
}

#endif

#endif
