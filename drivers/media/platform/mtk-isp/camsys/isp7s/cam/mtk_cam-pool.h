/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_CAM_POOL_H
#define __MTK_CAM_POOL_H

struct dma_buf;
struct dma_buf *mtk_cam_cached_buffer_alloc(size_t size);
struct dma_buf *mtk_cam_noncached_buffer_alloc(size_t size);

struct mtk_cam_device_buf {
	struct dma_buf *dbuf;
	size_t size;
	struct dma_buf_attachment *db_attach;
	struct sg_table *dma_sgt;

	dma_addr_t daddr;
	void *vaddr;
};

/* attach + map_attachment + get_dma_buf */
int mtk_cam_device_buf_init(struct mtk_cam_device_buf *buf,
			    struct dma_buf *dbuf,
			    struct device *dev,
			    size_t expected_size);

/* detach + unmap_attachment + dma_buf_put */
void mtk_cam_device_buf_uninit(struct mtk_cam_device_buf *buf);
int mtk_cam_device_buf_vmap(struct mtk_cam_device_buf *buf);
static inline int mtk_cam_device_buf_fd(struct mtk_cam_device_buf *buf)
{
	return dma_buf_fd(buf->dbuf, O_RDWR | O_CLOEXEC);
}

/* a wrapper to divide buffer into chunks as buffer pool */
struct mtk_cam_pool_buffer {
	dma_addr_t daddr;
	void *vaddr;

	struct mtk_cam_pool *pool;
	char _priv;
};

struct mtk_cam_pool {
	size_t n_buffers;
	struct mtk_cam_pool_buffer *buffers;

	spinlock_t lock;
	int fetch_idx;
};

int mtk_cam_pool_alloc(struct mtk_cam_pool *pool,
		      struct mtk_cam_device_buf *buf, int n_buffers);
void mtk_cam_pool_destroy(struct mtk_cam_pool *pool);

int mtk_cam_pool_buffer_fetch(struct mtk_cam_pool *pool,
			      struct mtk_cam_pool_buffer *buf);
void mtk_cam_pool_buffer_return(struct mtk_cam_pool_buffer *buf);

#endif //__MTK_CAM_POOL_H
