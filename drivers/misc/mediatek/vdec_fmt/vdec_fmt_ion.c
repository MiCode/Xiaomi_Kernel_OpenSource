/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "vdec_fmt_ion.h"
#include "vdec_fmt_utils.h"
#include <ion_drv.h>
#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include "mach/pseudo_m4u.h"
#include <soc/mediatek/smi.h>
#include "mtk_iommu_ext.h"
#endif

#if defined(CONFIG_MTK_IOMMU_V2)
static struct ion_client *g_fmt_ion_client;
#endif

int fmt_ion_get_iova(struct ion_handle *handle,
	u64 *iova, int port)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_mm_data mm_data;
	size_t iova_size;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.module_id = port;
	mm_data.get_phys_param.kernel_handle = handle;

	if (ion_kernel_ioctl(g_fmt_ion_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		fmt_err("get iova failed.%p -%p\n",
			g_fmt_ion_client, handle);
		ion_free(g_fmt_ion_client, handle);
		return -1;
	}

	*iova = mm_data.get_phys_param.phy_addr;
	iova_size = mm_data.get_phys_param.len;
	if (*iova == 0)
		fmt_err("alloc mmu addr hnd=0x%p,iova=0x%08lx\n",
			handle, *iova);

	fmt_debug(1, "iova: %llu", *iova);
#endif
	return 0;
}

struct ion_handle *fmt_ion_import_handle(int fd)
{
	struct ion_handle *handle = NULL;

#if defined(CONFIG_MTK_IOMMU_V2)
	/* If no need Ion support, do nothing! */
	if (fd <= 0) {
		fmt_err("NO NEED ion support, fd %d\n", fd);
		return handle;
	}

	if (!g_fmt_ion_client) {
		fmt_err("invalid ion client!");
		return handle;
	}

	handle = ion_import_dma_buf_fd(g_fmt_ion_client, fd);
	if (IS_ERR(handle)) {
		fmt_err("import ion handle failed!");
		return NULL;
	}

	fmt_debug(1, "import ion handle fd=%d, hnd=0x%p", fd, handle);
#endif
	return handle;
}

void fmt_ion_free_handle(struct ion_handle *handle)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (!g_fmt_ion_client) {
		fmt_err("invalid ion client!");
		return;
	}
	if (!handle)
		return;

	ion_free(g_fmt_ion_client, handle);

	fmt_debug(1, "handle 0x%p", handle);
#endif
}

void fmt_ion_cache_flush(struct ion_handle *handle)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_sys_data sys_data;
	void *buffer_va;

	if (!g_fmt_ion_client || !handle)
		return;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_INVALID_BY_RANGE;

	buffer_va = ion_map_kernel(g_fmt_ion_client, handle);
	sys_data.cache_sync_param.va = buffer_va;
	sys_data.cache_sync_param.size = handle->buffer->size;

	if (ion_kernel_ioctl(g_fmt_ion_client, ION_CMD_SYSTEM,
		(unsigned long)&sys_data))
		fmt_err("ion cache flush failed!");
	ion_unmap_kernel(g_fmt_ion_client, handle);
#endif
}

void fmt_ion_create(const char *name)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (g_ion_device)
		g_fmt_ion_client = ion_client_create(g_ion_device, name);
	else
		fmt_err("invalid g_ion_device");

	if (!g_fmt_ion_client)
		fmt_err("create ion client failed!");

#endif
}

void fmt_ion_destroy(void)
{
#if defined(CONFIG_MTK_IOMMU_V2)
	if (g_fmt_ion_client && g_ion_device)
		ion_client_destroy(g_fmt_ion_client);
#endif
}

u64 fmt_translate_fd(u64 fd, u32 offset, struct ionmap map[])
{
	int i;
	struct ion_handle *ion_h;
	u64 iova = 0;

	for (i = 0; i < FMT_FD_RESERVE; i++) {
		if (fd == map[i].fd) {
			fmt_debug(1, "quick search iova 0x%x",
				map[i].iova + offset);
			return map[i].iova + offset;
		}
	}

	/* need to map ion handle and iova */
	ion_h = fmt_ion_import_handle(fd);
	if (!ion_h)
		return 0;

	fmt_ion_get_iova(ion_h, &iova, M4U_PORT_L4_MINI_MDP_R0_EXT);
	for (i = 0; i < FMT_FD_RESERVE; i++) {
		if (map[i].fd == -1) {
			map[i].fd = fd;
			map[i].iova = iova;
			break;
		}
	}

	iova += offset;

	fmt_debug(1, "iova 0x%x", iova);

	return iova;
}

