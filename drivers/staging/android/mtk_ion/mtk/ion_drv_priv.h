/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ION_DRV_PRIV_H__
#define __ION_DRV_PRIV_H__

#include "ion_priv.h"

/* STRUCT ION_HEAP *G_ION_HEAPS[ION_HEAP_IDX_MAX]; */

/* Import from multimedia heap */
long ion_mm_ioctl(struct ion_client *client, unsigned int cmd,
		  unsigned long arg, int from_kernel);

void smp_inner_dcache_flush_all(void);

#ifdef ION_HISTORY_RECORD
int ion_history_init(void);
void ion_history_count_kick(bool allc, size_t len);
#else
static inline int ion_history_init(void)
{
	return 0;
}

void ion_history_count_kick(bool allc, size_t len)
{
	/*do nothing */
}
#endif

int
ion_mm_heap_for_each_pool(int (*fn)
			  (int high, int order, int cache, size_t size));
struct ion_heap *ion_drv_get_heap(struct ion_device *dev, int heap_id,
				  int need_lock);
int ion_drv_create_heap(struct ion_platform_heap *heap_data);
struct ion_buffer *ion_drv_file_to_buffer(struct file *file);
int ion_mm_heap_cache_allocate(struct ion_heap *heap,
			       struct ion_buffer *buffer,
			       unsigned long size,
			       unsigned long align,
			       unsigned long flags);
void ion_mm_heap_cache_free(struct ion_buffer *buffer);
int ion_mm_heap_pool_size(struct ion_heap *heap, gfp_t gfp_mask, bool cache);

int ion_comm_init(void);
void ion_comm_event_notify(bool allc, size_t len);
#ifdef CONFIG_PM
void shrink_ion_by_scenario(int need_lock);
#endif

#endif
