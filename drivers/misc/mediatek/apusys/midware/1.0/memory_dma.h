/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __APUSYS_MEMORY_DMA_H__
#define __APUSYS_MEMORY_DMA_H__

int dma_mem_alloc(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem);
int dma_mem_free(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem);
int dma_mem_init(struct apusys_mem_mgr *mem_mgr);
int dma_mem_destroy(struct apusys_mem_mgr *mem_mgr);

#endif
