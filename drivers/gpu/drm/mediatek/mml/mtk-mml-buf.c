/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>

#include "mtk-mml.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-core.h"

void mml_buf_get(struct mml_file_buf *buf, int32_t *fd, u8 cnt)
{
	u8 i;

	for (i = 0; i < cnt; i++) {
		struct dma_buf *dmabuf = dma_buf_get(fd[i]);

		if (!dmabuf || IS_ERR(dmabuf)) {
			mml_err("%s fail to get dma_buf by fd%hhu %d err %d",
				i, fd[i], PTR_ERR(dmabuf));
			continue;
		}

		buf->dma[i].dmabuf = dmabuf;
	}
}

inline static int dmabuf_to_iova(struct device *dev, struct mml_dma_buf *dma)
{
	int err;

	dma->attach = dma_buf_attach(dma->dmabuf, dev);

	if (IS_ERR(dma->attach)) {
		err = PTR_ERR(dma->attach);
		mml_err("%s attach fail buf %p dev %p err %d",
			__func__, dma->dmabuf, dev, err);
		goto err;
	}

	dma->sgt = dma_buf_map_attachment(dma->attach, DMA_TO_DEVICE);
	if (IS_ERR(dma->sgt)) {
		err = PTR_ERR(dma->sgt);
		mml_err("%s map failed err %d attach %p dev %p",
			__func__, err, dma->attach, dev);
		goto err_detach;
	}

	dma->iova = sg_dma_address(dma->sgt->sgl);
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
		if (!buf->dma[i].dmabuf)
			continue;
		ret = dmabuf_to_iova(dev, &buf->dma[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

inline static void dmabuf_iova_free(struct mml_dma_buf *dma)
{
	dma_buf_unmap_attachment(dma->attach, dma->sgt, DMA_TO_DEVICE);
	dma_buf_detach(dma->dmabuf, dma->attach);

	dma->sgt = NULL;
	dma->attach = NULL;
}

void mml_buf_iova_free(struct mml_file_buf *buf)
{
	u8 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].attach)
			continue;
		dmabuf_iova_free(&buf->dma[i]);
	}
}

void mml_buf_put(struct mml_file_buf *buf)
{
	u8 i;

	for (i = 0; i < buf->cnt; i++) {
		if (!buf->dma[i].dmabuf)
			continue;
		dma_buf_put(buf->dma[i].dmabuf);
	}
}

