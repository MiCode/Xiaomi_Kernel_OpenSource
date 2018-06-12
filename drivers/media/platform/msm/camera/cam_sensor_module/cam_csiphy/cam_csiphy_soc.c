/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include "cam_csiphy_soc.h"
#include "cam_csiphy_core.h"
#include "include/cam_csiphy_1_1_hwreg.h"
#include "include/cam_csiphy_1_0_hwreg.h"

#define BYTES_PER_REGISTER           4
#define NUM_REGISTER_PER_LINE        4
#define REG_OFFSET(__start, __i)    ((__start) + ((__i) * BYTES_PER_REGISTER))

static int cam_io_phy_dump(void __iomem *base_addr,
	uint32_t start_offset, int size)
{
	char          line_str[128];
	char         *p_str;
	int           i;
	uint32_t      data;

	CAM_INFO(CAM_CSIPHY, "addr=%pK offset=0x%x size=%d",
		base_addr, start_offset, size);

	if (!base_addr || (size <= 0))
		return -EINVAL;

	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size; i++) {
		if (i % NUM_REGISTER_PER_LINE == 0) {
			snprintf(p_str, 12, "0x%08x: ",
				REG_OFFSET(start_offset, i));
			p_str += 11;
		}
		data = readl_relaxed(base_addr + REG_OFFSET(start_offset, i));
		snprintf(p_str, 9, "%08x ", data);
		p_str += 8;
		if ((i + 1) % NUM_REGISTER_PER_LINE == 0) {
			CAM_ERR(CAM_CSIPHY, "%s", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CAM_ERR(CAM_CSIPHY, "%s", line_str);

	return 0;
}

int32_t cam_csiphy_mem_dmp(struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0;
	resource_size_t size = 0;
	void __iomem *addr = NULL;

	if (!soc_info) {
		rc = -EINVAL;
		CAM_ERR(CAM_CSIPHY, "invalid input %d", rc);
		return rc;
	}
	addr = soc_info->reg_map[0].mem_base;
	size = resource_size(soc_info->mem_block[0]);
	rc = cam_io_phy_dump(addr, 0, (size >> 2));
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "generating dump failed %d", rc);
		return rc;
	}
	return rc;
}

int32_t cam_csiphy_enable_hw(struct csiphy_device *csiphy_dev)
{
	int32_t rc = 0;
	struct cam_hw_soc_info   *soc_info;

	soc_info = &csiphy_dev->soc_info;

	if (csiphy_dev->ref_count++) {
		CAM_ERR(CAM_CSIPHY, "csiphy refcount = %d",
			csiphy_dev->ref_count);
		return rc;
	}

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, ENABLE_IRQ);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "failed to enable platform resources %d",
			rc);
		return rc;
	}

	rc = cam_soc_util_set_src_clk_rate(soc_info,
		soc_info->clk_rate[0][soc_info->src_clk_idx]);

	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "csiphy_clk_set_rate failed rc: %d", rc);
		goto csiphy_disable_platform_resource;
	}

	cam_csiphy_reset(csiphy_dev);

	return rc;


csiphy_disable_platform_resource:
	cam_soc_util_disable_platform_resource(soc_info, true, true);

	return rc;
}

int32_t cam_csiphy_disable_hw(struct csiphy_device *csiphy_dev)
{
	struct cam_hw_soc_info   *soc_info;

	if (!csiphy_dev || !csiphy_dev->ref_count) {
		CAM_ERR(CAM_CSIPHY, "csiphy dev NULL / ref_count ZERO");
		return 0;
	}
	soc_info = &csiphy_dev->soc_info;

	if (--csiphy_dev->ref_count) {
		CAM_ERR(CAM_CSIPHY, "csiphy refcount = %d",
			csiphy_dev->ref_count);
		return 0;
	}

	cam_csiphy_reset(csiphy_dev);

	cam_soc_util_disable_platform_resource(soc_info, true, true);

	return 0;
}

int32_t cam_csiphy_parse_dt_info(struct platform_device *pdev,
	struct csiphy_device *csiphy_dev)
{
	int32_t   rc = 0, i = 0;
	uint32_t  clk_cnt = 0;
	char      *csi_3p_clk_name = "csi_phy_3p_clk";
	char      *csi_3p_clk_src_name = "csiphy_3p_clk_src";
	struct cam_hw_soc_info   *soc_info;

	csiphy_dev->is_csiphy_3phase_hw = 0;
	soc_info = &csiphy_dev->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "parsing common soc dt(rc %d)", rc);
		return  rc;
	}

	csiphy_dev->is_csiphy_3phase_hw = 0;

	if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.0")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg =
				csiphy_2ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg =
				csiphy_3ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_reg = csiphy_reset_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_0;
		csiphy_dev->hw_version = CSIPHY_VERSION_V10;
		csiphy_dev->is_csiphy_3phase_hw = CSI_3PHASE_HW;
		csiphy_dev->clk_lane = 0;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.1")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg =
			csiphy_2ph_v1_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg =
			csiphy_3ph_v1_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_common_reg =
			csiphy_common_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_reset_reg =
			csiphy_reset_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_1;
		csiphy_dev->is_csiphy_3phase_hw = CSI_3PHASE_HW;
		csiphy_dev->hw_version = CSIPHY_VERSION_V11;
		csiphy_dev->clk_lane = 0;
	} else {
		CAM_ERR(CAM_CSIPHY, "invalid hw version : 0x%x",
			csiphy_dev->hw_version);
		rc =  -EINVAL;
		return rc;
	}

	if (soc_info->num_clk > CSIPHY_NUM_CLK_MAX) {
		CAM_ERR(CAM_CSIPHY, "invalid clk count=%d, max is %d",
			soc_info->num_clk, CSIPHY_NUM_CLK_MAX);
		return -EINVAL;
	}
	for (i = 0; i < soc_info->num_clk; i++) {
		if (!strcmp(soc_info->clk_name[i],
			csi_3p_clk_src_name)) {
			csiphy_dev->csiphy_3p_clk_info[0].clk_name =
				soc_info->clk_name[i];
			csiphy_dev->csiphy_3p_clk_info[0].clk_rate =
				soc_info->clk_rate[0][i];
			csiphy_dev->csiphy_3p_clk[0] =
				soc_info->clk[i];
			continue;
		} else if (!strcmp(soc_info->clk_name[i],
				csi_3p_clk_name)) {
			csiphy_dev->csiphy_3p_clk_info[1].clk_name =
				soc_info->clk_name[i];
			csiphy_dev->csiphy_3p_clk_info[1].clk_rate =
				soc_info->clk_rate[0][i];
			csiphy_dev->csiphy_3p_clk[1] =
				soc_info->clk[i];
			continue;
		}

		CAM_DBG(CAM_CSIPHY, "clk_rate[%d] = %d", clk_cnt,
			soc_info->clk_rate[0][clk_cnt]);
		clk_cnt++;
	}

	csiphy_dev->csiphy_max_clk =
		soc_info->clk_rate[0][soc_info->src_clk_idx];

	rc = cam_soc_util_request_platform_resource(&csiphy_dev->soc_info,
		cam_csiphy_irq, csiphy_dev);

	return rc;
}

int32_t cam_csiphy_soc_release(struct csiphy_device *csiphy_dev)
{
	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "csiphy dev NULL");
		return 0;
	}

	cam_soc_util_release_platform_resource(&csiphy_dev->soc_info);

	return 0;
}
