// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
//
// * Copyright (C) 2022 Unisoc Inc.
//

#include <asm/cacheflush.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <uapi/linux/sprd_dmabuf.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/rbtree.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>


#define DMABUF_CARVEOUT_ALLOCATE_FAIL	-1

static int num_heaps;
static struct dma_heap *carve_heap;
static struct gen_pool *carve_mm_pool, *carve_fd_pool, *carve_oem_pool;
static struct dma_heap *carve_uncached_heap;
//static struct dma_heap **heaps;

struct dmabuf_platform_heap {

	const char *name;
	phys_addr_t base;
	size_t size;
	phys_addr_t align;
	void *priv;
};

struct system_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
};

struct carveout_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table *sg_table;
	//struct gen_pool *pool;
	phys_addr_t base;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
#ifdef CONFIG_E_SHOW_MEM
	struct rb_node node;
	pid_t pid;
	char task_name[TASK_COMM_LEN];
	struct timespec64 alloc_ts;
	struct dmabuf_map_info mappers[MAX_MAP_USER];
#endif
};

#ifdef CONFIG_E_SHOW_MEM
struct carveout_device {
	struct rb_root buffers;
	struct mutex buffer_lock;
};

static struct carveout_device *internal_dev;
#endif

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

struct dmabuf_device {
	struct miscdevice dev;
};

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

static int carveout_heap_attach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
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
static void carveout_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{

	struct carveout_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *carveout_heap_map_dma_buf(struct dma_buf_attachment *attachment,
													enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = attachment->dma_map_attrs;
	int ret;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

static void carveout_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
										struct sg_table *table,
										enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

static int carveout_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
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

static int carveout_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
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

#ifdef CONFIG_E_SHOW_MEM
int dmabuf_carveheap_map_user(struct dma_heap *heap, struct carveout_heap_buffer *buffer,
				struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
					vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}
#endif

static int carveout_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
#ifdef CONFIG_E_SHOW_MEM
	struct dma_heap *heap = buffer->heap;
	struct task_struct *task = current->group_leader;
	pid_t pid = task_pid_nr(task);
	int i;
#endif
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

#ifdef CONFIG_E_SHOW_MEM
	ret = dmabuf_carveheap_map_user(heap, buffer, vma);

	if (ret)
		pr_err("%s: failure mapping carvebuffer to userspace\n", __func__);

	for (i = 0; i < MAX_MAP_USER; i++) {
		if (pid == buffer->mappers[i].pid) {
			ktime_get_real_ts64(&buffer->mappers[i].map_ts);
			buffer->mappers[i].map_ts.tv_sec -= sys_tz.tz_minuteswest * 60;
			goto out;
		}
	}

	for (i = 0; i < MAX_MAP_USER; i++) {
		if (!(buffer->mappers[i].pid)) {
			buffer->mappers[i].pid = pid;
			get_task_comm(buffer->mappers[i].task_name, task);
			ktime_get_real_ts64(&buffer->mappers[i].map_ts);
			buffer->mappers[i].map_ts.tv_sec -= sys_tz.tz_minuteswest * 60;
			break;
		}
	}
out:
	return ret;
#endif

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
					vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static void *carveout_heap_do_vmap(struct carveout_heap_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
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

static int carveout_heap_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;
	int ret = 0;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		dma_buf_map_set_vaddr(map, buffer->vaddr);
		goto out;
	}

	vaddr = carveout_heap_do_vmap(buffer);
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

static void carveout_heap_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	dma_buf_map_clear(map);
}

static int dmabuf_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int dmabuf_heap_sglist_zero(struct scatterlist *sgl, unsigned int nents,
				pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = dmabuf_heap_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = dmabuf_heap_clear_pages(pages, p, pgprot);

	return ret;
}

static int dmabuf_heap_buffer_zero(struct dma_buf *dmabuf)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	pgprot_t pgprot;
	if (!buffer->uncached)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	return dmabuf_heap_sglist_zero(table->sgl, table->nents, pgprot);
}

