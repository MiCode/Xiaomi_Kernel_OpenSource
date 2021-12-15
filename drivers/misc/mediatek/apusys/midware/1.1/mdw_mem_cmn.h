// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
