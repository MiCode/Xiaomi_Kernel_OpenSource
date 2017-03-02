/* Copyright (c) 2008-2017, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>
#include <linux/rbtree.h>
#include <linux/major.h>
#include <linux/io.h>
#include <linux/mman.h>
#include <linux/sort.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <linux/ctype.h>

#include "kgsl.h"
#include "kgsl_debugfs.h"
#include "kgsl_cffdump.h"
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#include "kgsl_drawobj.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_sync.h"
#include "kgsl_compat.h"
#include "kgsl_pool.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

#ifndef arch_mmap_check
#define arch_mmap_check(addr, len, flags)	(0)
#endif

#ifndef pgprot_writebackcache
#define pgprot_writebackcache(_prot)	(_prot)
#endif

#ifndef pgprot_writethroughcache
#define pgprot_writethroughcache(_prot)	(_prot)
#endif

#ifdef CONFIG_ARM_LPAE
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(64)
#else
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(32)
#endif

static char *kgsl_mmu_type;
module_param_named(mmutype, kgsl_mmu_type, charp, 0);
MODULE_PARM_DESC(kgsl_mmu_type, "Type of MMU to be used for graphics");

/* Mutex used for the IOMMU sync quirk */
DEFINE_MUTEX(kgsl_mmu_sync);
EXPORT_SYMBOL(kgsl_mmu_sync);

struct kgsl_dma_buf_meta {
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
	struct sg_table *table;
};

static inline struct kgsl_pagetable *_get_memdesc_pagetable(
		struct kgsl_pagetable *pt, struct kgsl_mem_entry *entry)
{
	/* if a secured buffer, map it to secure global pagetable */
	if (kgsl_memdesc_is_secured(&entry->memdesc))
		return pt->mmu->securepagetable;

	return pt;
}

static void kgsl_mem_entry_detach_process(struct kgsl_mem_entry *entry);

static const struct file_operations kgsl_fops;

/*
 * The memfree list contains the last N blocks of memory that have been freed.
 * On a GPU fault we walk the list to see if the faulting address had been
 * recently freed and print out a message to that effect
 */

#define MEMFREE_ENTRIES 512

static DEFINE_SPINLOCK(memfree_lock);

struct memfree_entry {
	pid_t ptname;
	uint64_t gpuaddr;
	uint64_t size;
	pid_t pid;
	uint64_t flags;
};

static struct {
	struct memfree_entry *list;
	int head;
	int tail;
} memfree;

static int kgsl_memfree_init(void)
{
	memfree.list = kzalloc(MEMFREE_ENTRIES * sizeof(struct memfree_entry),
		GFP_KERNEL);

	return (memfree.list) ? 0 : -ENOMEM;
}

static void kgsl_memfree_exit(void)
{
	kfree(memfree.list);
	memset(&memfree, 0, sizeof(memfree));
}

static inline bool match_memfree_addr(struct memfree_entry *entry,
		pid_t ptname, uint64_t gpuaddr)
{
	return ((entry->ptname == ptname) &&
		(entry->size > 0) &&
		(gpuaddr >= entry->gpuaddr &&
			 gpuaddr < (entry->gpuaddr + entry->size)));
}
int kgsl_memfree_find_entry(pid_t ptname, uint64_t *gpuaddr,
	uint64_t *size, uint64_t *flags, pid_t *pid)
{
	int ptr;

	if (memfree.list == NULL)
		return 0;

	spin_lock(&memfree_lock);

	ptr = memfree.head - 1;
	if (ptr < 0)
		ptr = MEMFREE_ENTRIES - 1;

	/* Walk backwards through the list looking for the last match  */
	while (ptr != memfree.tail) {
		struct memfree_entry *entry = &memfree.list[ptr];

		if (match_memfree_addr(entry, ptname, *gpuaddr)) {
			*gpuaddr = entry->gpuaddr;
			*flags = entry->flags;
			*size = entry->size;
			*pid = entry->pid;

			spin_unlock(&memfree_lock);
			return 1;
		}

		ptr = ptr - 1;

		if (ptr < 0)
			ptr = MEMFREE_ENTRIES - 1;
	}

	spin_unlock(&memfree_lock);
	return 0;
}

static void kgsl_memfree_purge(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	pid_t ptname = pagetable ? pagetable->name : 0;
	int i;

	if (memfree.list == NULL)
		return;

	spin_lock(&memfree_lock);

	for (i = 0; i < MEMFREE_ENTRIES; i++) {
		struct memfree_entry *entry = &memfree.list[i];

		if (entry->ptname != ptname || entry->size == 0)
			continue;

		if (gpuaddr > entry->gpuaddr &&
			gpuaddr < entry->gpuaddr + entry->size) {
			/* truncate the end of the entry */
			entry->size = gpuaddr - entry->gpuaddr;
		} else if (gpuaddr <= entry->gpuaddr) {
			if (gpuaddr + size > entry->gpuaddr &&
				gpuaddr + size < entry->gpuaddr + entry->size)
				/* Truncate the beginning of the entry */
				entry->gpuaddr = gpuaddr + size;
			else if (gpuaddr + size >= entry->gpuaddr + entry->size)
				/* Remove the entire entry */
				entry->size = 0;
		}
	}
	spin_unlock(&memfree_lock);
}

static void kgsl_memfree_add(pid_t pid, pid_t ptname, uint64_t gpuaddr,
		uint64_t size, uint64_t flags)

{
	struct memfree_entry *entry;

	if (memfree.list == NULL)
		return;

	spin_lock(&memfree_lock);

	entry = &memfree.list[memfree.head];

	entry->pid = pid;
	entry->ptname = ptname;
	entry->gpuaddr = gpuaddr;
	entry->size = size;
	entry->flags = flags;

	memfree.head = (memfree.head + 1) % MEMFREE_ENTRIES;

	if (memfree.head == memfree.tail)
		memfree.tail = (memfree.tail + 1) % MEMFREE_ENTRIES;

	spin_unlock(&memfree_lock);
}

int kgsl_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp)
{
	return device->ftbl->readtimestamp(device, priv, type, timestamp);
}
EXPORT_SYMBOL(kgsl_readtimestamp);

static long gpumem_free_entry(struct kgsl_mem_entry *entry);

/* Scheduled by kgsl_mem_entry_put_deferred() */
static void _deferred_put(struct work_struct *work)
{
	struct kgsl_mem_entry *entry =
		container_of(work, struct kgsl_mem_entry, work);

	kgsl_mem_entry_put(entry);
}

static inline struct kgsl_mem_entry *
kgsl_mem_entry_create(void)
{
	struct kgsl_mem_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

	if (entry != NULL)
		kref_init(&entry->refcount);

	return entry;
}
#ifdef CONFIG_DMA_SHARED_BUFFER
static void kgsl_destroy_ion(struct kgsl_dma_buf_meta *meta)
{
	if (meta != NULL) {
		dma_buf_unmap_attachment(meta->attach, meta->table,
			DMA_FROM_DEVICE);
		dma_buf_detach(meta->dmabuf, meta->attach);
		dma_buf_put(meta->dmabuf);
		kfree(meta);
	}
}
#else
static void kgsl_destroy_ion(struct kgsl_dma_buf_meta *meta)
{

}
#endif

void
kgsl_mem_entry_destroy(struct kref *kref)
{
	struct kgsl_mem_entry *entry = container_of(kref,
						    struct kgsl_mem_entry,
						    refcount);
	unsigned int memtype;

	if (entry == NULL)
		return;

	/* pull out the memtype before the flags get cleared */
	memtype = kgsl_memdesc_usermem_type(&entry->memdesc);

	/* Detach from process list */
	kgsl_mem_entry_detach_process(entry);

	if (memtype != KGSL_MEM_ENTRY_KERNEL)
		atomic_long_sub(entry->memdesc.size,
			&kgsl_driver.stats.mapped);

	/*
	 * Ion takes care of freeing the sg_table for us so
	 * clear the sg table before freeing the sharedmem
	 * so kgsl_sharedmem_free doesn't try to free it again
	 */
	if (memtype == KGSL_MEM_ENTRY_ION)
		entry->memdesc.sgt = NULL;

	if ((memtype == KGSL_MEM_ENTRY_USER)
		&& !(entry->memdesc.flags & KGSL_MEMFLAGS_GPUREADONLY)) {
		int i = 0, j;
		struct scatterlist *sg;
		struct page *page;
		/*
		 * Mark all of pages in the scatterlist as dirty since they
		 * were writable by the GPU.
		 */
		for_each_sg(entry->memdesc.sgt->sgl, sg,
			    entry->memdesc.sgt->nents, i) {
			page = sg_page(sg);
			for (j = 0; j < (sg->length >> PAGE_SHIFT); j++)
				set_page_dirty(nth_page(page, j));
		}
	}

	kgsl_sharedmem_free(&entry->memdesc);

	switch (memtype) {
	case KGSL_MEM_ENTRY_ION:
		kgsl_destroy_ion(entry->priv_data);
		break;
	default:
		break;
	}

	kfree(entry);
}
EXPORT_SYMBOL(kgsl_mem_entry_destroy);

/* Allocate a IOVA for memory objects that don't use SVM */
static int kgsl_mem_entry_track_gpuaddr(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	struct kgsl_pagetable *pagetable;

	/*
	 * If SVM is enabled for this object then the address needs to be
	 * assigned elsewhere
	 * Also do not proceed further in case of NoMMU.
	 */
	if (kgsl_memdesc_use_cpu_map(&entry->memdesc) ||
		(kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE))
		return 0;

	pagetable = kgsl_memdesc_is_secured(&entry->memdesc) ?
		device->mmu.securepagetable : process->pagetable;

	return kgsl_mmu_get_gpuaddr(pagetable, &entry->memdesc);
}

/* Commit the entry to the process so it can be accessed by other operations */
static void kgsl_mem_entry_commit_process(struct kgsl_mem_entry *entry)
{
	if (!entry)
		return;

	spin_lock(&entry->priv->mem_lock);
	idr_replace(&entry->priv->mem_idr, entry, entry->id);
	spin_unlock(&entry->priv->mem_lock);
}

/*
 * Attach the memory object to a process by (possibly) getting a GPU address and
 * (possibly) mapping it
 */
static int kgsl_mem_entry_attach_process(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	int id, ret;

	ret = kgsl_process_private_get(process);
	if (!ret)
		return -EBADF;

	ret = kgsl_mem_entry_track_gpuaddr(device, process, entry);
	if (ret) {
		kgsl_process_private_put(process);
		return ret;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&process->mem_lock);
	/* Allocate the ID but don't attach the pointer just yet */
	id = idr_alloc(&process->mem_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&process->mem_lock);
	idr_preload_end();

	if (id < 0) {
		if (!kgsl_memdesc_use_cpu_map(&entry->memdesc))
			kgsl_mmu_put_gpuaddr(&entry->memdesc);
		kgsl_process_private_put(process);
		return id;
	}

	entry->id = id;
	entry->priv = process;

	/*
	 * Map the memory if a GPU address is already assigned, either through
	 * kgsl_mem_entry_track_gpuaddr() or via some other SVM process
	 */
	if (entry->memdesc.gpuaddr) {
		if (entry->memdesc.flags & KGSL_MEMFLAGS_SPARSE_VIRT)
			ret = kgsl_mmu_sparse_dummy_map(
					entry->memdesc.pagetable,
					&entry->memdesc, 0,
					entry->memdesc.size);
		else if (entry->memdesc.gpuaddr)
			ret = kgsl_mmu_map(entry->memdesc.pagetable,
					&entry->memdesc);

		if (ret)
			kgsl_mem_entry_detach_process(entry);
	}

	kgsl_memfree_purge(entry->memdesc.pagetable, entry->memdesc.gpuaddr,
		entry->memdesc.size);

	return ret;
}

/* Detach a memory entry from a process and unmap it from the MMU */
static void kgsl_mem_entry_detach_process(struct kgsl_mem_entry *entry)
{
	unsigned int type;

	if (entry == NULL)
		return;

	/*
	 * First remove the entry from mem_idr list
	 * so that no one can operate on obsolete values
	 */
	spin_lock(&entry->priv->mem_lock);
	if (entry->id != 0)
		idr_remove(&entry->priv->mem_idr, entry->id);
	entry->id = 0;

	type = kgsl_memdesc_usermem_type(&entry->memdesc);
	entry->priv->stats[type].cur -= entry->memdesc.size;
	spin_unlock(&entry->priv->mem_lock);

	kgsl_mmu_put_gpuaddr(&entry->memdesc);

	kgsl_process_private_put(entry->priv);

	entry->priv = NULL;
}

/**
 * kgsl_context_dump() - dump information about a draw context
 * @device: KGSL device that owns the context
 * @context: KGSL context to dump information about
 *
 * Dump specific information about the context to the kernel log.  Used for
 * fence timeout callbacks
 */
void kgsl_context_dump(struct kgsl_context *context)
{
	struct kgsl_device *device;

	if (_kgsl_context_get(context) == 0)
		return;

	device = context->device;

	if (kgsl_context_detached(context)) {
		dev_err(device->dev, "  context[%d]: context detached\n",
			context->id);
	} else if (device->ftbl->drawctxt_dump != NULL)
		device->ftbl->drawctxt_dump(device, context);

	kgsl_context_put(context);
}
EXPORT_SYMBOL(kgsl_context_dump);

/* Allocate a new context ID */
static int _kgsl_get_context_id(struct kgsl_device *device)
{
	int id;

	idr_preload(GFP_KERNEL);
	write_lock(&device->context_lock);
	/* Allocate the slot but don't put a pointer in it yet */
	id = idr_alloc(&device->context_idr, NULL, 1,
		KGSL_MEMSTORE_MAX, GFP_NOWAIT);
	write_unlock(&device->context_lock);
	idr_preload_end();

	return id;
}

/**
 * kgsl_context_init() - helper to initialize kgsl_context members
 * @dev_priv: the owner of the context
 * @context: the newly created context struct, should be allocated by
 * the device specific drawctxt_create function.
 *
 * This is a helper function for the device specific drawctxt_create
 * function to initialize the common members of its context struct.
 * If this function succeeds, reference counting is active in the context
 * struct and the caller should kgsl_context_put() it on error.
 * If it fails, the caller should just free the context structure
 * it passed in.
 */
int kgsl_context_init(struct kgsl_device_private *dev_priv,
			struct kgsl_context *context)
{
	struct kgsl_device *device = dev_priv->device;
	char name[64];
	int ret = 0, id;

	id = _kgsl_get_context_id(device);
	if (id == -ENOSPC) {
		/*
		 * Before declaring that there are no contexts left try
		 * flushing the event workqueue just in case there are
		 * detached contexts waiting to finish
		 */

		flush_workqueue(device->events_wq);
		id = _kgsl_get_context_id(device);
	}

	if (id < 0) {
		if (id == -ENOSPC)
			KGSL_DRV_INFO(device,
				"cannot have more than %zu contexts due to memstore limitation\n",
				KGSL_MEMSTORE_MAX);

		return id;
	}

	context->id = id;

	kref_init(&context->refcount);
	/*
	 * Get a refernce to the process private so its not destroyed, until
	 * the context is destroyed. This will also prevent the pagetable
	 * from being destroyed
	 */
	if (!kgsl_process_private_get(dev_priv->process_priv)) {
		ret = -EBADF;
		goto out;
	}
	context->device = dev_priv->device;
	context->dev_priv = dev_priv;
	context->proc_priv = dev_priv->process_priv;
	context->tid = task_pid_nr(current);

	ret = kgsl_sync_timeline_create(context);
	if (ret)
		goto out;

	snprintf(name, sizeof(name), "context-%d", id);
	kgsl_add_event_group(&context->events, context, name,
		kgsl_readtimestamp, context);

out:
	if (ret) {
		write_lock(&device->context_lock);
		idr_remove(&dev_priv->device->context_idr, id);
		write_unlock(&device->context_lock);
	}

	return ret;
}
EXPORT_SYMBOL(kgsl_context_init);