static int dmabuf_heap_pages_zero(struct page *page, size_t size, pgprot_t pgprot)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	return dmabuf_heap_sglist_zero(&sg, 1, pgprot);

}

static void carveout_free(struct dma_heap *heap, phys_addr_t addr, unsigned long size)
{
	struct gen_pool *free_pool;
	const char *heap_name = dma_heap_get_name(heap);

	if (!strcmp(heap_name, "uncached_carveout_mm"))
		free_pool = carve_mm_pool;
	if (!strcmp(heap_name, "carveout_fd"))
		free_pool = carve_fd_pool;
	if (!strcmp(heap_name, "uncached_carveout_oem"))
		free_pool = carve_oem_pool;

	if (addr == DMABUF_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(free_pool, addr, size);
}

static void carveout_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct carveout_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	struct dma_heap *heap = buffer->heap;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

#ifdef CONFIG_E_SHOW_MEM
	struct carveout_device *dev = internal_dev;
	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
#endif

	dmabuf_heap_buffer_zero(dmabuf);

	carveout_free(heap, paddr, buffer->len);

	sg_free_table(table);
	kfree(table);
	kfree(buffer);
	pr_debug("%s: heap name: %s, paddr: 0x%llx, len: %lu\n",
			__func__, dma_heap_get_name(heap), (u64)paddr, buffer->len);

}

static const struct dma_buf_ops carveout_heap_buf_ops = {
	.attach = carveout_heap_attach,
	.detach = carveout_heap_detach,
	.map_dma_buf = carveout_heap_map_dma_buf,
	.unmap_dma_buf = carveout_heap_unmap_dma_buf,
	.begin_cpu_access = carveout_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = carveout_heap_dma_buf_end_cpu_access,
	.mmap = carveout_heap_mmap,
	.vmap = carveout_heap_vmap,
	.vunmap = carveout_heap_vunmap,
	.release = carveout_heap_dma_buf_release,
};


static phys_addr_t dmabuf_carveout_allocate(struct dma_heap *heap, unsigned long size)
{
	struct gen_pool *alloc_pool;
	unsigned long offset;
	const char *heap_name = dma_heap_get_name(heap);

	if (!strcmp(heap_name, "uncached_carveout_mm"))
		alloc_pool = carve_mm_pool;
	if (!strcmp(heap_name, "carveout_fd"))
		alloc_pool = carve_fd_pool;
	if (!strcmp(heap_name, "uncached_carveout_oem"))
		alloc_pool = carve_oem_pool;

	offset = gen_pool_alloc(alloc_pool, size);

	if (!offset)
		return DMABUF_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

#ifdef CONFIG_E_SHOW_MEM
int dmabuf_debug_carveheap_show_printk(void)
{
	int i;
	struct rb_node *n;
	struct carveout_device *dev = internal_dev;
	size_t total_size = 0;
	unsigned long pool_used = 0;

	struct tm t;

	pr_info("Heap: \n", dma_heap_get_name(carve_heap));
	pr_info("Detail:\n");
	pr_info("%-10s %-6s %-16s %-10s\n", "size", "pid", "name", "alloc_ts");

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct carveout_heap_buffer *buffer = rb_entry(n, struct carveout_heap_buffer,
			node);

		time64_to_tm(buffer->alloc_ts.tv_sec, 0, &t);
		pr_info("%-10zu %-5d %-16s %ld.%d.%d-%d:%d:%d.%ld\n",
			buffer->len, buffer->pid, buffer->task_name,
			t.tm_year + 1900, t.tm_mon + 1,
			t.tm_mday, t.tm_hour, t.tm_min,
			t.tm_sec, buffer->alloc_ts.tv_nsec);
		for (i = 0; i < MAX_MAP_USER; i++) {
			if (buffer->mappers[i].pid) {
				time64_to_tm(buffer->mappers[i].map_ts.tv_sec, 0, &t);
				pr_info("       |---%-5d  %-16s  %ld.%d.%d-%d:%d:%d.%ld\n",
						buffer->mappers[i].pid,
						buffer->mappers[i].task_name,
						t.tm_year + 1900, t.tm_mon + 1,
						t.tm_mday, t.tm_hour, t.tm_min,
						t.tm_sec, buffer->mappers[i].map_ts.tv_nsec);
			}
		}

		total_size += buffer->len;
	}
	mutex_unlock(&dev->buffer_lock);
	pr_info("----------------------------------------------------\n");
	pr_info("%16s %16zu\n", "total ", total_size);

	pr_info("----------------------------------------------------\n");

	pr_info("%16.s %lu\n", "total pooled", pool_used);

	pr_info("----------------------------------------------------------\n");
	pr_info("Total used: %lu kB\n", (unsigned long)(total_size +
		pool_used) / 1024);
	pr_info("----------------------------------------------------------\n");
	pr_info("\n");

	//*total_used += (unsigned long)(total_size + pool_used);

	return 0;
}
#endif

