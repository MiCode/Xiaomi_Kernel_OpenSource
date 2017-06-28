/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_csiphy_soc.h"
#include "cam_csiphy_core.h"
#include "include/cam_csiphy_1_0_hwreg.h"

int32_t cam_csiphy_enable_hw(struct csiphy_device *csiphy_dev)
{
	int32_t rc = 0;
	struct cam_hw_soc_info   *soc_info;

	soc_info = &csiphy_dev->soc_info;

	if (csiphy_dev->ref_count++) {
		pr_err("%s:%d csiphy refcount = %d\n", __func__,
			__LINE__, csiphy_dev->ref_count);
		return rc;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_TURBO_VOTE, ENABLE_IRQ);
	if (rc < 0) {
		pr_err("%s:%d failed to enable platform resources %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	rc = cam_soc_util_set_clk_rate(
		soc_info->clk[csiphy_dev->csiphy_clk_index],
		soc_info->clk_name[csiphy_dev->csiphy_clk_index],
		soc_info->clk_rate[0][csiphy_dev->csiphy_clk_index]);

	if (rc < 0) {
		pr_err("%s:%d csiphy_clk_set_rate failed\n",
			__func__, __LINE__);
		goto csiphy_disable_platform_resource;
	}

	cam_csiphy_reset(csiphy_dev);

	return rc;


csiphy_disable_platform_resource:
	cam_soc_util_disable_platform_resource(soc_info, true, true);

	return rc;
}

int32_t cam_csiphy_disable_hw(struct csiphy_device *csiphy_dev)
{
	struct cam_hw_soc_info   *soc_info;

	if (!csiphy_dev || !csiphy_dev->ref_count) {
		pr_err("%s:%d csiphy dev NULL / ref_count ZERO\n", __func__,
			__LINE__);
		return 0;
	}
	soc_info = &csiphy_dev->soc_info;

	if (--csiphy_dev->ref_count) {
		pr_err("%s:%d csiphy refcount = %d\n", __func__,
			__LINE__, csiphy_dev->ref_count);
		return 0;
	}

	cam_csiphy_reset(csiphy_dev);

	cam_soc_util_disable_platform_resource(soc_info, true, true);

	return 0;
}

int32_t cam_csiphy_parse_dt_info(struct platform_device *pdev,
	struct csiphy_device *csiphy_dev)
{
	int32_t   rc = 0, i = 0;
	uint32_t  clk_cnt = 0;
	char      *csi_3p_clk_name = "csi_phy_3p_clk";
	char      *csi_3p_clk_src_name = "csiphy_3p_clk_src";
	struct cam_hw_soc_info   *soc_info;

	csiphy_dev->is_csiphy_3phase_hw = 0;
	soc_info = &csiphy_dev->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		pr_err("%s:%d :Error: parsing common soc dt(rc %d)\n",
			 __func__, __LINE__, rc);
		return  rc;
	}

	csiphy_dev->is_csiphy_3phase_hw = 0;
	if (of_device_is_compatible(csiphy_dev->v4l2_dev_str.pdev->dev.of_node,
		"qcom,csiphy-v1.0")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg =
			csiphy_2ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg =
			csiphy_3ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_reg = csiphy_reset_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_0;
		csiphy_dev->hw_version = CSIPHY_VERSION_V10;
		csiphy_dev->is_csiphy_3phase_hw = CSI_3PHASE_HW;
		csiphy_dev->clk_lane = 0;
	} else {
		pr_err("%s:%d, invalid hw version : 0x%x\n", __func__, __LINE__,
		csiphy_dev->hw_version);
		rc =  -EINVAL;
		return rc;
	}

	if (soc_info->num_clk > CSIPHY_NUM_CLK_MAX) {
		pr_err("%s:%d invalid clk count=%d, max is %d\n", __func__,
			__LINE__, soc_info->num_clk, CSIPHY_NUM_CLK_MAX);
		return -EINVAL;
	}
	for (i = 0; i < soc_info->num_clk; i++) {
		if (!strcmp(soc_info->clk_name[i],
			csi_3p_clk_src_name)) {
			csiphy_dev->csiphy_3p_clk_info[0].clk_name =
				soc_info->clk_name[i];
			csiphy_dev->csiphy_3p_clk_info[0].clk_rate =
				soc_info->clk_rate[0][i];
			csiphy_dev->csiphy_3p_clk[0] =
				soc_info->clk[i];
			continue;
		} else if (!strcmp(soc_info->clk_name[i],
				csi_3p_clk_name)) {
			csiphy_dev->csiphy_3p_clk_info[1].clk_name =
				soc_info->clk_name[i];
			csiphy_dev->csiphy_3p_clk_info[1].clk_rate =
				soc_info->clk_rate[0][i];
			csiphy_dev->csiphy_3p_clk[1] =
				soc_info->clk[i];
			continue;
		}

		if (!strcmp(soc_info->clk_name[i],
			"csiphy_timer_src_clk")) {
			csiphy_dev->csiphy_max_clk =
			soc_info->clk_rate[0][clk_cnt];
			csiphy_dev->csiphy_clk_index = clk_cnt;
		}
		CDBG("%s:%d clk_rate[%d] = %d\n", __func__, __LINE__, clk_cnt,
			soc_info->clk_rate[0][clk_cnt]);
		clk_cnt++;
	}
	rc = cam_soc_util_request_platform_resource(&csiphy_dev->soc_info,
		cam_csiphy_irq, csiphy_dev);

	return rc;
}

int32_t cam_csiphy_soc_release(struct csiphy_device *csiphy_dev)
{
	if (!csiphy_dev) {
		pr_err("%s:%d csiphy dev NULL\n", __func__, __LINE__);
		return 0;
	}

	cam_soc_util_release_platform_resource(&csiphy_dev->soc_info);

	return 0;
}
