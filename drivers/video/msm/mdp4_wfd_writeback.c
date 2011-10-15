/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

static int writeback_on(struct platform_device *pdev)
{
	return 0;
}
static int writeback_off(struct platform_device *pdev)
{
	return 0;
}
static int writeback_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct platform_device *mdp_dev = NULL;
	struct msm_fb_panel_data *pdata = NULL;
	int rc = 0;

	WRITEBACK_MSG_ERR("Inside writeback_probe\n");
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mdp_dev = platform_device_alloc("mdp", pdev->id);
	if (!mdp_dev)
		return -ENOMEM;
	/*
	 * link to the latest pdev
	 */
	mfd->pdev = mdp_dev;
	mfd->dest = DISPLAY_LCD;

	if (platform_device_add_data
			(mdp_dev, pdev->dev.platform_data,
			 sizeof(struct msm_fb_panel_data))) {
		pr_err("writeback_probe: "
			"platform_device_add_data failed!\n");
		platform_device_put(mdp_dev);
		return -ENOMEM;
	}
	pdata = (struct msm_fb_panel_data *)mdp_dev->dev.platform_data;
	pdata->on = writeback_on;
	pdata->off = writeback_off;
	pdata->next = pdev;

	/*
	 * get/set panel specific fb info
	 */
	mfd->panel_info = pdata->panel_info;

	mfd->fb_imgType = MDP_RGB_565;

	platform_set_drvdata(mdp_dev, mfd);

	rc = platform_device_add(mdp_dev);
	if (rc) {
		WRITEBACK_MSG_ERR("failed to add device");
		platform_device_put(mdp_dev);
		return rc;
	}
	return rc;
}

static struct platform_driver writeback_driver = {
	.probe = writeback_probe,
	.driver = {
		.name = "writeback",
	},
};

static int __init writeback_driver_init(void)
{
	int rc = 0;
	WRITEBACK_MSG_ERR("Inside writeback_driver_init\n");
	rc = platform_driver_register(&writeback_driver);
	return rc;
}

module_init(writeback_driver_init);
