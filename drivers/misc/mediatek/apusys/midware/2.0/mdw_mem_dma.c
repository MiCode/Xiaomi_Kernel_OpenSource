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
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	uint32_t dma_size;
	uint32_t size;
	struct {
		int handle;
		void *vaddr;
		struct list_head attachments;
		struct sg_table sgt;
		void *buf;
	} a;
	struct {
		struct dma_buf_attachment *attach;
		struct sg_table *sgt;
	} m;
	bool uncached;
	struct kref attach_ref;

	struct mutex mtx;

	struct mdw_mem *mmem;
	struct device *mem_dev;
	struct list_head m_item;
};

struct mdw_mem_dma_mgr {
	struct list_head mems;
	struct mutex mtx;
};

#define mdw_mem_dma_show(d) \
	mdw_mem_debug("mem(0x%llx/%d/0x%llx/0x%x/0x%llx/0x%x/%d/%d)(%d)\n", \
	(uint64_t) d->mmem, d->mmem->handle, (uint64_t)d->mmem->vaddr, d->mmem->size, \
	d->dma_addr, d->dma_size, d->mmem->need_handle, file_count(d->dbuf->file), current->pid)


static struct mdw_mem_dma_mgr mdmgr;

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
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);
	mdw_mem_debug("start p: 0x%llx buf: 0x%llx\n",
			(uint64_t) p, (uint64_t) buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kfree(pages);
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
	kfree(pages);
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
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	int ret = 0;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = mdw_mem_dma_dup_sg(&mdbuf->a.sgt);
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
	list_add(&a->node, &mdbuf->a.attachments);
	mutex_unlock(&mdbuf->mtx);

	mdw_mem_dma_show(mdbuf);

	return ret;
}

static void mdw_dmabuf_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	struct mdw_mem_dma *mdbuf = dbuf->priv;

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

	return table;
}

static void mdw_dmabuf_unmap_dma(struct dma_buf_attachment *attach,
				    struct sg_table *sgt,
				    enum dma_data_direction dir)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	int attr = attach->dma_map_attrs;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	a->mapped = false;
	dma_unmap_sgtable(attach->dev, sgt, dir, attr);
}

static void *mdw_dmabuf_vmap(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;

	mdw_mem_debug("dmabuf vmap: 0x%llx\n", (uint64_t) mdbuf->a.vaddr);
	return mdbuf->a.vaddr;
}

static void mdw_dmabuf_release(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	struct mdw_mem *m = mdbuf->mmem;

	mdw_mem_dma_show(mdbuf);

	mutex_lock(&mdmgr.mtx);
	list_del(&mdbuf->m_item);
	mutex_unlock(&mdmgr.mtx);

	if (m->type != MDW_MEM_TYPE_IMPORT) {
		mdw_mem_dma_free_sgt(&mdbuf->a.sgt);
		vunmap(mdbuf->a.vaddr);
		kvfree(mdbuf->a.buf);
	}

	kfree(mdbuf);
	m->release(m);
}

static int mdw_dmabuf_mmap(struct dma_buf *dbuf,
				  struct vm_area_struct *vma)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	struct sg_table *table = &mdbuf->a.sgt;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret = 0;

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


static struct dma_buf_ops mdw_dmabuf_ops = {
	.attach = mdw_dmabuf_attach,
	.detach = mdw_dmabuf_detach,
	.map_dma_buf = mdw_dmabuf_map_dma,
	.unmap_dma_buf = mdw_dmabuf_unmap_dma,
	.vmap = mdw_dmabuf_vmap,
	.mmap = mdw_dmabuf_mmap,
	.release = mdw_dmabuf_release,
};