/**
 * kgsl_context_detach() - Release the "master" context reference
 * @context: The context that will be detached
 *
 * This is called when a context becomes unusable, because userspace
 * has requested for it to be destroyed. The context itself may
 * exist a bit longer until its reference count goes to zero.
 * Other code referencing the context can detect that it has been
 * detached by checking the KGSL_CONTEXT_PRIV_DETACHED bit in
 * context->priv.
 */
static void kgsl_context_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;

	if (context == NULL)
		return;

	/*
	 * Mark the context as detached to keep others from using
	 * the context before it gets fully removed, and to make sure
	 * we don't try to detach twice.
	 */
	if (test_and_set_bit(KGSL_CONTEXT_PRIV_DETACHED, &context->priv))
		return;

	device = context->device;

	trace_kgsl_context_detach(device, context);

	context->device->ftbl->drawctxt_detach(context);

	/*
	 * Cancel all pending events after the device-specific context is
	 * detached, to avoid possibly freeing memory while it is still
	 * in use by the GPU.
	 */
	kgsl_cancel_events(device, &context->events);

	/* Remove the event group from the list */
	kgsl_del_event_group(&context->events);

	kgsl_context_put(context);
}

void
kgsl_context_destroy(struct kref *kref)
{
	struct kgsl_context *context = container_of(kref, struct kgsl_context,
						    refcount);
	struct kgsl_device *device = context->device;

	trace_kgsl_context_destroy(device, context);

	BUG_ON(!kgsl_context_detached(context));

	write_lock(&device->context_lock);
	if (context->id != KGSL_CONTEXT_INVALID) {

		/* Clear the timestamps in the memstore during destroy */
		kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp), 0);
		kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp), 0);

		/* clear device power constraint */
		if (context->id == device->pwrctrl.constraint.owner_id) {
			trace_kgsl_constraint(device,
				device->pwrctrl.constraint.type,
				device->pwrctrl.active_pwrlevel,
				0);
			device->pwrctrl.constraint.type = KGSL_CONSTRAINT_NONE;
		}

		idr_remove(&device->context_idr, context->id);
		context->id = KGSL_CONTEXT_INVALID;
	}
	write_unlock(&device->context_lock);
	kgsl_sync_timeline_destroy(context);
	kgsl_process_private_put(context->proc_priv);

	device->ftbl->drawctxt_destroy(context);
}

struct kgsl_device *kgsl_get_device(int dev_idx)
{
	int i;
	struct kgsl_device *ret = NULL;

	mutex_lock(&kgsl_driver.devlock);

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->id == dev_idx) {
			ret = kgsl_driver.devp[i];
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}
EXPORT_SYMBOL(kgsl_get_device);

static struct kgsl_device *kgsl_get_minor(int minor)
{
	struct kgsl_device *ret = NULL;

	if (minor < 0 || minor >= KGSL_DEVICE_MAX)
		return NULL;

	mutex_lock(&kgsl_driver.devlock);
	ret = kgsl_driver.devp[minor];
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
}

/**
 * kgsl_check_timestamp() - return true if the specified timestamp is retired
 * @device: Pointer to the KGSL device to check
 * @context: Pointer to the context for the timestamp
 * @timestamp: The timestamp to compare
 */
int kgsl_check_timestamp(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	unsigned int ts_processed;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&ts_processed);

	return (timestamp_cmp(ts_processed, timestamp) >= 0);
}
EXPORT_SYMBOL(kgsl_check_timestamp);

static int kgsl_suspend_device(struct kgsl_device *device, pm_message_t state)
{
	int status = -EINVAL;

	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "suspend start\n");

	mutex_lock(&device->mutex);
	status = kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	mutex_unlock(&device->mutex);

	KGSL_PWR_WARN(device, "suspend end\n");
	return status;
}

static int kgsl_resume_device(struct kgsl_device *device)
{
	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "resume start\n");
	mutex_lock(&device->mutex);
	if (device->state == KGSL_STATE_SUSPEND) {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
	} else if (device->state != KGSL_STATE_INIT) {
		/*
		 * This is an error situation,so wait for the device
		 * to idle and then put the device to SLUMBER state.
		 * This will put the device to the right state when
		 * we resume.
		 */
		if (device->state == KGSL_STATE_ACTIVE)
			device->ftbl->idle(device);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
		KGSL_PWR_ERR(device,
			"resume invoked without a suspend\n");
	}

	mutex_unlock(&device->mutex);
	KGSL_PWR_WARN(device, "resume end\n");
	return 0;
}

static int kgsl_suspend(struct device *dev)
{

	pm_message_t arg = {0};
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_suspend_device(device, arg);
}

static int kgsl_resume(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_resume_device(device);
}

static int kgsl_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kgsl_runtime_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops kgsl_pm_ops = {
	.suspend = kgsl_suspend,
	.resume = kgsl_resume,
	.runtime_suspend = kgsl_runtime_suspend,
	.runtime_resume = kgsl_runtime_resume,
};
EXPORT_SYMBOL(kgsl_pm_ops);

int kgsl_suspend_driver(struct platform_device *pdev,
					pm_message_t state)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_suspend_device(device, state);
}
EXPORT_SYMBOL(kgsl_suspend_driver);

int kgsl_resume_driver(struct platform_device *pdev)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_resume_device(device);
}
EXPORT_SYMBOL(kgsl_resume_driver);

/**
 * kgsl_destroy_process_private() - Cleanup function to free process private
 * @kref: - Pointer to object being destroyed's kref struct
 * Free struct object and all other resources attached to it.
 * Since the function can be used when not all resources inside process
 * private have been allocated, there is a check to (before each resource
 * cleanup) see if the struct member being cleaned is in fact allocated or not.
 * If the value is not NULL, resource is freed.
 */
static void kgsl_destroy_process_private(struct kref *kref)
{
	struct kgsl_process_private *private = container_of(kref,
			struct kgsl_process_private, refcount);

	idr_destroy(&private->mem_idr);
	idr_destroy(&private->syncsource_idr);

	/* When using global pagetables, do not detach global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_putpagetable(private->pagetable);

	kfree(private);
	return;
}

void
kgsl_process_private_put(struct kgsl_process_private *private)
{
	if (private)
		kref_put(&private->refcount, kgsl_destroy_process_private);
}

/**
 * kgsl_process_private_find() - Find the process associated with the specified
 * name
 * @name: pid_t of the process to search for
 * Return the process struct for the given ID.
 */
struct kgsl_process_private *kgsl_process_private_find(pid_t pid)
{
	struct kgsl_process_private *p, *private = NULL;

	mutex_lock(&kgsl_driver.process_mutex);
	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		if (p->pid == pid) {
			if (kgsl_process_private_get(p))
				private = p;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}

static struct kgsl_process_private *kgsl_process_private_new(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;
	pid_t tgid = task_tgid_nr(current);

	/* Search in the process list */
	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == tgid) {
			if (!kgsl_process_private_get(private))
				private = ERR_PTR(-EINVAL);
			return private;
		}
	}

	/* Create a new object */
	private = kzalloc(sizeof(struct kgsl_process_private), GFP_KERNEL);
	if (private == NULL)
		return ERR_PTR(-ENOMEM);

	kref_init(&private->refcount);

	private->pid = tgid;
	get_task_comm(private->comm, current->group_leader);

	spin_lock_init(&private->mem_lock);
	spin_lock_init(&private->syncsource_lock);

	idr_init(&private->mem_idr);
	idr_init(&private->syncsource_idr);

	/* Allocate a pagetable for the new process object */
	private->pagetable = kgsl_mmu_getpagetable(&device->mmu, tgid);
	if (IS_ERR(private->pagetable)) {
		int err = PTR_ERR(private->pagetable);

		idr_destroy(&private->mem_idr);
		idr_destroy(&private->syncsource_idr);

		kfree(private);
		private = ERR_PTR(err);
	}

	return private;
}

static void process_release_memory(struct kgsl_process_private *private)
{
	struct kgsl_mem_entry *entry;
	int next = 0;

	while (1) {
		spin_lock(&private->mem_lock);
		entry = idr_get_next(&private->mem_idr, &next);
		if (entry == NULL) {
			spin_unlock(&private->mem_lock);
			break;
		}
		/*
		 * If the free pending flag is not set it means that user space
		 * did not free it's reference to this entry, in that case
		 * free a reference to this entry, other references are from
		 * within kgsl so they will be freed eventually by kgsl
		 */
		if (!entry->pending_free) {
			entry->pending_free = 1;
			spin_unlock(&private->mem_lock);
			kgsl_mem_entry_put(entry);
		} else {
			spin_unlock(&private->mem_lock);
		}
		next = next + 1;
	}
}

static void process_release_sync_sources(struct kgsl_process_private *private)
{
	struct kgsl_syncsource *syncsource;
	int next = 0;

	while (1) {
		spin_lock(&private->syncsource_lock);
		syncsource = idr_get_next(&private->syncsource_idr, &next);
		spin_unlock(&private->syncsource_lock);

		if (syncsource == NULL)
			break;

		kgsl_syncsource_put(syncsource);
		next = next + 1;
	}
}

static void kgsl_process_private_close(struct kgsl_device_private *dev_priv,
		struct kgsl_process_private *private)
{
	mutex_lock(&kgsl_driver.process_mutex);

	if (--private->fd_count > 0) {
		mutex_unlock(&kgsl_driver.process_mutex);
		kgsl_process_private_put(private);
		return;
	}

	/*
	 * If this is the last file on the process take down the debug
	 * directories and garbage collect any outstanding resources
	 */

	kgsl_process_uninit_sysfs(private);
	debugfs_remove_recursive(private->debug_root);

	process_release_sync_sources(private);

	/* When using global pagetables, do not detach global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_detach_pagetable(private->pagetable);

	/* Remove the process struct from the master list */
	list_del(&private->list);

	/*
	 * Unlock the mutex before releasing the memory - this prevents a
	 * deadlock with the IOMMU mutex if a page fault occurs
	 */
	mutex_unlock(&kgsl_driver.process_mutex);

	process_release_memory(private);

	kgsl_process_private_put(private);
}


static struct kgsl_process_private *kgsl_process_private_open(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;

	mutex_lock(&kgsl_driver.process_mutex);
	private = kgsl_process_private_new(device);

	if (IS_ERR(private))
		goto done;

	/*
	 * If this is a new process create the debug directories and add it to
	 * the process list
	 */

	if (private->fd_count++ == 0) {
		kgsl_process_init_sysfs(device, private);
		kgsl_process_init_debugfs(private);

		list_add(&private->list, &kgsl_driver.process_list);
	}

done:
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}

static int kgsl_close_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	device->open_count--;
	if (device->open_count == 0) {

		/* Wait for the active count to go to 0 */
		kgsl_active_count_wait(device, 0);

		/* Fail if the wait times out */
		BUG_ON(atomic_read(&device->active_cnt) > 0);

		result = kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	}
	mutex_unlock(&device->mutex);
	return result;

}

static void device_release_contexts(struct kgsl_device_private *dev_priv)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	int next = 0;
	int result = 0;

	while (1) {
		read_lock(&device->context_lock);
		context = idr_get_next(&device->context_idr, &next);

		if (context == NULL) {
			read_unlock(&device->context_lock);
			break;
		} else if (context->dev_priv == dev_priv) {
			/*
			 * Hold a reference to the context in case somebody
			 * tries to put it while we are detaching
			 */
			result = _kgsl_context_get(context);
		}
		read_unlock(&device->context_lock);

		if (result) {
			kgsl_context_detach(context);
			kgsl_context_put(context);
			result = 0;
		}

		next = next + 1;
	}
}

static int kgsl_release(struct inode *inodep, struct file *filep)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_device *device = dev_priv->device;
	int result;

	filep->private_data = NULL;

	/* Release the contexts for the file */
	device_release_contexts(dev_priv);

	/* Close down the process wide resources for the file */
	kgsl_process_private_close(dev_priv, dev_priv->process_priv);

	kfree(dev_priv);

	result = kgsl_close_device(device);
	pm_runtime_put(&device->pdev->dev);

	return result;
}

static int kgsl_open_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	if (device->open_count == 0) {
		/*
		 * active_cnt special case: we are starting up for the first
		 * time, so use this sequence instead of the kgsl_pwrctrl_wake()
		 * which will be called by kgsl_active_count_get().
		 */
		atomic_inc(&device->active_cnt);
		kgsl_sharedmem_set(device, &device->memstore, 0, 0,
				device->memstore.size);
		kgsl_sharedmem_set(device, &device->scratch, 0, 0,
				device->scratch.size);

		result = device->ftbl->init(device);
		if (result)
			goto err;

		result = device->ftbl->start(device, 0);
		if (result)
			goto err;
		/*
		 * Make sure the gates are open, so they don't block until
		 * we start suspend or FT.
		 */
		complete_all(&device->hwaccess_gate);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		kgsl_active_count_put(device);
	}
	device->open_count++;
err:
	if (result) {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
		atomic_dec(&device->active_cnt);
	}

	mutex_unlock(&device->mutex);
	return result;
}

