/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */

#ifndef IOMMU_DEBUG_H
#define IOMMU_DEBUG_H

#ifdef CONFIG_IOMMU_DEBUG_TRACKING

void iommu_debug_attach_device(struct iommu_domain *domain, struct device *dev);
void iommu_debug_domain_remove(struct iommu_domain *domain);

#else  /* !CONFIG_IOMMU_DEBUG_TRACKING */

static inline void iommu_debug_attach_device(struct iommu_domain *domain,
					     struct device *dev)
{
}

static inline void iommu_debug_domain_remove(struct iommu_domain *domain)
{
}

#endif  /* CONFIG_IOMMU_DEBUG_TRACKING */

#endif /* IOMMU_DEBUG_H */
