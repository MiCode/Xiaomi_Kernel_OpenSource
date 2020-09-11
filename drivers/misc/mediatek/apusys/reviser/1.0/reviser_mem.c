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

#include <linux/errno.h>
#include <linux/slab.h>
#include "reviser_cmn.h"
#include "reviser_mem.h"
#include "reviser_ioctl.h"

#include <linux/dma-mapping.h>
#include <asm/mman.h>


#define APUSYS_ION 1
#if APUSYS_ION
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>

#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#else
#include <mt_iommu.h>
#endif

#endif

static struct reviser_mem_mgr g_rmem;

int reviser_mem_free(struct reviser_mem *mem)
{
#if APUSYS_ION
	struct ion_handle *handle;
	struct ion_client *ion_client;

	ion_client = g_rmem.client;
	if (ion_client == NULL) {
		LOG_ERR("ion_client invalid\n");
		return -ENOMEM;
	}
	handle = (struct ion_handle *) mem->handle;
	if (handle == NULL) {
		LOG_DEBUG("Invalid handle\n");
		return -ENOMEM;
	}

	ion_unmap_kernel(ion_client, handle);
	ion_free(ion_client, handle);

#else
	dma_free_attrs(mem_mgr->dev, mem->size,
			(void *) mem->kva, mem->iova, 0);
#endif

	LOG_DEBUG("Done\n");
	return 0;
}

int reviser_mem_alloc(struct reviser_mem *mem)
{
#if APUSYS_ION
	struct ion_mm_data mm_data;
	struct ion_handle *handle;
	struct ion_client *ion_client;
	void *buffer = NULL;


	//ion_client = ion_client_create(g_ion_device, "vpu");
	//reviser_mem_init();
	ion_client = g_rmem.client;
	if (ion_client == NULL) {
		LOG_ERR("ion_client invalid\n");
		goto out;
	}


	handle = ion_alloc(ion_client, mem->size, 0,
			ION_HEAP_MULTIMEDIA_MASK, 0);
	if (handle == NULL) {
		LOG_ERR("alloc ion buffer fail\n");
		goto out;
	}

	buffer = ion_map_kernel(ion_client, handle);
	if (buffer == NULL) {
		LOG_ERR("map kernel va fail(%p/%p)\n", ion_client, handle);
		goto free_alloc;
	}

	/* use get_iova replace config_buffer & get_phys*/
	memset((void *)&mm_data, 0, sizeof(mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.kernel_handle = handle;
	mm_data.get_phys_param.module_id = M4U_PORT_L21_APU_FAKE_VLM;
	mm_data.get_phys_param.coherent = 1;
	//mm_data.get_phys_param.reserve_iova_start = 0x60000000;
	//mm_data.get_phys_param.reserve_iova_end = 0x82600000;

	//mm_data.get_phys_param.phy_addr =
	//	((unsigned long)M4U_PORT_VPU<<24) | ION_FLAG_GET_FIXED_PHYS;
	mm_data.get_phys_param.phy_addr =
		((unsigned long) M4U_PORT_L21_APU_FAKE_VLM<<24);
	//mm_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
			(unsigned long)&mm_data)) {
		LOG_ERR("Get MVA failed: ION_CMD_MULTIMEDIA failed\n");
		goto free_map;
	}

	mem->kva = (uint64_t)buffer;
	mem->handle = (uint64_t) handle;
	mem->iova = mm_data.get_phys_param.phy_addr;

	LOG_INFO("mem(%p/0x%x/%d/0x%lx/0x%llx/0x%llx)\n",
			ion_client, mem->iova, mem->size,
			mm_data.get_phys_param.len, mem->handle, mem->kva);

	return 0;
free_map:
	ion_unmap_kernel(ion_client, handle);
free_alloc:
	ion_free(ion_client, handle);
out:
	LOG_ERR("Fail\n");

	return -ENOMEM;

#else
	dma_addr_t dma_addr = 0;

	mem->kva = (uint64_t) dma_alloc_coherent(mem_mgr->dev,
			mem->size, &dma_addr, GFP_KERNEL);

	mem->iova = dma_to_phys(mem_mgr->dev, dma_addr);

	LOG_DEBUG("iova: %08x kva: %08x\n", mem->iova, mem->kva);
#endif

}
int reviser_mem_init(void)
{

	/* check init */
	if (g_rmem.is_init) {
		LOG_INFO("apusys memory mgr is already inited\n");
		return -EALREADY;
	}

	g_rmem.client = ion_client_create(g_ion_device, "reviser");
	g_rmem.is_init = 1;

	return 0;
}

int reviser_mem_destroy(void)
{

	int ret = 0;

	if (!g_rmem.is_init) {
		LOG_INFO("apusys memory mgr is not init, can't destroy\n");
		return -EALREADY;
	}

	ion_client_destroy(g_rmem.client);
	g_rmem.is_init = 0;

	LOG_DEBUG("done\n");

	return ret;

}