static int kgsl_open(struct inode *inodep, struct file *filep)
{
	int result;
	struct kgsl_device_private *dev_priv;
	struct kgsl_device *device;
	unsigned int minor = iminor(inodep);

	device = kgsl_get_minor(minor);
	BUG_ON(device == NULL);

	result = pm_runtime_get_sync(&device->pdev->dev);
	if (result < 0) {
		KGSL_DRV_ERR(device,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}
	result = 0;

	dev_priv = kzalloc(sizeof(struct kgsl_device_private), GFP_KERNEL);
	if (dev_priv == NULL) {
		result = -ENOMEM;
		goto err;
	}

	dev_priv->device = device;
	filep->private_data = dev_priv;

	result = kgsl_open_device(device);
	if (result)
		goto err;

	/*
	 * Get file (per process) private struct. This must be done
	 * after the first start so that the global pagetable mappings
	 * are set up before we create the per-process pagetable.
	 */
	dev_priv->process_priv = kgsl_process_private_open(device);
	if (IS_ERR(dev_priv->process_priv)) {
		result = PTR_ERR(dev_priv->process_priv);
		kgsl_close_device(device);
		goto err;
	}

err:
	if (result) {
		filep->private_data = NULL;
		kfree(dev_priv);
		pm_runtime_put(&device->pdev->dev);
	}
	return result;
}

#define GPUADDR_IN_MEMDESC(_val, _memdesc) \
	(((_val) >= (_memdesc)->gpuaddr) && \
	 ((_val) < ((_memdesc)->gpuaddr + (_memdesc)->size)))

/**
 * kgsl_sharedmem_find() - Find a gpu memory allocation
 *
 * @private: private data for the process to check.
 * @gpuaddr: start address of the region
 *
 * Find a gpu allocation. Caller must kgsl_mem_entry_put()
 * the returned entry when finished using it.
 */
struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find(struct kgsl_process_private *private, uint64_t gpuaddr)
{
	int ret = 0, id;
	struct kgsl_mem_entry *entry = NULL;

	if (!private)
		return NULL;

	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, gpuaddr))
		return NULL;

	spin_lock(&private->mem_lock);
	idr_for_each_entry(&private->mem_idr, entry, id) {
		if (GPUADDR_IN_MEMDESC(gpuaddr, &entry->memdesc)) {
			ret = kgsl_mem_entry_get(entry);
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	return (ret == 0) ? NULL : entry;
}
EXPORT_SYMBOL(kgsl_sharedmem_find);

struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id_flags(struct kgsl_process_private *process,
		unsigned int id, uint64_t flags)
{
	int count = 0;
	struct kgsl_mem_entry *entry;

	spin_lock(&process->mem_lock);
	entry = idr_find(&process->mem_idr, id);
	if (entry)
		if (!entry->pending_free &&
				(flags & entry->memdesc.flags) == flags)
			count = kgsl_mem_entry_get(entry);
	spin_unlock(&process->mem_lock);

	return (count == 0) ? NULL : entry;
}

/**
 * kgsl_sharedmem_find_id() - find a memory entry by id
 * @process: the owning process
 * @id: id to find
 *
 * @returns - the mem_entry or NULL
 *
 * Caller must kgsl_mem_entry_put() the returned entry, when finished using
 * it.
 */
struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id(struct kgsl_process_private *process, unsigned int id)
{
	return kgsl_sharedmem_find_id_flags(process, id, 0);
}

/**
 * kgsl_mem_entry_unset_pend() - Unset the pending free flag of an entry
 * @entry - The memory entry
 */
static inline void kgsl_mem_entry_unset_pend(struct kgsl_mem_entry *entry)
{
	if (entry == NULL)
		return;
	spin_lock(&entry->priv->mem_lock);
	entry->pending_free = 0;
	spin_unlock(&entry->priv->mem_lock);
}

/**
 * kgsl_mem_entry_set_pend() - Set the pending free flag of a memory entry
 * @entry - The memory entry
 *
 * @returns - true if pending flag was 0 else false
 *
 * This function will set the pending free flag if it is previously unset. Used
 * to prevent race condition between ioctls calling free/freememontimestamp
 * on the same entry. Whichever thread set's the flag first will do the free.
 */
static inline bool kgsl_mem_entry_set_pend(struct kgsl_mem_entry *entry)
{
	bool ret = false;

	if (entry == NULL)
		return false;

	spin_lock(&entry->priv->mem_lock);
	if (!entry->pending_free) {
		entry->pending_free = 1;
		ret = true;
	}
	spin_unlock(&entry->priv->mem_lock);
	return ret;
}

/*call all ioctl sub functions with driver locked*/
long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_device_getproperty *param = data;

	switch (param->type) {
	case KGSL_PROP_VERSION:
	{
		struct kgsl_version version;
		if (param->sizebytes != sizeof(version)) {
			result = -EINVAL;
			break;
		}

		version.drv_major = KGSL_VERSION_MAJOR;
		version.drv_minor = KGSL_VERSION_MINOR;
		version.dev_major = dev_priv->device->ver_major;
		version.dev_minor = dev_priv->device->ver_minor;

		if (copy_to_user(param->value, &version, sizeof(version)))
			result = -EFAULT;

		break;
	}
	case KGSL_PROP_GPU_RESET_STAT:
	{
		/* Return reset status of given context and clear it */
		uint32_t id;
		struct kgsl_context *context;

		if (param->sizebytes != sizeof(unsigned int)) {
			result = -EINVAL;
			break;
		}
		/* We expect the value passed in to contain the context id */
		if (copy_from_user(&id, param->value,
			sizeof(unsigned int))) {
			result = -EFAULT;
			break;
		}
		context = kgsl_context_get_owner(dev_priv, id);
		if (!context) {
			result = -EINVAL;
			break;
		}
		/*
		 * Copy the reset status to value which also serves as
		 * the out parameter
		 */
		if (copy_to_user(param->value, &(context->reset_status),
			sizeof(unsigned int)))
			result = -EFAULT;
		else {
			/* Clear reset status once its been queried */
			context->reset_status = KGSL_CTX_STAT_NO_ERROR;
		}

		kgsl_context_put(context);
		break;
	}
	default:
		if (is_compat_task())
			result = dev_priv->device->ftbl->getproperty_compat(
					dev_priv->device, param->type,
					param->value, param->sizebytes);
		else
			result = dev_priv->device->ftbl->getproperty(
					dev_priv->device, param->type,
					param->value, param->sizebytes);
	}


	return result;
}

long kgsl_ioctl_device_setproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	int result = 0;
	/* The getproperty struct is reused for setproperty too */
	struct kgsl_device_getproperty *param = data;

	/* Reroute to compat version if coming from compat_ioctl */
	if (is_compat_task())
		result = dev_priv->device->ftbl->setproperty_compat(
			dev_priv, param->type, param->value,
			param->sizebytes);
	else if (dev_priv->device->ftbl->setproperty)
		result = dev_priv->device->ftbl->setproperty(
			dev_priv, param->type, param->value,
			param->sizebytes);

	return result;
}

long kgsl_ioctl_device_waittimestamp_ctxtid(
		struct kgsl_device_private *dev_priv, unsigned int cmd,
		void *data)
{
	struct kgsl_device_waittimestamp_ctxtid *param = data;
	struct kgsl_device *device = dev_priv->device;
	long result = -EINVAL;
	unsigned int temp_cur_ts = 0;
	struct kgsl_context *context;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return result;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&temp_cur_ts);

	trace_kgsl_waittimestamp_entry(device, context->id, temp_cur_ts,
		param->timestamp, param->timeout);

	result = device->ftbl->waittimestamp(device, context, param->timestamp,
		param->timeout);

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&temp_cur_ts);
	trace_kgsl_waittimestamp_exit(device, temp_cur_ts, result);

	kgsl_context_put(context);

	return result;
}

static inline bool _check_context_is_sparse(struct kgsl_context *context,
			uint64_t flags)
{
	if ((context->flags & KGSL_CONTEXT_SPARSE) ||
		(flags & KGSL_DRAWOBJ_SPARSE))
		return true;

	return false;
}


long kgsl_ioctl_rb_issueibcmds(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	struct kgsl_ringbuffer_issueibcmds *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj;
	struct kgsl_drawobj_cmd *cmdobj;
	long result = -EINVAL;

	/* The legacy functions don't support synchronization commands */
	if ((param->flags & (KGSL_DRAWOBJ_SYNC | KGSL_DRAWOBJ_MARKER)))
		return -EINVAL;

	/* Sanity check the number of IBs */
	if (param->flags & KGSL_DRAWOBJ_SUBMIT_IB_LIST &&
			(param->numibs == 0 || param->numibs > KGSL_MAX_NUMIBS))
		return -EINVAL;

	/* Get the context */
	context = kgsl_context_get_owner(dev_priv, param->drawctxt_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	cmdobj = kgsl_drawobj_cmd_create(device, context, param->flags,
					CMDOBJ_TYPE);
	if (IS_ERR(cmdobj)) {
		kgsl_context_put(context);
		return PTR_ERR(cmdobj);
	}

	drawobj = DRAWOBJ(cmdobj);

	if (param->flags & KGSL_DRAWOBJ_SUBMIT_IB_LIST)
		result = kgsl_drawobj_cmd_add_ibdesc_list(device, cmdobj,
			(void __user *) param->ibdesc_addr,
			param->numibs);
	else {
		struct kgsl_ibdesc ibdesc;
		/* Ultra legacy path */

		ibdesc.gpuaddr = param->ibdesc_addr;
		ibdesc.sizedwords = param->numibs;
		ibdesc.ctrl = 0;

		result = kgsl_drawobj_cmd_add_ibdesc(device, cmdobj, &ibdesc);
	}

	if (result == 0)
		result = dev_priv->device->ftbl->queue_cmds(dev_priv, context,
				&drawobj, 1, &param->timestamp);

	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		kgsl_drawobj_destroy(drawobj);

	kgsl_context_put(context);
	return result;
}

/* Returns 0 on failure.  Returns command type(s) on success */
static unsigned int _process_command_input(struct kgsl_device *device,
		unsigned int flags, unsigned int numcmds,
		unsigned int numobjs, unsigned int numsyncs)
{
	if (numcmds > KGSL_MAX_NUMIBS ||
			numobjs > KGSL_MAX_NUMIBS ||
			numsyncs > KGSL_MAX_SYNCPOINTS)
		return 0;

	/*
	 * The SYNC bit is supposed to identify a dummy sync object
	 * so warn the user if they specified any IBs with it.
	 * A MARKER command can either have IBs or not but if the
	 * command has 0 IBs it is automatically assumed to be a marker.
	 */

	/* If they specify the flag, go with what they say */
	if (flags & KGSL_DRAWOBJ_MARKER)
		return MARKEROBJ_TYPE;
	else if (flags & KGSL_DRAWOBJ_SYNC)
		return SYNCOBJ_TYPE;

	/* If not, deduce what they meant */
	if (numsyncs && numcmds)
		return SYNCOBJ_TYPE | CMDOBJ_TYPE;
	else if (numsyncs)
		return SYNCOBJ_TYPE;
	else if (numcmds)
		return CMDOBJ_TYPE;
	else if (numcmds == 0)
		return MARKEROBJ_TYPE;

	return 0;
}

long kgsl_ioctl_submit_commands(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	struct kgsl_submit_commands *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	unsigned int type;
	long result;
	unsigned int i = 0;

	type = _process_command_input(device, param->flags, param->numcmds, 0,
			param->numsyncs);
	if (!type)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj =
				kgsl_drawobj_sync_create(device, context);
		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);

		result = kgsl_drawobj_sync_add_syncpoints(device, syncobj,
				param->synclist, param->numsyncs);
		if (result)
			goto done;
	}

	if (type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)) {
		struct kgsl_drawobj_cmd *cmdobj =
				kgsl_drawobj_cmd_create(device,
					context, param->flags, type);
		if (IS_ERR(cmdobj)) {
			result = PTR_ERR(cmdobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(cmdobj);

		result = kgsl_drawobj_cmd_add_ibdesc_list(device, cmdobj,
				param->cmdlist, param->numcmds);
		if (result)
			goto done;

		/* If no profiling buffer was specified, clear the flag */
		if (cmdobj->profiling_buf_entry == NULL)
			DRAWOBJ(cmdobj)->flags &= ~KGSL_DRAWOBJ_PROFILING;
	}

	result = device->ftbl->queue_cmds(dev_priv, context, drawobj,
			i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);


	kgsl_context_put(context);
	return result;
}

long kgsl_ioctl_gpu_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpu_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	unsigned int type;
	long result;
	unsigned int i = 0;

	type = _process_command_input(device, param->flags, param->numcmds,
			param->numobjs, param->numsyncs);
	if (!type)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj =
				kgsl_drawobj_sync_create(device, context);

		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);

		result = kgsl_drawobj_sync_add_synclist(device, syncobj,
				to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (result)
			goto done;
	}

	if (type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)) {
		struct kgsl_drawobj_cmd *cmdobj =
				kgsl_drawobj_cmd_create(device,
					context, param->flags, type);

		if (IS_ERR(cmdobj)) {
			result = PTR_ERR(cmdobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(cmdobj);

		result = kgsl_drawobj_cmd_add_cmdlist(device, cmdobj,
			to_user_ptr(param->cmdlist),
			param->cmdsize, param->numcmds);
		if (result)
			goto done;

		result = kgsl_drawobj_cmd_add_memlist(device, cmdobj,
			to_user_ptr(param->objlist),
			param->objsize, param->numobjs);
		if (result)
			goto done;

		/* If no profiling buffer was specified, clear the flag */
		if (cmdobj->profiling_buf_entry == NULL)
			DRAWOBJ(cmdobj)->flags &= ~KGSL_DRAWOBJ_PROFILING;
	}

	result = device->ftbl->queue_cmds(dev_priv, context, drawobj,
				i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);

	kgsl_context_put(context);
	return result;
}

long kgsl_ioctl_cmdstream_readtimestamp_ctxtid(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	struct kgsl_cmdstream_readtimestamp_ctxtid *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	long result = -EINVAL;

	mutex_lock(&device->mutex);
	context = kgsl_context_get_owner(dev_priv, param->context_id);

	if (context) {
		result = kgsl_readtimestamp(device, context,
			param->type, &param->timestamp);

		trace_kgsl_readtimestamp(device, context->id,
			param->type, param->timestamp);
	}

	kgsl_context_put(context);
	mutex_unlock(&device->mutex);
	return result;
}

long kgsl_ioctl_drawctxt_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_create *param = data;
	struct kgsl_context *context = NULL;
	struct kgsl_device *device = dev_priv->device;

	context = device->ftbl->drawctxt_create(dev_priv, &param->flags);
	if (IS_ERR(context)) {
		result = PTR_ERR(context);
		goto done;
	}
	trace_kgsl_context_create(dev_priv->device, context, param->flags);

	/* Commit the pointer to the context in context_idr */
	write_lock(&device->context_lock);
	idr_replace(&device->context_idr, context, context->id);
	write_unlock(&device->context_lock);

	param->drawctxt_id = context->id;
done:
	return result;
}

long kgsl_ioctl_drawctxt_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_drawctxt_destroy *param = data;
	struct kgsl_context *context;

	context = kgsl_context_get_owner(dev_priv, param->drawctxt_id);
	if (context == NULL)
		return -EINVAL;

	kgsl_context_detach(context);
	kgsl_context_put(context);

	return 0;
}

static long gpumem_free_entry(struct kgsl_mem_entry *entry)
{
	pid_t ptname = 0;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	trace_kgsl_mem_free(entry);

	if (entry->memdesc.pagetable != NULL)
		ptname = entry->memdesc.pagetable->name;

	kgsl_memfree_add(entry->priv->pid, ptname, entry->memdesc.gpuaddr,
		entry->memdesc.size, entry->memdesc.flags);

	kgsl_mem_entry_put(entry);

	return 0;
}

static void gpumem_free_func(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int ret)
{
	struct kgsl_context *context = group->context;
	struct kgsl_mem_entry *entry = priv;
	unsigned int timestamp;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &timestamp);

	/* Free the memory for all event types */
	trace_kgsl_mem_timestamp_free(device, entry, KGSL_CONTEXT_ID(context),
		timestamp, 0);
	kgsl_mem_entry_put(entry);
}

static long gpumem_free_entry_on_timestamp(struct kgsl_device *device,
		struct kgsl_mem_entry *entry,
		struct kgsl_context *context, unsigned int timestamp)
{
	int ret;
	unsigned int temp;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &temp);
	trace_kgsl_mem_timestamp_queue(device, entry, context->id, temp,
		timestamp);
	ret = kgsl_add_event(device, &context->events,
		timestamp, gpumem_free_func, entry);

	if (ret)
		kgsl_mem_entry_unset_pend(entry);

	return ret;
}

long kgsl_ioctl_sharedmem_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);
	if (entry == NULL)
		return -EINVAL;

	ret = gpumem_free_entry(entry);
	kgsl_mem_entry_put(entry);

	return ret;
}

long kgsl_ioctl_gpumem_free_id(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_gpumem_free_id *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	ret = gpumem_free_entry(entry);
	kgsl_mem_entry_put(entry);

	return ret;
}

static long gpuobj_free_on_timestamp(struct kgsl_device_private *dev_priv,
		struct kgsl_mem_entry *entry, struct kgsl_gpuobj_free *param)
{
	struct kgsl_gpu_event_timestamp event;
	struct kgsl_context *context;
	long ret;

	memset(&event, 0, sizeof(event));

	ret = _copy_from_user(&event, to_user_ptr(param->priv),
		sizeof(event), param->len);
	if (ret)
		return ret;

	if (event.context_id == 0)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, event.context_id);
	if (context == NULL)
		return -EINVAL;

	ret = gpumem_free_entry_on_timestamp(dev_priv->device, entry, context,
		event.timestamp);

	kgsl_context_put(context);
	return ret;
}

static void gpuobj_free_fence_func(void *priv)
{
	struct kgsl_mem_entry *entry = priv;

	INIT_WORK(&entry->work, _deferred_put);
	queue_work(kgsl_driver.mem_workqueue, &entry->work);
}

static long gpuobj_free_on_fence(struct kgsl_device_private *dev_priv,
		struct kgsl_mem_entry *entry, struct kgsl_gpuobj_free *param)
{
	struct kgsl_sync_fence_waiter *handle;
	struct kgsl_gpu_event_fence event;
	long ret;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	memset(&event, 0, sizeof(event));

	ret = _copy_from_user(&event, to_user_ptr(param->priv),
		sizeof(event), param->len);
	if (ret) {
		kgsl_mem_entry_unset_pend(entry);
		return ret;
	}

	if (event.fd < 0) {
		kgsl_mem_entry_unset_pend(entry);
		return -EINVAL;
	}

	handle = kgsl_sync_fence_async_wait(event.fd,
		gpuobj_free_fence_func, entry);

	/* if handle is NULL the fence has already signaled */
	if (handle == NULL)
		return gpumem_free_entry(entry);

	if (IS_ERR(handle)) {
		kgsl_mem_entry_unset_pend(entry);
		return PTR_ERR(handle);
	}

