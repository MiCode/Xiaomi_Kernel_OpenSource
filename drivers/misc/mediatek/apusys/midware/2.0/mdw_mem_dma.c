// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/highmem.h>

#include "mdw_mem_rsc.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_trace.h"

struct mdw_mem_dma_attachment {
	struct sg_table *sgt;
	struct device *dev;
	struct list_head node;
	bool mapped;
	bool uncached;
};

struct mdw_mem_dma {
	dma_addr_t dma_addr;
	uint32_t dma_size;
	uint32_t size;

	void *vaddr;
	struct list_head attachments;
	struct sg_table sgt;
	void *buf;

	bool uncached;

	struct mutex mtx;

	struct mdw_mem *mmem;
	struct device *mem_dev;
};

#define mdw_mem_dma_show(d) \
	mdw_mem_debug("mem(0x%llx/%d/0x%llx/0x%x/0x%llx/0x%x/%d/%d)(%d)\n", \
	(uint64_t) d->mmem, d->mmem->handle, (uint64_t)d->mmem->vaddr, d->mmem->size, \
	d->dma_addr, d->dma_size, d->mmem->need_handle, \
	file_count(d->mmem->dbuf->file), current->pid)

static struct sg_table *mdw_mem_dma_dup_sg(struct sg_table *table)
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

static int mdw_mem_dma_allocate_sgt(const char *buf,
		size_t len, struct sg_table *sgt, bool uncached, void **vaddr)
{
	struct page **pages = NULL;
	unsigned int nr_pages;
	unsigned int index;
	const char *p;
	int ret;
	pgprot_t pgprot = PAGE_KERNEL;
	void *va;

	nr_pages = DIV_ROUND_UP((unsigned long)buf + len, PAGE_SIZE)
		- ((unsigned long)buf / PAGE_SIZE);
	pages = kvmalloc(nr_pages * sizeof(struct page *), GFP_KERNEL);

	mdw_mem_debug("mdw buf: 0x%llx, len: 0x%lx, nr_pages: %d\n",
		buf, len, nr_pages);

	if (!pages) {
		mdw_drv_err("No Page 0x%llx, len: 0x%lx, nr_pages: %d\n",
				buf, len, nr_pages);
		return -ENOMEM;
	}


	p = buf - offset_in_page(buf);
	mdw_mem_debug("start p: 0x%llx buf: 0x%llx\n",
			(uint64_t) p, (uint64_t) buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kvfree(pages);
			mdw_drv_err("map failed\n");
			return -EFAULT;
		}
		p += PAGE_SIZE;
	}
	if (uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	//vmap to set page property
	va = vmap(pages, nr_pages, VM_MAP, pgprot);

	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);
	kvfree(pages);
	if (ret) {
		mdw_drv_err("sg_alloc_table_from_pages: %d\n", ret);
		return ret;
	}

	*vaddr = va;

	mdw_mem_debug("buf: 0x%llx, len: 0x%lx, sgt: 0x%llx nr_pages: %d va 0x%llx\n",
		buf, len, sgt, nr_pages, va);

	return 0;
}

static int mdw_mem_dma_free_sgt(struct sg_table *sgt)
{
	int ret = 0;

	sg_free_table(sgt);

	return ret;
}

static int mdw_dmabuf_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_dma_attachment *a = NULL;
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;
	int ret = 0;
	struct sg_table *table;

	mdw_mem_debug("dbuf(0x%llx)\n", (uint64_t)dbuf);

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = mdw_mem_dma_dup_sg(&mdbuf->sgt);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->sgt = table;
	a->dev = attach->dev;
	INIT_LIST_HEAD(&a->node);
	a->mapped = false;
	a->uncached = mdbuf->uncached;
	attach->priv = a;


	mutex_lock(&mdbuf->mtx);
	list_add(&a->node, &mdbuf->attachments);
	mutex_unlock(&mdbuf->mtx);

	mdw_mem_dma_show(mdbuf);

	return ret;
}

static void mdw_dmabuf_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;

	mdw_mem_debug("dbuf(0x%llx)\n", (uint64_t)dbuf);

	mutex_lock(&mdbuf->mtx);
	list_del(&a->node);
	mutex_unlock(&mdbuf->mtx);

	sg_free_table(a->sgt);
	kfree(a->sgt);
	kfree(a);

	mdw_mem_dma_show(mdbuf);
}

static struct sg_table *mdw_dmabuf_map_dma(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	struct sg_table *table = NULL;
	int attr = attach->dma_map_attrs;
	int ret = 0;

	table = a->sgt;
	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attach->dev, table, dir, attr);
	if (ret)
		table = ERR_PTR(ret);

	a->mapped = true;
	mdw_mem_debug("\n");

	return table;
}

static void mdw_dmabuf_unmap_dma(struct dma_buf_attachment *attach,
				    struct sg_table *sgt,
				    enum dma_data_direction dir)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	int attr = attach->dma_map_attrs;

	mdw_mem_debug("\n");

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	a->mapped = false;
	dma_unmap_sgtable(attach->dev, sgt, dir, attr);
}

static void *mdw_dmabuf_vmap(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;

	mdw_mem_debug("dmabuf vmap: 0x%llx\n", (uint64_t) mdbuf->vaddr);
	return mdbuf->vaddr;
}

static int mdw_dmabuf_invalidate(struct dma_buf *dbuf,
	enum dma_data_direction dir)
{
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;

	if (!mdbuf->vaddr) {
		mdw_drv_err("mdbuf vaddr NULL\n");
		return -EINVAL;
	}

