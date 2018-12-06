/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
