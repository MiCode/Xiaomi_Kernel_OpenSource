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

#ifndef __APUSYS_MDW_MEM_H__
#define __APUSYS_MDW_MEM_H__

#include "apusys_drv.h"
#include "apusys_device.h"

enum {
	APUSYS_MEM_PROP_NONE,
	APUSYS_MEM_PROP_ALLOC,
	APUSYS_MEM_PROP_IMPORT,
	APUSYS_MEM_PROP_MAP,
	APUSYS_MEM_PROP_MAX,
};

struct mdw_mem {
	struct apusys_kmem kmem;
	struct list_head m_item; // to mem list
	struct list_head u_item; // to usr list
};

int mdw_mem_alloc(struct mdw_mem *m);
int mdw_mem_free(struct mdw_mem *m);
int mdw_mem_map(struct mdw_mem *m);
int mdw_mem_unmap(struct mdw_mem *m);
int mdw_mem_import(struct mdw_mem *m);
int mdw_mem_unimport(struct mdw_mem *m);
int mdw_mem_flush(struct apusys_kmem *km);
int mdw_mem_invalidate(struct apusys_kmem *km);
int mdw_mem_map_iova(struct apusys_kmem *km);
int mdw_mem_map_kva(struct apusys_kmem *km);
int mdw_mem_unmap_iova(struct apusys_kmem *km);
int mdw_mem_unmap_kva(struct apusys_kmem *km);
int mdw_mem_init(void);
void mdw_mem_exit(void);

unsigned int mdw_mem_get_support(void);
void mdw_mem_get_vlm(unsigned int *start, unsigned int *size);

void mdw_mem_u2k(struct apusys_mem *umem, struct apusys_kmem *kmem);
void mdw_mem_k2u(struct apusys_kmem *kmem, struct apusys_mem *umem);
int mdw_mem_create_idr(uint64_t handle, int *id);
int mdw_mem_u2k_handle(struct apusys_kmem *kmem, struct apusys_mem *umem);
int mdw_mem_delete_idr(int id);

#endif
