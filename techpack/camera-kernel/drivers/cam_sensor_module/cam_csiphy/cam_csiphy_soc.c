// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include "cam_csiphy_soc.h"
#include "cam_csiphy_core.h"
#include "include/cam_csiphy_1_1_hwreg.h"
#include "include/cam_csiphy_1_0_hwreg.h"
#include "include/cam_csiphy_1_2_hwreg.h"
#include "include/cam_csiphy_1_2_1_hwreg.h"
#include "include/cam_csiphy_1_2_2_hwreg.h"
#include "include/cam_csiphy_1_2_3_hwreg.h"
#include "include/cam_csiphy_1_2_5_hwreg.h"
#include "include/cam_csiphy_2_0_hwreg.h"
#include "include/cam_csiphy_2_1_0_hwreg.h"

/* Clock divide factor for CPHY spec v1.0 */
#define CSIPHY_DIVISOR_16                    16
/* Clock divide factor for CPHY spec v1.2 and up */
#define CSIPHY_DIVISOR_32                    32
/* Clock divide factor for DPHY */
#define CSIPHY_DIVISOR_8                     8
#define CSIPHY_LOG_BUFFER_SIZE_IN_BYTES      250
#define ONE_LOG_LINE_MAX_SIZE                20

static int cam_csiphy_io_dump(void __iomem *base_addr, uint16_t num_regs, int csiphy_idx)
{
	char                                    *buffer;
	uint8_t                                  buffer_offset = 0;
	uint8_t                                  rem_buffer_size = CSIPHY_LOG_BUFFER_SIZE_IN_BYTES;
	uint16_t                                 i;
	uint32_t                                 reg_offset;

	if (!base_addr || !num_regs) {
		CAM_ERR(CAM_CSIPHY, "Invalid params. base_addr: 0x%p num_regs: %u",
			base_addr, num_regs);
		return -EINVAL;
	}

	buffer = kzalloc(CSIPHY_LOG_BUFFER_SIZE_IN_BYTES, GFP_KERNEL);
	if (!buffer) {
		CAM_ERR(CAM_CSIPHY, "Could not allocate the memory for buffer");
		return -ENOMEM;
	}

	CAM_INFO(CAM_CSIPHY, "Base: 0x%pK num_regs: %u", base_addr, num_regs);
	CAM_INFO(CAM_CSIPHY, "CSIPHY:%d Dump", csiphy_idx);
	for (i = 0; i < num_regs; i++) {
		reg_offset = i << 2;
		buffer_offset += scnprintf(buffer + buffer_offset, rem_buffer_size, "0x%x=0x%x\n",
			reg_offset, cam_io_r_mb(base_addr + reg_offset));

		rem_buffer_size = CSIPHY_LOG_BUFFER_SIZE_IN_BYTES - buffer_offset;

		if (rem_buffer_size <= ONE_LOG_LINE_MAX_SIZE) {
			buffer[buffer_offset - 1] = '\0';
			pr_info("%s\n", buffer);
			buffer_offset = 0;
			rem_buffer_size = CSIPHY_LOG_BUFFER_SIZE_IN_BYTES;
		}
	}

	if (buffer_offset) {
		buffer[buffer_offset - 1] = '\0';
		pr_info("%s\n", buffer);
	}

	kfree(buffer);

	return 0;
}

int32_t cam_csiphy_reg_dump(struct cam_hw_soc_info *soc_info)
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
	rc = cam_csiphy_io_dump(addr, (size >> 2), soc_info->index);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "generating dump failed %d", rc);
		return rc;
	}
	return rc;
}

