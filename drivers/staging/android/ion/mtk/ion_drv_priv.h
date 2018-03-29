/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __ION_DRV_PRIV_H__
#define __ION_DRV_PRIV_H__

#include "ion_priv.h"
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#include "secmem.h"
#endif

/* STRUCT ION_HEAP *G_ION_HEAPS[ION_HEAP_IDX_MAX]; */

/* Import from multimedia heap */
long ion_mm_ioctl(struct ion_client *client, unsigned int cmd,
		unsigned long arg, int from_kernel);

void smp_inner_dcache_flush_all(void);
#ifdef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL
int mt_smp_cache_flush(struct sg_table *table, unsigned int sync_type, int npages);

extern int (*ion_sync_kernel_func)(unsigned long start, size_t size,
		unsigned int sync_type);
#endif

#ifdef ION_HISTORY_RECORD
extern int ion_history_init(void);
#else
static inline int ion_history_init(void)
{
	return 0;
}
#endif

int ion_mm_heap_for_each_pool(int (*fn)(int high, int order, int cache, size_t size));
struct ion_heap *ion_drv_get_heap(struct ion_device *dev, int heap_id, int need_lock);
int ion_drv_create_heap(struct ion_platform_heap *heap_data);

#ifdef CONFIG_PM
extern void shrink_ion_by_scenario(void);
#endif

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
extern int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount,
					u32 *sec_handle, uint8_t *owner, uint32_t id);
int secmem_api_alloc_zero(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle, uint8_t *owner,
	uint32_t id);
extern int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id);
#endif

#endif
