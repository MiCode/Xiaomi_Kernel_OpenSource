// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF System heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */

#include <linux/sprd_iommu.h>
#include <linux/proc_fs.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm/emem.h>

#include "page_pool.h"
#include <linux/rbtree.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/miscdevice.h>

static DEFINE_MUTEX(system_heap_lock);

static struct dma_heap *sys_heap;
static struct dma_heap *sys_uncached_heap;

extern int sprd_iommu_notifier_call_chain(void *data);

struct system_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
	struct rb_node node;
	pid_t pid;
	char task_name[TASK_COMM_LEN];
	struct timespec64 alloc_ts;
	struct dmabuf_map_info mappers[MAX_MAP_USER];

	struct dmabuf_map_info mmap[MAX_MAP_USER];
};

struct system_device {
	struct rb_root buffers;
	struct mutex buffer_lock;
};

static struct system_device *internal_dev;
#define EGL_MEMTRACK_MAP _IOW('e', 0, int)
#define EGL_MEMTRACK_UNMAP _IOW('e', 1, int)
#define MMAP_USER 15

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

#ifdef CONFIG_UNISOC_CMA_FEATURES
#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP | __GFP_CMA)
#define MID_ORDER_GFP (LOW_ORDER_GFP | __GFP_NOWARN)
#define HIGH_ORDER_GFP  (((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM) \
				| __GFP_COMP | __GFP_CMA)
#else
#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
#define MID_ORDER_GFP (LOW_ORDER_GFP | __GFP_NOWARN)
#define HIGH_ORDER_GFP  (((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM) \
				| __GFP_COMP)
#endif
static gfp_t order_flags[] = {HIGH_ORDER_GFP, HIGH_ORDER_GFP, LOW_ORDER_GFP};
/*
 * The selection of the orders used for allocation (1MB, 64K, 4K) is designed
 * to match with the sizes often found in IOMMUs. Using order 4 pages instead
 * of order 0 pages can significantly improve the performance of many IOMMUs
 * by reducing TLB pressure and time spent updating page tables.
 */
static const unsigned int orders[] = {8, 4, 0};
#define NUM_ORDERS ARRAY_SIZE(orders)
struct dmabuf_page_pool *pools[NUM_ORDERS];

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int system_heap_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void system_heap_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *system_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = attachment->dma_map_attrs;
	int ret;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

static void system_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

static int system_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int system_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int system_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int k;
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sg(table->sgl, sg, table->orig_nents, k) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
					vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static void *system_heap_do_vmap(struct system_heap_buffer *buffer)
{
	struct sg_table *table = &buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static int system_heap_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;
	int ret = 0;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		dma_buf_map_set_vaddr(map, buffer->vaddr);
		goto out;
	}

	vaddr = system_heap_do_vmap(buffer);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		goto out;
	}

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
	dma_buf_map_set_vaddr(map, buffer->vaddr);
out:
	mutex_unlock(&buffer->lock);

	return ret;
}

static void system_heap_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct system_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	dma_buf_map_clear(map);
}

static int system_heap_zero_buffer(struct system_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_local_page(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_local(vaddr);
	}

	return ret;
}

static void system_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;

	struct system_device *dev = internal_dev;
	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
	/* Zero the buffer pages before adding back to the pool */
	system_heap_zero_buffer(buffer);

	table = &buffer->sg_table;
	for_each_sgtable_sg(table, sg, i) {
		struct page *page = sg_page(sg);

		for (j = 0; j < NUM_ORDERS; j++) {
			if (compound_order(page) == orders[j])
				break;
		}
		dmabuf_page_pool_free(pools[j], page);
	}
	sg_free_table(table);
	sprd_iommu_notifier_call_chain((void *)buffer);
	kfree(buffer);
}

static const struct dma_buf_ops system_heap_buf_ops = {
	.attach = system_heap_attach,
	.detach = system_heap_detach,
	.map_dma_buf = system_heap_map_dma_buf,
	.unmap_dma_buf = system_heap_unmap_dma_buf,
	.begin_cpu_access = system_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = system_heap_dma_buf_end_cpu_access,
	.mmap = system_heap_mmap,
	.vmap = system_heap_vmap,
	.vunmap = system_heap_vunmap,
	.release = system_heap_dma_buf_release,
};

