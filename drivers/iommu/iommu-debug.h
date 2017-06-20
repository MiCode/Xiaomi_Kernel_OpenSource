/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