int32_t cam_csiphy_common_status_reg_dump(struct csiphy_device *csiphy_dev)
{
	struct csiphy_reg_parms_t *csiphy_reg = NULL;
	int32_t                    rc = 0;
	resource_size_t            size = 0;
	void __iomem              *phy_base = NULL;
	int                        reg_id = 0;
	uint32_t                   val, status_reg, clear_reg;

	if (!csiphy_dev) {
		rc = -EINVAL;
		CAM_ERR(CAM_CSIPHY, "invalid input %d", rc);
		return rc;
	}

	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;
	phy_base = csiphy_dev->soc_info.reg_map[0].mem_base;
	status_reg = csiphy_reg->mipi_csiphy_interrupt_status0_addr;
	clear_reg = csiphy_reg->mipi_csiphy_interrupt_clear0_addr;
	size = csiphy_reg->csiphy_num_common_status_regs;

	CAM_INFO(CAM_CSIPHY, "PHY base addr=%pK offset=0x%x size=%d",
		phy_base, status_reg, size);

	if (phy_base != NULL) {
		for (reg_id = 0; reg_id < size; reg_id++) {
			val = cam_io_r(phy_base + status_reg + (0x4 * reg_id));

			if (reg_id < csiphy_reg->csiphy_interrupt_status_size)
				cam_io_w_mb(val, phy_base + clear_reg + (0x4 * reg_id));

			CAM_INFO(CAM_CSIPHY, "CSIPHY%d_COMMON_STATUS%u = 0x%x",
				csiphy_dev->soc_info.index, reg_id, val);
		}
	} else {
		rc = -EINVAL;
		CAM_ERR(CAM_CSIPHY, "phy base is NULL  %d", rc);
		return rc;
	}
	return rc;
}

enum cam_vote_level get_clk_vote_default(struct csiphy_device *csiphy_dev,
	int32_t index)
{
	CAM_DBG(CAM_CSIPHY, "voting for SVS");
	return CAM_SVS_VOTE;
}

enum cam_vote_level get_clk_voting_dynamic(
	struct csiphy_device *csiphy_dev, int32_t index)
{
	uint32_t cam_vote_level = 0;
	uint32_t last_valid_vote = 0;
	struct cam_hw_soc_info *soc_info;
	uint64_t phy_data_rate = csiphy_dev->csiphy_info[index].data_rate;

	soc_info = &csiphy_dev->soc_info;
	phy_data_rate = max(phy_data_rate, csiphy_dev->current_data_rate);

	if (csiphy_dev->csiphy_info[index].csiphy_3phase) {
		if (csiphy_dev->is_divisor_32_comp)
			do_div(phy_data_rate, CSIPHY_DIVISOR_32);
		else
			do_div(phy_data_rate, CSIPHY_DIVISOR_16);
	} else {
		do_div(phy_data_rate, CSIPHY_DIVISOR_8);
	}

	 /* round off to next integer */
	phy_data_rate += 1;
	csiphy_dev->current_data_rate = phy_data_rate;

	for (cam_vote_level = 0;
			cam_vote_level < CAM_MAX_VOTE; cam_vote_level++) {
		if (soc_info->clk_level_valid[cam_vote_level] != true)
			continue;

		if (soc_info->clk_rate[cam_vote_level]
			[csiphy_dev->rx_clk_src_idx] > phy_data_rate) {
			CAM_DBG(CAM_CSIPHY,
				"match detected %s : %llu:%d level : %d",
				soc_info->clk_name[csiphy_dev->rx_clk_src_idx],
				phy_data_rate,
				soc_info->clk_rate[cam_vote_level]
				[csiphy_dev->rx_clk_src_idx],
				cam_vote_level);
			return cam_vote_level;
		}
		last_valid_vote = cam_vote_level;
	}
	return last_valid_vote;
}

