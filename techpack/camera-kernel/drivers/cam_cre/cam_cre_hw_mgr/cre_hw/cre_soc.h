/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CRE_SOC_H_
#define _CAM_CRE_SOC_H_

#include "cam_soc_util.h"

#define CAM_CRE_HW_MAX_NUM_PID 2

/**
 * struct cam_cre_soc_private
 *
 * @num_pid: CRE number of pids
 * @pid:     CRE pid value list
 */
struct cam_cre_soc_private {
	uint32_t num_pid;
	uint32_t pid[CAM_CRE_HW_MAX_NUM_PID];
};

int cam_cre_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t cre_irq_handler, void *irq_data);

int cam_cre_enable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_cre_disable_soc_resources(struct cam_hw_soc_info *soc_info);

int cam_cre_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate);
#endif /* _CAM_CRE_SOC_H_*/
