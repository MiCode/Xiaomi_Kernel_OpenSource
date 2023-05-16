// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_cpas_api.h"
#include "cam_custom_sub_mod_soc.h"
#include "cam_debug_util.h"

int cam_custom_hw_sub_mod_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *irq_data)
{
	int                               rc = 0;
	struct cam_custom_hw_soc_private *soc_private = NULL;
	struct cam_cpas_register_params   cpas_register_param;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CUSTOM,
			"Error! Get DT properties failed rc=%d", rc);
		/* For Test Purposes */
		return 0;
	}

	soc_private = kzalloc(sizeof(struct cam_custom_hw_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		CAM_DBG(CAM_CUSTOM, "Error! soc_private Alloc Failed");
		return -ENOMEM;
	}
	soc_info->soc_private = soc_private;

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler,
		irq_data);
	if (rc < 0) {
		CAM_ERR(CAM_CUSTOM,
			"Error! Request platform resources failed rc=%d", rc);
		return rc;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));

	strlcpy(cpas_register_param.identifier, "custom",
		CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = soc_info->dev;
	cpas_register_param.cam_cpas_client_cb = NULL;
	cpas_register_param.userdata = soc_info;

	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc < 0)
		goto release_soc;

	soc_private->cpas_handle =
		cpas_register_param.client_handle;

	return rc;

release_soc:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}

int cam_custom_hw_sub_mod_deinit_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int                               rc = 0;
	struct cam_custom_hw_soc_private *soc_private = NULL;

	if (!soc_info) {
		CAM_ERR(CAM_CUSTOM, "Error! soc_info NULL");
		return -ENODEV;
	}

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_CUSTOM, "Error! soc_private NULL");
		return -ENODEV;
	}
	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "CPAS0 unregistration failed rc=%d", rc);

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc < 0)
		CAM_ERR(CAM_CUSTOM,
			"Error! Release platform resources failed rc=%d", rc);

	kfree(soc_private);

	return rc;
}

int cam_custom_hw_sub_mod_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int                               rc = 0;
	struct cam_custom_hw_soc_private *soc_private = soc_info->soc_private;
	struct cam_ahb_vote               ahb_vote;
	struct cam_axi_vote axi_vote =    {0};

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 2;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_READ;
	axi_vote.axi_path[0].camnoc_bw = 7200000;
	axi_vote.axi_path[0].mnoc_ab_bw = 7200000;
	axi_vote.axi_path[0].mnoc_ib_bw = 7200000;
	axi_vote.axi_path[1].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[1].transac_type = CAM_AXI_TRANSACTION_WRITE;
	axi_vote.axi_path[1].camnoc_bw = 512000000;
	axi_vote.axi_path[1].mnoc_ab_bw = 512000000;
	axi_vote.axi_path[1].mnoc_ib_bw = 512000000;

	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Error! CPAS0 start failed rc=%d", rc);
		rc = -EFAULT;
		goto end;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_TURBO_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Error! enable platform failed rc=%d", rc);
		goto stop_cpas;
	}

	return 0;

stop_cpas:
	cam_cpas_stop(soc_private->cpas_handle);
end:
	return rc;
}

int cam_custom_hw_sub_mod_disable_soc_resources(
	struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_custom_hw_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_CUSTOM, "Error! Invalid params");
		rc = -EINVAL;
		return rc;
	}
	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Disable platform failed rc=%d", rc);
		return rc;
	}

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Error! CPAS stop failed rc=%d", rc);
		return rc;
	}

	return rc;
}
