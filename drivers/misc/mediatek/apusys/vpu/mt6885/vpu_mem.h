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

#ifndef __VPU_MEM_H__
#define __VPU_MEM_H__

#include <linux/scatterlist.h>

#ifdef CONFIG_MTK_M4U
#include <m4u.h>

#define	VPU_MVA_START_FROM (M4U_FLAGS_START_FROM)
#define VPU_MVA_SG_READY   (M4U_FLAGS_SG_READY)
#else
#define	VPU_MVA_START_FROM (0)
#define VPU_MVA_SG_READY   (1)
#endif

struct vpu_mem_param {
	uint32_t size;
	bool require_pa;
	bool require_va;
	uint32_t fixed_addr;
};

struct vpu_mem {
	void *handle;
	uint64_t va;
	uint32_t pa;
	uint32_t length;
};

int vpu_init_mem(void);
void vpu_exit_mem(void);

// interface to ION
int vpu_mem_alloc(struct vpu_mem **m, struct vpu_mem_param *param);
void vpu_mem_free(struct vpu_mem **m);
int vpu_mem_flush(struct vpu_mem *m);

// interface to M4U
int vpu_mva_alloc(unsigned long va, struct sg_table *sg,
	unsigned int size, unsigned int flags,
	unsigned int *pMva);

int vpu_mva_free(const unsigned int mva);

#endif

