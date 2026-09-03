// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF CMA heap exporter
 *
 * Copyright (C) 2012, 2019, 2020 Linaro Ltd.
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * Also utilizing parts of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */
#include <linux/cma.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#include <linux/genalloc.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include "ion_ipc_trusty.h"
#include "cma.h"


#define CMA_ALLOCATE_FAIL  -1


struct cma_heap {
	struct dma_heap *heap;
	struct cma *cma;
	phys_addr_t cma_base;
} cma_heaps[MAX_CMA_AREAS];

struct cma_heap_buffer {
	struct cma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table table;
	struct page *cma_pages;
	struct page **pages;
	pgoff_t pagecount;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table table;
	struct list_head list;
	bool mapped;

	bool uncached;
};



static struct gen_pool *pool;
static bool gen_tags;
static unsigned long pool_alloc_size;
static struct cma_heap *wv_cma_heap;
static struct page *wv_pages;
static unsigned long wv_nr_pages;
static unsigned long  widevine_size;
static phys_addr_t widevine_base;
static DEFINE_MUTEX(gen_pool_lock);
static struct delayed_work destroy_pool_work;


static phys_addr_t cma_to_genallocate(unsigned long size)
{
	unsigned long offset = gen_pool_alloc(pool, size);

	pr_info("%s, pool: %p, offset: %lu\n", __func__, pool, offset);

	if (!offset)
		return CMA_ALLOCATE_FAIL;
	return offset;
}


static void destroy_gen_pool(struct work_struct *work)
{
	struct ion_message in_buf;
	struct ion_message out_buf;

	pr_info("%s, cma_pool: %p, destroyed gen pool!\n", __func__, pool);
	gen_tags = false;
	/*close firewall*/
	in_buf.cmd = TA_UNLOCK_DRM_MEM;
	ion_tipc_write(&in_buf, sizeof(in_buf));
	ion_tipc_read(&out_buf, sizeof(out_buf));
	if ((out_buf.cmd == TA_UNLOCK_DRM_MEM) && (out_buf.payload[0] == 1))
		pr_debug("TA_UNLOCK_DRM_MEM success\n");
	ion_tipc_exit();

	mutex_lock(&gen_pool_lock);
	gen_pool_destroy(pool);
	cma_release(wv_cma_heap->cma, wv_pages, wv_nr_pages);
	//sg_free_table(wv_table);
	//kfree(wv_table);
	//wv_table = NULL;
	pool = NULL;
	mutex_unlock(&gen_pool_lock);
}


/*start-up firewall*/
static void ion_notice_firewall(void)
{
	unsigned char buf[32];
	struct ion_message in_buf;
	struct ion_message out_buf;

	int ret;

	ret = ion_tipc_init();
	if (!ret) {
		ion_tipc_read(buf, sizeof(buf));
		pr_debug("tipc init succsess\n");
		in_buf.cmd = TA_LOCK_DRM_MEM;
		in_buf.payload[0] = widevine_base;
		in_buf.payload[1] = widevine_size;
		ion_tipc_write(&in_buf, sizeof(in_buf));
		ion_tipc_read(&out_buf, sizeof(out_buf));
		if ((out_buf.cmd == TA_LOCK_DRM_MEM) && (out_buf.payload[0] == 1))
			pr_debug("TA_LOCK_DRM_MEM success\n");

	} else {
		pr_debug("tipc init failed\n");
	}
}


static int cma_create_gen_pool(struct cma_heap *cma_heap)
{
	struct page *pages;
	unsigned long nr_pages;
	unsigned long align;

	widevine_size = cma_heap->cma->count << PAGE_SHIFT;
	widevine_base = PFN_PHYS(cma_heap->cma->base_pfn);
	pr_debug("%s, widevine paddr: 0x%llx, widevine size: %lu\n", __func__,
			(u64)widevine_base, widevine_size);

	nr_pages = widevine_size >> PAGE_SHIFT;
	align = get_order(widevine_size);

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	pages = cma_alloc(cma_heap->cma, nr_pages, align, false);

	if (!pages)
		return -ENOMEM;

	if (PageHighMem(pages)) {
		unsigned long nr_clear_pages = nr_pages;
		struct page *page = pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			if (fatal_signal_pending(current))
				goto free_cma;
			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(pages), 0, widevine_size);
	}

	pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!pool) {
		pr_err("%s, cma_pool: %p, create gen pool failure!\n", __func__, pool);
		goto free_cma;
	}

	cma_heap->cma_base = PFN_PHYS(page_to_pfn(pages));
	gen_pool_add(pool, cma_heap->cma_base, widevine_size, -1);
	gen_tags = true;
	pr_info("%s, cma_pool: %p, cma_heap: %p, cma_heap_base: 0x%llx,  widevine_size: %lu,gen_pool_add success!\n",
				__func__, pool, cma_heap, cma_heap->cma_base, widevine_size);

	wv_cma_heap = cma_heap;
	wv_pages = pages;
	wv_nr_pages = nr_pages;

	return 0;

