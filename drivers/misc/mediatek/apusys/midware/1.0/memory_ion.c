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
#include "memory_ion.h"

int _ion_mem_ctl_cache(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	int ret = 0;

	switch (mem->ctl_data.cache_param.cache_type) {
	case APUSYS_CACHE_SYNC:
		LOG_ERR("ION Not Support APUSYS_CACHE_SYNC\n");
		ret = -1;
		break;
	case APUSYS_CACHE_INVALIDATE:
		LOG_ERR("ION Not Support APUSYS_CACHE_INVALIDATE\n");
		ret = -1;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int _ion_mem_ctl_map(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	int ret = 0;

	DEBUG_TAG;
	switch (mem->ctl_data.map_param.map_type) {
	case APUSYS_MAP_KVA:
		ret = ion_mem_map_kva(mem_mgr, mem);
		break;
	case APUSYS_UNMAP_KVA:
		ret = ion_mem_unmap_kva(mem_mgr, mem);
		break;
	case APUSYS_MAP_IOVA:
		LOG_ERR("Not Support APUSYS_MAP_IOVA!");
		break;
	case APUSYS_UNMAP_IOVA:
		LOG_ERR("Not Support APUSYS_UNMAP_IOVA!");
		break;
	case APUSYS_MAP_PA:
		LOG_ERR("Not Support APUSYS_MAP_PA!");
		break;
	case APUSYS_UNMAP_PA:
		LOG_ERR("Not Support APUSYS_UNMAP_PA!");
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int ion_mem_map_iova(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	int ret = 0;

	return ret;
}

int ion_mem_alloc(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	void *buffer = NULL;
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d/%d)\n",
		mem->iova, mem->uva, mem->kva, mem->size,
		mem->ion_data.ion_share_fd);
	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->ion_data.ion_share_fd);

	if (ion_hnd == NULL)
		return -ENOMEM;

	LOG_DEBUG("mem(%d/%p)\n", mem->ion_data.ion_share_fd, ion_hnd);

	/* map kernel va*/
	buffer = ion_map_kernel(mem_mgr->client, ion_hnd);
	if (buffer == NULL) {
		LOG_ERR("map kernel va fail(%d/%p)\n",
			mem_mgr->client, ion_hnd);
		ret = -ENOMEM;
	}
	mem->ion_data.ion_khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d)\n",
		mem->iova, mem->uva, mem->kva, mem->size);
	//LOG_DEBUG("try to write kva(%p/0x%llx)...\n", buffer, mem->kva);
	//memset(buffer, 0, 10);
	//LOG_DEBUG("write done...\n");

	/* free ref count from import */
	//ion_free(mem_mgr->client, ion_hnd);
	//ion_free(g_mem_mgr.client, ion_hnd);
	//LOG_DEBUG("try to write kva(%p)...\n", buffer);
	//memset(buffer, 0, 10);
	//LOG_DEBUG("write done...\n");

	return ret;
}

int ion_mem_free(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{

	struct ion_handle *ion_hnd;

	ion_hnd = (struct ion_handle *) mem->ion_data.ion_khandle;
	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d/%d,%p)\n",
		mem->iova, mem->uva, mem->kva, mem->size,
		mem->ion_data.ion_share_fd, ion_hnd);

	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	//ion_hnd = (struct ion_handle *) mem->ion_khandle;
	ion_free(mem_mgr->client, ion_hnd);
	//ion_free(mem_mgr->client, ion_hnd);

	return 0;
}

int ion_mem_init(struct apusys_mem_mgr *mem_mgr)
{
	/* check init */
	if (mem_mgr->is_init) {
		LOG_INFO("apusys memory mgr is already inited\n");
		return -EALREADY;
	}

	/* create ion client */
	mem_mgr->client = ion_client_create(g_ion_device, "apusys midware");
	if (IS_ERR_OR_NULL(mem_mgr->client)) {
		LOG_ERR("create ion client fail\n");
		return -ENOMEM;
	}

	/* init */
	mutex_init(&mem_mgr->list_mtx);
	INIT_LIST_HEAD(&mem_mgr->list);

	mem_mgr->is_init = 1;

	LOG_DEBUG("done\n");

	return 0;
}
int ion_mem_destroy(struct apusys_mem_mgr *mem_mgr)
{
	int ret = 0;

	if (!mem_mgr->is_init) {
		LOG_INFO("apusys memory mgr is not init, can't destroy\n");
		return -EALREADY;
	}

	mem_mgr->is_init = 0;
	ion_client_destroy(mem_mgr->client);
	mem_mgr->client = NULL;
	LOG_DEBUG("done\n");

	return ret;
}
int ion_mem_ctl(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	int ret = 0;

	switch (mem->ctl_data.cmd) {
	case APUSYS_CACHE:
		ret = _ion_mem_ctl_cache(mem_mgr, mem);
		break;
	case APUSYS_MAP:
		ret = _ion_mem_ctl_map(mem_mgr, mem);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int ion_mem_map_kva(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	void *buffer = NULL;
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d/%d)\n",
		mem->iova, mem->uva, mem->kva, mem->size,
		mem->ion_data.ion_share_fd);
	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->ion_data.ion_share_fd);

	if (ion_hnd == NULL)
		return -ENOMEM;

	LOG_DEBUG("mem(%d/%p)\n",
		mem->ion_data.ion_share_fd, ion_hnd);

	/* map kernel va*/
	buffer = ion_map_kernel(mem_mgr->client, ion_hnd);
	if (buffer == NULL) {
		LOG_ERR("map kernel va fail(%d/%p)\n",
			mem_mgr->client, ion_hnd);
		ret = -ENOMEM;
	}
	mem->ion_data.ion_khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d)\n",
		mem->iova, mem->uva, mem->kva, mem->size);
	//LOG_DEBUG("try to write kva(%p/0x%llx)...\n", buffer, mem->kva);
	//memset(buffer, 0, 10);
	//LOG_DEBUG("write done...\n");

	/* free ref count from import */
	//ion_free(mem_mgr->client, ion_hnd);
	//ion_free(g_mem_mgr.client, ion_hnd);
	//LOG_DEBUG("try to write kva(%p)...\n", buffer);
	//memset(buffer, 0, 10);
	//LOG_DEBUG("write done...\n");

	return ret;
}

int ion_mem_unmap_iova(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	int ret = 0;

	return ret;
}

int ion_mem_unmap_kva(struct apusys_mem_mgr *mem_mgr, struct apusys_mem *mem)
{
	struct ion_handle *ion_hnd;

	ion_hnd = (struct ion_handle *) mem->ion_data.ion_khandle;
	LOG_DEBUG("mem(0x%x/0x%llx/0x%llx/%d/%d,%p)\n",
		mem->iova, mem->uva, mem->kva, mem->size,
		mem->ion_data.ion_share_fd, ion_hnd);

	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	//ion_hnd = (struct ion_handle *) mem->ion_khandle;
	ion_free(mem_mgr->client, ion_hnd);
	//ion_free(mem_mgr->client, ion_hnd);
	return 0;
}
