// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_cpas_api.h"
#include "cam_tfe_soc.h"
#include "cam_debug_util.h"

static bool cam_tfe_cpas_cb(uint32_t client_handle, void *userdata,
	struct cam_cpas_irq_data *irq_data)
{
	bool error_handled = false;

	if (!irq_data)
		return error_handled;

	CAM_DBG(CAM_ISP, "CPSS error type=%d ",
		irq_data->irq_type);

	return error_handled;
}

int cam_tfe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t tfe_irq_handler, void *irq_data)
{
	struct cam_tfe_soc_private       *soc_private;
	struct cam_cpas_register_params   cpas_register_param;
	int    rc = 0,  i = 0, num_pid = 0;

	soc_private = kzalloc(sizeof(struct cam_tfe_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		CAM_DBG(CAM_ISP, "Error! soc_private Alloc Failed");
		return -ENOMEM;
	}
	soc_info->soc_private = soc_private;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! get DT properties failed rc=%d", rc);
		goto free_soc_private;
	}

	/* set some default values */
	soc_private->num_pid = 0;

	num_pid = of_property_count_u32_elems(soc_info->pdev->dev.of_node,
		"cam_hw_pid");
	CAM_DBG(CAM_CPAS, "tfe:%d pid count %d", soc_info->index, num_pid);

	if (num_pid <= 0  || num_pid > CAM_ISP_HW_MAX_PID_VAL)
		goto clk_option;

	for (i = 0; i < num_pid; i++) {
		of_property_read_u32_index(soc_info->pdev->dev.of_node,
		"cam_hw_pid", i, &soc_private->pid[i]);
		CAM_INFO(CAM_CPAS, "tfe:%d I:%d pid %d", soc_info->index,
			i, soc_private->pid[i]);
	}

	soc_private->num_pid = num_pid;

clk_option:

	rc = cam_soc_util_get_option_clk_by_name(soc_info,
		CAM_TFE_DSP_CLK_NAME, &soc_private->dsp_clk,
		&soc_private->dsp_clk_index, &soc_private->dsp_clk_rate);
	if (rc)
		CAM_WARN(CAM_ISP, "Option clk get failed with rc %d", rc);

	rc = cam_soc_util_request_platform_resource(soc_info, tfe_irq_handler,
		irq_data);

	if (rc < 0) {
		CAM_ERR(CAM_ISP,
			"Error! Request platform resources failed rc=%d", rc);
		goto free_soc_private;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier, "tfe",
		CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = soc_info->dev;
	cpas_register_param.cam_cpas_client_cb = cam_tfe_cpas_cb;
	cpas_register_param.userdata = soc_info;
	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		CAM_ERR(CAM_ISP, "CPAS registration failed rc=%d", rc);
		goto release_soc;
	}

	soc_private->cpas_handle = cpas_register_param.client_handle;

	return rc;

release_soc:
	cam_soc_util_release_platform_resource(soc_info);
free_soc_private:
	kfree(soc_private);

	return rc;
}

int cam_tfe_deinit_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int                               rc = 0;
	struct cam_tfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error! soc_info NULL");
		return -ENODEV;
	}

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error! soc_private NULL");
		return -ENODEV;
	}
	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_ISP, "CPAS0 unregistration failed rc=%d", rc);

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Error! Release platform resource failed rc=%d", rc);


	rc = cam_soc_util_clk_put(&soc_private->dsp_clk);
	if (rc < 0)
		CAM_ERR(CAM_ISP,
			"Error Put dsp clk failed rc=%d", rc);

	kfree(soc_private);

	return rc;
}

int cam_tfe_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int                               rc = 0;
	struct cam_tfe_soc_private       *soc_private;
	struct cam_ahb_vote               ahb_vote;
	struct cam_axi_vote               axi_vote = {0};

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid params");
		rc = -EINVAL;
		goto end;
	}
	soc_private = soc_info->soc_private;

	ahb_vote.type       = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_IFE_VID;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_WRITE;
	axi_vote.axi_path[0].camnoc_bw = 10640000000L;
	axi_vote.axi_path[0].mnoc_ab_bw = 10640000000L;
	axi_vote.axi_path[0].mnoc_ib_bw = 10640000000L;

	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! CPAS0 start failed rc=%d", rc);
		rc = -EFAULT;
		goto end;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_TURBO_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! enable platform failed rc=%d", rc);
		goto stop_cpas;
	}

	return rc;

stop_cpas:
	cam_cpas_stop(soc_private->cpas_handle);
end:
	return rc;
}

int cam_tfe_soc_enable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name)
{
	int  rc = 0;
	struct cam_tfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid params");
		rc = -EINVAL;
		return rc;
	}
	soc_private = soc_info->soc_private;

	if (strcmp(clk_name, CAM_TFE_DSP_CLK_NAME) == 0) {
		rc = cam_soc_util_clk_enable(soc_private->dsp_clk,
			CAM_TFE_DSP_CLK_NAME, soc_private->dsp_clk_rate);
		if (rc)
			CAM_ERR(CAM_ISP,
			"Error enable dsp clk failed rc=%d", rc);
	}

	return rc;
}

int cam_tfe_soc_disable_clk(struct cam_hw_soc_info *soc_info,
	const char *clk_name)
{
	int  rc = 0;
	struct cam_tfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error Invalid params");
		rc = -EINVAL;
		return rc;
	}
	soc_private = soc_info->soc_private;

	if (strcmp(clk_name, CAM_TFE_DSP_CLK_NAME) == 0) {
		rc = cam_soc_util_clk_disable(soc_private->dsp_clk,
			CAM_TFE_DSP_CLK_NAME);
		if (rc)
			CAM_ERR(CAM_ISP,
			"Error enable dsp clk failed rc=%d", rc);
	}

	return rc;
}


int cam_tfe_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_tfe_soc_private       *soc_private;

	if (!soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid params");
		rc = -EINVAL;
		return rc;
	}
	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_ISP, "Disable platform failed rc=%d", rc);
		return rc;
	}

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! CPAS stop failed rc=%d", rc);
		return rc;
	}

	return rc;
}