free_cma:
	cma_release(cma_heap->cma, pages, nr_pages);

	return -ENOMEM;
}

static int cma_create_pool(struct cma_heap *cma_heap)
{
	return cma_create_gen_pool(cma_heap);
}



static int cma_heap_attach(struct dma_buf *dmabuf,
			   struct dma_buf_attachment *attachment)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = sg_alloc_table_from_pages(&a->table, buffer->pages,
					buffer->pagecount, 0,
					buffer->pagecount << PAGE_SHIFT,
					GFP_KERNEL);
	if (ret) {
		kfree(a);
		return ret;
	}

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

static void cma_heap_detach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attachment)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(&a->table);
	kfree(a);
}

static struct sg_table *cma_heap_map_dma_buf(struct dma_buf_attachment *attachment,
					     enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = &a->table;
	int attrs = attachment->dma_map_attrs;
	int ret;

	if (a->uncached)
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attrs);
	if (ret)
		return ERR_PTR(-ENOMEM);
	a->mapped = true;
	return table;
}

static void cma_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attrs = attachment->dma_map_attrs;

	if (a->uncached)
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attrs);
}

static int cma_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					     enum dma_data_direction direction)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_cpu(a->dev, &a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int cma_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, &a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static vm_fault_t cma_heap_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct cma_heap_buffer *buffer = vma->vm_private_data;

	if (vmf->pgoff >= buffer->pagecount)
		return VM_FAULT_SIGBUS;

	vmf->page = buffer->pages[vmf->pgoff];
	get_page(vmf->page);

	return 0;
}

static const struct vm_operations_struct dma_heap_vm_ops = {
	.fault = cma_heap_vm_fault,
};

static int cma_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{

	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if ((vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) == 0)
		return -EINVAL;

	if (buffer->uncached) {

		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

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

	vma->vm_ops = &dma_heap_vm_ops;
	vma->vm_private_data = buffer;

	return 0;
}

static void *cma_heap_do_vmap(struct cma_heap_buffer *buffer)
{
	void *vaddr;
	pgprot_t pgprot = PAGE_KERNEL;

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	vaddr = vmap(buffer->pages, buffer->pagecount, VM_MAP, pgprot);
	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static int cma_heap_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;
	int ret = 0;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		dma_buf_map_set_vaddr(map, buffer->vaddr);
		goto out;
	}

	vaddr = cma_heap_do_vmap(buffer);
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

static void cma_heap_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	dma_buf_map_clear(map);
}

static void cma_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct cma_heap_buffer *buffer = dmabuf->priv;
	struct cma_heap *cma_heap = buffer->heap;
	struct page *pages = buffer->cma_pages;
	phys_addr_t addrs = PFN_PHYS(page_to_pfn(pages));
	phys_addr_t offset = widevine_size;
	phys_addr_t low = cma_heap->cma_base;
	phys_addr_t high = low + offset;

	if (buffer->vmap_cnt > 0) {
		WARN(1, "%s: buffer still mapped in the kernel\n", __func__);
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}

	pr_info("%s, cma_pool: %p, size: %zu, cma_addrs: 0x%llx, offset: 0x%llx, low: 0x%llx\n",
				__func__, pool, buffer->len, addrs, offset, low);

	/* free page list */
	kfree(buffer->pages);

	if (!strcmp(dma_heap_get_name(cma_heap->heap), "uncached_carveout_mm")) {
		if (pool != NULL) {
			if (addrs >= low && addrs < high) {
				pr_debug("%s, cma_pool: %p, free to gen pool,pool_alloc_size: %lu!\n",
						__func__, pool, pool_alloc_size);
				mutex_lock(&gen_pool_lock);
				gen_pool_free(pool, addrs, buffer->len);
				pool_alloc_size -= buffer->len;
				mutex_unlock(&gen_pool_lock);
				pr_debug("%s, remain pool_alloc_size: %lu, buffer_size: %zu\n",
						__func__, pool_alloc_size, buffer->len);
			} else {
				pr_err("%s: addr out of range\n", __func__);
				return;
			}
		} else {
			/*release memory*/
			pr_err("%s, buffer release process exception!\n", __func__);
		}
	} else {
		/* release memory */
		cma_release(cma_heap->cma, buffer->cma_pages, buffer->pagecount);
	}
	kfree(buffer);

	if (!strcmp(dma_heap_get_name(cma_heap->heap), "uncached_carveout_mm") &&
							(pool_alloc_size == 0) && (pool != NULL)) {
		pr_debug("%s, cma_pool: %p, wait for destroying gen pool!\n", __func__, pool);
		schedule_delayed_work(&destroy_pool_work, msecs_to_jiffies(5000));
	}

}

