// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "cam_ife_csid_lite17x.h"
#include "cam_ife_csid_lite480.h"
#include "cam_ife_csid_core.h"
#include "cam_ife_csid_dev.h"

#define CAM_CSID_LITE_DRV_NAME                    "csid_lite"

static struct cam_ife_csid_hw_info cam_ife_csid_lite_17x_hw_info = {
	.csid_reg = &cam_ife_csid_lite_17x_reg_offset,
};

static struct cam_ife_csid_hw_info cam_ife_csid_lite_480_hw_info = {
	.csid_reg = &cam_ife_csid_lite_480_reg_offset,
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
		.compatible = "qcom,csid-lite480",
		.data = &cam_ife_csid_lite_480_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_ife_csid_lite_dt_match);

static struct platform_driver cam_ife_csid_lite_driver = {
	.probe = cam_ife_csid_probe,
	.remove = cam_ife_csid_remove,
	.driver = {
		.name = CAM_CSID_LITE_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_ife_csid_lite_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_ife_csid_lite_init_module(void)
{
	return platform_driver_register(&cam_ife_csid_lite_driver);
}

static void __exit cam_ife_csid_lite_exit_module(void)
{
	platform_driver_unregister(&cam_ife_csid_lite_driver);
}

module_init(cam_ife_csid_lite_init_module);
module_exit(cam_ife_csid_lite_exit_module);
MODULE_DESCRIPTION("CAM IFE_CSID_LITE driver");
MODULE_LICENSE("GPL v2");
