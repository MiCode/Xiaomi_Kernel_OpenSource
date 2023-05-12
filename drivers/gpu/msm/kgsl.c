// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2008-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <uapi/linux/sched/types.h>
#include <linux/bitfield.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <linux/fdtable.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mem-buf.h>
#include <linux/mman.h>
#include <linux/mm_types.h>
#include <linux/msm_kgsl.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/pm_runtime.h>
#include <linux/qcom_dma_heap.h>
#include <linux/security.h>
#include <linux/sort.h>
#include <linux/string_helpers.h>
#include <soc/qcom/of_common.h>
#include <soc/qcom/secure_buffer.h>

#include "kgsl_compat.h"
#include "kgsl_debugfs.h"
#include "kgsl_device.h"
#include "kgsl_eventlog.h"
#include "kgsl_mmu.h"
#include "kgsl_pool.h"
#include "kgsl_reclaim.h"
#include "kgsl_sync.h"
#include "kgsl_sysfs.h"
#include "kgsl_trace.h"
/* Instantiate tracepoints */
#define CREATE_TRACE_POINTS
#include "kgsl_power_trace.h"

#ifndef arch_mmap_check
#define arch_mmap_check(addr, len, flags)	(0)
#endif

#ifndef pgprot_writebackcache
#define pgprot_writebackcache(_prot)	(_prot)
#endif

#ifndef pgprot_writethroughcache
#define pgprot_writethroughcache(_prot)	(_prot)
#endif

#if defined(CONFIG_ARM64) || defined(CONFIG_ARM_LPAE)
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(64)
#else
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(32)
#endif

/* List of dmabufs mapped */
static LIST_HEAD(kgsl_dmabuf_list);
static DEFINE_SPINLOCK(kgsl_dmabuf_lock);

struct dmabuf_list_entry {
	struct page *firstpage;
	struct list_head node;
	struct list_head dmabuf_list;
};

struct kgsl_dma_buf_meta {
	struct kgsl_mem_entry *entry;
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct dmabuf_list_entry *dle;
	struct list_head node;
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

static const struct vm_operations_struct kgsl_gpumem_vm_ops;

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
	if (device)
		return device->ftbl->readtimestamp(device, priv, type,
			timestamp);
	return -EINVAL;

}

const char *kgsl_context_type(int type)
{
	if (type == KGSL_CONTEXT_TYPE_GL)
		return "GL";
	else if (type == KGSL_CONTEXT_TYPE_CL)
		return "CL";
	else if (type == KGSL_CONTEXT_TYPE_C2D)
		return "C2D";
	else if (type == KGSL_CONTEXT_TYPE_RS)
		return "RS";
	else if (type == KGSL_CONTEXT_TYPE_VK)
		return "VK";

	return "ANY";
}

/* Scheduled by kgsl_mem_entry_put_deferred() */
static void _deferred_put(struct work_struct *work)
{
	struct kgsl_mem_entry *entry =
		container_of(work, struct kgsl_mem_entry, work);

	kgsl_mem_entry_put(entry);
}

static struct kgsl_mem_entry *kgsl_mem_entry_create(void)
{
	struct kgsl_mem_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

	if (entry != NULL) {
		kref_init(&entry->refcount);
		/* put this ref in userspace memory alloc and map ioctls */
		kref_get(&entry->refcount);
		atomic_set(&entry->map_count, 0);
	}

	return entry;
}

static void add_dmabuf_list(struct kgsl_dma_buf_meta *metadata)
{
	struct kgsl_device *device = dev_get_drvdata(metadata->attach->dev);
	struct dmabuf_list_entry *dle;
	struct page *page;

	/*
	 * Get the first page. We will use it to identify the imported
	 * buffer, since the same buffer can be mapped as different
	 * mem entries.
	 */
	page = sg_page(metadata->table->sgl);

	spin_lock(&kgsl_dmabuf_lock);

	/* Go through the list to see if we imported this buffer before */
	list_for_each_entry(dle, &kgsl_dmabuf_list, node) {
		if (dle->firstpage == page) {
			/* Add the dmabuf metadata to the list for this dle */
			metadata->dle = dle;
			list_add(&metadata->node, &dle->dmabuf_list);
			spin_unlock(&kgsl_dmabuf_lock);
			return;
		}
	}

	/* This is a new buffer. Add a new entry for it */
	dle = kzalloc(sizeof(*dle), GFP_ATOMIC);
	if (dle) {
		dle->firstpage = page;
		INIT_LIST_HEAD(&dle->dmabuf_list);
		list_add(&dle->node, &kgsl_dmabuf_list);
		metadata->dle = dle;
		list_add(&metadata->node, &dle->dmabuf_list);
		kgsl_trace_gpu_mem_total(device,
				 metadata->entry->memdesc.size);
	}
	spin_unlock(&kgsl_dmabuf_lock);
}

static void remove_dmabuf_list(struct kgsl_dma_buf_meta *metadata)
{
	struct kgsl_device *device = dev_get_drvdata(metadata->attach->dev);
	struct dmabuf_list_entry *dle = metadata->dle;

	if (!dle)
		return;

	spin_lock(&kgsl_dmabuf_lock);
	list_del(&metadata->node);
	if (list_empty(&dle->dmabuf_list)) {
		list_del(&dle->node);
		kfree(dle);
		kgsl_trace_gpu_mem_total(device,
				-(metadata->entry->memdesc.size));
	}
	spin_unlock(&kgsl_dmabuf_lock);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static void kgsl_destroy_ion(struct kgsl_memdesc *memdesc)
{
	struct kgsl_mem_entry *entry = container_of(memdesc,
		struct kgsl_mem_entry, memdesc);
	struct kgsl_dma_buf_meta *metadata = entry->priv_data;

	if (metadata != NULL) {
		remove_dmabuf_list(metadata);
		dma_buf_unmap_attachment(metadata->attach, metadata->table,
			DMA_BIDIRECTIONAL);
		dma_buf_detach(metadata->dmabuf, metadata->attach);
		dma_buf_put(metadata->dmabuf);
		kfree(metadata);
	}

	memdesc->sgt = NULL;
}

static const struct kgsl_memdesc_ops kgsl_dmabuf_ops = {
	.free = kgsl_destroy_ion,
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
};
#endif

static void kgsl_destroy_anon(struct kgsl_memdesc *memdesc)
{
	int i = 0, j;
	struct scatterlist *sg;
	struct page *page;

	for_each_sg(memdesc->sgt->sgl, sg, memdesc->sgt->nents, i) {
		page = sg_page(sg);
		for (j = 0; j < (sg->length >> PAGE_SHIFT); j++) {

			/*
			 * Mark the page in the scatterlist as dirty if they
			 * were writable by the GPU.
			 */
			if (!(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY))
				set_page_dirty_lock(nth_page(page, j));

			/*
			 * Put the page reference taken using get_user_pages
			 * during memdesc_sg_virt.
			 */
			put_page(nth_page(page, j));
		}
	}

	sg_free_table(memdesc->sgt);
	kfree(memdesc->sgt);
	memdesc->sgt = NULL;
}

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

	/*
	 * VBO allocations at gpumem_alloc_vbo_entry are not added into stats
	 * (using kgsl_process_add_stats) so do not subtract here. For all other
	 * allocations subtract before freeing memdesc
	 */
	if (!(entry->memdesc.flags & KGSL_MEMFLAGS_VBO))
		atomic64_sub(entry->memdesc.size, &entry->priv->stats[memtype].cur);

	/* Detach from process list */
	kgsl_mem_entry_detach_process(entry);

	if (memtype != KGSL_MEM_ENTRY_KERNEL)
		atomic_long_sub(entry->memdesc.size,
			&kgsl_driver.stats.mapped);

	kgsl_sharedmem_free(&entry->memdesc);

	kfree(entry);
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

static int kgsl_mem_entry_attach_to_process(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	struct kgsl_memdesc *memdesc = &entry->memdesc;
	int ret, id;

	ret = kgsl_process_private_get(process);
	if (!ret)
		return -EBADF;

	/* Assign a gpu address */
	if (!kgsl_memdesc_use_cpu_map(memdesc) &&
		kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_NONE) {
		struct kgsl_pagetable *pagetable;

		pagetable = kgsl_memdesc_is_secured(memdesc) ?
			device->mmu.securepagetable : process->pagetable;

		ret = kgsl_mmu_get_gpuaddr(pagetable, memdesc);
		if (ret) {
			kgsl_process_private_put(process);
			return ret;
		}
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&process->mem_lock);
	/* Allocate the ID but don't attach the pointer just yet */
	id = idr_alloc(&process->mem_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&process->mem_lock);
	idr_preload_end();

	if (id < 0) {
		if (!kgsl_memdesc_use_cpu_map(memdesc))
			kgsl_mmu_put_gpuaddr(memdesc->pagetable, memdesc);
		kgsl_process_private_put(process);
		return id;
	}

	entry->id = id;
	entry->priv = process;

	return 0;
}

/*
 * Attach the memory object to a process by (possibly) getting a GPU address and
 * (possibly) mapping it
 */
static int kgsl_mem_entry_attach_and_map(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	struct kgsl_memdesc *memdesc = &entry->memdesc;
	int ret;

	ret = kgsl_mem_entry_attach_to_process(device, process, entry);
	if (ret)
		return ret;

	if (memdesc->gpuaddr) {
		/*
		 * Map the memory if a GPU address is already assigned, either
		 * through kgsl_mem_entry_attach_to_process() or via some other
		 * SVM process
		 */
		ret = kgsl_mmu_map(memdesc->pagetable, memdesc);

		if (ret) {
			kgsl_mem_entry_detach_process(entry);
			return ret;
		}
	}

	kgsl_memfree_purge(memdesc->pagetable, memdesc->gpuaddr,
		memdesc->size);

	return ret;
}

/* Detach a memory entry from a process and unmap it from the MMU */
static void kgsl_mem_entry_detach_process(struct kgsl_mem_entry *entry)
{
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

	spin_unlock(&entry->priv->mem_lock);

	kgsl_sharedmem_put_gpuaddr(&entry->memdesc);

	if (entry->memdesc.priv & KGSL_MEMDESC_RECLAIMED)
		atomic_sub(entry->memdesc.page_count,
					&entry->priv->unpinned_page_count);

	kgsl_process_private_put(entry->priv);

	entry->priv = NULL;
}

#ifdef CONFIG_QCOM_KGSL_CONTEXT_DEBUG
static void kgsl_context_debug_info(struct kgsl_device *device)
{
	struct kgsl_context *context;
	struct kgsl_process_private *p;
	int next;
	/*
	 * Keep an interval between consecutive logging to avoid
	 * flooding the kernel log
	 */
	static DEFINE_RATELIMIT_STATE(_rs, 10 * HZ, 1);

	if (!__ratelimit(&_rs))
		return;

	dev_info(device->dev, "KGSL active contexts:\n");
	dev_info(device->dev, "pid      process         total    attached   detached\n");

	read_lock(&kgsl_driver.proclist_lock);
	read_lock(&device->context_lock);

	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		int total_contexts = 0, num_detached = 0;

		idr_for_each_entry(&device->context_idr, context, next) {
			if (context->proc_priv == p) {
				total_contexts++;
				if (kgsl_context_detached(context))
					num_detached++;
			}
		}

		dev_info(device->dev, "%-8u %-15.15s %-8d %-10d %-10d\n",
				pid_nr(p->pid), p->comm, total_contexts,
				total_contexts - num_detached, num_detached);
	}

	read_unlock(&device->context_lock);
	read_unlock(&kgsl_driver.proclist_lock);
}
#else
static void kgsl_context_debug_info(struct kgsl_device *device)
{
}
#endif

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
		dev_err(device->dev, "  context[%u]: context detached\n",
			context->id);
	} else if (device->ftbl->drawctxt_dump != NULL)
		device->ftbl->drawctxt_dump(device, context);

	kgsl_context_put(context);
}

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
	int ret = 0, id;
	struct kgsl_process_private  *proc_priv = dev_priv->process_priv;

	/*
	 * Read and increment the context count under lock to make sure
	 * no process goes beyond the specified context limit.
	 */
	spin_lock(&proc_priv->ctxt_count_lock);
	if (atomic_read(&proc_priv->ctxt_count) > KGSL_MAX_CONTEXTS_PER_PROC) {
		dev_err(device->dev,
			     "Per process context limit reached for pid %u\n",
			     pid_nr(dev_priv->process_priv->pid));
		spin_unlock(&proc_priv->ctxt_count_lock);
		kgsl_context_debug_info(device);
		return -ENOSPC;
	}

	atomic_inc(&proc_priv->ctxt_count);
	spin_unlock(&proc_priv->ctxt_count_lock);

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
		if (id == -ENOSPC) {
			dev_warn(device->dev,
				      "cannot have more than %zu contexts due to memstore limitation\n",
				      KGSL_MEMSTORE_MAX);
			kgsl_context_debug_info(device);
		}
		atomic_dec(&proc_priv->ctxt_count);
		return id;
	}

	context->id = id;

	mutex_init(&context->fault_lock);
	INIT_LIST_HEAD(&context->faults);
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
	if (ret) {
		kgsl_process_private_put(dev_priv->process_priv);
		goto out;
	}

	kgsl_add_event_group(device, &context->events, context,
		kgsl_readtimestamp, context, "context-%d", id);

