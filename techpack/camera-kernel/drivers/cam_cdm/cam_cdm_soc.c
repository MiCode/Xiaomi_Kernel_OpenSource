// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_cdm.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_cdm_soc.h"

#define CAM_CDM_OFFSET_FROM_REG(x, y) ((x)->offsets[y].offset)
#define CAM_CDM_ATTR_FROM_REG(x, y) ((x)->offsets[y].attribute)

bool cam_cdm_read_hw_reg(struct cam_hw_info *cdm_hw,
	uint32_t reg, uint32_t *value)
{
	void __iomem *reg_addr;
	void __iomem *base =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].mem_base;
	resource_size_t mem_len =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].size;

	CAM_DBG(CAM_CDM, "E: b=%pK blen=%d off=%x", (void __iomem *)base,
		(int)mem_len, reg);

	reg_addr = (base + reg);
	if (reg_addr > (base + mem_len)) {
		CAM_ERR_RATE_LIMIT(CAM_CDM,
			"Invalid mapped region %d", reg);
		goto permission_error;
	}
	*value = cam_io_r_mb(reg_addr);
	CAM_DBG(CAM_CDM, "X b=%pK off=%x val=%x",
		(void __iomem *)base, reg,
		*value);
	return false;

permission_error:
	*value = 0;
	return true;

}

bool cam_cdm_write_hw_reg(struct cam_hw_info *cdm_hw,
	uint32_t reg, uint32_t value)
{
	void __iomem *reg_addr;
	void __iomem *base =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].mem_base;
	resource_size_t mem_len =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].size;

	CAM_DBG(CAM_CDM, "E: b=%pK off=%x val=%x", (void __iomem *)base,
		reg, value);

	reg_addr = (base + reg);
	if (reg_addr > (base + mem_len)) {
		CAM_ERR_RATE_LIMIT(CAM_CDM,
			"Accessing invalid region:%d\n",
			reg);
		goto permission_error;
	}
	cam_io_w_mb(value, reg_addr);
	return false;

permission_error:
	return true;

}

int cam_cdm_soc_load_dt_private(struct platform_device *pdev,
	struct cam_cdm_private_dt_data *cdm_pvt_data)
{
	int i, rc = -EINVAL, num_fifo_entries = 0, num_clients = 0;

	num_clients = of_property_count_strings(
			pdev->dev.of_node, "cdm-client-names");
	if ((num_clients <= 0) ||
		(num_clients > CAM_PER_CDM_MAX_REGISTERED_CLIENTS)) {
		CAM_ERR(CAM_CDM, "Invalid count of client names count=%d",
			num_clients);

		rc = -EINVAL;
		goto end;
	}

	cdm_pvt_data->dt_num_supported_clients = (uint32_t)num_clients;
	CAM_DBG(CAM_CDM, "Num supported cdm_client = %u",
		cdm_pvt_data->dt_num_supported_clients);

	cdm_pvt_data->dt_cdm_shared = true;

	for (i = 0; i < cdm_pvt_data->dt_num_supported_clients; i++) {
		rc = of_property_read_string_index(pdev->dev.of_node,
			"cdm-client-names", i,
			&(cdm_pvt_data->dt_cdm_client_name[i]));
		CAM_DBG(CAM_CDM, "cdm-client-names[%d] = %s", i,
			cdm_pvt_data->dt_cdm_client_name[i]);
		if (rc < 0) {
			CAM_ERR(CAM_CDM,
				"Reading cdm-client-names failed for client: %d",
				i);
			goto end;
		}

	}

	cdm_pvt_data->is_single_ctx_cdm =
		of_property_read_bool(pdev->dev.of_node, "single-context-cdm");

	rc = of_property_read_u32(pdev->dev.of_node, "cam_hw_pid", &cdm_pvt_data->pid);
	if (rc)
		cdm_pvt_data->pid = -1;

	rc = of_property_read_u32(pdev->dev.of_node, "cam-hw-mid", &cdm_pvt_data->mid);
	if (rc)
		cdm_pvt_data->mid = -1;

