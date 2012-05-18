/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#define KGSL_PAGETABLE_BASE	0x10000000

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

struct kgsl_device;

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
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_pagetable;
struct kgsl_memdesc;

struct kgsl_memdesc_ops {
	int (*vmflags)(struct kgsl_memdesc *);
	int (*vmfault)(struct kgsl_memdesc *, struct vm_area_struct *,
		       struct vm_fault *);
	void (*free)(struct kgsl_memdesc *memdesc);
	int (*map_kernel_mem)(struct kgsl_memdesc *);
};

#define KGSL_MEMDESC_GUARD_PAGE BIT(0)

/* shared memory allocation */
struct kgsl_memdesc {
	struct kgsl_pagetable *pagetable;
	void *hostptr;
	unsigned int gpuaddr;
	unsigned int physaddr;
	unsigned int size;
	unsigned int priv;
	struct scatterlist *sg;
	unsigned int sglen;
	struct kgsl_memdesc_ops *ops;
	int flags;
};

/* List of different memory entry types */

#define KGSL_MEM_ENTRY_KERNEL 0
#define KGSL_MEM_ENTRY_PMEM   1
#define KGSL_MEM_ENTRY_ASHMEM 2
#define KGSL_MEM_ENTRY_USER   3
#define KGSL_MEM_ENTRY_ION    4
#define KGSL_MEM_ENTRY_MAX    5

/* List of flags */

#define KGSL_MEM_ENTRY_FROZEN (1 << 0)

struct kgsl_mem_entry {
	struct kref refcount;
	struct kgsl_memdesc memdesc;
	int memtype;
	int flags;
	void *priv_data;
	struct rb_node node;
	unsigned int context_id;
	/* back pointer to private structure under whose context this
	* allocation is made */
	struct kgsl_process_private *priv;
};

#ifdef CONFIG_MSM_KGSL_MMU_PAGE_FAULT
#define MMU_CONFIG 2
#else
#define MMU_CONFIG 1
#endif

void kgsl_mem_entry_destroy(struct kref *kref);
int kgsl_postmortem_dump(struct kgsl_device *device, int manual);

struct kgsl_mem_entry *kgsl_get_mem_entry(unsigned int ptbase,
		unsigned int gpuaddr, unsigned int size);

struct kgsl_mem_entry *kgsl_sharedmem_find_region(
	struct kgsl_process_private *private, unsigned int gpuaddr,
	size_t size);

int kgsl_add_event(struct kgsl_device *device, u32 id, u32 ts,
	void (*cb)(struct kgsl_device *, void *, u32, u32), void *priv,
	void *owner);

void kgsl_cancel_events(struct kgsl_device *device,
	void *owner);

extern const struct dev_pm_ops kgsl_pm_ops;

struct early_suspend;
int kgsl_suspend_driver(struct platform_device *pdev, pm_message_t state);
int kgsl_resume_driver(struct platform_device *pdev);
void kgsl_early_suspend_driver(struct early_suspend *h);
void kgsl_late_resume_driver(struct early_suspend *h);

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

static inline void
kgsl_mem_entry_get(struct kgsl_mem_entry *entry)
{
	kref_get(&entry->refcount);
}

static inline void
kgsl_mem_entry_put(struct kgsl_mem_entry *entry)
{
	kref_put(&entry->refcount, kgsl_mem_entry_destroy);
}

#endif /* __KGSL_H */