int32_t cam_csiphy_enable_hw(struct csiphy_device *csiphy_dev, int32_t index)
{
	int32_t rc = 0;
	struct cam_hw_soc_info   *soc_info;
	enum cam_vote_level vote_level = CAM_SVS_VOTE;

	soc_info = &csiphy_dev->soc_info;

	if (csiphy_dev->ref_count++) {
		CAM_ERR(CAM_CSIPHY, "csiphy refcount = %d",
			csiphy_dev->ref_count);
		return rc;
	}

	vote_level = csiphy_dev->ctrl_reg->getclockvoting(csiphy_dev, index);
	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		vote_level, true);
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
	uint32_t   is_regulator_enable_sync;
	char      *csi_3p_clk_name = "csi_phy_3p_clk";
	char      *csi_3p_clk_src_name = "csiphy_3p_clk_src";
	struct cam_hw_soc_info   *soc_info;

	soc_info = &csiphy_dev->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "parsing common soc dt(rc %d)", rc);
		return  rc;
	}

	rc = of_property_read_u32(soc_info->dev->of_node, "rgltr-enable-sync",
		&is_regulator_enable_sync);
	if (rc) {
		rc = 0;
		is_regulator_enable_sync = 0;
	}

	csiphy_dev->prgm_cmn_reg_across_csiphy = (bool) is_regulator_enable_sync;

	if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.0")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = csiphy_3ph_v1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_0;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->hw_version = CSIPHY_VERSION_V10;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = NULL;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.1")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = csiphy_3ph_v1_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_1;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_1;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->hw_version = CSIPHY_VERSION_V11;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = NULL;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_voting_dynamic;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2;
		csiphy_dev->is_divisor_32_comp = true;
		csiphy_dev->hw_version = CSIPHY_VERSION_V12;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.1")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2_1;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2_1;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2_1;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2_1;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_voting_dynamic;
		csiphy_dev->is_divisor_32_comp = true;
		csiphy_dev->hw_version = CSIPHY_VERSION_V121;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2_1;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.2")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->hw_version = CSIPHY_VERSION_V12;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.2.2")) {
		/* settings for lito v2 */
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_2_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2_2;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2_2;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->hw_version = CSIPHY_VERSION_V12;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.3")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_3_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_3_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_3_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2_3;
		csiphy_dev->is_divisor_32_comp = true;
		csiphy_dev->hw_version = CSIPHY_VERSION_V123;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2_3;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.4")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_3_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_3_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_3_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2_3;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_voting_dynamic;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2_3;
		csiphy_dev->is_divisor_32_comp = true;
		csiphy_dev->hw_version = CSIPHY_VERSION_V124;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_1_2_3;
	}  else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v1.2.5")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v1_2_5_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v1_2_5_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v1_2_5_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_1_2_5;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_1_2_5;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_1_2_5;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v1_2_5;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->hw_version = CSIPHY_VERSION_V125;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = NULL;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v2.0")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v2_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v2_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v2_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v2_0;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->hw_version = CSIPHY_VERSION_V20;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = NULL;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v2.0.1")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v2_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v2_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v2_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_reg_2_0;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = NULL;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v2_0;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_vote_default;
		csiphy_dev->hw_version = CSIPHY_VERSION_V201;
		csiphy_dev->is_divisor_32_comp = false;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = NULL;
	} else if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v2.1.0") || of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v2.1.0-xiaomi-l2")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_2ph_v2_1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg = csiphy_2ph_v2_1_0_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_3ph_v2_1_0_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg = NULL;
		csiphy_dev->ctrl_reg->csiphy_irq_reg = csiphy_irq_reg_2_1_0;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_common_reg_2_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_enter_regs = csiphy_reset_enter_reg_2_1_0;
		csiphy_dev->ctrl_reg->csiphy_reset_exit_regs = csiphy_reset_exit_reg_2_1_0;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_v2_1_0;
		csiphy_dev->ctrl_reg->getclockvoting = get_clk_voting_dynamic;
		csiphy_dev->hw_version = CSIPHY_VERSION_V210;
		csiphy_dev->is_divisor_32_comp = true;
		csiphy_dev->clk_lane = 0;
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_2_1_0;
		csiphy_dev->ctrl_reg->csiphy_bist_reg = &bist_setting_2_1_0;
	} else {
		CAM_ERR(CAM_CSIPHY, "invalid hw version : 0x%x",
			csiphy_dev->hw_version);
		rc =  -EINVAL;
		return rc;
	}

	if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-v2.1.0-xiaomi-l2")) {
		csiphy_dev->ctrl_reg->data_rates_settings_table = &data_rate_delta_table_2_1_0_xiaomi_l2;
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
		} else if (!strcmp(soc_info->clk_name[i],
				CAM_CSIPHY_RX_CLK_SRC)) {
			csiphy_dev->rx_clk_src_idx = i;
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
