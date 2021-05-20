/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_ION_PRIV_H
#define _MSM_ION_PRIV_H

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/msm_ion.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/msm_ion.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/shrinker.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/bitops.h>
#include <linux/vmstat.h>
#include <uapi/linux/ion.h>
#include <uapi/linux/msm_ion.h>
#include "../ion_private.h"

#define ION_ADSP_HEAP_NAME	"adsp"
#define ION_SYSTEM_HEAP_NAME	"system"
#define ION_MM_HEAP_NAME	"mm"
#define ION_SPSS_HEAP_NAME	"spss"
#define ION_SECURE_CARVEOUT_HEAP_NAME	"secure_carveout"
#define ION_USER_CONTIG_HEAP_NAME	"user_contig"
#define ION_QSECOM_HEAP_NAME	"qsecom"
#define ION_QSECOM_TA_HEAP_NAME	"qsecom_ta"
#define ION_SECURE_HEAP_NAME	"secure_heap"
#define ION_SECURE_DISPLAY_HEAP_NAME "secure_display"
#define ION_AUDIO_HEAP_NAME    "audio"
#define ION_TUI_CARVEOUT_HEAP_NAME "tui_carveout"
#define ION_DISPLAY_HEAP_NAME "display"
#define ION_AUDIO_ML_HEAP_NAME "audio_ml"
#define ION_AUDIO_CARVEOUT_HEAP_NAME "audio_carveout"

/**
 * Debug feature. Make ION allocations DMA
 * ready to help identify clients who are wrongly
 * dependending on ION allocations being DMA
 * ready.
 *
 * As default set to 'false' since ION allocations
 * are no longer required to be DMA ready
 */
#ifdef CONFIG_ION_FORCE_DMA_SYNC
#define MAKE_ION_ALLOC_DMA_READY 1
#else
#define MAKE_ION_ALLOC_DMA_READY 0
#endif

#define to_msm_ion_heap(x) container_of(x, struct msm_ion_heap, ion_heap)

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating higher numb ers
 *		will be allocated from first.  At allocation these are passed
 *		as a bit mask and therefore can not exceed ION_NUM_HEAP_IDS.
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @priv:	private info passed from the board file
 *
 * Provided by the board file.
 */
struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	phys_addr_t base;
	size_t size;
	phys_addr_t align;
	void *priv;
};

/**
 * struct msm_ion_heap_ops - defines a set of ops that are specific to the MSM
 * ION heaps.
 * @heap_prefetch:	called to asynchronously prefetch a certain amount of
 *			memory for allocations from the heap.
 * @heap_drain:		called to asynchronously drain a certain amount of
 *			memory that was prefetched for the heap at an earlier
 *			point in time.
 * @add_memory:		called to add memory to an ION heap. Subsequent
 *			allocations may be satisfied utilizing newly added
 *			memory.
 * @remove_memory:	called to remove memory from an ION heap. Subsequent
 *			allocations will fail if the heap no longer has memory.
 * @debug_show:		called when the heap debug file is read to add any heap
 *			specific debug info to output
 */
struct msm_ion_heap_ops {
	int (*heap_prefetch)(struct ion_heap *heap,
			     struct ion_prefetch_region *regions,
			     int nr_regions);
	int (*heap_drain)(struct ion_heap *heap,
			  struct ion_prefetch_region *regions,
			  int nr_regions);
	int (*add_memory)(struct ion_heap *heap, struct sg_table *sgt);
	int (*remove_memory)(struct ion_heap *heap, struct sg_table *sgt);
	int (*debug_show)(struct ion_heap *heap, struct seq_file *s,
			  void *unused);
};

/**
 * struct msm_ion_heap - defines an ion heap, as well as additional information
 * relevant to the heap.
 * @dev:		the device structure associated with the heap
 * @msm_heap_ops:	the MSM ION specific heap ops for the heap
 * @ion_heap:		ion heap
 *
 */
struct msm_ion_heap {
	struct device *dev;
	struct msm_ion_heap_ops *msm_heap_ops;
	struct ion_heap ion_heap;
};

struct msm_ion_buf_lock_state {
	bool locked;
	int vma_count;
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @nr:    number of structures in the array
 * @heaps: array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct ion_platform_data {
	int nr;
	struct ion_platform_heap *heaps;
};

/**
 * ion_buffer_cached - this ion buffer is cached
 * @buffer:		buffer
 *
 * indicates whether this ion buffer is cached
 */
bool ion_buffer_cached(struct ion_buffer *buffer);

/**
 * functions for creating and destroying the built in ion heaps.
 * architectures can add their own custom architecture specific
 * heaps as appropriate.
 */

struct ion_heap *ion_msm_system_heap_create(struct ion_platform_heap *unused);

struct ion_heap *ion_system_secure_heap_create(struct ion_platform_heap *heap);

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *heap);

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data);

struct ion_heap
*ion_secure_carveout_heap_create(struct ion_platform_heap *heap);

struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *heap_data);

#ifdef CONFIG_CMA
struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *heap);

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *data);
#else
static inline struct ion_heap
			*ion_cma_secure_heap_create(struct ion_platform_heap *h)
{
	return NULL;
}

static inline struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *h)
{
	return NULL;
}
#endif

struct device *msm_ion_heap_device(struct ion_heap *heap);

struct ion_heap *get_ion_heap(int heap_id);

void ion_prepare_sgl_for_force_dma_sync(struct sg_table *table);

/**
 * ion_pages_sync_for_device - cache flush pages for use with the specified
 *                             device
 * @dev:		the device the pages will be used with
 * @page:		the first page to be flushed
 * @size:		size in bytes of region to be flushed
 * @dir:		direction of dma transfer
 */
void ion_pages_sync_for_device(struct device *dev, struct page *page,
			       size_t size, enum dma_data_direction dir);

extern const struct dma_buf_ops msm_ion_dma_buf_ops;
#endif /* _MSM_ION_PRIV_H */
