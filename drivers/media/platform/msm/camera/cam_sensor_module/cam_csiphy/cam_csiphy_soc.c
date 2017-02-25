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
#include "cam_sensor_util.h"

int32_t cam_csiphy_enable_hw(struct csiphy_device *csiphy_dev)
{
	int32_t rc = 0;
	long clk_rate = 0;

	if (csiphy_dev->ref_count++) {
		pr_err("%s:%d csiphy refcount = %d\n", __func__,
			__LINE__, csiphy_dev->ref_count);
		return rc;
	}

	rc = msm_camera_config_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->num_vreg, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d failed regulator get\n", __func__, __LINE__);
		goto csiphy_config_regulator_fail;
	}

	rc = msm_camera_enable_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->num_vreg, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d failed to enable regulators\n", __func__, rc);
		goto csiphy_regulator_fail;
	}

	/*Enable clocks*/
	rc = msm_camera_clk_enable(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, true);
	if (rc < 0) {
		pr_err("%s: csiphy clk enable failed\n", __func__);
		csiphy_dev->ref_count--;
		goto csiphy_regulator_fail;
	}

	clk_rate = msm_camera_clk_set_rate(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_clk[csiphy_dev->csiphy_clk_index],
		clk_rate);
	if (clk_rate < 0) {
		pr_err("csiphy_clk_set_rate failed\n");
		goto csiphy_clk_enable_fail;
	}

	rc = msm_camera_enable_irq(csiphy_dev->irq, ENABLE_IRQ);
	if (rc < 0) {
		pr_err("%s:%d :ERROR: irq enable failed\n",
			__func__, __LINE__);
		goto csiphy_clk_enable_fail;
		return -EINVAL;
	}

	cam_csiphy_reset(csiphy_dev);

	return rc;
csiphy_clk_enable_fail:
	msm_camera_clk_enable(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, false);
csiphy_regulator_fail:
	msm_camera_enable_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->num_vreg, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);
csiphy_config_regulator_fail:
	msm_camera_config_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->num_vreg, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);

	return rc;
}

int32_t cam_csiphy_disable_hw(struct platform_device *pdev)
{
	struct csiphy_device *csiphy_dev =
		platform_get_drvdata(pdev);

	/*Disable regulators*/
	msm_camera_enable_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg,
		csiphy_dev->num_vreg, NULL, 0,
		&csiphy_dev->csiphy_reg_ptr[0], 0);

	/*Disable clocks*/
	msm_camera_clk_enable(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, false);

	/*Disable IRQ*/
	msm_camera_enable_irq(csiphy_dev->irq, false);

	return 0;

}

int32_t cam_csiphy_parse_dt_info(struct platform_device *pdev,
	struct csiphy_device *csiphy_dev)
{
	int32_t   rc = 0, i = 0;
	uint32_t  clk_cnt = 0;
	char      *csi_3p_clk_name = "csi_phy_3p_clk";
	char      *csi_3p_clk_src_name = "csiphy_3p_clk_src";

	if (pdev->dev.of_node) {
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);
		CDBG("%s: device id = %d\n", __func__, pdev->id);
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

	rc = msm_camera_get_clk_info(csiphy_dev->v4l2_dev_str.pdev,
		&csiphy_dev->csiphy_clk_info,
		&csiphy_dev->csiphy_clk,
		&csiphy_dev->num_clk);
	if (rc < 0) {
		pr_err("%s:%d failed clock get\n", __func__, __LINE__);
		return rc;
	}

	if (csiphy_dev->num_clk > CSIPHY_NUM_CLK_MAX) {
		pr_err("%s: invalid clk count=%zu, max is %d\n", __func__,
			csiphy_dev->num_clk, CSIPHY_NUM_CLK_MAX);
		goto clk_mem_ovf_err;
	}