static struct page *alloc_largest_available(unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		page = dmabuf_page_pool_alloc(pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

static void dmabuf_sysbuffer_add(struct system_device *dev, struct system_heap_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct system_heap_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct system_heap_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}
	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

static struct dma_buf *system_heap_do_allocate(struct dma_heap *heap,
					       unsigned long len,
					       unsigned long fd_flags,
					       unsigned long heap_flags,
					       bool uncached)
{
	struct system_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size_remaining = len;
	unsigned int max_order = orders[0];
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i, ret = -ENOMEM;
	struct system_device *dev = internal_dev;
	struct timespec64 ts;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = uncached;

	INIT_LIST_HEAD(&pages);
	i = 0;
	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			goto free_buffer;
		}

		page = alloc_largest_available(size_remaining, max_order);
		if (!page)
			goto free_buffer;

		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_buffer;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	mutex_lock(&dev->buffer_lock);
	dmabuf_sysbuffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	buffer->pid = task_pid_nr(current->group_leader);
	get_task_comm(buffer->task_name, current->group_leader);
	ktime_get_real_ts64(&ts);
	ts.tv_sec -= sys_tz.tz_minuteswest * 60;
	buffer->alloc_ts = ts;

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &system_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	if (buffer->uncached) {
		dma_map_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
		dma_unmap_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
	}

	return dmabuf;

free_pages:
	for_each_sgtable_sg(table, sg, i) {
		struct page *p = sg_page(sg);

		__free_pages(p, compound_order(p));
	}
	sg_free_table(table);
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		__free_pages(page, compound_order(page));
	kfree(buffer);

	return ERR_PTR(ret);
}

static struct dma_buf *system_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	return system_heap_do_allocate(heap, len, fd_flags, heap_flags, false);
}

static long system_get_pool_size(struct dma_heap *heap)
{
	int i;
	long num_pages = 0;
	struct dmabuf_page_pool **pool;

	pool = pools;
	for (i = 0; i < NUM_ORDERS; i++, pool++) {
		num_pages += ((*pool)->count[POOL_LOWPAGE] +
			      (*pool)->count[POOL_HIGHPAGE]) << (*pool)->order;
	}

	return num_pages << PAGE_SHIFT;
}

#if (IS_ENABLED(CONFIG_UNISOC_MM_ENHANCE_MEMINFO)) || (IS_ENABLED(CONFIG_E_SHOW_MEM))
int dmabuf_debug_sysheap_show_printk(struct dma_heap *heap, void *data)
{
	int i;
	struct rb_node *n;
	struct system_device *dev = internal_dev;
	size_t total_size = 0;
	unsigned long pool_used = 0;
	unsigned long *total_used = data;
	struct tm t;

	pr_info("Heap: %s\n", dma_heap_get_name(heap));
	pr_info("Detail:\n");
	pr_info("%-10s %-6s %-16s %-10s\n", "size", "pid", "name", "alloc_ts");

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct system_heap_buffer *buffer = rb_entry(n, struct system_heap_buffer,
			node);
		if (!IS_ERR_OR_NULL(buffer)) {
			time64_to_tm(buffer->alloc_ts.tv_sec, 0, &t);
			pr_info("%-10zu %-5d %-16s %ld.%d.%d-%d:%d:%d.%ld\n",
				buffer->len, buffer->pid, buffer->task_name,
				t.tm_year + 1900, t.tm_mon + 1,
				t.tm_mday, t.tm_hour, t.tm_min,
				t.tm_sec, buffer->alloc_ts.tv_nsec);
		}
		for (i = 0; i < MAX_MAP_USER; i++) {
			if (!IS_ERR_OR_NULL(buffer)) {
				if (buffer->mappers[i].valid) {
					time64_to_tm(buffer->mappers[i].map_ts.tv_sec, 0, &t);
					pr_info("       |---%-5d  %-5d  %-16s  %ld.%d.%d-%d:%d:%d.%ld\n",
						buffer->mappers[i].pid,
						buffer->mappers[i].fd,
						buffer->mappers[i].task_name,
						t.tm_year + 1900, t.tm_mon + 1,
						t.tm_mday, t.tm_hour, t.tm_min,
						t.tm_sec, buffer->mappers[i].map_ts.tv_nsec);
				}
			}
		}
		if (!IS_ERR_OR_NULL(buffer))
			total_size += buffer->len;
	}
	mutex_unlock(&dev->buffer_lock);
	pr_info("----------------------------------------------------\n");
	pr_info("%16s %16zu\n", "total ", total_size);

	pr_info("----------------------------------------------------\n");

	pool_used = system_get_pool_size(heap);
	pr_info("%16.s %lu\n", "total pooled", pool_used);
	pr_info("----------------------------------------------------------\n");
	pr_info("Total used: %lu kB\n", (unsigned long)(total_size +
		pool_used) / 1024);
	pr_info("----------------------------------------------------------\n");
	pr_info("\n");

	*total_used += (unsigned long)(total_size + pool_used);

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_debug_sysheap_show_printk);
#endif

