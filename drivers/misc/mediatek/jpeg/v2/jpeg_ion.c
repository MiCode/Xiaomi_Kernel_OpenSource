/*
 * Copyright (C) 2021 MediaTek Inc.
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

#include "jpeg_ion.h"
#include "jpeg_drv_common.h"
#include <ion_drv.h>
#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include "mach/pseudo_m4u.h"
#include <soc/mediatek/smi.h>
#include "mtk_iommu_ext.h"
#include "mtk_ion.h"
#endif

#if defined(CONFIG_MTK_IOMMU_V2)
static struct ion_client *g_jpg_ion_client;
#endif

extern int jpg_dbg_level;

int jpg_ion_get_iova(struct ion_handle *handle,
	u64 *iova, int port)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_mm_data mm_data;
	size_t iova_size;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.module_id = port;
	mm_data.get_phys_param.kernel_handle = handle;

	if (ion_kernel_ioctl(g_jpg_ion_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		JPEG_LOG(0, "get iova failed.%p -%p\n",
			g_jpg_ion_client, handle);
		ion_free(g_jpg_ion_client, handle);
		return -1;
	}

	*iova = mm_data.get_phys_param.phy_addr;
	iova_size = mm_data.get_phys_param.len;
	if (*iova == 0)
		JPEG_LOG(0, "alloc mmu addr hnd=0x%p,iova=0x%08lx\n",
			handle, *iova);

	JPEG_LOG(1, "iova: %llu", *iova);
#endif
	return 0;
}

struct ion_handle *jpg_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;

#if defined(CONFIG_MTK_IOMMU_V2)
	/* If no need Ion support, do nothing! */
	if (fd <= 0) {
		JPEG_LOG(0, "NO NEED ion support, fd %d\n", fd);
		return handle;
	}

	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return handle;
	}

	handle = ion_import_dma_buf_fd(g_jpg_ion_client, fd);
	if (IS_ERR(handle)) {
		JPEG_LOG(0, "import ion handle failed!");
		return NULL;
	}

	JPEG_LOG(1, "import ion handle fd=%d, hnd=0x%p", fd, handle);
#endif
	return handle;
}

struct ion_handle *jpg_ion_alloc_handle(size_t size, size_t align, unsigned int flags)
{
	struct ion_handle *handle = NULL;

#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return handle;
	}

	handle = ion_alloc(g_jpg_ion_client, size, align, ION_HEAP_MULTIMEDIA_MASK, flags);
	if (IS_ERR(handle)) {
		JPEG_LOG(0, "alloc ion handle failed!");
		return NULL;
	}

	JPEG_LOG(1, "alloc ion handle hnd=0x%p", handle);
#endif
	return handle;
}

int jpg_ion_share_handle(struct ion_handle *handle)
{
	int fd = -1;

#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return -1;
	}

	fd = ion_share_dma_buf_fd(g_jpg_ion_client, handle);
	if (fd < 0) {
		JPEG_LOG(0, "share ion handle failed!");
		return -1;
	}

	JPEG_LOG(1, "share ion handle hnd=0x%p fd:%d", handle, fd);
#endif
	return fd;
}

void *jpg_ion_map_handle(struct ion_handle *handle)
{
	void *ptr = NULL;

#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return NULL;
	}

	ptr = ion_map_kernel(g_jpg_ion_client, handle);
	if (ptr == NULL) {
		JPEG_LOG(0, "map ion handle failed!");
		return NULL;
	}

	JPEG_LOG(1, "map ion handle hnd=0x%p ptr:%p", handle, ptr);
#endif
	return ptr;
}

void jpg_ion_unmap_handle(struct ion_handle *handle)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return;
	}

	ion_unmap_kernel(g_jpg_ion_client, handle);

	JPEG_LOG(1, "unmap ion handle hnd=0x%p fd:%d", handle);
#endif
}


void jpg_ion_free_handle(struct ion_handle *handle)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_jpg_ion_client) {
		JPEG_LOG(0, "invalid ion client!");
		return;
	}
	if (!handle)
		return;

	ion_free(g_jpg_ion_client, handle);

	JPEG_LOG(1, "handle 0x%p", handle);
#endif
}

void jpg_ion_cache_flush(struct ion_handle *handle)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_sys_data sys_data;
	void *buffer_va;

	if (!g_jpg_ion_client || !handle)
		return;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;

	buffer_va = ion_map_kernel(g_jpg_ion_client, handle);
	sys_data.cache_sync_param.va = buffer_va;
	sys_data.cache_sync_param.size = handle->buffer->size;

	if (ion_kernel_ioctl(g_jpg_ion_client, ION_CMD_SYSTEM,
		(unsigned long)&sys_data))
		JPEG_LOG(0, "ion cache flush failed!");
	ion_unmap_kernel(g_jpg_ion_client, handle);
#endif
}

void jpg_ion_create(const char *name)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (g_ion_device)
		g_jpg_ion_client = ion_client_create(g_ion_device, name);
	else
		JPEG_LOG(0, "invalid g_ion_device");

	if (!g_jpg_ion_client)
		JPEG_LOG(0, "create ion client failed!");

#endif
}

void jpg_ion_destroy(void)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (g_jpg_ion_client && g_ion_device)
		ion_client_destroy(g_jpg_ion_client);
#endif
}

u64 jpg_translate_fd(u64 fd, u32 offset, u32 port)
{
	struct ion_handle *ion_h;
	u64 iova = 0;

	/* need to map ion handle and iova */
	ion_h = jpg_ion_import_handle(fd);
	if (!ion_h)
		return 0;

	jpg_ion_get_iova(ion_h, &iova, port);

	iova += offset;

	jpg_ion_free_handle(ion_h);
	JPEG_LOG(1, "iova 0x%x", iova);

	return iova;
}

