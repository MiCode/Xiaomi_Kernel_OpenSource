/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_MEM_CMN_H__
#define __APUSYS_MDW_MEM_CMN_H__

#include "mdw_mem.h"

struct mdw_mem_ops {
	int (*alloc)(struct apusys_kmem *mem);
	int (*free)(struct apusys_kmem *mem);
	int (*flush)(struct apusys_kmem *mem);
	int (*invalidate)(struct apusys_kmem *mem);
	int (*map_kva)(struct apusys_kmem *mem);
	int (*unmap_kva)(struct apusys_kmem *mem);
	int (*map_iova)(struct apusys_kmem *mem);
	int (*unmap_iova)(struct apusys_kmem *mem);
	void (*destroy)(void);
};

/* ion allocator */
struct mdw_mem_ops *mdw_mem_ion_init(void);

#endif
