#ifndef _UAPI_MSM_ION_H
#define _UAPI_MSM_ION_H

#include "ion.h"

#define ION_BIT(nr) (1UL << (nr))

enum msm_ion_heap_types {
	ION_HEAP_TYPE_MSM_START = ION_HEAP_TYPE_CUSTOM + 1,
	ION_HEAP_TYPE_SECURE_DMA = ION_HEAP_TYPE_MSM_START,
	ION_HEAP_TYPE_SYSTEM_SECURE,
	ION_HEAP_TYPE_HYP_CMA,
	ION_HEAP_TYPE_SECURE_CARVEOUT,
	/*
	 * if you add a heap type here you should also add it to
	 * heap_types_info[] in msm_ion.c
	 */
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
	ION_SECURE_HEAP_ID = 9,
	ION_SECURE_DISPLAY_HEAP_ID = 10,
	ION_CP_MFC_HEAP_ID = 12,
	ION_SPSS_HEAP_ID = 13, /* Secure Processor ION heap */
	ION_SECURE_CARVEOUT_HEAP_ID = 14,
	ION_CP_WB_HEAP_ID = 16, /* 8660 only */
	ION_QSECOM_TA_HEAP_ID = 19,
	ION_CAMERA_HEAP_ID = 20, /* 8660 only */
	ION_SYSTEM_CONTIG_HEAP_ID = 21,
	ION_ADSP_HEAP_ID = 22,
	ION_PIL1_HEAP_ID = 23, /* Currently used for other PIL images */
	ION_SF_HEAP_ID = 24,
	ION_SYSTEM_HEAP_ID = 25,
	ION_PIL2_HEAP_ID = 26, /* Currently used for modem firmware images */
	ION_QSECOM_HEAP_ID = 27,
	ION_AUDIO_HEAP_ID = 28,

	ION_MM_FIRMWARE_HEAP_ID = 29,

	ION_HEAP_ID_RESERVED = 31 /** Bit reserved for ION_FLAG_SECURE flag */
};

/*
 * The IOMMU heap is deprecated! Here are some aliases for backwards
 * compatibility:
 */
#define ION_IOMMU_HEAP_ID ION_SYSTEM_HEAP_ID
#define ION_HEAP_TYPE_IOMMU ION_HEAP_TYPE_SYSTEM

#define ION_SPSS_HEAP_ID ION_SPSS_HEAP_ID

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
	DISPLAY_SECURE_CP_USAGE = 0x4,
	CAMERA_SECURE_CP_USAGE = 0x5,
	MAX_USAGE = 0x6,
	UNKNOWN = 0x7FFFFFFF,
};

/**
 * Flags to be used when allocating from the secure heap for
 * content protection
 */
#define ION_FLAG_CP_TOUCH		ION_BIT(17)
#define ION_FLAG_CP_BITSTREAM		ION_BIT(18)
#define ION_FLAG_CP_PIXEL		ION_BIT(19)
#define ION_FLAG_CP_NON_PIXEL		ION_BIT(20)
#define ION_FLAG_CP_CAMERA		ION_BIT(21)
#define ION_FLAG_CP_HLOS		ION_BIT(22)
#define ION_FLAG_CP_SPSS_SP		ION_BIT(23)
#define ION_FLAG_CP_SPSS_SP_SHARED	ION_BIT(24)
#define ION_FLAG_CP_SEC_DISPLAY		ION_BIT(25)
#define ION_FLAG_CP_APP			ION_BIT(26)
#define ION_FLAG_CP_CAMERA_PREVIEW	ION_BIT(27)
/* ION_FLAG_ALLOW_NON_CONTIG uses ION_BIT(28) */
#define ION_FLAG_CP_CDSP		ION_BIT(29)
#define ION_FLAG_CP_SPSS_HLOS_SHARED	ION_BIT(30)

/**
 * Flag to allow non continguous allocation of memory from secure
 * heap
 */
#define ION_FLAG_ALLOW_NON_CONTIG       ION_BIT(28)

/**
 * Flag to use when allocating to indicate that a heap is secure.
 * Do NOT use BIT macro since it is defined in #ifdef __KERNEL__
 */
#define ION_FLAG_SECURE			ION_BIT(ION_HEAP_ID_RESERVED)

/*
 * Used in conjunction with heap which pool memory to force an allocation
 * to come from the page allocator directly instead of from the pool allocation
 */
#define ION_FLAG_POOL_FORCE_ALLOC	ION_BIT(16)

/**
 * Deprecated! Please use the corresponding ION_FLAG_*
 */
#define ION_SECURE ION_FLAG_SECURE

/**
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit)			ION_BIT(bit)

#define ION_ADSP_HEAP_NAME	"adsp"
#define ION_SYSTEM_HEAP_NAME	"system"
#define ION_VMALLOC_HEAP_NAME	ION_SYSTEM_HEAP_NAME
#define ION_KMALLOC_HEAP_NAME	"kmalloc"
#define ION_AUDIO_HEAP_NAME	"audio"
#define ION_SF_HEAP_NAME	"sf"
#define ION_MM_HEAP_NAME	"mm"
#define ION_CAMERA_HEAP_NAME	"camera_preview"
#define ION_IOMMU_HEAP_NAME	"iommu"
#define ION_MFC_HEAP_NAME	"mfc"
#define ION_SPSS_HEAP_NAME	"spss"
#define ION_SECURE_CARVEOUT_HEAP_NAME	"secure_carveout"
#define ION_WB_HEAP_NAME	"wb"
#define ION_MM_FIRMWARE_HEAP_NAME	"mm_fw"
#define ION_PIL1_HEAP_NAME  "pil_1"
#define ION_PIL2_HEAP_NAME  "pil_2"
#define ION_QSECOM_HEAP_NAME	"qsecom"
#define ION_QSECOM_TA_HEAP_NAME	"qsecom_ta"
#define ION_SECURE_HEAP_NAME	"secure_heap"
#define ION_SECURE_DISPLAY_HEAP_NAME "secure_display"

#define ION_SET_CACHED(__cache)		((__cache) | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache)	((__cache) & ~ION_FLAG_CACHED)

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
	ion_user_handle_t handle;
	int fd;
	void *vaddr;
	unsigned int offset;
	unsigned int length;
};

struct ion_prefetch_regions {
	unsigned int vmid;
	size_t __user *sizes;
	unsigned int nr_sizes;
};

struct ion_prefetch_data {
	int heap_id;
	unsigned long len;
	struct ion_prefetch_regions __user *regions;
	unsigned int nr_regions;
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

#define ION_IOC_PREFETCH		_IOWR(ION_IOC_MSM_MAGIC, 3, \
						struct ion_prefetch_data)

#define ION_IOC_DRAIN			_IOWR(ION_IOC_MSM_MAGIC, 4, \
						struct ion_prefetch_data)

#endif
