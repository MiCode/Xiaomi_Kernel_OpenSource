/* SPDX-License-Identifier: GPL-2.0 */
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
	uint64_t pa;
	uint32_t length;
};

struct vpu_iova {
	/* settings from dts */
	uint32_t addr;  /* iova settng from dts */
	uint32_t size;  /* iova size from dts */
	uint32_t bin;   /* offset in binary */
	/* allocated memory */
	struct vpu_mem m;
	/* allocated iova */
	struct sg_table sgt;
	uint64_t time;  /* allocated time */
	uint64_t iova;  /* allocated iova */
	/* link in vpu driver */
	struct list_head list;
};

void *vpu_vmap(phys_addr_t start, size_t size);

extern struct vpu_mem_ops vpu_mops_v1;
extern struct vpu_mem_ops vpu_mops_v2;

#endif

