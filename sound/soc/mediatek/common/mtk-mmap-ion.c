// SPDX-License-Identifier: GPL-2.0
//
// mtk-mmap-ion.c  --  Mediatek Audio MMAP Get ION Buffer
//
// Copyright (c) 2017 MediaTek Inc.
// Author: Chih-Hao Chang <chih-hao.chang@mediatek.com>

#include <linux/io.h>
#include "mtk-mmap-ion.h"

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>


static void *dl_alloc_vaddr;
static void *ul_alloc_vaddr;

static unsigned long dl_phy_addr;
static unsigned long ul_phy_addr;

static void *dl_vir_addr;
static void *ul_vir_addr;

static size_t dl_size;
static size_t ul_size;

static int dl_fd;
static int ul_fd;

struct device *dmabuf_dev;


int mtk_get_mmap_dl_fd(void)
{
	return dl_fd;
}
EXPORT_SYMBOL_GPL(mtk_get_mmap_dl_fd);

int mtk_get_mmap_ul_fd(void)
{
	return ul_fd;
}
EXPORT_SYMBOL_GPL(mtk_get_mmap_ul_fd);

void mtk_get_mmap_dl_buffer(unsigned long *phy_addr, void **vir_addr)
{
	*phy_addr = dl_phy_addr;
	*vir_addr = dl_vir_addr;
}
EXPORT_SYMBOL_GPL(mtk_get_mmap_dl_buffer);

void mtk_get_mmap_ul_buffer(unsigned long *phy_addr, void **vir_addr)
{
	*phy_addr = ul_phy_addr;
	*vir_addr = ul_vir_addr;
}
EXPORT_SYMBOL_GPL(mtk_get_mmap_ul_buffer);

static struct sg_table *mtk_map_dma_buf(struct dma_buf_attachment *attachment,
				 enum dma_data_direction dir)
{
	return NULL;
}

static void mtk_unmap_dma_buf(struct dma_buf_attachment *attachment,
				 struct sg_table *table,
				 enum dma_data_direction dir)
{
}

static void mtk_release(struct dma_buf *dmabuf)
{
}

static int mtk_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	int ret;
	void *vaddr = dmabuf->priv;
	unsigned long size = vma->vm_end - vma->vm_start;

	// check size
	size = (size > dmabuf->size) ? dmabuf->size : size;

	// nocached
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	ret = remap_pfn_range(vma, vma->vm_start, virt_to_pfn(vaddr),
				size, vma->vm_page_prot);

	dev_info(dmabuf_dev, "%s(), ret %d, %p, %p, size %ld %ld\n",
		__func__, ret, vaddr, vma->vm_start, size, dmabuf->size);

	return ret;
}

static const struct dma_buf_ops exp_dmabuf_ops = {
	.map_dma_buf = mtk_map_dma_buf,
	.unmap_dma_buf = mtk_unmap_dma_buf,
	.release = mtk_release,
	.mmap = mtk_mmap, // for user space
};

static struct dma_buf *mtk_dma_buf_init(struct device *dev, void **vaddr)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;

	dmabuf_dev = dev;

	if (*vaddr == NULL) {
		dev_info(dev, "%s(), kazlloc()\n", __func__);
		*vaddr = kzalloc(MMAP_BUFFER_SIZE, GFP_KERNEL);
		if (!(*vaddr)) {
			dev_info(dev, "%s() kzalloc fail!!\n", __func__);
			return NULL;
		}
	}

	exp_info.ops = &exp_dmabuf_ops;
	exp_info.size = MMAP_BUFFER_SIZE;
	exp_info.flags = O_RDWR;
	exp_info.priv = *vaddr;

	dev_info(dev, "%s(), priv %p %p\n", __func__, *vaddr, exp_info.priv);

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_info(dev, "%s() couldn't export dma_buf", __func__);
		kfree(vaddr);
		return NULL;
	}
	return dmabuf;
}

int mtk_exporter_init(struct device *dev)
{
	struct dma_buf *dmabuf;

	dev_info(dev, "%s(), dl_alloc_vaddr %p, ul_alloc_vaddr %p\n",
		 __func__, dl_alloc_vaddr, ul_alloc_vaddr);

	// for DL
	dmabuf = mtk_dma_buf_init(dev, &dl_alloc_vaddr);
	if (dmabuf != NULL) {
		//dev_info(dev, "+%s() for DL, addr %p, phy_addr %ld, size %zu, fd %d\n",
		//		__func__, dl_vir_addr, dl_phy_addr, dl_size, dl_fd);
		dl_vir_addr = dl_alloc_vaddr;
		dl_phy_addr = __pa(dl_vir_addr);
		dl_size     = MMAP_BUFFER_SIZE;
		dl_fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		dev_info(dev, "-%s() for DL, addr %p, phy_addr %ld, size %zu, fd %d\n",
			__func__, dl_vir_addr, dl_phy_addr, dl_size, dl_fd);
	}

	// for UL
	dmabuf = mtk_dma_buf_init(dev, &ul_alloc_vaddr);
	if (dmabuf != NULL) {
		//dev_info(dev, "+%s() for UL, addr %p, phy_addr %ld, size %zu, fd %d\n",
		//		__func__, ul_vir_addr, ul_phy_addr, ul_size, ul_fd);
		ul_vir_addr = ul_alloc_vaddr;
		ul_phy_addr = __pa(ul_vir_addr);
		ul_size     = MMAP_BUFFER_SIZE;
		ul_fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		dev_info(dev, "-%s() for UL, addr %p, phy_addr %ld, size %zu, fd %d\n",
			__func__, ul_vir_addr, ul_phy_addr, ul_size, ul_fd);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_exporter_init);


MODULE_DESCRIPTION("Mediatek Smart Phone PCM Operation");
MODULE_AUTHOR("Chih-Hao Chang <chih-hao.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

