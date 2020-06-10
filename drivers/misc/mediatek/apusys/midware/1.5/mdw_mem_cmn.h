/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_MEM_CMN_H__
#define __APUSYS_MDW_MEM_CMN_H__

#include "mdw_mem.h"

/* memory operations */
struct mdw_mem_ops {
	int (*init)(void);
	void (*exit)(void);
	int (*alloc)(struct apusys_kmem *mem);
	int (*free)(struct apusys_kmem *mem);
	int (*flush)(struct apusys_kmem *mem);
	int (*invalidate)(struct apusys_kmem *mem);
	int (*map_kva)(struct apusys_kmem *mem);
	int (*unmap_kva)(struct apusys_kmem *mem);
	int (*map_iova)(struct apusys_kmem *mem);
	int (*unmap_iova)(struct apusys_kmem *mem);
};

struct mdw_mem_ops *mdw_mops_dmy(void);
struct mdw_mem_ops *mdw_mops_aosp(void);
struct mdw_mem_ops *mdw_mops_ion(void);

#endif
