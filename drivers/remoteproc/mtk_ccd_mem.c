// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/fdtable.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <uapi/linux/mtk_ccd_controls.h>

#define CCD_ALLOCATE_MAX_BUFFER_SIZE 0x20000000UL /*512MB*/

struct mtk_ccd_buf;

struct mtk_ccd_buf_vmarea_handler {
	refcount_t		*refcount;
	void			(*put)(struct mtk_ccd_buf *buf);
	void			*arg;
};

struct mtk_ccd_dmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

struct mtk_ccd_buf {
	struct device *dev;
	void *vaddr;
	unsigned long size;
	void *cpu_addr;
	dma_addr_t dma_addr;
	unsigned long attrs;
	enum dma_data_direction dma_dir;
	struct sg_table *dma_sgt;

	/* MMAP related */
	struct mtk_ccd_buf_vmarea_handler handler;
	refcount_t refcount;
	struct sg_table *sgt_base;

	/* DMABUF related */
	struct dma_buf *dbuf;
	struct dma_buf_attachment *db_attach;
};

static void mtk_ccd_buf_vm_open(struct vm_area_struct *vma)
{
	struct mtk_ccd_buf_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, refcount_read(h->refcount), vma->vm_start,
	       vma->vm_end);

	refcount_inc(h->refcount);
}

static void mtk_ccd_buf_vm_close(struct vm_area_struct *vma)
{
	struct mtk_ccd_buf_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, refcount_read(h->refcount), vma->vm_start,
	       vma->vm_end);

	h->put(h->arg);
}

const struct vm_operations_struct mtk_ccd_buf_vm_ops = {
	.open = mtk_ccd_buf_vm_open,
	.close = mtk_ccd_buf_vm_close,
};

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

static void mtk_ccd_buf_put(struct mtk_ccd_buf *buf)
{
	if (!refcount_dec_and_test(&buf->refcount))
		return;

	if (buf->sgt_base) {
		sg_free_table(buf->sgt_base);
		kfree(buf->sgt_base);

	}
	dma_free_attrs(buf->dev, buf->size, buf->cpu_addr, buf->dma_addr,
		       buf->attrs);
	put_device(buf->dev);
	kfree(buf);
}

