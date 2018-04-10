#ifndef __RTMM_H__
#define __MIMEM_H__

#define GLOBAL_RECLAIM_SWAPPINESS 150

enum {
	RTMM_POOL_THREADINFO = 0,
	RTMM_POOL_NR
};

int __init rtmm_pool_init(struct dentry *dir);
struct page *rtmm_alloc_pages(int pool_type);
void rtmm_free_pages(void *addr, int pool_type);

int __init rtmm_reclaim_init(struct dentry *dir);

struct scan_control;
#ifdef CONFIG_RTMM
static inline bool rtmm_reclaim(struct scan_control *sc)
{
	return strncmp("rtmm_reclaim", current->comm, strlen("rtmm_reclaim")) == 0;
}
#else
static inline bool rtmm_reclaim(struct scan_control *sc)
{
	return false;
}
#endif

#endif
