/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
	int core_index, struct msm_cam_server_dev *server_dev)
{
	int rc = -ENODEV;

	/* register csiphy subdev */
	p_mctl->csiphy_sdev = server_dev->csiphy_device[core_index];
	if (!p_mctl->csiphy_sdev)
		goto out;
	v4l2_set_subdev_hostdata(p_mctl->csiphy_sdev, p_mctl);

	/* register csid subdev */
	p_mctl->csid_sdev = server_dev->csid_device[core_index];
	if (!p_mctl->csid_sdev)
		goto out;
	v4l2_set_subdev_hostdata(p_mctl->csid_sdev, p_mctl);

	/* register ispif subdev */
	p_mctl->ispif_sdev = server_dev->ispif_device[0];
	if (!p_mctl->ispif_sdev)
		goto out;
	v4l2_set_subdev_hostdata(p_mctl->ispif_sdev, p_mctl);

	rc = 0;
	return rc;
out:
	p_mctl->ispif_sdev = NULL;
	return rc;
}

