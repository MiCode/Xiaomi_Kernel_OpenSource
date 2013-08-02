/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __KGSL_H
#define __KGSL_H

#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>
#include <linux/mm.h>

#include <mach/kgsl.h>

#define KGSL_NAME "kgsl"

/* The number of memstore arrays limits the number of contexts allowed.
 * If more contexts are needed, update multiple for MEMSTORE_SIZE
 */
#define KGSL_MEMSTORE_SIZE	((int)(PAGE_SIZE * 2))
#define KGSL_MEMSTORE_GLOBAL	(0)
#define KGSL_MEMSTORE_MAX	(KGSL_MEMSTORE_SIZE / \
		sizeof(struct kgsl_devmemstore) - 1)

/* Timestamp window used to detect rollovers (half of integer range) */
#define KGSL_TIMESTAMP_WINDOW 0x80000000

/*cache coherency ops */
#define DRM_KGSL_GEM_CACHE_OP_TO_DEV	0x0001
#define DRM_KGSL_GEM_CACHE_OP_FROM_DEV	0x0002

/* The size of each entry in a page table */
#define KGSL_PAGETABLE_ENTRY_SIZE  4

/* Pagetable Virtual Address base */
#ifndef CONFIG_MSM_KGSL_CFF_DUMP
#define KGSL_PAGETABLE_BASE	0x10000000
#else
#define KGSL_PAGETABLE_BASE	0xE0000000
#endif

/* Extra accounting entries needed in the pagetable */
#define KGSL_PT_EXTRA_ENTRIES      16

#define KGSL_PAGETABLE_ENTRIES(_sz) (((_sz) >> PAGE_SHIFT) + \
				     KGSL_PT_EXTRA_ENTRIES)

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
#define KGSL_PAGETABLE_COUNT (CONFIG_MSM_KGSL_PAGE_TABLE_COUNT)
#else
#define KGSL_PAGETABLE_COUNT 1
#endif

/* Casting using container_of() for structures that kgsl owns. */
#define KGSL_CONTAINER_OF(ptr, type, member) \
		container_of(ptr, type, member)

/* A macro for memory statistics - add the new size to the stat and if
   the statisic is greater then _max, set _max
*/

#define KGSL_STATS_ADD(_size, _stat, _max) \
	do { _stat += (_size); if (_stat > _max) _max = _stat; } while (0)


#define KGSL_MEMFREE_HIST_SIZE	((int)(PAGE_SIZE * 2))

#define KGSL_MAX_NUMIBS 100000

struct kgsl_memfree_hist_elem {
	unsigned int pid;
	unsigned int gpuaddr;
	unsigned int size;
	unsigned int flags;
};

struct kgsl_memfree_hist {
	void *base_hist_rb;
	unsigned int size;
	struct kgsl_memfree_hist_elem *wptr;
};


struct kgsl_device;
struct kgsl_context;

struct kgsl_driver {
	struct cdev cdev;
	dev_t major;
	struct class *class;
	/* Virtual device for managing the core */
	struct device virtdev;
	/* Kobjects for storing pagetable and process statistics */
	struct kobject *ptkobj;
	struct kobject *prockobj;
	struct kgsl_device *devp[KGSL_DEVICE_MAX];

	/* Global lilst of open processes */
	struct list_head process_list;
	/* Global list of pagetables */
	struct list_head pagetable_list;
	/* Spinlock for accessing the pagetable list */
	spinlock_t ptlock;
	/* Mutex for accessing the process list */
	struct mutex process_mutex;

	/* Mutex for protecting the device list */
	struct mutex devlock;

	void *ptpool;

	struct mutex memfree_hist_mutex;
	struct kgsl_memfree_hist memfree_hist;

