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

#include <linux/slab.h>

#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>

#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include "apusys_cmn.h"
#include "apusys_drv.h"
#include "memory_mgt.h"
#include "memory_dma.h"



int dma_mem_alloc(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	dma_addr_t dma_addr = 0;

	mem->kva = (uint64_t) dma_alloc_coherent(mem_mgr->dev,
		mem->size, &dma_addr, GFP_KERNEL);

	mem->iova = dma_to_phys(mem_mgr->dev, dma_addr);

	MLOG_DEBUG("iova: %08x kva: %08llx\n", mem->iova, mem->kva);

	return (mem->kva) ? 0 : -ENOMEM;
}

int dma_mem_free(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	dma_free_attrs(mem_mgr->dev, mem->size,
		(void *) mem->kva, mem->iova, 0);
	MLOG_DEBUG("Done\n");
	return 0;
}

int dma_mem_init(struct apusys_mem_mgr *mem_mgr)
{
	/* check init */
	if (mem_mgr->is_init) {
		LOG_INFO("apusys memory mgr is already inited\n");
		return -EALREADY;
	}

	/* init */
	mutex_init(&mem_mgr->list_mtx);
	INIT_LIST_HEAD(&mem_mgr->list);

	mem_mgr->is_init = 1;

	MLOG_DEBUG("done\n");

	return 0;
}
int dma_mem_destroy(struct apusys_mem_mgr *mem_mgr)
{
	int ret = 0;

	if (!mem_mgr->is_init) {
		LOG_INFO("apusys memory mgr is not init, can't destroy\n");
		return -EALREADY;
	}

	mem_mgr->is_init = 0;
	MLOG_DEBUG("done\n");

	return ret;
}
