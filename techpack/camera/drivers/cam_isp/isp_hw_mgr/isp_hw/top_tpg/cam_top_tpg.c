// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "cam_top_tpg101.h"
#include "cam_top_tpg102.h"
#include "cam_top_tpg103.h"
#include "cam_top_tpg_core.h"
#include "cam_top_tpg_dev.h"
#include "camera_main.h"

static const struct of_device_id cam_top_tpg_dt_match[] = {
	{
		.compatible = "qcom,tpg101",
		.data = &cam_top_tpg101_hw_info,
	},
	{
		.compatible = "qcom,tpg102",
		.data = &cam_top_tpg102_hw_info,
	},
	{
		.compatible = "qcom,tpg103",
		.data = &cam_top_tpg103_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_top_tpg_dt_match);

struct platform_driver cam_top_tpg_driver = {
	.probe = cam_top_tpg_probe,
	.remove = cam_top_tpg_remove,
	.driver = {
		.name = "cam_top_tpg",
		.of_match_table = cam_top_tpg_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_top_tpg_init_module(void)
{
	return platform_driver_register(&cam_top_tpg_driver);
}

void cam_top_tpg_exit_module(void)
{
	platform_driver_unregister(&cam_top_tpg_driver);
}

MODULE_DESCRIPTION("CAM TOP TPG driver");
MODULE_LICENSE("GPL v2");