#ifdef CONFIG_E_SHOW_MEM
static void dmabuf_carvebuffer_add(struct carveout_device *dev, struct carveout_heap_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct carveout_heap_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct carveout_heap_buffer, node);

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
#endif

static struct dma_buf *carveout_heap_do_allocate(struct dma_heap *heap, unsigned long len,
								unsigned long fd_flags,
								unsigned long heap_flags,
								bool uncached)
{
	struct carveout_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct sg_table *table;
	size_t size = PAGE_ALIGN(len);
	unsigned int align = get_order(size);
	phys_addr_t paddr;
	int ret;
#ifdef CONFIG_E_SHOW_MEM
	struct carveout_device *dev = internal_dev;
	struct timespec64 ts;
#endif

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = size;
	buffer->uncached = uncached;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr =  dmabuf_carveout_allocate(heap, size);
	if (paddr == DMABUF_CARVEOUT_ALLOCATE_FAIL) {
		pr_err("%s: failed to alloc heap name: %s, size: %zu, len: %lu\n",
			 __func__, dma_heap_get_name(heap), size, len);
		ret = -ENOMEM;
		goto err_free_table;
	}
	pr_info("%s: heap name: %s, paddr: 0x%llx, size: %lu\n",
		__func__, dma_heap_get_name(heap), (u64)paddr, size);
	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;

#ifdef CONFIG_E_SHOW_MEM
	mutex_lock(&dev->buffer_lock);
	dmabuf_carvebuffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	buffer->pid = task_pid_nr(current->group_leader);
	get_task_comm(buffer->task_name, current->group_leader);
	ktime_get_real_ts64(&ts);
	ts.tv_sec -= sys_tz.tz_minuteswest * 60;
	buffer->alloc_ts = ts;
#endif

	/*create the dmabuf*/
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &carveout_heap_buf_ops;
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
	__free_pages(pfn_to_page(PFN_DOWN(paddr)), align);
err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	kfree(buffer);
	return ERR_PTR(ret);
}

static struct dma_buf *carveout_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	return carveout_heap_do_allocate(heap, len, fd_flags, heap_flags, false);
}

static const struct dma_heap_ops carveout_heap_ops = {
	.allocate = carveout_heap_allocate,
};

