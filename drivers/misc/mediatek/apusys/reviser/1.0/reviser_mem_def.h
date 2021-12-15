// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_MEM_DEF_H__
#define __APUSYS_REVISER_MEM_DEF_H__

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>

struct reviser_mem {
	uint64_t uva;
	uint64_t kva;
	uint32_t iova;
	uint32_t size;

	uint32_t align;
	uint32_t cache;
	uint32_t type;

	uint64_t handle;
	struct sg_table sgt;

};

#endif
