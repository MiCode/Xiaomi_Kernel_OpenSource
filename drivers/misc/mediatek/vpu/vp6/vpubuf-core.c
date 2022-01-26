/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * @file vpubuf_core.c
 * Handle about VPU memory management.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/iommu.h>

#include "vpubuf-core.h"
#include "vpubuf-dma-contig.h"
#include "vpu_cmn.h"

DECLARE_VLIST(vpu_kbuffer);
DECLARE_VLIST(vpu_dbg_buf);

static int __vpu_buf_mmap_alloc(struct vpu_kbuffer *vpub)
{
	struct vpu_manage *mng = vpub->vpu_manage;
	enum dma_data_direction dma_dir = DMA_BIDIRECTIONAL;
	void *mem_priv;

	/*
	 * Allocate memory for all planes in this buffer
	 * NOTE: mmapped areas should be page aligned
	 */
	unsigned long size = PAGE_ALIGN(vpub->length);

	vpub->length = size;

	mem_priv = mng->mem_ops->alloc(mng->alloc_ctx, size, dma_dir, 0,
				       vpub->is_cached);
	if (IS_ERR_OR_NULL(mem_priv))
		goto free;

	/* Associate allocator private data with this plane */
	vpub->mem_priv = mem_priv;
	vpub->dma_addr = *(int *)mng->mem_ops->cookie(mem_priv);
	vpub->vaddr = mng->mem_ops->vaddr(mem_priv);

	return 0;
free:
	vpub->mem_priv = NULL;

	return -ENOMEM;
}

static void __vpu_buf_mmap_free(struct vpu_kbuffer *vpub)
{
	struct vpu_manage *mng = vpub->vpu_manage;

	if (!vpub->mem_priv)
		return;

	mng->mem_ops->put(vpub->mem_priv);
	vpub->mem_priv = NULL;
}

static void __vpu_buf_dmabuf_put(struct vpu_kbuffer *vpub)
{
	struct vpu_manage *mng = vpub->vpu_manage;

	if (!vpub->mem_priv)
		return;

	if (vpub->dbuf_mapped)
		mng->mem_ops->unmap_dmabuf(vpub->mem_priv);

	mng->mem_ops->detach_dmabuf(vpub->mem_priv);
	dma_buf_put(vpub->dbuf);
	vpub->mem_priv = NULL;
	vpub->dbuf = NULL;
	vpub->dbuf_mapped = 0;
}

static int __vpu_buf_mem_import(struct vpu_kbuffer *vpub,
				unsigned int dma_fd)
{
	struct vpu_manage *mng = vpub->vpu_manage;
	void *mem_priv;
	int ret;
	enum dma_data_direction dma_dir = DMA_BIDIRECTIONAL;
	struct dma_buf *dbuf = dma_buf_get(dma_fd);

	if (IS_ERR_OR_NULL(dbuf)) {
		LOG_ERR("invalid dmabuf fd\n");
		ret = -EINVAL;
		goto err;
	}

	/* use DMABUF size if length is not provided */
	vpub->length = dbuf->size;

	/* Acquire each plane's memory */
	mem_priv =
	    mng->mem_ops->attach_dmabuf(mng->alloc_ctx, dbuf, vpub->length,
					dma_dir);
	if (IS_ERR(mem_priv)) {
		LOG_ERR("failed to attach dmabuf\n");
		ret = PTR_ERR(mem_priv);
		dma_buf_put(dbuf);
		goto err;
	}

	vpub->dbuf = dbuf;
	vpub->mem_priv = mem_priv;

	/* TODO: This pins the buffer(s) with  dma_buf_map_attachment()).. but
	 * really we want to do this just before the DMA, not while queueing
	 * the buffer(s)..
	 */
	ret = mng->mem_ops->map_dmabuf(vpub->mem_priv);
	if (ret) {
		LOG_ERR("failed to map dmabuf\n");
		goto err;
	}
	vpub->dbuf_mapped = 1;

	/*
	 * Now that everything is in order, copy relevant information
	 * provided by userspace.
	 */
	vpub->dma_fd = dma_fd;
	vpub->dma_addr = *(int *)mng->mem_ops->cookie(mem_priv);
	vpub->vaddr = mng->mem_ops->vaddr(mem_priv);

	return 0;
err:
	/* In case of errors, release planes that were already acquired */
	__vpu_buf_dmabuf_put(vpub);

	return ret;
}

static int vpu_check_buf_handle(struct vpu_device *vpu_device,
				struct vpu_kbuffer *vpub)
{
	struct vpu_manage *mng;
	struct vpu_kbuffer *vpub_t;
	struct list_head *head;
	bool is_valid = false;