	return 0;
}

long kgsl_ioctl_gpuobj_free(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpuobj_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	/* If no event is specified then free immediately */
	if (!(param->flags & KGSL_GPUOBJ_FREE_ON_EVENT))
		ret = gpumem_free_entry(entry);
	else if (param->type == KGSL_GPU_EVENT_TIMESTAMP)
		ret = gpuobj_free_on_timestamp(dev_priv, entry, param);
	else if (param->type == KGSL_GPU_EVENT_FENCE)
		ret = gpuobj_free_on_fence(dev_priv, entry, param);
	else
		ret = -EINVAL;

	kgsl_mem_entry_put(entry);
	return ret;
}

long kgsl_ioctl_cmdstream_freememontimestamp_ctxtid(
		struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_cmdstream_freememontimestamp_ctxtid *param = data;
	struct kgsl_context *context = NULL;
	struct kgsl_mem_entry *entry;
	long ret = -EINVAL;

	if (param->type != KGSL_TIMESTAMP_RETIRED)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	entry = kgsl_sharedmem_find(dev_priv->process_priv,
		(uint64_t) param->gpuaddr);
	if (entry == NULL) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	ret = gpumem_free_entry_on_timestamp(dev_priv->device, entry,
		context, param->timestamp);

	kgsl_mem_entry_put(entry);
	kgsl_context_put(context);

	return ret;
}

static inline int _check_region(unsigned long start, unsigned long size,
				uint64_t len)
{
	uint64_t end = ((uint64_t) start) + size;
	return (end > len);
}

static int check_vma_flags(struct vm_area_struct *vma,
		unsigned int flags)
{
	unsigned long flags_requested = (VM_READ | VM_WRITE);

	if (flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags_requested &= ~VM_WRITE;

	if ((vma->vm_flags & flags_requested) == flags_requested)
		return 0;

	return -EFAULT;
}

static int check_vma(struct vm_area_struct *vma, struct file *vmfile,
		struct kgsl_memdesc *memdesc)
{
	if (vma == NULL || vma->vm_file != vmfile)
		return -EINVAL;

	/* userspace may not know the size, in which case use the whole vma */
	if (memdesc->size == 0)
		memdesc->size = vma->vm_end - vma->vm_start;
	/* range checking */
	if (vma->vm_start != memdesc->useraddr ||
		(memdesc->useraddr + memdesc->size) != vma->vm_end)
		return -EINVAL;
	return check_vma_flags(vma, memdesc->flags);
}

static int memdesc_sg_virt(struct kgsl_memdesc *memdesc, struct file *vmfile)
{
	int ret = 0;
	long npages = 0, i;
	size_t sglen = (size_t) (memdesc->size / PAGE_SIZE);
	struct page **pages = NULL;
	int write = ((memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 0 : 1);

	if (sglen == 0 || sglen >= LONG_MAX)
		return -EINVAL;

	pages = kgsl_malloc(sglen * sizeof(struct page *));
	if (pages == NULL)
		return -ENOMEM;

	memdesc->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (memdesc->sgt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	down_read(&current->mm->mmap_sem);
	/* If we have vmfile, make sure we map the correct vma and map it all */
	if (vmfile != NULL)
		ret = check_vma(find_vma(current->mm, memdesc->useraddr),
				vmfile, memdesc);

	if (ret == 0) {
		npages = get_user_pages(current, current->mm, memdesc->useraddr,
					sglen, write, 0, pages, NULL);
		ret = (npages < 0) ? (int)npages : 0;
	}
	up_read(&current->mm->mmap_sem);

	if (ret)
		goto out;

	if ((unsigned long) npages != sglen) {
		ret = -EINVAL;
		goto out;
	}

	ret = sg_alloc_table_from_pages(memdesc->sgt, pages, npages,
					0, memdesc->size, GFP_KERNEL);
out:
	if (ret) {
		for (i = 0; i < npages; i++)
			put_page(pages[i]);

		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
	}
	kgsl_free(pages);
	return ret;
}

static int kgsl_setup_anon_useraddr(struct kgsl_pagetable *pagetable,
	struct kgsl_mem_entry *entry, unsigned long hostptr,
	size_t offset, size_t size)
{
	/* Map an anonymous memory chunk */

	if (size == 0 || offset != 0 ||
		!IS_ALIGNED(size, PAGE_SIZE))
		return -EINVAL;

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = (uint64_t) size;
	entry->memdesc.useraddr = hostptr;
	entry->memdesc.flags |= KGSL_MEMFLAGS_USERMEM_ADDR;

	if (kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		int ret;

		/* Register the address in the database */
		ret = kgsl_mmu_set_svm_region(pagetable,
			(uint64_t) entry->memdesc.useraddr, (uint64_t) size);

		if (ret)
			return ret;

		entry->memdesc.gpuaddr = (uint64_t)  entry->memdesc.useraddr;
	}

	return memdesc_sg_virt(&entry->memdesc, NULL);
}

static int match_file(const void *p, struct file *file, unsigned int fd)
{
	/*
	 * We must return fd + 1 because iterate_fd stops searching on
	 * non-zero return, but 0 is a valid fd.
	 */
	return (p == file) ? (fd + 1) : 0;
}

static void _setup_cache_mode(struct kgsl_mem_entry *entry,
		struct vm_area_struct *vma)
{
	unsigned int mode;
	pgprot_t pgprot = vma->vm_page_prot;

	if (pgprot == pgprot_noncached(pgprot))
		mode = KGSL_CACHEMODE_UNCACHED;
	else if (pgprot == pgprot_writecombine(pgprot))
		mode = KGSL_CACHEMODE_WRITECOMBINE;
	else
		mode = KGSL_CACHEMODE_WRITEBACK;

	entry->memdesc.flags |= (mode << KGSL_CACHEMODE_SHIFT);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static int kgsl_setup_dma_buf(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable,
				struct kgsl_mem_entry *entry,
				struct dma_buf *dmabuf);

static int kgsl_setup_dmabuf_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry, unsigned long hostptr)
{
	struct vm_area_struct *vma;
	struct dma_buf *dmabuf = NULL;
	int ret;

	/*
	 * Find the VMA containing this pointer and figure out if it
	 * is a dma-buf.
	 */
	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, hostptr);

	if (vma && vma->vm_file) {
		int fd;

		ret = check_vma_flags(vma, entry->memdesc.flags);
		if (ret) {
			up_read(&current->mm->mmap_sem);
			return ret;
		}

		/*
		 * Check to see that this isn't our own memory that we have
		 * already mapped
		 */
		if (vma->vm_file->f_op == &kgsl_fops) {
			up_read(&current->mm->mmap_sem);
			return -EFAULT;
		}

		/* Look for the fd that matches this the vma file */
		fd = iterate_fd(current->files, 0, match_file, vma->vm_file);
		if (fd != 0)
			dmabuf = dma_buf_get(fd - 1);
	}
	up_read(&current->mm->mmap_sem);

	if (IS_ERR_OR_NULL(dmabuf))
		return dmabuf ? PTR_ERR(dmabuf) : -ENODEV;

	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret) {
		dma_buf_put(dmabuf);
		return ret;
	}

	/* Setup the user addr/cache mode for cache operations */
	entry->memdesc.useraddr = hostptr;
	_setup_cache_mode(entry, vma);

	return 0;
}
#else
static int kgsl_setup_dmabuf_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry, unsigned long hostptr)
{
	return -ENODEV;
}
#endif

static int kgsl_setup_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned long hostptr, size_t offset, size_t size)
{
	int ret;

	if (hostptr == 0 || !IS_ALIGNED(hostptr, PAGE_SIZE))
		return -EINVAL;

	/* Try to set up a dmabuf - if it returns -ENODEV assume anonymous */
	ret = kgsl_setup_dmabuf_useraddr(device, pagetable, entry, hostptr);
	if (ret != -ENODEV)
		return ret;

	/* Okay - lets go legacy */
	return kgsl_setup_anon_useraddr(pagetable, entry,
		hostptr, offset, size);
}

static long _gpuobj_map_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param)
{
	struct kgsl_gpuobj_import_useraddr useraddr;
	int ret;

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMFLAGS_FORCE_32BIT;

	/* Specifying SECURE is an explicit error */
	if (param->flags & KGSL_MEMFLAGS_SECURE)
		return -ENOTSUPP;

	ret = _copy_from_user(&useraddr,
		to_user_ptr(param->priv), sizeof(useraddr),
		param->priv_len);
	if (ret)
		return ret;

	/* Verify that the virtaddr and len are within bounds */
	if (useraddr.virtaddr > ULONG_MAX)
		return -EINVAL;

	return kgsl_setup_useraddr(device, pagetable, entry,
		(unsigned long) useraddr.virtaddr, 0, param->priv_len);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static long _gpuobj_map_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param,
		int *fd)
{
	struct kgsl_gpuobj_import_dma_buf buf;
	struct dma_buf *dmabuf;
	int ret;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE) {
		if (!kgsl_mmu_is_secured(&device->mmu)) {
			dev_WARN_ONCE(device->dev, 1,
				"Secure buffer not supported");
			return -ENOTSUPP;
		}

		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;
	}

	ret = _copy_from_user(&buf, to_user_ptr(param->priv),
			sizeof(buf), param->priv_len);
	if (ret)
		return ret;

	if (buf.fd < 0)
		return -EINVAL;

	*fd = buf.fd;
	dmabuf = dma_buf_get(buf.fd);

	if (IS_ERR_OR_NULL(dmabuf))
		return (dmabuf == NULL) ? -EINVAL : PTR_ERR(dmabuf);

	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret)
		dma_buf_put(dmabuf);

	return ret;
}
#else
static long _gpuobj_map_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param,
		int *fd)
{
	return -EINVAL;
}
#endif

long kgsl_ioctl_gpuobj_import(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_import *param = data;
	struct kgsl_mem_entry *entry;
	int ret, fd = -1;
	struct kgsl_mmu *mmu = &dev_priv->device->mmu;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY
			| KGSL_MEMTYPE_MASK
			| KGSL_MEMALIGN_MASK
			| KGSL_MEMFLAGS_USE_CPU_MAP
			| KGSL_MEMFLAGS_SECURE
			| KGSL_MEMFLAGS_FORCE_32BIT;

	entry->memdesc.flags = param->flags;

	if (MMU_FEATURE(mmu, KGSL_MMU_NEED_GUARD_PAGE))
		entry->memdesc.priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (param->type == KGSL_USER_MEM_TYPE_ADDR)
		ret = _gpuobj_map_useraddr(dev_priv->device, private->pagetable,
			entry, param);
	else if (param->type == KGSL_USER_MEM_TYPE_DMABUF)
		ret = _gpuobj_map_dma_buf(dev_priv->device, private->pagetable,
			entry, param, &fd);
	else
		ret = -ENOTSUPP;

	if (ret)
		goto out;

	if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64K));

	param->flags = entry->memdesc.flags;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret)
		goto unmap;

	param->id = entry->id;

	KGSL_STATS_ADD(entry->memdesc.size, &kgsl_driver.stats.mapped,
		&kgsl_driver.stats.mapped_max);

	kgsl_process_add_stats(private,
		kgsl_memdesc_usermem_type(&entry->memdesc),
		entry->memdesc.size);

	trace_kgsl_mem_map(entry, fd);

	kgsl_mem_entry_commit_process(entry);
	return 0;

unmap:
	if (param->type == KGSL_USER_MEM_TYPE_DMABUF) {
		kgsl_destroy_ion(entry->priv_data);
		entry->memdesc.sgt = NULL;
	}

	kgsl_sharedmem_free(&entry->memdesc);

out:
	kfree(entry);
	return ret;
}

static long _map_usermem_addr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable, struct kgsl_mem_entry *entry,
		unsigned long hostptr, size_t offset, size_t size)
{
	if (!MMU_FEATURE(&device->mmu, KGSL_MMU_PAGED))
		return -EINVAL;

	/* No CPU mapped buffer could ever be secure */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE)
		return -EINVAL;

	return kgsl_setup_useraddr(device, pagetable, entry, hostptr,
		offset, size);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static int _map_usermem_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned int fd)
{
	int ret;
	struct dma_buf *dmabuf;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */

	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE) {
		if (!kgsl_mmu_is_secured(&device->mmu)) {
			dev_WARN_ONCE(device->dev, 1,
				"Secure buffer not supported");
			return -EINVAL;
		}

		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;
	}

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		return ret ? ret : -EINVAL;
	}
	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret)
		dma_buf_put(dmabuf);
	return ret;
}
#else
static int _map_usermem_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned int fd)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
static int kgsl_setup_dma_buf(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable,
				struct kgsl_mem_entry *entry,
				struct dma_buf *dmabuf)
{
	int ret = 0;
	struct scatterlist *s;
	struct sg_table *sg_table;
	struct dma_buf_attachment *attach = NULL;
	struct kgsl_dma_buf_meta *meta;

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta)
		return -ENOMEM;

	attach = dma_buf_attach(dmabuf, device->dev);
	if (IS_ERR_OR_NULL(attach)) {
		ret = attach ? PTR_ERR(attach) : -EINVAL;
		goto out;
	}

	meta->dmabuf = dmabuf;
	meta->attach = attach;

	attach->priv = entry;

	entry->priv_data = meta;
	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = 0;
	/* USE_CPU_MAP is not impemented for ION. */
	entry->memdesc.flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);
	entry->memdesc.flags |= KGSL_MEMFLAGS_USERMEM_ION;

	sg_table = dma_buf_map_attachment(attach, DMA_TO_DEVICE);

	if (IS_ERR_OR_NULL(sg_table)) {
		ret = PTR_ERR(sg_table);
		goto out;
	}

	meta->table = sg_table;
	entry->priv_data = meta;
	entry->memdesc.sgt = sg_table;

	/* Calculate the size of the memdesc from the sglist */
	for (s = entry->memdesc.sgt->sgl; s != NULL; s = sg_next(s)) {
		int priv = (entry->memdesc.priv & KGSL_MEMDESC_SECURE) ? 1 : 0;

		/*
		 * Check that each chunk of of the sg table matches the secure
		 * flag.
		 */

		if (PagePrivate(sg_page(s)) != priv) {
			ret = -EPERM;
			goto out;
		}

		entry->memdesc.size += (uint64_t) s->length;
	}

	entry->memdesc.size = PAGE_ALIGN(entry->memdesc.size);

out:
	if (ret) {
		if (!IS_ERR_OR_NULL(attach))
			dma_buf_detach(dmabuf, attach);


		kfree(meta);
	}

	return ret;
}
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
	struct kgsl_dma_buf_meta *meta = entry->priv_data;
	struct dma_buf *dmabuf = meta->dmabuf;
	struct dma_buf_attachment *mem_entry_buf_attachment = meta->attach;
	struct device *buf_attachment_dev = mem_entry_buf_attachment->dev;
	struct dma_buf_attachment *attachment = NULL;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry(attachment, &dmabuf->attachments, node) {
		struct kgsl_mem_entry *scan_mem_entry = NULL;

		if (attachment->dev != buf_attachment_dev)
			continue;

		scan_mem_entry = attachment->priv;
		if (!scan_mem_entry)
			continue;

		switch (kgsl_memdesc_get_memtype(&scan_mem_entry->memdesc)) {
		case KGSL_MEMTYPE_EGL_SURFACE:
			(*egl_surface_count)++;
			break;
		case KGSL_MEMTYPE_EGL_IMAGE:
			(*egl_image_count)++;
			break;
		}
	}
	mutex_unlock(&dmabuf->lock);
}
#else
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
}
#endif