out:
	if (ret) {
		atomic_dec(&proc_priv->ctxt_count);
		write_lock(&device->context_lock);
		idr_remove(&dev_priv->device->context_idr, id);
		write_unlock(&device->context_lock);
	}

	return ret;
}

void kgsl_free_faults(struct kgsl_context *context)
{
	struct kgsl_fault_node *p, *tmp;

	if (!(context->flags & KGSL_CONTEXT_FAULT_INFO))
		return;

	list_for_each_entry_safe(p, tmp, &context->faults, node) {
		list_del(&p->node);
		kfree(p->priv);
		kfree(p);
	}
}

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
void kgsl_context_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;

	if (context == NULL)
		return;

	device = context->device;
	device->ftbl->dequeue_recurring_cmd(device, context);

	/*
	 * Mark the context as detached to keep others from using
	 * the context before it gets fully removed, and to make sure
	 * we don't try to detach twice.
	 */
	if (test_and_set_bit(KGSL_CONTEXT_PRIV_DETACHED, &context->priv))
		return;

	trace_kgsl_context_detach(device, context);

	context->device->ftbl->drawctxt_detach(context);

	/*
	 * Cancel all pending events after the device-specific context is
	 * detached, to avoid possibly freeing memory while it is still
	 * in use by the GPU.
	 */
	kgsl_cancel_events(device, &context->events);

	/* Remove the event group from the list */
	kgsl_del_event_group(device, &context->events);

	kgsl_sync_timeline_detach(context->ktimeline);
	kgsl_context_put(context);
}

void
kgsl_context_destroy(struct kref *kref)
{
	struct kgsl_context *context = container_of(kref, struct kgsl_context,
						    refcount);
	struct kgsl_device *device = context->device;

	trace_kgsl_context_destroy(device, context);

	/*
	 * It's not safe to destroy the context if it's not detached as GPU
	 * may still be executing commands
	 */
	BUG_ON(!kgsl_context_detached(context));

	kgsl_free_faults(context);
	kgsl_sync_timeline_put(context->ktimeline);

	write_lock(&device->context_lock);
	if (context->id != KGSL_CONTEXT_INVALID) {

		/* Clear the timestamps in the memstore during destroy */
		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp), 0);
		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp), 0);

		/* clear device power constraint */
		if (context->id == device->pwrctrl.constraint.owner_id) {
			trace_kgsl_constraint(device,
				device->pwrctrl.constraint.type,
				device->pwrctrl.active_pwrlevel,
				0);
			device->pwrctrl.constraint.type = KGSL_CONSTRAINT_NONE;
		}

		atomic_dec(&context->proc_priv->ctxt_count);
		idr_remove(&device->context_idr, context->id);
		context->id = KGSL_CONTEXT_INVALID;
	}
	write_unlock(&device->context_lock);
	kgsl_process_private_put(context->proc_priv);

	device->ftbl->drawctxt_destroy(context);
}

struct kgsl_device *kgsl_get_device(int dev_idx)
{
	int i;
	struct kgsl_device *ret = NULL;

	mutex_lock(&kgsl_driver.devlock);

	for (i = 0; i < ARRAY_SIZE(kgsl_driver.devp); i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->id == dev_idx) {
			ret = kgsl_driver.devp[i];
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}

static struct kgsl_device *kgsl_get_minor(int minor)
{
	struct kgsl_device *ret = NULL;

	if (minor < 0 || minor >= ARRAY_SIZE(kgsl_driver.devp))
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
bool kgsl_check_timestamp(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	unsigned int ts_processed;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&ts_processed);

	return (timestamp_cmp(ts_processed, timestamp) >= 0);
}

static void kgsl_work_period_release(struct kref *kref)
{
	struct gpu_work_period *wp = container_of(kref,
			struct gpu_work_period, refcount);

	spin_lock(&kgsl_driver.wp_list_lock);
	if (!list_empty(&wp->list))
		list_del_init(&wp->list);
	spin_unlock(&kgsl_driver.wp_list_lock);

	kfree(wp);
}

static void kgsl_put_work_period(struct gpu_work_period *wp)
{
	if (!IS_ERR_OR_NULL(wp))
		kref_put(&wp->refcount, kgsl_work_period_release);
}

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

	kgsl_put_work_period(private->period);
	/*
	 * While removing sysfs entries, kernfs_mutex is held by sysfs apis. Since
	 * it is a global fs mutex, sometimes it takes longer for kgsl to get hold
	 * of the lock. Meanwhile, kgsl open thread may exhaust all its re-tries
	 * and open can fail. To avoid this, remove sysfs entries inside process
	 * mutex to avoid wasting re-tries when kgsl is waiting for kernfs mutex.
	 */
	mutex_lock(&kgsl_driver.process_mutex);

	debugfs_remove_recursive(private->debug_root);
	kobject_put(&private->kobj_memtype);
	kobject_put(&private->kobj);

	/* When using global pagetables, do not detach global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_detach_pagetable(private->pagetable);

	/* Remove the process struct from the master list */
	write_lock(&kgsl_driver.proclist_lock);
	list_del(&private->list);
	write_unlock(&kgsl_driver.proclist_lock);
	mutex_unlock(&kgsl_driver.process_mutex);

	kfree(private->cmdline);
	put_pid(private->pid);
	idr_destroy(&private->mem_idr);
	idr_destroy(&private->syncsource_idr);

	/* When using global pagetables, do not put global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_putpagetable(private->pagetable);

	kfree(private);
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

	read_lock(&kgsl_driver.proclist_lock);
	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		if (pid_nr(p->pid) == pid) {
			if (kgsl_process_private_get(p))
				private = p;
			break;
		}
	}
	read_unlock(&kgsl_driver.proclist_lock);

	return private;
}

void kgsl_work_period_update(struct kgsl_device *device,
				  struct gpu_work_period *period, u64 active)
{
	spin_lock(&device->work_period_lock);
	if (test_bit(KGSL_WORK_PERIOD, &period->flags)) {
		period->active += active;
		period->cmds++;
	}
	spin_unlock(&device->work_period_lock);
}

static void _defer_work_period_put(struct work_struct *work)
{
	struct gpu_work_period *wp =
		container_of(work, struct gpu_work_period, defer_ws);

	/* Put back the refcount that was taken in kgsl_drawobj_cmd_create() */
	kgsl_put_work_period(wp);
}

#define KGSL_GPU_ID 1
static void _log_gpu_work_events(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
							work_period_ws);
	struct gpu_work_period *wp;
	u64 active_time;
	bool restart = false;

	spin_lock(&device->work_period_lock);
	device->gpu_period.end = ktime_get_ns();

	spin_lock(&kgsl_driver.wp_list_lock);
	list_for_each_entry(wp, &kgsl_driver.wp_list, list) {
		if (!test_bit(KGSL_WORK_PERIOD, &wp->flags))
			continue;

		/* Active time in XO cycles(19.2MHz), convert to nanoseconds */
		active_time = wp->active * 10000;
		do_div(active_time, 192);

		/* Ensure active_time is within work period */
		active_time = min_t(u64, active_time,
			device->gpu_period.end - device->gpu_period.begin);
		/*
		 * Emit GPU work period events via a kernel tracepoint
		 * to provide information to the Android OS about how
		 * apps are using the GPU.
		 */
		if (active_time)
			trace_gpu_work_period(KGSL_GPU_ID, wp->uid,
					device->gpu_period.begin,
					device->gpu_period.end,
					active_time);
		/* Reset gpu work period stats */
		wp->active = 0;
		wp->cmds = 0;
		atomic_set(&wp->frames, 0);

		/* make sure other CPUs see the update */
		smp_wmb();

		if (!atomic_read(&wp->active_cmds)) {
			__clear_bit(KGSL_WORK_PERIOD, &wp->flags);
			queue_work(kgsl_driver.lockless_workqueue, &wp->defer_ws);
		} else {
			restart = true;
		}
	}
	spin_unlock(&kgsl_driver.wp_list_lock);

	if (restart) {
		/*
		 * GPU work period duration (end time - begin time) must be at
		 * most 1 second. The event for a period must be emitted within
		 * 1 second of the end time of the period. Restart timer within
		 * 1 second to emit gpu work period events.
		 */
		mod_timer(&device->work_period_timer,
			  jiffies + msecs_to_jiffies(KGSL_WORK_PERIOD_MS));
		device->gpu_period.begin = device->gpu_period.end;
	} else {
		memset(&device->gpu_period, 0, sizeof(device->gpu_period));
		__clear_bit(KGSL_WORK_PERIOD, &device->flags);
	}
	spin_unlock(&device->work_period_lock);

}

static void kgsl_work_period_timer(struct timer_list *t)
{
	struct kgsl_device *device = from_timer(device, t, work_period_timer);

	queue_work(kgsl_driver.lockless_workqueue, &device->work_period_ws);
}

static struct gpu_work_period *kgsl_get_work_period(uid_t uid)
{
	struct gpu_work_period *wp;

	spin_lock(&kgsl_driver.wp_list_lock);
	list_for_each_entry(wp, &kgsl_driver.wp_list, list) {
		if ((uid == wp->uid) && kref_get_unless_zero(&wp->refcount)) {
			spin_unlock(&kgsl_driver.wp_list_lock);
			return wp;
		}
	}

	wp = kzalloc(sizeof(*wp), GFP_ATOMIC);
	if (!wp) {
		spin_unlock(&kgsl_driver.wp_list_lock);
		return ERR_PTR(-ENOMEM);
	}

	kref_init(&wp->refcount);
	wp->uid = uid;
	INIT_WORK(&wp->defer_ws, _defer_work_period_put);
	list_add(&wp->list, &kgsl_driver.wp_list);
	spin_unlock(&kgsl_driver.wp_list_lock);

	return wp;
}

static struct kgsl_process_private *kgsl_process_private_new(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;
	struct pid *cur_pid = get_task_pid(current->group_leader, PIDTYPE_PID);

	/* Search in the process list */
	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == cur_pid) {
			if (!kgsl_process_private_get(private)) {
				/*
				 * This will happen only if refcount is zero
				 * i.e. destroy is triggered but didn't complete
				 * yet. Return -EEXIST to indicate caller that
				 * destroy is pending to allow caller to take
				 * appropriate action.
				 */
				private = ERR_PTR(-EEXIST);
			} else {
				mutex_lock(&private->private_mutex);
				private->fd_count++;
				mutex_unlock(&private->private_mutex);
			}

			/*
			 * We need to hold only one reference to the PID for
			 * each process struct to avoid overflowing the
			 * reference counter which can lead to use-after-free.
			 */
			put_pid(cur_pid);
			return private;
		}
	}

	/* Create a new object */
	private = kzalloc(sizeof(struct kgsl_process_private), GFP_KERNEL);
	if (private == NULL) {
		put_pid(cur_pid);
		return ERR_PTR(-ENOMEM);
	}

	private->period = kgsl_get_work_period(current_uid().val);
	if (IS_ERR(private->period)) {
		int err = PTR_ERR(private->period);

		kfree(private);
		return ERR_PTR(err);
	}

	kref_init(&private->refcount);

	private->fd_count = 1;
	private->pid = cur_pid;
	get_task_comm(private->comm, current->group_leader);
	private->cmdline = kstrdup_quotable_cmdline(current, GFP_KERNEL);

	spin_lock_init(&private->mem_lock);
	spin_lock_init(&private->syncsource_lock);
	spin_lock_init(&private->ctxt_count_lock);
	mutex_init(&private->private_mutex);

	idr_init(&private->mem_idr);
	idr_init(&private->syncsource_idr);

	kgsl_reclaim_proc_private_init(private);

	/* Allocate a pagetable for the new process object */
	private->pagetable = kgsl_mmu_getpagetable(&device->mmu, pid_nr(cur_pid));
	if (IS_ERR(private->pagetable)) {
		int err = PTR_ERR(private->pagetable);

		kgsl_put_work_period(private->period);
		idr_destroy(&private->mem_idr);
		idr_destroy(&private->syncsource_idr);
		put_pid(private->pid);

		kfree(private);
		private = ERR_PTR(err);
		return private;
	}

	kgsl_process_init_sysfs(device, private);
	kgsl_process_init_debugfs(private);
	write_lock(&kgsl_driver.proclist_lock);
	list_add(&private->list, &kgsl_driver.process_list);
	write_unlock(&kgsl_driver.proclist_lock);

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

