// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>


#include "mdw_mem_rsc.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

struct mdw_mem_dma_attachment {
	struct sg_table sgt;
	struct device *dev;
	struct list_head node;
};

struct mdw_mem_dma {
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	uint32_t dma_size;
	uint32_t size;
	union {
		struct {
			int handle;
			void *vaddr;
			struct list_head attachments;
		} a;
		struct {
			struct dma_buf_attachment *attach;
			struct sg_table *sgt;
		} m;
	};

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
	mdw_mem_debug("mem(%p/%d/0x%llx/0x%x/0x%llx/0x%x)(%d)\n", \
	d->mmem, d->mmem->handle, (uint64_t)d->mmem->vaddr, d->mmem->size, \
	d->dma_addr, d->dma_size, current->pid)


static struct mdw_mem_dma_mgr mdmgr;

static int mdw_dmabuf_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_dma_attachment *a = NULL;
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	int ret = 0;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = dma_get_sgtable(attach->dev, &a->sgt,
		mdbuf->a.vaddr, mdbuf->dma_addr, mdbuf->dma_size);
	if (ret < 0) {
		mdw_drv_err("failed to get sgtable\n");
		kfree(a);
		return ret;
	}
	mdw_mem_debug("attach dev %s\n", dev_name(attach->dev));
	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	a->dev = attach->dev;
	attach->priv = a;
	mutex_lock(&mdbuf->mtx);
	list_add(&a->node, &mdbuf->a.attachments);
	mutex_unlock(&mdbuf->mtx);

	return 0;
}

static void mdw_dmabuf_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	struct mdw_mem_dma *mdbuf = dbuf->priv;

	mdw_mem_debug("attach dev %s\n", dev_name(attach->dev));
	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	mutex_lock(&mdbuf->mtx);
	list_del(&a->node);
	mutex_unlock(&mdbuf->mtx);
	sg_free_table(&a->sgt);
	kfree(a);
}

static struct sg_table *mdw_dmabuf_map_dma(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	struct mdw_mem_dma_attachment *a = attach->priv;
	struct sg_table *t = NULL;
	int ret = 0;

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);

	t = &a->sgt;
	ret = dma_map_sgtable(attach->dev, &a->sgt, dir, 0);
	if (ret)
		t = ERR_PTR(ret);

	return t;
}

static void mdw_dmabuf_unmap_dma(struct dma_buf_attachment *attach,
				    struct sg_table *sgt,
				    enum dma_data_direction dir)
{
	dma_unmap_sgtable(attach->dev, sgt, dir, 0);
}

static void *mdw_dmabuf_vmap(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;

	mdw_mem_debug("dmabuf vmap:%p\n", mdbuf->a.vaddr);
	return mdbuf->a.vaddr;
}

static void mdw_dmabuf_release(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	struct mdw_mem *m = mdbuf->mmem;

	mutex_lock(&mdmgr.mtx);
	list_del(&mdbuf->m_item);
	mutex_unlock(&mdmgr.mtx);

	mdw_mem_debug("mdbuf->mem_dev dev %s\n", dev_name(mdbuf->mem_dev));
	mdw_mem_dma_show(mdbuf);
	dma_free_coherent(mdbuf->mem_dev, mdbuf->dma_size,
		mdbuf->a.vaddr, mdbuf->dma_addr);

	kfree(mdbuf);
	m->release(m);
}

static int mdw_dmabuf_mmap(struct dma_buf *dbuf,
				  struct vm_area_struct *vma)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	int ret = 0;

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	mdw_mem_debug("mdbuf->mem_dev dev %s\n", dev_name(mdbuf->mem_dev));
	ret = dma_mmap_coherent(mdbuf->mem_dev, vma, mdbuf->a.vaddr,
				mdbuf->dma_addr, mdbuf->dma_size);
	if (ret)
		mdw_drv_err("mmap dma-buf error(%d)\n", ret);

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
		mdw_mem_debug("handle(%d) not belong APU\n", handle);
		return NULL;
	}

	return m->mmem;
}

