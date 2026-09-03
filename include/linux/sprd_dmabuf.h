/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Unisoc Inc.
 */

#ifndef _SPRD_DMABUF_H
#define _SPRD_DMABUF_H

#include <uapi/linux/dma-heap.h>
#include <uapi/linux/sprd_dmabuf.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>

struct dmabuf_phy_data {
	__u32 fd;
	__u64 len;
	__u64 addr;
};

int sprd_dmabuf_get_sysbuffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size);
int sprd_dmabuf_get_carvebuffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size);
int sprd_dmabuf_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size);
int sprd_dmabuf_map_kernel(struct dma_buf *dmabuf, struct dma_buf_map *map);
int sprd_dmabuf_unmap_kernel(struct dma_buf *dmabuf, struct dma_buf_map *map);

#endif /* _SPRD_DMABUF_H */
