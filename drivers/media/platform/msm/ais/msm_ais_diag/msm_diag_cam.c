/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include "msm_sd.h"
#include "msm_diag_cam.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "cam_hw_ops.h"
#include "msm_camera_diag_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#undef DIAG_CAM_DBG
#ifdef MSM_DIAG_CAM_DEBUG
#define DIAG_CAM_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define DIAG_CAM_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define MSM_DIAG_CAM_DRV_NAME "msm_diag_cam"
static struct platform_driver msm_diag_camera_driver;
static struct diag_cam_device *new_diag_cam_dev;

int msm_ais_enable_allclocks(void)
{
	int rc = 0;

	CDBG("%s:\n", __func__);
	/* Vote ON for clocks */
	if (new_diag_cam_dev == NULL) {
		rc = -EINVAL;
		pr_err("%s: clock structure uninitialised %d\n", __func__,
			rc);
		return rc;
	}

	rc = msm_camera_enable_vreg(&new_diag_cam_dev->pdev->dev,
		new_diag_cam_dev->diag_cam_vreg,
		new_diag_cam_dev->regulator_count,
		NULL,
		0,
		&new_diag_cam_dev->diag_cam_reg_ptr[0], 1);
	if (rc < 0)
		pr_err("%s:%d diag_cam enable_vreg failed\n", __func__,
		__LINE__);

	rc = msm_camera_clk_enable(&new_diag_cam_dev->pdev->dev,
		new_diag_cam_dev->diag_cam_clk_info,
		new_diag_cam_dev->diag_cam_clk,
		new_diag_cam_dev->num_clk, true);

	if (rc < 0) {
		pr_err("%s: clk enable failed %d\n", __func__, rc);
		rc = 0;
		return rc;
	}
	pr_debug("Turned ON camera clocks\n");
	return 0;

}

int msm_ais_disable_allclocks(void)
{
	int rc = 0;

	CDBG("%s:\n", __func__);
	/* Vote OFF for clocks */
	if (new_diag_cam_dev == NULL) {
		rc = -EINVAL;
		pr_err("%s: clock structure uninitialised %d\n", __func__,
			rc);
		return rc;
	}

	if ((new_diag_cam_dev->pdev == NULL) ||
		(new_diag_cam_dev->diag_cam_clk_info == NULL) ||
		(new_diag_cam_dev->diag_cam_clk == NULL) ||
		(new_diag_cam_dev->num_clk == 0)) {
		rc = -EINVAL;
		pr_err("%s: Clock details uninitialised %d\n", __func__,
			rc);
		return rc;
	}

	rc = msm_camera_clk_enable(&new_diag_cam_dev->pdev->dev,
		new_diag_cam_dev->diag_cam_clk_info,
		new_diag_cam_dev->diag_cam_clk,
		new_diag_cam_dev->num_clk, false);
	if (rc < 0) {
		pr_err("%s: clk disable failed %d\n", __func__, rc);
		return rc;
	}

	rc = msm_camera_enable_vreg(&new_diag_cam_dev->pdev->dev,
		new_diag_cam_dev->diag_cam_vreg,
		new_diag_cam_dev->regulator_count,
		NULL,
		0,
		&new_diag_cam_dev->diag_cam_reg_ptr[0], 0);
	if (rc < 0)
		pr_err("%s:%d diag_cam disable_vreg failed\n", __func__,
		__LINE__);

	pr_debug("Turned OFF camera clocks\n");
	return 0;
}

int msm_diag_camera_get_vreginfo_list(
		struct msm_ais_diag_regulator_info_list_t *p_vreglist)
{
	int rc = 0;
	uint32_t i = 0;
	uint32_t len = 0;
	uint32_t len1 = 0;
	struct regulator *vreg = NULL;
	char *vreg_name_inuser = NULL;

	p_vreglist->regulator_num = new_diag_cam_dev->regulator_count;

	pr_debug("ais diag regulator_count %u\n",
			new_diag_cam_dev->regulator_count);