static const struct dma_heap_ops system_heap_ops = {
	.allocate = system_heap_allocate,
	.get_pool_size = system_get_pool_size,
};

static struct dma_buf *system_uncached_heap_allocate(struct dma_heap *heap,
						     unsigned long len,
						     unsigned long fd_flags,
						     unsigned long heap_flags)
{
	return system_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

/* Dummy function to be used until we can call coerce_mask_and_coherent */
static struct dma_buf *system_uncached_heap_not_initialized(struct dma_heap *heap,
							    unsigned long len,
							    unsigned long fd_flags,
							    unsigned long heap_flags)
{
	return ERR_PTR(-EBUSY);
}

#if (IS_ENABLED(CONFIG_UNISOC_MM_ENHANCE_MEMINFO)) || (IS_ENABLED(CONFIG_E_SHOW_MEM))
static int dmabuf_e_show_mem_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	struct dma_heap *heap;
	unsigned long total_used = 0;
	const char *name = "system";

	pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("Enhanced Mem-info :DMABUF_Heap\n");

	heap = dma_heap_find(name);
	mutex_lock(&system_heap_lock);
	dmabuf_debug_sysheap_show_printk(heap, &total_used);
	mutex_unlock(&system_heap_lock);

	pr_info("Total allocated from Buddy: %lu kB\n", total_used / 1024);
	return 0;
}

static struct notifier_block dmabuf_e_show_mem_notifier = {
	.notifier_call = dmabuf_e_show_mem_handler,
};
#else
static struct notifier_block dmabuf_e_show_mem_notifier;
#endif

static struct dma_heap_ops system_uncached_heap_ops = {
	/* After system_heap_create is complete, we will swap this */
	.allocate = system_uncached_heap_not_initialized,
};

static int egl_mtrack_mmap(int fd, bool is_mapping)
{
	int i;
	struct system_heap_buffer *buffer;
	struct task_struct *task = current->group_leader;
	pid_t pid = task_pid_nr(task);
	struct dma_buf *dmabuf;
	int ret = 0;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return -ENOENT;

	if (strcmp(dmabuf->exp_name, "system") && strcmp(dmabuf->exp_name, "system-uncached")) {
		dma_buf_put(dmabuf);
		return -EFAULT;
	}

	buffer = dmabuf->priv;

	if (is_mapping) { // Map operation
		for (i = 0; i < MAX_MAP_USER; i++) {
			if (pid == buffer->mmap[i].pid) // Mapping already exists
				goto out;
		}

		for (i = 0; i < MAX_MAP_USER; i++) { // If no mapping found, add a new mapping
			if (!buffer->mmap[i].pid) {
				buffer->mmap[i].pid = pid;
				get_task_comm(buffer->mmap[i].task_name, task);
				break;
			}
		}
	} else { // Unmap operation
		for (i = 0; i < MAX_MAP_USER; i++) {
			if (buffer->mmap[i].pid == pid) {
				memset((void *)(&buffer->mmap[i]), 0x0,
					   sizeof(struct dmabuf_map_info)); // Clear mapping info
				break;
			}
		}
	}

out:
	dma_buf_put(dmabuf);
	return ret;
}