	if (vpu_device->vbuf_mng == NULL) {
		LOG_ERR("vbuf_std_init not ready\n");
		return is_valid;
	}
	mng = vpu_device->vbuf_mng;

	list_for_each(head, &mng->buf_list) {
		vpub_t = vlist_node_of(head, struct vpu_kbuffer);
		if (vpub_t == vpub) {
			is_valid = true;
			break;
		}
	}

	return is_valid;
}

int vbuf_std_info(struct vpu_device *vpu_device, struct vbuf_info *info_ctx)
{
	info_ctx->method_sel = vpu_device->method;
	return 0;
}

/** @ingroup IP_group_vpu_internal_function
 * @par Description
 *     To create buffer.
 * @param[out]
 *     mng: pointer to the vpu memory manage.
 * @param[out]
 *     alloc_ctx: pointer to the alloc buffer contex.
 * @return
 *     0, successful to creat buffer. \n
 *     -ENOMEM, out of memory.
 * @par Boundary case and Limitation
 *     none.
 * @par Error case and Error handling
 *     If allocating buffer memory fails, return -ENOMEM.
 * @par Call graph and Caller graph (refer to the graph below)
 * @par Refer to the source code
 */
int vbuf_std_alloc(struct vpu_device *vpu_device, struct vbuf_alloc *alloc_ctx)
{
	struct vpu_manage *mng;
	struct vpu_kbuffer *vpub;
	int ret;
	struct dma_buf *dbuf;
	unsigned int flags;

	if (vpu_device->vbuf_mng == NULL) {
		LOG_ERR("vbuf_std_init not ready\n");
		return -EINVAL;
	}
	mng = vpu_device->vbuf_mng;

	if (alloc_ctx->type == VBUF_TYPE_ALLOC) {
		/* ALLOC BUF */
		mutex_lock(&mng->buf_mutex);

		/* Allocate videobuf buffer structures */
		vpub = kzalloc(sizeof(vlist_type(struct vpu_kbuffer)),
				GFP_KERNEL);
		if (!vpub) {
			mutex_unlock(&mng->buf_mutex);
			return -ENOMEM;
		}

		vpub->vpu_manage = mng;
		vpub->length = alloc_ctx->req_size;
		vpub->memory = alloc_ctx->type;
		vpub->is_cached = alloc_ctx->cache_flag;

		ret = __vpu_buf_mmap_alloc(vpub);
		if (ret) {
			LOG_ERR("failed allocating memory\n");
			kfree(vpub);
			mutex_unlock(&mng->buf_mutex);
			return ret;
		}

		list_add_tail(vlist_link(vpub, struct vpu_kbuffer),
			      &mng->buf_list);
		mng->buf_num++;

		alloc_ctx->handle = (u64)(uintptr_t)vpub;
		alloc_ctx->buf_size = vpub->length;
		alloc_ctx->iova_addr = vpub->dma_addr;
		alloc_ctx->iova_size = PAGE_ALIGN(vpub->length);

		/* EXPORT BUF */
		if (alloc_ctx->exp_flag) {
			flags = O_RDWR | O_CLOEXEC;

			if (!mng->mem_ops->get_dmabuf) {
				LOG_ERR("not support DMA buffer exporting\n");
				__vpu_buf_mmap_free(vpub);
				kfree(vpub);
				mutex_unlock(&mng->buf_mutex);
				return -EINVAL;
			}

			dbuf = mng->mem_ops->get_dmabuf(vpub->mem_priv,
							flags & O_ACCMODE);
			if (IS_ERR_OR_NULL(dbuf)) {
				LOG_ERR("failed to export buffer\n");
				__vpu_buf_mmap_free(vpub);
				kfree(vpub);
				mutex_unlock(&mng->buf_mutex);
				return -EINVAL;
			}

			ret = dma_buf_fd(dbuf, flags & ~O_ACCMODE);
			if (ret < 0) {
				LOG_ERR("buffer, failed to export (%d)\n",
					 ret);
				dma_buf_put(dbuf);
				__vpu_buf_mmap_free(vpub);
				kfree(vpub);
				mutex_unlock(&mng->buf_mutex);
				return ret;
			}

			alloc_ctx->exp_fd = ret;
		}

		mutex_unlock(&mng->buf_mutex);
	} else {
		/* IMPORT BUF */
		mutex_lock(&mng->buf_mutex);

		/* Allocate videobuf buffer structures */
		vpub = kzalloc(sizeof(vlist_type(struct vpu_kbuffer)),
				GFP_KERNEL);
		if (!vpub) {
			mutex_unlock(&mng->buf_mutex);
			return -EINVAL;
		}

		vpub->vpu_manage = mng;
		vpub->length = 0;
		vpub->memory = alloc_ctx->type;

		ret = __vpu_buf_mem_import(vpub, alloc_ctx->imp_fd);
		if (ret) {
			LOG_ERR("failed importing memory\n");
			kfree(vpub);
			mutex_unlock(&mng->buf_mutex);
			return -EINVAL;
		}

		list_add_tail(vlist_link(vpub, struct vpu_kbuffer),
			      &mng->buf_list);
		mng->buf_num++;

		alloc_ctx->handle = (u64)(uintptr_t)vpub;
		alloc_ctx->buf_size = vpub->length;
		alloc_ctx->iova_addr = vpub->dma_addr;
		alloc_ctx->iova_size = vpub->length;
		alloc_ctx->exp_fd = alloc_ctx->imp_fd;

		mutex_unlock(&mng->buf_mutex);
	}

	return 0;
}