static const struct dma_buf_ops cma_heap_buf_ops = {
	.attach = cma_heap_attach,
	.detach = cma_heap_detach,
	.map_dma_buf = cma_heap_map_dma_buf,
	.unmap_dma_buf = cma_heap_unmap_dma_buf,
	.begin_cpu_access = cma_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = cma_heap_dma_buf_end_cpu_access,
	.mmap = cma_heap_mmap,
	.vmap = cma_heap_vmap,
	.vunmap = cma_heap_vunmap,
	.release = cma_heap_dma_buf_release,
};

static struct dma_buf *cma_heap_do_allocate(struct dma_heap *heap,
					 unsigned long len,
					 unsigned long fd_flags,
					 unsigned long heap_flags,
					 bool uncached)
{
	struct cma_heap *cma_heap = dma_heap_get_drvdata(heap);
	struct cma_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	size_t size = PAGE_ALIGN(len);
	pgoff_t pagecount = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);
	struct page *cma_pages;
	struct dma_buf *dmabuf;
	int ret = -ENOMEM;
	int er;
	pgoff_t pg;
	phys_addr_t alloc_base;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->len = size;

	buffer->uncached = uncached;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	pr_info("%s, buffer_size: %zu, heap_name: %s, buffer_uncached: %d\n",
		__func__, buffer->len, dma_heap_get_name(heap), buffer->uncached);

	if (!strcmp(dma_heap_get_name(heap), "uncached_carveout_mm")) {
		pr_debug("%s, cma_heap: %p, heap: %p, cma_pool: %p, gen_tags: %d, start from gen_pool allocate!\n",
			 __func__, cma_heap, heap, pool, gen_tags);

		if (delayed_work_pending(&destroy_pool_work))
			cancel_delayed_work_sync(&destroy_pool_work);

		mutex_lock(&gen_pool_lock);
		if (gen_tags) {
			alloc_base = cma_to_genallocate(size);
			pr_debug("%s, second paddr: 0x%llx,get buffer from gen pool!\n",
					__func__, (u64)alloc_base);
			if (alloc_base == CMA_ALLOCATE_FAIL) {
				pr_err("%s: second failed to alloc heap_name: %s, size: %lu\n",
					__func__, dma_heap_get_name(heap), size);
				mutex_unlock(&gen_pool_lock);
				return ERR_PTR(-ENOMEM);
			}
			pool_alloc_size += size;
			cma_pages = pfn_to_page(PFN_DOWN(alloc_base));
			pr_debug("%s, second gen_alloc pages range: %p!\n", __func__, cma_pages);
			mutex_unlock(&gen_pool_lock);
		} else {
			if (cma_create_pool(cma_heap) == 0) {
				alloc_base = cma_to_genallocate(size);
				pr_debug("%s, first paddr: 0x%llx,get buffer from gen pool!\n",
						__func__, (u64)alloc_base);
				if (alloc_base == CMA_ALLOCATE_FAIL) {
					pr_err("%s: first failed to alloc heap_name: %s, size: %lu\n",
						__func__, dma_heap_get_name(heap), size);
					mutex_unlock(&gen_pool_lock);
					return ERR_PTR(-ENOMEM);
				}
				pool_alloc_size += size;
				cma_pages = pfn_to_page(PFN_DOWN(alloc_base));
				pr_debug("%s, first gen_alloc pages range: %p!\n",
						__func__, cma_pages);
				mutex_unlock(&gen_pool_lock);
				ion_notice_firewall();
			} else {
				mutex_unlock(&gen_pool_lock);
				return ERR_PTR(-ENOMEM);
			}
		}
	} else {
		cma_pages = cma_alloc(cma_heap->cma, pagecount, align, false);
	}

	pr_debug("%s, cma_heap->cma: %p, cma_pages: %p, buffer_uncached: %d\n",
		__func__, cma_heap->cma, cma_pages, buffer->uncached);

	if (!cma_pages)
		goto free_buffer;

	/* Clear the cma pages */
	if (PageHighMem(cma_pages)) {
		unsigned long nr_clear_pages = pagecount;
		struct page *page = cma_pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			/*
			 * Avoid wasting time zeroing memory if the process
			 * has been killed by by SIGKILL
			 */
			if (fatal_signal_pending(current))
				goto free_cma;
			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(cma_pages), 0, size);
	}

	buffer->pages = kmalloc_array(pagecount, sizeof(*buffer->pages), GFP_KERNEL);
	if (!buffer->pages) {
		ret = -ENOMEM;
		goto free_cma;
	}

	for (pg = 0; pg < pagecount; pg++)
		buffer->pages[pg] = &cma_pages[pg];

	buffer->cma_pages = cma_pages;
	buffer->heap = cma_heap;
	buffer->pagecount = pagecount;

	if (buffer->uncached) {
		er = sg_alloc_table_from_pages(&buffer->table, buffer->pages, buffer->pagecount,
				0, buffer->pagecount << PAGE_SHIFT, GFP_KERNEL);
		if (er) {
			pr_err("%s, get sg fail\n", __func__);
			kfree(buffer);
			return ERR_PTR(ret);
		}
	}

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &cma_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	if (buffer->uncached) {
		dma_map_sgtable(dma_heap_get_dev(heap), &buffer->table, DMA_BIDIRECTIONAL, 0);
		dma_unmap_sgtable(dma_heap_get_dev(heap), &buffer->table, DMA_BIDIRECTIONAL, 0);
	}

	return dmabuf;

