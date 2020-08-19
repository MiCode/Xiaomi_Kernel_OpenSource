// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_mem_cmn.h"
#include "mdw_import.h"

struct mdw_mem_dev {
	struct device *dev;
	spinlock_t lock;
};

static struct mdw_mem_dev md;

#define APU_PAGE_SIZE PAGE_SIZE

static int check_arg(struct apusys_kmem *mem);

#define APU_MEM_DBG(func, m) \
	func("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx/%p/%p)\n", \
	m->fd, m->uva, m->iova, \
	m->iova_size ? (m->iova + m->iova_size - 1) : 0, \
	m->size, m->iova_size, m->d, m->kva, m->attach, m->sgt)

#define apu_mem_dbg(m) APU_MEM_DBG(mdw_mem_debug, m)
#define apu_mem_err(m) APU_MEM_DBG(mdw_drv_err, m)

static int check_arg(struct apusys_kmem *mem)
{
	int ret = 0;

	if (!mem)
		return -EINVAL;

	if ((mem->align != 0) &&
		((mem->align > APU_PAGE_SIZE) ||
		((APU_PAGE_SIZE % mem->align) != 0))) {
		mdw_drv_err("invalid alignment: %d\n", mem->align);
		return -EINVAL;
	}
	if (mem->cache > 1) {
		mdw_drv_err("invalid cache setting: %d\n", mem->cache);
		return -EINVAL;
	}
	if ((mem->iova_size % APU_PAGE_SIZE) != 0) {
		mdw_drv_err("invalid iova_size: 0x%x\n",
			mem->iova_size);
		return -EINVAL;
	}

	return ret;
}

static int mdw_mem_aosp_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&md.lock, flags);
	get_device(md.dev);
	spin_unlock_irqrestore(&md.lock, flags);

	return 0;
}

static void mdw_mem_aosp_exit(void)
{
	unsigned long flags;

	spin_lock_irqsave(&md.lock, flags);
	put_device(md.dev);
	spin_unlock_irqrestore(&md.lock, flags);
}

/**
 * Map fd to kva<br>
 * <b>Inputs</b><br>
 *   mem->fd: file descriptor shared from ION or DMA buffer.<br>
 * <b>Outputs</b><br>
 *   mem->kva: mapped kernel virtual address.<br>
 * @param[in,out] mem The apusys memory
 * @return 0: Success.<br>
 */
static int mdw_mem_aosp_map_kva(struct apusys_kmem *mem)
{
	void *buffer = NULL;
	struct dma_buf *d = NULL;
	int ret = 0;

	if (!md.dev)
		return -ENODEV;

	if (!mem)
		return -EINVAL;

	d = dma_buf_get(mem->fd);
	if (IS_ERR_OR_NULL(d))
		return -ENOMEM;

	ret = dma_buf_begin_cpu_access(d, DMA_FROM_DEVICE);
	if (ret) {
		mdw_drv_err("dma_buf_begin_cpu_access fail(%p)\n", d);
		goto err;
	}
	/* map kernel va*/
	buffer = dma_buf_vmap(d);
	if (IS_ERR_OR_NULL(buffer)) {
		mdw_drv_err("map kernel va fail(%p)\n",	d);
		ret = -ENOMEM;
		goto err;
	}

	mem->d = d;
	mem->kva = (uint64_t)buffer;
	apu_mem_dbg(mem);

err:
	if (ret)
		dma_buf_put(d);

	return ret;
}

/**
 * Map fd to iova.<br>
 * <b>Inputs</b><br>
 *   mem->fd: file descriptor shared from ION or DMA buffer.<br>
 * <b>Outputs</b><br>
 *   mem->d: dma buffer.<br>
 *   mem->attach: DMA buffer attachment.<br>
 *   mem->sgt: scatter list table.<br>
 *   mem->iova: mapped iova address.<br>
 *   mem->iova_size: mapped iova size.<br>
 * @param[in,out] mem The apusys memory
 * @return 0: Success.<br>
 */
static int mdw_mem_aosp_map_iova(struct apusys_kmem *mem)
{
	int ret = 0;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf *d = NULL;

	if (!md.dev)
		return -ENODEV;

	/* check argument */
	if (check_arg(mem))
		return -EINVAL;

	d = dma_buf_get(mem->fd);

	if (IS_ERR_OR_NULL(d))
		return -ENOMEM;

	attach = dma_buf_attach(d, md.dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		mdw_drv_err("dma_buf_attach failed: %d\n", ret);
		goto free_import;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		mdw_drv_err("dma_buf_map_attachment failed: %d\n", ret);
		goto free_attach;
	}

	mem->iova = sg_dma_address(sgt->sgl);
	mem->iova_size = sg_dma_len(sgt->sgl);
	mem->attach = attach;
	mem->sgt = sgt;
	mem->d = d;

	apu_mem_dbg(mem);
	return ret;

free_attach:
	dma_buf_detach(d, attach);
free_import:
	dma_buf_put(d);
	return ret;
}

