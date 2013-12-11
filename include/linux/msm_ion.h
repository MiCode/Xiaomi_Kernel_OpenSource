#ifndef _LINUX_MSM_ION_H
#define _LINUX_MSM_ION_H

#include <linux/ion.h>

enum msm_ion_heap_types {
	ION_HEAP_TYPE_MSM_START = ION_HEAP_TYPE_CUSTOM + 1,
	ION_HEAP_TYPE_DMA = ION_HEAP_TYPE_MSM_START,
	ION_HEAP_TYPE_CP,
	ION_HEAP_TYPE_SECURE_DMA,
	ION_HEAP_TYPE_REMOVED,
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
	ION_CP_MFC_HEAP_ID = 12,
	ION_CP_WB_HEAP_ID = 16, /* 8660 only */
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
#define ION_HEAP_TYPE_DMA_MASK         (1 << ION_HEAP_TYPE_DMA)

/**
 * Flag to use when allocating to indicate that a heap is secure.
 */
#define ION_FLAG_SECURE (1 << ION_HEAP_ID_RESERVED)

/**
 * Flag for clients to force contiguous memort allocation
 *
 * Use of this flag is carefully monitored!
 */
#define ION_FLAG_FORCE_CONTIGUOUS (1 << 30)

/*
 * Used in conjunction with heap which pool memory to force an allocation
 * to come from the page allocator directly instead of from the pool allocation
 */
#define ION_FLAG_POOL_FORCE_ALLOC (1 << 16)

/**
* Deprecated! Please use the corresponding ION_FLAG_*
*/
#define ION_SECURE ION_FLAG_SECURE
#define ION_FORCE_CONTIGUOUS ION_FLAG_FORCE_CONTIGUOUS

/**
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit) (1 << (bit))

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
#define ION_WB_HEAP_NAME	"wb"
#define ION_MM_FIRMWARE_HEAP_NAME	"mm_fw"
#define ION_PIL1_HEAP_NAME  "pil_1"
#define ION_PIL2_HEAP_NAME  "pil_2"
#define ION_QSECOM_HEAP_NAME	"qsecom"

#define ION_SET_CACHED(__cache)		(__cache | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache)	(__cache & ~ION_FLAG_CACHED)

#define ION_IS_CACHED(__flags)	((__flags) & ION_FLAG_CACHED)

#ifdef __KERNEL__

/*
 * This flag allows clients when mapping into the IOMMU to specify to
 * defer un-mapping from the IOMMU until the buffer memory is freed.
 */
#define ION_IOMMU_UNMAP_DELAYED 1

/*
 * This flag allows clients to defer unsecuring a buffer until the buffer
 * is actually freed.
 */
#define ION_UNSECURE_DELAYED	1

/**
 * struct ion_cp_heap_pdata - defines a content protection heap in the given
 * platform
 * @permission_type:	Memory ID used to identify the memory to TZ
 * @align:		Alignment requirement for the memory
 * @secure_base:	Base address for securing the heap.
 *			Note: This might be different from actual base address
 *			of this heap in the case of a shared heap.
 * @secure_size:	Memory size for securing the heap.
 *			Note: This might be different from actual size
 *			of this heap in the case of a shared heap.
 * @fixed_position	If nonzero, position in the fixed area.
 * @iommu_map_all:	Indicates whether we should map whole heap into IOMMU.
 * @iommu_2x_map_domain: Indicates the domain to use for overmapping.
 * @request_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_region:	function to be called upon ion registration
 * @memory_type:Memory type used for the heap
 * @allow_nonsecure_alloc: allow non-secure allocations from this heap. For
 *			secure heaps, this flag must be set so allow non-secure
 *			allocations. For non-secure heaps, this flag is ignored.
 *
 */
struct ion_cp_heap_pdata {
	enum ion_permission_type permission_type;
	unsigned int align;
	ion_phys_addr_t secure_base; /* Base addr used when heap is shared */
	size_t secure_size; /* Size used for securing heap when heap is shared*/
	int is_cma;
	enum ion_fixed_position fixed_position;
	int iommu_map_all;
	int iommu_2x_map_domain;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *(*setup_region)(void);
	enum ion_memory_types memory_type;
	int allow_nonsecure_alloc;
};

/**
 * struct ion_co_heap_pdata - defines a carveout heap in the given platform
 * @adjacent_mem_id:	Id of heap that this heap must be adjacent to.
 * @align:		Alignment requirement for the memory
 * @fixed_position	If nonzero, position in the fixed area.
 * @request_region:	function to be called when the number of allocations
 *			goes from 0 -> 1
 * @release_region:	function to be called when the number of allocations
 *			goes from 1 -> 0
 * @setup_region:	function to be called upon ion registration
 * @memory_type:Memory type used for the heap
 *
 */
struct ion_co_heap_pdata {
	int adjacent_mem_id;
	unsigned int align;
	enum ion_fixed_position fixed_position;
	int (*request_region)(void *);
	int (*release_region)(void *);
	void *(*setup_region)(void);
	enum ion_memory_types memory_type;
};

/*
 * struct ion_cma_pdata - extra data for CMA regions
 * @default_prefetch_size - default size to use for prefetching
 */
struct ion_cma_pdata {
	unsigned long default_prefetch_size;
};

#ifdef CONFIG_ION
/**
 *  msm_ion_client_create - allocate a client using the ion_device specified in
 *				drivers/gpu/ion/msm/msm_ion.c
 *
 * heap_mask and name are the same as ion_client_create, return values
 * are the same as ion_client_create.
 */

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name);

