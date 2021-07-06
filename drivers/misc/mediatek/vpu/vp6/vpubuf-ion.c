/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * @file vpubuf-ion.c
 * Handle about VPU memory management.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/iommu.h>

#include "vpubuf-core.h"
#include "vpubuf-dma-contig.h"
#include "vpu_cmn.h"

#include <m4u.h>
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>

#define IOMMU_VA_START      (0x7DA00000)
#define IOMMU_VA_END        (0x82600000)


static struct vpu_kernel_buf *vbuf_ion_kmap(struct vpu_device *vpu_device,
					uint32_t usage, uint64_t phy_addr,
					uint64_t kva_addr,
					uint32_t iova_addr,
					uint32_t size)
{
	struct vpu_kernel_buf *vkbuf;
	struct sg_table *sg;
	int ret;

	vkbuf = kzalloc(sizeof(*vkbuf), GFP_KERNEL);
	if (!vkbuf)
		return NULL;

	switch (usage) {
	case VKBUF_MAP_FPHY_FIOVA:
	{
		LOG_ERR("VKBUF_MAP_FPHY_FIOVA +\n");
		sg = &(vkbuf->sg);
		ret = sg_alloc_table(sg, 1, GFP_KERNEL);
		if (ret) {
			LOG_ERR("fail to sg_alloc_table\n");
			kfree(vkbuf);
			return NULL;
		}

		sg_dma_address(sg->sgl) = phy_addr;
		sg_dma_len(sg->sgl) = size;
		ret = m4u_alloc_mva(vpu_device->m4u_client, VPU_PORT_OF_IOMMU,
				0, sg,
				size,
				M4U_PROT_READ | M4U_PROT_WRITE,
				M4U_FLAGS_START_FROM, &iova_addr);
		if (ret) {
			LOG_ERR("fail to m4u_alloc_mva\n");
			sg_free_table(sg);
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = phy_addr;
		vkbuf->iova_addr = iova_addr;
		vkbuf->size = size;
		vkbuf->kva = NULL;
		LOG_ERR("VKBUF_MAP_FPHY_FIOVA -\n");
		break;
	}
	case VKBUF_MAP_FPHY_DIOVA:
	{
		LOG_ERR("VKBUF_MAP_FPHY_DIOVA +\n");
		sg = &(vkbuf->sg);
		ret = sg_alloc_table(sg, 1, GFP_KERNEL);
		if (ret) {
			LOG_ERR("fail to sg_alloc_table\n");
			kfree(vkbuf);
			return NULL;
		}

		sg_dma_address(sg->sgl) = phy_addr;
		sg_dma_len(sg->sgl) = size;
		ret = m4u_alloc_mva(vpu_device->m4u_client, VPU_PORT_OF_IOMMU,
				0, sg,
				size,
				M4U_PROT_READ | M4U_PROT_WRITE,
				M4U_FLAGS_SG_READY, &iova_addr);
		if (ret) {
			LOG_ERR("fail to m4u_alloc_mva\n");
			sg_free_table(sg);
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = phy_addr;
		vkbuf->iova_addr = iova_addr;	/* output */
		vkbuf->size = size;
		vkbuf->kva = NULL;

		LOG_ERR("VKBUF_MAP_FPHY_DIOVA -\n");
		break;
	}
	case VKBUF_MAP_DPHY_FIOVA:
	{
		struct ion_mm_data mm_data;
		struct ion_sys_data sys_data;
		struct ion_handle *handle = NULL;

		LOG_ERR("VKBUF_MAP_DPHY_FIOVA +\n");

		handle = ion_alloc(vpu_device->ion_client, size, 0,
				   ION_HEAP_MULTIMEDIA_MASK, 0);
		if (handle == NULL) {
			LOG_ERR("fail to alloc ion buffer\n");
			kfree(vkbuf);
			return NULL;
		}

		mm_data.mm_cmd = ION_MM_CONFIG_BUFFER_EXT;
		mm_data.config_buffer_param.kernel_handle = handle;
		mm_data.config_buffer_param.module_id = VPU_PORT_OF_IOMMU;
		mm_data.config_buffer_param.security = 0;
		mm_data.config_buffer_param.coherent = 1;
		/* fixed_addr */
		mm_data.config_buffer_param.reserve_iova_start = iova_addr;
		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;

		ret = ion_kernel_ioctl(vpu_device->ion_client,
					ION_CMD_MULTIMEDIA,
					(unsigned long)&mm_data);
		if (ret) {
			LOG_ERR("fail to config ion buffer\n");
			ion_free(vpu_device->ion_client, handle);
			kfree(vkbuf);
			return NULL;
		}

		/* map pa */
		sys_data.sys_cmd = ION_SYS_GET_PHYS;
		sys_data.get_phys_param.kernel_handle = handle;
		sys_data.get_phys_param.phy_addr = VPU_PORT_OF_IOMMU << 24 |
						   ION_FLAG_GET_FIXED_PHYS;
		sys_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
		ret = ion_kernel_ioctl(vpu_device->ion_client,
					ION_CMD_SYSTEM,
					(unsigned long)&sys_data);
		if (ret) {
			LOG_ERR("fail to get ion phys\n");
			ion_free(vpu_device->ion_client, handle);
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = (uint64_t)handle;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->iova_addr = sys_data.get_phys_param.phy_addr;
		vkbuf->size = sys_data.get_phys_param.len;
		/* map va */
		vkbuf->kva = (void *)ion_map_kernel(vpu_device->ion_client,
							handle);

		LOG_ERR("VKBUF_MAP_DPHY_FIOVA -\n");
		break;
	}
	case VKBUF_MAP_DPHY_DIOVA:
	{
		struct ion_mm_data mm_data;
		struct ion_sys_data sys_data;
		struct ion_handle *handle = NULL;

		LOG_ERR("VKBUF_MAP_DPHY_DIOVA +\n");

		handle = ion_alloc(vpu_device->ion_client, size, 0,
				   ION_HEAP_MULTIMEDIA_MASK, 0);
		if (handle == NULL) {
			LOG_ERR("fail to alloc ion buffer\n");
			kfree(vkbuf);
			return NULL;
		}

		mm_data.mm_cmd = ION_MM_CONFIG_BUFFER_EXT;
		mm_data.config_buffer_param.kernel_handle = handle;
		mm_data.config_buffer_param.module_id = VPU_PORT_OF_IOMMU;
		mm_data.config_buffer_param.security = 0;
		mm_data.config_buffer_param.coherent = 1;

		/* CHRISTODO, need revise starting address for working buffer*/
		mm_data.config_buffer_param.reserve_iova_start = 0x60000000;
		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;

		ret = ion_kernel_ioctl(vpu_device->ion_client,
					ION_CMD_MULTIMEDIA,
					(unsigned long)&mm_data);
		if (ret) {
			LOG_ERR("fail to config ion buffer\n");
			ion_free(vpu_device->ion_client, handle);
			kfree(vkbuf);
			return NULL;
		}

		/* map pa */
		sys_data.sys_cmd = ION_SYS_GET_PHYS;
		sys_data.get_phys_param.kernel_handle = handle;
		sys_data.get_phys_param.phy_addr = VPU_PORT_OF_IOMMU << 24 |
						   ION_FLAG_GET_FIXED_PHYS;
		sys_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
		ret = ion_kernel_ioctl(vpu_device->ion_client,
					ION_CMD_SYSTEM,
					(unsigned long)&sys_data);
		if (ret) {
			LOG_ERR("fail to get ion phys\n");
			ion_free(vpu_device->ion_client, handle);
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = (uint64_t)handle;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->iova_addr = sys_data.get_phys_param.phy_addr;
		vkbuf->size = sys_data.get_phys_param.len;
		/* map va */
		vkbuf->kva = (void *)ion_map_kernel(vpu_device->ion_client,
							handle);

		LOG_ERR("VKBUF_MAP_DPHY_DIOVA -\n");
		break;
	}
	default:
	{
		break;
	}
	}

	return vkbuf;
}

static void vbuf_ion_kunmap(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf)
{
	switch (vkbuf->usage) {
	case VKBUF_MAP_FPHY_FIOVA:
	{
		m4u_dealloc_mva(vpu_device->m4u_client, VPU_PORT_OF_IOMMU,
				vkbuf->iova_addr);
		sg_free_table(&(vkbuf->sg));

		break;
	}
	case VKBUF_MAP_FPHY_DIOVA:
	{
		m4u_dealloc_mva(vpu_device->m4u_client, VPU_PORT_OF_IOMMU,
				vkbuf->iova_addr);
		sg_free_table(&(vkbuf->sg));
		break;
	}
	case VKBUF_MAP_DPHY_FIOVA:
	{
		struct ion_handle *handle;

		handle = (struct ion_handle *) vkbuf->handle;
		if (handle) {
			ion_unmap_kernel(vpu_device->ion_client, handle);
			ion_free(vpu_device->ion_client, handle);
		}

		break;
	}
	case VKBUF_MAP_DPHY_DIOVA:
	{
		struct ion_handle *handle;

		handle = (struct ion_handle *) vkbuf->handle;
		if (handle) {
			ion_unmap_kernel(vpu_device->ion_client, handle);
			ion_free(vpu_device->ion_client, handle);
		}

		break;
	}
	default:
	{
		break;
	}
	}

	kfree(vkbuf);
}

static void vbuf_ion_init(struct vpu_device *vpu_device)
{
	if (!vpu_device->m4u_client)
		vpu_device->m4u_client = m4u_create_client();
	if (!vpu_device->ion_client)
		vpu_device->ion_client = ion_client_create(g_ion_device,
					       "vpu");
	if (!vpu_device->ion_drv_client)
		vpu_device->ion_drv_client = ion_client_create(g_ion_device,
						   "vpu_drv");
}

static void vbuf_ion_deinit(struct vpu_device *vpu_device)
{
	if (vpu_device->m4u_client) {
		m4u_destroy_client(vpu_device->m4u_client);
		vpu_device->m4u_client = NULL;
	}

	if (vpu_device->ion_client) {
		ion_client_destroy(vpu_device->ion_client);
		vpu_device->ion_client = NULL;
	}

	if (vpu_device->ion_drv_client) {
		ion_client_destroy(vpu_device->ion_drv_client);
		vpu_device->ion_drv_client = NULL;
	}
}

static uint64_t vbuf_ion_import_handle(struct vpu_device *vpu_device, int fd)
{
	uint64_t ret;
	struct ion_handle *handle = NULL;

	if (!vpu_device->ion_drv_client) {
		LOG_ERR("[vpu] invalid ion client!\n");
		return handle;
	}
	if (fd == -1) {
		LOG_ERR("[vpu] invalid ion fd!\n");
		return handle;
	}

	handle = ion_import_dma_buf(vpu_device->ion_drv_client, fd);

	if (IS_ERR(handle)) {
		LOG_ERR("[vpu] import ion handle failed!\n");
		return NULL;
	}

	ret = (uint64_t)(uintptr_t)handle;
	return ret;
}

static void vbuf_ion_free_handle(struct vpu_device *vpu_device, uint64_t id)
{
	struct ion_handle *handle;

	handle = (struct ion_handle *)(uintptr_t)id;

	if (!vpu_device->ion_drv_client) {
		LOG_ERR("[vpu] invalid ion client!\n");
		return;
	}
	if (!handle) {
		LOG_ERR("[vpu] invalid ion handle(0x%p)!\n", handle);
		return;
	}

	ion_free(vpu_device->ion_drv_client, handle);
}

struct vpu_map_ops vpu_ion_mapops = {
	.init_phy_iova = vbuf_ion_init,
	.deinit_phy_iova = vbuf_ion_deinit,
	.kmap_phy_iova = vbuf_ion_kmap,
	.kunmap_phy_iova = vbuf_ion_kunmap,
	.import_handle = vbuf_ion_import_handle,
	.free_handle = vbuf_ion_free_handle,
};

