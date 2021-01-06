/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef __QCOM_QCOM_IO_PGTABLE_H
#define __QCOM_QCOM_IO_PGTABLE_H

#include <linux/io-pgtable.h>

struct qcom_io_pgtable_info {
	struct io_pgtable_cfg cfg;
};

#define IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT BIT(31)
#define IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA       BIT(30)

#define ARM_V8L_FAST ((unsigned int)-1)

struct io_pgtable_ops *qcom_alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
				struct qcom_io_pgtable_info *pgtbl_info,
				void *cookie);
void qcom_free_io_pgtable_ops(struct io_pgtable_ops *ops);

#ifdef CONFIG_IOMMU_IO_PGTABLE_FAST
extern struct io_pgtable_init_fns io_pgtable_av8l_fast_init_fns;
#endif

#endif /* __QCOM_QCOM_IO_PGTABLE_H */
