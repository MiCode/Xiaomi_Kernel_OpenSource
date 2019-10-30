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
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#else
#include <mt_iommu.h>
#endif


#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include "apusys_cmn.h"
#include "apusys_drv.h"
#include "memory_mgt.h"
#include "memory_ion.h"

#ifdef MTK_APUSYS_IOMMU_LEGACY
#define APUSYS_IOMMU_PORT M4U_PORT_VPU
#else
#define APUSYS_IOMMU_PORT M4U_PORT_L21_APU_FAKE_DATA
#endif



int _ion_mem_ctl_cache(struct apusys_mem_mgr *mem_mgr,
		struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem)
{
	int ret = 0;

	switch (ctl_data->cache_param.cache_type) {
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



int ion_mem_alloc(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	DEBUG_TAG;
	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	/* allocate buffer by fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* map kernel va*/
	if (ion_mem_map_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}
	/* map iova*/
	if (ion_mem_map_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	LOG_INFO("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return 0;

free_import:
	ion_free(mem_mgr->client, ion_hnd);
	return ret;
}

int ion_mem_free(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd;
	int ret = 0;

	DEBUG_TAG;
	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	/* check argument */
	if (mem->khandle == 0) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	/* unmap iova*/
	if (ion_mem_unmap_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	/* unmap kernel va*/
	if (ion_mem_unmap_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	/* free buffer by fd */
	ion_free(mem_mgr->client, ion_hnd);

	LOG_INFO("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

free_import:
	return ret;
}

int ion_mem_import(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	DEBUG_TAG;
	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}


	/* allocate buffer by fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* map kernel va*/
	if (ion_mem_map_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}
	/* map iova*/
	if (ion_mem_map_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	LOG_INFO("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return 0;

free_import:
	ion_free(mem_mgr->client, ion_hnd);
	return ret;
}

int ion_mem_unimport(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{

	struct ion_handle *ion_hnd;
	int ret = 0;

	DEBUG_TAG;
	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	/* check argument */
	if (mem->khandle == 0) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	/* unmap iova*/
	if (ion_mem_unmap_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	/* unmap kernel va*/
	if (ion_mem_unmap_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	/* free buffer by fd */
	ion_free(mem_mgr->client, ion_hnd);

	LOG_INFO("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

free_import:
	return ret;
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
int ion_mem_ctl(struct apusys_mem_mgr *mem_mgr,
		struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem)
{
	int ret = 0;

	switch (ctl_data->cmd) {
	case APUSYS_CACHE:
		ret = _ion_mem_ctl_cache(mem_mgr, ctl_data, mem);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int ion_mem_map_kva(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	void *buffer = NULL;
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* map kernel va*/
	buffer = ion_map_kernel(mem_mgr->client, ion_hnd);
	if (IS_ERR_OR_NULL(buffer)) {
		LOG_ERR("map kernel va fail(%p/%p)\n",
			mem_mgr->client, ion_hnd);
		ret = -ENOMEM;
		goto free_import;
	}
	mem->khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	LOG_DEBUG("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return 0;

free_import:
	ion_free(mem_mgr->client, ion_hnd);
	return ret;
}

int ion_mem_map_iova(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;
	struct ion_mm_data mm_data;
	unsigned long iova = 0;
	size_t iova_size = 0;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = ion_hnd;
	mm_data.config_buffer_param.module_id = APUSYS_IOMMU_PORT;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 1;

	if (ion_kernel_ioctl(mem_mgr->client, ION_CMD_MULTIMEDIA,
			(unsigned long)&mm_data)) {
		LOG_ERR("ion_config_buffer: ION_CMD_MULTIMEDIA failed\n");
		ret = -ENOMEM;
		goto free_import;
	}

	iova = ((unsigned long) APUSYS_IOMMU_PORT << 24);
	if (ion_phys(mem_mgr->client, ion_hnd, &iova, &iova_size)) {
		LOG_ERR("Get MVA failed\n");
		ret = -ENOMEM;
		goto free_import;
	}

	mem->iova = iova;
	mem->iova_size = iova_size;

	LOG_DEBUG("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);


free_import:
	ion_free(mem_mgr->client, ion_hnd);
	return ret;
}
int ion_mem_unmap_iova(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	/* check argument */
	if (mem->khandle == 0) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	LOG_DEBUG("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return ret;
}

int ion_mem_unmap_kva(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (mem == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	/* check argument */
	if (mem->khandle == 0) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	ion_free(mem_mgr->client, ion_hnd);

	LOG_DEBUG("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return ret;

}
