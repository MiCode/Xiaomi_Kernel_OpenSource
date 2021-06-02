/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
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

#ifndef __VPUBUF_DMA_CONTIG_H__
#define __VPUBUF_DMA_CONTIG_H__

#include "vpubuf-core.h"
#include <linux/dma-mapping.h>

void *vpu_dma_contig_init_ctx(struct device *dev);
void vpu_dma_contig_cleanup_ctx(void *alloc_ctx);

extern const struct vpu_mem_ops vpu_dma_contig_memops;

#endif