	for (i = 0; i < csiphy_dev->num_clk; i++) {
		if (!strcmp(csiphy_dev->csiphy_clk_info[i].clk_name,
			csi_3p_clk_src_name)) {
			csiphy_dev->csiphy_3p_clk_info[0].clk_name =
				csiphy_dev->csiphy_clk_info[i].clk_name;
			csiphy_dev->csiphy_3p_clk_info[0].clk_rate =
				csiphy_dev->csiphy_clk_info[i].clk_rate;
			csiphy_dev->csiphy_3p_clk[0] =
				csiphy_dev->csiphy_clk[i];
			continue;
		} else if (!strcmp(csiphy_dev->csiphy_clk_info[i].clk_name,
					csi_3p_clk_name)) {
			csiphy_dev->csiphy_3p_clk_info[1].clk_name =
				csiphy_dev->csiphy_clk_info[i].clk_name;
			csiphy_dev->csiphy_3p_clk_info[1].clk_rate =
				csiphy_dev->csiphy_clk_info[i].clk_rate;
			csiphy_dev->csiphy_3p_clk[1] =
				csiphy_dev->csiphy_clk[i];
			continue;
		}

		if (!strcmp(csiphy_dev->csiphy_clk_info[clk_cnt].clk_name,
			"csiphy_timer_src_clk")) {
			csiphy_dev->csiphy_max_clk =
				csiphy_dev->csiphy_clk_info[clk_cnt].clk_rate;
			csiphy_dev->csiphy_clk_index = clk_cnt;
		}
		CDBG("%s: clk_rate[%d] = %ld\n", __func__, clk_cnt,
			csiphy_dev->csiphy_clk_info[clk_cnt].clk_rate);
		clk_cnt++;
	}

	rc = cam_sensor_get_dt_vreg_data(pdev->dev.of_node,
		&(csiphy_dev->csiphy_vreg), &(csiphy_dev->num_vreg));
	if (rc < 0) {
		pr_err("%s:%d Reg get failed\n", __func__, __LINE__);
		csiphy_dev->num_vreg = 0;
	}

	csiphy_dev->base = msm_camera_get_reg_base(pdev, "csiphy", true);
	if (!csiphy_dev->base) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_resource;
	}

	csiphy_dev->irq = msm_camera_get_irq(pdev, "csiphy");
	if (!csiphy_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto csiphy_no_resource;
	}

	rc = msm_camera_register_irq(pdev, csiphy_dev->irq,
		cam_csiphy_irq, IRQF_TRIGGER_RISING, "csiphy", csiphy_dev);
	if (rc < 0) {
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto csiphy_no_resource;
	}
	msm_camera_enable_irq(csiphy_dev->irq, false);
	return rc;

csiphy_no_resource:
	msm_camera_put_reg_base(pdev, csiphy_dev->base, "csiphy", true);
clk_mem_ovf_err:
	msm_camera_put_clk_info(csiphy_dev->v4l2_dev_str.pdev,
		&csiphy_dev->csiphy_clk_info,
		&csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk);
	return rc;
}

int32_t cam_csiphy_soc_release(struct csiphy_device *csiphy_dev)
{

	if (!csiphy_dev || !csiphy_dev->ref_count) {
		pr_err("%s csiphy dev NULL / ref_count ZERO\n", __func__);
		return 0;
	}

	if (--csiphy_dev->ref_count) {
		pr_err("%s:%d csiphy refcount = %d\n", __func__,
			__LINE__, csiphy_dev->ref_count);
		return 0;
	}

	cam_csiphy_reset(csiphy_dev);

	msm_camera_enable_irq(csiphy_dev->irq, false);

	msm_camera_clk_enable(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_clk_info, csiphy_dev->csiphy_clk,
		csiphy_dev->num_clk, false);

	msm_camera_enable_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg, csiphy_dev->num_vreg,
		NULL, 0, &csiphy_dev->csiphy_reg_ptr[0], 0);

	msm_camera_config_vreg(&csiphy_dev->v4l2_dev_str.pdev->dev,
		csiphy_dev->csiphy_vreg, csiphy_dev->num_vreg,
		NULL, 0, &csiphy_dev->csiphy_reg_ptr[0], 0);


	return 0;
}
