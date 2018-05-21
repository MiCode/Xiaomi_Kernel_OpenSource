#ifndef __RTMM_H__
#define __MIMEM_H__

struct page *threadinfo_pool_alloc_pages(void);
void threadinfo_pool_free_pages(void *addr);

#endif
