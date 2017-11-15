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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_fd_hw_core.h"
#include "cam_fd_hw_soc.h"

static bool cam_fd_hw_util_cpas_callback(uint32_t handle, void *userdata,
	struct cam_cpas_irq_data *irq_data)
{
	if (!irq_data)
		return false;

	CAM_DBG(CAM_FD, "CPAS hdl=%d, udata=%pK, irq_type=%d",
		handle, userdata, irq_data->irq_type);

	return false;
}

static int cam_fd_hw_soc_util_setup_regbase_indices(
	struct cam_hw_soc_info *soc_info)
{
	struct cam_fd_soc_private *soc_private =
		(struct cam_fd_soc_private *)soc_info->soc_private;
	uint32_t index;
	int rc, i;

	for (i = 0; i < CAM_FD_REG_MAX; i++)
		soc_private->regbase_index[i] = -1;

	if ((soc_info->num_mem_block > CAM_SOC_MAX_BLOCK) ||
		(soc_info->num_mem_block != CAM_FD_REG_MAX)) {
		CAM_ERR(CAM_FD, "Invalid num_mem_block=%d",
			soc_info->num_mem_block);
		return -EINVAL;
	}

	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "fd_core", &index);
	if ((rc == 0) && (index < CAM_FD_REG_MAX)) {
		soc_private->regbase_index[CAM_FD_REG_CORE] = index;
	} else {
		CAM_ERR(CAM_FD, "regbase not found for FD_CORE, rc=%d, %d %d",
			rc, index, CAM_FD_REG_MAX);
		return -EINVAL;
	}

	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "fd_wrapper", &index);
	if ((rc == 0) && (index < CAM_FD_REG_MAX)) {
		soc_private->regbase_index[CAM_FD_REG_WRAPPER] = index;
	} else {
		CAM_ERR(CAM_FD, "regbase not found FD_WRAPPER, rc=%d, %d %d",
			rc, index, CAM_FD_REG_MAX);
		return -EINVAL;
	}

	CAM_DBG(CAM_FD, "Reg indices : CORE=%d, WRAPPER=%d",
		soc_private->regbase_index[CAM_FD_REG_CORE],
		soc_private->regbase_index[CAM_FD_REG_WRAPPER]);

	return 0;
}

static int cam_fd_soc_set_clk_flags(struct cam_hw_soc_info *soc_info)
{
	int i, rc = 0;

	if (soc_info->num_clk > CAM_SOC_MAX_CLK) {
		CAM_ERR(CAM_FD, "Invalid num clk %d", soc_info->num_clk);
		return -EINVAL;
	}

	/* set memcore and mem periphery logic flags to 0 */
	for (i = 0; i < soc_info->num_clk; i++) {
		if ((strcmp(soc_info->clk_name[i], "fd_core_clk") == 0) ||
			(strcmp(soc_info->clk_name[i], "fd_core_uar_clk") ==
			0)) {
			rc = cam_soc_util_set_clk_flags(soc_info, i,
				CLKFLAG_NORETAIN_MEM);
			if (rc)
				CAM_ERR(CAM_FD,
					"Failed in NORETAIN_MEM i=%d, rc=%d",
					i, rc);

			cam_soc_util_set_clk_flags(soc_info, i,
				CLKFLAG_NORETAIN_PERIPH);
			if (rc)
				CAM_ERR(CAM_FD,
					"Failed in NORETAIN_PERIPH i=%d, rc=%d",
					i, rc);
		}
	}

	return rc;
}

void cam_fd_soc_register_write(struct cam_hw_soc_info *soc_info,
	enum cam_fd_reg_base reg_base, uint32_t reg_offset, uint32_t reg_value)
{
	struct cam_fd_soc_private *soc_private =
		(struct cam_fd_soc_private *)soc_info->soc_private;
	int32_t reg_index = soc_private->regbase_index[reg_base];

	CAM_DBG(CAM_FD, "FD_REG_WRITE: Base[%d] Offset[0x%8x] Value[0x%8x]",
		reg_base, reg_offset, reg_value);

	cam_io_w_mb(reg_value,
		soc_info->reg_map[reg_index].mem_base + reg_offset);
}

uint32_t cam_fd_soc_register_read(struct cam_hw_soc_info *soc_info,
	enum cam_fd_reg_base reg_base, uint32_t reg_offset)
{
	struct cam_fd_soc_private *soc_private =
		(struct cam_fd_soc_private *)soc_info->soc_private;
	int32_t reg_index = soc_private->regbase_index[reg_base];
	uint32_t reg_value;