/**
 * Unmap the iova of the apusys memory (mem->iova)
 * @param[in] mem The apusys memory
 * @return 0: Success
 */
static int mdw_mem_aosp_unmap_iova(struct apusys_kmem *mem)
{
	if (!md.dev)
		return -ENODEV;

	if (check_arg(mem) || (IS_ERR_OR_NULL(mem->d)) ||
		IS_ERR_OR_NULL(mem->attach) ||
		IS_ERR_OR_NULL(mem->sgt))
		return -EINVAL;

	apu_mem_dbg(mem);
	dma_buf_unmap_attachment(mem->attach, mem->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(mem->d, mem->attach);
	dma_buf_put(mem->d);
	apu_mem_dbg(mem);
	mem->sgt = NULL;
	mem->attach = NULL;

	return 0;
}

/**
 * Unmap the kernel virtual address of the apusys memory (mem->kva)
 * @param[in] mem The apusys memory
 * @return 0: Success
 */
static int mdw_mem_aosp_unmap_kva(struct apusys_kmem *mem)
{
	int ret = 0;

	if (!md.dev)
		return -ENODEV;

	if (check_arg(mem) || IS_ERR_OR_NULL(mem->d))
		return -EINVAL;

	dma_buf_vunmap(mem->d, (void *)mem->kva);

	ret = dma_buf_end_cpu_access(mem->d, DMA_FROM_DEVICE);
	if (ret)
		mdw_drv_err("dma_buf_end_cpu_access fail(%p): %d\n",
			mem->d, ret);

	dma_buf_put(mem->d);
	apu_mem_dbg(mem);

	return ret;

}

static int mdw_mem_aosp_flush(struct apusys_kmem *mem)
{
	int ret = 0;

	if (!md.dev)
		return -ENODEV;

	if (!mem || IS_ERR_OR_NULL(mem->d) || IS_ERR_OR_NULL(mem->sgt))
		return -EINVAL;

	dma_sync_sg_for_device(md.dev, mem->sgt->sgl,
			mem->sgt->nents, DMA_FROM_DEVICE);

	return ret;
}

static int mdw_mem_aosp_invalidate(struct apusys_kmem *mem)
{
	int ret = 0;

	if (!md.dev)
		return -ENODEV;

	if (!mem || IS_ERR_OR_NULL(mem->d) || IS_ERR_OR_NULL(mem->sgt))
		return -EINVAL;

	dma_sync_sg_for_cpu(md.dev, mem->sgt->sgl,
			mem->sgt->nents, DMA_FROM_DEVICE);

	return ret;
}

static struct mdw_mem_ops mem_aosp_ops = {
	.init = mdw_mem_aosp_init,
	.exit = mdw_mem_aosp_exit,
	.alloc = NULL,
	.free = NULL,
	.flush = mdw_mem_aosp_flush,
	.invalidate = mdw_mem_aosp_invalidate,
	.map_kva = mdw_mem_aosp_map_kva,
	.unmap_kva = mdw_mem_aosp_unmap_kva,
	.map_iova = mdw_mem_aosp_map_iova,
	.unmap_iova = mdw_mem_aosp_unmap_iova
};

struct mdw_mem_ops *mdw_mops_aosp(void)
{
	return &mem_aosp_ops;
}

static int mdw_mem_probe(struct platform_device *pdev)
{
	mdw_drv_info("%s: %s\n", __func__, pdev->name);
	md.dev = &pdev->dev;
	spin_lock_init(&md.lock);
	platform_set_drvdata(pdev, &md);
	dma_set_mask_and_coherent(md.dev, DMA_BIT_MASK(34));

	return 0;
}

static int mdw_mem_remove(struct platform_device *pdev)
{
	md.dev = NULL;
	return 0;
}

static const struct of_device_id mdw_mem_of_ids[] = {
	{.compatible = "mediatek,apu_iommu_data",},
	{}
};

static struct platform_driver mdw_mem_drv = {
	.probe   = mdw_mem_probe,
	.remove  = mdw_mem_remove,
	.driver  = {
	.name = "mdw_mem",
	.owner = THIS_MODULE,
	.of_match_table = mdw_mem_of_ids,
	}
};


int mdw_mem_drv_init(struct apusys_core_info *info)
{
	md.dev = NULL;

	if (!mdw_pwr_check())
		return -ENODEV;

	if (platform_driver_register(&mdw_mem_drv)) {
		mdw_drv_err("failed to register apusys memory driver");
		return -ENODEV;
	}

	return 0;
}

void mdw_mem_drv_exit(void)
{
	platform_driver_unregister(&mdw_mem_drv);
}


