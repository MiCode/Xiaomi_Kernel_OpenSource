// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include "mdw_mem_cmn.h"

/*
 * Not used in kernel-5.4
 * Reserved for backward compatible to legacy platforms that supports ION
 */

#define APU_PAGE_SIZE PAGE_SIZE

static struct ion_client *client;

//#define APUSYS_IOMMU_PORT M4U_PORT_VPU
#define APUSYS_IOMMU_PORT M4U_PORT_L21_APU_FAKE_DATA

/* check argument */
static int check_arg(struct apusys_kmem *mem)
{
	int ret = 0;

	if (!mem)
		return -EINVAL;

	if ((mem->align != 0) &&
		((mem->align > APU_PAGE_SIZE) ||
		((APU_PAGE_SIZE % mem->align) != 0))) {
		mdw_drv_err("align argument invalid (%d)\n", mem->align);
		return -EINVAL;
	}
	if (mem->cache > 1) {
		mdw_drv_err("Cache argument invalid (%d)\n", mem->cache);
		return -EINVAL;
	}
	if ((mem->iova_size % APU_PAGE_SIZE) != 0) {
		mdw_drv_err("iova_size argument invalid 0x%x\n",
			mem->iova_size);
		return -EINVAL;
	}

	return ret;
}

static int mdw_mem_ion_map_kva(struct apusys_kmem *mem)
{
	void *buffer = NULL;
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (!mem)
		return -EINVAL;

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(client, mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -ENOMEM;

	/* map kernel va*/
	buffer = ion_map_kernel(client, ion_hnd);
	if (IS_ERR_OR_NULL(buffer)) {
		mdw_drv_err("map kernel va fail(%p/%p)\n",
			client, ion_hnd);
		ret = -ENOMEM;
		goto free_import;
	}

	if (!mem->khandle)
		mem->khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return 0;

free_import:
	ion_free(client, ion_hnd);
	return ret;
}

static int mdw_mem_ion_map_iova(struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;
	struct ion_mm_data mm_data;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (check_arg(mem))
		return -EINVAL;

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(client,
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

	if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
			(unsigned long)&mm_data)) {
		mdw_drv_err("ion_config_buffer: ION_CMD_MULTIMEDIA failed\n");
		ret = -ENOMEM;
		goto free_import;
	}

	mem->iova = mm_data.get_phys_param.phy_addr;
	mem->iova_size = mm_data.get_phys_param.len;

	if (!mem->khandle)
		mem->khandle = (uint64_t)ion_hnd;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return ret;

free_import:
	ion_free(ion_ma.client, ion_hnd);
	return ret;
}

static int mdw_mem_ion_unmap_iova(struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (check_arg(mem))
		return -EINVAL;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	if (!mem->khandle)
		return -EINVAL;

	ion_hnd = (struct ion_handle *) mem->khandle;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	ion_free(client, ion_hnd);

	return ret;
}

static int mdw_mem_ion_unmap_kva(struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (check_arg(mem))
		return -EINVAL;

	if (!mem->khandle)
		return -EINVAL;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	ion_hnd = (struct ion_handle *) mem->khandle;
	ion_unmap_kernel(client, ion_hnd);
	ion_free(client, ion_hnd);
	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return ret;
}

static int mdw_mem_ion_alloc(struct apusys_kmem *mem)
{
	struct ion_handle *ion_hnd = NULL;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	ion_hnd = ion_alloc(client, mem->size, mem->align,
		ION_HEAP_MULTIMEDIA_MASK, 0);
	if (!ion_hnd)
		return -ENOMEM;

	mem->khandle = (unsigned long long)ion_hnd;

	if (mdw_mem_ion_map_iova(mem))
		goto map_iova_fail;
	if (mdw_mem_ion_map_kva(mem))
		goto map_kva_fail;

	return 0;

map_kva_fail:
	mdw_mem_ion_unmap_iova(mem);
map_iova_fail:
	ion_free(client, ion_hnd);
	return -ENOMEM;
}

static int mdw_mem_ion_free(struct apusys_kmem *mem)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (mdw_mem_ion_unmap_kva(mem)) {
		mdw_drv_err("unmap kva fail\n");
		ret = -ENOMEM;
	}

	if (mdw_mem_ion_unmap_iova(mem)) {
		mdw_drv_err("unmap iova fail\n");
		ret = -ENOMEM;
	}

	ion_free(client, (struct ion_handle *)mem->khandle);

	return ret;
}

static int mdw_mem_ion_flush(struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	mdw_mem_debug("\n");

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (!mem->khandle)
		return -EINVAL;

	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(client, ion_hnd);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_FLUSH_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(client, ion_hnd);

	return ret;
}

static int mdw_mem_ion_invalidate(struct apusys_kmem *mem)
{
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	if (!mem->khandle)
		return -EINVAL;

	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(client, ion_hnd);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_INVALID_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(client, ion_hnd);

	return ret;
}

static void mdw_mem_ion_exit(void)
{
	if (IS_ERR_OR_NULL(client))
		return;

	ion_client_destroy(client);
	memset(&ion_ma, 0, sizeof(ion_ma));
}

static int mdw_mem_ion_init(void)
{
	/* create ion client */
	client = ion_client_create(g_ion_device, "apusys midware");
	if (IS_ERR_OR_NULL(client))
		return -ENODEV;

	return 0;
}

static struct mdw_mem_ops mem_ion_ops = {
	.init = mdw_mem_ion_init,
	.exit = mdw_mem_ion_exit,
	.alloc = mdw_mem_ion_alloc,
	.free = mdw_mem_ion_free,
	.flush = mdw_mem_ion_flush,
	.invalidate = mdw_mem_ion_invalidate,
	.map_kva = mdw_mem_ion_map_kva,
	.unmap_kva = mdw_mem_ion_unmap_kva,
	.map_iova = mdw_mem_ion_map_iova,
	.unmap_iova = mdw_mem_ion_unmap_iova
};

struct mdw_mem_ops *mdw_mops_ion(void)
{
	return &mem_ion_ops;
}

