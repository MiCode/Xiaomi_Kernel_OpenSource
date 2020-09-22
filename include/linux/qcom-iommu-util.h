/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_IOMMU_UTIL_H
#define __QCOM_IOMMU_UTIL_H

#include <linux/iommu.h>

struct qcom_iommu_ops {
	struct iommu_ops iommu_ops;
};
#define to_qcom_iommu_ops(x) (container_of(x, struct qcom_iommu_ops, iommu_ops))

struct device_node *qcom_iommu_group_parse_phandle(struct device *dev);
int qcom_iommu_generate_dma_regions(struct device *dev,
				    struct list_head *head);
void qcom_iommu_generate_resv_regions(struct device *dev,
				      struct list_head *list);

/* Remove once these functions are exported by upstream kernel */
void qcom_iommu_get_resv_regions(struct device *dev, struct list_head *list);
void qcom_iommu_put_resv_regions(struct device *dev, struct list_head *list);

extern int __init qcom_dma_iommu_generic_driver_init(void);
extern void qcom_dma_iommu_generic_driver_exit(void);
#endif /* __QCOM_IOMMU_UTIL_H */