struct mdw_mem *mdw_mem_dma_get(int handle)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_mem_dma *m = NULL, *pos = NULL, *tmp = NULL;

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get dma_buf handle(%d) fail\n", handle);
		return NULL;
	}

	mutex_lock(&mdmgr.mtx);
	list_for_each_entry_safe(pos, tmp, &mdmgr.mems, m_item) {
		if (pos->dbuf == dbuf) {
			m = pos;
			break;
		}
	}
	mutex_unlock(&mdmgr.mtx);

	dma_buf_put(dbuf);
	if (!m) {
		mdw_mem_debug("handle(%d) not belong to apu\n", handle);
		return NULL;
	}

	return m->mmem;
}



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
	INIT_LIST_HEAD(&mdbuf->a.attachments);

	/* alloc buffer by dma */
	mdbuf->dma_size = PAGE_ALIGN(mem->size);
	mdw_mem_debug("alloc mem(0x%llx)(%u/%u)\n",
		(uint64_t) mem, mem->size, mdbuf->dma_size);

	if (mem->flags & (1ULL << MDW_MEM_IOCTL_ALLOC_HIGHADDR)) {
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
	if (mem->flags & (1ULL << MDW_MEM_IOCTL_ALLOC_CACHEABLE))
		uncached = true;
	else
		uncached = true;


	if (!dev) {
		mdw_drv_err("dev invalid\n");
		ret = -ENOMEM;
		goto free_mdw_dbuf;
	}

	kva = kvzalloc(mdbuf->dma_size, GFP_KERNEL);
	if (!kva) {
		ret = -ENOMEM;
		goto free_mdw_dbuf;
	}

	mdbuf->a.buf = kva;

	if (mdw_mem_dma_allocate_sgt(
			kva, mdbuf->dma_size, &mdbuf->a.sgt, uncached, &mdbuf->a.vaddr)) {
		mdw_drv_err("get sgt: failed\n");
		ret = -ENOMEM;
		goto free_buf;
	}

	/* export as dma-buf */
	exp_info.ops = &mdw_dmabuf_ops;
	exp_info.size = mdbuf->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = mdbuf;

	mdbuf->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(mdbuf->dbuf)) {
		mdw_drv_err("dma_buf_export Fail\n");
		ret = -ENOMEM;
		goto free_sgt;
	}

	mdbuf->dbuf->priv = mdbuf;
	mdbuf->mmem = mem;
	mdbuf->mem_dev = dev;
	mdbuf->size = mem->size;
	mdbuf->uncached = uncached;
	/* access data to mdw_mem */
	mem->device_va = mdbuf->dma_addr;
	mem->vaddr = mdbuf->a.vaddr;
	mem->priv = mdbuf;
	mutex_lock(&mdmgr.mtx);
	list_add_tail(&mdbuf->m_item, &mdmgr.mems);
	mutex_unlock(&mdmgr.mtx);

	if (uncached)
		dma_sync_sgtable_for_device(mdbuf->mem_dev, &mdbuf->a.sgt, DMA_TO_DEVICE);

	/* internal use, don't export fd */
	if (mem->need_handle == false) {
		mem->handle = -1;
		goto out;
	}

	/* create fd from dma-buf */
	mdbuf->a.handle =  dma_buf_fd(mdbuf->dbuf,
		(O_RDWR | O_CLOEXEC) & ~O_ACCMODE);
	if (mdbuf->a.handle < 0) {
		ret = -EINVAL;
		mdw_drv_err("dma_buf_fd Fail\n");
		dma_buf_put(mdbuf->dbuf);
		return ret;
	}
	mem->handle = mdbuf->a.handle;

out:
	mdw_mem_dma_show(mdbuf);

	return ret;

free_sgt:
	mdw_mem_dma_free_sgt(&mdbuf->a.sgt);
free_buf:
	kvfree(kva);
free_mdw_dbuf:
	kfree(mdbuf);

	return ret;
}

int mdw_mem_dma_free(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = mem->priv;

	mdw_mem_dma_show(mdbuf);
	dma_buf_put(mdbuf->dbuf);

	return 0;
}

