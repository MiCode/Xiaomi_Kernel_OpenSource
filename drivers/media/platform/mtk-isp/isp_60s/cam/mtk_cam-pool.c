// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */

#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/remoteproc.h>
#include <linux/spinlock.h>

#include "mtk_cam.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-pool.h"

#ifndef CONFIG_MTK_SCP
#include <linux/platform_data/mtk_ccd_controls.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#endif

int mtk_cam_working_buf_pool_init(struct mtk_cam_device *cam)
{
	int i;
	struct ccd_mem_obj smem;
	struct mtk_ccd *ccd;
	struct dma_buf *dmabuf;
	void *mem_priv;
	int dmabuf_fd;
	const int working_buf_size = round_up(WORKING_BUF_SIZE, PAGE_SIZE);

	dev_dbg(cam->dev, "%s:working_buf_pool_init S\n", __func__);

	INIT_LIST_HEAD(&cam->cam_freebufferlist.list);
	spin_lock_init(&cam->cam_freebufferlist.lock);
	cam->cam_freebufferlist.cnt = 0;
	cam->working_buf_mem_size = CAM_SUB_FRM_DATA_NUM *
		working_buf_size;
	smem.len = cam->working_buf_mem_size;

	ccd = (struct mtk_ccd *)cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	dmabuf = mtk_ccd_get_dmabuf(ccd, mem_priv);
	dmabuf_fd = mtk_ccd_get_dmabuf_fd(ccd, dmabuf, 0);

	cam->working_buf_mem_pa = smem.pa;
	cam->working_buf_mem_va = smem.va;
	cam->working_buf_mem_iova = smem.iova;
	cam->working_buf_mem_fd = dmabuf_fd;

	for (i = 0; i < CAM_SUB_FRM_DATA_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &cam->working_buf[i];
		int offset;

		offset = i * working_buf_size;
		buf->buffer.addr_offset = offset;
		buf->buffer.pa =
		    cam->working_buf_mem_pa + offset;
		buf->buffer.va =
		    cam->working_buf_mem_va + offset;
		buf->buffer.iova =
		    cam->working_buf_mem_iova + offset;
		buf->buffer.size = working_buf_size;
		dev_info(cam->dev,
			 "%s:buf(%d), pa(%pad), iova(%pad)\n",
			__func__, i, &buf->buffer.pa,
			&buf->buffer.iova);

		list_add_tail(&buf->list_entry,
			      &cam->cam_freebufferlist.list);
		cam->cam_freebufferlist.cnt++;
	}
	dev_info(cam->dev, "%s:freebuf cnt(%d)\n", __func__,
		 cam->cam_freebufferlist.cnt);
	dev_info(cam->dev, "%s:working_buf_pool_init E\n", __func__);
	return 0;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_device *cam)
{
	struct mtk_ccd *ccd = cam->rproc_handle->priv;
	struct ccd_mem_obj smem;

	dev_dbg(cam->dev, "%s: s\n", __func__);

	smem.pa = cam->working_buf_mem_pa;
	smem.va = cam->working_buf_mem_va;
	smem.iova = cam->working_buf_mem_iova;
	smem.len = cam->working_buf_mem_size;
	mtk_ccd_free_buffer(ccd, &smem);

	dev_dbg(cam->dev, "%s: e\n", __func__);
}

void mtk_cam_working_buf_put(struct mtk_cam_device *cam,
			     struct mtk_cam_working_buf_entry *buf_entry)
{
	unsigned long flags;

	dev_dbg(cam->dev, "%s: s\n", __func__);

	if (!buf_entry)
		return;

	spin_lock_irqsave(&cam->cam_freebufferlist.lock, flags);
	list_add_tail(&buf_entry->list_entry,
		      &cam->cam_freebufferlist.list);
	cam->cam_freebufferlist.cnt++;
	spin_unlock_irqrestore(&cam->cam_freebufferlist.lock, flags);

	dev_dbg(cam->dev, "%s: e\n", __func__);
}

struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_device *cam)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	unsigned long flags;

	dev_dbg(cam->dev, "%s: s\n", __func__);

	/* get from free list */
	spin_lock_irqsave(&cam->cam_freebufferlist.lock, flags);
	if (list_empty(&cam->cam_freebufferlist.list)) {
		spin_unlock_irqrestore(&cam->cam_freebufferlist.lock, flags);
	return NULL;
	}

	buf_entry = list_first_entry(&cam->cam_freebufferlist.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cam->cam_freebufferlist.cnt--;
	spin_unlock_irqrestore(&cam->cam_freebufferlist.lock, flags);

	dev_dbg(cam->dev, "%s: e, buf_entry->buffer.addr_offset 0x%x\n",
		__func__, buf_entry->buffer.addr_offset);
	return buf_entry;
}

