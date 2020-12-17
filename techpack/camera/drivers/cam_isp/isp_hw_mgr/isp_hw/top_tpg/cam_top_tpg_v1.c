// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */


#include <linux/module.h>
#include "cam_top_tpg_core.h"
#include "cam_top_tpg_v1.h"
#include "cam_top_tpg_dev.h"

#define CAM_TOP_TPG_DRV_NAME                    "tpg_v1"
#define CAM_TOP_TPG_VERSION_V1                  0x10000000

static struct cam_top_tpg_hw_info cam_top_tpg_v1_hw_info = {
	.tpg_reg = &cam_top_tpg_v1_reg_offset,
	.hw_dts_version = CAM_TOP_TPG_VERSION_V1,
	.csid_max_clk = 426400000,
	.phy_max_clk = 384000000,
};

static const struct of_device_id cam_top_tpg_v1_dt_match[] = {
	{
		.compatible = "qcom,tpgv1",
		.data = &cam_top_tpg_v1_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_top_tpg_v1_dt_match);

static struct platform_driver cam_top_tpg_v1_driver = {
	.probe = cam_top_tpg_probe,
	.remove = cam_top_tpg_remove,
	.driver = {
		.name = CAM_TOP_TPG_DRV_NAME,
		.of_match_table = cam_top_tpg_v1_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_top_tpg_v1_init_module(void)
{
	return platform_driver_register(&cam_top_tpg_v1_driver);
}

static void __exit cam_top_tpg_v1_exit_module(void)
{
	platform_driver_unregister(&cam_top_tpg_v1_driver);
}

module_init(cam_top_tpg_v1_init_module);
module_exit(cam_top_tpg_v1_exit_module);
MODULE_DESCRIPTION("CAM TOP TPG driver");
MODULE_LICENSE("GPL v2");
