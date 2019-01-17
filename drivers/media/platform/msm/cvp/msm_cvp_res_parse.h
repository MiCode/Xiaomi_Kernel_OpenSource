/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_CVP_RES_PARSE_H__
#define __MSM_CVP_RES_PARSE_H__
#include <linux/of.h>
#include "msm_cvp_resources.h"
#include "msm_cvp_common.h"
void msm_cvp_free_platform_resources(
		struct msm_cvp_platform_resources *res);

int read_hfi_type(struct platform_device *pdev);

int cvp_read_platform_resources_from_drv_data(
		struct msm_cvp_core *core);
int cvp_read_platform_resources_from_dt(
		struct msm_cvp_platform_resources *res);

int cvp_read_context_bank_resources_from_dt(struct platform_device *pdev);

int cvp_read_bus_resources_from_dt(struct platform_device *pdev);
int cvp_read_mem_cdsp_resources_from_dt(struct platform_device *pdev);

int msm_cvp_load_u32_table(struct platform_device *pdev,
		struct device_node *of_node, char *table_name, int struct_size,
		u32 **table, u32 *num_elements);

#endif