	rc = of_property_read_u8(pdev->dev.of_node, "cdm-priority-group",
			&cdm_pvt_data->priority_group);
	if (rc < 0) {
		cdm_pvt_data->priority_group = 0;
		rc = 0;
	}

	cdm_pvt_data->config_fifo = of_property_read_bool(pdev->dev.of_node,
		"config-fifo");
	if (cdm_pvt_data->config_fifo) {
		num_fifo_entries = of_property_count_u32_elems(
			pdev->dev.of_node,
			"fifo-depths");
		if (num_fifo_entries != CAM_CDM_NUM_BL_FIFO) {
			CAM_ERR(CAM_CDM,
				"Wrong number of configurable FIFOs %d",
				num_fifo_entries);
			rc = -EINVAL;
			goto end;
		}
		for (i = 0; i < num_fifo_entries; i++) {
			rc = of_property_read_u32_index(pdev->dev.of_node,
				"fifo-depths", i, &cdm_pvt_data->fifo_depth[i]);
			if (rc < 0) {
				CAM_ERR(CAM_CDM,
					"Unable to read fifo-depth rc %d",
					rc);
				goto end;
			}
			CAM_DBG(CAM_CDM, "FIFO%d depth is %d",
				i, cdm_pvt_data->fifo_depth[i]);
		}
	} else {
		for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
			cdm_pvt_data->fifo_depth[i] =
				CAM_CDM_BL_FIFO_LENGTH_MAX_DEFAULT;
			CAM_DBG(CAM_CDM, "FIFO%d depth is %d",
				i, cdm_pvt_data->fifo_depth[i]);
		}
	}
end:
	return rc;
}

int cam_hw_cdm_soc_get_dt_properties(struct cam_hw_info *cdm_hw,
	const struct of_device_id *table)
{
	int rc;
	struct cam_hw_soc_info *soc_ptr;
	const struct of_device_id *id;
	struct cam_cdm *cdm_core = NULL;

	if (!cdm_hw  || (cdm_hw->soc_info.soc_private)
		|| !(cdm_hw->soc_info.pdev))
		return -EINVAL;

	cdm_core = cdm_hw->core_info;
	soc_ptr = &cdm_hw->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_ptr);
	if (rc != 0) {
		CAM_ERR(CAM_CDM, "Failed to retrieve the CDM dt properties");
		goto end;
	}

	soc_ptr->soc_private = kzalloc(
			sizeof(struct cam_cdm_private_dt_data),
			GFP_KERNEL);
	if (!soc_ptr->soc_private)
		return -ENOMEM;

	rc = cam_cdm_soc_load_dt_private(soc_ptr->pdev,
		soc_ptr->soc_private);
	if (rc != 0) {
		CAM_ERR(CAM_CDM, "Failed to load CDM dt private data");
		goto error;
	}

	id = of_match_node(table, soc_ptr->pdev->dev.of_node);
	if ((!id) || !(id->data)) {
		CAM_ERR(CAM_CDM, "Failed to retrieve the CDM id table");
		goto error;
	}
	cdm_core->offsets =
		(struct cam_cdm_hw_reg_offset *)id->data;

	CAM_DBG(CAM_CDM, "name %s", cdm_core->name);

	snprintf(cdm_core->name, sizeof(cdm_core->name), "%s%d",
		id->compatible, soc_ptr->index);

	CAM_DBG(CAM_CDM, "name %s", cdm_core->name);

	goto end;

error:
	rc = -EINVAL;
	kfree(soc_ptr->soc_private);
	soc_ptr->soc_private = NULL;
end:
	return rc;
}

int cam_cdm_intf_mgr_soc_get_dt_properties(
	struct platform_device *pdev, struct cam_cdm_intf_mgr *mgr)
{
	int rc;

	rc = of_property_read_u32(pdev->dev.of_node,
		"num-hw-cdm", &mgr->dt_supported_hw_cdm);
	CAM_DBG(CAM_CDM, "Number of HW cdm supported =%d",
		mgr->dt_supported_hw_cdm);

	return rc;
}
