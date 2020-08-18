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

#include "mdw_cmn.h"
#include "apusys_drv.h"
#include "memory_mgt.h"
#include "memory_ion.h"
#include "apusys_user.h"
#include "memory_dump.h"

#ifdef MTK_APUSYS_IOMMU_LEGACY
#define APUSYS_IOMMU_PORT M4U_PORT_VPU
#else
#define APUSYS_IOMMU_PORT M4U_PORT_L21_APU_FAKE_DATA
#endif

static int _ion_mem_ctl_cache(struct apusys_mem_mgr *mem_mgr,
		struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem);
static int _ion_mem_check_arg(struct apusys_kmem *mem);

static int _ion_mem_ctl_cache(struct apusys_mem_mgr *mem_mgr,
		struct apusys_mem_ctl *ctl_data, struct apusys_kmem *mem)
{
	int ret = 0;

	switch (ctl_data->cache_param.cache_type) {
	case APUSYS_CACHE_SYNC:
		mdw_drv_err("ION Not Support APUSYS_CACHE_SYNC\n");
		ret = -1;
		break;
	case APUSYS_CACHE_INVALIDATE:
		mdw_drv_err("ION Not Support APUSYS_CACHE_INVALIDATE\n");
		ret = -1;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
/* check argument */
static int _ion_mem_check_arg(struct apusys_kmem *mem)
{
	int ret = 0;

	if (mem == NULL) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	if ((mem->align != 0) &&
		((mem->align > APUSYS_ION_PAGE_SIZE) ||
		((APUSYS_ION_PAGE_SIZE % mem->align) != 0))) {
		mdw_drv_err("align argument invalid (%d)\n", mem->align);
		return -EINVAL;
	}
	if (mem->cache > 1) {
		mdw_drv_err("Cache argument invalid (%d)\n", mem->cache);
		return -EINVAL;
	}
	if ((mem->iova_size % APUSYS_ION_PAGE_SIZE) != 0) {
		mdw_drv_err("iova_size argument invalid 0x%x\n",
			mem->iova_size);
		return -EINVAL;
	}

	return ret;
}

int ion_mem_alloc(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	mdw_lne_debug();
	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
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

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

	return 0;

free_import:
	mdw_drv_err("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);
	ion_free(mem_mgr->client, ion_hnd);
	apusys_user_print_log();
	apusys_aee_print("mem fail");
	return ret;
}

int ion_mem_free(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd;
	int ret = 0;

	mdw_lne_debug();
	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
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

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

	return 0;

free_import:
	mdw_drv_err("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);
	apusys_user_print_log();
	apusys_aee_print("mem fail");
	return ret;
}

int ion_mem_import(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	mdw_lne_debug();
	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* allocate buffer by fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* map kernel va*/
#if 0
	if (ion_mem_map_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}
#endif

	/* map iova*/
	if (ion_mem_map_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

	return 0;

free_import:
	mdw_drv_err("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);
	ion_free(mem_mgr->client, ion_hnd);
	apusys_user_print_log();
	apusys_aee_print("mem fail");
	return ret;
}

int ion_mem_unimport(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{

	struct ion_handle *ion_hnd;
	int ret = 0;

	mdw_lne_debug();
	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	/* unmap iova*/
	if (ion_mem_unmap_iova(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}
#if 0
	/* unmap kernel va*/
	if (ion_mem_unmap_kva(mem_mgr, mem)) {
		ret = -ENOMEM;
		goto free_import;
	}
#endif

	/* free buffer by fd */
	ion_free(mem_mgr->client, ion_hnd);

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);
	return 0;

free_import:
	mdw_drv_err("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);
	apusys_user_print_log();
	apusys_aee_print("mem fail");
	return ret;
}

int ion_mem_init(struct apusys_mem_mgr *mem_mgr)
{
	/* check init */
	if (mem_mgr->is_init) {
		mdw_drv_warn("apusys memory mgr is already inited\n");
		return -EALREADY;
	}

	/* create ion client */
	mem_mgr->client = ion_client_create(g_ion_device, "apusys midware");
	if (IS_ERR_OR_NULL(mem_mgr->client)) {
		mdw_drv_err("create ion client fail\n");
		return -ENOMEM;
	}

	/* init */
	mutex_init(&mem_mgr->list_mtx);
	INIT_LIST_HEAD(&mem_mgr->list);

	mem_mgr->is_init = 1;

	mdw_mem_debug("done\n");

	return 0;
}
int ion_mem_destroy(struct apusys_mem_mgr *mem_mgr)
{
	int ret = 0;

	if (!mem_mgr->is_init) {
		mdw_drv_warn("apusys memory mgr is not init, can't destroy\n");
		return -EALREADY;
	}

	mem_mgr->is_init = 0;
	ion_client_destroy(mem_mgr->client);
	mem_mgr->client = NULL;
	mdw_mem_debug("done\n");

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
		mdw_drv_err("invalid argument\n");
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
		mdw_drv_err("map kernel va fail(%p/%p)\n",
			mem_mgr->client, ion_hnd);
		ret = -ENOMEM;
		goto free_import;
	}

	if (mem->khandle == 0)
		mem->khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

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

	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(mem_mgr->client,
	mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* use get_iova replace config_buffer & get_phys*/
	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.kernel_handle = ion_hnd;
	mm_data.get_phys_param.module_id = APUSYS_IOMMU_PORT;
	mm_data.get_phys_param.coherent = 1;
	mm_data.get_phys_param.phy_addr =
		((unsigned long) APUSYS_IOMMU_PORT << 24);

	if (ion_kernel_ioctl(mem_mgr->client, ION_CMD_MULTIMEDIA,
			(unsigned long)&mm_data)) {
		mdw_drv_err("ion_config_buffer: ION_CMD_MULTIMEDIA failed\n");
		ret = -ENOMEM;
		goto free_import;
	}

	mem->iova = mm_data.get_phys_param.phy_addr;
	mem->iova_size = mm_data.get_phys_param.len;

	if (mem->khandle == 0)
		mem->khandle = (uint64_t)ion_hnd;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);


free_import:
	ion_free(mem_mgr->client, ion_hnd);
	return ret;
}
int ion_mem_unmap_iova(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;

	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

	return ret;
}

int ion_mem_unmap_kva(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (_ion_mem_check_arg(mem)) {
		//mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}
	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	ion_free(mem_mgr->client, ion_hnd);

	mdw_mem_debug("mem(%d/0x%llx/0x%x/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova,
			mem->iova + mem->iova_size - 1,
			mem->size, mem->iova_size,
			mem->khandle, mem->kva);

	return ret;

}


int ion_mem_flush(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	mdw_lne_debug();

	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}
	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(mem_mgr->client, ion_hnd);
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(mem_mgr->client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_FLUSH_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	return ret;
}

int ion_mem_invalidate(struct apusys_mem_mgr *mem_mgr, struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	mdw_lne_debug();

	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}
	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(mem_mgr->client, ion_hnd);

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(mem_mgr->client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_INVALID_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(mem_mgr->client, ion_hnd);

	return ret;
}

