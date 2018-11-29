#ifndef __RTMM_H__
#define __MIMEM_H__

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
