// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <media/cam_defs.h>
#include <media/cam_jpeg.h>

#include "jpeg_dma_soc.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

int cam_jpeg_dma_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t jpeg_dma_irq_handler, void *irq_data)
{
	struct cam_jpeg_dma_soc_private  *soc_private;
	struct platform_device *pdev = NULL;
	int num_pid = 0, i = 0;
	int rc;

	soc_private = kzalloc(sizeof(struct cam_jpeg_dma_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		CAM_DBG(CAM_JPEG, "Error! soc_private Alloc Failed");
		return -ENOMEM;
	}
	soc_info->soc_private = soc_private;
	pdev = soc_info->pdev;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc)
		return rc;

	rc = cam_soc_util_request_platform_resource(soc_info,
		jpeg_dma_irq_handler,
		irq_data);
	if (rc)
		CAM_ERR(CAM_JPEG, "init soc failed %d", rc);

	soc_private->num_pid = 0;
	soc_private->rd_mid = UINT_MAX;
	soc_private->wr_mid = UINT_MAX;

	num_pid = of_property_count_u32_elems(pdev->dev.of_node, "cam_hw_pid");
	CAM_DBG(CAM_JPEG, "jpeg:%d pid count %d", soc_info->index, num_pid);

	if (num_pid <= 0  || num_pid > CAM_JPEG_HW_MAX_NUM_PID)
		goto end;

	soc_private->num_pid  = num_pid;
	for (i = 0; i < num_pid; i++)
		of_property_read_u32_index(pdev->dev.of_node, "cam_hw_pid", i,
		&soc_private->pid[i]);

	of_property_read_u32(pdev->dev.of_node,
		"cam_hw_rd_mid", &soc_private->rd_mid);

	of_property_read_u32(pdev->dev.of_node,
		"cam_hw_wr_mid", &soc_private->wr_mid);

end:
	return rc;
}

int cam_jpeg_dma_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc)
		CAM_ERR(CAM_JPEG, "enable platform failed %d", rc);

	return rc;
}

int cam_jpeg_dma_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		CAM_ERR(CAM_JPEG, "disable platform failed %d", rc);

	return rc;
}
