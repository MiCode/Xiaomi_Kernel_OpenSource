/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include "ais_vfe170.h"
#include "ais_vfe175.h"
#include "ais_vfe175_130.h"
#include "ais_vfe_lite17x.h"
#include "ais_vfe_hw_intf.h"
#include "ais_vfe_core.h"
#include "ais_vfe_dev.h"

static const struct of_device_id ais_vfe_dt_match[] = {
	{
		.compatible = "qcom,ais-vfe170",
		.data = &ais_vfe170_hw_info,
	},
	{
		.compatible = "qcom,ais-vfe175",
		.data = &ais_vfe175_hw_info,
	},
	{
		.compatible = "qcom,ais-vfe175_130",
		.data = &ais_vfe175_130_hw_info,
	},
	{
		.compatible = "qcom,ais-vfe-lite170",
		.data = &ais_vfe_lite17x_hw_info,
	},
	{
		.compatible = "qcom,ais-vfe-lite175",
		.data = &ais_vfe_lite17x_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ais_vfe_dt_match);

static struct platform_driver ais_vfe_driver = {
	.probe = ais_vfe_probe,
	.remove = ais_vfe_remove,
	.driver = {
		.name = "ais_vfe17x",
		.owner = THIS_MODULE,
		.of_match_table = ais_vfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init ais_vfe_init_module(void)
{
	return platform_driver_register(&ais_vfe_driver);
}

static void __exit ais_vfe_exit_module(void)
{
	platform_driver_unregister(&ais_vfe_driver);
}

module_init(ais_vfe_init_module);
module_exit(ais_vfe_exit_module);
MODULE_DESCRIPTION("AIS VFE17X driver");
MODULE_LICENSE("GPL v2");
