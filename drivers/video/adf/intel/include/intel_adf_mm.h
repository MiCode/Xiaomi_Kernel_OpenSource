/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_ADF_MM_H_
#define INTEL_ADF_MM_H_

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/kernel.h>
#include <linux/wait.h>

#include "core/intel_dc_config.h"

enum intel_dma_buf_type {
	INTEL_DMA_BUF_ALLOCATED,
	INTEL_DMA_BUF_IMPORTED,
};

struct intel_adf_mm {
	struct device *parent;
	struct intel_dc_memory *mem;
};

extern int intel_adf_mm_alloc_buf(struct intel_adf_mm *mm,
	u32 size, struct dma_buf **buf);
extern void intel_adf_mm_free_buf(struct dma_buf *buf);
extern int intel_adf_mm_export(struct intel_adf_mm *mm, struct page **pages,
	u32 page_num, void *vaddr, enum intel_dma_buf_type type,
	struct dma_buf **buf);
extern int intel_adf_mm_fd(struct dma_buf *buf);
extern int intel_adf_mm_gtt(struct dma_buf *buf, u32 *gtt);
extern int intel_adf_mm_init(struct intel_adf_mm *mm, struct device *parent,
			struct intel_dc_memory *mem);
extern void intel_adf_mm_destroy(struct intel_adf_mm *mm);

#endif /* INTEL_ADF_MM_H_ */
