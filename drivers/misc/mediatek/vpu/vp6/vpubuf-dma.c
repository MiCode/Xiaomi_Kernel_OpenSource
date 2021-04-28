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
 * @file vpubuf-dma.c
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

#if (defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U))
#include "pseudo_m4u.h"
#endif

static struct vpu_kernel_buf *vbuf_dma_kmap(struct vpu_device *vpu_device,
					uint32_t usage, uint64_t phy_addr,
					uint64_t kva_addr,
					uint32_t iova_addr,
					uint32_t size)
{
	struct vpu_kernel_buf *vkbuf;
	struct sg_table *sg;
	int ret;
	dma_addr_t dma_addr;
	int pg_size;

	vkbuf = kzalloc(sizeof(*vkbuf), GFP_KERNEL);
	if (!vkbuf)
		return NULL;

	switch (usage) {
	case VKBUF_MAP_FPHY_FIOVA:
	{
		sg = &(vkbuf->sg);
		ret = sg_alloc_table(sg, 1, GFP_KERNEL);
		if (ret) {
			LOG_ERR("fail to sg_alloc_table\n");
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->kva = (void *)(uintptr_t)kva_addr;
		sg_init_table(sg->sgl, 1);
		sg_set_page(sg->sgl, phys_to_page(phy_addr), size,
			    offset_in_page(phy_addr));

		pg_size = dma_map_sg_within_reserved_iova(vpu_device->dev,
							sg->sgl,
							sg->orig_nents,
							IOMMU_READ |
							IOMMU_WRITE,
							iova_addr);
		if (!pg_size)
			LOG_ERR("fail to dma_map_sg_within_reserved_iova\n");

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		/* vkbuf->sg = sg; */
		vkbuf->phy_addr = phy_addr;
		vkbuf->iova_addr = (uint32_t)iova_addr;
		vkbuf->size = size;
		break;
	}
	case VKBUF_MAP_FPHY_DIOVA:
	{
#if (defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U))
		struct port_mva_info_t port_info;

		sg = &(vkbuf->sg);
		ret = sg_alloc_table(sg, 1, GFP_KERNEL);
		if (ret) {
			LOG_ERR("fail to sg_alloc_table\n");
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->kva = (void *)(uintptr_t)kva_addr;
		sg_init_table(sg->sgl, 1);
		sg_set_page(sg->sgl, phys_to_page(phy_addr), size,
			    offset_in_page(phy_addr));

		memset((void *)&port_info, 0, sizeof(port_info));
		port_info.emoduleid = 0;
		port_info.buf_size = size;

		ret = m4u_alloc_mva_sg(&port_info, sg);
		if (ret) {
			LOG_ERR("fail to m4u_alloc_mva_sg\n");
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = phy_addr;
		vkbuf->iova_addr = (uint32_t)port_info.mva;
		vkbuf->size = size;
#else
		LOG_ERR("fail to VKBUF_MAP_FPHY_DIOVA\n");
#endif
		break;
	}
	case VKBUF_MAP_DPHY_FIOVA:
	{
		vkbuf->kva = dma_alloc_coherent_fix_iova(vpu_device->dev,
							iova_addr,
							size, GFP_KERNEL);
		if (vkbuf->kva == NULL) {
			LOG_ERR("fail to dma_alloc_coherent_fix_iova\n");
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->iova_addr = iova_addr;
		vkbuf->size = size;
		break;
	}
	case VKBUF_MAP_DPHY_DIOVA:
	{
		vkbuf->kva = dma_alloc_coherent(vpu_device->dev, size,
						&dma_addr, GFP_KERNEL);
		if (vkbuf->kva == NULL) {
			LOG_ERR("fail to dma_alloc_coherent\n");
			kfree(vkbuf);
			return NULL;
		}

		vkbuf->handle = 0;
		vkbuf->usage = usage;
		vkbuf->phy_addr = 0;
		vkbuf->iova_addr = (uint32_t)dma_addr;
		vkbuf->size = size;
		break;
	}
	default:
	{
		break;
	}
	}

	return vkbuf;
}

static void vbuf_dma_kunmap(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf)
{
	int ret;

	switch (vkbuf->usage) {
	case VKBUF_MAP_FPHY_FIOVA:
	{
		dma_unmap_sg_within_reserved_iova(vpu_device->dev,
						vkbuf->sg.sgl,
						vkbuf->sg.nents,
						IOMMU_READ | IOMMU_WRITE,
						vkbuf->size);
		sg_free_table(&(vkbuf->sg));
		break;
	}
	case VKBUF_MAP_FPHY_DIOVA:
	{
		ret = m4u_dealloc_mva_sg(0, &(vkbuf->sg), vkbuf->size,
					 vkbuf->iova_addr);
		if (ret)
			LOG_ERR("fail to m4u_dealloc_mva_sg\n");

		sg_free_table(&(vkbuf->sg));
		break;
	}
	case VKBUF_MAP_DPHY_FIOVA:
	{
		dma_free_coherent_fix_iova(vpu_device->dev, vkbuf->kva,
					   vkbuf->iova_addr, vkbuf->size);
		break;
	}
	case VKBUF_MAP_DPHY_DIOVA:
	{
		dma_free_coherent(vpu_device->dev, vkbuf->size, vkbuf->kva,
				  vkbuf->iova_addr);
		break;
	}
	default:
	{
		break;
	}
	}

	kfree(vkbuf);
}

static void vbuf_dma_init(struct vpu_device *vpu_device)
{
	vbuf_std_init(vpu_device);
}

static void vbuf_dma_deinit(struct vpu_device *vpu_device)
{
	vbuf_std_deinit(vpu_device);
}

static uint64_t vbuf_dma_import_handle(struct vpu_device *vpu_device, int fd)
{
	return 0;
}

static void vbuf_dma_free_handle(struct vpu_device *vpu_device, uint64_t id)
{
}

struct vpu_map_ops vpu_dma_mapops = {
	.init_phy_iova = vbuf_dma_init,
	.deinit_phy_iova = vbuf_dma_deinit,
	.kmap_phy_iova = vbuf_dma_kmap,
	.kunmap_phy_iova = vbuf_dma_kunmap,
	.import_handle = vbuf_dma_import_handle,
	.free_handle = vbuf_dma_free_handle,
};