/**
 * ion_handle_get_flags - get the flags for a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the flags
 * @flags - pointer to store the flags
 *
 * Gets the current flags for a handle. These flags indicate various options
 * of the buffer (caching, security, etc.)
 */
int ion_handle_get_flags(struct ion_client *client, struct ion_handle *handle,
				unsigned long *flags);


/**
 * ion_map_iommu - map the given handle into an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to map
 * @domain_num - domain number to map to
 * @partition_num - partition number to allocate iova from
 * @align - alignment for the iova
 * @iova_length - length of iova to map. If the iova length is
 *		greater than the handle length, the remaining
 *		address space will be mapped to a dummy buffer.
 * @iova - pointer to store the iova address
 * @buffer_size - pointer to store the size of the buffer
 * @flags - flags for options to map
 * @iommu_flags - flags specific to the iommu.
 *
 * Maps the handle into the iova space specified via domain number. Iova
 * will be allocated from the partition specified via partition_num.
 * Returns 0 on success, negative value on error.
 */
int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags);


/**
 * ion_handle_get_size - get the allocated size of a given handle
 *
 * @client - client who allocated the handle
 * @handle - handle to get the size
 * @size - pointer to store the size
 *
 * gives the allocated size of a handle. returns 0 on success, negative
 * value on error
 *
 * NOTE: This is intended to be used only to get a size to pass to map_iommu.
 * You should *NOT* rely on this for any other usage.
 */

int ion_handle_get_size(struct ion_client *client, struct ion_handle *handle,
			unsigned long *size);

/**
 * ion_unmap_iommu - unmap the handle from an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to unmap
 * @domain_num - domain to unmap from
 * @partition_num - partition to unmap from
 *
 * Decrement the reference count on the iommu mapping. If the count is
 * 0, the mapping will be removed from the iommu.
 */
void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num);


/**
 * ion_secure_heap - secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Secure a heap
 * Returns 0 on success
 */
int ion_secure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * ion_unsecure_heap - un-secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to un-secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int ion_unsecure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * msm_ion_do_cache_op - do cache operations.
 *
 * @client - pointer to ION client.
 * @handle - pointer to buffer handle.
 * @vaddr -  virtual address to operate on.
 * @len - Length of data to do cache operation on.
 * @cmd - Cache operation to perform:
 *		ION_IOC_CLEAN_CACHES
 *		ION_IOC_INV_CACHES
 *		ION_IOC_CLEAN_INV_CACHES
 *
 * Returns 0 on success
 */