static void kgsl_process_private_close(struct kgsl_device_private *dev_priv,
		struct kgsl_process_private *private)
{
	mutex_lock(&private->private_mutex);

	if (--private->fd_count > 0) {
		mutex_unlock(&private->private_mutex);
		kgsl_process_private_put(private);
		return;
	}

	/*
	 * If this is the last file on the process garbage collect
	 * any outstanding resources
	 */
	process_release_memory(private);

	/* Release all syncsource objects from process private */
	kgsl_syncsource_process_release_syncsources(private);

	mutex_unlock(&private->private_mutex);

	kgsl_process_private_put(private);
}

static struct kgsl_process_private *_process_private_open(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;

	mutex_lock(&kgsl_driver.process_mutex);
	private = kgsl_process_private_new(device);
	mutex_unlock(&kgsl_driver.process_mutex);

	return private;
}

static struct kgsl_process_private *kgsl_process_private_open(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;
	int i;

	private = _process_private_open(device);

	/*
	 * If we get error and error is -EEXIST that means previous process
	 * private destroy is triggered but didn't complete. Retry creating
	 * process private after sometime to allow previous destroy to complete.
	 */
	for (i = 0; (PTR_ERR_OR_ZERO(private) == -EEXIST) && (i < 50); i++) {
		usleep_range(10, 100);
		private = _process_private_open(device);
	}

	return private;
}

int kgsl_gpu_frame_count(pid_t pid, u64 *frame_count)
{
	struct kgsl_process_private *p;

	if (!frame_count)
		return -EINVAL;

	p = kgsl_process_private_find(pid);
	if (!p)
		return -ENOENT;

	*frame_count = atomic64_read(&p->frame_count);
	kgsl_process_private_put(p);

	return 0;
}
EXPORT_SYMBOL(kgsl_gpu_frame_count);

int kgsl_add_rcu_notifier(struct notifier_block *nb)
{
	struct kgsl_device *device = kgsl_get_device(0);

	return srcu_notifier_chain_register(&device->nh, nb);
}
EXPORT_SYMBOL(kgsl_add_rcu_notifier);

int kgsl_del_rcu_notifier(struct notifier_block *nb)
{
	struct kgsl_device *device = kgsl_get_device(0);

	return srcu_notifier_chain_unregister(&device->nh, nb);
}
EXPORT_SYMBOL(kgsl_del_rcu_notifier);

static int kgsl_close_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	if (device->open_count == 1)
		result = device->ftbl->last_close(device);

	/*
	 * We must decrement the open_count after last_close() has finished.
	 * This is because last_close() relinquishes device mutex while
	 * waiting for active count to become 0. This opens up a window
	 * where a new process can come in, see that open_count is 0, and
	 * initiate a first_open(). This can potentially mess up the power
	 * state machine. To avoid a first_open() from happening before
	 * last_close() has finished, decrement the open_count after
	 * last_close().
	 */
	device->open_count--;
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

	/* Destroy the device-specific structure */
	device->ftbl->device_private_destroy(dev_priv);

	result = kgsl_close_device(device);
	pm_runtime_put(&device->pdev->dev);

	return result;
}

static int kgsl_open_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	if (device->open_count == 0) {
		result = device->ftbl->first_open(device);
		if (result)
			goto out;
	}
	device->open_count++;
