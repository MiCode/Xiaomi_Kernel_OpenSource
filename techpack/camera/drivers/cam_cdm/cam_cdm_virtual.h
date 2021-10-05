/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CDM_VIRTUAL_H_
#define _CAM_CDM_VIRTUAL_H_

#include "cam_cdm_intf_api.h"

int cam_virtual_cdm_probe(struct platform_device *pdev);
int cam_virtual_cdm_remove(struct platform_device *pdev);
int cam_cdm_util_cmd_buf_write(void __iomem **current_device_base,
	uint32_t *cmd_buf, uint32_t cmd_buf_size,
	struct cam_soc_reg_map *base_table[CAM_SOC_MAX_BLOCK],
	uint32_t base_array_size, uint8_t bl_tag);

#endif /* _CAM_CDM_VIRTUAL_H_ */
