// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "cam_tfe530.h"
#include "cam_tfe_hw_intf.h"
#include "cam_tfe_core.h"
#include "cam_tfe_dev.h"

static const struct of_device_id cam_tfe_dt_match[] = {
	{
		.compatible = "qcom,tfe530",
		.data = &cam_tfe530,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_tfe_dt_match);

static struct platform_driver cam_tfe_driver = {
	.probe = cam_tfe_probe,
	.remove = cam_tfe_remove,
	.driver = {
		.name = "cam_tfe",
		.of_match_table = cam_tfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_tfe_init_module(void)
{
	return platform_driver_register(&cam_tfe_driver);
}

static void __exit cam_tfe_exit_module(void)
{
	platform_driver_unregister(&cam_tfe_driver);
}

module_init(cam_tfe_init_module);
module_exit(cam_tfe_exit_module);
MODULE_DESCRIPTION("CAM TFE driver");
MODULE_LICENSE("GPL v2");
