/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include "msm_sd.h"
#include "msm_early_cam.h"
#include "msm_cam_cci_hwreg.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "cam_hw_ops.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#undef EARLY_CAM_DBG
#ifdef MSM_EARLY_CAM_DEBUG
#define EARLY_CAM_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define EARLY_CAM_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define MSM_EARLY_CAM_DRV_NAME "msm_early_cam"
static struct platform_driver msm_early_camera_driver;
static struct early_cam_device *new_early_cam_dev;

int msm_early_cam_disable_clocks(void)
{
	int rc = 0;

	CDBG("%s:\n", __func__);
	/* Vote OFF for clocks */
	if (new_early_cam_dev == NULL) {
		rc = -EINVAL;
		pr_err("%s: clock structure uninitialised %d\n", __func__,
			rc);
		return rc;
	}

	if ((new_early_cam_dev->pdev == NULL) ||
		(new_early_cam_dev->early_cam_clk_info == NULL) ||
		(new_early_cam_dev->early_cam_clk == NULL) ||
		(new_early_cam_dev->num_clk == 0)) {
		rc = -EINVAL;
		pr_err("%s: Clock details uninitialised %d\n", __func__,
			rc);
		return rc;
	}

	rc = msm_camera_clk_enable(&new_early_cam_dev->pdev->dev,
		new_early_cam_dev->early_cam_clk_info,
		new_early_cam_dev->early_cam_clk,
		new_early_cam_dev->num_clk, false);
	if (rc < 0) {
		pr_err("%s: clk disable failed %d\n", __func__, rc);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		CAM_AHB_SUSPEND_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote OFF AHB_CLIENT_CSIPHY %d\n",
			__func__, rc);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSID,
		CAM_AHB_SUSPEND_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote OFF AHB_CLIENT_CSID %d\n",
			__func__, rc);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CCI,
		CAM_AHB_SUSPEND_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote OFF AHB_CLIENT_CCI %d\n",
			__func__, rc);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_ISPIF,
		CAM_AHB_SUSPEND_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote OFF AHB_CLIENT_ISPIF %d\n",
			__func__, rc);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_VFE0,
			CAM_AHB_SUSPEND_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote OFF AHB_CLIENT_VFE0 %d\n",
			__func__, rc);
		return rc;
	}
	pr_debug("Turned OFF camera clocks\n");
	return 0;

}
static int msm_early_cam_probe(struct platform_device *pdev)
{
	int rc = 0;

	CDBG("%s: pdev %pK device id = %d\n", __func__, pdev, pdev->id);

	/* Vote for Early camera if enabled */
	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSIPHY,
		CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CSID,
		CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CCI,
		CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_ISPIF,
		CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_VFE0,
			CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	new_early_cam_dev = kzalloc(sizeof(struct early_cam_device),
		GFP_KERNEL);

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	rc = msm_camera_get_clk_info_and_rates(pdev,
		&new_early_cam_dev->early_cam_clk_info,
		&new_early_cam_dev->early_cam_clk,
		&new_early_cam_dev->early_cam_clk_rates,
		&new_early_cam_dev->num_clk_cases,
		&new_early_cam_dev->num_clk);
	if (rc < 0) {
		pr_err("%s: msm_early_cam_get_clk_info() failed", __func__);
		kfree(new_early_cam_dev);
		return -EFAULT;
	}

	new_early_cam_dev->ref_count = 0;
	new_early_cam_dev->pdev = pdev;

	rc = msm_camera_get_dt_vreg_data(
		new_early_cam_dev->pdev->dev.of_node,
		&(new_early_cam_dev->early_cam_vreg),
		&(new_early_cam_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: msm_camera_get_dt_vreg_data fail\n", __func__);
		rc = -EFAULT;
		goto early_cam_release_mem;
	}

	if ((new_early_cam_dev->regulator_count < 0) ||
		(new_early_cam_dev->regulator_count > MAX_REGULATOR)) {
		pr_err("%s: invalid reg count = %d, max is %d\n", __func__,
			new_early_cam_dev->regulator_count, MAX_REGULATOR);
		rc = -EFAULT;
		goto early_cam_invalid_vreg_data;
	}

	rc = msm_camera_config_vreg(&new_early_cam_dev->pdev->dev,
		new_early_cam_dev->early_cam_vreg,
		new_early_cam_dev->regulator_count,
		NULL,
		0,
		&new_early_cam_dev->early_cam_reg_ptr[0], 1);
	if (rc < 0)
		pr_err("%s:%d early_cam config_vreg failed\n", __func__,
			__LINE__);

	rc = msm_camera_enable_vreg(&new_early_cam_dev->pdev->dev,
		new_early_cam_dev->early_cam_vreg,
		new_early_cam_dev->regulator_count,
		NULL,
		0,
		&new_early_cam_dev->early_cam_reg_ptr[0], 1);
	if (rc < 0)
		pr_err("%s:%d early_cam enable_vreg failed\n", __func__,
		__LINE__);

	rc = msm_camera_clk_enable(&new_early_cam_dev->pdev->dev,
		new_early_cam_dev->early_cam_clk_info,
		new_early_cam_dev->early_cam_clk,
		new_early_cam_dev->num_clk, true);

	if (rc < 0) {
		pr_err("%s: clk enable failed %d\n", __func__, rc);
		rc = 0;
		goto early_cam_release_mem;
	}

	platform_set_drvdata(pdev, new_early_cam_dev);

	return 0;

early_cam_invalid_vreg_data:
	kfree(new_early_cam_dev->early_cam_vreg);
early_cam_release_mem:
	kfree(new_early_cam_dev);
	new_early_cam_dev = NULL;
	return rc;
}

static int msm_early_cam_exit(struct platform_device *pdev)
{
	return 0;
}

static int __init msm_early_cam_init_module(void)
{
	return platform_driver_register(&msm_early_camera_driver);
}

static void __exit msm_early_cam_exit_module(void)
{
	kfree(new_early_cam_dev);
	platform_driver_unregister(&msm_early_camera_driver);
}

static const struct of_device_id msm_early_camera_match_table[] = {
	{ .compatible = "qcom,early-cam" },
	{},
};

static struct platform_driver msm_early_camera_driver = {
	.probe = msm_early_cam_probe,
	.remove = msm_early_cam_exit,
	.driver = {
		.name = MSM_EARLY_CAM_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_early_camera_match_table,
	},
};

MODULE_DEVICE_TABLE(of, msm_early_camera_match_table);

module_init(msm_early_cam_init_module);
module_exit(msm_early_cam_exit_module);
MODULE_DESCRIPTION("MSM early camera driver");
MODULE_LICENSE("GPL v2");