out:
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
	if (device == NULL) {
		pr_err("kgsl: No device found\n");
		return -ENODEV;
	}

	result = pm_runtime_get_sync(&device->pdev->dev);
	if (result < 0) {
		dev_err(device->dev,
			     "Runtime PM: Unable to wake up the device, rc = %d\n",
			     result);
		return result;
	}
	result = 0;

	dev_priv = device->ftbl->device_private_create();
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
	int id;
	struct kgsl_mem_entry *entry, *ret = NULL;

	if (!private)
		return NULL;

	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, gpuaddr, 0) &&
		!kgsl_mmu_gpuaddr_in_range(
			private->pagetable->mmu->securepagetable, gpuaddr, 0))
		return NULL;

	spin_lock(&private->mem_lock);
	idr_for_each_entry(&private->mem_idr, entry, id) {
		if (GPUADDR_IN_MEMDESC(gpuaddr, &entry->memdesc)) {
			if (!entry->pending_free)
				ret = kgsl_mem_entry_get(entry);
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	return ret;
}

static struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id_flags(struct kgsl_process_private *process,
		unsigned int id, uint64_t flags)
{
	struct kgsl_mem_entry *entry, *ret = NULL;

	spin_lock(&process->mem_lock);
	entry = idr_find(&process->mem_idr, id);
	if (entry)
		if (!entry->pending_free &&
				(flags & entry->memdesc.flags) == flags)
			ret = kgsl_mem_entry_get(entry);
	spin_unlock(&process->mem_lock);

	return ret;
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

static int kgsl_get_ctxt_fault_stats(struct kgsl_context *context,
		struct kgsl_context_property *ctxt_property)
{
	struct kgsl_context_property_fault fault_stats;
	size_t copy;

	/* Return the size of the subtype struct */
	if (ctxt_property->size == 0) {
		ctxt_property->size = sizeof(fault_stats);
		return 0;
	}

	memset(&fault_stats, 0, sizeof(fault_stats));

	copy = min_t(size_t, ctxt_property->size, sizeof(fault_stats));

	fault_stats.faults = context->total_fault_count;
	fault_stats.timestamp = context->last_faulted_cmd_ts;

	/*
	 * Copy the context fault stats to data which also serves as
	 * the out parameter.
	 */
	if (copy_to_user(u64_to_user_ptr(ctxt_property->data),
				&fault_stats, copy))
		return -EFAULT;

	return 0;
}

static long kgsl_get_ctxt_properties(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	/* Return fault stats of given context */
	struct kgsl_context_property ctxt_property;
	struct kgsl_context *context;
	size_t copy;
	long ret;

	/*
	 * If sizebytes is zero, tell the user how big the
	 * ctxt_property struct should be.
	 */
	if (param->sizebytes == 0) {
		param->sizebytes = sizeof(ctxt_property);
		return 0;
	}

	memset(&ctxt_property, 0, sizeof(ctxt_property));

	copy = min_t(size_t, param->sizebytes, sizeof(ctxt_property));

	/* We expect the value passed in to contain the context id */
	if (copy_from_user(&ctxt_property, param->value, copy))
		return -EFAULT;

	/* ctxt type zero is not valid, as we consider it as uninitialized. */
	if (ctxt_property.type == 0)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv,
			ctxt_property.contextid);
	if (!context)
		return -EINVAL;

	if (ctxt_property.type == KGSL_CONTEXT_PROP_FAULTS)
		ret = kgsl_get_ctxt_fault_stats(context, &ctxt_property);
	else
		ret = -EOPNOTSUPP;

	kgsl_context_put(context);

	return ret;
}

static long kgsl_prop_version(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_version version = {
		.drv_major = KGSL_VERSION_MAJOR,
		.drv_minor = KGSL_VERSION_MINOR,
		.dev_major = 3,
		.dev_minor = 1,
	};

	if (param->sizebytes != sizeof(version))
		return -EINVAL;

	if (copy_to_user(param->value, &version, sizeof(version)))
		return -EFAULT;

	return 0;
}

/* Return reset status of given context and clear it */
static long kgsl_prop_gpu_reset_stat(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 id;
	struct kgsl_context *context;

	if (param->sizebytes != sizeof(id))
		return -EINVAL;

	/* We expect the value passed in to contain the context id */
	if (copy_from_user(&id, param->value, sizeof(id)))
		return -EFAULT;

	context = kgsl_context_get_owner(dev_priv, id);
	if (!context)
		return -EINVAL;

	/*
	 * Copy the reset status to value which also serves as
	 * the out parameter
	 */
	id = context->reset_status;

	context->reset_status = KGSL_CTX_STAT_NO_ERROR;
	kgsl_context_put(context);

	if (copy_to_user(param->value, &id, sizeof(id)))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_secure_buf_alignment(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 align = PAGE_SIZE;

	if (param->sizebytes != sizeof(align))
		return -EINVAL;

	if (copy_to_user(param->value, &align, sizeof(align)))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_secure_ctxt_support(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 secure;

	if (param->sizebytes != sizeof(secure))
		return -EINVAL;

	secure = dev_priv->device->mmu.secured ? 1 : 0;

	if (copy_to_user(param->value, &secure, sizeof(secure)))
		return -EFAULT;

	return 0;
}

static int kgsl_query_caps_properties(struct kgsl_device *device,
		struct kgsl_capabilities *caps)
{
	struct kgsl_capabilities_properties props;
	size_t copy;
	u32 count, *local;
	int ret;

	/* Return the size of the subtype struct */
	if (caps->size == 0) {
		caps->size = sizeof(props);
		return 0;
	}

	memset(&props, 0, sizeof(props));

	copy = min_t(size_t, caps->size, sizeof(props));

	if (copy_from_user(&props, u64_to_user_ptr(caps->data), copy))
		return -EFAULT;

	/* Get the number of properties */
	count = kgsl_query_property_list(device, NULL, 0);

	/*
	 * If the incoming user count is zero, they are querying the number of
	 * available properties. Set it and return.
	 */
	if (props.count == 0) {
		props.count = count;
		goto done;
	}

	/* Copy the lesser of the user or kernel property count */
	if (props.count < count)
		count = props.count;

	/* Create a local buffer to store the property list */
	local = kcalloc(count, sizeof(u32), GFP_KERNEL);
	if (!local)
		return -ENOMEM;

	/* Get the properties */
	props.count = kgsl_query_property_list(device, local, count);

	ret = copy_to_user(u64_to_user_ptr(props.list), local,
		props.count * sizeof(u32));

	kfree(local);

	if (ret)
		return -EFAULT;

done:
	if (copy_to_user(u64_to_user_ptr(caps->data), &props, copy))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_query_capabilities(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_capabilities caps;
	long ret;
	size_t copy;

	/*
	 * If sizebytes is zero, tell the user how big the capabilities struct
	 * should be
	 */
	if (param->sizebytes == 0) {
		param->sizebytes = sizeof(caps);
		return 0;
	}

	memset(&caps, 0, sizeof(caps));

	copy = min_t(size_t, param->sizebytes, sizeof(caps));

	if (copy_from_user(&caps, param->value, copy))
		return -EFAULT;

	/* querytype must be non zero */
	if (caps.querytype == 0)
		return -EINVAL;

	if (caps.querytype == KGSL_QUERY_CAPS_PROPERTIES)
		ret = kgsl_query_caps_properties(dev_priv->device, &caps);
	else {
		/* Unsupported querytypes should return a unique return value */
		return -EOPNOTSUPP;
	}

	if (copy_to_user(param->value, &caps, copy))
		return -EFAULT;

	return ret;
}

static const struct {
	int type;
	long (*func)(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param);
} kgsl_property_funcs[] = {
	{ KGSL_PROP_VERSION, kgsl_prop_version },
	{ KGSL_PROP_GPU_RESET_STAT, kgsl_prop_gpu_reset_stat},
	{ KGSL_PROP_SECURE_BUFFER_ALIGNMENT, kgsl_prop_secure_buf_alignment },
	{ KGSL_PROP_SECURE_CTXT_SUPPORT, kgsl_prop_secure_ctxt_support },
	{ KGSL_PROP_QUERY_CAPABILITIES, kgsl_prop_query_capabilities },
	{ KGSL_PROP_CONTEXT_PROPERTY, kgsl_get_ctxt_properties },
};

/*call all ioctl sub functions with driver locked*/
long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_device_getproperty *param = data;
	int i;

	for (i = 0; i < ARRAY_SIZE(kgsl_property_funcs); i++) {
		if (param->type == kgsl_property_funcs[i].type)
			return kgsl_property_funcs[i].func(dev_priv, param);
	}

	if (is_compat_task())
		return device->ftbl->getproperty_compat(device, param);

	return device->ftbl->getproperty(device, param);
}

int kgsl_query_property_list(struct kgsl_device *device, u32 *list, u32 count)
{
	int num = 0;

	if (!list) {
		num = ARRAY_SIZE(kgsl_property_funcs);

		if (device->ftbl->query_property_list)
			num += device->ftbl->query_property_list(device, list,
				count);

		return num;
	}

	for (; num < count && num < ARRAY_SIZE(kgsl_property_funcs); num++)
		list[num] = kgsl_property_funcs[num].type;

	if (device->ftbl->query_property_list)
		num += device->ftbl->query_property_list(device, &list[num],
			count - num);

	return num;
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
		result = kgsl_reclaim_to_pinned_state(dev_priv->process_priv);

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

		if (!(syncobj->flags & KGSL_SYNCOBJ_SW))
			syncobj->flags |= KGSL_SYNCOBJ_HW;
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
			DRAWOBJ(cmdobj)->flags &=
				~(unsigned long)KGSL_DRAWOBJ_PROFILING;

		if (type & CMDOBJ_TYPE) {
			result = kgsl_reclaim_to_pinned_state(
					dev_priv->process_priv);
			if (result)
				goto done;
		}
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

	if (type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj =
				kgsl_drawobj_sync_create(device, context);

		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);

		result = kgsl_drawobj_sync_add_synclist(device, syncobj,
				u64_to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (result)
			goto done;

		if (!(syncobj->flags & KGSL_SYNCOBJ_SW))
			syncobj->flags |= KGSL_SYNCOBJ_HW;
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
			u64_to_user_ptr(param->cmdlist),
			param->cmdsize, param->numcmds);
		if (result)
			goto done;

		result = kgsl_drawobj_cmd_add_memlist(device, cmdobj,
			u64_to_user_ptr(param->objlist),
			param->objsize, param->numobjs);
		if (result)
			goto done;

		/* If no profiling buffer was specified, clear the flag */
		if (cmdobj->profiling_buf_entry == NULL)
			DRAWOBJ(cmdobj)->flags &=
				~(unsigned long)KGSL_DRAWOBJ_PROFILING;

		if (type & CMDOBJ_TYPE) {
			result = kgsl_reclaim_to_pinned_state(
					dev_priv->process_priv);
			if (result)
				goto done;
		}
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

long kgsl_ioctl_gpu_aux_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpu_aux_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj **drawobjs;
	struct kgsl_drawobj_sync *tsobj;
	void __user *cmdlist;
	u32 queued, count;
	int i, index = 0;
	long ret;
	struct kgsl_gpu_aux_command_generic generic;

	/* We support only one aux command */
	if (param->numcmds != 1)
		return -EINVAL;

	if (!(param->flags &
		(KGSL_GPU_AUX_COMMAND_BIND | KGSL_GPU_AUX_COMMAND_TIMELINE)))
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (!context)
		return -EINVAL;

	/*
	 * param->numcmds is always one and we have one additional drawobj
	 * for the timestamp sync if KGSL_GPU_AUX_COMMAND_SYNC flag is passed.
	 * On top of that we make an implicit sync object for the last queued
	 * timestamp on this context.
	 */
	count = (param->flags & KGSL_GPU_AUX_COMMAND_SYNC) ? 3 : 2;

	drawobjs = kvcalloc(count, sizeof(*drawobjs),
		GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN);

	if (!drawobjs) {
		kgsl_context_put(context);
		return -ENOMEM;
	}

	trace_kgsl_aux_command(context->id, param->numcmds, param->flags,
		param->timestamp);

	if (param->flags & KGSL_GPU_AUX_COMMAND_SYNC) {
		struct kgsl_drawobj_sync *syncobj =
			kgsl_drawobj_sync_create(device, context);

		if (IS_ERR(syncobj)) {
			ret = PTR_ERR(syncobj);
			goto err;
		}

		drawobjs[index++] = DRAWOBJ(syncobj);

		ret = kgsl_drawobj_sync_add_synclist(device, syncobj,
				u64_to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (ret)
			goto err;
	}

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_QUEUED, &queued);

	/*
	 * Make an implicit sync object for the last queued timestamp on this
	 * context
	 */
	tsobj = kgsl_drawobj_create_timestamp_syncobj(device,
		context, queued);

	if (IS_ERR(tsobj)) {
		ret = PTR_ERR(tsobj);
		goto err;
	}

	drawobjs[index++] = DRAWOBJ(tsobj);

	cmdlist = u64_to_user_ptr(param->cmdlist);

	/*
	 * Create a draw object for KGSL_GPU_AUX_COMMAND_BIND or
	 * KGSL_GPU_AUX_COMMAND_TIMELINE.
	 */
	if (copy_struct_from_user(&generic, sizeof(generic),
		cmdlist, param->cmdsize)) {
		ret = -EFAULT;
		goto err;
	}

	if (generic.type == KGSL_GPU_AUX_COMMAND_BIND) {
		struct kgsl_drawobj_bind *bindobj;

		bindobj = kgsl_drawobj_bind_create(device, context);

		if (IS_ERR(bindobj)) {
			ret = PTR_ERR(bindobj);
			goto err;
		}

		drawobjs[index++] = DRAWOBJ(bindobj);

		ret = kgsl_drawobj_add_bind(dev_priv, bindobj,
			cmdlist, param->cmdsize);
		if (ret)
			goto err;
	} else if (generic.type == KGSL_GPU_AUX_COMMAND_TIMELINE) {
		struct kgsl_drawobj_timeline *timelineobj;

		timelineobj = kgsl_drawobj_timeline_create(device,
			context);

		if (IS_ERR(timelineobj)) {
			ret = PTR_ERR(timelineobj);
			goto err;
		}

		drawobjs[index++] = DRAWOBJ(timelineobj);

		ret = kgsl_drawobj_add_timeline(dev_priv, timelineobj,
			cmdlist, param->cmdsize);
		if (ret)
			goto err;

	} else {
		ret = -EINVAL;
		goto err;
	}

	ret = device->ftbl->queue_cmds(dev_priv, context,
		drawobjs, index, &param->timestamp);

err:
	kgsl_context_put(context);

	if (ret && ret != -EPROTO) {
		for (i = 0; i < count; i++)
			kgsl_drawobj_destroy(drawobjs[i]);
	}

	kvfree(drawobjs);
	return ret;
}

/* Returns 0 on failure.  Returns command type(s) on success */
static unsigned int _process_recurring_input(struct kgsl_device *device,
		unsigned int flags, unsigned int numcmds,
		unsigned int numobjs)
{
	if (numcmds > KGSL_MAX_NUMIBS ||
			numobjs > KGSL_MAX_NUMIBS)
		return 0;

	/* SYNC and MARKER object is not allowed through recurring command */
	if ((flags & KGSL_DRAWOBJ_MARKER) || (flags & KGSL_DRAWOBJ_SYNC))
		return 0;

	if (numcmds)
		return CMDOBJ_TYPE;

	return 0;
}

long kgsl_ioctl_recurring_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_recurring_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context = NULL;
	struct kgsl_drawobj *drawobj = NULL;
	struct kgsl_drawobj_cmd *cmdobj = NULL;
	unsigned int type;
	long result;

	if (!(param->flags & (unsigned long)(KGSL_DRAWOBJ_START_RECURRING |
			KGSL_DRAWOBJ_STOP_RECURRING)))
		return  -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	type = _process_recurring_input(device, param->flags, param->numcmds,
			param->numobjs);
	if (!type) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	cmdobj = kgsl_drawobj_cmd_create(device, context, param->flags, type);
	if (IS_ERR(cmdobj)) {
		result = PTR_ERR(cmdobj);
		goto done;
	}

	drawobj = DRAWOBJ(cmdobj);

	/* Clear the profiling flag for recurring command */
	drawobj->flags &= ~(unsigned long)KGSL_DRAWOBJ_PROFILING;

	result = kgsl_drawobj_cmd_add_cmdlist(device, cmdobj,
		u64_to_user_ptr(param->cmdlist),
		param->cmdsize, param->numcmds);
	if (result)
		goto done;

	result = kgsl_drawobj_cmd_add_memlist(device, cmdobj,
		u64_to_user_ptr(param->objlist),
		param->objsize, param->numobjs);
	if (result)
		goto done;

	if (drawobj->flags & KGSL_DRAWOBJ_STOP_RECURRING) {
		result = device->ftbl->dequeue_recurring_cmd(device, context);
		if (!result)
			kgsl_drawobj_destroy(drawobj);
	} else {
		result = device->ftbl->queue_recurring_cmd(dev_priv, context, drawobj);
	}

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		kgsl_drawobj_destroy(drawobj);

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
	param->drawctxt_id = context->id;
	write_unlock(&device->context_lock);

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

long gpumem_free_entry(struct kgsl_mem_entry *entry)
{
	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	trace_kgsl_mem_free(entry);
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

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
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

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

	if (copy_struct_from_user(&event, sizeof(event),
		u64_to_user_ptr(param->priv), param->len))
		return -EFAULT;

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

static bool gpuobj_free_fence_func(void *priv)
{
	struct kgsl_mem_entry *entry = priv;

	trace_kgsl_mem_free(entry);
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

	INIT_WORK(&entry->work, _deferred_put);
	queue_work(kgsl_driver.lockless_workqueue, &entry->work);
	return true;
}

static long gpuobj_free_on_fence(struct kgsl_device_private *dev_priv,
		struct kgsl_mem_entry *entry, struct kgsl_gpuobj_free *param)
{
	struct kgsl_sync_fence_cb *handle;
	struct kgsl_gpu_event_fence event;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	if (copy_struct_from_user(&event, sizeof(event),
		u64_to_user_ptr(param->priv), param->len)) {
		kgsl_mem_entry_unset_pend(entry);
		return -EFAULT;
	}

	if (event.fd < 0) {
		kgsl_mem_entry_unset_pend(entry);
		return -EINVAL;
	}

	handle = kgsl_sync_fence_async_wait(event.fd,
		gpuobj_free_fence_func, entry, NULL);

	if (IS_ERR(handle)) {
		kgsl_mem_entry_unset_pend(entry);
		return PTR_ERR(handle);
	}

	/* if handle is NULL the fence has already signaled */
	if (handle == NULL)
		gpuobj_free_fence_func(entry);

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

static int check_vma_flags(struct vm_area_struct *vma,
		unsigned int flags)
{
	unsigned long flags_requested = (VM_READ | VM_WRITE);

	if (flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags_requested &= ~(unsigned long)VM_WRITE;

	if ((vma->vm_flags & flags_requested) == flags_requested)
		return 0;

	return -EFAULT;
}

static int check_vma(unsigned long hostptr, u64 size)
{
	struct vm_area_struct *vma;
	unsigned long cur = hostptr;

	while (cur < (hostptr + size)) {
		vma = find_vma(current->mm, cur);
		if (!vma)
			return false;

		/* Don't remap memory that we already own */
		if (vma->vm_file && vma->vm_ops == &kgsl_gpumem_vm_ops)
			return false;

		cur = vma->vm_end;
	}

	return true;
}

static int memdesc_sg_virt(struct kgsl_memdesc *memdesc, unsigned long useraddr)
{
	int ret = 0;
	long npages = 0, i;
	size_t sglen = (size_t) (memdesc->size / PAGE_SIZE);
	struct page **pages = NULL;
	int write = ((memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 0 :
								FOLL_WRITE);

	if (sglen == 0 || sglen >= LONG_MAX)
		return -EINVAL;

	pages = kvcalloc(sglen, sizeof(*pages), GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	memdesc->sgt = kmalloc(sizeof(*memdesc->sgt), GFP_KERNEL);
	if (memdesc->sgt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	mmap_read_lock(current->mm);
	if (!check_vma(useraddr, memdesc->size)) {
		mmap_read_unlock(current->mm);
		ret = -EFAULT;
		goto out;
	}

	npages = get_user_pages(useraddr, sglen, write, pages, NULL);
	mmap_read_unlock(current->mm);

	ret = (npages < 0) ? (int)npages : 0;
	if (ret)
		goto out;

	if ((unsigned long) npages != sglen) {
		ret = -EINVAL;
		goto out;
	}

	ret = sg_alloc_table_from_pages(memdesc->sgt, pages, npages,
					0, memdesc->size, GFP_KERNEL);

	if (ret)
		goto out;

	ret = kgsl_cache_range_op(memdesc, 0, memdesc->size,
			KGSL_CACHE_OP_FLUSH);

	if (ret)
		sg_free_table(memdesc->sgt);
out:
	if (ret) {
		for (i = 0; i < npages; i++)
			put_page(pages[i]);

		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
	}
	kvfree(pages);
	return ret;
}

static const struct kgsl_memdesc_ops kgsl_usermem_ops = {
	.free = kgsl_destroy_anon,
	.put_gpuaddr = kgsl_unmap_and_put_gpuaddr,
};

static int kgsl_setup_anon_useraddr(struct kgsl_pagetable *pagetable,
	struct kgsl_mem_entry *entry, unsigned long hostptr,
	size_t offset, size_t size)
{
	/* Map an anonymous memory chunk */

	int ret;

	if (size == 0 || offset != 0 ||
		!IS_ALIGNED(size, PAGE_SIZE))
		return -EINVAL;

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = (uint64_t) size;
	entry->memdesc.flags |= (uint64_t)KGSL_MEMFLAGS_USERMEM_ADDR;
	entry->memdesc.ops = &kgsl_usermem_ops;

	if (kgsl_memdesc_use_cpu_map(&entry->memdesc)) {

		/* Register the address in the database */
		ret = kgsl_mmu_set_svm_region(pagetable,
			(uint64_t) hostptr, (uint64_t) size);

		if (ret)
			return ret;

		entry->memdesc.gpuaddr = (uint64_t) hostptr;
	}

	ret =  memdesc_sg_virt(&entry->memdesc, hostptr);

	if (ret && kgsl_memdesc_use_cpu_map(&entry->memdesc))
		kgsl_mmu_put_gpuaddr(pagetable, &entry->memdesc);

	return ret;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static void _setup_cache_mode(struct kgsl_mem_entry *entry,
		struct vm_area_struct *vma)
{
	uint64_t mode;
	pgprot_t pgprot = vma->vm_page_prot;

	if ((pgprot_val(pgprot) == pgprot_val(pgprot_noncached(pgprot))) ||
	    (pgprot_val(pgprot) == pgprot_val(pgprot_writecombine(pgprot))))
		mode = KGSL_CACHEMODE_WRITECOMBINE;
	else
		mode = KGSL_CACHEMODE_WRITEBACK;

	entry->memdesc.flags |= FIELD_PREP(KGSL_CACHEMODE_MASK, mode);
}

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
	mmap_read_lock(current->mm);
	vma = find_vma(current->mm, hostptr);

	if (vma && vma->vm_file) {
		ret = check_vma_flags(vma, entry->memdesc.flags);
		if (ret) {
			mmap_read_unlock(current->mm);
			return ret;
		}

		/*
		 * Check to see that this isn't our own memory that we have
		 * already mapped
		 */
		if (vma->vm_ops == &kgsl_gpumem_vm_ops) {
			mmap_read_unlock(current->mm);
			return -EFAULT;
		}

		if (!is_dma_buf_file(vma->vm_file)) {
			mmap_read_unlock(current->mm);
			return -ENODEV;
		}

		/* Take a refcount because dma_buf_put() decrements the refcount */
		get_file(vma->vm_file);

		dmabuf = vma->vm_file->private_data;
	}

	if (!dmabuf) {
		mmap_read_unlock(current->mm);
		return -ENODEV;
	}

	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret) {
		dma_buf_put(dmabuf);
		mmap_read_unlock(current->mm);
		return ret;
	}

	/* Setup the cache mode for cache operations */
	_setup_cache_mode(entry, vma);

	if (kgsl_mmu_has_feature(device, KGSL_MMU_IO_COHERENT) &&
	   (IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT) &&
		kgsl_cachemode_is_cached(entry->memdesc.flags)))
		entry->memdesc.flags |= KGSL_MEMFLAGS_IOCOHERENT;
	else
		entry->memdesc.flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	mmap_read_unlock(current->mm);
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

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMFLAGS_USE_CPU_MAP
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMFLAGS_FORCE_32BIT
		| KGSL_MEMFLAGS_IOCOHERENT;

	/* Specifying SECURE is an explicit error */
	if (param->flags & KGSL_MEMFLAGS_SECURE)
		return -ENOTSUPP;

	kgsl_memdesc_init(device, &entry->memdesc, param->flags);

	if (copy_from_user(&useraddr,
		u64_to_user_ptr(param->priv), sizeof(useraddr)))
		return -EINVAL;

	/* Verify that the virtaddr and len are within bounds */
	if (useraddr.virtaddr > ULONG_MAX)
		return -EINVAL;

	return kgsl_setup_useraddr(device, pagetable, entry,
		(unsigned long) useraddr.virtaddr, 0, param->priv_len);
}

static bool check_and_warn_secured(struct kgsl_device *device)
{
	if (kgsl_mmu_is_secured(&device->mmu))
		return true;

	dev_WARN_ONCE(device->dev, 1, "Secure buffers are not supported\n");
	return false;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static long _gpuobj_map_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param,
		int *fd)
{
	bool iocoherent = (param->flags & KGSL_MEMFLAGS_IOCOHERENT);
	struct kgsl_gpuobj_import_dma_buf buf;
	struct dma_buf *dmabuf;
	int ret;

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY |
		KGSL_MEMTYPE_MASK |
		KGSL_MEMALIGN_MASK |
		KGSL_MEMFLAGS_SECURE |
		KGSL_MEMFLAGS_FORCE_32BIT |
		KGSL_MEMFLAGS_GUARD_PAGE;

	kgsl_memdesc_init(device, &entry->memdesc, param->flags);

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE) {
		if (!check_and_warn_secured(device))
			return -ENOTSUPP;

		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;
	}

	if (copy_struct_from_user(&buf, sizeof(buf),
		u64_to_user_ptr(param->priv), param->priv_len))
		return -EFAULT;

	if (buf.fd < 0)
		return -EINVAL;

	*fd = buf.fd;
	dmabuf = dma_buf_get(buf.fd);

	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	/*
	 * DMA BUFS are always cached so make sure that is reflected in
	 * the memdesc.
	 */
	entry->memdesc.flags |=
		FIELD_PREP(KGSL_CACHEMODE_MASK, KGSL_CACHEMODE_WRITEBACK);

	/*
	 * Enable I/O coherency if it is 1) a thing, and either
	 * 2) enabled by default or 3) enabled by the caller
	 */
	if (kgsl_mmu_has_feature(device, KGSL_MMU_IO_COHERENT) &&
			(IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT) ||
			 iocoherent))
		entry->memdesc.flags |= KGSL_MEMFLAGS_IOCOHERENT;

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

static void kgsl_process_add_stats(struct kgsl_process_private *priv,
	unsigned int type, uint64_t size)
{
	u64 ret = atomic64_add_return(size, &priv->stats[type].cur);

	if (ret > priv->stats[type].max)
		priv->stats[type].max = ret;
}

u64 kgsl_get_stats(pid_t pid)
{
	struct kgsl_process_private *process;
	u64 ret;

	if (pid < 0)
		return atomic_long_read(&kgsl_driver.stats.page_alloc);

	process = kgsl_process_private_find(pid);

	if (!process)
		return 0;

	ret = atomic64_read(&process->stats[KGSL_MEM_ENTRY_KERNEL].cur);
	kgsl_process_private_put(process);

	return ret;
}
EXPORT_SYMBOL(kgsl_get_stats);

long kgsl_ioctl_gpuobj_import(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_gpuobj_import *param = data;
	struct kgsl_mem_entry *entry;
	int ret, fd = -1;

	if (param->type != KGSL_USER_MEM_TYPE_ADDR &&
		param->type != KGSL_USER_MEM_TYPE_DMABUF)
		return -ENOTSUPP;

	if (param->flags & KGSL_MEMFLAGS_VBO)
		return -EINVAL;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	if (param->type == KGSL_USER_MEM_TYPE_ADDR)
		ret = _gpuobj_map_useraddr(device, private->pagetable,
			entry, param);
	else
		ret = _gpuobj_map_dma_buf(device, private->pagetable,
			entry, param, &fd);

	if (ret)
		goto out;

	if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64K));

	param->flags = entry->memdesc.flags;

	ret = kgsl_mem_entry_attach_and_map(device, private, entry);
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

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;

unmap:
	kgsl_sharedmem_free(&entry->memdesc);

out:
	kfree(entry);
	return ret;
}

static long _map_usermem_addr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable, struct kgsl_mem_entry *entry,
		unsigned long hostptr, size_t offset, size_t size)
{
	if (!kgsl_mmu_has_feature(device, KGSL_MMU_PAGED))
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
		if (!check_and_warn_secured(device))
			return -EOPNOTSUPP;

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
static int verify_secure_access(struct kgsl_device *device,
	struct kgsl_mem_entry *entry, struct dma_buf *dmabuf)
{
	bool secure = entry->memdesc.priv & KGSL_MEMDESC_SECURE;
	uint32_t *vmid_list = NULL, *perms_list = NULL;
	uint32_t nelems = 0;
	int i;

	if (mem_buf_dma_buf_copy_vmperm(dmabuf, (int **)&vmid_list,
		(int **)&perms_list, (int *)&nelems)) {
		dev_info(device->dev, "Skipped access check\n");
		return 0;
	}

	/* Check if secure buffer is accessible to CP_PIXEL */
	for (i = 0; i < nelems; i++) {
		if  (vmid_list[i] == VMID_CP_PIXEL)
			break;
	}

	kfree(vmid_list);
	kfree(perms_list);

	/*
	 * Do not import a buffer if it is accessible to CP_PIXEL but is being imported as
	 * a buffer accessible to non-secure GPU. Also, make sure if buffer is to be made
	 * accessible to secure GPU, it must be accessible to CP_PIXEL
	 */
	if (!(secure ^ (i == nelems)))
		return -EPERM;

	if (secure && mem_buf_dma_buf_exclusive_owner(dmabuf))
		return -EPERM;

	return 0;
}

static int kgsl_setup_dma_buf(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable,
				struct kgsl_mem_entry *entry,
				struct dma_buf *dmabuf)
{
	int ret = 0;
	struct scatterlist *s;
	struct sg_table *sg_table = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct kgsl_dma_buf_meta *metadata;

	metadata = kzalloc(sizeof(*metadata), GFP_KERNEL);
	if (!metadata)
		return -ENOMEM;

	attach = dma_buf_attach(dmabuf, device->dev);

	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		goto out;
	}

	/*
	 * If dma buffer is marked IO coherent, skip sync at attach,
	 * which involves flushing the buffer on CPU.
	 * HW manages coherency for IO coherent buffers.
	 */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_IOCOHERENT)
		attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	metadata->dmabuf = dmabuf;
	metadata->attach = attach;
	metadata->entry = entry;

	entry->priv_data = metadata;
	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = 0;
	entry->memdesc.ops = &kgsl_dmabuf_ops;
	/* USE_CPU_MAP is not impemented for ION. */
	entry->memdesc.flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);
	entry->memdesc.flags |= (uint64_t)KGSL_MEMFLAGS_USERMEM_ION;

	sg_table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);

	if (IS_ERR_OR_NULL(sg_table)) {
		ret = PTR_ERR(sg_table);
		goto out;
	}

	metadata->table = sg_table;
	entry->priv_data = metadata;
	entry->memdesc.sgt = sg_table;

	ret = verify_secure_access(device, entry, dmabuf);
	if (ret)
		goto out;

	/* Calculate the size of the memdesc from the sglist */
	for (s = entry->memdesc.sgt->sgl; s != NULL; s = sg_next(s))
		entry->memdesc.size += (uint64_t) s->length;

	if (!entry->memdesc.size) {
		ret = -EINVAL;
		goto out;
	}

	add_dmabuf_list(metadata);
	entry->memdesc.size = PAGE_ALIGN(entry->memdesc.size);

out:
	if (ret) {
		if (!IS_ERR_OR_NULL(sg_table))
			dma_buf_unmap_attachment(attach, sg_table, DMA_BIDIRECTIONAL);

		if (!IS_ERR_OR_NULL(attach))
			dma_buf_detach(dmabuf, attach);

		kfree(metadata);
	}

	return ret;
}
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
	struct kgsl_dma_buf_meta *metadata = entry->priv_data;
	struct dmabuf_list_entry *dle = metadata->dle;
	struct kgsl_dma_buf_meta *scan_meta;
	struct kgsl_mem_entry *scan_mem_entry;

	if (!dle)
		return;

	spin_lock(&kgsl_dmabuf_lock);
	list_for_each_entry(scan_meta, &dle->dmabuf_list, node) {
		scan_mem_entry = scan_meta->entry;

		switch (kgsl_memdesc_get_memtype(&scan_mem_entry->memdesc)) {
		case KGSL_MEMTYPE_EGL_SURFACE:
			(*egl_surface_count)++;
			break;
		case KGSL_MEMTYPE_EGL_IMAGE:
			(*egl_image_count)++;
			break;
		}
	}
	spin_unlock(&kgsl_dmabuf_lock);
}