long kgsl_ioctl_map_user_mem(struct kgsl_device_private *dev_priv,
				     unsigned int cmd, void *data)
{
	int result = -EINVAL;
	struct kgsl_map_user_mem *param = data;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mmu *mmu = &dev_priv->device->mmu;
	unsigned int memtype;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */

	if (param->flags & KGSL_MEMFLAGS_SECURE) {
		/* Log message and return if context protection isn't enabled */
		if (!kgsl_mmu_is_secured(mmu)) {
			dev_WARN_ONCE(dev_priv->device->dev, 1,
				"Secure buffer not supported");
			return -EOPNOTSUPP;
		}

		/* Can't use CPU map with secure buffers */
		if (param->flags & KGSL_MEMFLAGS_USE_CPU_MAP)
			return -EINVAL;
	}

	entry = kgsl_mem_entry_create();

	if (entry == NULL)
		return -ENOMEM;

	/*
	 * Convert from enum value to KGSL_MEM_ENTRY value, so that
	 * we can use the latter consistently everywhere.
	 */
	memtype = param->memtype + 1;

	/*
	 * Mask off unknown flags from userspace. This way the caller can
	 * check if a flag is supported by looking at the returned flags.
	 * Note: CACHEMODE is ignored for this call. Caching should be
	 * determined by type of allocation being mapped.
	 */
	param->flags &= KGSL_MEMFLAGS_GPUREADONLY
			| KGSL_MEMTYPE_MASK
			| KGSL_MEMALIGN_MASK
			| KGSL_MEMFLAGS_USE_CPU_MAP
			| KGSL_MEMFLAGS_SECURE;
	entry->memdesc.flags = ((uint64_t) param->flags)
		| KGSL_MEMFLAGS_FORCE_32BIT;

	if (!kgsl_mmu_use_cpu_map(mmu))
		entry->memdesc.flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	if (MMU_FEATURE(mmu, KGSL_MMU_NEED_GUARD_PAGE))
		entry->memdesc.priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (param->flags & KGSL_MEMFLAGS_SECURE)
		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;

	switch (memtype) {
	case KGSL_MEM_ENTRY_USER:
		result = _map_usermem_addr(dev_priv->device, private->pagetable,
			entry, param->hostptr, param->offset, param->len);
		break;
	case KGSL_MEM_ENTRY_ION:
		if (param->offset != 0)
			result = -EINVAL;
		else
			result = _map_usermem_dma_buf(dev_priv->device,
				private->pagetable, entry, param->fd);
		break;
	default:
		result = -EOPNOTSUPP;
		break;
	}

	if (result)
		goto error;

	if ((param->flags & KGSL_MEMFLAGS_SECURE) &&
		(entry->memdesc.size & mmu->secure_align_mask)) {
		result = -EINVAL;
		goto error_attach;
	}

	if (entry->memdesc.size >= SZ_2M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_2M));
	else if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64));

	/* echo back flags */
	param->flags = (unsigned int) entry->memdesc.flags;

	result = kgsl_mem_entry_attach_process(dev_priv->device, private,
		entry);
	if (result)
		goto error_attach;

	/* Adjust the returned value for a non 4k aligned offset */
	param->gpuaddr = (unsigned long)
		entry->memdesc.gpuaddr + (param->offset & PAGE_MASK);

	KGSL_STATS_ADD(param->len, &kgsl_driver.stats.mapped,
		&kgsl_driver.stats.mapped_max);

	kgsl_process_add_stats(private,
			kgsl_memdesc_usermem_type(&entry->memdesc), param->len);

	trace_kgsl_mem_map(entry, param->fd);

	kgsl_mem_entry_commit_process(entry);
	return result;

error_attach:
	switch (memtype) {
	case KGSL_MEM_ENTRY_ION:
		kgsl_destroy_ion(entry->priv_data);
		entry->memdesc.sgt = NULL;
		break;
	default:
		break;
	}
	kgsl_sharedmem_free(&entry->memdesc);
error:
	/* Clear gpuaddr here so userspace doesn't get any wrong ideas */
	param->gpuaddr = 0;

	kfree(entry);
	return result;
}

static int _kgsl_gpumem_sync_cache(struct kgsl_mem_entry *entry,
		uint64_t offset, uint64_t length, unsigned int op)
{
	int ret = 0;
	int cacheop;
	int mode;

	/*
	 * Flush is defined as (clean | invalidate).  If both bits are set, then
	 * do a flush, otherwise check for the individual bits and clean or inv
	 * as requested
	 */

	if ((op & KGSL_GPUMEM_CACHE_FLUSH) == KGSL_GPUMEM_CACHE_FLUSH)
		cacheop = KGSL_CACHE_OP_FLUSH;
	else if (op & KGSL_GPUMEM_CACHE_CLEAN)
		cacheop = KGSL_CACHE_OP_CLEAN;
	else if (op & KGSL_GPUMEM_CACHE_INV)
		cacheop = KGSL_CACHE_OP_INV;
	else {
		ret = -EINVAL;
		goto done;
	}

	if (!(op & KGSL_GPUMEM_CACHE_RANGE)) {
		offset = 0;
		length = entry->memdesc.size;
	}

	mode = kgsl_memdesc_get_cachemode(&entry->memdesc);
	if (mode != KGSL_CACHEMODE_UNCACHED
		&& mode != KGSL_CACHEMODE_WRITECOMBINE) {
		trace_kgsl_mem_sync_cache(entry, offset, length, op);
		ret = kgsl_cache_range_op(&entry->memdesc, offset,
					length, cacheop);
	}

done:
	return ret;
}

/* New cache sync function - supports both directions (clean and invalidate) */

long kgsl_ioctl_gpumem_sync_cache(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_gpumem_sync_cache *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	long ret;

	if (param->id != 0)
		entry = kgsl_sharedmem_find_id(private, param->id);
	else if (param->gpuaddr != 0)
		entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);

	if (entry == NULL)
		return -EINVAL;

	ret = _kgsl_gpumem_sync_cache(entry, (uint64_t) param->offset,
					(uint64_t) param->length, param->op);
	kgsl_mem_entry_put(entry);
	return ret;
}

static int mem_id_cmp(const void *_a, const void *_b)
{
	const unsigned int *a = _a, *b = _b;
	if (*a == *b)
		return 0;
	return (*a > *b) ? 1 : -1;
}

#ifdef CONFIG_ARM64
/* Do not support full flush on ARM64 targets */
static inline bool check_full_flush(size_t size, int op)
{
	return false;
}
#else
/* Support full flush if the size is bigger than the threshold */
static inline bool check_full_flush(size_t size, int op)
{
	/* If we exceed the breakeven point, flush the entire cache */
	return (kgsl_driver.full_cache_threshold != 0) &&
		(size >= kgsl_driver.full_cache_threshold) &&
		(op == KGSL_GPUMEM_CACHE_FLUSH);
}
#endif

long kgsl_ioctl_gpumem_sync_cache_bulk(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	int i;
	struct kgsl_gpumem_sync_cache_bulk *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	unsigned int id, last_id = 0, *id_list = NULL, actual_count = 0;
	struct kgsl_mem_entry **entries = NULL;
	long ret = 0;
	uint64_t op_size = 0;
	bool full_flush = false;

	if (param->id_list == NULL || param->count == 0
			|| param->count > (PAGE_SIZE / sizeof(unsigned int)))
		return -EINVAL;

	id_list = kzalloc(param->count * sizeof(unsigned int), GFP_KERNEL);
	if (id_list == NULL)
		return -ENOMEM;

	entries = kzalloc(param->count * sizeof(*entries), GFP_KERNEL);
	if (entries == NULL) {
		ret = -ENOMEM;
		goto end;
	}

	if (copy_from_user(id_list, param->id_list,
				param->count * sizeof(unsigned int))) {
		ret = -EFAULT;
		goto end;
	}
	/* sort the ids so we can weed out duplicates */
	sort(id_list, param->count, sizeof(*id_list), mem_id_cmp, NULL);

	for (i = 0; i < param->count; i++) {
		unsigned int cachemode;
		struct kgsl_mem_entry *entry = NULL;

		id = id_list[i];
		/* skip 0 ids or duplicates */
		if (id == last_id)
			continue;

		entry = kgsl_sharedmem_find_id(private, id);
		if (entry == NULL)
			continue;

		/* skip uncached memory */
		cachemode = kgsl_memdesc_get_cachemode(&entry->memdesc);
		if (cachemode != KGSL_CACHEMODE_WRITETHROUGH &&
		    cachemode != KGSL_CACHEMODE_WRITEBACK) {
			kgsl_mem_entry_put(entry);
			continue;
		}

		op_size += entry->memdesc.size;
		entries[actual_count++] = entry;

		full_flush  = check_full_flush(op_size, param->op);
		if (full_flush)
			break;

		last_id = id;
	}
	if (full_flush) {
		trace_kgsl_mem_sync_full_cache(actual_count, op_size);
		flush_cache_all();
	}

	param->op &= ~KGSL_GPUMEM_CACHE_RANGE;

	for (i = 0; i < actual_count; i++) {
		if (!full_flush)
			_kgsl_gpumem_sync_cache(entries[i], 0,
						entries[i]->memdesc.size,
						param->op);
		kgsl_mem_entry_put(entries[i]);
	}
end:
	kfree(entries);
	kfree(id_list);
	return ret;
}

/* Legacy cache function, does a flush (clean  + invalidate) */

long kgsl_ioctl_sharedmem_flush_cache(struct kgsl_device_private *dev_priv,
				 unsigned int cmd, void *data)
{
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	long ret;

	entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);
	if (entry == NULL)
		return -EINVAL;

	ret = _kgsl_gpumem_sync_cache(entry, 0, entry->memdesc.size,
					KGSL_GPUMEM_CACHE_FLUSH);
	kgsl_mem_entry_put(entry);
	return ret;
}

long kgsl_ioctl_gpuobj_sync(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_sync *param = data;
	struct kgsl_gpuobj_sync_obj *objs;
	struct kgsl_mem_entry **entries;
	long ret = 0;
	bool full_flush = false;
	uint64_t size = 0;
	int i, count = 0;
	void __user *ptr;

	if (param->count == 0 || param->count > 128)
		return -EINVAL;

	objs = kzalloc(param->count * sizeof(*objs), GFP_KERNEL);
	if (objs == NULL)
		return -ENOMEM;

	entries = kzalloc(param->count * sizeof(*entries), GFP_KERNEL);
	if (entries == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ptr = to_user_ptr(param->objs);

	for (i = 0; i < param->count; i++) {
		ret = _copy_from_user(&objs[i], ptr, sizeof(*objs),
			param->obj_len);
		if (ret)
			goto out;

		entries[i] = kgsl_sharedmem_find_id(private, objs[i].id);

		/* Not finding the ID is not a fatal failure - just skip it */
		if (entries[i] == NULL)
			continue;

		count++;

		if (!(objs[i].op & KGSL_GPUMEM_CACHE_RANGE))
			size += entries[i]->memdesc.size;
		else if (objs[i].offset < entries[i]->memdesc.size)
			size += (entries[i]->memdesc.size - objs[i].offset);

		full_flush = check_full_flush(size, objs[i].op);
		if (full_flush)
			break;

		ptr += sizeof(*objs);
	}

	if (full_flush) {
		trace_kgsl_mem_sync_full_cache(count, size);
		flush_cache_all();
	} else {
		for (i = 0; !ret && i < param->count; i++)
			if (entries[i])
				ret = _kgsl_gpumem_sync_cache(entries[i],
						objs[i].offset, objs[i].length,
						objs[i].op);
	}

	for (i = 0; i < param->count; i++)
		if (entries[i])
			kgsl_mem_entry_put(entries[i]);

out:
	kfree(entries);
	kfree(objs);

	return ret;
}

#ifdef CONFIG_ARM64
static uint64_t kgsl_filter_cachemode(uint64_t flags)
{
	/*
	 * WRITETHROUGH is not supported in arm64, so we tell the user that we
	 * use WRITEBACK which is the default caching policy.
	 */
	if ((flags & KGSL_CACHEMODE_MASK) >> KGSL_CACHEMODE_SHIFT ==
					KGSL_CACHEMODE_WRITETHROUGH) {
		flags &= ~((uint64_t) KGSL_CACHEMODE_MASK);
		flags |= (KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) &
							KGSL_CACHEMODE_MASK;
	}
	return flags;
}
#else
static uint64_t kgsl_filter_cachemode(uint64_t flags)
{
	return flags;
}
#endif

/* The largest allowable alignment for a GPU object is 32MB */
#define KGSL_MAX_ALIGN (32 * SZ_1M)

static struct kgsl_mem_entry *gpumem_alloc_entry(
		struct kgsl_device_private *dev_priv,
		uint64_t size, uint64_t flags)
{
	int ret;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	unsigned int align;

	flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMALIGN_MASK
		| KGSL_MEMFLAGS_USE_CPU_MAP
		| KGSL_MEMFLAGS_SECURE
		| KGSL_MEMFLAGS_FORCE_32BIT;

	/* Turn off SVM if the system doesn't support it */
	if (!kgsl_mmu_use_cpu_map(&dev_priv->device->mmu))
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Return not supported error if secure memory isn't enabled */
	if (!kgsl_mmu_is_secured(&dev_priv->device->mmu) &&
			(flags & KGSL_MEMFLAGS_SECURE)) {
		dev_WARN_ONCE(dev_priv->device->dev, 1,
				"Secure memory not supported");
		return ERR_PTR(-EOPNOTSUPP);
	}

	/* Secure memory disables advanced addressing modes */
	if (flags & KGSL_MEMFLAGS_SECURE)
		flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	/* Cap the alignment bits to the highest number we can handle */
	align = MEMFLAGS(flags, KGSL_MEMALIGN_MASK, KGSL_MEMALIGN_SHIFT);
	if (align >= ilog2(KGSL_MAX_ALIGN)) {
		KGSL_CORE_ERR("Alignment too large; restricting to %dK\n",
			KGSL_MAX_ALIGN >> 10);

		flags &= ~((uint64_t) KGSL_MEMALIGN_MASK);
		flags |= (ilog2(KGSL_MAX_ALIGN) << KGSL_MEMALIGN_SHIFT) &
			KGSL_MEMALIGN_MASK;
	}

	/* For now only allow allocations up to 4G */
	if (size == 0 || size > UINT_MAX)
		return ERR_PTR(-EINVAL);

	flags = kgsl_filter_cachemode(flags);

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return ERR_PTR(-ENOMEM);

	if (MMU_FEATURE(&dev_priv->device->mmu, KGSL_MMU_NEED_GUARD_PAGE))
		entry->memdesc.priv |= KGSL_MEMDESC_GUARD_PAGE;

	if (flags & KGSL_MEMFLAGS_SECURE)
		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;

	ret = kgsl_allocate_user(dev_priv->device, &entry->memdesc,
		size, flags);
	if (ret != 0)
		goto err;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret != 0) {
		kgsl_sharedmem_free(&entry->memdesc);
		goto err;
	}

	kgsl_process_add_stats(private,
			kgsl_memdesc_usermem_type(&entry->memdesc),
			entry->memdesc.size);
	trace_kgsl_mem_alloc(entry);

	kgsl_mem_entry_commit_process(entry);
	return entry;
err:
	kfree(entry);
	return ERR_PTR(ret);
}

static void copy_metadata(struct kgsl_mem_entry *entry, uint64_t metadata,
		unsigned int len)
{
	unsigned int i, size;

	if (len == 0)
		return;

	size = min_t(unsigned int, len, sizeof(entry->metadata) - 1);

	if (copy_from_user(entry->metadata, to_user_ptr(metadata), size)) {
		memset(entry->metadata, 0, sizeof(entry->metadata));
		return;
	}

	/* Clean up non printable characters in the string */
	for (i = 0; i < size && entry->metadata[i] != 0; i++) {
		if (!isprint(entry->metadata[i]))
			entry->metadata[i] = '?';
	}
}

long kgsl_ioctl_gpuobj_alloc(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpuobj_alloc *param = data;
	struct kgsl_mem_entry *entry;

	entry = gpumem_alloc_entry(dev_priv, param->size, param->flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	copy_metadata(entry, param->metadata, param->metadata_len);

	param->size = entry->memdesc.size;
	param->flags = entry->memdesc.flags;
	param->mmapsize = kgsl_memdesc_footprint(&entry->memdesc);
	param->id = entry->id;

	return 0;
}

long kgsl_ioctl_gpumem_alloc(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpumem_alloc *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags = param->flags;

	/* Legacy functions doesn't support these advanced features */
	flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);
	flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;
	param->size = (size_t) entry->memdesc.size;
	param->flags = (unsigned int) entry->memdesc.flags;

	return 0;
}

