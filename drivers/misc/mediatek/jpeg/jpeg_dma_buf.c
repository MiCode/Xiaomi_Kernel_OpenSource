// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "jpeg_dma_buf.h"
#include "jpeg_drv.h"

extern int jpg_dbg_level;

int jpg_dmabuf_get_iova(struct dma_buf *dbuf, u64 *iova,
	struct device *dev, struct dma_buf_attachment **attach, struct sg_table **sgt)
{
	*attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(*attach)) {
		JPEG_LOG(0, "attach fail, return");
        *attach = NULL;
		return -1;
	}

	*sgt = dma_buf_map_attachment(*attach, DMA_TO_DEVICE);
	if (IS_ERR(*sgt)) {
		JPEG_LOG(0, "map failed, detach and return");
		dma_buf_detach(dbuf, *attach);
        *sgt = NULL;
		return -1;
	}

	*iova = sg_dma_address((*sgt)->sgl);
	return 0;
}

void jpg_dmabuf_free_iova(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (dbuf == NULL || attach == NULL || sgt == NULL) {
		JPEG_LOG(0, "dbuf or attach or sgt null, not need to free iova");
		return;
	}
	dma_buf_unmap_attachment(attach, sgt, DMA_TO_DEVICE);
	dma_buf_detach(dbuf, attach);
}

int jpg_dmabuf_fd(struct dma_buf *dbuf)
{
	return dma_buf_fd(dbuf, O_CLOEXEC | O_RDWR);
}

struct dma_buf *jpg_dmabuf_get(int fd)
{
	struct dma_buf * dbuf;

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		JPEG_LOG(0, "dma_buf_get fail");
		return NULL;
	}

	return dbuf;
}

void jpg_get_dmabuf(struct dma_buf *dbuf)
{
	get_dma_buf(dbuf);
}

void jpg_dmabuf_put(struct dma_buf *dbuf)
{
	if (!dbuf) {
		JPEG_LOG(0, "dma_buf null, no need to put.");
		return;
	}
	dma_buf_put(dbuf);
}

void* jpg_dmabuf_vmap(struct dma_buf *dbuf)
{
	void *buf_ptr;

	buf_ptr = dma_buf_vmap(dbuf);
	if (!buf_ptr) {
		JPEG_LOG(0, "map failed");
		return NULL;
	}

	return buf_ptr;
}

void jpg_dmabuf_vunmap(struct dma_buf *dbuf, void* buf_ptr)
{
	dma_buf_vunmap(dbuf, buf_ptr);
}

struct dma_buf *jpg_dmabuf_alloc(size_t size, size_t align, unsigned int flags)
{
	struct dma_heap * dma_heap;
	struct dma_buf *dbuf;

	dma_heap = dma_heap_find("mtk_mm-uncached");

	if (!dma_heap) {
		JPEG_LOG(0, "heap find fail");
		return NULL;
	}

	dbuf = dma_heap_buffer_alloc(dma_heap, size,
		O_CLOEXEC | O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
	dma_heap_put(dma_heap);
	if(IS_ERR(dbuf)) {
		JPEG_LOG(0, "buffer alloc fail");
		return NULL;
	}

	return dbuf;
}

