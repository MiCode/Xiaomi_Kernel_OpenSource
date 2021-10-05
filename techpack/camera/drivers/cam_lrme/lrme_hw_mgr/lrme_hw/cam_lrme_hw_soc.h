/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_LRME_HW_SOC_H_
#define _CAM_LRME_HW_SOC_H_

#include "cam_soc_util.h"

struct cam_lrme_soc_private {
	uint32_t cpas_handle;
};

int cam_lrme_soc_enable_resources(struct cam_hw_info *lrme_hw);
int cam_lrme_soc_disable_resources(struct cam_hw_info *lrme_hw);
int cam_lrme_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *private_data);
int cam_lrme_soc_deinit_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_LRME_HW_SOC_H_ */