	reg_value = cam_io_r_mb(
		soc_info->reg_map[reg_index].mem_base + reg_offset);

	CAM_DBG(CAM_FD, "FD_REG_READ: Base[%d] Offset[0x%8x] Value[0x%8x]",
		reg_base, reg_offset, reg_value);

	return reg_value;
}

int cam_fd_soc_enable_resources(struct cam_hw_soc_info *soc_info)
{
	struct cam_fd_soc_private *soc_private = soc_info->soc_private;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	int rc;

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = 7200000;
	axi_vote.uncompressed_bw = 7200000;
	rc = cam_cpas_start(soc_private->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in CPAS START, rc=%d", rc);
		return -EFAULT;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true, CAM_SVS_VOTE,
		true);
	if (rc) {
		CAM_ERR(CAM_FD, "Error enable platform failed, rc=%d", rc);
		goto stop_cpas;
	}

	return rc;

stop_cpas:
	if (cam_cpas_stop(soc_private->cpas_handle))
		CAM_ERR(CAM_FD, "Error in CPAS STOP");

	return rc;
}


int cam_fd_soc_disable_resources(struct cam_hw_soc_info *soc_info)
{
	struct cam_fd_soc_private *soc_private;
	int rc = 0;

	if (!soc_info) {
		CAM_ERR(CAM_FD, "Invalid soc_info param");
		return -EINVAL;
	}
	soc_private = soc_info->soc_private;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_FD, "disable platform resources failed, rc=%d", rc);
		return rc;
	}

	rc = cam_cpas_stop(soc_private->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_FD, "Error in CPAS STOP, handle=0x%x, rc=%d",
			soc_private->cpas_handle, rc);
		return rc;
	}

	return rc;
}

int cam_fd_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *private_data)
{
	struct cam_fd_soc_private *soc_private;
	struct cam_cpas_register_params cpas_register_param;
	int rc;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in get_dt_properties, rc=%d", rc);
		return rc;
	}

	rc = cam_soc_util_request_platform_resource(soc_info, irq_handler,
		private_data);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in request_platform_resource rc=%d",
			rc);
		return rc;
	}

	rc = cam_fd_soc_set_clk_flags(soc_info);
	if (rc) {
		CAM_ERR(CAM_FD, "failed in set_clk_flags rc=%d", rc);
		goto release_res;
	}

	soc_private = kzalloc(sizeof(struct cam_fd_soc_private), GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto release_res;
	}
	soc_info->soc_private = soc_private;

	rc = cam_fd_hw_soc_util_setup_regbase_indices(soc_info);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in setup regbase, rc=%d", rc);
		goto free_soc_private;
	}

	memset(&cpas_register_param, 0, sizeof(cpas_register_param));
	strlcpy(cpas_register_param.identifier, "fd", CAM_HW_IDENTIFIER_LENGTH);
	cpas_register_param.cell_index = soc_info->index;
	cpas_register_param.dev = &soc_info->pdev->dev;
	cpas_register_param.userdata = private_data;
	cpas_register_param.cam_cpas_client_cb = cam_fd_hw_util_cpas_callback;

	rc = cam_cpas_register_client(&cpas_register_param);
	if (rc) {
		CAM_ERR(CAM_FD, "CPAS registration failed");
		goto free_soc_private;
	}
	soc_private->cpas_handle = cpas_register_param.client_handle;
	CAM_DBG(CAM_FD, "CPAS handle=%d", soc_private->cpas_handle);

	return rc;

free_soc_private:
	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;
release_res:
	cam_soc_util_release_platform_resource(soc_info);

	return rc;
}

int cam_fd_soc_deinit_resources(struct cam_hw_soc_info *soc_info)
{
	struct cam_fd_soc_private *soc_private =
		(struct cam_fd_soc_private *)soc_info->soc_private;
	int rc;

	rc = cam_cpas_unregister_client(soc_private->cpas_handle);
	if (rc)
		CAM_ERR(CAM_FD, "Unregister cpas failed, handle=%d, rc=%d",
			soc_private->cpas_handle, rc);

	rc = cam_soc_util_release_platform_resource(soc_info);
	if (rc)
		CAM_ERR(CAM_FD, "release platform failed, rc=%d", rc);

	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;

	return rc;
}