static struct dma_buf *carveout_uncached_heap_allocate(struct dma_heap *heap,
												unsigned long len,
												unsigned long fd_flags,
												unsigned long heap_flags)
{
	return carveout_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

/* Dummy function to be used until we can call coerce_mask_and_coherent */
static struct dma_buf *carveout_uncached_heap_not_initialized(struct dma_heap *heap,
							    unsigned long len,
							    unsigned long fd_flags,
							    unsigned long heap_flags)
{
	return ERR_PTR(-EBUSY);
}

static struct dma_heap_ops carveout_uncached_heap_ops = {
	/* After carveout_heap_create is complete, we will swap this */
	.allocate = carveout_uncached_heap_not_initialized,
};

static int carveout_heap_create(struct dmabuf_platform_heap *heap_data)
{
	struct carveout_heap_buffer *carveout_heap;
	struct dma_heap_export_info exp_info;
	struct page *page;
	struct gen_pool *carve_tmp_pool;
	size_t size;
	int ret;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ret = dmabuf_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ret;

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return -ENOMEM;

	if (strncmp(heap_data->name, "uncached", strlen("uncached"))) {
		exp_info.name = heap_data->name;
		exp_info.ops = &carveout_heap_ops;
		exp_info.priv = NULL;

		carve_heap = dma_heap_add(&exp_info);
		if (IS_ERR(carve_heap))
			return PTR_ERR(carve_heap);

		carve_fd_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!carve_fd_pool) {
			kfree(carveout_heap);
			return -ENOMEM;
		}

		carveout_heap->base = heap_data->base;
		ret = gen_pool_add(carve_fd_pool, carveout_heap->base, heap_data->size,
						-1);
		if (ret) {
			gen_pool_destroy(carve_fd_pool);
			return ret;
		}
		pr_info("%s: create carveout heap_name:%s, carve_heap_name: %s\n", __func__,
				heap_data->name, dma_heap_get_name(carve_heap));
	} else {
		exp_info.name = heap_data->name;
		exp_info.ops = &carveout_uncached_heap_ops;
		exp_info.priv = NULL;

		carve_uncached_heap = dma_heap_add(&exp_info);
		if (IS_ERR(carve_uncached_heap))
			return PTR_ERR(carve_uncached_heap);

		carve_tmp_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!carve_tmp_pool) {
			kfree(carveout_heap);
			return -ENOMEM;
		}

		carveout_heap->base = heap_data->base;
		ret = gen_pool_add(carve_tmp_pool, carveout_heap->base, heap_data->size,
						-1);
		if (ret) {
			gen_pool_destroy(carve_tmp_pool);
			return ret;
		}

		if (!strcmp(heap_data->name, "uncached_carveout_mm"))
			carve_mm_pool = carve_tmp_pool;
		else
			carve_oem_pool = carve_tmp_pool;

		pr_info("%s: carve_tmp_pool: %p, carve_uncached_heap_name: %s\n", __func__,
				carve_tmp_pool, dma_heap_get_name(carve_uncached_heap));
	}
	if (!IS_ERR_OR_NULL(carve_uncached_heap)) {
		dma_coerce_mask_and_coherent(dma_heap_get_dev(carve_uncached_heap), DMA_BIT_MASK(64));
		mb(); /* make sure we only set allocate after dma_mask is set */
		carveout_uncached_heap_ops.allocate = carveout_uncached_heap_allocate;
	}

	return 0;
}

static struct system_heap_buffer *get_dmabuf_sysbuffer(int fd, struct dma_buf *dmabuf)
{
	struct system_heap_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd, dmabuf);
		return ERR_PTR(-EINVAL);
		}

		if (fd >= 0) {
			dmabuf = dma_buf_get(fd);
			if (IS_ERR_OR_NULL(dmabuf)) {
				pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
					   dmabuf);
				return ERR_PTR(-EBADF);
			}
			buffer = dmabuf->priv;
			dma_buf_put(dmabuf);
		} else {
			buffer = dmabuf->priv;
		}

		return buffer;
}

int sprd_dmabuf_get_sysbuffer(int fd, struct dma_buf *dmabuf, void **buf, size_t *size)
{
	struct system_heap_buffer *buffer;

	buffer = get_dmabuf_sysbuffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->len;

	return 0;
}
EXPORT_SYMBOL(sprd_dmabuf_get_sysbuffer);

static struct carveout_heap_buffer *get_dmabuf_carvebuffer(int fd, struct dma_buf *dmabuf)
{
	struct carveout_heap_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd, dmabuf);
		return ERR_PTR(-EINVAL);
		}

		if (fd >= 0) {
			dmabuf = dma_buf_get(fd);
			if (IS_ERR_OR_NULL(dmabuf)) {
				pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
					   dmabuf);
				return ERR_PTR(-EBADF);
			}
			buffer = dmabuf->priv;
			dma_buf_put(dmabuf);
		} else {
			buffer = dmabuf->priv;
		}

		return buffer;
}

