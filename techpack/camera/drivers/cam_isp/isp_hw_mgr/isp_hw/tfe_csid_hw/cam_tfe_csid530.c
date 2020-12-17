// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */


#include <linux/module.h>
#include "cam_tfe_csid_core.h"
#include "cam_tfe_csid530.h"
#include "cam_tfe_csid_dev.h"

#define CAM_TFE_CSID_DRV_NAME                    "csid_530"
#define CAM_TFE_CSID_VERSION_V530                 0x50030000

static struct cam_tfe_csid_hw_info cam_tfe_csid530_hw_info = {
	.csid_reg = &cam_tfe_csid_530_reg_offset,
	.hw_dts_version = CAM_TFE_CSID_VERSION_V530,
};

static const struct of_device_id cam_tfe_csid530_dt_match[] = {
	{
		.compatible = "qcom,csid530",
		.data = &cam_tfe_csid530_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_tfe_csid530_dt_match);

static struct platform_driver cam_tfe_csid530_driver = {
	.probe = cam_tfe_csid_probe,
	.remove = cam_tfe_csid_remove,
	.driver = {
		.name = CAM_TFE_CSID_DRV_NAME,
		.of_match_table = cam_tfe_csid530_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_tfe_csid530_init_module(void)
{
	return platform_driver_register(&cam_tfe_csid530_driver);
}

static void __exit cam_tfe_csid530_exit_module(void)
{
	platform_driver_unregister(&cam_tfe_csid530_driver);
}

module_init(cam_tfe_csid530_init_module);
module_exit(cam_tfe_csid530_exit_module);
MODULE_DESCRIPTION("CAM TFE_CSID530 driver");
MODULE_LICENSE("GPL v2");
