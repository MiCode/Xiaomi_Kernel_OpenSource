/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#ifndef __LINUX_QTI_IOMMU_LOGGER_H
#define __LINUX_QTI_IOMMU_LOGGER_H

struct iommu_debug_attachment;

#if IS_ENABLED(CONFIG_QTI_IOMMU_SUPPORT)

int iommu_logger_register(struct iommu_debug_attachment **a,
			  struct iommu_domain *domain,
			  struct iommu_group *group);
void iommu_logger_unregister(struct iommu_debug_attachment *a);
#else
static inline int iommu_logger_register(struct iommu_debug_attachment **a,
					struct iommu_domain *domain,
					struct iommu_group *group)
{
	return 0;
}

static inline void iommu_logger_unregister(struct iommu_debug_attachment *a) {}
#endif /* CONFIG_QTI_IOMMU_LOGGER */
#endif /* __LINUX_QTI_IOMMU_LOGGER_H */
