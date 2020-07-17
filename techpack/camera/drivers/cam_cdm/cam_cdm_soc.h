/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CDM_SOC_H_
#define _CAM_CDM_SOC_H_

int cam_hw_cdm_soc_get_dt_properties(struct cam_hw_info *cdm_hw,
	const struct of_device_id *table);
bool cam_cdm_read_hw_reg(struct cam_hw_info *cdm_hw,
	enum cam_cdm_regs reg, uint32_t *value);
bool cam_cdm_write_hw_reg(struct cam_hw_info *cdm_hw,
	enum cam_cdm_regs reg, uint32_t value);
int cam_cdm_intf_mgr_soc_get_dt_properties(
	struct platform_device *pdev,
	struct cam_cdm_intf_mgr *mgr);
int cam_cdm_soc_load_dt_private(struct platform_device *pdev,
	struct cam_cdm_private_dt_data *ptr);

#endif /* _CAM_CDM_SOC_H_ */
