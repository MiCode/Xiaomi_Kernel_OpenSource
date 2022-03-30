/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __VDEC_FMT_DMABUF_H__
#define __VDEC_FMT_DMABUF_H__

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

#define FMT_FD_RESERVE         3
struct dmabufmap {
	int fd;
	u64 iova;
};
int fmt_dmabuf_get_iova(struct dma_buf *dbuf, u64 *iova,
	struct device *dev, struct dma_buf_attachment **attach, struct sg_table **sgt,
	bool cache_sync);
void fmt_dmabuf_free_iova(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach, struct sg_table *sgt);
struct dma_buf *fmt_dmabuf_get(int fd);
void fmt_dmabuf_put(struct dma_buf *dbuf);
u64 fmt_translate_fd(u64 fd, u32 offset, struct dmabufmap map[], struct device *dev,
	struct dma_buf **dbuf, struct dma_buf_attachment **attach, struct sg_table **sgt,
	bool cache_sync);


#endif /*__VDEC_FMT_DMABUF_H__*/
