/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _AIE_MEDIA_VIDEOBUF2_DMA_CONTIG_H
#define _AIE_MEDIA_VIDEOBUF2_DMA_CONTIG_H

#include <media/videobuf2-v4l2.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
aie_vb2_dma_contig_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

int aie_vb2_dma_contig_set_max_seg_size(struct device *dev, unsigned int size);
void aie_vb2_dma_contig_clear_max_seg_size(struct device *dev);
unsigned long aie_vb2_dc_get_contiguous_size(struct sg_table *sgt);
extern const struct vb2_mem_ops aie_vb2_dma_contig_memops;

#endif
