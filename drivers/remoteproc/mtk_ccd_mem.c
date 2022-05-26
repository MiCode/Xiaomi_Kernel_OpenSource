// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/module.h>
#include <linux/fdtable.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <uapi/linux/mtk_ccd_controls.h>
#include <uapi/linux/dma-heap.h>

#define CCD_ALLOCATE_MAX_BUFFER_SIZE 0x20000000UL /*512MB*/

struct mtk_ccd_buf;

struct mtk_ccd_buf {
	struct device *dev;
	void *vaddr;
	unsigned long size;
	dma_addr_t dma_addr;

	/* DMABUF related */
	struct sg_table *dma_sgt;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *db_attach;
};

static struct mtk_ccd_buf *mtk_ccd_buf_alloc(
					   struct device *dev, unsigned long size)
{
	struct mtk_ccd_buf *buf;
	struct dma_heap *dma_heap;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	dma_heap = dma_heap_find("mtk_mm-uncached");
	if (!dma_heap) {
		pr_info("dma_heap find fail\n");
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->dbuf = dma_heap_buffer_alloc(dma_heap, size,
			O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(buf->dbuf)) {
		pr_info("dma_heap buffer alloc fail\n");
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->db_attach = dma_buf_attach(buf->dbuf, dev);
	if (IS_ERR(buf->db_attach)) {
		pr_info("dma_heap attach fail\n");
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->dma_sgt = dma_buf_map_attachment(buf->db_attach,
				DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->dma_sgt)) {
		pr_info("dma_heap map failed\n");
		dma_buf_detach(buf->dbuf, buf->db_attach);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->vaddr = dma_buf_vmap(buf->dbuf);
	buf->dma_addr = sg_dma_address(buf->dma_sgt->sgl);
	buf->dev = get_device(dev);
	buf->size = size;
	/* increase file count */
	if (buf->dbuf == NULL) {
		kfree(buf);
		pr_info("buf->dbuf IS Null\n");
		return ERR_PTR(-ENOMEM);
	}
	get_dma_buf(buf->dbuf);

	return buf;
}

static void mtk_ccd_buf_put(struct mtk_ccd_buf *buf)
{
	/* free va */
	if (buf->vaddr)
		dma_buf_vunmap(buf->dbuf, buf->vaddr);

	/* free iova */
	if (buf->db_attach && buf->dma_sgt)
		dma_buf_unmap_attachment(
			buf->db_attach, buf->dma_sgt, DMA_BIDIRECTIONAL);

	if (buf->dbuf && buf->db_attach)
		dma_buf_detach(buf->dbuf, buf->db_attach);

	/* decrease file count */
	dma_heap_buffer_free(buf->dbuf);

	put_device(buf->dev);
	kfree(buf);
}

static dma_addr_t mtk_ccd_buf_get_daddr(struct mtk_ccd_buf *buf)
{
	return buf->dma_addr;
}

static void *mtk_ccd_buf_get_vaddr(struct mtk_ccd_buf *buf)
{
	if (!buf->vaddr && buf->db_attach)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);

	return buf->vaddr;
}

struct dma_buf *mtk_ccd_get_buffer_dmabuf(struct mtk_ccd *ccd, void *mem_priv)
{
	struct mtk_ccd_buf *buf = mem_priv;

	if (ccd == NULL || buf == NULL) {
		pr_info("mtk_ccd or mem_priv is NULL\n");
		return NULL;
	}

	return buf->dbuf;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_buffer_dmabuf);

struct mtk_ccd_memory *mtk_ccd_mem_init(struct device *dev)
{
	struct mtk_ccd_memory *ccd_memory;

	ccd_memory = kzalloc(sizeof(*ccd_memory), GFP_KERNEL);
	if (!ccd_memory)
		return NULL;

	ccd_memory->dev = dev;
	ccd_memory->num_buffers = 0;
	mutex_init(&ccd_memory->mmap_lock);

	return ccd_memory;
}
EXPORT_SYMBOL_GPL(mtk_ccd_mem_init);

void mtk_ccd_mem_release(struct mtk_ccd *ccd)
{
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	kfree(ccd_memory);
}
EXPORT_SYMBOL_GPL(mtk_ccd_mem_release);

void *mtk_ccd_get_buffer(struct mtk_ccd *ccd,
			 struct mem_obj *mem_buff_data)
{
	void *va;
	dma_addr_t da;
	unsigned int buffers;
	struct mtk_ccd_buf *buf;
	struct mtk_ccd_mem *ccd_buffer;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	mem_buff_data->iova = 0;
	mem_buff_data->va = NULL;

	mutex_lock(&ccd_memory->mmap_lock);
	buffers = ccd_memory->num_buffers;
	if (mem_buff_data->len > CCD_ALLOCATE_MAX_BUFFER_SIZE ||
	    mem_buff_data->len == 0U ||
	    buffers >= MAX_NUMBER_OF_BUFFER) {
		dev_info(ccd_memory->dev,
			"%s: Failed: buffer len = %u num_buffers = %d !!\n",
			 __func__, mem_buff_data->len, buffers);
		mutex_unlock(&ccd_memory->mmap_lock);
		return ERR_PTR(-EINVAL);
	}

	ccd_buffer = &ccd_memory->bufs[buffers];
	buf = mtk_ccd_buf_alloc(ccd_memory->dev, mem_buff_data->len);
	ccd_buffer->mem_priv = buf;
	ccd_buffer->size = mem_buff_data->len;
	if (IS_ERR(ccd_buffer->mem_priv)) {
		dev_info(ccd_memory->dev, "%s: CCD buf allocation failed\n",
			__func__);
		mutex_unlock(&ccd_memory->mmap_lock);
		return ERR_PTR(-ENOMEM);
	}

	va = mtk_ccd_buf_get_vaddr(buf);
	da = mtk_ccd_buf_get_daddr(buf);
	mem_buff_data->iova = da;
	mem_buff_data->va = va;
	ccd_memory->num_buffers++;
	mutex_unlock(&ccd_memory->mmap_lock);

	dev_dbg(ccd_memory->dev,
		"Num_bufs = %d iova = %pad va = %p size = %d priv = %p\n",
		 ccd_memory->num_buffers, &mem_buff_data->iova,
		 mem_buff_data->va,
		 (unsigned int)ccd_buffer->size,
		 ccd_buffer->mem_priv);

	return ccd_buffer->mem_priv;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_buffer);

int mtk_ccd_put_buffer(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data)
{
	struct mtk_ccd_buf *buf;
	void *va;
	dma_addr_t da;
	int ret = -EINVAL;
	struct mtk_ccd_mem *ccd_buffer;
	unsigned int buffer, num_buffers, last_buffer;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	mutex_lock(&ccd_memory->mmap_lock);
	num_buffers = ccd_memory->num_buffers;
	if (num_buffers != 0U) {
		for (buffer = 0; buffer < num_buffers; buffer++) {
			ccd_buffer = &ccd_memory->bufs[buffer];
			buf = (struct mtk_ccd_buf *)ccd_buffer->mem_priv;
			va = mtk_ccd_buf_get_vaddr(buf);
			da = mtk_ccd_buf_get_daddr(buf);

			if (mem_buff_data->va == va &&
				mem_buff_data->len == ccd_buffer->size) {
				dev_dbg(ccd_memory->dev,
					"Free buff = %d iova = %pad va = %p, queue_num = %d\n",
					 buffer, &mem_buff_data->iova,
					 mem_buff_data->va,
					 num_buffers);
				mtk_ccd_buf_put(buf);
				last_buffer = num_buffers - 1U;
				if (last_buffer != buffer)
					ccd_memory->bufs[buffer] =
						ccd_memory->bufs[last_buffer];

				ccd_memory->bufs[last_buffer].mem_priv = NULL;
				ccd_memory->bufs[last_buffer].size = 0;
				ccd_memory->num_buffers--;
				ret = 0;
				break;
			}
		}
	}
	mutex_unlock(&ccd_memory->mmap_lock);

	if (ret != 0)
		dev_info(ccd_memory->dev,
			"Can not free memory va %p iova %pad len %u!\n",
			 mem_buff_data->va, &mem_buff_data->iova,
			 mem_buff_data->len);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_ccd_put_buffer);


int mtk_ccd_get_buffer_fd(struct mtk_ccd *ccd, void *mem_priv)
{
	struct mtk_ccd_buf *buf = mem_priv;
	int fd;

	if (ccd == NULL || buf == NULL) {
		pr_info("mtk_ccd or buf is NULL\n");
		return -EINVAL;
	}

	fd = dma_buf_fd(buf->dbuf,  O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		pr_info("mtk_ccd couldn't get fd from dma_buf\n");
		return -EINVAL;
	}

	/* increase file count */
	if (buf->dbuf == NULL) {
		pr_info("buf->dbuf IS Null\n");
		return -EINVAL;
	}
	get_dma_buf(buf->dbuf);

	return fd;
}
EXPORT_SYMBOL_GPL(mtk_ccd_get_buffer_fd);


int mtk_ccd_put_fd(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data,
			unsigned int target_fd)
{
	unsigned int i, num_buffers;
	dma_addr_t da;
	void *va = NULL;
	struct mtk_ccd_buf *buf = NULL;
	struct mtk_ccd_mem *ccd_buffer = NULL;
	struct mtk_ccd_memory *ccd_memory = ccd->ccd_memory;

	num_buffers = ccd_memory->num_buffers;

	if (unlikely(!num_buffers)) {
		dev_info(ccd_memory->dev, "buffer queue is empty");
		return -EINVAL;
	}

	for (i = 0; i < num_buffers; i++) {
		ccd_buffer = &ccd_memory->bufs[i];
		buf = (struct mtk_ccd_buf *)ccd_buffer->mem_priv;
		va = mtk_ccd_buf_get_vaddr(buf);
		da = mtk_ccd_buf_get_daddr(buf);

		if (mem_buff_data->va == va &&
			mem_buff_data->len == ccd_buffer->size) {

			if (atomic_long_read(&buf->dbuf->file->f_count) > 1 &&
			    current->files)
				__close_fd(current->files, target_fd);
			else
				dev_info(ccd_memory->dev,
						 "%s user space signal exit to close fd already",
						 __func__);
			/* decrease file count */
			dma_heap_buffer_free(buf->dbuf);

			dev_dbg(ccd_memory->dev,
					"put dma buf : %d, iova = %p, va = %p, fd = %d",
					i, &mem_buff_data->iova, mem_buff_data->va, target_fd);

			break;
		}
	}

	if (unlikely(i == num_buffers)) {
		dev_info(ccd_memory->dev,
				"mismatch dma buf iova = %p, va = %p",
				&mem_buff_data->iova, mem_buff_data->va);
		return -EINVAL;
	}

	return 0;
}

int mtk_ccd_put_buffer_fd(struct mtk_ccd *ccd,
				struct mem_obj *mem_buff_data,
				unsigned int target_fd)
{
	if (ccd == NULL || mem_buff_data == NULL) {
		pr_info("mtk_ccd or mem_obj is NULL\n");
		return -EINVAL;
	}
	return mtk_ccd_put_fd(ccd, mem_buff_data, target_fd);
}
EXPORT_SYMBOL_GPL(mtk_ccd_put_buffer_fd);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd memory interface");
