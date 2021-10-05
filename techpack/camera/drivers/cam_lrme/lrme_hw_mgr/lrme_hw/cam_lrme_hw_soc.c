// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_lrme_hw_core.h"
#include "cam_lrme_hw_soc.h"


int cam_lrme_soc_enable_resources(struct cam_hw_info *lrme_hw)
{
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_soc_private *soc_private =
		(struct cam_lrme_soc_private *)soc_info->soc_private;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};
	int rc = 0;

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
	axi_vote.axi_path[1].camnoc_bw = 7200000;
	axi_vote.axi_path[1].mnoc_ab_bw = 7200000;
	axi_vote.axi_path[1].mnoc_ib_bw = 7200000;

	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to start cpas, rc %d", rc);
		return -EFAULT;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true, CAM_SVS_VOTE,
		true);
	if (rc) {
		CAM_ERR(CAM_LRME,
			"Failed to enable platform resource, rc %d", rc);
		goto stop_cpas;
	}

	cam_lrme_set_irq(lrme_hw, CAM_LRME_IRQ_ENABLE);

	return rc;

stop_cpas:
	if (cam_cpas_stop(soc_private->cpas_handle))
		CAM_ERR(CAM_LRME, "Failed to stop cpas");

	return rc;
}

int cam_lrme_soc_disable_resources(struct cam_hw_info *lrme_hw)
{
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_soc_private *soc_private;
	int rc = 0;

	soc_private = soc_info->soc_private;

	cam_lrme_set_irq(lrme_hw, CAM_LRME_IRQ_DISABLE);

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to disable platform resource");
		return rc;
	}
	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_LRME, "Failed to stop cpas");

	return rc;
}

int cam_lrme_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *private_data)
{
	struct cam_lrme_soc_private *soc_private;
	struct cam_cpas_register_params cpas_register_param;
	int rc;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed in get_dt_properties, rc=%d", rc);
		return rc;
	}

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler,
		private_data);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed in request_platform_resource rc=%d",
			rc);
		return rc;
	}

	soc_private = kzalloc(sizeof(struct cam_lrme_soc_private), GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto release_res;
	}
	soc_info->soc_private = soc_private;

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier,
		"lrmecpas", CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = &soc_info->pdev->dev;
	cpas_register_param.userdata = private_data;
	cpas_register_param.cam_cpas_client_cb = NULL;

	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		CAM_ERR(CAM_LRME, "CPAS registration failed");
		goto free_soc_private;
	}
	soc_private->cpas_handle = cpas_register_param.client_handle;
	CAM_DBG(CAM_LRME, "CPAS handle=%d", soc_private->cpas_handle);

	return rc;

free_soc_private:
	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;
release_res:
	cam_soc_util_release_platform_resource(soc_info);

	return rc;
}

int cam_lrme_soc_deinit_resources(struct cam_hw_soc_info *soc_info)
{
	struct cam_lrme_soc_private *soc_private =
		(struct cam_lrme_soc_private *)soc_info->soc_private;
	int rc;

	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_LRME, "Unregister cpas failed, handle=%d, rc=%d",
			soc_private->cpas_handle, rc);

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc)
		CAM_ERR(CAM_LRME, "release platform failed, rc=%d", rc);

	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;

	return rc;
}
