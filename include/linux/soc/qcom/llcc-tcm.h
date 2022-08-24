/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _TCM_QCOM_H_
#define _TCM_QCOM_H_

struct llcc_tcm_data {
	phys_addr_t phys_addr;
	void __iomem *virt_addr;
	size_t mem_size;
};

int qcom_llcc_tcm_probe(struct platform_device *pdev,
		const struct llcc_slice_config *table, size_t size,
		struct device_node *node);

struct llcc_tcm_data *llcc_tcm_activate(void);

phys_addr_t llcc_tcm_get_phys_addr(struct llcc_tcm_data *tcm_data);

void __iomem *llcc_tcm_get_virt_addr(struct llcc_tcm_data *tcm_data);

size_t llcc_tcm_get_slice_size(struct llcc_tcm_data *tcm_data);

void llcc_tcm_deactivate(struct llcc_tcm_data *tcm_data);

#endif //_TCM_QCOM_H_