static struct mtk_ccd_buf *mtk_ccd_buf_alloc(struct device *dev,
					     unsigned long attrs, unsigned long size,
					     enum dma_data_direction dma_dir,
	gfp_t gfp_flags)
{
	struct mtk_ccd_buf *buf;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	if (attrs)
		buf->attrs = attrs;
	buf->cpu_addr = dma_alloc_attrs(dev, size, &buf->dma_addr,
					GFP_KERNEL | gfp_flags, buf->attrs);
	if (!buf->cpu_addr) {
		dev_info(dev, "dma_alloc_coherent of size %ld failed\n", size);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	if ((buf->attrs & DMA_ATTR_NO_KERNEL_MAPPING) == 0)
		buf->vaddr = buf->cpu_addr;

	buf->dev = get_device(dev);
	buf->size = size;
	buf->dma_dir = dma_dir;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = mtk_ccd_buf_put;
	buf->handler.arg = buf;

	refcount_set(&buf->refcount, 1);

	return buf;
}

static int mtk_ccd_buf_mmap(struct mtk_ccd_buf *buf, struct vm_area_struct *vma)
{
	int ret;

	if (!buf) {
		pr_info("No buffer to map\n");
		return -EINVAL;
	}

	ret = dma_mmap_attrs(buf->dev, vma, buf->cpu_addr,
			     buf->dma_addr, buf->size, buf->attrs);

	if (ret) {
		pr_debug("Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vma->vm_flags		|= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &mtk_ccd_buf_vm_ops;

	vma->vm_ops->open(vma);

	dev_dbg(buf->dev, "%s: mapped dma addr 0x%08lx at 0x%08lx, size %ld\n",
		 __func__, (unsigned long)buf->dma_addr, vma->vm_start,
		buf->size);

	return 0;
}

static int mtk_ccd_dmabuf_ops_attach(struct dma_buf *dbuf,
				     struct dma_buf_attachment *dbuf_attach)
{
	struct mtk_ccd_dmabuf_attachment *attach;
	unsigned int i;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt;
	struct mtk_ccd_buf *buf = dbuf->priv;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;

	ret = sg_alloc_table(sgt, buf->sgt_base->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	rd = buf->sgt_base->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void mtk_ccd_dmabuf_ops_detach(struct dma_buf *dbuf,
				      struct dma_buf_attachment *db_attach)
{
	struct mtk_ccd_dmabuf_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	if (attach->dma_dir != DMA_NONE)
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents,
				   attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *mtk_ccd_dmabuf_ops_map(struct dma_buf_attachment *db_attach,
					       enum dma_data_direction dma_dir)
{
	struct mtk_ccd_dmabuf_attachment *attach = db_attach->priv;

	struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

	mutex_lock(lock);

	sgt = &attach->sgt;

	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents,
				   attach->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		attach->dma_dir = DMA_NONE;
	}

	sgt->nents = dma_map_sg_attrs(db_attach->dev, sgt->sgl, sgt->orig_nents,
				      dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (!sgt->nents) {
		pr_debug("failed to map scatterlist\n");
		mutex_unlock(lock);
		return ERR_PTR(-EIO);
	}

	attach->dma_dir = dma_dir;

	mutex_unlock(lock);

	return sgt;
}

static void mtk_ccd_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
				     struct sg_table *sgt, enum dma_data_direction dma_dir)
{
}

static void mtk_ccd_dmabuf_ops_release(struct dma_buf *dbuf)
{
	mtk_ccd_buf_put(dbuf->priv);
}

static void *mtk_ccd_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	struct mtk_ccd_buf *buf = dbuf->priv;

	return buf->vaddr;
}

static int mtk_ccd_dmabuf_ops_mmap(struct dma_buf *dbuf,
				   struct vm_area_struct *vma)
{
	return mtk_ccd_buf_mmap(dbuf->priv, vma);
}

static const struct dma_buf_ops mtk_ccd_dmabuf_ops = {
	.attach = mtk_ccd_dmabuf_ops_attach,
	.detach = mtk_ccd_dmabuf_ops_detach,
	.map_dma_buf = mtk_ccd_dmabuf_ops_map,
	.unmap_dma_buf = mtk_ccd_dmabuf_ops_unmap,
	.vmap = mtk_ccd_dmabuf_ops_vmap,
	.mmap = mtk_ccd_dmabuf_ops_mmap,
	.release = mtk_ccd_dmabuf_ops_release,
};

static struct sg_table *mtk_ccd_buf_get_base_sgt(struct mtk_ccd_buf *buf)
{
	int ret;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	ret = dma_get_sgtable_attrs(buf->dev, sgt, buf->cpu_addr, buf->dma_addr,
				    buf->size, buf->attrs);
	if (ret < 0) {
		dev_dbg(buf->dev, "failed to get scatterlist from DMA API\n");
		kfree(sgt);
		return NULL;
	}

	return sgt;
}

static struct dma_buf *mtk_ccd_get_dmabuf(struct mtk_ccd_buf *buf, unsigned long flags)
{
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &mtk_ccd_dmabuf_ops;
	exp_info.size = buf->size;
	exp_info.flags = flags;
	exp_info.priv = buf;

	if (!buf->sgt_base)
		buf->sgt_base = mtk_ccd_buf_get_base_sgt(buf);

	if (WARN_ON(!buf->sgt_base))
		return NULL;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;

	buf->dbuf = dbuf;

	refcount_inc(&buf->refcount);

	return dbuf;
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
		return ERR_PTR(-EINVAL);
	}

	ccd_buffer = &ccd_memory->bufs[buffers];
	buf = mtk_ccd_buf_alloc(ccd_memory->dev, 0, mem_buff_data->len, 0, 0);
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


int mtk_ccd_get_fd(struct mtk_ccd *ccd, struct mtk_ccd_buf *buf)
{
	int target_fd = 0;
	struct dma_buf *dbuf;

	dbuf = mtk_ccd_get_dmabuf(buf, O_RDWR);

	if (dbuf == NULL || dbuf->file == NULL)
		return 0;

	target_fd = get_unused_fd_flags(O_CLOEXEC);

	get_file(dbuf->file);
	if (target_fd < 0)
		return -EMFILE;

	fd_install(target_fd, dbuf->file);
	return target_fd;
}

int mtk_ccd_get_buffer_fd(struct mtk_ccd *ccd, void *mem_priv)
{
	if (ccd == NULL || mem_priv == NULL) {
		pr_info("mtk_ccd or mem_priv is NULL\n");
		return -EINVAL;
	}
	return mtk_ccd_get_fd(ccd, mem_priv);
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

			dma_buf_put(buf->dbuf);

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