unsigned long kgsl_get_dmabuf_inode_number(struct kgsl_mem_entry *entry)
{
	struct kgsl_dma_buf_meta *metadata = entry->priv_data;

	return metadata ? file_inode(metadata->dmabuf->file)->i_ino : 0;
}
#else
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
}

unsigned long kgsl_get_dmabuf_inode_number(struct kgsl_mem_entry *entry)
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
	struct kgsl_device *device = dev_priv->device;
	unsigned int memtype;
	uint64_t flags;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */

	if (param->flags & KGSL_MEMFLAGS_SECURE) {
		if (!check_and_warn_secured(device))
			return -EOPNOTSUPP;

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
	flags = param->flags & (KGSL_MEMFLAGS_GPUREADONLY
				| KGSL_MEMTYPE_MASK
				| KGSL_MEMALIGN_MASK
				| KGSL_MEMFLAGS_USE_CPU_MAP
				| KGSL_MEMFLAGS_SECURE
				| KGSL_MEMFLAGS_IOCOHERENT);

	if (is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	kgsl_memdesc_init(device, &entry->memdesc, flags);

	switch (memtype) {
	case KGSL_MEM_ENTRY_USER:
		result = _map_usermem_addr(device, private->pagetable,
			entry, param->hostptr, param->offset, param->len);
		break;
	case KGSL_MEM_ENTRY_ION:
		if (param->offset != 0)
			result = -EINVAL;
		else
			result = _map_usermem_dma_buf(device,
				private->pagetable, entry, param->fd);
		break;
	default:
		result = -EOPNOTSUPP;
		break;
	}

	if (result)
		goto error;

	if (entry->memdesc.size >= SZ_2M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_2M));
	else if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64));

	/* echo back flags */
	param->flags = (unsigned int) entry->memdesc.flags;

	result = kgsl_mem_entry_attach_and_map(device, private,
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

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return result;

error_attach:
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

	if (!entry)
		return 0;

	 /* Cache ops are not allowed on secure memory */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE)
		return 0;

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

	if (kgsl_cachemode_is_cached(entry->memdesc.flags)) {
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
	bool ret = (kgsl_driver.full_cache_threshold != 0) &&
		(size >= kgsl_driver.full_cache_threshold) &&
		(op == KGSL_GPUMEM_CACHE_FLUSH);
	if (ret)
		flush_cache_all();
	return ret;
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

	id_list = kcalloc(param->count, sizeof(unsigned int), GFP_KERNEL);
	if (id_list == NULL)
		return -ENOMEM;

	entries = kcalloc(param->count, sizeof(*entries), GFP_KERNEL);
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
		if (full_flush) {
			trace_kgsl_mem_sync_full_cache(actual_count, op_size);
			break;
		}

		last_id = id;
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
	uint64_t size = 0;
	int i;
	void __user *ptr;

	if (param->count == 0 || param->count > 128)
		return -EINVAL;

	objs = kcalloc(param->count, sizeof(*objs), GFP_KERNEL);
	if (objs == NULL)
		return -ENOMEM;

	entries = kcalloc(param->count, sizeof(*entries), GFP_KERNEL);
	if (entries == NULL) {
		kfree(objs);
		return -ENOMEM;
	}

	ptr = u64_to_user_ptr(param->objs);

	for (i = 0; i < param->count; i++) {
		ret = copy_struct_from_user(&objs[i], sizeof(*objs), ptr,
			param->obj_len);
		if (ret)
			goto out;

		entries[i] = kgsl_sharedmem_find_id(private, objs[i].id);

		/* Not finding the ID is not a fatal failure - just skip it */
		if (entries[i] == NULL)
			continue;

		if (!(objs[i].op & KGSL_GPUMEM_CACHE_RANGE))
			size += entries[i]->memdesc.size;
		else if (objs[i].offset < entries[i]->memdesc.size)
			size += (entries[i]->memdesc.size - objs[i].offset);

		if (check_full_flush(size, objs[i].op)) {
			trace_kgsl_mem_sync_full_cache(i, size);
			goto out;
		}

		ptr += sizeof(*objs);
	}

	for (i = 0; !ret && i < param->count; i++)
		ret = _kgsl_gpumem_sync_cache(entries[i],
			objs[i].offset, objs[i].length, objs[i].op);

out:
	for (i = 0; i < param->count; i++)
		kgsl_mem_entry_put(entries[i]);

	kfree(entries);
	kfree(objs);

	return ret;
}

