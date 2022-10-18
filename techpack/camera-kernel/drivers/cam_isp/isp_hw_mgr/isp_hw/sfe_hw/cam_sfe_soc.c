// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_cpas_api.h"
#include "cam_sfe_soc.h"
#include "cam_debug_util.h"

static int cam_sfe_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	struct cam_sfe_soc_private       *soc_private = soc_info->soc_private;
	struct platform_device           *pdev = soc_info->pdev;
	uint32_t                          num_pid = 0;
	int                               i, rc = 0;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_SFE, "Error get DT properties failed rc=%d", rc);
		goto end;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "rt-wrapper-base",
		&soc_private->rt_wrapper_base);
	if (rc) {
		soc_private->rt_wrapper_base = 0;
		CAM_DBG(CAM_ISP, "rc: %d Error reading rt_wrapper_base for core_idx: %u",
			rc, soc_info->index);
		rc = 0;
	}

	soc_private->num_pid = 0;
	num_pid = of_property_count_u32_elems(pdev->dev.of_node, "cam_hw_pid");
	CAM_DBG(CAM_SFE, "sfe:%d pid count %d", soc_info->index, num_pid);

	if (num_pid <= 0  || num_pid > CAM_ISP_HW_MAX_PID_VAL)
		goto end;

	for (i = 0; i < num_pid; i++)
		of_property_read_u32_index(pdev->dev.of_node, "cam_hw_pid", i,
			&soc_private->pid[i]);

	soc_private->num_pid = num_pid;
end:
	return rc;
}

static int cam_sfe_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler_func, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler_func,
		irq_data);
	if (rc)
		CAM_ERR(CAM_SFE,
			"Error Request platform resource failed rc=%d", rc);

	return rc;
}

static int cam_sfe_release_platform_resource(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc)
		CAM_ERR(CAM_SFE,
			"Error Release platform resource failed rc=%d", rc);

	return rc;
}

int cam_sfe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler_func, void *irq_data)
{
	int rc = 0;
	struct cam_sfe_soc_private       *soc_private;
	struct cam_cpas_register_params   cpas_register_param;

	soc_private = kzalloc(sizeof(struct cam_sfe_soc_private),
		GFP_KERNEL);
	if (!soc_private)
		return -ENOMEM;

	soc_info->soc_private = soc_private;
	rc = cam_cpas_get_cpas_hw_version(&soc_private->cpas_version);
	if (rc) {
		CAM_ERR(CAM_SFE, "Error! Invalid cpas version rc=%d", rc);
		goto free_soc_private;
	}
	soc_info->hw_version = soc_private->cpas_version;

	rc = cam_sfe_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SFE, "Error Get DT properties failed rc=%d", rc);
		goto free_soc_private;
	}

	rc = cam_sfe_request_platform_resource(soc_info,
		irq_handler_func, irq_data);
	if (rc < 0) {
		CAM_ERR(CAM_SFE,
			"Error Request platform resources failed rc=%d", rc);
		goto free_soc_private;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier, "sfe",
		CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = soc_info->dev;
	cpas_register_param.cam_cpas_client_cb = NULL;
	cpas_register_param.userdata = soc_info;
	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		CAM_ERR(CAM_SFE, "CPAS registration failed rc=%d", rc);
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

int cam_sfe_deinit_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_sfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_SFE, "Error soc_info NULL");
		return -ENODEV;
	}

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_SFE, "Error! soc_private NULL");
		return -ENODEV;
	}

	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_SFE, "CPAS unregistration failed rc=%d", rc);

	rc = cam_sfe_release_platform_resource(soc_info);
	if (rc < 0)
		CAM_ERR(CAM_SFE,
			"Error Release platform resources failed rc=%d", rc);

	kfree(soc_private);
	return rc;
}

int cam_sfe_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int                               rc = 0;
	struct cam_sfe_soc_private       *soc_private;
	struct cam_ahb_vote               ahb_vote;
	struct cam_axi_vote               axi_vote = {0};

	if (!soc_info) {
		CAM_ERR(CAM_SFE, "Error! Invalid params");
		rc = -EINVAL;
		goto end;
	}
	soc_private = soc_info->soc_private;

	ahb_vote.type       = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_SFE_NRDI;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_WRITE;
	axi_vote.axi_path[0].camnoc_bw = CAM_CPAS_DEFAULT_RT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ab_bw = CAM_CPAS_DEFAULT_RT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ib_bw = CAM_CPAS_DEFAULT_RT_AXI_BW;

	rc = cam_cpas_start(soc_private->cpas_handle,
			&ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_SFE, "CPAS start failed rc=%d", rc);
		rc = -EFAULT;
		goto end;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_LOWSVS_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_SFE, "Enable platform failed rc=%d", rc);
		goto stop_cpas;
	}

	return rc;

stop_cpas:
	cam_cpas_stop(soc_private->cpas_handle);
end:
	return rc;
}

int cam_sfe_soc_enable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name)
{
	return -EPERM;
}

int cam_sfe_soc_disable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name)
{
	return -EPERM;
}

int cam_sfe_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_sfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_SFE, "Invalid params");
		rc = -EINVAL;
		return rc;
	}

	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		CAM_ERR(CAM_SFE, "Disable platform failed rc=%d", rc);

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_SFE, "CPAS stop failed rc=%d", rc);

	return rc;
}