int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd);

/**
 * msm_ion_secure_heap - secure a heap. Wrapper around ion_secure_heap.
 *
 * @heap_id - heap id to secure.
 *
 * Secure a heap
 * Returns 0 on success
 */
int msm_ion_secure_heap(int heap_id);

/**
 * msm_ion_unsecure_heap - unsecure a heap. Wrapper around ion_unsecure_heap.
 *
  * @heap_id - heap id to secure.
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int msm_ion_unsecure_heap(int heap_id);

/**
 * msm_ion_secure_heap_2_0 - secure a heap using 2.0 APIs
 *  Wrapper around ion_secure_heap.
 *
 * @heap_id - heap id to secure.
 * @usage - usage hint to TZ
 *
 * Secure a heap
 * Returns 0 on success
 */
int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage);

/**
 * msm_ion_unsecure_heap - unsecure a heap secured with 3.0 APIs.
 * Wrapper around ion_unsecure_heap.
 *
 * @heap_id - heap id to secure.
 * @usage - usage hint to TZ
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int msm_ion_unsecure_heap_2_0(int heap_id, enum cp_mem_usage usage);

/**
 * msm_ion_secure_buffer - secure an individual buffer
 *
 * @client - client who has access to the buffer
 * @handle - buffer to secure
 * @usage - usage hint to TZ
 * @flags - flags for the securing
 */
int msm_ion_secure_buffer(struct ion_client *client, struct ion_handle *handle,
				enum cp_mem_usage usage, int flags);

/**
 * msm_ion_unsecure_buffer - unsecure an individual buffer
 *
 * @client - client who has access to the buffer
 * @handle - buffer to secure
 */
int msm_ion_unsecure_buffer(struct ion_client *client,
				struct ion_handle *handle);
#else
static inline struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_map_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num, unsigned long align,
			unsigned long iova_length, unsigned long *iova,
			unsigned long *buffer_size,
			unsigned long flags,
			unsigned long iommu_flags)
{
	return -ENODEV;
}

static inline int ion_handle_get_size(struct ion_client *client,
				struct ion_handle *handle, unsigned long *size)
{
	return -ENODEV;
}

static inline void ion_unmap_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num)
{
	return;
}

static inline int ion_secure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;

}

static inline int ion_unsecure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;
}

static inline void ion_mark_dangling_buffers_locked(struct ion_device *dev)
{
}

static inline int msm_ion_do_cache_op(struct ion_client *client,
			struct ion_handle *handle, void *vaddr,
			unsigned long len, unsigned int cmd)
{
	return -ENODEV;
}

static inline int msm_ion_secure_heap(int heap_id)
{
	return -ENODEV;

}

static inline int msm_ion_unsecure_heap(int heap_id)
{
	return -ENODEV;
}

static inline int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
	return -ENODEV;
}

static inline int msm_ion_unsecure_heap_2_0(int heap_id,
					enum cp_mem_usage usage)
{
	return -ENODEV;
}

static inline int msm_ion_secure_buffer(struct ion_client *client,
					struct ion_handle *handle,
					enum cp_mem_usage usage,
					int flags)
{
	return -ENODEV;
}

static inline int msm_ion_unsecure_buffer(struct ion_client *client,
					struct ion_handle *handle)
{
	return -ENODEV;
}
#endif /* CONFIG_ION */

#endif /* __KERNEL */

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

struct ion_prefetch_data {
       int heap_id;
       unsigned long len;
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

#define ION_IOC_PREFETCH               _IOWR(ION_IOC_MSM_MAGIC, 3, \
                                               struct ion_prefetch_data)

#define ION_IOC_DRAIN                  _IOWR(ION_IOC_MSM_MAGIC, 4, \
                                               struct ion_prefetch_data)

#endif
