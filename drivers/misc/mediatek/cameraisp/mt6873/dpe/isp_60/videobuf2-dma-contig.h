/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MEDIA_VIDEOBUF2_DMA_CONTIG_H
#define _MEDIA_VIDEOBUF2_DMA_CONTIG_H

#include <media/videobuf2-v4l2.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
vb2_dma_contig_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

int vb2_dpe_dma_contig_set_max_seg_size(struct device *dev, unsigned int size);
void vb2_dpe_dma_contig_clear_max_seg_size(struct device *dev);

extern const struct vb2_mem_ops vb2_dpe_dma_contig_memops;
void *vb2_dc_alloc(struct device *dev, unsigned long attrs,
			  unsigned long size, enum dma_data_direction dma_dir,
			  gfp_t gfp_flags);
struct dma_buf *vb2_dc_get_dmabuf(void *buf_priv, unsigned long flags);
void *vb2_dc_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
	unsigned long size, enum dma_data_direction dma_dir);
int vb2_dc_map_dmabuf(void *mem_priv);
void vb2_dc_unmap_dmabuf(void *mem_priv);
void vb2_dc_detach_dmabuf(void *mem_priv);
void vb2_dc_put(void *buf_priv);

extern struct frame_vector *vb2_create_framevec(unsigned long start,
					 unsigned long length,
					 bool write);
#endif
