// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>
#include "camera_main.h"
#include "cam_ife_csid_dev.h"
#include "cam_ife_csid_common.h"
#include "cam_ife_csid_hw_ver1.h"
#include "cam_ife_csid_lite17x.h"
#include "cam_ife_csid_lite480.h"
#include "cam_ife_csid_lite680.h"
#include "cam_ife_csid_lite780.h"

#define CAM_CSID_LITE_DRV_NAME                    "csid_lite"

static struct cam_ife_csid_core_info cam_ife_csid_lite_17x_hw_info = {
	.csid_reg = &cam_ife_csid_lite_17x_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid_lite_480_hw_info = {
	.csid_reg = &cam_ife_csid_lite_480_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid_lite_680_hw_info = {
	.csid_reg = &cam_ife_csid_lite_680_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_2_0,
};

static struct cam_ife_csid_core_info cam_ife_csid_lite_780_hw_info = {
	.csid_reg = &cam_ife_csid_lite_780_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_2_0,
};

static const struct of_device_id cam_ife_csid_lite_dt_match[] = {
	{
		.compatible = "qcom,csid-lite170",
		.data = &cam_ife_csid_lite_17x_hw_info,
	},
	{
		.compatible = "qcom,csid-lite175",
		.data = &cam_ife_csid_lite_17x_hw_info,
	},
	{
		.compatible = "qcom,csid-lite165",
		.data = &cam_ife_csid_lite_17x_hw_info,
	},
	{
		.compatible = "qcom,csid-lite480",
		.data = &cam_ife_csid_lite_480_hw_info,
	},
	{
		.compatible = "qcom,csid-lite570",
		.data = &cam_ife_csid_lite_480_hw_info,
	},
	{
		.compatible = "qcom,csid-lite580",
		.data = &cam_ife_csid_lite_480_hw_info,
	},
	{
		.compatible = "qcom,csid-lite680",
		.data = &cam_ife_csid_lite_680_hw_info,
	},
	{
		.compatible = "qcom,csid-lite780",
		.data = &cam_ife_csid_lite_780_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_ife_csid_lite_dt_match);

struct platform_driver cam_ife_csid_lite_driver = {
	.probe = cam_ife_csid_probe,
	.remove = cam_ife_csid_remove,
	.driver = {
		.name = CAM_CSID_LITE_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_ife_csid_lite_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_ife_csid_lite_init_module(void)
{
	return platform_driver_register(&cam_ife_csid_lite_driver);
}

void cam_ife_csid_lite_exit_module(void)
{
	platform_driver_unregister(&cam_ife_csid_lite_driver);
}

MODULE_DESCRIPTION("CAM IFE_CSID_LITE driver");
MODULE_LICENSE("GPL v2");
