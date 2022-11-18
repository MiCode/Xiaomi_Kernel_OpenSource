/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_IOMMU_UTIL_H
#define __QCOM_IOMMU_UTIL_H

#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/iova.h>

/* QCOM iommu domain attributes */
#define EXTENDED_ATTR_BASE			(DOMAIN_ATTR_MAX + 16)

#define DOMAIN_ATTR_CONTEXT_BANK		(EXTENDED_ATTR_BASE + 0)
#define DOMAIN_ATTR_NON_FATAL_FAULTS		(EXTENDED_ATTR_BASE + 1)
#define DOMAIN_ATTR_S1_BYPASS			(EXTENDED_ATTR_BASE + 2)
#define DOMAIN_ATTR_ATOMIC			(EXTENDED_ATTR_BASE + 3)
#define DOMAIN_ATTR_SECURE_VMID			(EXTENDED_ATTR_BASE + 4)
#define DOMAIN_ATTR_FAST			(EXTENDED_ATTR_BASE + 5)
#define DOMAIN_ATTR_PGTBL_INFO			(EXTENDED_ATTR_BASE + 6)
#define DOMAIN_ATTR_USE_UPSTREAM_HINT		(EXTENDED_ATTR_BASE + 7)
#define DOMAIN_ATTR_EARLY_MAP			(EXTENDED_ATTR_BASE + 8)
#define DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT	(EXTENDED_ATTR_BASE + 9)
#define DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT	(EXTENDED_ATTR_BASE + 10)
#define DOMAIN_ATTR_USE_LLC_NWA			(EXTENDED_ATTR_BASE + 11)
#define DOMAIN_ATTR_SPLIT_TABLES		(EXTENDED_ATTR_BASE + 12)
#define DOMAIN_ATTR_FAULT_MODEL_NO_CFRE		(EXTENDED_ATTR_BASE + 13)
#define DOMAIN_ATTR_FAULT_MODEL_NO_STALL	(EXTENDED_ATTR_BASE + 14)
#define DOMAIN_ATTR_FAULT_MODEL_HUPCF		(EXTENDED_ATTR_BASE + 15)
#define DOMAIN_ATTR_EXTENDED_MAX		(EXTENDED_ATTR_BASE + 16)

/* iommu transaction flags */
/* 1 Write, 0 Read */
#define QCOM_IOMMU_ATOS_TRANS_WRITE	BIT(0)
/* 1 Privileged, 0 Unprivileged */
#define QCOM_IOMMU_ATOS_TRANS_PRIV	BIT(1)
/* 1 Instruction fetch, 0 Data access */
#define QCOM_IOMMU_ATOS_TRANS_INST	BIT(2)
/* Non secure unprivileged Data read operation */
#define QCOM_IOMMU_ATOS_TRANS_DEFAULT	(0U)

/* Use upstream device's bus attribute */
#define IOMMU_USE_UPSTREAM_HINT	(IOMMU_SYS_CACHE)

/* Use upstream device's bus attribute with no write-allocate cache policy */
#define IOMMU_USE_LLC_NWA	(IOMMU_SYS_CACHE_NWA)

/* vendor iommu fault flags */
#define IOMMU_FAULT_TRANSLATION         (1 << 2)
#define IOMMU_FAULT_PERMISSION          (1 << 3)
#define IOMMU_FAULT_EXTERNAL            (1 << 4)
#define IOMMU_FAULT_TRANSACTION_STALLED (1 << 5)

/* iommu transaction flags */
#define IOMMU_TRANS_WRITE	BIT(0)	/* 1 Write, 0 Read */
#define IOMMU_TRANS_PRIV	BIT(1)	/* 1 Privileged, 0 Unprivileged */
#define IOMMU_TRANS_INST	BIT(2)	/* 1 Instruction fetch, 0 Data access */
#define IOMMU_TRANS_SEC	BIT(3)	/* 1 Secure, 0 Non-secure access*/

/* Non secure unprivileged Data read operation */
#define IOMMU_TRANS_DEFAULT	(0U)

struct iommu_pgtbl_info {
	void *ops;
};

struct qcom_iommu_atos_txn {
	u64 addr;
	u32 flags;
	u32 id;
};

enum sid_switch_direction {
	SID_ACQUIRE,
	SID_RELEASE,
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
	int (*get_asid_nr)(struct iommu_domain *domain);
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

int qcom_iommu_sid_switch(struct device *dev, enum sid_switch_direction dir);

extern int qcom_iommu_get_fault_ids(struct iommu_domain *domain,
				struct qcom_iommu_fault_ids *f_ids);
extern int qcom_iommu_get_msi_size(struct device *dev, u32 *msi_size);

int qcom_iommu_get_asid_nr(struct iommu_domain *domain);

#ifdef CONFIG_IOMMU_IO_PGTABLE_LPAE
int __init qcom_arm_lpae_do_selftests(void);
#else
static inline int __init qcom_arm_lpae_do_selftests(void)
{
	return 0;
}
#endif
#endif /* __QCOM_IOMMU_UTIL_H */