static int kgsl_update_fault_details(struct kgsl_context *context,
		void __user *ptr, u32 faultnents, u32 faultsize)
{
	u32 size = min_t(u32, sizeof(struct kgsl_fault), faultsize);
	u32 cur_idx[KGSL_FAULT_TYPE_MAX] = {0};
	struct kgsl_fault_node *fault_node;
	struct kgsl_fault *faults;
	int i, ret = 0;

	faults = kcalloc(KGSL_FAULT_TYPE_MAX, sizeof(struct kgsl_fault),
			GFP_KERNEL);
	if (!faults)
		return -ENOMEM;

	for (i = 0; i < faultnents; i++) {
		struct kgsl_fault fault = {0};

		if (copy_from_user(&fault, ptr + i * faultsize, size)) {
			ret = -EFAULT;
			goto err;
		}

		if (fault.type >= KGSL_FAULT_TYPE_MAX) {
			ret = -EINVAL;
			goto err;
		}

		memcpy(&faults[fault.type], &fault, sizeof(fault));
	}

	list_for_each_entry(fault_node, &context->faults, node) {
		u32 fault_type = fault_node->type;

		if (cur_idx[fault_type] >= faults[fault_type].count)
			continue;

		switch (fault_type) {
		case KGSL_FAULT_TYPE_PAGEFAULT:
			size = sizeof(struct kgsl_pagefault_report);
		}

		size = min_t(u32, size, faults[fault_type].size);

		if (copy_to_user(u64_to_user_ptr(faults[fault_type].fault +
			cur_idx[fault_type] * faults[fault_type].size),
			fault_node->priv, size)) {
			ret = -EFAULT;
			goto err;
		}

		cur_idx[fault_type] += 1;
	}

err:
	kfree(faults);
	return ret;
}

static int kgsl_update_fault_count(struct kgsl_context *context,
		void __user *faults, u32 faultnents, u32 faultsize)
{
	u32 size = min_t(u32, sizeof(struct kgsl_fault), faultsize);
	u32 faultcount[KGSL_FAULT_TYPE_MAX] = {0};
	struct kgsl_fault_node *fault_node;
	int i, j;

	list_for_each_entry(fault_node, &context->faults, node)
		faultcount[fault_node->type]++;

	/* KGSL_FAULT_TYPE_NO_FAULT (i.e. 0) is not an actual fault type */
	for (i = 0, j = 1; i < faultnents && j < KGSL_FAULT_TYPE_MAX; j++) {
		struct kgsl_fault fault = {0};

		if (!faultcount[j])
			continue;

		fault.type = j;
		fault.count = faultcount[j];

		if (copy_to_user(faults, &fault, size))
			return -EFAULT;

		faults += faultsize;
		i++;
	}

	return 0;
}

long kgsl_ioctl_get_fault_report(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_fault_report *param = data;
	u32 size = min_t(u32, sizeof(struct kgsl_fault), param->faultsize);
	void __user *ptr = u64_to_user_ptr(param->faultlist);
	struct kgsl_context *context;
	int i, ret = 0;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (!context)
		return -EINVAL;

	/* This IOCTL is valid for invalidated contexts only */
	if (!(context->flags & KGSL_CONTEXT_FAULT_INFO) ||
		!kgsl_context_invalid(context)) {
		ret = -EINVAL;
		goto err;
	}

	/* Return the number of fault types */
	if (!param->faultlist) {
		param->faultnents = KGSL_FAULT_TYPE_MAX;
		kgsl_context_put(context);
		return 0;
	}

	/* Check if it's a request to get fault counts or to fill the fault information */
	for (i = 0; i < param->faultnents; i++) {
		struct kgsl_fault fault = {0};

		if (copy_from_user(&fault, ptr, size)) {
			ret = -EFAULT;
			goto err;
		}

		if (fault.fault)
			break;

		ptr += param->faultsize;
	}

	ptr = u64_to_user_ptr(param->faultlist);

	if (i == param->faultnents)
		ret = kgsl_update_fault_count(context, ptr, param->faultnents,
			param->faultsize);
	else
		ret = kgsl_update_fault_details(context, ptr, param->faultnents,
			param->faultsize);

err:
	kgsl_context_put(context);
	return ret;
}

int kgsl_add_fault(struct kgsl_context *context, u32 type, void *priv)
{
	struct kgsl_fault_node *fault, *p, *tmp;
	int length = 0;
	ktime_t tout;

	if (kgsl_context_is_bad(context))
		return -EINVAL;

	fault = kmalloc(sizeof(struct kgsl_fault_node), GFP_KERNEL);
	if (!fault)
		return -ENOMEM;

	fault->type = type;
	fault->priv = priv;
	fault->time = ktime_get();

	tout = ktime_sub_ms(ktime_get(), KGSL_MAX_FAULT_TIME_THRESHOLD);

	mutex_lock(&context->fault_lock);

	list_for_each_entry_safe(p, tmp, &context->faults, node) {
		if (ktime_compare(p->time, tout) > 0) {
			length++;
			continue;
		}

		list_del(&p->node);
		kfree(p->priv);
		kfree(p);
	}

	if (length == KGSL_MAX_FAULT_ENTRIES) {
		tmp = list_first_entry(&context->faults, struct kgsl_fault_node, node);
		list_del(&tmp->node);
		kfree(tmp->priv);
		kfree(tmp);
	}

	list_add_tail(&fault->node, &context->faults);
	mutex_unlock(&context->fault_lock);

	return 0;
}

