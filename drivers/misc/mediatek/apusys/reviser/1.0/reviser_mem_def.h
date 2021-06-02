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
