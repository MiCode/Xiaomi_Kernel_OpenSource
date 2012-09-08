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
	uint8_t csiphy_code_index, uint8_t csid_core_index,
	struct msm_cam_server_dev *server_dev)
{
	int rc = -ENODEV;

	p_mctl->csic_sdev = server_dev->csic_device[csid_core_index];
	if (!p_mctl->csic_sdev)
		goto out;
	v4l2_set_subdev_hostdata(p_mctl->csic_sdev, p_mctl);

	rc = 0;
	p_mctl->ispif_sdev = NULL;
	return rc;

out:
	p_mctl->ispif_sdev = NULL;
	return rc;
}

