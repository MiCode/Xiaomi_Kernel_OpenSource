/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/sched/clock.h>
#include <mtk_heap.h>

#include "mtk-mml.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-core.h"

s32 mml_buf_get_fd(struct mml_file_buf *buf, int32_t *fd, u32 cnt, const char *name)
{
	u32 i;
	struct dma_buf *dmabuf;

	for (i = 0; i < cnt; i++) {
		if (fd[i] < 0)
			continue;

		dmabuf = dma_buf_get(fd[i]);
		if (IS_ERR_OR_NULL(dmabuf)) {
			mml_err("%s fail to get dma_buf %u by fd %d err %ld",
				__func__, i, fd[i], PTR_ERR(dmabuf));
			return -ENOMEM;
		}

		buf->dma[i].dmabuf = dmabuf;
		mtk_dma_buf_set_name(dmabuf, name);
	}
	return 0;
}

void mml_buf_get(struct mml_file_buf *buf, void **dmabufs, u32 cnt, const char *name)
{
	u32 i;

	for (i = 0; i < cnt; i++) {
		if (!dmabufs[i])
			continue;

		get_dma_buf(dmabufs[i]);
		buf->dma[i].dmabuf = dmabufs[i];
		mtk_dma_buf_set_name(dmabufs[i], name);
	}
}

inline static int dmabuf_to_iova(struct device *dev, struct mml_dma_buf *dma)
{
	int err;

	dma->attach = dma_buf_attach(dma->dmabuf, dev);
	if (IS_ERR_OR_NULL(dma->attach)) {
		err = PTR_ERR(dma->attach);
		mml_err("%s attach fail buf %p dev %p err %d",
			__func__, dma->dmabuf, dev, err);
		goto err;
	}

	dma->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	dma->sgt = dma_buf_map_attachment(dma->attach, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sgt)) {
		err = PTR_ERR(dma->sgt);
		mml_err("%s map failed err %d attach %p dev %p",
			__func__, err, dma->attach, dev);
		goto err_detach;
	}

	dma->iova = sg_dma_address(dma->sgt->sgl);
	if (!dma->iova) {
		mml_err("%s iova map fail dev %p", __func__, dev);
		err = -ENOMEM;
		goto err_detach;
	}
	return 0;

err_detach:
	dma_buf_detach(dma->dmabuf, dma->attach);
	dma->sgt = NULL;
err:
	dma->attach = NULL;
	return err;
}

int mml_buf_iova_get(struct device *dev, struct mml_file_buf *buf)
{
	u8 i;
	int ret;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf) {
			/* no fd/dmabuf but need this plane,
			 * use previous iova
			 */
			if (i)
				buf->dma[i].iova = buf->dma[i-1].iova;
			continue;
		}
		ret = dmabuf_to_iova(dev, &buf->dma[i]);
		if (ret < 0)
			return ret;
	}
	buf->map_time = sched_clock();

	return 0;
}

int mml_buf_va_get(struct mml_file_buf *buf)
{
	u32 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf || buf->dma[i].va)
			continue;
		buf->dma[i].va = dma_buf_vmap(buf->dma[i].dmabuf);
		if (!buf->dma[i].va)
			return -ENOMEM;
	}

	return 0;
}

inline static void dmabuf_iova_free(struct mml_dma_buf *dma)
{
	dma_buf_unmap_attachment(dma->attach, dma->sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dma->dmabuf, dma->attach);

	dma->sgt = NULL;
	dma->attach = NULL;
}

void mml_buf_put(struct mml_file_buf *buf)
{
	u8 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf)
			continue;
		if (buf->dma[i].va)
			dma_buf_vunmap(buf->dma[i].dmabuf, buf->dma[i].va);
		if (buf->dma[i].attach)
			dmabuf_iova_free(&buf->dma[i]);
		dma_buf_put(buf->dma[i].dmabuf);
	}
	buf->unmap_time = sched_clock();
}

void mml_buf_flush(struct mml_file_buf *buf)
{
	u8 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf)
			continue;
		if (!buf->dma[i].attach) {
			mml_err("%s no attach to flush plane %hhu",
				__func__, i);
			continue;
		}
		buf->dma[i].attach->dma_map_attrs &= ~DMA_ATTR_SKIP_CPU_SYNC;
		dma_buf_end_cpu_access(buf->dma[i].dmabuf, DMA_TO_DEVICE);
		buf->dma[i].attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	}
}

void mml_buf_invalid(struct mml_file_buf *buf)
{
	u8 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf)
			continue;
		buf->dma[i].attach->dma_map_attrs &= ~DMA_ATTR_SKIP_CPU_SYNC;
		dma_buf_begin_cpu_access(buf->dma[i].dmabuf, DMA_FROM_DEVICE);
		buf->dma[i].attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	}
}
