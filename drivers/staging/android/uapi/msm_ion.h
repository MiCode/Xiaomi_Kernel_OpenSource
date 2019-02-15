#ifndef _UAPI_LINUX_MSM_ION_H
#define _UAPI_LINUX_MSM_ION_H

#include <linux/types.h>

#define ION_BIT(nr) (1U << (nr))

/**
 * TARGET_ION_ABI_VERSION can be used by user space clients to ensure that at
 * compile time only their code which uses the appropriate ION APIs for
 * this kernel is included.
 */
#define TARGET_ION_ABI_VERSION 2

enum msm_ion_heap_types {
	ION_HEAP_TYPE_MSM_START = 6,
	ION_HEAP_TYPE_SECURE_DMA = ION_HEAP_TYPE_MSM_START,
	ION_HEAP_TYPE_SYSTEM_SECURE,
	ION_HEAP_TYPE_HYP_CMA,
	ION_HEAP_TYPE_SECURE_CARVEOUT,
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
	ION_SPSS_HEAP_ID = 13, /* Secure Processor ION heap */
	ION_ADSP_HEAP_ID = 22,
	ION_SYSTEM_HEAP_ID = 25,
	ION_QSECOM_HEAP_ID = 27,
	ION_HEAP_ID_RESERVED = 31 /** Bit reserved for ION_FLAG_SECURE flag */
};

/**
 * Newly added heap ids have to be #define(d) since all API changes must
 * include a new #define.
 */
#define ION_SECURE_CARVEOUT_HEAP_ID	14
#define ION_QSECOM_TA_HEAP_ID		19
#define ION_AUDIO_HEAP_ID		28
#define ION_CAMERA_HEAP_ID		20
#define ION_USER_CONTIG_HEAP_ID		26
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

#define ION_FLAGS_CP_MASK	0x6FFE0000

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
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit)			ION_BIT(bit)

#define ION_IOC_MSM_MAGIC 'M'

struct ion_prefetch_regions {
	__u64 sizes;
	__u32 vmid;
	__u32 nr_sizes;
};

struct ion_prefetch_data {
	__u64 len;
	__u64 regions;
	__u32 heap_id;
	__u32 nr_regions;
};

#define ION_IOC_PREFETCH		_IOWR(ION_IOC_MSM_MAGIC, 3, \
						struct ion_prefetch_data)

#define ION_IOC_DRAIN			_IOWR(ION_IOC_MSM_MAGIC, 4, \
						struct ion_prefetch_data)

#endif /* _UAPI_LINUX_MSM_ION_H */
