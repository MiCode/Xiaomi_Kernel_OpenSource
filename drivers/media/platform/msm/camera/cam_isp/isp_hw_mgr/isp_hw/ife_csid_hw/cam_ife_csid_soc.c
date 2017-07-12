/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

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
		pr_err("Error Request platform resources failed rc=%d\n", rc);
		goto free_soc_private;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier, "csid",
		CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = &soc_info->pdev->dev;
	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		pr_err("CPAS registration failed rc=%d\n", rc);
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
		pr_err("Error soc_private NULL\n");
		return -ENODEV;
	}

	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		pr_err("CPAS unregistration failed rc=%d\n", rc);

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc < 0)
		return rc;

	return rc;
}

int cam_ife_csid_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;

	soc_private = soc_info->soc_private;

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = 640000000;
	axi_vote.uncompressed_bw = 640000000;

	CDBG("%s:csid vote compressed_bw:%lld uncompressed_bw:%lld\n",
		__func__, axi_vote.compressed_bw, axi_vote.uncompressed_bw);

	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		pr_err("Error CPAS start failed\n");
		rc = -EFAULT;
		goto end;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_TURBO_VOTE, true);
	if (rc) {
		pr_err("%s: enable platform failed\n", __func__);
		goto stop_cpas;
	}

	return rc;

stop_cpas:
	cam_cpas_stop(soc_private->cpas_handle);
end:
	return rc;
}

int cam_ife_csid_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct cam_csid_soc_private       *soc_private;

	if (!soc_info) {
		pr_err("Error Invalid params\n");
		return -EINVAL;
	}
	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		pr_err("%s: Disable platform failed\n", __func__);

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc) {
		pr_err("Error CPAS stop failed rc=%d\n", rc);
		return rc;
	}

	return rc;
}

