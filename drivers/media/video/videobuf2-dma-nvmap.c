/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/nvmap.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_conf {
	struct device		*dev;
	struct nvmap_client	*nvmap_client;
};

struct vb2_dc_buf {
	struct vb2_dc_conf		*conf;
	void				*vaddr;
	dma_addr_t			paddr;
	unsigned long			size;
	struct vm_area_struct		*vma;
	atomic_t			refcount;
	struct vb2_vmarea_handler	handler;

	struct nvmap_handle_ref		*nvmap_ref;
};

static void vb2_dma_nvmap_put(void *buf_priv);

static void *vb2_dma_nvmap_alloc(void *alloc_ctx, unsigned long size)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct vb2_dc_buf *buf;
	int ret;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto exit;
	}

	buf->nvmap_ref = nvmap_alloc(conf->nvmap_client, size, 32,
				     NVMAP_HANDLE_CACHEABLE, NVMAP_HEAP_SYSMEM);
	if (IS_ERR(buf->nvmap_ref)) {
		dev_err(conf->dev, "nvmap_alloc failed\n");
		ret = -ENOMEM;
		goto exit_free;
	}

	buf->paddr = nvmap_pin(conf->nvmap_client, buf->nvmap_ref);
	if (IS_ERR_VALUE(buf->paddr)) {
		dev_err(conf->dev, "nvmap_pin failed\n");
		ret = -ENOMEM;
		goto exit_dealloc;
	}

	buf->vaddr = nvmap_mmap(buf->nvmap_ref);
	if (!buf->vaddr) {
		dev_err(conf->dev, "nvmap_mmap failed\n");
		ret = -ENOMEM;
		goto exit_unpin;
	}

	buf->conf = conf;
	buf->size = size;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_dma_nvmap_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;

exit_unpin:
	nvmap_unpin(conf->nvmap_client, buf->nvmap_ref);
exit_dealloc:
	nvmap_free(conf->nvmap_client, buf->nvmap_ref);
exit_free:
	kfree(buf);
exit:
	return ERR_PTR(ret);
}

static void vb2_dma_nvmap_put(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (atomic_dec_and_test(&buf->refcount)) {
		nvmap_munmap(buf->nvmap_ref, buf->vaddr);
		nvmap_unpin(buf->conf->nvmap_client, buf->nvmap_ref);
		nvmap_free(buf->conf->nvmap_client, buf->nvmap_ref);
		kfree(buf);
	}
}

static void *vb2_dma_nvmap_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->paddr;
}

static void *vb2_dma_nvmap_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	if (!buf)
		return 0;

	return buf->vaddr;
}

static unsigned int vb2_dma_nvmap_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static int vb2_dma_nvmap_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!buf) {
		printk(KERN_ERR "No buffer to map\n");
		return -EINVAL;
	}

	return vb2_mmap_pfn_range(vma, buf->paddr, buf->size,
				  &vb2_common_vm_ops, &buf->handler);
}

static void *vb2_dma_nvmap_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	struct vm_area_struct *vma;
	dma_addr_t paddr = 0;
	int ret;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = vb2_get_contig_userptr(vaddr, size, &vma, &paddr);
	if (ret) {
		printk(KERN_ERR "Failed acquiring VMA for vaddr 0x%08lx\n",
				vaddr);
		kfree(buf);
		return ERR_PTR(ret);
	}

	buf->size = size;
	buf->paddr = paddr;
	buf->vma = vma;

	return buf;
}

static void vb2_dma_nvmap_put_userptr(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (!buf)
		return;

	vb2_put_vma(buf->vma);
	kfree(buf);
}

const struct vb2_mem_ops vb2_dma_nvmap_memops = {
	.alloc		= vb2_dma_nvmap_alloc,
	.put		= vb2_dma_nvmap_put,
	.cookie		= vb2_dma_nvmap_cookie,
	.vaddr		= vb2_dma_nvmap_vaddr,
	.mmap		= vb2_dma_nvmap_mmap,
	.get_userptr	= vb2_dma_nvmap_get_userptr,
	.put_userptr	= vb2_dma_nvmap_put_userptr,
	.num_users	= vb2_dma_nvmap_num_users,
};
EXPORT_SYMBOL_GPL(vb2_dma_nvmap_memops);

void *vb2_dma_nvmap_init_ctx(struct device *dev)
{
	struct vb2_dc_conf *conf;
	int ret;

	conf = kzalloc(sizeof *conf, GFP_KERNEL);
	if (!conf) {
		ret = -ENOMEM;
		goto exit;
	}

	conf->dev = dev;

	conf->nvmap_client = nvmap_create_client(nvmap_dev,
						 "videobuf2-dma-nvmap");
	if (!conf->nvmap_client) {
		ret = -ENOMEM;
		goto exit_free;
	}

	return conf;

exit_free:
	kfree(conf);
exit:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(vb2_dma_nvmap_init_ctx);

void vb2_dma_nvmap_cleanup_ctx(void *alloc_ctx)
{
	struct vb2_dc_conf *conf = alloc_ctx;

	nvmap_client_put(conf->nvmap_client);

	kfree(alloc_ctx);
}
EXPORT_SYMBOL_GPL(vb2_dma_nvmap_cleanup_ctx);

MODULE_DESCRIPTION("DMA-nvmap memory handling routines for videobuf2");
MODULE_AUTHOR("Andrew Chew <achew@nvidia.com>");
MODULE_LICENSE("GPL");