long kgsl_ioctl_gpumem_alloc_id(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_gpumem_alloc_id *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags = param->flags;

	flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->id = entry->id;
	param->flags = (unsigned int) entry->memdesc.flags;
	param->size = (size_t) entry->memdesc.size;
	param->mmapsize = (size_t) kgsl_memdesc_footprint(&entry->memdesc);
	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;

	return 0;
}

long kgsl_ioctl_gpumem_get_info(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpumem_get_info *param = data;
	struct kgsl_mem_entry *entry = NULL;
	int result = 0;

	if (param->id != 0)
		entry = kgsl_sharedmem_find_id(private, param->id);
	else if (param->gpuaddr != 0)
		entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);

	if (entry == NULL)
		return -EINVAL;

	/*
	 * If any of the 64 bit address / sizes would end up being
	 * truncated, return -ERANGE.  That will signal the user that they
	 * should use a more modern API
	 */
	if (entry->memdesc.gpuaddr > ULONG_MAX)
		result = -ERANGE;

	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;
	param->id = entry->id;
	param->flags = (unsigned int) entry->memdesc.flags;
	param->size = (size_t) entry->memdesc.size;
	param->mmapsize = (size_t) kgsl_memdesc_footprint(&entry->memdesc);
	param->useraddr = entry->memdesc.useraddr;

	kgsl_mem_entry_put(entry);
	return result;
}

static inline int _sparse_alloc_param_sanity_check(uint64_t size,
		uint64_t pagesize)
{
	if (size == 0 || pagesize == 0)
		return -EINVAL;

	if (pagesize != PAGE_SIZE && pagesize != SZ_64K)
		return -EINVAL;

	if (pagesize > size || !IS_ALIGNED(size, pagesize))
		return -EINVAL;

	return 0;
}

long kgsl_ioctl_sparse_phys_alloc(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_sparse_phys_alloc *param = data;
	struct kgsl_mem_entry *entry;
	int ret;
	int id;

	ret = _sparse_alloc_param_sanity_check(param->size, param->pagesize);
	if (ret)
		return ret;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	ret = kgsl_process_private_get(process);
	if (!ret) {
		ret = -EBADF;
		goto err_free_entry;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&process->mem_lock);
	/* Allocate the ID but don't attach the pointer just yet */
	id = idr_alloc(&process->mem_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&process->mem_lock);
	idr_preload_end();

	if (id < 0) {
		ret = id;
		goto err_put_proc_priv;
	}

	entry->id = id;
	entry->priv = process;

	entry->memdesc.flags = KGSL_MEMFLAGS_SPARSE_PHYS;
	kgsl_memdesc_set_align(&entry->memdesc, ilog2(param->pagesize));

	ret = kgsl_allocate_user(dev_priv->device, &entry->memdesc,
			param->size, entry->memdesc.flags);
	if (ret)
		goto err_remove_idr;

	/* Sanity check to verify we got correct pagesize */
	if (param->pagesize != PAGE_SIZE && entry->memdesc.sgt != NULL) {
		struct scatterlist *s;
		int i;

		for_each_sg(entry->memdesc.sgt->sgl, s,
				entry->memdesc.sgt->nents, i) {
			if (!IS_ALIGNED(s->length, param->pagesize))
				goto err_invalid_pages;
		}
	}

	param->id = entry->id;
	param->flags = entry->memdesc.flags;

	trace_sparse_phys_alloc(entry->id, param->size, param->pagesize);
	kgsl_mem_entry_commit_process(entry);

	return 0;

err_invalid_pages:
	kgsl_sharedmem_free(&entry->memdesc);
err_remove_idr:
	spin_lock(&process->mem_lock);
	idr_remove(&process->mem_idr, entry->id);
	spin_unlock(&process->mem_lock);
err_put_proc_priv:
	kgsl_process_private_put(process);
err_free_entry:
	kfree(entry);

	return ret;
}

long kgsl_ioctl_sparse_phys_free(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_sparse_phys_free *param = data;
	struct kgsl_mem_entry *entry;

	entry = kgsl_sharedmem_find_id_flags(process, param->id,
			KGSL_MEMFLAGS_SPARSE_PHYS);
	if (entry == NULL)
		return -EINVAL;

	if (entry->memdesc.cur_bindings != 0) {
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	trace_sparse_phys_free(entry->id);

	/* One put for find_id(), one put for the kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_sparse_virt_alloc(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_sparse_virt_alloc *param = data;
	struct kgsl_mem_entry *entry;
	int ret;

	ret = _sparse_alloc_param_sanity_check(param->size, param->pagesize);
	if (ret)
		return ret;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	entry->memdesc.flags = KGSL_MEMFLAGS_SPARSE_VIRT;
	entry->memdesc.size = param->size;
	entry->memdesc.cur_bindings = 0;
	kgsl_memdesc_set_align(&entry->memdesc, ilog2(param->pagesize));

	spin_lock_init(&entry->bind_lock);
	entry->bind_tree = RB_ROOT;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret) {
		kfree(entry);
		return ret;
	}

	param->id = entry->id;
	param->gpuaddr = entry->memdesc.gpuaddr;
	param->flags = entry->memdesc.flags;

	trace_sparse_virt_alloc(entry->id, param->size, param->pagesize);
	kgsl_mem_entry_commit_process(entry);

	return 0;
}

long kgsl_ioctl_sparse_virt_free(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_sparse_virt_free *param = data;
	struct kgsl_mem_entry *entry = NULL;

	entry = kgsl_sharedmem_find_id_flags(process, param->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (entry == NULL)
		return -EINVAL;

	if (entry->bind_tree.rb_node != NULL) {
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	trace_sparse_virt_free(entry->id);

	/* One put for find_id(), one put for the kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);
	kgsl_mem_entry_put(entry);

	return 0;
}

static int _sparse_add_to_bind_tree(struct kgsl_mem_entry *entry,
		uint64_t v_offset,
		struct kgsl_memdesc *memdesc,
		uint64_t p_offset,
		uint64_t size,
		uint64_t flags)
{
	struct sparse_bind_object *new;
	struct rb_node **node, *parent = NULL;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (new == NULL)
		return -ENOMEM;

	new->v_off = v_offset;
	new->p_off = p_offset;
	new->p_memdesc = memdesc;
	new->size = size;
	new->flags = flags;

	node = &entry->bind_tree.rb_node;

	while (*node != NULL) {
		struct sparse_bind_object *this;

		parent = *node;
		this = rb_entry(parent, struct sparse_bind_object, node);

		if (new->v_off < this->v_off)
			node = &parent->rb_left;
		else if (new->v_off > this->v_off)
			node = &parent->rb_right;
	}

	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &entry->bind_tree);

	return 0;
}

static int _sparse_rm_from_bind_tree(struct kgsl_mem_entry *entry,
		struct sparse_bind_object *obj,
		uint64_t v_offset, uint64_t size)
{
	spin_lock(&entry->bind_lock);
	if (v_offset == obj->v_off && size >= obj->size) {
		/*
		 * We are all encompassing, remove the entry and free
		 * things up
		 */
		rb_erase(&obj->node, &entry->bind_tree);
		kfree(obj);
	} else if (v_offset == obj->v_off) {
		/*
		 * We are the front of the node, adjust the front of
		 * the node
		 */
		obj->v_off += size;
		obj->p_off += size;
		obj->size -= size;
	} else if ((v_offset + size) == (obj->v_off + obj->size)) {
		/*
		 * We are at the end of the obj, adjust the beginning
		 * points
		 */
		obj->size -= size;
	} else {
		/*
		 * We are in the middle of a node, split it up and
		 * create a new mini node. Adjust this node's bounds
		 * and add the new node to the list.
		 */
		uint64_t tmp_size = obj->size;
		int ret;

		obj->size = v_offset - obj->v_off;

		spin_unlock(&entry->bind_lock);
		ret = _sparse_add_to_bind_tree(entry, v_offset + size,
				obj->p_memdesc,
				obj->p_off + (v_offset - obj->v_off) + size,
				tmp_size - (v_offset - obj->v_off) - size,
				obj->flags);

		return ret;
	}

	spin_unlock(&entry->bind_lock);

	return 0;
}

static struct sparse_bind_object *_find_containing_bind_obj(
		struct kgsl_mem_entry *entry,
		uint64_t offset, uint64_t size)
{
	struct sparse_bind_object *obj = NULL;
	struct rb_node *node = entry->bind_tree.rb_node;

	spin_lock(&entry->bind_lock);

	while (node != NULL) {
		obj = rb_entry(node, struct sparse_bind_object, node);

		if (offset == obj->v_off) {
			break;
		} else if (offset < obj->v_off) {
			if (offset + size > obj->v_off)
				break;
			node = node->rb_left;
			obj = NULL;
		} else if (offset > obj->v_off) {
			if (offset < obj->v_off + obj->size)
				break;
			node = node->rb_right;
			obj = NULL;
		}
	}

	spin_unlock(&entry->bind_lock);

	return obj;
}

static int _sparse_unbind(struct kgsl_mem_entry *entry,
		struct sparse_bind_object *bind_obj,
		uint64_t offset, uint64_t size)
{
	struct kgsl_memdesc *memdesc = bind_obj->p_memdesc;
	struct kgsl_pagetable *pt = memdesc->pagetable;
	int ret;

	if (memdesc->cur_bindings < (size / PAGE_SIZE))
		return -EINVAL;

	memdesc->cur_bindings -= size / PAGE_SIZE;

	ret = kgsl_mmu_unmap_offset(pt, memdesc,
			entry->memdesc.gpuaddr, offset, size);
	if (ret)
		return ret;

	ret = kgsl_mmu_sparse_dummy_map(pt, &entry->memdesc, offset, size);
	if (ret)
		return ret;

	ret = _sparse_rm_from_bind_tree(entry, bind_obj, offset, size);
	if (ret == 0) {
		atomic_long_sub(size, &kgsl_driver.stats.mapped);
		trace_sparse_unbind(entry->id, offset, size);
	}

	return ret;
}

static long sparse_unbind_range(struct kgsl_sparse_binding_object *obj,
	struct kgsl_mem_entry *virt_entry)
{
	struct sparse_bind_object *bind_obj;
	int ret = 0;
	uint64_t size = obj->size;
	uint64_t tmp_size = obj->size;
	uint64_t offset = obj->virtoffset;

	while (size > 0 && ret == 0) {
		tmp_size = size;
		bind_obj = _find_containing_bind_obj(virt_entry, offset, size);
		if (bind_obj == NULL)
			return 0;

		if (bind_obj->v_off > offset) {
			tmp_size = size - bind_obj->v_off - offset;
			if (tmp_size > bind_obj->size)
				tmp_size = bind_obj->size;
			offset = bind_obj->v_off;
		} else if (bind_obj->v_off < offset) {
			uint64_t diff = offset - bind_obj->v_off;

			if (diff + size > bind_obj->size)
				tmp_size = bind_obj->size - diff;
		} else {
			if (tmp_size > bind_obj->size)
				tmp_size = bind_obj->size;
		}

		ret = _sparse_unbind(virt_entry, bind_obj, offset, tmp_size);
		if (ret == 0) {
			offset += tmp_size;
			size -= tmp_size;
		}
	}

	return ret;
}

static inline bool _is_phys_bindable(struct kgsl_mem_entry *phys_entry,
		uint64_t offset, uint64_t size, uint64_t flags)
{
	struct kgsl_memdesc *memdesc = &phys_entry->memdesc;

	if (!IS_ALIGNED(offset | size, kgsl_memdesc_get_pagesize(memdesc)))
		return false;

	if (offset + size < offset)
		return false;

	if (!(flags & KGSL_SPARSE_BIND_MULTIPLE_TO_PHYS) &&
			offset + size > memdesc->size)
		return false;

	return true;
}

static int _sparse_bind(struct kgsl_process_private *process,
		struct kgsl_mem_entry *virt_entry, uint64_t v_offset,
		struct kgsl_mem_entry *phys_entry, uint64_t p_offset,
		uint64_t size, uint64_t flags)
{
	int ret;
	struct kgsl_pagetable *pagetable;
	struct kgsl_memdesc *memdesc = &phys_entry->memdesc;

	/* map the memory after unlocking if gpuaddr has been assigned */
	if (memdesc->gpuaddr)
		return -EINVAL;

	if (memdesc->useraddr != 0)
		return -EINVAL;

	pagetable = memdesc->pagetable;

	/* Clear out any mappings */
	ret = kgsl_mmu_unmap_offset(pagetable, &virt_entry->memdesc,
			virt_entry->memdesc.gpuaddr, v_offset, size);
	if (ret)
		return ret;

	ret = kgsl_mmu_map_offset(pagetable, virt_entry->memdesc.gpuaddr,
			v_offset, memdesc, p_offset, size, flags);
	if (ret) {
		/* Try to clean up, but not the end of the world */
		kgsl_mmu_sparse_dummy_map(pagetable, &virt_entry->memdesc,
				v_offset, size);
		return ret;
	}

	ret = _sparse_add_to_bind_tree(virt_entry, v_offset, memdesc,
			p_offset, size, flags);
	if (ret == 0)
		memdesc->cur_bindings += size / PAGE_SIZE;

	return ret;
}

static long sparse_bind_range(struct kgsl_process_private *private,
		struct kgsl_sparse_binding_object *obj,
		struct kgsl_mem_entry *virt_entry)
{
	struct kgsl_mem_entry *phys_entry;
	int ret;

	phys_entry = kgsl_sharedmem_find_id_flags(private, obj->id,
			KGSL_MEMFLAGS_SPARSE_PHYS);
	if (phys_entry == NULL)
		return -EINVAL;

	if (!_is_phys_bindable(phys_entry, obj->physoffset, obj->size,
				obj->flags)) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	if (kgsl_memdesc_get_align(&virt_entry->memdesc) !=
			kgsl_memdesc_get_align(&phys_entry->memdesc)) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	ret = sparse_unbind_range(obj, virt_entry);
	if (ret) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	ret = _sparse_bind(private, virt_entry, obj->virtoffset,
			phys_entry, obj->physoffset, obj->size,
			obj->flags & KGSL_SPARSE_BIND_MULTIPLE_TO_PHYS);
	if (ret == 0) {
		KGSL_STATS_ADD(obj->size, &kgsl_driver.stats.mapped,
				&kgsl_driver.stats.mapped_max);

		trace_sparse_bind(virt_entry->id, obj->virtoffset,
				phys_entry->id, obj->physoffset,
				obj->size, obj->flags);
	}

	kgsl_mem_entry_put(phys_entry);

	return ret;
}

