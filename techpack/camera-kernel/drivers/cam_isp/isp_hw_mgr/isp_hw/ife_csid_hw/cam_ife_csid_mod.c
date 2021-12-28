// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>
#include "cam_ife_csid_dev.h"
#include "camera_main.h"
#include "cam_ife_csid_common.h"
#include "cam_ife_csid_hw_ver1.h"
#include "cam_ife_csid_hw_ver2.h"
#include "cam_ife_csid170.h"
#include "cam_ife_csid170_200.h"
#include "cam_ife_csid175.h"
#include "cam_ife_csid175_200.h"
#include "cam_ife_csid480.h"
#include "cam_ife_csid570.h"
#include "cam_ife_csid580.h"
#include "cam_ife_csid680.h"
#include "cam_ife_csid780.h"

#define CAM_CSID_DRV_NAME                    "csid"

static struct cam_ife_csid_core_info cam_ife_csid170_hw_info = {
	.csid_reg = &cam_ife_csid_170_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid170_200_hw_info = {
	.csid_reg = &cam_ife_csid_170_200_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid175_hw_info = {
	.csid_reg = &cam_ife_csid_175_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid175_200_hw_info = {
	.csid_reg = &cam_ife_csid_175_200_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid165_204_hw_info = {
	.csid_reg = &cam_ife_csid_175_200_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid480_hw_info = {
	.csid_reg = &cam_ife_csid_480_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid570_hw_info = {
	.csid_reg = &cam_ife_csid_570_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid580_hw_info = {
	.csid_reg = &cam_ife_csid_580_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static struct cam_ife_csid_core_info cam_ife_csid680_hw_info = {
	.csid_reg = &cam_ife_csid_680_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_2_0,
};

static struct cam_ife_csid_core_info cam_ife_csid780_hw_info = {
	.csid_reg = &cam_ife_csid_780_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_2_0,
};

static const struct of_device_id cam_ife_csid_dt_match[] = {

	{
		.compatible = "qcom,csid170",
		.data = &cam_ife_csid170_hw_info,
	},
	{
		.compatible = "qcom,csid170_200",
		.data = &cam_ife_csid170_200_hw_info,
	},
	{
		.compatible = "qcom,csid175",
		.data = &cam_ife_csid175_hw_info,
	},
	{
		.compatible = "qcom,csid175_200",
		.data = &cam_ife_csid175_200_hw_info,
	},
	{
		.compatible = "qcom,csid165_204",
		.data = &cam_ife_csid165_204_hw_info,
	},
	{
		.compatible = "qcom,csid480",
		.data = &cam_ife_csid480_hw_info,
	},
	{
		.compatible = "qcom,csid570",
		.data = &cam_ife_csid570_hw_info,
	},
	{
		.compatible = "qcom,csid580",
		.data = &cam_ife_csid580_hw_info,
	},
	{
		.compatible = "qcom,csid680",
		.data = &cam_ife_csid680_hw_info,
	},
	{
		.compatible = "qcom,csid780",
		.data = &cam_ife_csid780_hw_info,
	},
	{},
};

MODULE_DEVICE_TABLE(of, cam_ife_csid_dt_match);

struct platform_driver cam_ife_csid_driver = {
	.probe = cam_ife_csid_probe,
	.remove = cam_ife_csid_remove,
	.driver = {
		.name = CAM_CSID_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_ife_csid_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_ife_csid_init_module(void)
{
	return platform_driver_register(&cam_ife_csid_driver);
}

void cam_ife_csid_exit_module(void)
{
	platform_driver_unregister(&cam_ife_csid_driver);
}

MODULE_DESCRIPTION("CAM IFE_CSID driver");
MODULE_LICENSE("GPL v2");
