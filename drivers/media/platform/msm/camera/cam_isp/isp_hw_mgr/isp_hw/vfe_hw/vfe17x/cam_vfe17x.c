/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include <linux/module.h>
#include "cam_vfe170.h"
#include "cam_vfe175.h"
#include "cam_vfe_lite17x.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_core.h"
#include "cam_vfe_dev.h"

static const struct of_device_id cam_vfe_dt_match[] = {
	{
		.compatible = "qcom,vfe170",
		.data = &cam_vfe170_hw_info,
	},
	{
		.compatible = "qcom,vfe175",
		.data = &cam_vfe175_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite170",
		.data = &cam_vfe_lite17x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite175",
		.data = &cam_vfe_lite17x_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_vfe_dt_match);

static struct platform_driver cam_vfe_driver = {
	.probe = cam_vfe_probe,
	.remove = cam_vfe_remove,
	.driver = {
		.name = "cam_vfe17x",
		.owner = THIS_MODULE,
		.of_match_table = cam_vfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_vfe_init_module(void)
{
	return platform_driver_register(&cam_vfe_driver);
}

static void __exit cam_vfe_exit_module(void)
{
	platform_driver_unregister(&cam_vfe_driver);
}

module_init(cam_vfe_init_module);
module_exit(cam_vfe_exit_module);
MODULE_DESCRIPTION("CAM VFE17X driver");
MODULE_LICENSE("GPL v2");
