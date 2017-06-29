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

#define pr_fmt(fmt) "CAM-CDM-SOC %s:%d " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_cdm.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"

#define CAM_CDM_OFFSET_FROM_REG(x, y) ((x)->offsets[y].offset)
#define CAM_CDM_ATTR_FROM_REG(x, y) ((x)->offsets[y].attribute)

bool cam_cdm_read_hw_reg(struct cam_hw_info *cdm_hw,
	enum cam_cdm_regs reg, uint32_t *value)
{
	void __iomem *reg_addr;
	struct cam_cdm *cdm = (struct cam_cdm *)cdm_hw->core_info;
	void __iomem *base =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].mem_base;
	resource_size_t mem_len =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].size;

	CDM_CDBG("E: b=%pK blen=%d reg=%x off=%x\n", (void *)base,
		(int)mem_len, reg, (CAM_CDM_OFFSET_FROM_REG(cdm->offset_tbl,
		reg)));
	CDM_CDBG("E: b=%pK reg=%x off=%x\n", (void *)base,
		reg, (CAM_CDM_OFFSET_FROM_REG(cdm->offset_tbl, reg)));

	if ((reg > cdm->offset_tbl->offset_max_size) ||
		(reg > cdm->offset_tbl->last_offset)) {
		pr_err_ratelimited("Invalid reg=%d\n", reg);
		goto permission_error;
	} else {
		reg_addr = (base + (CAM_CDM_OFFSET_FROM_REG(
				cdm->offset_tbl, reg)));
		if (reg_addr > (base + mem_len)) {
			pr_err_ratelimited("Invalid mapped region %d\n", reg);
			goto permission_error;
		}
		*value = cam_io_r_mb(reg_addr);
		CDM_CDBG("X b=%pK reg=%x off=%x val=%x\n",
			(void *)base, reg, (CAM_CDM_OFFSET_FROM_REG(
				cdm->offset_tbl, reg)),	*value);
		return false;
	}
permission_error:
	*value = 0;
	return true;

}

bool cam_cdm_write_hw_reg(struct cam_hw_info *cdm_hw,
	enum cam_cdm_regs reg, uint32_t value)
{
	void __iomem *reg_addr;
	struct cam_cdm *cdm = (struct cam_cdm *)cdm_hw->core_info;
	void __iomem *base =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].mem_base;
	resource_size_t mem_len =
		cdm_hw->soc_info.reg_map[CAM_HW_CDM_BASE_INDEX].size;

	CDM_CDBG("E: b=%pK reg=%x off=%x val=%x\n", (void *)base,
		reg, (CAM_CDM_OFFSET_FROM_REG(cdm->offset_tbl, reg)), value);

	if ((reg > cdm->offset_tbl->offset_max_size) ||
		(reg > cdm->offset_tbl->last_offset)) {
		pr_err_ratelimited("CDM accessing invalid reg=%d\n", reg);
		goto permission_error;
	} else {
		reg_addr = (base + CAM_CDM_OFFSET_FROM_REG(
				cdm->offset_tbl, reg));
		if (reg_addr > (base + mem_len)) {
			pr_err_ratelimited("Accessing invalid region %d:%d\n",
				reg, (CAM_CDM_OFFSET_FROM_REG(
				cdm->offset_tbl, reg)));
			goto permission_error;
		}
		cam_io_w_mb(value, reg_addr);
		return false;
	}
permission_error:
	return true;

}

int cam_cdm_soc_load_dt_private(struct platform_device *pdev,
	struct cam_cdm_private_dt_data *ptr)
{
	int i, rc = -EINVAL;

	ptr->dt_num_supported_clients = of_property_count_strings(
						pdev->dev.of_node,
						"cdm-client-names");
	CDM_CDBG("Num supported cdm_client = %d\n",
		ptr->dt_num_supported_clients);
	if (ptr->dt_num_supported_clients >
		CAM_PER_CDM_MAX_REGISTERED_CLIENTS) {
		pr_err("Invalid count of client names count=%d\n",
			ptr->dt_num_supported_clients);
		rc = -EINVAL;
		return rc;
	}
	if (ptr->dt_num_supported_clients < 0) {
		CDM_CDBG("No cdm client names found\n");
		ptr->dt_num_supported_clients = 0;
		ptr->dt_cdm_shared = false;
	} else {
		ptr->dt_cdm_shared = true;
	}
	for (i = 0; i < ptr->dt_num_supported_clients; i++) {
		rc = of_property_read_string_index(pdev->dev.of_node,
			"cdm-client-names", i, &(ptr->dt_cdm_client_name[i]));
		CDM_CDBG("cdm-client-names[%d] = %s\n",	i,
			ptr->dt_cdm_client_name[i]);
		if (rc < 0) {
			pr_err("Reading cdm-client-names failed\n");
			break;
		}
	}

	return rc;
}

int cam_hw_cdm_soc_get_dt_properties(struct cam_hw_info *cdm_hw,
	const struct of_device_id *table)
{
	int rc;
	struct cam_hw_soc_info *soc_ptr;
	const struct of_device_id *id;

	if (!cdm_hw  || (cdm_hw->soc_info.soc_private)
		|| !(cdm_hw->soc_info.pdev))
		return -EINVAL;

	soc_ptr = &cdm_hw->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_ptr);
	if (rc != 0) {
		pr_err("Failed to retrieve the CDM dt properties\n");
	} else {
		soc_ptr->soc_private = kzalloc(
				sizeof(struct cam_cdm_private_dt_data),
				GFP_KERNEL);
		if (!soc_ptr->soc_private)
			return -ENOMEM;

		rc = cam_cdm_soc_load_dt_private(soc_ptr->pdev,
			soc_ptr->soc_private);
		if (rc != 0) {
			pr_err("Failed to load CDM dt private data\n");
			goto error;
		}
		id = of_match_node(table, soc_ptr->pdev->dev.of_node);
		if ((!id) || !(id->data)) {
			pr_err("Failed to retrieve the CDM id table\n");
			goto error;
		}
		CDM_CDBG("CDM Hw Id compatible =%s\n", id->compatible);
		((struct cam_cdm *)cdm_hw->core_info)->offset_tbl =
			(struct cam_cdm_reg_offset_table *)id->data;
		strlcpy(((struct cam_cdm *)cdm_hw->core_info)->name,
			id->compatible,
			sizeof(((struct cam_cdm *)cdm_hw->core_info)->name));
	}

	return rc;

error:
	rc = -EINVAL;
	kfree(soc_ptr->soc_private);
	soc_ptr->soc_private = NULL;
	return rc;
}

int cam_cdm_intf_mgr_soc_get_dt_properties(
	struct platform_device *pdev, struct cam_cdm_intf_mgr *mgr)
{
	int rc;

	rc = of_property_read_u32(pdev->dev.of_node,
		"num-hw-cdm", &mgr->dt_supported_hw_cdm);
	CDM_CDBG("Number of HW cdm supported =%d\n", mgr->dt_supported_hw_cdm);

	return rc;
}