int sprd_dmabuf_get_carvebuffer(int fd, struct dma_buf *dmabuf, void **buf, size_t *size)
{
	struct carveout_heap_buffer *buffer;

	buffer = get_dmabuf_carvebuffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->len;

	return 0;
}
EXPORT_SYMBOL(sprd_dmabuf_get_carvebuffer);

int sprd_dmabuf_get_phys_addr(int fd, struct dma_buf *dmabuf, unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct carveout_heap_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	buffer = get_dmabuf_carvebuffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	table = buffer->sg_table;
	if (table && table->sgl) {
		sgl = table->sgl;
	} else {
		if (!table)
			pr_err("invalid table\n");
		else if (!table->sgl)
			pr_err("invalid table->sgl\n");
		return -EINVAL;
	}

	*phys_addr = sg_phys(sgl);
	*size = buffer->len;

	return ret;
}
EXPORT_SYMBOL(sprd_dmabuf_get_phys_addr);

int sprd_dmabuf_map_kernel(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	int ret = 0;

	if (!dmabuf)
		return -EINVAL;

	dmabuf->ops->begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	//ret = dmabuf->ops->vmap(dmabuf, &ptr);
	ret = dma_buf_vmap(dmabuf, map);

	return ret;
}
EXPORT_SYMBOL(sprd_dmabuf_map_kernel);

int sprd_dmabuf_unmap_kernel(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	if (!dmabuf)
		return -EINVAL;

	dma_buf_vunmap(dmabuf, map);
	dmabuf->ops->end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

	return 0;
}
EXPORT_SYMBOL(sprd_dmabuf_unmap_kernel);

static long sprd_dmabuf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct dmabuf_phy_data data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;
	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(_IOC_DIR(cmd) & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case DMABUF_IOC_PHY:
	{
		int fd = data.fd;

		ret = sprd_dmabuf_get_phys_addr(fd, NULL, (unsigned long *)&data.addr,
				(size_t *)&data.len);
		break;
	}
	default:
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}

static const struct file_operations sprd_dmabuf_fops = {
		.owner          = THIS_MODULE,
		.unlocked_ioctl = sprd_dmabuf_ioctl,
		.compat_ioctl = sprd_dmabuf_ioctl,
};

static int sprd_dmabuf_device_create(void)
{
	struct dmabuf_device *sprd_dmabuf_dev;
	int ret;

	sprd_dmabuf_dev = kzalloc(sizeof(*sprd_dmabuf_dev), GFP_KERNEL);
	if (!sprd_dmabuf_dev)
		return -ENOMEM;

	sprd_dmabuf_dev->dev.minor = MISC_DYNAMIC_MINOR;
	sprd_dmabuf_dev->dev.name = "sprd_dmabuf";
	sprd_dmabuf_dev->dev.fops = &sprd_dmabuf_fops;
	sprd_dmabuf_dev->dev.parent = NULL;
	ret = misc_register(&sprd_dmabuf_dev->dev);
	if (ret) {
		pr_err("dmabuf: failed to register misc device.\n");
		goto err_reg;
	}

	return 0;

err_reg:
	kfree(sprd_dmabuf_dev);
	return ret;
}

