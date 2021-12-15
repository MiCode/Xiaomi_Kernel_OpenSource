// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VPU_MEM_H__
#define __VPU_MEM_H__

#include <linux/platform_device.h>
#include <linux/scatterlist.h>

#define VPU_MEM_ALLOC  (0xFFFFFFFF)

struct vpu_mem {
	void *handle;
	unsigned long va;
	uint32_t pa;
	uint32_t length;
};

struct vpu_iova {
	/* settings from dts */
	uint32_t addr;  /* iova */
	uint32_t size;
	uint32_t bin;   /* offset in binary */
	/* allocated memory */
	struct vpu_mem m;
	/* allocated iova */
	struct sg_table sgt;
};

dma_addr_t vpu_iova_alloc(struct platform_device *pdev,
	struct vpu_iova *i);

void vpu_iova_free(struct device *dev, struct vpu_iova *i);

void vpu_iova_sync_for_device(struct device *dev,
	struct vpu_iova *i);

void vpu_iova_sync_for_cpu(struct device *dev,
	struct vpu_iova *i);

int vpu_iova_dts(struct platform_device *pdev,
	const char *name, struct vpu_iova *i);

void *vpu_vmap(phys_addr_t start, size_t size,
	unsigned int memtype);

#endif

