/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_IOMMU_UTIL_H
#define __QCOM_IOMMU_UTIL_H

#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/iova.h>

/* iommu transaction flags */
/* 1 Write, 0 Read */
#define QCOM_IOMMU_ATOS_TRANS_WRITE	BIT(0)
/* 1 Privileged, 0 Unprivileged */
#define QCOM_IOMMU_ATOS_TRANS_PRIV	BIT(1)
/* 1 Instruction fetch, 0 Data access */
#define QCOM_IOMMU_ATOS_TRANS_INST	BIT(2)
/* Non secure unprivileged Data read operation */
#define QCOM_IOMMU_ATOS_TRANS_DEFAULT	(0U)

struct qcom_iommu_atos_txn {
	u64 addr;
	u32 flags;
	u32 id;
};

enum sid_switch_direction {
	SID_SWITCH_HLOS_TO_SECURE,
	SID_SWITCH_SECURE_TO_HLOS,
};

struct qcom_iommu_fault_ids {
	u32 bid;
	u32 pid;
	u32 mid;
};

/*
 * @sid_switch: add/remove all SIDS in the iommu domain containing dev from
 *              iommu registers.
 */
struct qcom_iommu_ops {
	phys_addr_t (*iova_to_phys_hard)(struct iommu_domain *domain,
					struct qcom_iommu_atos_txn *txn);
	int (*sid_switch)(struct device *dev, enum sid_switch_direction dir);
	int (*get_fault_ids)(struct iommu_domain *domain,
			struct qcom_iommu_fault_ids *ids);
	struct iommu_ops iommu_ops;
};
#define to_qcom_iommu_ops(x) (container_of(x, struct qcom_iommu_ops, iommu_ops))

struct device_node *qcom_iommu_group_parse_phandle(struct device *dev);
int qcom_iommu_generate_dma_regions(struct device *dev,
				    struct list_head *head);
void qcom_iommu_generate_resv_regions(struct device *dev,
				      struct list_head *list);
int qcom_iommu_get_fast_iova_range(struct device *dev,
				   dma_addr_t *ret_iova_base,
				   dma_addr_t *ret_iova_end);

/* Remove once these functions are exported by upstream kernel */
void qcom_iommu_get_resv_regions(struct device *dev, struct list_head *list);
void qcom_iommu_put_resv_regions(struct device *dev, struct list_head *list);

phys_addr_t qcom_iommu_iova_to_phys_hard(struct iommu_domain *domain,
				    struct qcom_iommu_atos_txn *txn);


extern int qcom_iommu_get_fault_ids(struct iommu_domain *domain,
				struct qcom_iommu_fault_ids *f_ids);


extern int __init qcom_dma_iommu_generic_driver_init(void);
extern void qcom_dma_iommu_generic_driver_exit(void);
#endif /* __QCOM_IOMMU_UTIL_H */
