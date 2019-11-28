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

#ifndef __APUSYS_MEMORY_MGT_H__
#define __APUSYS_MEMORY_MGT_H__

#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>
#include "apusys_device.h"


enum {
	APUSYS_MEM_PROP_NONE,
	APUSYS_MEM_PROP_ALLOC,
	APUSYS_MEM_PROP_IMPORT,
	APUSYS_MEM_PROP_MAX,
};

struct apusys_mem_mgr {
	struct ion_client *client;

	struct list_head list;
	struct mutex list_mtx;
	struct device *dev;
	uint8_t is_init;
};
int apusys_mem_copy_from_user(
		struct apusys_mem *umem,
		struct apusys_kmem *kmem);
int apusys_mem_copy_to_user(
		struct apusys_mem *umem,
		struct apusys_kmem *kmem);

int apusys_mem_alloc(struct apusys_kmem *mem);
int apusys_mem_free(struct apusys_kmem *mem);
int apusys_mem_import(struct apusys_kmem *mem);
int apusys_mem_unimport(struct apusys_kmem *mem);
int apusys_mem_release(struct apusys_kmem *mem);
int apusys_mem_flush(struct apusys_kmem *mem);
int apusys_mem_invalidate(struct apusys_kmem *mem);
int apusys_mem_init(struct device *dev);
int apusys_mem_destroy(void);
int apusys_mem_ctl(struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem);
int apusys_mem_map_iova(struct apusys_kmem *mem);
int apusys_mem_map_kva(struct apusys_kmem *mem);
int apusys_mem_unmap_iova(struct apusys_kmem *mem);
int apusys_mem_unmap_kva(struct apusys_kmem *mem);

unsigned int apusys_mem_get_support(void);
int apusys_mem_get_vlm(unsigned int *start, unsigned int *size);

#endif
