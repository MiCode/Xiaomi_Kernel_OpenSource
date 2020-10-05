/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef SDE_ROTATOR_SMMU_H
#define SDE_ROTATOR_SMMU_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/mdss_smmu_ext.h>

#include "sde_rotator_io_util.h"

enum sde_iommu_domain_type {
	SDE_IOMMU_DOMAIN_ROT_UNSECURE,
	SDE_IOMMU_DOMAIN_ROT_SECURE,
	SDE_IOMMU_MAX_DOMAIN
};

int sde_smmu_init(struct device *dev);

int sde_smmu_ctrl(int enable);

struct dma_buf_attachment *sde_smmu_dma_buf_attach(
		struct dma_buf *dma_buf, struct device *dev, int domain);

int sde_smmu_map_dma_buf(struct dma_buf *dma_buf,
		struct sg_table *table, int domain, dma_addr_t *iova,
		unsigned long *size, int dir);

void sde_smmu_unmap_dma_buf(struct sg_table *table, int domain,
		int dir, struct dma_buf *dma_buf);

int sde_smmu_secure_ctrl(int enable);

int sde_smmu_set_dma_direction(int dir);
#endif /* SDE_ROTATOR_SMMU_H */
