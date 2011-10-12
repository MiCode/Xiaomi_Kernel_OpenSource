/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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

#define KGSL_NAME "kgsl"

/*cache coherency ops */
#define DRM_KGSL_GEM_CACHE_OP_TO_DEV	0x0001
#define DRM_KGSL_GEM_CACHE_OP_FROM_DEV	0x0002

/* The size of each entry in a page table */
#define KGSL_PAGETABLE_ENTRY_SIZE  4

/* Pagetable Virtual Address base */
#define KGSL_PAGETABLE_BASE	0x66000000

/* Extra accounting entries needed in the pagetable */
#define KGSL_PT_EXTRA_ENTRIES      16

#define KGSL_PAGETABLE_ENTRIES(_sz) (((_sz) >> PAGE_SHIFT) + \
				     KGSL_PT_EXTRA_ENTRIES)

#define KGSL_PAGETABLE_SIZE \
ALIGN(KGSL_PAGETABLE_ENTRIES(CONFIG_MSM_KGSL_PAGE_TABLE_SIZE) * \
KGSL_PAGETABLE_ENTRY_SIZE, PAGE_SIZE)

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
		unsigned int coherent;
		unsigned int coherent_max;
		unsigned int mapped;
		unsigned int mapped_max;
		unsigned int histogram[16];
	} stats;
};

extern struct kgsl_driver kgsl_driver;

#define KGSL_USER_MEMORY 1
#define KGSL_MAPPED_MEMORY 2

struct kgsl_pagetable;
struct kgsl_memdesc_ops;

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
};

struct kgsl_mem_entry {
	struct kref refcount;
	struct kgsl_memdesc memdesc;
	int memtype;
	struct file *file_ptr;
	struct list_head list;
	uint32_t free_timestamp;
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
uint8_t *kgsl_gpuaddr_to_vaddr(const struct kgsl_memdesc *memdesc,
	unsigned int gpuaddr, unsigned int *size);
struct kgsl_mem_entry *kgsl_sharedmem_find_region(
	struct kgsl_process_private *private, unsigned int gpuaddr,
	size_t size);

extern const struct dev_pm_ops kgsl_pm_ops;

struct early_suspend;
int kgsl_suspend_driver(struct platform_device *pdev, pm_message_t state);
int kgsl_resume_driver(struct platform_device *pdev);
void kgsl_early_suspend_driver(struct early_suspend *h);
void kgsl_late_resume_driver(struct early_suspend *h);

#ifdef CONFIG_MSM_KGSL_DRM
extern int kgsl_drm_init(struct platform_device *dev);
extern void kgsl_drm_exit(void);
extern void kgsl_gpu_mem_flush(int op);
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
				unsigned int gpuaddr)
{
	if (gpuaddr >= memdesc->gpuaddr && (gpuaddr + sizeof(unsigned int)) <=
		(memdesc->gpuaddr + memdesc->size)) {
		return 1;
	}
	return 0;
}

static inline bool timestamp_cmp(unsigned int new, unsigned int old)
{
	int ts_diff = new - old;
	return (ts_diff >= 0) || (ts_diff < -20000);
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