static struct dmabuf_platform_heap *sprd_dmabuf_parse_dt(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	const struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct dmabuf_platform_heap *dmabuf_heaps = NULL;
	struct platform_device *new_dev = NULL;
	const char *name;
	u32 out_values[4];
	struct device_node *np_memory;

	for_each_child_of_node(parent, child)
		num_heaps++;

	pr_info("%s: num_heaps=%d\n", __func__, num_heaps);

	if (!num_heaps)
		return NULL;

	dmabuf_heaps = kcalloc(num_heaps, sizeof(struct dmabuf_platform_heap),
			GFP_KERNEL);
	if (!dmabuf_heaps)
		return ERR_PTR(-ENOMEM);
	for_each_child_of_node(parent, child) {
		new_dev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", child->name);
			goto out;
		}

		dmabuf_heaps[i].priv = &new_dev->dev;

		ret = of_property_read_string(child, "label", &name);
		if (ret) {
			pr_err("%s: Unable to find label key, ret=%d", __func__, ret);
			goto out;
		}
		dmabuf_heaps[i].name = name;

		np_memory = of_parse_phandle(child, "memory-region", 0);

		if (!np_memory) {
			dmabuf_heaps[i].base = 0;
			dmabuf_heaps[i].size = 0;
		} else {
#ifdef CONFIG_64BIT
			ret = of_property_read_u32_array(np_memory, "reg",  out_values, 4);
			if (!ret) {
				dmabuf_heaps[i].base = out_values[0];
				dmabuf_heaps[i].base = dmabuf_heaps[i].base << 32;
				dmabuf_heaps[i].base |= out_values[1];

				dmabuf_heaps[i].size = out_values[2];
				dmabuf_heaps[i].size = dmabuf_heaps[i].size << 32;
				dmabuf_heaps[i].size |= out_values[3];
			} else {
				dmabuf_heaps[i].base = 0;
				dmabuf_heaps[i].size = 0;
			}
#else
			ret = of_property_read_u32_array(np_memory, "reg", out_values, 2);
			if (!ret) {
				dmabuf_heaps[i].base = out_values[0];
				dmabuf_heaps[i].size = out_values[1];
			} else {
				dmabuf_heaps[i].base = 0;
				dmabuf_heaps[i].size = 0;
			}
#endif
		}

		pr_info("%s: heaps[%d]: name:%s base: 0x%llx size 0x%zx\n",
				__func__, i, dmabuf_heaps[i].name,
				(u64)(dmabuf_heaps[i].base), dmabuf_heaps[i].size);
		++i;
	}
	return dmabuf_heaps;
out:
	kfree(dmabuf_heaps);
	return ERR_PTR(ret);
}


#ifdef CONFIG_E_SHOW_MEM
static int carveheap_e_show_mem_handler(struct notifier_block *nb,
unsigned long val, void *data)
{
	pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("Enhanced Mem-info :DMABUF_Heap-Carveout_heap\n");

	//for(i = 0; i < num_heaps; i++)
	dmabuf_debug_carveheap_show_printk();

	return 0;
}

static struct notifier_block carveheap_e_show_mem_notifier = {
	.notifier_call = carveheap_e_show_mem_handler,
};
#endif

static int sprd_dmabuf_probe(struct platform_device *pdev)
{
	int i = 0, ret = -1;
	struct dmabuf_platform_heap *dmabuf_heaps = NULL;

#ifdef CONFIG_E_SHOW_MEM
	struct carveout_device *carvedev;

	carvedev = kzalloc(sizeof(*carvedev), GFP_KERNEL);
	if (!carvedev) {
		kfree(carvedev);
		return -ENOMEM;
	}
	carvedev->buffers = RB_ROOT;
	mutex_init(&carvedev->buffer_lock);
	internal_dev = carvedev;
#endif

	dmabuf_heaps = sprd_dmabuf_parse_dt(pdev);
	if (IS_ERR(dmabuf_heaps)) {
		pr_err("%s: parse dt failed with err %ld\n", __func__, PTR_ERR(dmabuf_heaps));
		return PTR_ERR(dmabuf_heaps);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct dmabuf_platform_heap *heap_data = &dmabuf_heaps[i];

		if (!pdev->dev.of_node)
			heap_data->priv = &pdev->dev;

		ret = carveout_heap_create(heap_data);
		if (ret)
			pr_err("%s, carveout_heap creat fail, heap_name: %s, i: %d\n",
					__func__, heap_data->name, i);
	}

#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&carveheap_e_show_mem_notifier);
#endif

	if (dmabuf_heaps)
		goto out;

out:
	kfree(dmabuf_heaps);

	return sprd_dmabuf_device_create();
}

static const struct of_device_id sprd_dmabuf_ids[] = {
	{ .compatible = "sprd,dmabuf"},
	{},
};

static struct platform_driver sprd_dmabuf_driver = {
	.probe = sprd_dmabuf_probe,
	.driver = {
		.name = "dmabuf",
		.of_match_table = of_match_ptr(sprd_dmabuf_ids),
	},
};

module_platform_driver(sprd_dmabuf_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc DMABUF Driver");