	struct {
		unsigned int vmalloc;
		unsigned int vmalloc_max;
		unsigned int page_alloc;
		unsigned int page_alloc_max;
		unsigned int coherent;
		unsigned int coherent_max;
		unsigned int mapped;
		unsigned int mapped_max;
		unsigned int histogram[16];
	} stats;
	unsigned int full_cache_threshold;
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_pagetable;
struct kgsl_memdesc;
struct kgsl_cmdbatch;

struct kgsl_memdesc_ops {
	int (*vmflags)(struct kgsl_memdesc *);
	int (*vmfault)(struct kgsl_memdesc *, struct vm_area_struct *,
		       struct vm_fault *);
	void (*free)(struct kgsl_memdesc *memdesc);
	int (*map_kernel_mem)(struct kgsl_memdesc *);
};

/* Internal definitions for memdesc->priv */
#define KGSL_MEMDESC_GUARD_PAGE BIT(0)
/* Set if the memdesc is mapped into all pagetables */
#define KGSL_MEMDESC_GLOBAL BIT(1)
/* The memdesc is frozen during a snapshot */
#define KGSL_MEMDESC_FROZEN BIT(2)
/* The memdesc is mapped into a pagetable */
#define KGSL_MEMDESC_MAPPED BIT(3)

/* shared memory allocation */
struct kgsl_memdesc {
	struct kgsl_pagetable *pagetable;
	void *hostptr; /* kernel virtual address */
	unsigned long useraddr; /* userspace address */
	unsigned int gpuaddr;
	phys_addr_t physaddr;
	unsigned int size;
	unsigned int priv; /* Internal flags and settings */
	struct scatterlist *sg;
	unsigned int sglen; /* Active entries in the sglist */
	unsigned int sglen_alloc;  /* Allocated entries in the sglist */
	struct kgsl_memdesc_ops *ops;
	unsigned int flags; /* Flags set from userspace */
};

/* List of different memory entry types */

#define KGSL_MEM_ENTRY_KERNEL 0
#define KGSL_MEM_ENTRY_PMEM   1
#define KGSL_MEM_ENTRY_ASHMEM 2
#define KGSL_MEM_ENTRY_USER   3
#define KGSL_MEM_ENTRY_ION    4
#define KGSL_MEM_ENTRY_MAX    5

struct kgsl_mem_entry {
	struct kref refcount;
	struct kgsl_memdesc memdesc;
	int memtype;
	void *priv_data;
	struct rb_node node;
	unsigned int id;
	unsigned int context_id;
	/* back pointer to private structure under whose context this
	* allocation is made */
	struct kgsl_process_private *priv;
	/* Initialized to 0, set to 1 when entry is marked for freeing */
	int pending_free;
};

#ifdef CONFIG_MSM_KGSL_MMU_PAGE_FAULT
#define MMU_CONFIG 2
#else
#define MMU_CONFIG 1
#endif

void kgsl_mem_entry_destroy(struct kref *kref);
int kgsl_postmortem_dump(struct kgsl_device *device, int manual);

struct kgsl_mem_entry *kgsl_get_mem_entry(struct kgsl_device *device,
		phys_addr_t ptbase, unsigned int gpuaddr, unsigned int size);

struct kgsl_mem_entry *kgsl_sharedmem_find_region(
	struct kgsl_process_private *private, unsigned int gpuaddr,
	size_t size);

void kgsl_get_memory_usage(char *str, size_t len, unsigned int memflags);

void kgsl_signal_event(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp,
		unsigned int type);

void kgsl_signal_events(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int type);

void kgsl_cancel_events(struct kgsl_device *device,
	void *owner);

extern const struct dev_pm_ops kgsl_pm_ops;

int kgsl_suspend_driver(struct platform_device *pdev, pm_message_t state);
int kgsl_resume_driver(struct platform_device *pdev);

void kgsl_trace_regwrite(struct kgsl_device *device, unsigned int offset,
		unsigned int value);

void kgsl_trace_issueibcmds(struct kgsl_device *device, int id,
		struct kgsl_cmdbatch *cmdbatch,
		unsigned int timestamp, unsigned int flags,
		int result, unsigned int type);

int kgsl_open_device(struct kgsl_device *device);

int kgsl_close_device(struct kgsl_device *device);

#ifdef CONFIG_MSM_KGSL_DRM
extern int kgsl_drm_init(struct platform_device *dev);
extern void kgsl_drm_exit(void);
#else
static inline int kgsl_drm_init(struct platform_device *dev)
{
	return 0;
}

static inline void kgsl_drm_exit(void)
{
}
#endif

static inline int kgsl_gpuaddr_in_memdesc(const struct kgsl_memdesc *memdesc,
				unsigned int gpuaddr, unsigned int size)
{
	/* set a minimum size to search for */
	if (!size)
		size = 1;

	/* don't overflow */
	if ((gpuaddr + size) < gpuaddr)
		return 0;

	if (gpuaddr >= memdesc->gpuaddr &&
	    ((gpuaddr + size) <= (memdesc->gpuaddr + memdesc->size))) {
		return 1;
	}
	return 0;
}

static inline void *kgsl_memdesc_map(struct kgsl_memdesc *memdesc)
{
	if (memdesc->hostptr == NULL && memdesc->ops &&
		memdesc->ops->map_kernel_mem)
		memdesc->ops->map_kernel_mem(memdesc);

	return memdesc->hostptr;
}

static inline uint8_t *kgsl_gpuaddr_to_vaddr(struct kgsl_memdesc *memdesc,
					     unsigned int gpuaddr)
{
	void *hostptr = NULL;

	if ((gpuaddr >= memdesc->gpuaddr) &&
		(gpuaddr < (memdesc->gpuaddr + memdesc->size)))
		hostptr = kgsl_memdesc_map(memdesc);

	return hostptr != NULL ? hostptr + (gpuaddr - memdesc->gpuaddr) : NULL;
}

static inline int timestamp_cmp(unsigned int a, unsigned int b)
{
	/* check for equal */
	if (a == b)
		return 0;

	/* check for greater-than for non-rollover case */
	if ((a > b) && (a - b < KGSL_TIMESTAMP_WINDOW))
		return 1;

	/* check for greater-than for rollover case
	 * note that <= is required to ensure that consistent
	 * results are returned for values whose difference is
	 * equal to the window size
	 */
	a += KGSL_TIMESTAMP_WINDOW;
	b += KGSL_TIMESTAMP_WINDOW;
	return ((a > b) && (a - b <= KGSL_TIMESTAMP_WINDOW)) ? 1 : -1;
}

static inline int
kgsl_mem_entry_get(struct kgsl_mem_entry *entry)
{
	return kref_get_unless_zero(&entry->refcount);
}

static inline void
kgsl_mem_entry_put(struct kgsl_mem_entry *entry)
{
	kref_put(&entry->refcount, kgsl_mem_entry_destroy);
}

/*
 * kgsl_addr_range_overlap() - Checks if 2 ranges overlap
 * @gpuaddr1: Start of first address range
 * @size1: Size of first address range
 * @gpuaddr2: Start of second address range
 * @size2: Size of second address range
 *
 * Function returns true if the 2 given address ranges overlap
 * else false
 */
static inline bool kgsl_addr_range_overlap(unsigned int gpuaddr1,
		unsigned int size1,
		unsigned int gpuaddr2, unsigned int size2)
{
	return !(((gpuaddr1 + size1) < gpuaddr2) ||
		(gpuaddr1 > (gpuaddr2 + size2)));
}

#endif /* __KGSL_H */
