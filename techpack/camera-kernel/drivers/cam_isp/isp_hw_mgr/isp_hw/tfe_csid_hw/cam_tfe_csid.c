// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */


#include <linux/module.h>
#include "cam_tfe_csid_core.h"
#include "cam_tfe_csid530.h"
#include "cam_tfe_csid640.h"
#include "cam_tfe_csid_dev.h"
#include "camera_main.h"

#define CAM_TFE_CSID_DRV_NAME                    "tfe_csid"

static const struct of_device_id cam_tfe_csid_dt_match[] = {
	{
		.compatible = "qcom,csid530",
		.data = &cam_tfe_csid530_hw_info,
	},
	{
		.compatible = "qcom,csid640",
		.data = &cam_tfe_csid640_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_tfe_csid_dt_match);

struct platform_driver cam_tfe_csid_driver = {
	.probe = cam_tfe_csid_probe,
	.remove = cam_tfe_csid_remove,
	.driver = {
		.name = CAM_TFE_CSID_DRV_NAME,
		.of_match_table = cam_tfe_csid_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_tfe_csid_init_module(void)
{
	return platform_driver_register(&cam_tfe_csid_driver);
}

void cam_tfe_csid_exit_module(void)
{
	platform_driver_unregister(&cam_tfe_csid_driver);
}

MODULE_DESCRIPTION("CAM TFE_CSID driver");
MODULE_LICENSE("GPL v2");