int mdw_mem_dma_map(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = NULL;
	int ret = 0;

	mdbuf = (struct mdw_mem_dma *)mem->priv;

	if (IS_ERR_OR_NULL(mdbuf->dbuf)) {
		mdw_drv_err("mem dbuf invalid (0x%llx)\n", (uint64_t) mem);
		return -EINVAL;
	}
	get_dma_buf(mdbuf->dbuf);


	// Only Attach after First Map
	if (kref_read(&mdbuf->attach_ref)) {
		kref_get(&mdbuf->attach_ref);
		goto out;
	} else
		kref_init(&mdbuf->attach_ref);

	mdbuf->m.attach = dma_buf_attach(mdbuf->dbuf, mdbuf->mem_dev);
	if (IS_ERR(mdbuf->m.attach)) {
		ret = PTR_ERR(mdbuf->m.attach);
		mdw_drv_err("dma_buf_attach failed: %d\n", ret);
		goto put_dbuf;
	}

	mdbuf->m.sgt = dma_buf_map_attachment(mdbuf->m.attach,
		DMA_BIDIRECTIONAL);
	if (IS_ERR(mdbuf->m.sgt)) {
		ret = PTR_ERR(mdbuf->m.sgt);
		mdw_drv_err("dma_buf_map_attachment failed: %d\n", ret);
		goto detach_dbuf;
	}

	mdbuf->dma_addr = sg_dma_address(mdbuf->m.sgt->sgl);
	mdbuf->dma_size = sg_dma_len(mdbuf->m.sgt->sgl);
	if (!mdbuf->dma_addr || !mdbuf->dma_size) {
		mdw_drv_err("can't get mem(0x%llx) dva(0x%llx/%u)\n",
			(uint64_t) mem, mdbuf->dma_addr, mdbuf->dma_size);
		ret = -ENOMEM;
		goto unmap_dbuf;
	}

	mem->device_va = mdbuf->dma_addr;
	mem->dva_size = mdbuf->dma_size;
out:
	mdw_mem_dma_show(mdbuf);

	return ret;

unmap_dbuf:
	dma_buf_unmap_attachment(mdbuf->m.attach,
		mdbuf->m.sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(mdbuf->dbuf, mdbuf->m.attach);
put_dbuf:
	dma_buf_put(mdbuf->dbuf);

	return ret;
}
static void mdw_mem_dma_detach(struct kref *ref)
{
	struct mdw_mem_dma *mdbuf;

	mdbuf =	container_of(ref, struct mdw_mem_dma, attach_ref);

	mdw_mem_dma_show(mdbuf);

	dma_buf_unmap_attachment(mdbuf->m.attach,
		mdbuf->m.sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(mdbuf->dbuf, mdbuf->m.attach);

}


int mdw_mem_dma_unmap(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = mem->priv;
	int ret = 0;

	mdw_mem_dma_show(mdbuf);
	// Only Detach after last Map
	kref_put(&mdbuf->attach_ref, mdw_mem_dma_detach);

	dma_buf_put(mdbuf->dbuf);

	return ret;
}

int mdw_mem_dma_import(struct mdw_mem *mem)
{
	int ret = 0;
	struct mdw_mem_dma *mdbuf = NULL;
	struct dma_buf *dbuf = NULL;
	struct device *dev;

	if (mem->device_va || mem->priv) {
		mdw_drv_err("mem(0x%llx) already has dva(0x%llx/0x%llx)\n",
			(uint64_t) mem, mem->device_va, (uint64_t) mem->priv);
		return -EINVAL;
	}

	dbuf = dma_buf_get(mem->handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("dma_buf_get fail\n");
		return -ENOMEM;
	}

	// Import Use 32 Bit Buffer
	dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
	if (dev) {
		mdw_mem_debug("code %x dev %s\n", mem->flags, dev_name(dev));
	} else {
		mdw_drv_err("dev invalid\n");
		ret = -ENOMEM;
		goto put_dbuf;
	}

	mdbuf = kzalloc(sizeof(*mdbuf), GFP_KERNEL);
	if (!mdbuf) {
		ret = -ENOMEM;
		goto put_dbuf;
	}



	mdbuf->mmem = mem;
	mdbuf->dbuf = dbuf;
	mdbuf->mem_dev = dev;
	mdbuf->size = mem->size;

	// Only Attach after First Map
	if (kref_read(&mdbuf->attach_ref)) {
		kref_get(&mdbuf->attach_ref);
		goto out;
	} else
		kref_init(&mdbuf->attach_ref);


	mdbuf->m.attach = dma_buf_attach(mdbuf->dbuf, dev);
	if (IS_ERR(mdbuf->m.attach)) {
		ret = PTR_ERR(mdbuf->m.attach);
		mdw_drv_err("dma_buf_attach failed: %d\n", ret);
		goto free_mdbuf;
	}

	mdbuf->m.sgt = dma_buf_map_attachment(mdbuf->m.attach,
		DMA_BIDIRECTIONAL);
	if (IS_ERR(mdbuf->m.sgt)) {
		ret = PTR_ERR(mdbuf->m.sgt);
		mdw_drv_err("dma_buf_map_attachment failed: %d\n", ret);
		goto detach_dbuf;
	}

	mdbuf->dma_addr = sg_dma_address(mdbuf->m.sgt->sgl);
	mdbuf->dma_size = sg_dma_len(mdbuf->m.sgt->sgl);
	if (!mdbuf->dma_addr || !mdbuf->dma_size) {
		mdw_drv_err("can't get mem(0x%llx) dva(0x%llx/%u)\n",
			(uint64_t) mem, mdbuf->dma_addr, mdbuf->dma_size);
		ret = -ENOMEM;
		goto unmap_dbuf;
	}
	mem->device_va = mdbuf->dma_addr;
	mem->priv = mdbuf;
	mutex_lock(&mdmgr.mtx);
	list_add_tail(&mdbuf->m_item, &mdmgr.mems);
	mutex_unlock(&mdmgr.mtx);

out:
	mdw_mem_dma_show(mdbuf);
	return ret;

unmap_dbuf:
	dma_buf_unmap_attachment(mdbuf->m.attach,
		mdbuf->m.sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(mdbuf->dbuf, mdbuf->m.attach);
free_mdbuf:
	kfree(mdbuf);
put_dbuf:
	dma_buf_put(dbuf);

	return ret;
}

int mdw_mem_dma_unimport(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(mem->priv))
		return -EINVAL;

	mdbuf = (struct mdw_mem_dma *)mem->priv;
	mdw_mem_dma_show(mdbuf);

	if (IS_ERR_OR_NULL(mdbuf->m.attach) ||
		IS_ERR_OR_NULL(mdbuf->m.sgt))
		return -EINVAL;

	mutex_lock(&mdmgr.mtx);
	list_del(&mdbuf->m_item);
	mutex_unlock(&mdmgr.mtx);

	// Only Detach after last Map
	kref_put(&mdbuf->attach_ref, mdw_mem_dma_detach);

	mem->device_va = 0;
	mem->priv = NULL;

	dma_buf_put(mdbuf->dbuf);
	kfree(mdbuf);

	return ret;
}
int mdw_mem_dma_flush(struct mdw_mem *mem)
{
	int ret = 0;
	struct mdw_mem_dma *mdbuf = mem->priv;

	if (!mdbuf->a.vaddr) {
		mdw_drv_err("mdbuf vaddr NULL\n");
		ret = -EINVAL;
		goto out;
	}


	if (!mdbuf->uncached) {
		dma_sync_sgtable_for_device(mdbuf->mem_dev, &mdbuf->a.sgt, DMA_TO_DEVICE);
		mdw_drv_info("mem flush(0x%llx) sgt (0x%llx)\n",
				(uint64_t) mem,  mdbuf->a.sgt);
	}

	mdw_mem_dma_show(mdbuf);

out:
	return ret;

}

int mdw_mem_dma_invalidate(struct mdw_mem *mem)
{
	int ret = 0;
	struct mdw_mem_dma *mdbuf = mem->priv;

	if (!mdbuf->a.vaddr) {
		mdw_drv_err("mdbuf vaddr NULL\n");
		ret = -EINVAL;
		goto out;
	}


	if (!mdbuf->uncached) {
		dma_sync_sgtable_for_cpu(mdbuf->mem_dev, &mdbuf->a.sgt, DMA_FROM_DEVICE);
		mdw_drv_info("mem invalidate(0x%llx) sgt (0x%llx)\n",
				(uint64_t) mem,  mdbuf->a.sgt);
	}

	mdw_mem_dma_show(mdbuf);

out:
	return ret;

}


uint64_t mdw_mem_dma_query_kva(uint64_t iova)
{
	struct mdw_mem_dma *pos = NULL, *tmp = NULL;
	struct mdw_mem *m = NULL;
	uint64_t kva = 0;

	mutex_lock(&mdmgr.mtx);
	list_for_each_entry_safe(pos, tmp, &mdmgr.mems, m_item) {
		m = pos->mmem;
		if (iova >= m->device_va &&
			iova < m->device_va + m->size) {
			if (m->vaddr == NULL)
				break;

			kva = (uint64_t)m->vaddr + (iova - m->device_va);
			mdw_mem_debug("query kva (0x%llx->0x%llx)\n",
				iova, kva);
		}
	}
	mutex_unlock(&mdmgr.mtx);

	return kva;
}

uint64_t mdw_mem_dma_query_iova(uint64_t kva)
{
	struct mdw_mem_dma *pos = NULL, *tmp = NULL;
	struct mdw_mem *m = NULL;
	uint64_t iova = 0;

	mutex_lock(&mdmgr.mtx);
	list_for_each_entry_safe(pos, tmp, &mdmgr.mems, m_item) {
		m = pos->mmem;
		if (kva >= (uint64_t)m->vaddr &&
			kva < (uint64_t)m->vaddr + m->size) {
			if (!m->device_va)
				break;

			iova = m->device_va + (kva - (uint64_t)m->vaddr);
			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, iova);
		}
	}
	mutex_unlock(&mdmgr.mtx);

	return iova;
}

struct mdw_mem *mdw_mem_dma_query_mem(uint64_t kva)
{
	struct mdw_mem_dma *pos = NULL, *tmp = NULL;
	struct mdw_mem *m = NULL;
	struct mdw_mem *target = NULL;

	mutex_lock(&mdmgr.mtx);
	list_for_each_entry_safe(pos, tmp, &mdmgr.mems, m_item) {
		m = pos->mmem;
		if (kva >= (uint64_t)m->vaddr &&
			kva < (uint64_t)m->vaddr + m->size) {

			target = m;
			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, (uint64_t) target);
		}
	}
	mutex_unlock(&mdmgr.mtx);

	return target;
}


int mdw_mem_dma_init(void)
{
	mutex_init(&mdmgr.mtx);
	INIT_LIST_HEAD(&mdmgr.mems);

	return 0;
}

void mdw_mem_dma_deinit(void)
{
}
