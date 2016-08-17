/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MEDIA_VIDEOBUF2_DMA_NVMAP_H
#define _MEDIA_VIDEOBUF2_DMA_NVMAP_H

#include <media/videobuf2-core.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
vb2_dma_nvmap_plane_paddr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *paddr = vb2_plane_cookie(vb, plane_no);

	return *paddr;
}

void *vb2_dma_nvmap_init_ctx(struct device *dev);
void vb2_dma_nvmap_cleanup_ctx(void *alloc_ctx);

extern const struct vb2_mem_ops vb2_dma_nvmap_memops;

#endif
