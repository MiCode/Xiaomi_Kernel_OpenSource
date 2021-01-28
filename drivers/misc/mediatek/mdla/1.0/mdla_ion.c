// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "mdla_debug.h"

#include <linux/types.h>
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <m4u.h>

#include "mdla_ioctl.h"
#include "mdla_ion.h"

static struct ion_client *ion_client;

void mdla_ion_init(void)
{
	ion_client = ion_client_create(g_ion_device, "mdla");

	mdla_drv_debug("%s: ion_client_create(): %p\n", __func__, ion_client);
}

void mdla_ion_exit(void)
{
	if (ion_client) {
		mdla_drv_debug("%s: %p\n", __func__, ion_client);
		ion_client_destroy(ion_client);
		ion_client = NULL;
	}
}

int mdla_ion_kmap(unsigned long arg)
{
	struct ioctl_ion ion_data;
	struct ion_mm_data mm_data;
	struct ion_handle *hndl;
	ion_phys_addr_t mva;
	void *kva;
	int ret;

	if (!ion_client)
		return -ENOENT;

	if (copy_from_user(&ion_data, (void * __user) arg, sizeof(ion_data)))
		return -EFAULT;

	mdla_mem_debug("%s: share fd: %d\n", __func__, ion_data.fd);

	hndl = ion_import_dma_buf_fd(ion_client, ion_data.fd);

	if (IS_ERR_OR_NULL(hndl)) {
		mdla_mem_debug("%s: ion_import_dma_buf_fd(): failed: %ld\n",
			__func__, PTR_ERR(hndl));
		return -EINVAL;
	}

	mdla_mem_debug("%s: ion_import_dma_buf_fd(): %p\n", __func__, hndl);

	/*	set memory port */
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = hndl;
	mm_data.config_buffer_param.module_id = M4U_PORT_VPU;
	ret = ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
		(unsigned long) &mm_data);
	if (ret < 0) {
		mdla_mem_debug("%s: ion_kernel_ioctl(%p, %p): %d\n",
			__func__, ion_client, hndl, ret);
		return -EINVAL;
	}

	/*  map to to kernel virtual address */
	kva = ion_map_kernel(ion_client, hndl);
	if (IS_ERR_OR_NULL(kva)) {
		mdla_mem_debug("%s: ion_map_kernel(%p, %p): %d\n",
			__func__, ion_client, hndl, ret);
		return -EINVAL;
	}

	/*  Get the phyiscal address (mva) to the buffer */
	ret = ion_phys(ion_client, hndl, &mva, &ion_data.len);
	if (ret < 0) {
		mdla_mem_debug("%s: ion_phys(%p, %p): %d\n",
			__func__, ion_client, hndl, ret);

		return -EINVAL;
	}

	ion_data.kva = (__u64) kva;
	ion_data.mva = mva;
	ion_data.khandle = (__u64) hndl;

	if (copy_to_user((void * __user) arg, &ion_data, sizeof(ion_data)))
		return -EFAULT;

	mdla_mem_debug("%s: mva=%llxh, kva=%llxh, kernel_handle=%llxh\n",
		__func__, ion_data.mva, ion_data.kva, ion_data.khandle);


	return 0;
}

int mdla_ion_kunmap(unsigned long arg)
{
	struct ioctl_ion ion_data;
	struct ion_handle *hndl;

	if (!ion_client)
		return -ENOENT;

	if (copy_from_user(&ion_data, (void * __user) arg, sizeof(ion_data)))
		return -EFAULT;

	mdla_mem_debug("%s: mva=%llxh, kva=%llxh, kernel_handle=%llxh\n",
		__func__, ion_data.mva, ion_data.kva, ion_data.khandle);

	hndl = (struct ion_handle *)ion_data.khandle;

	if (!virt_addr_valid(hndl))
		return -EINVAL;

	ion_unmap_kernel(ion_client, hndl);
	ion_free(ion_client, hndl);

	return 0;
}

void mdla_ion_sync(u64 hndl, void *kva, u32 size)
{
	struct ion_sys_data sys_data;
	int ret;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle =
		(struct ion_handle *)hndl;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	sys_data.cache_sync_param.va = kva;
	sys_data.cache_sync_param.size = size;


	ret = ion_kernel_ioctl(ion_client, ION_CMD_SYSTEM,
		(unsigned long)&sys_data);

	mdla_mem_debug("%s: ion_kernel_ioctl kernel_handle=%llx\n",
		__func__, (unsigned long long)hndl);

	if (ret) {
		mdla_mem_debug("%s: ion_kernel_ioctl(hndl=%llx): %d failed\n",
			__func__, (unsigned long long)hndl, ret);
	}
}

