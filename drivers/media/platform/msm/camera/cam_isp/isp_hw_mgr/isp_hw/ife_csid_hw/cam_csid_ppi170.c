/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include "cam_csid_ppi_core.h"
#include "cam_csid_ppi170.h"
#include "cam_csid_ppi_dev.h"

#define CAM_PPI_DRV_NAME                    "ppi_170"
#define CAM_PPI_VERSION_V170                 0x10070000

static struct cam_csid_ppi_hw_info cam_csid_ppi170_hw_info = {
	.ppi_reg = &cam_csid_ppi_170_reg_offset,
};

static const struct of_device_id cam_csid_ppi170_dt_match[] = {
	{
		.compatible = "qcom,ppi170",
		.data = &cam_csid_ppi170_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_csid_ppi170_dt_match);

static struct platform_driver cam_csid_ppi170_driver = {
	.probe  = cam_csid_ppi_probe,
	.remove = cam_csid_ppi_remove,
	.driver = {
		.name = CAM_PPI_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_csid_ppi170_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_csid_ppi170_init_module(void)
{
	return platform_driver_register(&cam_csid_ppi170_driver);
}

static void __exit cam_csid_ppi170_exit_module(void)
{
	platform_driver_unregister(&cam_csid_ppi170_driver);
}

module_init(cam_csid_ppi170_init_module);
MODULE_DESCRIPTION("CAM CSID_PPI170 driver");
MODULE_LICENSE("GPL v2");
