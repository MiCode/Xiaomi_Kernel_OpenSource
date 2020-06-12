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
#include <linux/slab.h>
#include "cam_ife_csid_soc.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

static int cam_ife_csid_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	struct device_node *of_node = NULL;
	struct csid_device_soc_info *csid_soc_info = NULL;
	int rc = 0;

	of_node = soc_info->pdev->dev.of_node;
	csid_soc_info = (struct csid_device_soc_info *)soc_info->soc_private;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc)
		return rc;

	return rc;
}

static int cam_ife_csid_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler,
	void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_request_platform_resource(soc_info, csid_irq_handler,
		irq_data);
	if (rc)
		return rc;

	return rc;
}

int cam_ife_csid_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler, void *irq_data)
{
	int rc = 0;
	struct cam_cpas_register_params   cpas_register_param;
	struct cam_csid_soc_private      *soc_private;

	soc_private = kzalloc(sizeof(struct cam_csid_soc_private), GFP_KERNEL);
	if (!soc_private)
		return -ENOMEM;

	soc_info->soc_private = soc_private;

	rc = cam_ife_csid_get_dt_properties(soc_info);
	if (rc < 0)
		return rc;

	/* Need to see if we want post process the clock list */

	rc = cam_ife_csid_request_platform_resource(soc_info, csid_irq_handler,
		irq_data);
	if (rc < 0) {
		CAM_ERR(CAM_ISP,
			"Error Request platform resources failed rc=%d", rc);
		goto free_soc_private;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier, "csid",
		CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = soc_info->dev;
	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		CAM_ERR(CAM_ISP, "CPAS registration failed rc=%d", rc);
		goto release_soc;
	} else {
		soc_private->cpas_handle = cpas_register_param.client_handle;
	}

	return rc;

release_soc:
	cam_soc_util_release_platform_resource(soc_info);
free_soc_private:
	kfree(soc_private);

	return rc;
}

int cam_ife_csid_deinit_soc_resources(
	struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -ENODEV;
	}

	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_ISP, "CPAS unregistration failed rc=%d", rc);

	rc = cam_soc_util_release_platform_resource(soc_info);

	return rc;
}

int cam_ife_csid_enable_soc_resources(
	struct cam_hw_soc_info *soc_info, enum cam_vote_level clk_level)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;

	soc_private = soc_info->soc_private;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		clk_level, true);
	if (rc) {
		CAM_ERR(CAM_ISP, "enable platform failed");
		goto end;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.compressed_bw_ab = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.uncompressed_bw = CAM_CPAS_DEFAULT_AXI_BW;

	CAM_DBG(CAM_ISP, "csid vote compressed_bw:%lld uncompressed_bw:%lld",
		axi_vote.compressed_bw, axi_vote.uncompressed_bw);

	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error CPAS start failed");
		rc = -EFAULT;
		goto disable_platform_resource;
	}
	goto end;

disable_platform_resource:
	if (cam_soc_util_disable_platform_resource(soc_info, true, true))
		CAM_ERR(CAM_ISP, "Disable platform resource failed");

end:
	return rc;
}

int cam_ife_csid_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid params");
		return -EINVAL;
	}
	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		CAM_ERR(CAM_ISP, "Disable platform failed");

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error CPAS stop failed rc=%d", rc);
		return rc;
	}

	return rc;
}

int cam_ife_csid_enable_ife_force_clock_on(struct cam_hw_soc_info  *soc_info,
	uint32_t cpas_ife_base_offset)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;
	uint32_t                           cpass_ife_force_clk_offset;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid params");
		return -EINVAL;
	}

	soc_private = soc_info->soc_private;
	cpass_ife_force_clk_offset =
		cpas_ife_base_offset + (0x4 * soc_info->index);
	rc = cam_cpas_reg_write(soc_private->cpas_handle, CAM_CPAS_REG_CPASTOP,
		cpass_ife_force_clk_offset, 1, 1);

	if (rc)
		CAM_ERR(CAM_ISP, "CPASS set IFE:%d Force clock On failed",
			soc_info->index);
	else
		CAM_DBG(CAM_ISP, "CPASS set IFE:%d Force clock On",
		soc_info->index);

	return rc;
}

int cam_ife_csid_disable_ife_force_clock_on(struct cam_hw_soc_info *soc_info,
	uint32_t cpas_ife_base_offset)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;
	uint32_t                           cpass_ife_force_clk_offset;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid params");
		return -EINVAL;
	}

	soc_private = soc_info->soc_private;
	cpass_ife_force_clk_offset =
		cpas_ife_base_offset + (0x4 * soc_info->index);
	rc = cam_cpas_reg_write(soc_private->cpas_handle, CAM_CPAS_REG_CPASTOP,
		cpass_ife_force_clk_offset,  1, 0);

	if (rc)
		CAM_ERR(CAM_ISP, "CPASS set IFE:%d Force clock Off failed",
			soc_info->index);
	else
		CAM_DBG(CAM_ISP, "CPASS set IFE:%d Force clock off",
		soc_info->index);

	return rc;
}