long kgsl_ioctl_sparse_bind(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_sparse_bind *param = data;
	struct kgsl_sparse_binding_object obj;
	struct kgsl_mem_entry *virt_entry;
	int pg_sz;
	void __user *ptr;
	int ret = 0;
	int i = 0;

	ptr = (void __user *) (uintptr_t) param->list;

	if (param->size > sizeof(struct kgsl_sparse_binding_object) ||
		param->count == 0 || ptr == NULL)
		return -EINVAL;

	virt_entry = kgsl_sharedmem_find_id_flags(private, param->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (virt_entry == NULL)
		return -EINVAL;

	pg_sz = kgsl_memdesc_get_pagesize(&virt_entry->memdesc);

	for (i = 0; i < param->count; i++) {
		memset(&obj, 0, sizeof(obj));
		ret = _copy_from_user(&obj, ptr, sizeof(obj), param->size);
		if (ret)
			break;

		/* Sanity check initial range */
		if (obj.size == 0 || obj.virtoffset + obj.size < obj.size ||
			obj.virtoffset + obj.size > virt_entry->memdesc.size ||
			!(IS_ALIGNED(obj.virtoffset | obj.size, pg_sz))) {
			ret = -EINVAL;
			break;
		}

		if (obj.flags & KGSL_SPARSE_BIND)
			ret = sparse_bind_range(private, &obj, virt_entry);
		else if (obj.flags & KGSL_SPARSE_UNBIND)
			ret = sparse_unbind_range(&obj, virt_entry);
		else
			ret = -EINVAL;
		if (ret)
			break;

		ptr += sizeof(obj);
	}

	kgsl_mem_entry_put(virt_entry);

	return ret;
}

long kgsl_ioctl_gpu_sparse_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpu_sparse_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	struct kgsl_drawobj_sparse *sparseobj;
	long result;
	unsigned int i = 0;

	/* Make sure sparse and syncpoint count isn't too big */
	if (param->numsparse > KGSL_MAX_SPARSE ||
		param->numsyncs > KGSL_MAX_SYNCPOINTS)
		return -EINVAL;

	/* Make sure there is atleast one sparse or sync */
	if (param->numsparse == 0 && param->numsyncs == 0)
		return -EINVAL;

	/* Only Sparse commands are supported in this ioctl */
	if (!(param->flags & KGSL_DRAWOBJ_SPARSE) || (param->flags &
			(KGSL_DRAWOBJ_SUBMIT_IB_LIST | KGSL_DRAWOBJ_MARKER
			| KGSL_DRAWOBJ_SYNC)))
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	/* Restrict bind commands to bind context */
	if (!(context->flags & KGSL_CONTEXT_SPARSE)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (param->numsyncs) {
		struct kgsl_drawobj_sync *syncobj = kgsl_drawobj_sync_create(
				device, context);
		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);
		result = kgsl_drawobj_sync_add_synclist(device, syncobj,
				to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (result)
			goto done;
	}

	if (param->numsparse) {
		sparseobj = kgsl_drawobj_sparse_create(device, context,
					param->flags);
		if (IS_ERR(sparseobj)) {
			result = PTR_ERR(sparseobj);
			goto done;
		}

		sparseobj->id = param->id;
		drawobj[i++] = DRAWOBJ(sparseobj);
		result = kgsl_drawobj_sparse_add_sparselist(device, sparseobj,
				param->id, to_user_ptr(param->sparselist),
				param->sparsesize, param->numsparse);
		if (result)
			goto done;
	}

	result = dev_priv->device->ftbl->queue_cmds(dev_priv, context,
					drawobj, i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);

	kgsl_context_put(context);
	return result;
}

void kgsl_sparse_bind(struct kgsl_process_private *private,
		struct kgsl_drawobj_sparse *sparseobj)
{
	struct kgsl_sparseobj_node *sparse_node;
	struct kgsl_mem_entry *virt_entry = NULL;
	long ret = 0;
	char *name;

	virt_entry = kgsl_sharedmem_find_id_flags(private, sparseobj->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (virt_entry == NULL)
		return;

	list_for_each_entry(sparse_node, &sparseobj->sparselist, node) {
		if (sparse_node->obj.flags & KGSL_SPARSE_BIND) {
			ret = sparse_bind_range(private, &sparse_node->obj,
					virt_entry);
			name = "bind";
		} else {
			ret = sparse_unbind_range(&sparse_node->obj,
					virt_entry);
			name = "unbind";
		}

		if (ret)
			KGSL_CORE_ERR("kgsl: Unable to '%s' ret %ld virt_id %d, phys_id %d, virt_offset %16.16llX, phys_offset %16.16llX, size %16.16llX, flags %16.16llX\n",
				name, ret, sparse_node->virt_id,
				sparse_node->obj.id,
				sparse_node->obj.virtoffset,
				sparse_node->obj.physoffset,
				sparse_node->obj.size, sparse_node->obj.flags);
	}

	kgsl_mem_entry_put(virt_entry);
}
EXPORT_SYMBOL(kgsl_sparse_bind);

long kgsl_ioctl_gpuobj_info(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_info *param = data;
	struct kgsl_mem_entry *entry;

	if (param->id == 0)
		return -EINVAL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	param->id = entry->id;
	param->gpuaddr = entry->memdesc.gpuaddr;
	param->flags = entry->memdesc.flags;
	param->size = entry->memdesc.size;
	param->va_len = kgsl_memdesc_footprint(&entry->memdesc);
	param->va_addr = (uint64_t) entry->memdesc.useraddr;

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_gpuobj_set_info(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_set_info *param = data;
	struct kgsl_mem_entry *entry;

	if (param->id == 0)
		return -EINVAL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	if (param->flags & KGSL_GPUOBJ_SET_INFO_METADATA)
		copy_metadata(entry, param->metadata, param->metadata_len);

	if (param->flags & KGSL_GPUOBJ_SET_INFO_TYPE) {
		entry->memdesc.flags &= ~((uint64_t) KGSL_MEMTYPE_MASK);
		entry->memdesc.flags |= param->type << KGSL_MEMTYPE_SHIFT;
	}

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_cff_syncmem(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_cff_syncmem *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	uint64_t offset, len;

	entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);
	if (entry == NULL)
		return -EINVAL;

	/*
	 * Calculate the offset between the requested GPU address and the start
	 * of the object
	 */

	offset = ((uint64_t) param->gpuaddr) - entry->memdesc.gpuaddr;

	if ((offset + param->len) > entry->memdesc.size)
		len = entry->memdesc.size - offset;
	else
		len = param->len;

	kgsl_cffdump_syncmem(dev_priv->device, entry, offset, len, true);

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_cff_sync_gpuobj(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_cff_sync_gpuobj *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	kgsl_cffdump_syncmem(dev_priv->device, entry, param->offset,
		param->length, true);

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_cff_user_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_cff_user_event *param = data;

	kgsl_cffdump_user_event(dev_priv->device, param->cff_opcode,
			param->op1, param->op2,
			param->op3, param->op4, param->op5);

	return result;
}

/**
 * kgsl_ioctl_timestamp_event - Register a new timestamp event from userspace
 * @dev_priv - pointer to the private device structure
 * @cmd - the ioctl cmd passed from kgsl_ioctl
 * @data - the user data buffer from kgsl_ioctl
 * @returns 0 on success or error code on failure
 */

long kgsl_ioctl_timestamp_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_timestamp_event *param = data;
	int ret;

	switch (param->type) {
	case KGSL_TIMESTAMP_EVENT_FENCE:
		ret = kgsl_add_fence_event(dev_priv->device,
			param->context_id, param->timestamp, param->priv,
			param->len, dev_priv);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int
kgsl_mmap_memstore(struct kgsl_device *device, struct vm_area_struct *vma)
{
	struct kgsl_memdesc *memdesc = &device->memstore;
	int result;
	unsigned int vma_size = vma->vm_end - vma->vm_start;

	/* The memstore can only be mapped as read only */

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	if (memdesc->size  !=  vma_size) {
		KGSL_MEM_ERR(device, "memstore bad size: %d should be %llu\n",
			     vma_size, memdesc->size);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	result = remap_pfn_range(vma, vma->vm_start,
				device->memstore.physaddr >> PAGE_SHIFT,
				 vma_size, vma->vm_page_prot);
	if (result != 0)
		KGSL_MEM_ERR(device, "remap_pfn_range failed: %d\n",
			     result);

	return result;
}

/*
 * kgsl_gpumem_vm_open is called whenever a vma region is copied or split.
 * Increase the refcount to make sure that the accounting stays correct
 */

static void kgsl_gpumem_vm_open(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (kgsl_mem_entry_get(entry) == 0)
		vma->vm_private_data = NULL;
}

static int
kgsl_gpumem_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (!entry)
		return VM_FAULT_SIGBUS;
	if (!entry->memdesc.ops || !entry->memdesc.ops->vmfault)
		return VM_FAULT_SIGBUS;

	return entry->memdesc.ops->vmfault(&entry->memdesc, vma, vmf);
}

static void
kgsl_gpumem_vm_close(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry  = vma->vm_private_data;

	if (!entry)
		return;

	entry->memdesc.useraddr = 0;
	kgsl_mem_entry_put(entry);
}

static struct vm_operations_struct kgsl_gpumem_vm_ops = {
	.open  = kgsl_gpumem_vm_open,
	.fault = kgsl_gpumem_vm_fault,
	.close = kgsl_gpumem_vm_close,
};

static int
get_mmap_entry(struct kgsl_process_private *private,
		struct kgsl_mem_entry **out_entry, unsigned long pgoff,
		unsigned long len)
{
	int ret = 0;
	struct kgsl_mem_entry *entry;

	entry = kgsl_sharedmem_find_id(private, pgoff);
	if (entry == NULL)
		entry = kgsl_sharedmem_find(private, pgoff << PAGE_SHIFT);

	if (!entry)
		return -EINVAL;

	if (!entry->memdesc.ops ||
		!entry->memdesc.ops->vmflags ||
		!entry->memdesc.ops->vmfault) {
		ret = -EINVAL;
		goto err_put;
	}

	if (entry->memdesc.flags & KGSL_MEMFLAGS_SPARSE_PHYS) {
		if (len != entry->memdesc.size) {
			ret = -EINVAL;
			goto err_put;
		}
	}

	if (entry->memdesc.useraddr != 0) {
		ret = -EBUSY;
		goto err_put;
	}

	if (kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		if (len != kgsl_memdesc_footprint(&entry->memdesc)) {
			ret = -ERANGE;
			goto err_put;
		}
	} else if (len != kgsl_memdesc_footprint(&entry->memdesc) &&
		len != entry->memdesc.size) {
		/*
		 * If cpu_map != gpumap then user can map either the
		 * footprint or the entry size
		 */
		ret = -ERANGE;
		goto err_put;
	}

	*out_entry = entry;
	return 0;
err_put:
	kgsl_mem_entry_put(entry);
	return ret;
}

static unsigned long _gpu_set_svm_region(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry, unsigned long addr,
		unsigned long size)
{
	int ret;

	ret = kgsl_mmu_set_svm_region(private->pagetable, (uint64_t) addr,
		(uint64_t) size);

	if (ret != 0)
		return ret;

	entry->memdesc.gpuaddr = (uint64_t) addr;
	entry->memdesc.pagetable = private->pagetable;

	ret = kgsl_mmu_map(private->pagetable, &entry->memdesc);
	if (ret) {
		kgsl_mmu_put_gpuaddr(&entry->memdesc);
		return ret;
	}

	kgsl_memfree_purge(private->pagetable, entry->memdesc.gpuaddr,
		entry->memdesc.size);

	return addr;
}

static unsigned long _gpu_find_svm(struct kgsl_process_private *private,
		unsigned long start, unsigned long end, unsigned long len,
		unsigned int align)
{
	uint64_t addr = kgsl_mmu_find_svm_region(private->pagetable,
		(uint64_t) start, (uint64_t)end, (uint64_t) len, align);

	BUG_ON(!IS_ERR_VALUE((unsigned long)addr) && (addr > ULONG_MAX));

	return (unsigned long) addr;
}

/* Search top down in the CPU VM region for a free address */
static unsigned long _cpu_get_unmapped_area(unsigned long bottom,
		unsigned long top, unsigned long len, unsigned long align)
{
	struct vm_unmapped_area_info info;
	unsigned long addr, err;

	info.flags = VM_UNMAPPED_AREA_TOPDOWN;
	info.low_limit = bottom;
	info.high_limit = top;
	info.length = len;
	info.align_offset = 0;
	info.align_mask = align - 1;

	addr = vm_unmapped_area(&info);

	if (IS_ERR_VALUE(addr))
		return addr;

	err = security_mmap_addr(addr);
	return err ? err : addr;
}

static unsigned long _search_range(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry,
		unsigned long start, unsigned long end,
		unsigned long len, uint64_t align)
{
	unsigned long cpu, gpu = end, result = -ENOMEM;

	while (gpu > start) {
		/* find a new empty spot on the CPU below the last one */
		cpu = _cpu_get_unmapped_area(start, gpu, len,
			(unsigned long) align);
		if (IS_ERR_VALUE(cpu)) {
			result = cpu;
			break;
		}
		/* try to map it on the GPU */
		result = _gpu_set_svm_region(private, entry, cpu, len);
		if (!IS_ERR_VALUE(result))
			break;

		trace_kgsl_mem_unmapped_area_collision(entry, cpu, len);

		if (cpu <= start) {
			result = -ENOMEM;
			break;
		}

		/* move downward to the next empty spot on the GPU */
		gpu = _gpu_find_svm(private, start, cpu, len, align);
		if (IS_ERR_VALUE(gpu)) {
			result = gpu;
			break;
		}

		/* Check that_gpu_find_svm doesn't put us in a loop */
		BUG_ON(gpu >= cpu);

		/* Break if the recommended GPU address is out of range */
		if (gpu < start) {
			result = -ENOMEM;
			break;
		}

		/*
		 * Add the length of the chunk to the GPU address to yield the
		 * upper bound for the CPU search
		 */
		gpu += len;
	}
	return result;
}

static unsigned long _get_svm_area(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry, unsigned long hint,
		unsigned long len, unsigned long flags)
{
	uint64_t start, end;
	int align_shift = kgsl_memdesc_get_align(&entry->memdesc);
	uint64_t align;
	unsigned long result;
	unsigned long addr;

	if (align_shift >= ilog2(SZ_2M))
		align = SZ_2M;
	else if (align_shift >= ilog2(SZ_1M))
		align = SZ_1M;
	else if (align_shift >= ilog2(SZ_64K))
		align = SZ_64K;
	else
		align = SZ_4K;

	/* get the GPU pagetable's SVM range */
	if (kgsl_mmu_svm_range(private->pagetable, &start, &end,
				entry->memdesc.flags))
		return -ERANGE;

	/* now clamp the range based on the CPU's requirements */
	start = max_t(uint64_t, start, mmap_min_addr);
	end = min_t(uint64_t, end, current->mm->mmap_base);
	if (start >= end)
		return -ERANGE;

	if (flags & MAP_FIXED) {
		/* we must use addr 'hint' or fail */
		return _gpu_set_svm_region(private, entry, hint, len);
	} else if (hint != 0) {
		struct vm_area_struct *vma;

		/*
		 * See if the hint is usable, if not we will use
		 * it as the start point for searching.
		 */
		addr = clamp_t(unsigned long, hint & ~(align - 1),
				start, (end - len) & ~(align - 1));

		vma = find_vma(current->mm, addr);

		if (vma == NULL || ((addr + len) <= vma->vm_start)) {
			result = _gpu_set_svm_region(private, entry, addr, len);

			/* On failure drop down to keep searching */
			if (!IS_ERR_VALUE(result))
				return result;
		}
	} else {
		/* no hint, start search at the top and work down */
		addr = end & ~(align - 1);
	}

	/*
	 * Search downwards from the hint first. If that fails we
	 * must try to search above it.
	 */
	result = _search_range(private, entry, start, addr, len, align);
	if (IS_ERR_VALUE(result) && hint != 0)
		result = _search_range(private, entry, addr, end, len, align);

	return result;
}

static unsigned long
kgsl_get_unmapped_area(struct file *file, unsigned long addr,
			unsigned long len, unsigned long pgoff,
			unsigned long flags)
{
	unsigned long val;
	unsigned long vma_offset = pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_mem_entry *entry = NULL;

	if (vma_offset == (unsigned long) device->memstore.gpuaddr)
		return get_unmapped_area(NULL, addr, len, pgoff, flags);

	val = get_mmap_entry(private, &entry, pgoff, len);
	if (val)
		return val;

	/* Do not allow CPU mappings for secure buffers */
	if (kgsl_memdesc_is_secured(&entry->memdesc)) {
		val = -EPERM;
		goto put;
	}

	if (!kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		val = get_unmapped_area(NULL, addr, len, 0, flags);
		if (IS_ERR_VALUE(val))
			KGSL_MEM_ERR(device,
				"get_unmapped_area: pid %d addr %lx pgoff %lx len %ld failed error %d\n",
				private->pid, addr, pgoff, len, (int) val);
	} else {
		 val = _get_svm_area(private, entry, addr, len, flags);
		 if (IS_ERR_VALUE(val))
			KGSL_MEM_ERR(device,
				"_get_svm_area: pid %d mmap_base %lx addr %lx pgoff %lx len %ld failed error %d\n",
				private->pid, current->mm->mmap_base, addr,
				pgoff, len, (int) val);
	}

put:
	kgsl_mem_entry_put(entry);
	return val;
}

static int kgsl_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int ret, cache;
	unsigned long vma_offset = vma->vm_pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_device *device = dev_priv->device;

	/* Handle leagacy behavior for memstore */

	if (vma_offset == (unsigned long) device->memstore.gpuaddr)
		return kgsl_mmap_memstore(device, vma);

	/*
	 * The reference count on the entry that we get from
	 * get_mmap_entry() will be held until kgsl_gpumem_vm_close().
	 */
	ret = get_mmap_entry(private, &entry, vma->vm_pgoff,
				vma->vm_end - vma->vm_start);
	if (ret)
		return ret;

	vma->vm_flags |= entry->memdesc.ops->vmflags;

	vma->vm_private_data = entry;

	/* Determine user-side caching policy */

	cache = kgsl_memdesc_get_cachemode(&entry->memdesc);

	switch (cache) {
	case KGSL_CACHEMODE_UNCACHED:
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		break;
	case KGSL_CACHEMODE_WRITETHROUGH:
		vma->vm_page_prot = pgprot_writethroughcache(vma->vm_page_prot);
		if (vma->vm_page_prot ==
			pgprot_writebackcache(vma->vm_page_prot))
			WARN_ONCE(1, "WRITETHROUGH is deprecated for arm64");
		break;
	case KGSL_CACHEMODE_WRITEBACK:
		vma->vm_page_prot = pgprot_writebackcache(vma->vm_page_prot);
		break;
	case KGSL_CACHEMODE_WRITECOMBINE:
	default:
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		break;
	}

	vma->vm_ops = &kgsl_gpumem_vm_ops;

	if (cache == KGSL_CACHEMODE_WRITEBACK
		|| cache == KGSL_CACHEMODE_WRITETHROUGH) {
		int i;
		unsigned long addr = vma->vm_start;
		struct kgsl_memdesc *m = &entry->memdesc;

		for (i = 0; i < m->page_count; i++) {
			struct page *page = m->pages[i];

			vm_insert_page(vma, addr, page);
			addr += PAGE_SIZE;
		}
	}

	vma->vm_file = file;

	entry->memdesc.useraddr = vma->vm_start;

	trace_kgsl_mem_mmap(entry);
	return 0;
}

static irqreturn_t kgsl_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;

	return device->ftbl->irq_handler(device);

}

#define KGSL_READ_MESSAGE "OH HAI GPU\n"

static ssize_t kgsl_read(struct file *filep, char __user *buf, size_t count,
		loff_t *pos)
{
	return simple_read_from_buffer(buf, count, pos,
			KGSL_READ_MESSAGE, strlen(KGSL_READ_MESSAGE) + 1);
}

static const struct file_operations kgsl_fops = {
	.owner = THIS_MODULE,
	.release = kgsl_release,
	.open = kgsl_open,
	.mmap = kgsl_mmap,
	.read = kgsl_read,
	.get_unmapped_area = kgsl_get_unmapped_area,
	.unlocked_ioctl = kgsl_ioctl,
	.compat_ioctl = kgsl_compat_ioctl,
};

struct kgsl_driver kgsl_driver  = {
	.process_mutex = __MUTEX_INITIALIZER(kgsl_driver.process_mutex),
	.ptlock = __SPIN_LOCK_UNLOCKED(kgsl_driver.ptlock),
	.devlock = __MUTEX_INITIALIZER(kgsl_driver.devlock),
	/*
	 * Full cache flushes are faster than line by line on at least
	 * 8064 and 8974 once the region to be flushed is > 16mb.
	 */
	.full_cache_threshold = SZ_16M,

	.stats.vmalloc = ATOMIC_LONG_INIT(0),
	.stats.vmalloc_max = ATOMIC_LONG_INIT(0),
	.stats.page_alloc = ATOMIC_LONG_INIT(0),
	.stats.page_alloc_max = ATOMIC_LONG_INIT(0),
	.stats.coherent = ATOMIC_LONG_INIT(0),
	.stats.coherent_max = ATOMIC_LONG_INIT(0),
	.stats.secure = ATOMIC_LONG_INIT(0),
	.stats.secure_max = ATOMIC_LONG_INIT(0),
	.stats.mapped = ATOMIC_LONG_INIT(0),
	.stats.mapped_max = ATOMIC_LONG_INIT(0),
};
EXPORT_SYMBOL(kgsl_driver);

static void _unregister_device(struct kgsl_device *device)
{
	int minor;

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (device == kgsl_driver.devp[minor])
			break;
	}
	if (minor != KGSL_DEVICE_MAX) {
		device_destroy(kgsl_driver.class,
				MKDEV(MAJOR(kgsl_driver.major), minor));
		kgsl_driver.devp[minor] = NULL;
	}
	mutex_unlock(&kgsl_driver.devlock);
}

static int _register_device(struct kgsl_device *device)
{
	int minor, ret;
	dev_t dev;

	/* Find a minor for the device */

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (kgsl_driver.devp[minor] == NULL) {
			kgsl_driver.devp[minor] = device;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.devlock);

	if (minor == KGSL_DEVICE_MAX) {
		KGSL_CORE_ERR("minor devices exhausted\n");
		return -ENODEV;
	}

	/* Create the device */
	dev = MKDEV(MAJOR(kgsl_driver.major), minor);
	device->dev = device_create(kgsl_driver.class,
				    &device->pdev->dev,
				    dev, device,
				    device->name);

	if (IS_ERR(device->dev)) {
		mutex_lock(&kgsl_driver.devlock);
		kgsl_driver.devp[minor] = NULL;
		mutex_unlock(&kgsl_driver.devlock);
		ret = PTR_ERR(device->dev);
		KGSL_CORE_ERR("device_create(%s): %d\n", device->name, ret);
		return ret;
	}

	dev_set_drvdata(&device->pdev->dev, device);
	return 0;
}

int kgsl_device_platform_probe(struct kgsl_device *device)
{
	int status = -EINVAL;
	struct resource *res;
	int cpu;

	status = _register_device(device);
	if (status)
		return status;

	/* Initialize logging first, so that failures below actually print. */
	kgsl_device_debugfs_init(device);

	status = kgsl_pwrctrl_init(device);
	if (status)
		goto error;

	/* Get starting physical address of device registers */
	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
					   device->iomemname);
	if (res == NULL) {
		KGSL_DRV_ERR(device, "platform_get_resource_byname failed\n");
		status = -EINVAL;
		goto error_pwrctrl_close;
	}
	if (res->start == 0 || resource_size(res) == 0) {
		KGSL_DRV_ERR(device, "dev %d invalid register region\n",
			device->id);
		status = -EINVAL;
		goto error_pwrctrl_close;
	}

	device->reg_phys = res->start;
	device->reg_len = resource_size(res);

	/*
	 * Check if a shadermemname is defined, and then get shader memory
	 * details including shader memory starting physical address
	 * and shader memory length
	 */
	if (device->shadermemname != NULL) {
		res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
						device->shadermemname);

		if (res == NULL) {
			KGSL_DRV_WARN(device,
			"Shader memory: platform_get_resource_byname failed\n");
		}

		else {
			device->shader_mem_phys = res->start;
			device->shader_mem_len = resource_size(res);
		}

		if (!devm_request_mem_region(device->dev,
					device->shader_mem_phys,
					device->shader_mem_len,
						device->name)) {
			KGSL_DRV_WARN(device, "request_mem_region_failed\n");
		}
	}

	if (!devm_request_mem_region(device->dev, device->reg_phys,
				device->reg_len, device->name)) {
		KGSL_DRV_ERR(device, "request_mem_region failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}

	device->reg_virt = devm_ioremap(device->dev, device->reg_phys,
					device->reg_len);

	if (device->reg_virt == NULL) {
		KGSL_DRV_ERR(device, "ioremap failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}
	/*acquire interrupt */
	device->pwrctrl.interrupt_num =
		platform_get_irq_byname(device->pdev, device->pwrctrl.irq_name);

	if (device->pwrctrl.interrupt_num <= 0) {
		KGSL_DRV_ERR(device, "platform_get_irq_byname failed: %d\n",
					 device->pwrctrl.interrupt_num);
		status = -EINVAL;
		goto error_pwrctrl_close;
	}

	status = devm_request_irq(device->dev, device->pwrctrl.interrupt_num,
				  kgsl_irq_handler, IRQF_TRIGGER_HIGH,
				  device->name, device);
	if (status) {
		KGSL_DRV_ERR(device, "request_irq(%d) failed: %d\n",
			      device->pwrctrl.interrupt_num, status);
		goto error_pwrctrl_close;
	}
	disable_irq(device->pwrctrl.interrupt_num);

	KGSL_DRV_INFO(device,
		"dev_id %d regs phys 0x%08lx size 0x%08x\n",
		device->id, device->reg_phys, device->reg_len);

	rwlock_init(&device->context_lock);

	setup_timer(&device->idle_timer, kgsl_timer, (unsigned long) device);

	status = kgsl_mmu_probe(device, kgsl_mmu_type);
	if (status != 0)
		goto error_pwrctrl_close;

	/* Check to see if our device can perform DMA correctly */
	status = dma_set_coherent_mask(&device->pdev->dev, KGSL_DMA_BIT_MASK);
	if (status)
		goto error_close_mmu;

	/* Initialize the memory pools */
	kgsl_init_page_pools(device->pdev);

	status = kgsl_allocate_global(device, &device->memstore,
		KGSL_MEMSTORE_SIZE, 0, KGSL_MEMDESC_CONTIG, "memstore");

	if (status != 0)
		goto error_close_mmu;

	status = kgsl_allocate_global(device, &device->scratch,
		PAGE_SIZE, 0, 0, "scratch");
	if (status != 0)
		goto error_free_memstore;

	/*
	 * The default request type PM_QOS_REQ_ALL_CORES is
	 * applicable to all CPU cores that are online and
	 * would have a power impact when there are more
	 * number of CPUs. PM_QOS_REQ_AFFINE_IRQ request
	 * type shall update/apply the vote only to that CPU to
	 * which IRQ's affinity is set to.
	 */
#ifdef CONFIG_SMP

	device->pwrctrl.pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
	device->pwrctrl.pm_qos_req_dma.irq = device->pwrctrl.interrupt_num;

#endif
	pm_qos_add_request(&device->pwrctrl.pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	if (device->pwrctrl.l2pc_cpus_mask) {

		device->pwrctrl.l2pc_cpus_qos.type =
				PM_QOS_REQ_AFFINE_CORES;
		cpumask_empty(&device->pwrctrl.l2pc_cpus_qos.cpus_affine);
		for_each_possible_cpu(cpu) {
			if ((1 << cpu) & device->pwrctrl.l2pc_cpus_mask)
				cpumask_set_cpu(cpu, &device->pwrctrl.
						l2pc_cpus_qos.cpus_affine);
		}

		pm_qos_add_request(&device->pwrctrl.l2pc_cpus_qos,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	}

	device->events_wq = alloc_workqueue("kgsl-events",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);

	/* Initalize the snapshot engine */
	kgsl_device_snapshot_init(device);

	/* Initialize common sysfs entries */
	kgsl_pwrctrl_init_sysfs(device);

	return 0;

error_free_memstore:
	kgsl_free_global(device, &device->memstore);
error_close_mmu:
	kgsl_mmu_close(device);
error_pwrctrl_close:
	kgsl_pwrctrl_close(device);
error:
	kgsl_device_debugfs_close(device);
	_unregister_device(device);
	return status;
}
EXPORT_SYMBOL(kgsl_device_platform_probe);

void kgsl_device_platform_remove(struct kgsl_device *device)
{
	destroy_workqueue(device->events_wq);

	kgsl_device_snapshot_close(device);

	kgsl_exit_page_pools();

	kgsl_pwrctrl_uninit_sysfs(device);

	pm_qos_remove_request(&device->pwrctrl.pm_qos_req_dma);
	if (device->pwrctrl.l2pc_cpus_mask)
		pm_qos_remove_request(&device->pwrctrl.l2pc_cpus_qos);

	idr_destroy(&device->context_idr);

	kgsl_free_global(device, &device->scratch);

	kgsl_free_global(device, &device->memstore);

	kgsl_mmu_close(device);

	kgsl_pwrctrl_close(device);

	kgsl_device_debugfs_close(device);
	_unregister_device(device);
}
EXPORT_SYMBOL(kgsl_device_platform_remove);

static void kgsl_core_exit(void)
{
	kgsl_events_exit();
	kgsl_cffdump_destroy();
	kgsl_core_debugfs_close();

	/*
	 * We call kgsl_sharedmem_uninit_sysfs() and device_unregister()
	 * only if kgsl_driver.virtdev has been populated.
	 * We check at least one member of kgsl_driver.virtdev to
	 * see if it is not NULL (and thus, has been populated).
	 */
	if (kgsl_driver.virtdev.class) {
		kgsl_sharedmem_uninit_sysfs();
		device_unregister(&kgsl_driver.virtdev);
	}

	if (kgsl_driver.class) {
		class_destroy(kgsl_driver.class);
		kgsl_driver.class = NULL;
	}

	kgsl_drawobjs_cache_exit();

	kgsl_memfree_exit();
	unregister_chrdev_region(kgsl_driver.major, KGSL_DEVICE_MAX);
}

static int __init kgsl_core_init(void)
{
	int result = 0;
	/* alloc major and minor device numbers */
	result = alloc_chrdev_region(&kgsl_driver.major, 0, KGSL_DEVICE_MAX,
		"kgsl");

	if (result < 0) {

		KGSL_CORE_ERR("alloc_chrdev_region failed err = %d\n", result);
		goto err;
	}

	cdev_init(&kgsl_driver.cdev, &kgsl_fops);
	kgsl_driver.cdev.owner = THIS_MODULE;
	kgsl_driver.cdev.ops = &kgsl_fops;
	result = cdev_add(&kgsl_driver.cdev, MKDEV(MAJOR(kgsl_driver.major), 0),
		       KGSL_DEVICE_MAX);

	if (result) {
		KGSL_CORE_ERR("kgsl: cdev_add() failed, dev_num= %d,"
			     " result= %d\n", kgsl_driver.major, result);
		goto err;
	}

	kgsl_driver.class = class_create(THIS_MODULE, "kgsl");

	if (IS_ERR(kgsl_driver.class)) {
		result = PTR_ERR(kgsl_driver.class);
		KGSL_CORE_ERR("failed to create class for kgsl");
		goto err;
	}

	/* Make a virtual device for managing core related things
	   in sysfs */
	kgsl_driver.virtdev.class = kgsl_driver.class;
	dev_set_name(&kgsl_driver.virtdev, "kgsl");
	result = device_register(&kgsl_driver.virtdev);
	if (result) {
		KGSL_CORE_ERR("driver_register failed\n");
		goto err;
	}

	/* Make kobjects in the virtual device for storing statistics */

	kgsl_driver.ptkobj =
	  kobject_create_and_add("pagetables",
				 &kgsl_driver.virtdev.kobj);

	kgsl_driver.prockobj =
		kobject_create_and_add("proc",
				       &kgsl_driver.virtdev.kobj);

	kgsl_core_debugfs_init();

	kgsl_sharedmem_init_sysfs();
	kgsl_cffdump_init();

	INIT_LIST_HEAD(&kgsl_driver.process_list);

	INIT_LIST_HEAD(&kgsl_driver.pagetable_list);

	kgsl_driver.workqueue = alloc_workqueue("kgsl-workqueue",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);

	kgsl_driver.mem_workqueue = alloc_workqueue("kgsl-mementry",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 0);

	kgsl_events_init();

	result = kgsl_drawobjs_cache_init();
	if (result)
		goto err;

	kgsl_memfree_init();

	return 0;

err:
	kgsl_core_exit();
	return result;
}

module_init(kgsl_core_init);
module_exit(kgsl_core_exit);

MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("MSM GPU driver");
MODULE_LICENSE("GPL");
