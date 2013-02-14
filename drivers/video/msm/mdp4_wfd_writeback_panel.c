/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include "mdp4_wfd_writeback_util.h"
#include "msm_fb.h"

static int __devinit writeback_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	if (pdev->id == 0)
		return 0;

	if (!msm_fb_add_device(pdev)) {
		WRITEBACK_MSG_ERR("Failed to add fd device\n");
		rc = -ENOMEM;
	}
	return rc;
}
static struct msm_fb_panel_data writeback_msm_panel_data = {
	.panel_info = {
		.type = WRITEBACK_PANEL,
		.xres = 1280,
		.yres = 720,
		.pdest = DISPLAY_3,
		.wait_cycle = 0,
		.bpp = 24,
		.fb_num = 1,
		.clk_rate = 74250000,
	},
};

static struct platform_device writeback_panel_device = {
	.name = "writeback_panel",
	.id = 1,
	.dev.platform_data = &writeback_msm_panel_data,
};
static struct platform_driver writeback_panel_driver = {
	.probe = writeback_panel_probe,
	.driver = {
		.name = "writeback_panel"
	}
};

static int __init writeback_panel_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&writeback_panel_driver);
	if (rc) {
		WRITEBACK_MSG_ERR("Failed to register platform driver\n");
		goto fail_driver_registration;
	}
	rc = platform_device_register(&writeback_panel_device);
	if (rc) {
		WRITEBACK_MSG_ERR("Failed to register "
				"writeback_panel_device\n");
		goto fail_device_registration;
	}
	return rc;
fail_device_registration:
	platform_driver_unregister(&writeback_panel_driver);
fail_driver_registration:
	return rc;
}

module_init(writeback_panel_init);
