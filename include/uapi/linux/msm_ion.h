/*
 * include/linux/ion.h
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _UAPI_MSM_ION_H
#define _UAPI_MSM_ION_H

#include <linux/ion.h>

enum msm_ion_heap_types {
	ION_HEAP_TYPE_MSM_START = ION_HEAP_TYPE_CUSTOM + 1,
	ION_HEAP_TYPE_IOMMU = ION_HEAP_TYPE_MSM_START,
	ION_HEAP_TYPE_CP,
};

/**
 * These are the only ids that should be used for Ion heap ids.
 * The ids listed are the order in which allocation will be attempted
 * if specified. Don't swap the order of heap ids unless you know what
 * you are doing!
 * Id's are spaced by purpose to allow new Id's to be inserted in-between (for
 * possible fallbacks)
 */

enum ion_heap_ids {
	INVALID_HEAP_ID = -1,
	ION_CP_MM_HEAP_ID = 8,
	ION_CP_MFC_HEAP_ID = 12,
	ION_CP_WB_HEAP_ID = 16, /* 8660 only */
	ION_CAMERA_HEAP_ID = 20, /* 8660 only */
	ION_SF_HEAP_ID = 24,
	ION_IOMMU_HEAP_ID = 25,
	ION_QSECOM_HEAP_ID = 27,
	ION_AUDIO_HEAP_ID = 28,

	ION_MM_FIRMWARE_HEAP_ID = 29,
	ION_SYSTEM_HEAP_ID = 30,

	ION_HEAP_ID_RESERVED = 31 /** Bit reserved for ION_SECURE flag */
};

enum ion_fixed_position {
	NOT_FIXED,
	FIXED_LOW,
	FIXED_MIDDLE,
	FIXED_HIGH,
};

enum cp_mem_usage {
	VIDEO_BITSTREAM = 0x1,
	VIDEO_PIXEL = 0x2,
	VIDEO_NONPIXEL = 0x3,
	MAX_USAGE = 0x4,
	UNKNOWN = 0x7FFFFFFF,
};

#define ION_HEAP_CP_MASK		(1 << ION_HEAP_TYPE_CP)

/**
 * Flag to use when allocating to indicate that a heap is secure.
 */
#define ION_SECURE (1 << ION_HEAP_ID_RESERVED)

/**
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit) (1 << (bit))

#define ION_VMALLOC_HEAP_NAME	"vmalloc"
#define ION_AUDIO_HEAP_NAME	"audio"
#define ION_SF_HEAP_NAME	"sf"
#define ION_MM_HEAP_NAME	"mm"
#define ION_CAMERA_HEAP_NAME	"camera_preview"
#define ION_IOMMU_HEAP_NAME	"iommu"
#define ION_MFC_HEAP_NAME	"mfc"
#define ION_WB_HEAP_NAME	"wb"
#define ION_MM_FIRMWARE_HEAP_NAME	"mm_fw"
#define ION_QSECOM_HEAP_NAME	"qsecom"
#define ION_FMEM_HEAP_NAME	"fmem"

#define ION_SET_CACHED(__cache)		(__cache | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache)	(__cache & ~ION_FLAG_CACHED)

#define ION_IS_CACHED(__flags)	((__flags) & ION_FLAG_CACHED)

/* struct ion_flush_data - data passed to ion for flushing caches
 *
 * @handle:	handle with data to flush
 * @fd:		fd to flush
 * @vaddr:	userspace virtual address mapped with mmap
 * @offset:	offset into the handle to flush
 * @length:	length of handle to flush
 *
 * Performs cache operations on the handle. If p is the start address
 * of the handle, p + offset through p + offset + length will have
 * the cache operations performed
 */
struct ion_flush_data {
	struct ion_handle *handle;
	int fd;
	void *vaddr;
	unsigned int offset;
	unsigned int length;
};

/* struct ion_flag_data - information about flags for this buffer
 *
 * @handle:	handle to get flags from
 * @flags:	flags of this handle
 *
 * Takes handle as an input and outputs the flags from the handle
 * in the flag field.
 */
struct ion_flag_data {
	struct ion_handle *handle;
	unsigned long flags;
};

#define ION_IOC_MSM_MAGIC 'M'

/**
 * DOC: ION_IOC_CLEAN_CACHES - clean the caches
 *
 * Clean the caches of the handle specified.
 */
#define ION_IOC_CLEAN_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 0, \
						struct ion_flush_data)
/**
 * DOC: ION_IOC_INV_CACHES - invalidate the caches
 *
 * Invalidate the caches of the handle specified.
 */
#define ION_IOC_INV_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 1, \
						struct ion_flush_data)
/**
 * DOC: ION_IOC_CLEAN_INV_CACHES - clean and invalidate the caches
 *
 * Clean and invalidate the caches of the handle specified.
 */
#define ION_IOC_CLEAN_INV_CACHES	_IOWR(ION_IOC_MSM_MAGIC, 2, \
						struct ion_flush_data)

/**
 * DOC: ION_IOC_GET_FLAGS - get the flags of the handle
 *
 * Gets the flags of the current handle which indicate cachability,
 * secure state etc.
 */
#define ION_IOC_GET_FLAGS		_IOWR(ION_IOC_MSM_MAGIC, 3, \
						struct ion_flag_data)

#endif