/** @ingroup IP_group_vpu_internal_function
 * @par Description
 *     To free the created buffer.
 * @param[out]
 *     mng: pointer to the vpu memory manage.
 * @param[out]
 *     free_ctx: pointer to the free buffer.
 * @return
 *     0, successful to free the created buffer. \n
 *     -EINVAL, wrong input parameter.
 * @par Boundary case and Limitation
 *     none.
 * @par Error case and Error handling
 *     If enter an wrong memory, return -EINVAL.
 * @par Call graph and Caller graph (refer to the graph below)
 * @par Refer to the source code
 */
int vbuf_std_free(struct vpu_device *vpu_device, struct vbuf_free *free_ctx)
{
	struct vpu_manage *mng;
	struct vpu_kbuffer *vpub;

	if (vpu_device->vbuf_mng == NULL) {
		LOG_ERR("vbuf_std_init not ready\n");
		return -EINVAL;
	}
	mng = vpu_device->vbuf_mng;

	mutex_lock(&mng->buf_mutex);

	vpub = (struct vpu_kbuffer *)(uintptr_t)free_ctx->handle;
	if (!vpu_check_buf_handle(vpu_device, vpub)) {
		LOG_ERR("%s fail, vpu_kbuffer is invalid\n", __func__);
		mutex_unlock(&mng->buf_mutex);
		return -EINVAL;
	}

	if (vpub->memory == VBUF_TYPE_ALLOC) {
		__vpu_buf_mmap_free(vpub);
	} else if (vpub->memory == VBUF_TYPE_IMPORT) {
		__vpu_buf_dmabuf_put(vpub);
	} else {
		mutex_unlock(&mng->buf_mutex);
		return -EINVAL;
	}

	mng->buf_num--;
	list_del_init(vlist_link(vpub, struct vpu_kbuffer));
	kfree(vpub);

	mutex_unlock(&mng->buf_mutex);

	return 0;
}

int vbuf_std_sync(struct vpu_device *vpu_device, struct vbuf_sync *sync_ctx)
{
	struct vpu_manage *mng;
	struct vpu_kbuffer *vpub;

	if (vpu_device->vbuf_mng == NULL) {
		LOG_ERR("vbuf_std_init not ready\n");
		return -EINVAL;
	}
	mng = vpu_device->vbuf_mng;

	mutex_lock(&mng->buf_mutex);

	vpub = (struct vpu_kbuffer *)(uintptr_t)sync_ctx->handle;
	if (!vpu_check_buf_handle(vpu_device, vpub)) {
		LOG_ERR("%s fail, vpu_kbuffer is invalid\n", __func__);
		mutex_unlock(&mng->buf_mutex);
		return -EINVAL;
	}

	if (sync_ctx->direction == VBUF_SYNC_TO_DEVICE)
		mng->mem_ops->prepare(vpub->mem_priv);
	else
		mng->mem_ops->finish(vpub->mem_priv);

	mutex_unlock(&mng->buf_mutex);

	return 0;
}

int vbuf_std_init(struct vpu_device *vpu_device)
{
	struct vpu_alloc_ctx *alloc_ctx;
	struct vpu_manage *mng;
	int ret = 0;

	if (vpu_device->vbuf_mng != NULL) {
		LOG_ERR("vbuf_mng has already created\n");
		return ret;
	}

	vpu_device->vbuf_mng = kzalloc(sizeof(*vpu_device->vbuf_mng),
					GFP_KERNEL);
	if (IS_ERR(vpu_device->vbuf_mng))
		return -ENOMEM;

	mng = vpu_device->vbuf_mng;

	alloc_ctx = vpu_dma_contig_init_ctx(vpu_device->dev);
	if (IS_ERR(alloc_ctx)) {
		kfree(mng);
		vpu_device->vbuf_mng = NULL;
		return -ENOMEM;
	}

	mng->mem_ops = &vpu_dma_contig_memops;
	mutex_init(&mng->buf_mutex);
	INIT_LIST_HEAD(&mng->buf_list);
	mng->buf_num = 0;
	mng->alloc_ctx = alloc_ctx;

	return ret;
}

