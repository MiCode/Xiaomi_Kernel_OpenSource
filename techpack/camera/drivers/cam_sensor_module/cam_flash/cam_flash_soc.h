/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_FLASH_SOC_H_
#define _CAM_FLASH_SOC_H_

#include "cam_flash_dev.h"

int cam_flash_get_dt_data(struct cam_flash_ctrl *fctrl,
	struct cam_hw_soc_info *soc_info);

void cam_flash_put_source_node_data(struct cam_flash_ctrl *fctrl);
#endif /*_CAM_FLASH_SOC_H_*/