#ifdef CONFIG_ARM64
static uint64_t kgsl_filter_cachemode(uint64_t flags)
{
	/*
	 * WRITETHROUGH is not supported in arm64, so we tell the user that we
	 * use WRITEBACK which is the default caching policy.
	 */
	if (FIELD_GET(KGSL_CACHEMODE_MASK, flags) == KGSL_CACHEMODE_WRITETHROUGH) {
		flags &= ~((uint64_t) KGSL_CACHEMODE_MASK);
		flags |= FIELD_PREP(KGSL_CACHEMODE_MASK, KGSL_CACHEMODE_WRITEBACK);
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

static u64 cap_alignment(struct kgsl_device *device, u64 flags)
{
	u32 align = FIELD_GET(KGSL_MEMALIGN_MASK, flags);

	if (align >= ilog2(KGSL_MAX_ALIGN)) {
	/* Cap the alignment bits to the highest number we can handle */
		dev_err(device->dev,
			"Alignment too large; restricting to %dK\n",
			KGSL_MAX_ALIGN >> 10);
		align = ilog2(KGSL_MAX_ALIGN);
	}

	flags &= ~((u64) KGSL_MEMALIGN_MASK);
	return flags | FIELD_PREP(KGSL_MEMALIGN_MASK, align);
}

static struct kgsl_mem_entry *
gpumem_alloc_vbo_entry(struct kgsl_device_private *dev_priv,
		u64 size, u64 flags)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_memdesc *memdesc;
	struct kgsl_mem_entry *entry;
	int ret;

	/* Disallow specific flags */
	if (flags & (KGSL_MEMFLAGS_GPUREADONLY | KGSL_CACHEMODE_MASK))
		return ERR_PTR(-EINVAL);

	if (flags & (KGSL_MEMFLAGS_USE_CPU_MAP | KGSL_MEMFLAGS_IOCOHERENT))
		return ERR_PTR(-EINVAL);

	/* Quietly ignore the other flags that aren't this list */
	flags &= KGSL_MEMFLAGS_SECURE |
		KGSL_MEMFLAGS_VBO    |
		KGSL_MEMTYPE_MASK   |
		KGSL_MEMALIGN_MASK  |
		KGSL_MEMFLAGS_FORCE_32BIT;

	if ((flags & KGSL_MEMFLAGS_SECURE) && !check_and_warn_secured(device))
		return ERR_PTR(-EOPNOTSUPP);

	flags = cap_alignment(device, flags);

	entry = kgsl_mem_entry_create();
	if (!entry)
		return ERR_PTR(-ENOMEM);

	memdesc = &entry->memdesc;

	ret = kgsl_sharedmem_allocate_vbo(device, memdesc, size, flags);
	if (ret) {
		kfree(entry);
		return ERR_PTR(ret);
	}

	if (flags & KGSL_MEMFLAGS_SECURE)
		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;

	ret = kgsl_mem_entry_attach_to_process(device, private, entry);
	if (ret)
		goto out;

	ret = kgsl_mmu_map_zero_page_to_range(memdesc->pagetable,
		memdesc, 0, memdesc->size);
	if (!ret) {
		trace_kgsl_mem_alloc(entry);
		kgsl_mem_entry_commit_process(entry);
		return entry;
	}

out:
	kgsl_sharedmem_free(memdesc);
	kfree(entry);
	return ERR_PTR(ret);
}

struct kgsl_mem_entry *gpumem_alloc_entry(
		struct kgsl_device_private *dev_priv,
		uint64_t size, uint64_t flags)
{
	int ret;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	struct kgsl_device *device = dev_priv->device;
	u32 cachemode;

	/* For 32-bit kernel world nothing to do with this flag */
	if (BITS_PER_LONG == 32)
		flags &= ~((uint64_t) KGSL_MEMFLAGS_FORCE_32BIT);

	if (flags & KGSL_MEMFLAGS_VBO)
		return gpumem_alloc_vbo_entry(dev_priv, size, flags);

	flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMALIGN_MASK
		| KGSL_MEMFLAGS_USE_CPU_MAP
		| KGSL_MEMFLAGS_SECURE
		| KGSL_MEMFLAGS_FORCE_32BIT
		| KGSL_MEMFLAGS_IOCOHERENT
		| KGSL_MEMFLAGS_GUARD_PAGE;

	/* Return not supported error if secure memory isn't enabled */
	if ((flags & KGSL_MEMFLAGS_SECURE) && !check_and_warn_secured(device))
		return ERR_PTR(-EOPNOTSUPP);

	flags = cap_alignment(device, flags);

	/* For now only allow allocations up to 4G */
	if (size == 0 || size > UINT_MAX)
		return ERR_PTR(-EINVAL);

	flags = kgsl_filter_cachemode(flags);

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return ERR_PTR(-ENOMEM);

	if (IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT) &&
		kgsl_cachemode_is_cached(flags))
		flags |= KGSL_MEMFLAGS_IOCOHERENT;

	ret = kgsl_allocate_user(device, &entry->memdesc,
		size, flags, 0);
	if (ret != 0)
		goto err;

	ret = kgsl_mem_entry_attach_and_map(device, private, entry);
	if (ret != 0) {
		kgsl_sharedmem_free(&entry->memdesc);
		goto err;
	}

	cachemode = kgsl_memdesc_get_cachemode(&entry->memdesc);
	/*
	 * Secure buffers cannot be reclaimed. For IO-COHERENT devices cached
	 * buffers can safely reclaimed. But avoid reclaim cached buffers of
	 * non IO-COHERENT devices as we could get request for cache operations
	 * on these buffers when they are reclaimed.
	 */
	if (!(flags & KGSL_MEMFLAGS_SECURE) &&
			(((flags & KGSL_MEMFLAGS_IOCOHERENT) &&
			!(cachemode == KGSL_CACHEMODE_WRITETHROUGH)) ||
			(!(flags & KGSL_MEMFLAGS_IOCOHERENT) &&
			 !(cachemode == KGSL_CACHEMODE_WRITEBACK) &&
			!(cachemode == KGSL_CACHEMODE_WRITETHROUGH))))
		entry->memdesc.priv |= KGSL_MEMDESC_CAN_RECLAIM;

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

	if (copy_from_user(entry->metadata, u64_to_user_ptr(metadata), size)) {
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

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

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

	if (is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;
	param->size = (size_t) entry->memdesc.size;
	param->flags = (unsigned int) entry->memdesc.flags;

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_gpumem_alloc_id(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_gpumem_alloc_id *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags = param->flags;

	if (is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->id = entry->id;
	param->flags = (unsigned int) entry->memdesc.flags;
	param->size = (size_t) entry->memdesc.size;
	param->mmapsize = (size_t) kgsl_memdesc_footprint(&entry->memdesc);
	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

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
	/*
	 * Entries can have multiple user mappings so thre isn't any one address
	 * we can report. Plus, the user should already know their mappings, so
	 * there isn't any value in reporting it back to them.
	 */
	param->useraddr = 0;

	kgsl_mem_entry_put(entry);
	return result;
}

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

	/* VBOs cannot be mapped, so don't report a va_len */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_VBO)
		param->va_len = 0;
	else
		param->va_len = kgsl_memdesc_footprint(&entry->memdesc);

	/*
	 * Entries can have multiple user mappings so thre isn't any one address
	 * we can report. Plus, the user should already know their mappings, so
	 * there isn't any value in reporting it back to them.
	 */
	param->va_addr = 0;

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_gpuobj_set_info(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_set_info *param = data;
	struct kgsl_mem_entry *entry;
	int ret = 0;

	if (param->id == 0)
		return -EINVAL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	if (param->flags & KGSL_GPUOBJ_SET_INFO_METADATA)
		copy_metadata(entry, param->metadata, param->metadata_len);

	if (param->flags & KGSL_GPUOBJ_SET_INFO_TYPE) {
		if (FIELD_FIT(KGSL_MEMTYPE_MASK, param->type)) {
			entry->memdesc.flags &= ~((uint64_t) KGSL_MEMTYPE_MASK);
			entry->memdesc.flags |=
				FIELD_PREP(KGSL_MEMTYPE_MASK, param->type);
		} else
			ret = -EINVAL;
	}

	kgsl_mem_entry_put(entry);
	return ret;
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

static vm_fault_t
kgsl_memstore_vm_fault(struct vm_fault *vmf)
{
	struct kgsl_memdesc *memdesc = vmf->vma->vm_private_data;

	return memdesc->ops->vmfault(memdesc, vmf->vma, vmf);
}

static const struct vm_operations_struct kgsl_memstore_vm_ops = {
	.fault = kgsl_memstore_vm_fault,
};

static int
kgsl_mmap_memstore(struct file *file, struct kgsl_device *device,
		struct vm_area_struct *vma)
{
	struct kgsl_memdesc *memdesc = device->memstore;
	unsigned int vma_size = vma->vm_end - vma->vm_start;

	/* The memstore can only be mapped as read only */

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	vma->vm_flags &= ~VM_MAYWRITE;

	if (memdesc->size  != vma_size) {
		dev_err(device->dev, "Cannot partially map the memstore\n");
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_private_data = memdesc;
	vma->vm_flags |= memdesc->ops->vmflags;
	vma->vm_ops = &kgsl_memstore_vm_ops;
	vma->vm_file = file;

	return 0;
}

/*
 * kgsl_gpumem_vm_open is called whenever a vma region is copied or split.
 * Increase the refcount to make sure that the accounting stays correct
 */

static void kgsl_gpumem_vm_open(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (!kgsl_mem_entry_get(entry))
		vma->vm_private_data = NULL;

	atomic_inc(&entry->map_count);
}

static vm_fault_t
kgsl_gpumem_vm_fault(struct vm_fault *vmf)
{
	struct kgsl_mem_entry *entry = vmf->vma->vm_private_data;

	if (!entry)
		return VM_FAULT_SIGBUS;
	if (!entry->memdesc.ops || !entry->memdesc.ops->vmfault)
		return VM_FAULT_SIGBUS;

	return entry->memdesc.ops->vmfault(&entry->memdesc, vmf->vma, vmf);
}

static void
kgsl_gpumem_vm_close(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry  = vma->vm_private_data;

	if (!entry)
		return;

	/*
	 * Remove the memdesc from the mapped stat once all the mappings have
	 * gone away
	 */
	if (!atomic_dec_return(&entry->map_count))
		atomic64_sub(entry->memdesc.size, &entry->priv->gpumem_mapped);

	kgsl_mem_entry_put(entry);
}

static const struct vm_operations_struct kgsl_gpumem_vm_ops = {
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

	/* Don't allow ourselves to remap user memory */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_USERMEM_ADDR) {
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

	/*
	 * Protect access to the gpuaddr here to prevent multiple vmas from
	 * trying to map a SVM region at the same time
	 */
	spin_lock(&entry->memdesc.lock);

	if (entry->memdesc.gpuaddr) {
		spin_unlock(&entry->memdesc.lock);
		return (unsigned long) -EBUSY;
	}

	ret = kgsl_mmu_set_svm_region(private->pagetable, (uint64_t) addr,
		(uint64_t) size);

	if (ret != 0) {
		spin_unlock(&entry->memdesc.lock);
		return (unsigned long) ret;
	}

	entry->memdesc.gpuaddr = (uint64_t) addr;
	spin_unlock(&entry->memdesc.lock);

	entry->memdesc.pagetable = private->pagetable;

	ret = kgsl_mmu_map(private->pagetable, &entry->memdesc);
	if (ret) {
		kgsl_mmu_put_gpuaddr(private->pagetable, &entry->memdesc);
		return (unsigned long) ret;
	}

	kgsl_memfree_purge(private->pagetable, entry->memdesc.gpuaddr,
		entry->memdesc.size);

	return addr;
}

static unsigned long get_align(struct kgsl_mem_entry *entry)
{
	int bit = kgsl_memdesc_get_align(&entry->memdesc);

	if (bit >= ilog2(SZ_2M))
		return SZ_2M;
	else if (bit >= ilog2(SZ_1M))
		return SZ_1M;
	else if (bit >= ilog2(SZ_64K))
		return SZ_64K;

	return SZ_4K;
}

static unsigned long set_svm_area(struct file *file,
		struct kgsl_mem_entry *entry,
		unsigned long addr, unsigned long len,
		unsigned long flags)
{
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	unsigned long ret;

	/*
	 * Do additoinal constraints checking on the address. Passing MAP_FIXED
	 * ensures that the address we want gets checked
	 */
	ret = current->mm->get_unmapped_area(file, addr, len, 0,
		flags & MAP_FIXED);

	/* If it passes, attempt to set the region in the SVM */
	if (!IS_ERR_VALUE(ret))
		return _gpu_set_svm_region(private, entry, addr, len);

	return ret;
}

static unsigned long get_svm_unmapped_area(struct file *file,
		struct kgsl_mem_entry *entry,
		unsigned long addr, unsigned long len,
		unsigned long flags)
{
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	unsigned long align = get_align(entry);
	unsigned long ret, iova;
	u64 start = 0, end = 0;
	struct vm_area_struct *vma;

	if (flags & MAP_FIXED) {
		/* Even fixed addresses need to obey alignment */
		if (!IS_ALIGNED(addr, align))
			return -EINVAL;

		return set_svm_area(file, entry, addr, len, flags);
	}

	/* If a hint was provided, try to use that first */
	if (addr) {
		if (IS_ALIGNED(addr, align)) {
			ret = set_svm_area(file, entry, addr, len, flags);
			if (!IS_ERR_VALUE(ret))
				return ret;
		}
	}

	/* Get the SVM range for the current process */
	if (kgsl_mmu_svm_range(private->pagetable, &start, &end,
		entry->memdesc.flags))
		return -ERANGE;

	/* Find the first gap in the iova map */
	iova = kgsl_mmu_find_svm_region(private->pagetable, start, end,
		len, align);

	while (!IS_ERR_VALUE(iova)) {
		vma = find_vma_intersection(current->mm, iova, iova + len - 1);
		if (vma) {
			iova = vma->vm_start;
		} else {
			ret = set_svm_area(file, entry, iova, len, flags);
			if (!IS_ERR_VALUE(ret))
				return ret;

			/*
			 * set_svm_area will return -EBUSY if we tried to set up
			 * SVM on an object that already has a GPU address. If
			 * that happens don't bother walking the rest of the
			 * region
			 */
			if ((long) ret == -EBUSY)
				return -EBUSY;

		}

		iova = kgsl_mmu_find_svm_region(private->pagetable,
			start, iova - 1, len, align);
	}

	return -ENOMEM;
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

	if (vma_offset == (unsigned long) KGSL_MEMSTORE_TOKEN_ADDRESS)
		return get_unmapped_area(NULL, addr, len, pgoff, flags);

	val = get_mmap_entry(private, &entry, pgoff, len);
	if (val)
		return val;

	/* Do not allow CPU mappings for secure buffers */
	if (kgsl_memdesc_is_secured(&entry->memdesc)) {
		kgsl_mem_entry_put(entry);
		return (unsigned long) -EPERM;
	}

	if (!kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		val = current->mm->get_unmapped_area(file, addr, len, 0, flags);
		if (IS_ERR_VALUE(val))
			dev_err_ratelimited(device->dev,
					       "get_unmapped_area: pid %d addr %lx pgoff %lx len %ld failed error %d\n",
					       pid_nr(private->pid), addr, pgoff, len,
					       (int) val);
	} else {
		val = get_svm_unmapped_area(file, entry, addr, len, flags);
		if (IS_ERR_VALUE(val))
			dev_err_ratelimited(device->dev,
					       "_get_svm_area: pid %d addr %lx pgoff %lx len %ld failed error %d\n",
					       pid_nr(private->pid), addr, pgoff, len,
					       (int) val);
	}

	kgsl_mem_entry_put(entry);
	return val;
}

static int kgsl_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int cache;
	unsigned long vma_offset = vma->vm_pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_device *device = dev_priv->device;
	uint64_t flags;
	int ret;

	/* Handle leagacy behavior for memstore */

	if (vma_offset == (unsigned long) KGSL_MEMSTORE_TOKEN_ADDRESS)
		return kgsl_mmap_memstore(file, device, vma);

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
	case KGSL_CACHEMODE_WRITETHROUGH:
		vma->vm_page_prot = pgprot_writethroughcache(vma->vm_page_prot);
		if (pgprot_val(vma->vm_page_prot) ==
			pgprot_val(pgprot_writebackcache(vma->vm_page_prot)))
			WARN_ONCE(1, "WRITETHROUGH is deprecated for arm64");
		break;
	case KGSL_CACHEMODE_WRITEBACK:
		vma->vm_page_prot = pgprot_writebackcache(vma->vm_page_prot);
		break;
	case KGSL_CACHEMODE_UNCACHED:
	case KGSL_CACHEMODE_WRITECOMBINE:
	default:
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		break;
	}

	vma->vm_ops = &kgsl_gpumem_vm_ops;

	flags = entry->memdesc.flags;

	if (!(flags & KGSL_MEMFLAGS_IOCOHERENT) &&
	    (cache == KGSL_CACHEMODE_WRITEBACK ||
	     cache == KGSL_CACHEMODE_WRITETHROUGH)) {
		int i;
		unsigned long addr = vma->vm_start;
		struct kgsl_memdesc *m = &entry->memdesc;

		for (i = 0; i < m->page_count; i++) {
			struct page *page = m->pages[i];

			vm_insert_page(vma, addr, page);
			addr += PAGE_SIZE;
		}
	}

	if (entry->memdesc.shmem_filp) {
		fput(vma->vm_file);
		vma->vm_file = get_file(entry->memdesc.shmem_filp);
	}

	/*
	 * kgsl gets the entry id or the gpu address through vm_pgoff.
	 * It is used during mmap and never needed again. But this vm_pgoff
	 * has different meaning at other parts of kernel. Not setting to
	 * zero will let way for wrong assumption when tried to unmap a page
	 * from this vma.
	 */
	vma->vm_pgoff = 0;

	if (atomic_inc_return(&entry->map_count) == 1)
		atomic64_add(entry->memdesc.size, &entry->priv->gpumem_mapped);

	trace_kgsl_mem_mmap(entry, vma->vm_start);
	return 0;
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
	.proclist_lock = __RW_LOCK_UNLOCKED(kgsl_driver.proclist_lock),
	.ptlock = __SPIN_LOCK_UNLOCKED(kgsl_driver.ptlock),
	.wp_list_lock = __SPIN_LOCK_UNLOCKED(kgsl_driver.wp_list_lock),
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

static void _unregister_device(struct kgsl_device *device)
{
	int minor;

	if (device->gpu_sysfs_kobj.state_initialized)
		kobject_put(&device->gpu_sysfs_kobj);

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < ARRAY_SIZE(kgsl_driver.devp); minor++) {
		if (device == kgsl_driver.devp[minor]) {
			device_destroy(kgsl_driver.class,
				MKDEV(MAJOR(kgsl_driver.major), minor));
			kgsl_driver.devp[minor] = NULL;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.devlock);
}

/* sysfs_ops for the /sys/kernel/gpu kobject */
static ssize_t kgsl_gpu_sysfs_attr_show(struct kobject *kobj,
		struct attribute *__attr, char *buf)
{
	struct kgsl_gpu_sysfs_attr *attr = container_of(__attr,
		struct kgsl_gpu_sysfs_attr, attr);
	struct kgsl_device *device = container_of(kobj,
			struct kgsl_device, gpu_sysfs_kobj);

	if (attr->show)
		return attr->show(device, buf);

	return -EIO;
}

static ssize_t kgsl_gpu_sysfs_attr_store(struct kobject *kobj,
		struct attribute *__attr, const char *buf, size_t count)
{
	struct kgsl_gpu_sysfs_attr *attr = container_of(__attr,
		struct kgsl_gpu_sysfs_attr, attr);
	struct kgsl_device *device = container_of(kobj,
			struct kgsl_device, gpu_sysfs_kobj);

	if (attr->store)
		return attr->store(device, buf, count);

	return -EIO;
}

/* Dummy release function - we have nothing to do here */
static void kgsl_gpu_sysfs_release(struct kobject *kobj)
{
}

static const struct sysfs_ops kgsl_gpu_sysfs_ops = {
	.show = kgsl_gpu_sysfs_attr_show,
	.store = kgsl_gpu_sysfs_attr_store,
};

static struct kobj_type kgsl_gpu_sysfs_ktype = {
	.sysfs_ops = &kgsl_gpu_sysfs_ops,
	.release = kgsl_gpu_sysfs_release,
};

static int _register_device(struct kgsl_device *device)
{
	static u64 dma_mask = DMA_BIT_MASK(64);
	static struct device_dma_parameters dma_parms;
	int minor, ret;
	dev_t dev;

	/* Find a minor for the device */

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < ARRAY_SIZE(kgsl_driver.devp); minor++) {
		if (kgsl_driver.devp[minor] == NULL) {
			kgsl_driver.devp[minor] = device;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.devlock);

	if (minor == ARRAY_SIZE(kgsl_driver.devp)) {
		pr_err("kgsl: minor devices exhausted\n");
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
		pr_err("kgsl: device_create(%s): %d\n", device->name, ret);
		return ret;
	}

	device->dev->dma_mask = &dma_mask;
	device->dev->dma_parms = &dma_parms;

	dma_set_max_seg_size(device->dev, DMA_BIT_MASK(32));

	set_dma_ops(device->dev, NULL);

	WARN_ON(kobject_init_and_add(&device->gpu_sysfs_kobj, &kgsl_gpu_sysfs_ktype,
		kernel_kobj, "gpu"));

	return 0;
}

int kgsl_request_irq(struct platform_device *pdev, const  char *name,
		irq_handler_t handler, void *data)
{
	int ret, num = platform_get_irq_byname(pdev, name);

	if (num < 0)
		return num;

	ret = devm_request_irq(&pdev->dev, num, handler, IRQF_TRIGGER_HIGH,
		name, data);

	if (ret) {
		dev_err(&pdev->dev, "Unable to get interrupt %s: %d\n",
			name, ret);
		return ret;
	}

	disable_irq(num);
	return num;
}

int kgsl_of_property_read_ddrtype(struct device_node *node, const char *base,
		u32 *ptr)
{
	char str[32];
	int ddr = of_fdt_get_ddrtype();

	/* of_fdt_get_ddrtype returns error if the DDR type isn't determined */
	if (ddr >= 0) {
		int ret;

		/* Construct expanded string for the DDR type  */
		ret = snprintf(str, sizeof(str), "%s-ddr%d", base, ddr);

		/* WARN_ON() if the array size was too small for the string */
		if (WARN_ON(ret > sizeof(str)))
			return -ENOMEM;

		/* Read the expanded string */
		if (!of_property_read_u32(node, str, ptr))
			return 0;
	}

	/* Read the default string */
	return of_property_read_u32(node, base, ptr);
}

int kgsl_device_platform_probe(struct kgsl_device *device)
{
	struct platform_device *pdev = device->pdev;
	int status = -EINVAL;

	status = _register_device(device);
	if (status)
		return status;

	/* Can return -EPROBE_DEFER */
	status = kgsl_pwrctrl_init(device);
	if (status)
		goto error;

	device->events_wq = alloc_workqueue("kgsl-events",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS | WQ_HIGHPRI, 0);

	if (!device->events_wq) {
		dev_err(device->dev, "Failed to allocate events workqueue\n");
		status = -ENOMEM;
		goto error_pwrctrl_close;
	}

	status = kgsl_reclaim_init();
	if (status)
		goto error_pwrctrl_close;

	rwlock_init(&device->context_lock);
	spin_lock_init(&device->submit_lock);

	idr_init(&device->timelines);
	spin_lock_init(&device->timelines_lock);

	kgsl_device_debugfs_init(device);

	dma_set_coherent_mask(&pdev->dev, KGSL_DMA_BIT_MASK);

	/* Set up the GPU events for the device */
	kgsl_device_events_probe(device);

	/* Initialize common sysfs entries */
	kgsl_pwrctrl_init_sysfs(device);

	timer_setup(&device->work_period_timer, kgsl_work_period_timer, 0);
	spin_lock_init(&device->work_period_lock);
	INIT_WORK(&device->work_period_ws, _log_gpu_work_events);

	return 0;

error_pwrctrl_close:
	if (device->events_wq) {
		destroy_workqueue(device->events_wq);
		device->events_wq = NULL;
	}

	kgsl_pwrctrl_close(device);
error:
	_unregister_device(device);
	return status;
}

void kgsl_device_platform_remove(struct kgsl_device *device)
{
	del_timer(&device->work_period_timer);

	if (device->events_wq) {
		destroy_workqueue(device->events_wq);
		device->events_wq = NULL;
	}

	kgsl_device_snapshot_close(device);

	idr_destroy(&device->context_idr);
	idr_destroy(&device->timelines);

	kgsl_device_events_remove(device);

	kgsl_free_globals(device);

	kgsl_pwrctrl_close(device);

	kgsl_device_debugfs_close(device);
	_unregister_device(device);
}

void kgsl_core_exit(void)
{
	kgsl_exit_page_pools();
	kgsl_eventlog_exit();

	if (kgsl_driver.workqueue) {
		destroy_workqueue(kgsl_driver.workqueue);
		kgsl_driver.workqueue = NULL;
	}

	if (kgsl_driver.lockless_workqueue) {
		destroy_workqueue(kgsl_driver.lockless_workqueue);
		kgsl_driver.lockless_workqueue = NULL;
	}

	kgsl_events_exit();
	kgsl_core_debugfs_close();

	kgsl_reclaim_close();

	/*
	 * We call device_unregister()
	 * only if kgsl_driver.virtdev has been populated.
	 * We check at least one member of kgsl_driver.virtdev to
	 * see if it is not NULL (and thus, has been populated).
	 */
	if (kgsl_driver.virtdev.class)
		device_unregister(&kgsl_driver.virtdev);

	if (kgsl_driver.class) {
		class_destroy(kgsl_driver.class);
		kgsl_driver.class = NULL;
	}

	kgsl_drawobjs_cache_exit();

	kfree(memfree.list);
	memset(&memfree, 0, sizeof(memfree));

	unregister_chrdev_region(kgsl_driver.major,
		ARRAY_SIZE(kgsl_driver.devp));
}

int __init kgsl_core_init(void)
{
	int result = 0;

	/* alloc major and minor device numbers */
	result = alloc_chrdev_region(&kgsl_driver.major, 0,
		ARRAY_SIZE(kgsl_driver.devp), "kgsl");

	if (result < 0) {

		pr_err("kgsl: alloc_chrdev_region failed err = %d\n", result);
		goto err;
	}

	cdev_init(&kgsl_driver.cdev, &kgsl_fops);
	kgsl_driver.cdev.owner = THIS_MODULE;
	kgsl_driver.cdev.ops = &kgsl_fops;
	result = cdev_add(&kgsl_driver.cdev, MKDEV(MAJOR(kgsl_driver.major), 0),
		ARRAY_SIZE(kgsl_driver.devp));

	if (result) {
		pr_err("kgsl: cdev_add() failed, dev_num= %d,result= %d\n",
				kgsl_driver.major, result);
		goto err;
	}

	kgsl_driver.class = class_create(THIS_MODULE, "kgsl");

	if (IS_ERR(kgsl_driver.class)) {
		result = PTR_ERR(kgsl_driver.class);
		pr_err("kgsl: failed to create class for kgsl\n");
		goto err;
	}

	/*
	 * Make a virtual device for managing core related things
	 * in sysfs
	 */
	kgsl_driver.virtdev.class = kgsl_driver.class;
	dev_set_name(&kgsl_driver.virtdev, "kgsl");
	result = device_register(&kgsl_driver.virtdev);
	if (result) {
		put_device(&kgsl_driver.virtdev);
		pr_err("kgsl: driver_register failed\n");
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

	/* Initialize the memory pools */
	kgsl_probe_page_pools();

	INIT_LIST_HEAD(&kgsl_driver.process_list);

	INIT_LIST_HEAD(&kgsl_driver.pagetable_list);

	INIT_LIST_HEAD(&kgsl_driver.wp_list);

	kgsl_driver.workqueue = alloc_workqueue("kgsl-workqueue",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);

	if (!kgsl_driver.workqueue) {
		pr_err("kgsl: Failed to allocate kgsl workqueue\n");
		result = -ENOMEM;
		goto err;
	}

	/*
	 * The lockless workqueue is used to perform work which doesn't need to
	 * take the device mutex
	 */
	kgsl_driver.lockless_workqueue = alloc_workqueue("kgsl-lockless-work",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 0);

	if (!kgsl_driver.lockless_workqueue) {
		pr_err("kgsl: Failed to allocate lockless workqueue\n");
		result = -ENOMEM;
		goto err;
	}

	kgsl_eventlog_init();

	kgsl_events_init();

	result = kgsl_drawobjs_cache_init();
	if (result)
		goto err;

	memfree.list = kcalloc(MEMFREE_ENTRIES, sizeof(struct memfree_entry),
		GFP_KERNEL);

	return 0;

err:
	kgsl_core_exit();
	return result;
}
