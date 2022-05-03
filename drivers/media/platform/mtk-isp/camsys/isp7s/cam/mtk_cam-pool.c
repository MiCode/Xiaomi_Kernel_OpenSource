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
#include <linux/dma-heap.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <uapi/linux/dma-heap.h>

#include <mtk_heap.h>
#include "mtk_cam-pool.h"

static struct dma_buf *mtk_cam_buffer_alloc_from_heap(const char *heap_name,
					       size_t size)
{
	struct dma_heap *dma_heap;
	struct dma_buf *dmabuf;

	dma_heap = dma_heap_find(heap_name);
	if (!dma_heap) {
		pr_info("failed to find dma heap: %s\n", heap_name);
		return ERR_PTR(-ENOMEM);
	}

	dmabuf = dma_heap_buffer_alloc(dma_heap, size,
				       O_RDWR | O_CLOEXEC,
				       DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dmabuf))  {
		pr_info("dma_heap_buffer_alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	return dmabuf;
}

struct dma_buf *mtk_cam_cached_buffer_alloc(size_t size)
{
	return mtk_cam_buffer_alloc_from_heap("mtk_mm", size);
}

struct dma_buf *mtk_cam_noncached_buffer_alloc(size_t size)
{
	return mtk_cam_buffer_alloc_from_heap("mtk_mm-uncached", size);
}

static unsigned long _get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

int mtk_cam_device_buf_init(struct mtk_cam_device_buf *buf,
			    struct dma_buf *dbuf,
			    struct device *dev,
			    size_t expected_size)
{
	unsigned long size;

	memset(buf, 0, sizeof(*buf));

	buf->db_attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(buf->db_attach)) {
		dev_info(dev, "failed to attach dbuf: %s\n", dev_name(dev));
		return -1;
	}

	buf->dma_sgt = dma_buf_map_attachment(buf->db_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->dma_sgt)) {
		dev_info(dev, "failed to map attachment\n");
		goto fail_detach;
	}

	/* check size */
	size = _get_contiguous_size(buf->dma_sgt);
	if (expected_size < size) {
		dev_info(dev, "%s: dma_sgt size(%zu) smaller than expected(%zu)\n",
			 __func__, size, expected_size);
		goto fail_attach_unmap;
	}

	buf->dbuf = dbuf;
	buf->size = expected_size;
	buf->daddr = sg_dma_address(buf->dma_sgt->sgl);

	get_dma_buf(dbuf);

	return 0;

fail_attach_unmap:
	dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt, DMA_BIDIRECTIONAL);
	buf->dma_sgt = NULL;
fail_detach:
	dma_buf_detach(buf->dbuf, buf->db_attach);
	buf->db_attach = NULL;
	return -1;
}

void mtk_cam_device_buf_uninit(struct mtk_cam_device_buf *buf)
{
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(buf->vaddr);

	WARN_ON(!buf->dbuf || !buf->size);

	if (buf->dma_sgt) {
		dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
					 DMA_BIDIRECTIONAL);
		buf->dma_sgt = NULL;
		buf->daddr = 0;
	}

	if (buf->vaddr) {
		dma_buf_vunmap(buf->dbuf, &map);
		buf->vaddr = NULL;
	}

	if (buf->db_attach) {
		dma_buf_detach(buf->dbuf, buf->db_attach);
		buf->db_attach = NULL;
	}

	dma_heap_buffer_free(buf->dbuf);
}

int mtk_cam_device_buf_vmap(struct mtk_cam_device_buf *buf)
{
	struct dma_buf_map map;

	WARN_ON(buf->vaddr);

	if (!dma_buf_vmap(buf->dbuf, &map))
		buf->vaddr = map.vaddr;

	return 0;
}

#define IS_AVAILABLE(priv)	(priv & 0x80)
#define INDEX(priv)		(priv & 0x7f)
#define MARK_AVAILABLE(index)	(index)
#define MARK_UNAVAILABLE(index)	(0x80 | (index))

static void _pool_init_elements(struct mtk_cam_pool *pool,
				dma_addr_t da, void *va, size_t per_size)
{
	struct mtk_cam_pool_buffer *buf = pool->buffers;
	int i;

	for (i = 0; i < pool->n_buffers; i++, buf++) {
		buf->daddr = da;
		buf->vaddr = va;
		buf->pool = pool;
		buf->_priv = i;

		da += per_size;
		va += per_size;
	}
}

int mtk_cam_pool_alloc(struct mtk_cam_pool *pool,
		       struct mtk_cam_device_buf *buf, int n_buffers)
{
	WARN_ON(!pool || !buf || !n_buffers);
	WARN_ON(n_buffers >= (1 << 7)); /* max 2^7 */

	if (!buf->daddr) {
		pr_info("buf is not mapped yet\n");
		return -1;
	}

	pool->n_buffers = n_buffers;
	pool->buffers = kcalloc(n_buffers, sizeof(*pool->buffers), GFP_KERNEL);
	if (!pool->buffers)
		return -ENOMEM;

	_pool_init_elements(pool, buf->daddr, buf->vaddr,
			    buf->size / n_buffers);

	spin_lock_init(&pool->lock);
	pool->fetch_idx = 0;

	return 0;
}

void mtk_cam_pool_destroy(struct mtk_cam_pool *pool)
{
	int i;

	if (pool->buffers) {
		// check buf status
		spin_lock(&pool->lock);
		for (i = 0; i < pool->n_buffers; i++)
			if (!IS_AVAILABLE(pool->buffers[i]._priv))
				pr_info("buf idx %d is not returned yet\n", i);
		spin_unlock(&pool->lock);

		kfree(pool->buffers);
	}

	memset(pool, 0, sizeof(*pool));
}

int mtk_cam_pool_buffer_fetch(struct mtk_cam_pool *pool,
			      struct mtk_cam_pool_buffer *buf)
{
	int n = pool->n_buffers;
	int i, c;

	spin_lock(&pool->lock);
	for (i = pool->fetch_idx, c = 0; c < n; i = (i + 1) % n)
		if (IS_AVAILABLE(pool->buffers[i]._priv)) {
			*buf = pool->buffers[i];
			pool->buffers[i]._priv = MARK_UNAVAILABLE(i);

			pool->fetch_idx = (i + 1) % n;
			break;
		}
	spin_unlock(&pool->lock);

	return (c == n) ? -1 : 0;
}

void mtk_cam_pool_buffer_return(struct mtk_cam_pool_buffer *buf)
{
	struct mtk_cam_pool *pool = buf->pool;
	struct mtk_cam_pool_buffer *dst;
	int i = INDEX(buf->_priv);

	dst = &pool->buffers[i];

	/* already return */
	WARN_ON(IS_AVAILABLE(dst->_priv));

	spin_lock(&pool->lock);
	dst->_priv = MARK_AVAILABLE(i);
	spin_unlock(&pool->lock);
}

