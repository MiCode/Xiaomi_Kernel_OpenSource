#ifndef __RTMM_H__
#define __RTMM_H__

enum {
	RTMM_POOL_THREADINFO = 0, /* for 64bit arm */
	RTMM_POOL_PGD, /* for 32bit arm */
	RTMM_POOL_NR
};

struct page *rtmm_alloc_pages(int pool_type);
void rtmm_free_pages(void *addr, int pool_type);

int __init rtmm_pool_init(struct dentry *dir);
int __init rtmm_reclaim_init(struct dentry *dir);

#ifdef CONFIG_RTMM
bool rtmm_reclaim(const char *name);
int rtmm_reclaim_swappiness(void);

#else
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
