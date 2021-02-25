// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "cam_csid_ppi_core.h"
#include "cam_csid_ppi100.h"
#include "cam_csid_ppi_dev.h"

#define CAM_PPI_DRV_NAME                    "ppi_100"
#define CAM_PPI_VERSION_V100                 0x10000000

static struct cam_csid_ppi_hw_info cam_csid_ppi100_hw_info = {
	.ppi_reg = &cam_csid_ppi_100_reg_offset,
};

static const struct of_device_id cam_csid_ppi100_dt_match[] = {
	{
		.compatible = "qcom,ppi100",
		.data = &cam_csid_ppi100_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_csid_ppi100_dt_match);

struct platform_driver cam_csid_ppi100_driver = {
	.probe  = cam_csid_ppi_probe,
	.remove = cam_csid_ppi_remove,
	.driver = {
		.name = CAM_PPI_DRV_NAME,
		.of_match_table = cam_csid_ppi100_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_csid_ppi100_init_module(void)
{
	return platform_driver_register(&cam_csid_ppi100_driver);
}

void cam_csid_ppi100_exit_module(void)
{
	platform_driver_unregister(&cam_csid_ppi100_driver);
}

MODULE_DESCRIPTION("CAM CSID_PPI100 driver");
MODULE_LICENSE("GPL v2");
