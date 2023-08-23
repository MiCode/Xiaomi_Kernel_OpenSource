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
#include "ais_ife_csid_core.h"
#include "ais_ife_csid170.h"
#include "ais_ife_csid175.h"
#include "ais_ife_csid175_200.h"
#include "ais_ife_csid_dev.h"

#define AIS_CSID_DRV_NAME                    "ais-csid_17x"
#define AIS_CSID_VERSION_V170                 0x10070000
#define AIS_CSID_VERSION_V175                 0x10070050

static struct ais_ife_csid_hw_info ais_ife_csid170_hw_info = {
	.csid_reg = &ais_ife_csid_170_reg_offset,
	.hw_dts_version = AIS_CSID_VERSION_V170,
};

static struct ais_ife_csid_hw_info ais_ife_csid175_hw_info = {
	.csid_reg = &ais_ife_csid_175_reg_offset,
	.hw_dts_version = AIS_CSID_VERSION_V175,
};

static struct ais_ife_csid_hw_info ais_ife_csid175_200_hw_info = {
	.csid_reg = &ais_ife_csid_175_200_reg_offset,
	.hw_dts_version = AIS_CSID_VERSION_V175,
};

static const struct of_device_id ais_ife_csid17x_dt_match[] = {
	{
		.compatible = "qcom,ais-csid170",
		.data = &ais_ife_csid170_hw_info,
	},
	{
		.compatible = "qcom,ais-csid175",
		.data = &ais_ife_csid175_hw_info,
	},
	{
		.compatible = "qcom,ais-csid175_200",
		.data = &ais_ife_csid175_200_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, ais_ife_csid17x_dt_match);

static struct platform_driver ais_ife_csid17x_driver = {
	.probe = ais_ife_csid_probe,
	.remove = ais_ife_csid_remove,
	.driver = {
		.name = AIS_CSID_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ais_ife_csid17x_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init ais_ife_csid17x_init_module(void)
{
	return platform_driver_register(&ais_ife_csid17x_driver);
}

static void __exit ais_ife_csid17x_exit_module(void)
{
	platform_driver_unregister(&ais_ife_csid17x_driver);
}

module_init(ais_ife_csid17x_init_module);
module_exit(ais_ife_csid17x_exit_module);
MODULE_DESCRIPTION("AIS IFE_CSID17X driver");
MODULE_LICENSE("GPL v2");
