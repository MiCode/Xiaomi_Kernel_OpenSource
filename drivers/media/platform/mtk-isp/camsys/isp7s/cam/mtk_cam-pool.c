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

#define POOL_DEBUG 0
#define pool_dbg(fmt, arg...)					\
	do {							\
		if (POOL_DEBUG)					\
			pr_info("%s: " fmt, __func__, ##arg);	\
	} while (0)

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

	buf->dbuf = dbuf;
	buf->db_attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(buf->db_attach)) {
		dev_info(dev, "failed to attach dbuf: %s\n", dev_name(dev));
		return -1;
	}

	buf->dma_sgt = dma_buf_map_attachment(buf->db_attach,
					      DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->dma_sgt)) {
		dev_info(dev, "failed to map attachment\n");
		goto fail_detach;
	}

	/* check size */
	size = _get_contiguous_size(buf->dma_sgt);
	if (expected_size > size) {
		dev_info(dev,
			 "%s: dma_sgt size(%zu) smaller than expected(%zu)\n",
			 __func__, size, expected_size);
		goto fail_attach_unmap;
	}

	buf->size = expected_size;
	buf->daddr = sg_dma_address(buf->dma_sgt->sgl);

	get_dma_buf(dbuf);

	return 0;

fail_attach_unmap:
	dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
	buf->dma_sgt = NULL;
fail_detach:
	dma_buf_detach(buf->dbuf, buf->db_attach);
	buf->db_attach = NULL;
	buf->dbuf = NULL;
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

static inline void *
element_at(struct mtk_cam_pool *pool, int index)
{
	return pool->elements + index * pool->element_size;
}

static inline struct mtk_cam_pool_priv *
element_priv(void *element)
{
	return element;
}

int mtk_cam_pool_alloc(struct mtk_cam_pool *pool,
		       size_t element_size, int n_element)
{
	WARN_ON(!pool || !n_element || !element_size);
	WARN_ON(n_element >= (1 << 7)); /* max 2^7 */

	pool->n_element = n_element;
	pool->element_size = element_size;
	pool->elements = kvcalloc(n_element, pool->element_size,
				  GFP_KERNEL);
	if (!pool->elements)
		return -ENOMEM;

	spin_lock_init(&pool->lock);
	pool->fetch_idx = 0;
	pool->available_cnt = pool->n_element;

	pool_dbg("pool %p: size %zu, n = %d\n",
		 pool, pool->element_size, pool->n_element);

	return 0;
}

void mtk_cam_pool_destroy(struct mtk_cam_pool *pool)
{
	struct mtk_cam_pool_priv *priv;
	int i;

	if (pool->elements) {

		// check buf status
		if (pool->available_cnt != pool->n_element)
			for (i = 0; i < pool->n_element; i++) {
				priv = element_priv(element_at(pool, i));

				if (!priv->available)
					pr_info("buf idx %d is not returned yet\n",
						i);
			}

		kvfree(pool->elements);
	}

	memset(pool, 0, sizeof(*pool));
}

int mtk_cam_pool_config(struct mtk_cam_pool *pool,
			fn_config_element fn, void *data)
{
	void *ele;
	struct mtk_cam_pool_priv *priv;
	int i;

	if (!pool->elements || !pool->n_element)
		return -EINVAL;

	for (i = 0; i < pool->n_element; i++) {
		ele = element_at(pool, i);
		priv = element_priv(ele);

		priv->pool = pool;
		priv->index = i;
		priv->available = true;

		fn(data, i, ele);
	}

	return 0;
}

int mtk_cam_pool_fetch(struct mtk_cam_pool *pool,
		       void *buf, size_t size)
{
	int n = pool->n_element;
	void *ele;
	struct mtk_cam_pool_priv *priv;
	int i, c;

	if (WARN_ON(size != pool->element_size))
		return -EINVAL;

	spin_lock(&pool->lock);
	for (i = pool->fetch_idx, c = 0; c < n; i = (i + 1) % n) {
		ele = element_at(pool, i);
		priv = element_priv(ele);

		if (priv->available) {
			memcpy(buf, ele, pool->element_size);
			priv->available = false;
			pool->fetch_idx = (i + 1) % n;
			--pool->available_cnt;
			break;
		}
	}
	spin_unlock(&pool->lock);

	pool_dbg("pool %p, idx %d fetch_idx %d available_cnt %d\n",
		 pool,
		 ((struct mtk_cam_pool_priv *)buf)->index,
		 pool->fetch_idx,
		 pool->available_cnt);

	return (c == n) ? -1 : 0;
}

void mtk_cam_pool_return(void *buf, size_t size)
{
	struct mtk_cam_pool *pool;
	struct mtk_cam_pool_priv *priv;
	void *ele;
	int i;

	priv = buf;
	pool = priv->pool;
	i = priv->index;

	if (WARN_ON(size != pool->element_size))
		return;

	ele = element_at(pool, i);
	priv = element_priv(ele);

	/* already return */
	WARN_ON(priv->available);

	spin_lock(&pool->lock);
	priv->available = true;
	++pool->available_cnt;
	spin_unlock(&pool->lock);

	pool_dbg("pool %p, idx %d fetch_idx %d available_cnt %d\n",
		 pool,
		 i,
		 pool->fetch_idx,
		 pool->available_cnt);
}

int mtk_cam_pool_available_cnt(struct mtk_cam_pool *pool)
{
	int cnt;

	spin_lock(&pool->lock);
	cnt = pool->available_cnt;
	spin_unlock(&pool->lock);
	return cnt;
}

struct buffer_pool_data {
	struct mtk_cam_device_buf *buf;
	int offset;
};

static void pool_config_device_buf(void *data, int index, void *element)
{
	struct buffer_pool_data *bf_data = data;
	struct mtk_cam_device_buf *buf = bf_data->buf;
	size_t ofst = index * bf_data->offset;
	struct mtk_cam_pool_buffer *ele_buf = element;

	ele_buf->daddr = buf->daddr + ofst;
	ele_buf->vaddr = buf->vaddr ? (buf->vaddr + ofst) : 0;
	ele_buf->size = bf_data->offset;
}

int mtk_cam_buffer_pool_alloc(struct mtk_cam_pool *pool,
			      struct mtk_cam_device_buf *buf, int n_buffers)
{
	struct buffer_pool_data data;
	int ret;

	if (WARN_ON(!buf || !buf->daddr)) {
		pr_info("buf is not mapped yet\n");
		return -EINVAL;
	}

	if (n_buffers <= 0)
		return -EINVAL;

	data.buf = buf;
	data.offset = buf->size / n_buffers;

	ret = mtk_cam_pool_alloc(pool,
				 sizeof(struct mtk_cam_pool_buffer), n_buffers);
	if (ret)
		return ret;

	ret = mtk_cam_pool_config(pool, pool_config_device_buf, &data);
	if (ret)
		mtk_cam_pool_destroy(pool);

	return ret;
}
