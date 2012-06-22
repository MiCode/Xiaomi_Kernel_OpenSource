/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/device.h>
#include "msm.h"
#include "msm_csi_register.h"

int msm_csi_register_subdevs(struct msm_cam_media_controller *p_mctl,
	int core_index,
	int (*msm_mctl_subdev_match_core)(struct device *, void *))
{
	int rc = -ENODEV;
	struct device_driver *driver;
	struct device *dev;

	driver = driver_find(MSM_CSIC_DRV_NAME, &platform_bus_type);
	if (!driver)
		goto out;

	dev = driver_find_device(driver, NULL, (void *)core_index,
			msm_mctl_subdev_match_core);
	if (!dev)
		goto out;

	p_mctl->csic_sdev = dev_get_drvdata(dev);

	rc = 0;
	p_mctl->ispif_sdev = NULL;
	return rc;

out:
	p_mctl->ispif_sdev = NULL;
	return rc;
}

