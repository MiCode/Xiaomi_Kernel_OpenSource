// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#include "mdw_cmn.h"
#include "mdw_mem.h"

struct mdw_mem_dma {
	struct mdw_mem *parent;
	struct sg_table *dma_sgt;
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	void *vaddr;
	unsigned int dma_size;
	int handle;

	struct mdw_device *mdev;
};

static int mdw_dmabuf_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	attach->priv = dbuf->priv;
	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	return 0;
}

static void mdw_dmabuf_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	attach->priv = NULL;
	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
}

static struct sg_table *mdw_dmabuf_map_dma(struct dma_buf_attachment *attach,
	enum dma_data_direction dma_dir)
{
	struct mdw_mem_dma *mdbuf = attach->priv;

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	if (!mdbuf)
		return NULL;

	dma_buf_get(mdbuf->handle);

	return mdbuf->dma_sgt;
}

static void mdw_dmabuf_unmap_dma(struct dma_buf_attachment *attach,
				    struct sg_table *sgt,
				    enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
	struct mdw_mem_dma *mdbuf = attach->priv;

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	dma_buf_put(mdbuf->dbuf);
}

static void *mdw_dmabuf_vmap(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;

	mdw_mem_debug("dmabuf vmap:%p\n", mdbuf->vaddr);
	return mdbuf->vaddr;
}

#ifdef MDW_UP_POC_SUPPORT
static void *mdw_dmabuf_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	mdw_drv_warn("not support\n");
	return NULL;
}
#endif

static void mdw_dmabuf_release(struct dma_buf *dbuf)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	/* drop reference obtained in dma_dc_get_dmabuf */

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);
	dma_free_coherent(&mdbuf->mdev->pdev->dev, mdbuf->dma_size,
		mdbuf->vaddr, mdbuf->dma_addr);
	kfree(mdbuf->dma_sgt);
	kfree(mdbuf);
}

static int mdw_dmabuf_mmap(struct dma_buf *dbuf,
				  struct vm_area_struct *vma)
{
	struct mdw_mem_dma *mdbuf = dbuf->priv;
	int ret = 0;

	mdw_mem_debug("%s: %d\n", __func__, __LINE__);

	ret = dma_mmap_coherent(&mdbuf->mdev->pdev->dev, vma, mdbuf->vaddr,
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
#ifdef MDW_UP_POC_SUPPORT
	.map = mdw_dmabuf_kmap,
	.map_atomic = mdw_dmabuf_kmap,
#endif
	.mmap = mdw_dmabuf_mmap,
	.release = mdw_dmabuf_release,
};

int mdw_mem_dma_alloc(struct mdw_fpriv *mpriv, struct mdw_mem *mem)
{
	struct mdw_device *mdev = mpriv->mdev;
	struct mdw_mem_dma *mdbuf = NULL;
	int ret = 0;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* alloc mdw dma-buf container */
	mdbuf = kzalloc(sizeof(*mdbuf), GFP_KERNEL);
	if (!mdbuf)
		return -ENOMEM;

	mdbuf->parent = mem;
	mdbuf->mdev = mdev;

	/* alloc buffer by dma */
	mdbuf->dma_size = PAGE_ALIGN(mem->size);
	mdw_mem_debug("alloc mem(%u/%u)\n", mem->size, mdbuf->dma_size);
	/* TODO, handle cache */
	mdbuf->vaddr = dma_alloc_coherent(&mdev->pdev->dev, mdbuf->dma_size,
		&mdbuf->dma_addr, GFP_KERNEL);

	if (!mdbuf->vaddr)
		goto free_mdw_dma_buf;

	mdbuf->dma_sgt = kzalloc(sizeof(*mdbuf->dma_sgt), GFP_KERNEL);
	if (!mdbuf->dma_sgt)
		goto free_dma_buf;

	ret = dma_get_sgtable(&mdev->pdev->dev, mdbuf->dma_sgt, mdbuf->vaddr,
		mdbuf->dma_addr, mdbuf->dma_size);
	if (ret)
		goto free_sgt;

	/* export as dma-buf */
	exp_info.ops = &mdw_dmabuf_ops;
	exp_info.size = mdbuf->dma_size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = mdbuf;

	mdbuf->dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(mdbuf->dbuf))
		goto free_sgt;

	mdbuf->dbuf->priv = mdbuf;

	/* create fd from dma-buf */
	mdbuf->handle =  dma_buf_fd(mdbuf->dbuf,
		(O_RDWR | O_CLOEXEC) & ~O_ACCMODE);
	if (mdbuf->handle < 0)
		goto free_sgt;

	/* access data to mdw_mem */
	mem->priv = mdbuf;
	//mem->device_va = mdbuf->dma_addr;
	mem->handle = mdbuf->handle;
	mem->vaddr = mdbuf->vaddr;

	mdw_mem_debug("alloc mem(%p/0x%llx) done\n",
		mem->vaddr, mem->device_va);

	return 0;

free_sgt:
	kfree(mdbuf->dma_sgt);
free_dma_buf:
	dma_free_coherent(&mdbuf->mdev->pdev->dev, mdbuf->dma_size,
		mdbuf->vaddr, mdbuf->dma_addr);
free_mdw_dma_buf:
	kfree(mdbuf);

	return ret;
}

void mdw_mem_dma_free(struct mdw_fpriv *mpriv, struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = (struct mdw_mem_dma *)mem->priv;

	dma_buf_put(mdbuf->dbuf);
}

int mdw_mem_dma_map(struct mdw_fpriv *mpriv, struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = (struct mdw_mem_dma *)mem->priv;

	/* TODO */
	dma_buf_get(mdbuf->handle);
	mem->device_va = mdbuf->dma_addr;

	return 0;
}

int mdw_mem_dma_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *mem)
{
	struct mdw_mem_dma *mdbuf = (struct mdw_mem_dma *)mem->priv;

	mem->device_va = 0;
	/* TODO */
	dma_buf_put(mdbuf->dbuf);
	return 0;
}
