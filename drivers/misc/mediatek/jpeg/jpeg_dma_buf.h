/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __JPEG_DMA_BUF_H__
#define __JPEG_DMA_BUF_H__

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

int jpg_dmabuf_get_iova(struct dma_buf *dbuf, u64 *iova,
	struct device *dev, struct dma_buf_attachment **attach, struct sg_table **sgt);
void jpg_dmabuf_free_iova(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach, struct sg_table *sgt);
int jpg_dmabuf_fd(struct dma_buf *dbuf);
struct dma_buf *jpg_dmabuf_get(int fd);
void jpg_get_dmabuf(struct dma_buf *dbuf);
void jpg_dmabuf_put(struct dma_buf *dbuf);
void* jpg_dmabuf_vmap(struct dma_buf *dbuf);
void jpg_dmabuf_vunmap(struct dma_buf *dbuf, void* buf_ptr);
struct dma_buf *jpg_dmabuf_alloc(size_t size, size_t align, unsigned int flags);

#endif /*__JPEG_DMA_BUF_H__*/