void vbuf_std_deinit(struct vpu_device *vpu_device)
{
	struct vpu_manage *mng;

	if (vpu_device->vbuf_mng == NULL)
		return;

	mng = vpu_device->vbuf_mng;

	vpu_dma_contig_cleanup_ctx(mng->alloc_ctx);
	mng->alloc_ctx = NULL;
	kfree(mng);
	vpu_device->vbuf_mng = NULL;
}

void vpu_add_dbg_buf(struct vpu_user *user, uint64_t buf_handle)
{
	struct vpu_dbg_buf *dbgbuf;

	dbgbuf = kzalloc(sizeof(vlist_type(struct vpu_dbg_buf)), GFP_KERNEL);
	if (!dbgbuf)
		return;

	dbgbuf->handle = buf_handle;

	mutex_lock(&user->dbgbuf_mutex);
	list_add_tail(vlist_link(dbgbuf, struct vpu_dbg_buf),
		      &user->dbgbuf_list);
	mutex_unlock(&user->dbgbuf_mutex);
}

void vpu_delete_dbg_buf(struct vpu_user *user, uint64_t buf_handle)
{
	struct list_head *head, *temp;
	struct vpu_dbg_buf *dbgbuf;

	mutex_lock(&user->dbgbuf_mutex);
	list_for_each_safe(head, temp, &user->dbgbuf_list) {
		dbgbuf = vlist_node_of(head, struct vpu_dbg_buf);
		if (dbgbuf->handle == buf_handle) {
			list_del_init(vlist_link(dbgbuf, struct vpu_dbg_buf));
			kfree(dbgbuf);
			break;
		}
	}
	mutex_unlock(&user->dbgbuf_mutex);
}

void vpu_check_dbg_buf(struct vpu_user *user)
{
	struct vpu_device *vpu_device = dev_get_drvdata(user->dev);
	struct list_head *head, *temp;
	struct vpu_dbg_buf *dbgbuf;
	struct vbuf_free free_buf;

	mutex_lock(&user->dbgbuf_mutex);
	list_for_each_safe(head, temp, &user->dbgbuf_list) {
		dbgbuf = vlist_node_of(head, struct vpu_dbg_buf);
		pr_info("[vpu BUF] free buffer leak : %llu\n", dbgbuf->handle);
		free_buf.handle = dbgbuf->handle;
		vbuf_std_free(vpu_device, &free_buf);
		list_del_init(vlist_link(dbgbuf, struct vpu_dbg_buf));
		kfree(dbgbuf);
	}
	mutex_unlock(&user->dbgbuf_mutex);
}

void vbuf_init_phy_iova(struct vpu_device *vpu_device)
{
#ifdef MTK_VPU_SUPPORT_ION
	vpu_device->vpu_std_mapops = &vpu_ion_mapops;
	vpu_device->method = VKBUF_METHOD_ION;
#else
	vpu_device->vpu_std_mapops = &vpu_dma_mapops;
	vpu_device->method = VKBUF_METHOD_STD;
#endif

	if (vpu_device->vpu_std_mapops)
		vpu_device->vpu_std_mapops->init_phy_iova(vpu_device);
}

void vbuf_deinit_phy_iova(struct vpu_device *vpu_device)
{
	if (vpu_device->vpu_std_mapops)
		vpu_device->vpu_std_mapops->deinit_phy_iova(vpu_device);
}

struct vpu_kernel_buf *vbuf_kmap_phy_iova(struct vpu_device *vpu_device,
					uint32_t usage, uint64_t phy_addr,
					uint64_t kva_addr, uint32_t iova_addr,
					uint32_t size)
{
	struct vpu_kernel_buf *vpubuf = NULL;

	if (vpu_device->vpu_std_mapops)
		vpubuf = vpu_device->vpu_std_mapops->kmap_phy_iova(vpu_device,
							usage, phy_addr,
							kva_addr, iova_addr,
							size);
	return vpubuf;
}

void vbuf_kunmap_phy_iova(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf)
{
	if (vpu_device->vpu_std_mapops)
		vpu_device->vpu_std_mapops->kunmap_phy_iova(vpu_device, vkbuf);
}

uint64_t vbuf_import_handle(struct vpu_device *vpu_device, int fd)
{
	uint64_t ret = 0;

	if (vpu_device->vpu_std_mapops)
		ret = vpu_device->vpu_std_mapops->import_handle(vpu_device, fd);

	return ret;
}

void vbuf_free_handle(struct vpu_device *vpu_device, uint64_t id)
{
	if (vpu_device->vpu_std_mapops)
		vpu_device->vpu_std_mapops->free_handle(vpu_device, id);
}