	if (!mdbuf->uncached) {
		dma_sync_sgtable_for_cpu(mdbuf->mem_dev,
			&mdbuf->sgt, DMA_FROM_DEVICE);
		mdw_mem_debug("mem invalidate(0x%llx)\n", (uint64_t)m);
	}

	mdw_mem_dma_show(mdbuf);
	return 0;
}

static int mdw_dmabuf_flush(struct dma_buf *dbuf,
	enum dma_data_direction dir)
{
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;

	if (!mdbuf->vaddr) {
		mdw_drv_err("mdbuf vaddr NULL\n");
		return -EINVAL;
	}

	if (!mdbuf->uncached) {
		dma_sync_sgtable_for_device(mdbuf->mem_dev,
			&mdbuf->sgt, DMA_TO_DEVICE);
		mdw_mem_debug("mem flush(0x%llx)\n", (uint64_t) m);
	}

	mdw_mem_dma_show(mdbuf);

	return 0;
}

static void mdw_dmabuf_release(struct dma_buf *dbuf)
{
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;

	mdw_mem_dma_show(mdbuf);

	mdw_mem_dma_free_sgt(&mdbuf->sgt);
	vunmap(mdbuf->vaddr);
	vfree(mdbuf->buf);
	kfree(mdbuf);
	m->release(m);
}

static int mdw_dmabuf_mmap(struct dma_buf *dbuf,
				  struct vm_area_struct *vma)
{
	struct mdw_mem *m = dbuf->priv;
	struct mdw_mem_dma *mdbuf = m->priv;
	struct sg_table *table = &mdbuf->sgt;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret = 0;

	mdw_mem_debug("%p/%p\n", m, mdbuf);

	if (mdbuf->uncached)
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
	mdw_mem_dma_show(mdbuf);
	return ret;
}

static int mdw_mem_dma_bind(void *session, struct mdw_mem *m)
{
	return 0;
}

static void mdw_mem_dma_unbind(void *session, struct mdw_mem *m)
{
}

static struct dma_buf_ops mdw_dmabuf_ops = {
	.attach = mdw_dmabuf_attach,
	.detach = mdw_dmabuf_detach,
	.map_dma_buf = mdw_dmabuf_map_dma,
	.unmap_dma_buf = mdw_dmabuf_unmap_dma,
	.vmap = mdw_dmabuf_vmap,
	.mmap = mdw_dmabuf_mmap,
	.begin_cpu_access = mdw_dmabuf_invalidate,
	.end_cpu_access = mdw_dmabuf_flush,
	.release = mdw_dmabuf_release,
};

int mdw_mem_dma_alloc(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = NULL;
	int ret = 0;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct device *dev;
	void *kva;
	bool uncached = true;

	/* alloc mdw dma-buf container */
	mdbuf = kzalloc(sizeof(*mdbuf), GFP_KERNEL);
	if (!mdbuf)
		return -ENOMEM;

	mutex_init(&mdbuf->mtx);
	INIT_LIST_HEAD(&mdbuf->attachments);

	/* alloc buffer by dma */
	mdbuf->dma_size = PAGE_ALIGN(mem->size);
	mdw_mem_debug("alloc mem(0x%llx)(%u/%u)\n",
		(uint64_t) mem, mem->size, mdbuf->dma_size);

	if (mem->flags & F_MDW_MEM_HIGHADDR) {
		dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_DATA);
		if (dev)
			mdw_mem_debug("data %x dev %s\n",
				mem->flags, dev_name(dev));
	} else {
		dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
		if (dev)
			mdw_mem_debug("code %x dev %s\n",
				mem->flags, dev_name(dev));
	}
	if (mem->flags & F_MDW_MEM_CACHEABLE)
		uncached = false;
	else
		uncached = true;

	if (!dev) {
		mdw_drv_err("dev invalid\n");
		ret = -ENOMEM;
		goto free_mdw_dbuf;
	}

	kva = vzalloc(mdbuf->dma_size);
	if (!kva) {
		ret = -ENOMEM;
		goto free_mdw_dbuf;
	}

	mdbuf->buf = kva;

	if (mdw_mem_dma_allocate_sgt(
			kva, mdbuf->dma_size, &mdbuf->sgt,
			uncached, &mdbuf->vaddr)) {
		mdw_drv_err("get sgt: failed\n");
		ret = -ENOMEM;
		goto free_buf;
	}

	/* export as dma-buf */
	exp_info.ops = &mdw_dmabuf_ops;
	exp_info.size = mdbuf->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = mem;

	mem->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(mem->dbuf)) {
		mdw_drv_err("dma_buf_export Fail\n");
		ret = -ENOMEM;
		goto free_sgt;
	}

	mdbuf->mmem = mem;
	mdbuf->mem_dev = dev;
	mdbuf->size = mem->size;
	mdbuf->uncached = uncached;
	/* access data to mdw_mem */
	mem->vaddr = mdbuf->vaddr;
	mem->mdev = dev;
	mem->priv = mdbuf;
	mem->bind = mdw_mem_dma_bind;
	mem->unbind = mdw_mem_dma_unbind;

	if (uncached) {
		dma_sync_sgtable_for_device(mdbuf->mem_dev,
			&mdbuf->sgt, DMA_TO_DEVICE);
	}

	mdw_mem_dma_show(mdbuf);
	goto out;

free_sgt:
	mdw_mem_dma_free_sgt(&mdbuf->sgt);
free_buf:
	vfree(kva);
free_mdw_dbuf:
	kfree(mdbuf);
out:
	return ret;
}