int mdw_mem_dma_alloc(struct mdw_mem *mem, bool need_handle)
{
	struct mdw_mem_dma *mdbuf = NULL;
	int ret = 0;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct device *dev;

	/* alloc mdw dma-buf container */
	mdbuf = kzalloc(sizeof(*mdbuf), GFP_KERNEL);
	if (!mdbuf)
		return -ENOMEM;

	mutex_init(&mdbuf->mtx);
	INIT_LIST_HEAD(&mdbuf->a.attachments);

	/* alloc buffer by dma */
	mdbuf->dma_size = PAGE_ALIGN(mem->size);
	mdw_mem_debug("alloc mem(%p)(%u/%u)\n",
		mem, mem->size, mdbuf->dma_size);


	if (mem->flags & (1ULL << MDW_MEM_IOCTL_ALLOC_32BIT)) {
		dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
		mdw_mem_debug("Code %x dev %s\n", mem->flags, dev_name(dev));
	} else {
		dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_DATA);
		mdw_mem_debug("data %x dev %s\n", mem->flags, dev_name(dev));
	}

	if (!dev) {
		mdw_drv_err("Dev Invalid\n");
		ret = -ENOMEM;
		goto free_mdw_dbuf;
	}

	/* TODO, handle cache */
	mdbuf->a.vaddr = dma_alloc_coherent(dev, mdbuf->dma_size,
		&mdbuf->dma_addr, GFP_KERNEL);

	if (!mdbuf->a.vaddr)
		goto free_mdw_dbuf;

	/* export as dma-buf */
	exp_info.ops = &mdw_dmabuf_ops;
	exp_info.size = mdbuf->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = mdbuf;

	mdbuf->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(mdbuf->dbuf))
		goto free_dma_buf;

	mdbuf->dbuf->priv = mdbuf;
	mdbuf->mmem = mem;
	mdbuf->mem_dev = dev;
	mdbuf->size = mem->size;
	mdw_mem_debug("mdbuf->mem_dev %s\n", dev_name(mdbuf->mem_dev));

	/* access data to mdw_mem */
	mem->device_va = mdbuf->dma_addr;
	mem->vaddr = mdbuf->a.vaddr;
	mem->priv = mdbuf;
	mutex_lock(&mdmgr.mtx);
	list_add_tail(&mdbuf->m_item, &mdmgr.mems);
	mutex_unlock(&mdmgr.mtx);

	/* internal use, don't export fd */
	if (need_handle == false) {
		mem->handle = -1;
		goto out;
	}

	/* create fd from dma-buf */
	mdbuf->a.handle =  dma_buf_fd(mdbuf->dbuf,
		(O_RDWR | O_CLOEXEC) & ~O_ACCMODE);
	if (mdbuf->a.handle < 0) {
		dma_buf_put(mdbuf->dbuf);
		return -EINVAL;
	}
	mem->handle = mdbuf->a.handle;

out:
	mdw_mem_dma_show(mdbuf);

	return 0;

free_dma_buf:
	dma_free_coherent(dev, mdbuf->dma_size,
		mdbuf->a.vaddr, mdbuf->dma_addr);
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
	struct dma_buf *dbuf = NULL;
	struct mdw_mem_dma *mdbuf = NULL;

	dbuf = dma_buf_get(mem->handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("mem invalid handle(%p/%llu)\n", mem, mem->handle);
		return -EINVAL;
	}

	mdbuf = (struct mdw_mem_dma *)mem->priv;
	mdw_mem_dma_show(mdbuf);

	return 0;
}

int mdw_mem_dma_unmap(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = mem->priv;

	mdw_mem_dma_show(mdbuf);
	dma_buf_put(mdbuf->dbuf);

	return 0;
}

int mdw_mem_dma_import(struct mdw_mem *mem)
{
	int ret = 0;
	struct mdw_mem_dma *mdbuf = NULL;
	struct dma_buf *dbuf = NULL;
	struct device *dev;

	if (mem->device_va || mem->priv) {
		mdw_drv_err("mem(%p) already has dva(0x%llx/%p)\n",
			mem, mem->device_va, mem->priv);
		return -EINVAL;
	}

	dbuf = dma_buf_get(mem->handle);
	if (IS_ERR_OR_NULL(dbuf))
		return -ENOMEM;

	// Import Use 32 Bit Buffer
	dev = mdw_mem_rsc_get_dev(APUSYS_MEMORY_CODE);
	mdw_mem_debug("Code %x dev %s\n", mem->flags, dev_name(dev));

	if (!dev) {
		mdw_drv_err("Dev Invalid\n");
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

	mdw_mem_debug("dev %s\n", dev_name(dev));

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
		mdw_drv_err("can't get mem(%p) dva(0x%llx/%u)\n",
			mem, mdbuf->dma_addr, mdbuf->dma_size);
		goto unmap_dbuf;
	}
	mem->device_va = mdbuf->dma_addr;
	mem->priv = mdbuf;
	mutex_lock(&mdmgr.mtx);
	list_add_tail(&mdbuf->m_item, &mdmgr.mems);
	mutex_unlock(&mdmgr.mtx);

	mdw_mem_dma_show(mdbuf);

	goto out;

unmap_dbuf:
	dma_buf_unmap_attachment(mdbuf->m.attach,
		mdbuf->m.sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(mdbuf->dbuf, mdbuf->m.attach);
free_mdbuf:
	kfree(mdbuf);
put_dbuf:
	dma_buf_put(dbuf);
out:
	return ret;
}

int mdw_mem_dma_unimport(struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = NULL;

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

	mem->device_va = 0;
	mem->priv = NULL;
	dma_buf_unmap_attachment(mdbuf->m.attach,
		mdbuf->m.sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(mdbuf->dbuf, mdbuf->m.attach);
	dma_buf_put(mdbuf->dbuf);
	kfree(mdbuf);

	return 0;
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
	uint64_t iova;

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


int mdw_mem_dma_init(void)
{
	mutex_init(&mdmgr.mtx);
	INIT_LIST_HEAD(&mdmgr.mems);

	return 0;
}

void mdw_mem_dma_deinit(void)
{
}
