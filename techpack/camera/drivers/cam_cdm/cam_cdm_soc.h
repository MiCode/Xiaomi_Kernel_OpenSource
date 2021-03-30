/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CDM_SOC_H_
#define _CAM_CDM_SOC_H_

#define CAM_HW_CDM_CPAS_0_NAME   "qcom,cam170-cpas-cdm0"
#define CAM_HW_CDM_CPAS_NAME_1_0 "qcom,cam-cpas-cdm1_0"
#define CAM_HW_CDM_CPAS_NAME_1_1 "qcom,cam-cpas-cdm1_1"
#define CAM_HW_CDM_CPAS_NAME_1_2 "qcom,cam-cpas-cdm1_2"
#define CAM_HW_CDM_IFE_NAME_1_2  "qcom,cam-ife-cdm1_2"
#define CAM_HW_CDM_CPAS_NAME_2_0 "qcom,cam-cpas-cdm2_0"
#define CAM_HW_CDM_OPE_NAME_2_0  "qcom,cam-ope-cdm2_0"
#define CAM_HW_CDM_CPAS_NAME_2_1 "qcom,cam-cpas-cdm2_1"
#define CAM_HW_CDM_RT_NAME_2_1   "qcom,cam-rt-cdm2_1"
#define CAM_HW_CDM_OPE_NAME_2_1  "qcom,cam-ope-cdm2_1"

int cam_hw_cdm_soc_get_dt_properties(struct cam_hw_info *cdm_hw,
	const struct of_device_id *table);
bool cam_cdm_read_hw_reg(struct cam_hw_info *cdm_hw,
	uint32_t reg, uint32_t *value);
bool cam_cdm_write_hw_reg(struct cam_hw_info *cdm_hw,
	uint32_t reg, uint32_t value);
int cam_cdm_intf_mgr_soc_get_dt_properties(
	struct platform_device *pdev,
	struct cam_cdm_intf_mgr *mgr);
int cam_cdm_soc_load_dt_private(struct platform_device *pdev,
	struct cam_cdm_private_dt_data *ptr);

#endif /* _CAM_CDM_SOC_H_ */
