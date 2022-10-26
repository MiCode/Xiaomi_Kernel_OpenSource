// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/fdtable.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include "ccu_cmn.h"
#include "ccu_mva.h"
#include "ccu_platform_def.h"

struct CcuMemHandle ccu_buffer_handle[2];

int ccu_allocate_mem(struct ccu_device_s *dev, struct CcuMemHandle *memHandle,
			 int size, bool cached)
{
	int ssize = size << 1;
	dma_addr_t dsize = size;

	LOG_DBG("size(%d) cached(%d)\n", ssize, cached);
	// get buffer virtual address
	memHandle->meminfo.size = ssize;
	memHandle->meminfo.cached = cached;

	memHandle->meminfo.va = dma_alloc_attrs(dev->dev, ssize,
		&memHandle->mva, GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);

	if (memHandle->meminfo.va == NULL) {
		LOG_ERR("fail to get buffer kernl virtual address");
		return -1;
	}

	memHandle->align_mva = (memHandle->mva + (dsize - 1)) & ~(dsize - 1);

	LOG_DBG("success: share_fd(%d), size(%x), cached(%d), va(%lx), mva(%lx) align_mva(%lx)\n",
	memHandle->meminfo.shareFd, memHandle->meminfo.size,
	memHandle->meminfo.cached, memHandle->meminfo.va, memHandle->mva,
	memHandle->align_mva);

	memHandle->meminfo.mva = (uint32_t)memHandle->mva;
	memHandle->meminfo.align_mva = (uint32_t)memHandle->align_mva;

	ccu_buffer_handle[memHandle->meminfo.cached] = *memHandle;

	return 0;
}

int ccu_deallocate_mem(struct ccu_device_s *dev, struct CcuMemHandle *memHandle)
{
	struct CcuMemHandle *handle = &ccu_buffer_handle[memHandle->meminfo.cached];

	if (handle->meminfo.va != NULL) {
		dma_free_attrs(dev->dev, handle->meminfo.size,
		handle->meminfo.va, handle->mva, DMA_ATTR_WRITE_COMBINE);
	}
	memset(handle, 0, sizeof(struct CcuMemHandle));

	return 0;
}

struct CcuMemInfo *ccu_get_binary_memory(void)
{
	if (ccu_buffer_handle[0].meminfo.va != NULL)
		return &ccu_buffer_handle[0].meminfo;
	LOG_ERR("ccu ddr va not found!\n");
	return NULL;
}
