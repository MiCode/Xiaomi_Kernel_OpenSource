/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Rick Chang <rick.chang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _MEDIA_MTK_DMA_CONTIG_H
#define _MEDIA_MTK_DMA_CONTIG_H

#include <media/videobuf2-v4l2.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
mtk_dma_contig_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

int mtk_dma_contig_set_max_seg_size(struct device *dev, unsigned int size);
void mtk_dma_contig_clear_max_seg_size(struct device *dev);
void mtk_dma_contig_set_secure_mode(struct device *dev, int secure_mode);

extern const struct vb2_mem_ops mtk_dma_contig_memops;

#endif