	for (; i < p_vreglist->regulator_num ; ++i) {
		vreg = new_diag_cam_dev->diag_cam_reg_ptr[i];
		p_vreglist->infolist[i].enable =
				regulator_is_enabled(vreg);
		len = strlen(new_diag_cam_dev->diag_cam_vreg[i].reg_name);
		len1 = sizeof(p_vreglist->infolist[i].regulatorname);
		len = (len >= len1) ? len1 : (len+1);
		vreg_name_inuser =
				p_vreglist->infolist[i].regulatorname;
		if (copy_to_user((void __user *)vreg_name_inuser,
			(void *)new_diag_cam_dev->diag_cam_vreg[i].reg_name,
			len)) {
			rc = -EFAULT;
			pr_err("%s copy_to_user fail\n", __func__);
			break;
		}
	}

	pr_debug("msm_diag_camera_get_vreginfo_list exit\n");
	return rc;
}

static int msm_diag_cam_probe(struct platform_device *pdev)
{
	int rc = 0;

	CDBG("%s: pdev %pK device id = %d\n", __func__, pdev, pdev->id);

	new_diag_cam_dev = kzalloc(sizeof(struct diag_cam_device),
		GFP_KERNEL);
	if (!new_diag_cam_dev)
		return -ENOMEM;

	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	rc = msm_camera_get_clk_info(pdev,
				&new_diag_cam_dev->diag_cam_clk_info,
				&new_diag_cam_dev->diag_cam_clk,
				&new_diag_cam_dev->num_clk);
	if (rc < 0) {
		pr_err("%s: msm_diag_cam_get_clk_info() failed", __func__);
		kfree(new_diag_cam_dev);
		return -EFAULT;
	}

	new_diag_cam_dev->ref_count = 0;
	new_diag_cam_dev->pdev = pdev;

	rc = msm_camera_get_dt_vreg_data(
		new_diag_cam_dev->pdev->dev.of_node,
		&(new_diag_cam_dev->diag_cam_vreg),
		&(new_diag_cam_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: msm_camera_get_dt_vreg_data fail\n", __func__);
		rc = -EFAULT;
		goto diag_cam_release_mem;
	}

	if ((new_diag_cam_dev->regulator_count < 0) ||
		(new_diag_cam_dev->regulator_count > MAX_REGULATOR)) {
		pr_err("%s: invalid reg count = %d, max is %d\n", __func__,
			new_diag_cam_dev->regulator_count, MAX_REGULATOR);
		rc = -EFAULT;
		goto diag_cam_invalid_vreg_data;
	}

	rc = msm_camera_config_vreg(&new_diag_cam_dev->pdev->dev,
		new_diag_cam_dev->diag_cam_vreg,
		new_diag_cam_dev->regulator_count,
		NULL,
		0,
		&new_diag_cam_dev->diag_cam_reg_ptr[0], 1);
	if (rc < 0)
		pr_err("%s:%d diag_cam config_vreg failed\n", __func__,
			__LINE__);

	platform_set_drvdata(pdev, new_diag_cam_dev);

	return 0;

diag_cam_invalid_vreg_data:
	kfree(new_diag_cam_dev->diag_cam_vreg);
diag_cam_release_mem:
	kfree(new_diag_cam_dev);
	new_diag_cam_dev = NULL;
	return rc;
}

static int msm_diag_cam_exit(struct platform_device *pdev)
{
	return 0;
}

static int __init msm_diag_cam_init_module(void)
{
	return platform_driver_register(&msm_diag_camera_driver);
}

static void __exit msm_diag_cam_exit_module(void)
{
	kfree(new_diag_cam_dev);
	platform_driver_unregister(&msm_diag_camera_driver);
}

static const struct of_device_id msm_diag_camera_match_table[] = {
	{ .compatible = "qcom,diag-cam" },
	{},
};

static struct platform_driver msm_diag_camera_driver = {
	.probe = msm_diag_cam_probe,
	.remove = msm_diag_cam_exit,
	.driver = {
		.name = MSM_DIAG_CAM_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_diag_camera_match_table,
	},
};

MODULE_DEVICE_TABLE(of, msm_diag_camera_match_table);

module_init(msm_diag_cam_init_module);
module_exit(msm_diag_cam_exit_module);
MODULE_DESCRIPTION("MSM diag camera driver");
MODULE_LICENSE("GPL v2");
