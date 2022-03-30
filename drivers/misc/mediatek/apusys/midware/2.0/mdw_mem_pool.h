/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_MEM_POOL_H__
#define __MTK_APU_MDW_MEM_POOL_H__

#include "mdw.h"

int mdw_mem_pool_create(struct mdw_fpriv *mpriv, struct mdw_mem_pool *pool,
	enum mdw_mem_type type,	uint32_t size, uint32_t align, uint64_t flags);
void mdw_mem_pool_destroy(struct mdw_mem_pool *pool);

struct mdw_mem *mdw_mem_pool_alloc(struct mdw_mem_pool *pool, uint32_t size,
	uint32_t align);
void mdw_mem_pool_free(struct mdw_mem *m);

int mdw_mem_pool_flush(struct mdw_mem *m);
int mdw_mem_pool_invalidate(struct mdw_mem *m);

#endif

