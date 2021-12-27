// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <media/cam_defs.h>
#include <media/cam_jpeg.h>

#include "jpeg_enc_soc.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

int cam_jpeg_enc_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t jpeg_enc_irq_handler, void *irq_data)
{
	int rc;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc)
		return rc;

	rc = cam_soc_util_request_platform_resource(soc_info,
		jpeg_enc_irq_handler,
		irq_data);
	if (rc)
		CAM_ERR(CAM_JPEG, "init soc failed %d", rc);

	return rc;
}

int cam_jpeg_enc_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc)
		CAM_ERR(CAM_JPEG, "enable platform failed %d", rc);

	return rc;
}

int cam_jpeg_enc_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		CAM_ERR(CAM_JPEG, "disable platform failed %d", rc);

	return rc;
}