free_pages:
	kfree(buffer->pages);
free_cma:
	if (gen_tags)
		gen_pool_free(pool, PFN_PHYS(page_to_pfn(cma_pages)), size);
	else
		cma_release(cma_heap->cma, cma_pages, pagecount);

free_buffer:
	kfree(buffer);

	return ERR_PTR(ret);
}

static struct dma_buf *cma_heap_allocate(struct dma_heap *heap,
				unsigned long len,
				unsigned long fd_flags,
				unsigned long heap_flags)
{
	return cma_heap_do_allocate(heap, len, fd_flags, heap_flags, false);
}

static const struct dma_heap_ops cma_heap_ops = {
	.allocate = cma_heap_allocate,
};

static struct dma_buf *cma_uncached_heap_allocate(struct dma_heap *heap,
					unsigned long len,
					unsigned long fd_flags,
					unsigned long heap_flags)
{
	return cma_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

/* Dummy function to be used until we can call coerce_mask_and_coherent */
static struct dma_buf *cma_uncached_heap_not_initialized(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags)
{
	return ERR_PTR(-EBUSY);
}

static struct dma_heap_ops cma_uncached_heap_ops = {
	/* After cma_heap_create is complete, we will swap this */
	.allocate = cma_uncached_heap_not_initialized,
};

static int __add_cma_heap(struct cma *cma, void *data)
{
	struct cma_heap *cma_heap;
	struct dma_heap_export_info exp_info;
	int *cma_nr = data;

	if (*cma_nr >= MAX_CMA_AREAS)
		return -EINVAL;

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);
	if (!cma_heap)
		return -ENOMEM;

	cma_heap = &cma_heaps[*cma_nr];

	if (!strncmp(cma_get_name(cma), "widevine", 8)) {

		exp_info.name = "uncached_carveout_mm";
		exp_info.ops = &cma_uncached_heap_ops;
		exp_info.priv = cma_heap;

		cma_heap->heap = dma_heap_add(&exp_info);
		if (IS_ERR(cma_heap->heap)) {
			int ret = PTR_ERR(cma_heap->heap);

			kfree(cma_heap);
			return ret;
		}

		dma_coerce_mask_and_coherent(dma_heap_get_dev(cma_heap->heap), DMA_BIT_MASK(64));
		mb(); /* make sure we only set allocate after dma_mask is set */
		cma_uncached_heap_ops.allocate = cma_uncached_heap_allocate;
	} else {
		exp_info.name = cma_get_name(cma);
		exp_info.ops = &cma_heap_ops;
		exp_info.priv = cma_heap;

		cma_heap->heap = dma_heap_add(&exp_info);
		if (IS_ERR(cma_heap->heap)) {
			int ret = PTR_ERR(cma_heap->heap);

			kfree(cma_heap);
			return ret;
		}

	}

	cma_heap->cma = cma;
	*cma_nr += 1;

	return 0;
}

static int add_default_cma_heap(void)
{
	int ret;
	int nr = 0;

	gen_tags = false;

	INIT_DELAYED_WORK(&destroy_pool_work, destroy_gen_pool);
	ret = cma_for_each_area(__add_cma_heap, &nr);
	if (ret)
		pr_err("%s, get cma_area failed!\n", __func__);

	return ret;
}

module_init(add_default_cma_heap);
MODULE_DESCRIPTION("DMA-BUF CMA Heap");
MODULE_LICENSE("GPL v2");
