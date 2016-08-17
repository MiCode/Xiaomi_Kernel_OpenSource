/*
 * IOMMU backend support for NVMAP
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/slab.h>
#include <mach/iomap.h>
#include <mach/iovmm.h>

#include "nvmap.h"

struct tegra_iovmm_area *tegra_iommu_create_vm(struct device *dev,
			       dma_addr_t req, size_t size, pgprot_t prot)
{
	struct tegra_iovmm_area *area;
	dma_addr_t iova;

	area = kmalloc(sizeof(*area), GFP_KERNEL);
	if (!area)
		return NULL;

	if (!req)
		iova = dma_iova_alloc(dev, size);
	else
		iova = dma_iova_alloc_at(dev, req, size);

	if (iova == DMA_ERROR_CODE)
		goto err_out;
	area->iovm_start = iova;
	area->iovm_length = size;
	area->pgprot = prot;
	area->dev = dev;
	return area;

err_out:
	kfree(area);
	return NULL;
}

void tegra_iommu_free_vm(struct tegra_iovmm_area *area)
{
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	dma_unmap_single_attrs(area->dev, area->iovm_start, area->iovm_length,
			       0, &attrs);
	kfree(area);
}

void tegra_iommu_zap_vm(struct tegra_iovmm_area *area)
{
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	dma_set_attr(DMA_ATTR_SKIP_FREE_IOVA, &attrs);
	dma_unmap_single_attrs(area->dev, area->iovm_start, area->iovm_length,
			       0, &attrs);
}

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU

static inline int tegra_iommu_create_map(struct device *dev)
{
	return 0;
}

static inline void tegra_iommu_delete_map(struct device *dev)
{
}

#else

static int tegra_iommu_create_map(struct device *dev)
{
	int err;
	struct dma_iommu_mapping *map;

	map = arm_iommu_create_mapping(&platform_bus_type,
				       TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE, 0);
	if (IS_ERR(map))
		return PTR_ERR(map);

	err = arm_iommu_attach_device(dev, map);
	if (err) {
		arm_iommu_release_mapping(map);
		return err;
	}
	return 0;
}

static void tegra_iommu_delete_map(struct device *dev)
{
	arm_iommu_release_mapping(dev->archdata.mapping);
}

#endif

struct tegra_iovmm_client *tegra_iommu_alloc_client(struct device *dev)
{
	struct tegra_iovmm_client *client;

	if (WARN_ON(!dev))
		return NULL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	if (tegra_iommu_create_map(dev)) {
		kfree(client);
		return NULL;
	}

	client->dev = dev;

	return client;
}

void tegra_iommu_free_client(struct tegra_iovmm_client *client)
{
	if (WARN_ON(!client))
		return;
	tegra_iommu_delete_map(client->dev);
	kfree(client);
}
