// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "ion_drv.h"
#ifdef CONFIG_MTK_IOMMU_V2
#include "mt_iommu.h"
#include "pseudo_m4u.h"
#endif

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include "mdw_cmn.h"
#include "mdw_mem_cmn.h"

#define APUSYS_ION_PAGE_SIZE PAGE_SIZE

/* ion mem allocator */
struct mdw_mem_ion_ma {
	struct ion_client *client;
	struct mdw_mem_ops ops;
};

static struct mdw_mem_ion_ma ion_ma;

#ifdef CONFIG_MTK_IOMMU_V2
#define APUSYS_IOMMU_PORT M4U_PORT_L21_APU_FAKE_DATA

/* check argument */
static int mdw_mem_ion_check(struct apusys_kmem *mem)
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
#endif

static uint32_t mdw_mem_ion_get_size(struct apusys_kmem *mem)
{
	uint32_t size = 0;
	struct dma_buf *db = NULL;

	db = dma_buf_get(mem->fd);
	if (IS_ERR(db)) {
		mdw_drv_err("get dmabuf from ion handle fail\n");
		return 0;
	}

	size = db->size;
	dma_buf_put(db);

	return size;
}

static int mdw_mem_ion_map_kva(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
	int ret = -ENODEV;
#else
	void *buffer = NULL;
	struct ion_handle *ion_hnd = NULL;
	uint32_t size = 0;
	int ret = 0;

	/* check argument */
	if (mem == NULL) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(ion_ma.client, mem->fd);
	if (IS_ERR_OR_NULL(ion_hnd))
		return -EINVAL;

	/* check size */
	size = mdw_mem_ion_get_size(mem);
	if (size < mem->size || !size) {
		mdw_drv_err("buffer size invalid(%u/%u)\n", size, mem->size);
		ret = -ENOMEM;
		goto fail_map_kernel;
	}
	mdw_mem_debug("mem check size(%u/%u)\n", size, mem->size);

	/* map kernel va*/
	buffer = ion_map_kernel(ion_ma.client, ion_hnd);
	if (IS_ERR_OR_NULL(buffer)) {
		mdw_drv_err("map kernel va fail(%p/%p)\n",
			ion_ma.client, ion_hnd);
		ret = -ENOMEM;
		goto fail_map_kernel;
	}

	if (!mem->khandle)
		mem->khandle = (uint64_t)ion_hnd;
	mem->kva = (uint64_t)buffer;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	return 0;

fail_map_kernel:
	mdw_drv_err("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);
	ion_free(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static int mdw_mem_ion_map_iova(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
	int ret = -ENODEV;
#else
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;
	struct ion_mm_data mm_data;

	/* check argument */
	if (mdw_mem_ion_check(mem))
		return -EINVAL;

	/* import fd */
	ion_hnd = ion_import_dma_buf_fd(ion_ma.client, mem->fd);

	if (IS_ERR_OR_NULL(ion_hnd))
		return -EINVAL;

	/* use get_iova replace config_buffer & get_phys*/
	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.kernel_handle = ion_hnd;
	mm_data.get_phys_param.module_id = APUSYS_IOMMU_PORT;
	mm_data.get_phys_param.coherent = 1;
	mm_data.get_phys_param.phy_addr =
		((unsigned long) APUSYS_IOMMU_PORT << 24);

	if (ion_kernel_ioctl(ion_ma.client, ION_CMD_MULTIMEDIA,
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
	mdw_drv_err("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);
	ion_free(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static int mdw_mem_ion_unmap_iova(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
		int ret = -ENODEV;
#else
	int ret = 0;
	struct ion_handle *ion_hnd = NULL;

	/* check argument */
	if (mdw_mem_ion_check(mem))
		return -EINVAL;

	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	ion_hnd = (struct ion_handle *) mem->khandle;

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	ion_free(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static int mdw_mem_ion_unmap_kva(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
	int ret = -ENODEV;
#else
	struct ion_handle *ion_hnd = NULL;
	int ret = 0;

	/* check argument */
	if (mdw_mem_ion_check(mem))
		return -EINVAL;

	/* check argument */
	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	mdw_mem_debug("mem(%d/0x%llx/0x%x/%d/0x%x/0x%llx/0x%llx)\n",
			mem->fd, mem->uva, mem->iova, mem->size,
			mem->iova_size, mem->khandle, mem->kva);

	ion_hnd = (struct ion_handle *) mem->khandle;

	ion_unmap_kernel(ion_ma.client, ion_hnd);

	ion_free(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static int mdw_mem_ion_alloc(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_ion_free(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_ion_flush(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
	int ret = -ENODEV;
#else
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	mdw_mem_debug("\n");

	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}
	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(ion_ma.client, ion_hnd);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(ion_ma.client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_FLUSH_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static int mdw_mem_ion_invalidate(struct apusys_kmem *mem)
{
#if !defined(CONFIG_MTK_IOMMU_V2)
	int ret = -ENODEV;
#else
	int ret = 0;
	struct ion_sys_data sys_data;
	void *va = NULL;
	struct ion_handle *ion_hnd = NULL;

	if (mem->khandle == 0) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}
	ion_hnd = (struct ion_handle *)mem->khandle;

	va = ion_map_kernel(ion_ma.client, ion_hnd);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = ion_hnd;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = mem->size;
	if (ion_kernel_ioctl(ion_ma.client,
			ION_CMD_SYSTEM, (unsigned long)&sys_data)) {
		mdw_drv_err("ION_CACHE_INVALID_BY_RANGE FAIL\n");
		ret = -EINVAL;
	}
	ion_unmap_kernel(ion_ma.client, ion_hnd);
#endif
	return ret;
}

static void mdw_mem_ion_destroy(void)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	ion_client_destroy(ion_ma.client);
	memset(&ion_ma, 0, sizeof(ion_ma));
#endif
}

struct mdw_mem_ops *mdw_mem_ion_init(void)
{
	memset(&ion_ma, 0, sizeof(ion_ma));

#if defined(CONFIG_MTK_IOMMU_V2)
	/* create ion client */
	ion_ma.client = ion_client_create(g_ion_device, "apusys midware");
#endif
	if (IS_ERR_OR_NULL(ion_ma.client))
		return NULL;

	ion_ma.ops.alloc = mdw_mem_ion_alloc;
	ion_ma.ops.free = mdw_mem_ion_free;
	ion_ma.ops.flush = mdw_mem_ion_flush;
	ion_ma.ops.invalidate = mdw_mem_ion_invalidate;
	ion_ma.ops.map_kva = mdw_mem_ion_map_kva;
	ion_ma.ops.unmap_kva = mdw_mem_ion_unmap_kva;
	ion_ma.ops.map_iova = mdw_mem_ion_map_iova;
	ion_ma.ops.unmap_iova = mdw_mem_ion_unmap_iova;
	ion_ma.ops.destroy = mdw_mem_ion_destroy;

	return &ion_ma.ops;
}