static int egl_mtrack_show(struct seq_file *m, void *v)
{
	int i;
	int p_num;
	struct system_device *dev = internal_dev;
	struct rb_node *n;

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct system_heap_buffer *buffer = rb_entry(n,
				struct system_heap_buffer, node);
		if (!strncmp(buffer->task_name, "allocator", strlen("allocator"))) {
			p_num = 0;
			for (i = 0; i < MMAP_USER; i++) {
				if (buffer->mmap[i].pid != 0)
					p_num++; // Count number of mapped processes
			}
			// If no processes are mapped to this buffer, skip to the next buffer
			if (p_num == 0)
				continue; // Skip to the next iteration of the loop

			for (i = 0; i < MMAP_USER; i++) {
				seq_printf(m, "%d %zu %s\n", buffer->mmap[i].pid,
					(buffer->len) / p_num, buffer->mmap[i].task_name);
			}
		}
	}
	mutex_unlock(&dev->buffer_lock);
	return 0;
}

static int egl_mtrack_open(struct inode *inode, struct file *file)
{
	return single_open(file, egl_mtrack_show, NULL);
}

static struct proc_ops egl_mtrack_ops = {
	.proc_open = egl_mtrack_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static long systemheap_ioctl(struct file *filp, unsigned int ioctl,
				unsigned long arg)
{
	int ret, fd;

	if (copy_from_user(&fd, (void __user *)arg, sizeof(int)))
		return -EFAULT;

	switch (ioctl) {
	case EGL_MEMTRACK_MAP:
	{
		ret = egl_mtrack_mmap(fd, true);
		break;
	}
	case EGL_MEMTRACK_UNMAP:
	{
		ret = egl_mtrack_mmap(fd, false);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations systemheap_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = systemheap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = systemheap_ioctl,
#endif
};

static struct miscdevice system_heap = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "systemheap",
	.fops = &systemheap_fops,
};

static int system_heap_create(void)
{
	struct proc_dir_entry *proc_root;
	struct dma_heap_export_info exp_info;
	int i, ret;
	struct system_device *sysdev;

	sysdev = kzalloc(sizeof(*sysdev), GFP_KERNEL);
	if (!sysdev) {
		kfree(sysdev);
		return -ENOMEM;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i] = dmabuf_page_pool_create(order_flags[i], orders[i]);

		if (IS_ERR(pools[i])) {
			int j;

			pr_err("%s: page pool creation failed!\n", __func__);
			for (j = 0; j < i; j++)
				dmabuf_page_pool_destroy(pools[j]);
			return PTR_ERR(pools[i]);
		}
	}

	exp_info.name = "system";
	exp_info.ops = &system_heap_ops;
	exp_info.priv = NULL;

	sys_heap = dma_heap_add(&exp_info);
	if (IS_ERR(sys_heap))
		return PTR_ERR(sys_heap);

	exp_info.name = "system-uncached";
	exp_info.ops = &system_uncached_heap_ops;
	exp_info.priv = NULL;

	sys_uncached_heap = dma_heap_add(&exp_info);
	if (IS_ERR(sys_uncached_heap))
		return PTR_ERR(sys_uncached_heap);

	sysdev->buffers = RB_ROOT;
	mutex_init(&sysdev->buffer_lock);
	internal_dev = sysdev;

	dma_coerce_mask_and_coherent(dma_heap_get_dev(sys_uncached_heap), DMA_BIT_MASK(64));
	mb(); /* make sure we only set allocate after dma_mask is set */
	system_uncached_heap_ops.allocate = system_uncached_heap_allocate;

#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&dmabuf_e_show_mem_notifier);
#endif
	register_unisoc_show_mem_notifier(&dmabuf_e_show_mem_notifier);

	ret = misc_register(&system_heap);
	if (ret < 0) {
		pr_err("could not initialize system_heap device");
		return ret;
	}
	proc_root = proc_mkdir("mtrack", NULL);
	proc_create("EGL_MTRACK", 0444, proc_root, &egl_mtrack_ops);

	return 0;
}
module_init(system_heap_create);
MODULE_LICENSE("GPL v2");
