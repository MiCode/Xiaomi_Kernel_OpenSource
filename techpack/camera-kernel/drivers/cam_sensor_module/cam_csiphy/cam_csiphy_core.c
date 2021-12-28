// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>

#include <dt-bindings/msm-camera.h>

#include "cam_compat.h"
#include "cam_csiphy_core.h"
#include "cam_csiphy_dev.h"
#include "cam_csiphy_soc.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "cam_mem_mgr.h"
#include "cam_cpas_api.h"
#include "cam_compat.h"

#define SCM_SVC_CAMERASS 0x18
#define SECURE_SYSCALL_ID 0x6
#define SECURE_SYSCALL_ID_2 0x7

#define LANE_MASK_2PH 0x1F
#define LANE_MASK_3PH 0x7

/* Size of CPAS_SEC_LANE_CP_CTRL register mask */
#define SEC_LANE_CP_REG_LEN 32
/*
 * PHY index at which CPAS_SEC_LANE_CP_CTRL register mask
 * changes depending on PHY HW version
 */
#define MAX_PHY_MSK_PER_REG 4

static DEFINE_MUTEX(active_csiphy_cnt_mutex);

static int csiphy_onthego_reg_count;
static unsigned int csiphy_onthego_regs[150];
module_param_array(csiphy_onthego_regs, uint, &csiphy_onthego_reg_count, 0644);
MODULE_PARM_DESC(csiphy_onthego_regs, "Functionality to let csiphy registers program on the fly");

struct g_csiphy_data {
	void __iomem *base_address;
	uint8_t is_3phase;
	uint32_t cpas_handle;
	bool enable_aon_support;
	struct cam_csiphy_aon_sel_params_t *aon_sel_param;
};

static struct g_csiphy_data g_phy_data[MAX_CSIPHY] = {{0, 0}};
static int active_csiphy_hw_cnt;

int32_t cam_csiphy_get_instance_offset(
	struct csiphy_device *csiphy_dev,
	int32_t dev_handle)
{
	int32_t i = 0;

	if ((csiphy_dev->acquire_count >
		csiphy_dev->session_max_device_support) ||
		(csiphy_dev->acquire_count < 0)) {
		CAM_ERR(CAM_CSIPHY,
			"Invalid acquire count: %d, Max supported device for session: %u",
			csiphy_dev->acquire_count,
			csiphy_dev->session_max_device_support);
		return -EINVAL;
	}

	for (i = 0; i < csiphy_dev->acquire_count; i++) {
		if (dev_handle ==
			csiphy_dev->csiphy_info[i].hdl_data.device_hdl)
			break;
	}

	return i;
}

static void cam_csiphy_reset_phyconfig_param(struct csiphy_device *csiphy_dev,
	int32_t index)
{
	CAM_DBG(CAM_CSIPHY, "Resetting phyconfig param at index: %d", index);
	csiphy_dev->csiphy_info[index].lane_cnt = 0;
	csiphy_dev->csiphy_info[index].lane_assign = 0;
	csiphy_dev->csiphy_info[index].lane_enable = 0;
	csiphy_dev->csiphy_info[index].settle_time = 0;
	csiphy_dev->csiphy_info[index].data_rate = 0;
	csiphy_dev->csiphy_info[index].secure_mode = 0;
	csiphy_dev->csiphy_info[index].mipi_flags = 0;
	csiphy_dev->csiphy_info[index].hdl_data.device_hdl = -1;
	csiphy_dev->csiphy_info[index].csiphy_3phase = -1;
}

static inline void cam_csiphy_apply_onthego_reg_values(void __iomem *csiphybase, uint8_t csiphy_idx)
{
	int                                                      i;

	CAM_DBG(CAM_CSIPHY, "csiphy: %d, onthego_reg_count: %d",
		csiphy_idx,
		csiphy_onthego_reg_count);

	if (csiphy_onthego_reg_count % 3)
		csiphy_onthego_reg_count -= (csiphy_onthego_reg_count % 3);

	for (i = 0; i < csiphy_onthego_reg_count; i += 3) {
		cam_io_w_mb(csiphy_onthego_regs[i+1],
			csiphybase + csiphy_onthego_regs[i]);

		if (csiphy_onthego_regs[i+2])
			usleep_range(csiphy_onthego_regs[i+2], csiphy_onthego_regs[i+2] + 5);

		CAM_INFO(CAM_CSIPHY, "Offset: 0x%x, Val: 0x%x Delay(us): %u",
			csiphy_onthego_regs[i],
			cam_io_r_mb(csiphybase + csiphy_onthego_regs[i]),
			csiphy_onthego_regs[i+2]);
	}
}

static inline int cam_csiphy_release_from_reset_state(struct csiphy_device *csiphy_dev,
	void __iomem *csiphybase, int32_t instance)
{
	int                                                  i;
	struct csiphy_reg_parms_t                           *csiphy_reg;
	struct csiphy_reg_t                                 *csiphy_reset_release_reg;
	bool                                                 config_found = false;

	if (!csiphy_dev || !csiphybase) {
		CAM_ERR(CAM_CSIPHY, "Invalid input params: csiphy_dev: %s, csiphybase: %s",
			CAM_IS_NULL_TO_STR(csiphy_dev),
			CAM_IS_NULL_TO_STR(csiphybase));
		return -EINVAL;
	}

	CAM_DBG(CAM_CSIPHY, "Csiphy idx: %d", csiphy_dev->soc_info.index);

	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;
	for (i = 0; i < csiphy_reg->csiphy_reset_exit_array_size; i++) {
		csiphy_reset_release_reg = &csiphy_dev->ctrl_reg->csiphy_reset_exit_regs[i];

		switch (csiphy_reset_release_reg->csiphy_param_type) {
		case CSIPHY_2PH_REGS:
			if (!g_phy_data[csiphy_dev->soc_info.index].is_3phase &&
				!csiphy_dev->combo_mode &&
				!csiphy_dev->cphy_dphy_combo_mode) {
				cam_io_w_mb(csiphy_reset_release_reg->reg_data,
					csiphybase + csiphy_reset_release_reg->reg_addr);
				config_found = true;
			}
			break;
		case CSIPHY_3PH_REGS:
			if (g_phy_data[csiphy_dev->soc_info.index].is_3phase &&
				!csiphy_dev->combo_mode  &&
				!csiphy_dev->cphy_dphy_combo_mode) {
				cam_io_w_mb(csiphy_reset_release_reg->reg_data,
					csiphybase + csiphy_reset_release_reg->reg_addr);
				config_found = true;
			}
			break;
		case CSIPHY_2PH_COMBO_REGS:
			if (!csiphy_dev->csiphy_info[instance].csiphy_3phase &&
					csiphy_dev->combo_mode &&
					!csiphy_dev->cphy_dphy_combo_mode) {
				cam_io_w_mb(csiphy_reset_release_reg->reg_data,
					csiphybase + csiphy_reset_release_reg->reg_addr);
				config_found = true;
			}
			break;
		case CSIPHY_3PH_COMBO_REGS:
			if (csiphy_dev->csiphy_info[instance].csiphy_3phase &&
					csiphy_dev->combo_mode &&
					!csiphy_dev->cphy_dphy_combo_mode) {
				cam_io_w_mb(csiphy_reset_release_reg->reg_data,
					csiphybase + csiphy_reset_release_reg->reg_addr);
				config_found = true;
			}
			break;
		case CSIPHY_2PH_3PH_COMBO_REGS:
			if (!csiphy_dev->combo_mode && csiphy_dev->cphy_dphy_combo_mode) {
				cam_io_w_mb(csiphy_reset_release_reg->reg_data,
					csiphybase + csiphy_reset_release_reg->reg_addr);
				config_found = true;
			}
			break;
		default:
			CAM_ERR(CAM_CSIPHY, "Invalid combination");
			return -EINVAL;
			break;
		}

		if (config_found) {
			if (csiphy_reset_release_reg->delay) {
				usleep_range(csiphy_reset_release_reg->delay,
					csiphy_reset_release_reg->delay + 5);
			}

			break;
		}
	}

	return 0;
}

void cam_csiphy_query_cap(struct csiphy_device *csiphy_dev,
	struct cam_csiphy_query_cap *csiphy_cap)
{
	struct cam_hw_soc_info *soc_info = &csiphy_dev->soc_info;

	csiphy_cap->slot_info = soc_info->index;
	csiphy_cap->version = csiphy_dev->hw_version;
	csiphy_cap->clk_lane = csiphy_dev->clk_lane;
}

int cam_csiphy_dump_status_reg(struct csiphy_device *csiphy_dev)
{
	struct cam_hw_soc_info *soc_info;
	void __iomem *phybase = NULL;
	void __iomem *lane0_offset = 0;
	void __iomem *lane1_offset = 0;
	void __iomem *lane2_offset = 0;
	void __iomem *lane3_offset = 0;
	struct csiphy_reg_parms_t *csiphy_reg;
	struct cam_cphy_dphy_status_reg_params_t *status_regs;
	int i = 0;

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Null csiphy_dev");
		return -EINVAL;
	}

	soc_info = &csiphy_dev->soc_info;
	if (!soc_info) {
		CAM_ERR(CAM_CSIPHY, "Null soc_info");
		return -EINVAL;
	}

	if (!g_phy_data[soc_info->index].base_address) {
		CAM_ERR(CAM_CSIPHY, "Invalid cphy_idx: %d", soc_info->index);
		return -EINVAL;
	}

	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;
	status_regs = csiphy_reg->status_reg_params;
	phybase = g_phy_data[soc_info->index].base_address;

	if (!status_regs) {
		CAM_ERR(CAM_CSIPHY, "2ph/3ph status offset not set");
		return -EINVAL;
	}

	if (g_phy_data[soc_info->index].is_3phase) {
		CAM_INFO(CAM_CSIPHY, "Dumping 3ph status regs");
		lane0_offset = phybase + status_regs->csiphy_3ph_status0_offset;
		lane1_offset =
			lane0_offset + csiphy_reg->size_offset_betn_lanes;
		lane2_offset =
			lane1_offset + csiphy_reg->size_offset_betn_lanes;

		for (i = 0; i < status_regs->csiphy_3ph_status_size; i++) {
			CAM_INFO(CAM_CSIPHY,
				"PHY: %d, Status%u. Ln0: 0x%x, Ln1: 0x%x, Ln2: 0x%x",
				soc_info->index, i,
				cam_io_r(lane0_offset + (i * 4)),
				cam_io_r(lane1_offset + (i * 4)),
				cam_io_r(lane2_offset + (i * 4)));
		}
	} else {
		CAM_INFO(CAM_CSIPHY, "Dumping 2ph status regs");
		lane0_offset = phybase + status_regs->csiphy_2ph_status0_offset;
		lane1_offset =
			lane0_offset + csiphy_reg->size_offset_betn_lanes;
		lane2_offset =
			lane1_offset + csiphy_reg->size_offset_betn_lanes;
		lane3_offset =
			lane2_offset + csiphy_reg->size_offset_betn_lanes;

		for (i = 0; i < status_regs->csiphy_2ph_status_size; i++) {
			CAM_INFO(CAM_CSIPHY,
				"PHY: %d, Status%u. Ln0: 0x%x, Ln1: 0x%x, Ln2: 0x%x, Ln3: 0x%x",
				soc_info->index, i,
				cam_io_r(lane0_offset + (i * 4)),
				cam_io_r(lane1_offset + (i * 4)),
				cam_io_r(lane2_offset + (i * 4)),
				cam_io_r(lane3_offset + (i * 4)));
		}
	}
	return 0;
}

void cam_csiphy_reset(struct csiphy_device *csiphy_dev)
{
	int32_t  i;
	void __iomem *base = NULL;
	uint32_t size =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_reset_enter_array_size;
	struct cam_hw_soc_info *soc_info = &csiphy_dev->soc_info;

	base = soc_info->reg_map[0].mem_base;

	for (i = 0; i < size; i++) {
		cam_io_w_mb(
			csiphy_dev->ctrl_reg->csiphy_reset_enter_regs[i].reg_data,
			base +
			csiphy_dev->ctrl_reg->csiphy_reset_enter_regs[i].reg_addr);
		if (csiphy_dev->ctrl_reg->csiphy_reset_enter_regs[i].delay > 0)
			usleep_range(
			csiphy_dev->ctrl_reg->csiphy_reset_enter_regs[i].delay,
			csiphy_dev->ctrl_reg->csiphy_reset_enter_regs[i].delay
			+ 5);
	}

	if (csiphy_dev->en_lane_status_reg_dump) {
		CAM_INFO(CAM_CSIPHY, "Status Reg Dump after phy reset");
		cam_csiphy_dump_status_reg(csiphy_dev);
	}
}

static void cam_csiphy_prgm_cmn_data(
	struct csiphy_device *csiphy_dev,
	bool reset)
{
	int csiphy_idx = 0;
	uint32_t size = 0;
	int i = 0;
	void __iomem *csiphybase;
	bool is_3phase = false;
	struct csiphy_reg_t *csiphy_common_reg = NULL;

	size = csiphy_dev->ctrl_reg->csiphy_reg.csiphy_common_array_size;

	if (active_csiphy_hw_cnt < 0 || active_csiphy_hw_cnt >= MAX_CSIPHY) {
		CAM_WARN(CAM_CSIPHY,
			"MisMatched in active phy hw: %d and Max supported: %d",
			active_csiphy_hw_cnt, MAX_CSIPHY);
		return;
	}

	if (active_csiphy_hw_cnt == 0) {
		CAM_DBG(CAM_CSIPHY, "CSIPHYs HW state needs to be %s",
			reset ? "reset" : "set");
	} else {
		CAM_DBG(CAM_CSIPHY, "Active CSIPHY hws are %d",
			active_csiphy_hw_cnt);
		return;
	}

	for (csiphy_idx = 0; csiphy_idx < MAX_CSIPHY; csiphy_idx++) {
		csiphybase = g_phy_data[csiphy_idx].base_address;
		is_3phase = g_phy_data[csiphy_idx].is_3phase;

		if (!csiphybase) {
			CAM_DBG(CAM_CSIPHY, "CSIPHY: %d is not available in platform",
				csiphy_idx);
			continue;
		}

		for (i = 0; i < size; i++) {
			csiphy_common_reg =
				&csiphy_dev->ctrl_reg->csiphy_common_reg[i];
			switch (csiphy_common_reg->csiphy_param_type) {
			case CSIPHY_DEFAULT_PARAMS:
				cam_io_w_mb(reset ? 0x00 :
					csiphy_common_reg->reg_data,
					csiphybase +
					csiphy_common_reg->reg_addr);
				break;
			default:
				break;
			}
			if (csiphy_common_reg->delay > 0)
				usleep_range(csiphy_common_reg->delay,
					csiphy_common_reg->delay + 5);
		}
	}
}

static int32_t cam_csiphy_update_secure_info(
	struct csiphy_device *csiphy_dev, int32_t index)
{
	uint32_t adj_lane_mask = 0;
	uint16_t lane_assign = 0;
	uint32_t phy_mask_len = 0;
	uint8_t lane_cnt = 0;

	lane_assign = csiphy_dev->csiphy_info[index].lane_assign;
	lane_cnt = csiphy_dev->csiphy_info[index].lane_cnt;

	while (lane_cnt--) {
		if ((lane_assign & 0xF) == 0x0)
			adj_lane_mask |= 0x1;
		else
			adj_lane_mask |= (1 << (lane_assign & 0xF));

		lane_assign >>= 4;
	}

	switch (csiphy_dev->hw_version) {
	case CSIPHY_VERSION_V201:
	case CSIPHY_VERSION_V125:
		phy_mask_len =
		CAM_CSIPHY_MAX_DPHY_LANES + CAM_CSIPHY_MAX_CPHY_LANES + 1;
		break;
	case CSIPHY_VERSION_V121:
	case CSIPHY_VERSION_V123:
	case CSIPHY_VERSION_V124:
	case CSIPHY_VERSION_V210:
		phy_mask_len =
		(csiphy_dev->soc_info.index < MAX_PHY_MSK_PER_REG) ?
		(CAM_CSIPHY_MAX_DPHY_LANES + CAM_CSIPHY_MAX_CPHY_LANES) :
		(CAM_CSIPHY_MAX_DPHY_LANES + CAM_CSIPHY_MAX_CPHY_LANES + 1);
		break;
	default:
		phy_mask_len =
		CAM_CSIPHY_MAX_DPHY_LANES + CAM_CSIPHY_MAX_CPHY_LANES;
		break;
	}

	if (csiphy_dev->soc_info.index < MAX_PHY_MSK_PER_REG) {
		csiphy_dev->csiphy_info[index].csiphy_cpas_cp_reg_mask =
			adj_lane_mask <<
			((csiphy_dev->soc_info.index * phy_mask_len) +
			(!csiphy_dev->csiphy_info[index].csiphy_3phase) *
			(CAM_CSIPHY_MAX_CPHY_LANES));
	} else {
		csiphy_dev->csiphy_info[index].csiphy_cpas_cp_reg_mask =
			((uint64_t)adj_lane_mask) <<
			((csiphy_dev->soc_info.index - MAX_PHY_MSK_PER_REG) *
			phy_mask_len + SEC_LANE_CP_REG_LEN +
			(!csiphy_dev->csiphy_info[index].csiphy_3phase) *
			(CAM_CSIPHY_MAX_CPHY_LANES));
	}

	CAM_DBG(CAM_CSIPHY, "csi phy idx:%d, cp_reg_mask:0x%lx",
		csiphy_dev->soc_info.index,
		csiphy_dev->csiphy_info[index].csiphy_cpas_cp_reg_mask);

	return 0;
}

static int cam_csiphy_get_lane_enable(
	struct csiphy_device *csiphy, int index,
	uint16_t lane_assign, uint32_t *lane_enable)
{
	uint32_t lane_select = 0;

	if (csiphy->csiphy_info[index].csiphy_3phase) {
		CAM_DBG(CAM_CSIPHY, "LaneEnable for CPHY");
		switch (lane_assign & 0xF) {
		case 0x0:
			lane_select |= CPHY_LANE_0;
			break;
		case 0x1:
			lane_select |= CPHY_LANE_1;
			break;
		case 0x2:
			lane_select |= CPHY_LANE_2;
			break;
		default:
			CAM_ERR(CAM_CSIPHY,
				"Wrong lane configuration for CPHY : %d",
				lane_assign);
			*lane_enable = 0;
			return -EINVAL;
		}
	} else {
		CAM_DBG(CAM_CSIPHY, "LaneEnable for DPHY");
		switch (lane_assign & 0xF) {
		case 0x0:
			lane_select |= DPHY_LANE_0;
			lane_select |= DPHY_CLK_LN;
			break;
		case 0x1:
			lane_select |= DPHY_LANE_1;
			lane_select |= DPHY_CLK_LN;
			break;
		case 0x2:
			lane_select |= DPHY_LANE_2;
			if (csiphy->combo_mode)
				lane_select |= DPHY_LANE_3;
			else
				lane_select |= DPHY_CLK_LN;
			break;
		case 0x3:
			if (csiphy->combo_mode) {
				CAM_ERR(CAM_CSIPHY,
					"Wrong lane configuration for DPHYCombo: %d",
					lane_assign);
				*lane_enable = 0;
				return -EINVAL;
			}
			lane_select |= DPHY_LANE_3;
			lane_select |= DPHY_CLK_LN;
			break;
		default:
			CAM_ERR(CAM_CSIPHY,
				"Wrong lane configuration for DPHY: %d",
				lane_assign);
			*lane_enable = 0;
			return -EINVAL;
		}
	}

	*lane_enable = lane_select;
	CAM_DBG(CAM_CSIPHY, "Lane_select: 0x%x", lane_select);

	return 0;
}

static int cam_csiphy_sanitize_lane_cnt(
	struct csiphy_device *csiphy_dev,
	int32_t index, uint8_t lane_cnt)
{
	uint8_t max_supported_lanes = 0;

	if (csiphy_dev->combo_mode) {
		if (csiphy_dev->csiphy_info[index].csiphy_3phase)
			max_supported_lanes = 1;
		else
			max_supported_lanes = 2;
	} else if (csiphy_dev->cphy_dphy_combo_mode) {
		/* 2DPHY + 1CPHY or 2CPHY + 1DPHY */
		if (csiphy_dev->csiphy_info[index].csiphy_3phase)
			max_supported_lanes = 2;
		else
			max_supported_lanes = 2;
	} else {
		/* Mission Mode */
		if (csiphy_dev->csiphy_info[index].csiphy_3phase)
			max_supported_lanes = 3;
		else
			max_supported_lanes = 4;
	}

	if (lane_cnt <= 0 || lane_cnt > max_supported_lanes) {
		CAM_ERR(CAM_CSIPHY,
			"wrong lane_cnt configuration: expected max lane_cnt: %u received lane_cnt: %u",
			max_supported_lanes, lane_cnt);
		return -EINVAL;
	}

	return 0;
}

int32_t cam_cmd_buf_parser(struct csiphy_device *csiphy_dev,
	struct cam_config_dev_cmd *cfg_dev)
{
	int                      rc = 0;
	uintptr_t                generic_ptr;
	uintptr_t                generic_pkt_ptr;
	struct cam_packet       *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uint32_t                *cmd_buf = NULL;
	struct cam_csiphy_info  *cam_cmd_csiphy_info = NULL;
	size_t                  len;
	size_t                  remain_len;
	int                     index;
	uint32_t                lane_enable = 0;
	uint16_t                lane_assign = 0;
	uint8_t                 lane_cnt = 0;
	uint16_t                preamble_en = 0;

	if (!cfg_dev || !csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Invalid Args");
		return -EINVAL;
	}

	rc = cam_mem_get_cpu_buf((int32_t) cfg_dev->packet_handle,
		&generic_pkt_ptr, &len);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "Failed to get packet Mem address: %d", rc);
		return rc;
	}

	remain_len = len;
	if ((sizeof(struct cam_packet) > len) ||
		((size_t)cfg_dev->offset >= len - sizeof(struct cam_packet))) {
		CAM_ERR(CAM_CSIPHY,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len);
		rc = -EINVAL;
		return rc;
	}

	remain_len -= (size_t)cfg_dev->offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_ptr + (uint32_t)cfg_dev->offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_CSIPHY, "Invalid packet params");
		rc = -EINVAL;
		return rc;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&csl_packet->payload +
		csl_packet->cmd_buf_offset / 4);

	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		&generic_ptr, &len);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY,
			"Failed to get cmd buf Mem address : %d", rc);
		return rc;
	}

	if ((len < sizeof(struct cam_csiphy_info)) ||
		(cmd_desc->offset > (len - sizeof(struct cam_csiphy_info)))) {
		CAM_ERR(CAM_CSIPHY,
			"Not enough buffer provided for cam_cisphy_info");
		rc = -EINVAL;
		return rc;
	}

	cmd_buf = (uint32_t *)generic_ptr;
	cmd_buf += cmd_desc->offset / 4;
	cam_cmd_csiphy_info = (struct cam_csiphy_info *)cmd_buf;

	index = cam_csiphy_get_instance_offset(csiphy_dev, cfg_dev->dev_handle);
	if (index < 0 || index  >= csiphy_dev->session_max_device_support) {
		CAM_ERR(CAM_CSIPHY, "index in invalid: %d", index);
		return -EINVAL;
	}

	rc = cam_csiphy_sanitize_lane_cnt(csiphy_dev, index,
		cam_cmd_csiphy_info->lane_cnt);
	if (rc) {
		CAM_ERR(CAM_CSIPHY,
			"Wrong configuration lane_cnt: %u",
			cam_cmd_csiphy_info->lane_cnt);
		return rc;
	}


	preamble_en = (cam_cmd_csiphy_info->mipi_flags &
		PREAMBLE_PATTEN_CAL_MASK);

	/* Cannot support CPHY combo mode with One sensor setting
	 * preamble enable and second/third sensor is without
	 * preamble enable.
	 */
	if (csiphy_dev->preamble_enable && !preamble_en &&
		csiphy_dev->combo_mode &&
		!csiphy_dev->cphy_dphy_combo_mode) {
		CAM_ERR(CAM_CSIPHY,
			"Cannot support %s combo mode with differnt preamble settings",
			(csiphy_dev->csiphy_info[index].csiphy_3phase ?
			"CPHY" : "DPHY"));
		return -EINVAL;
	}

	csiphy_dev->preamble_enable = preamble_en;
	csiphy_dev->csiphy_info[index].lane_cnt = cam_cmd_csiphy_info->lane_cnt;
	csiphy_dev->csiphy_info[index].lane_assign =
		cam_cmd_csiphy_info->lane_assign;

	csiphy_dev->csiphy_info[index].settle_time =
		cam_cmd_csiphy_info->settle_time;
	csiphy_dev->csiphy_info[index].data_rate =
		cam_cmd_csiphy_info->data_rate;
	csiphy_dev->csiphy_info[index].secure_mode =
		cam_cmd_csiphy_info->secure_mode;
	csiphy_dev->csiphy_info[index].mipi_flags =
		(cam_cmd_csiphy_info->mipi_flags & SKEW_CAL_MASK);

	lane_assign = csiphy_dev->csiphy_info[index].lane_assign;
	lane_cnt = csiphy_dev->csiphy_info[index].lane_cnt;

	while (lane_cnt--) {
		rc = cam_csiphy_get_lane_enable(csiphy_dev, index,
			(lane_assign & 0xF), &lane_enable);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Wrong lane configuration: %d",
				csiphy_dev->csiphy_info[index].lane_assign);
			if ((csiphy_dev->combo_mode) ||
				(csiphy_dev->cphy_dphy_combo_mode)) {
				CAM_DBG(CAM_CSIPHY,
					"Resetting error to zero for other devices to configure");
				rc = 0;
			}
			lane_enable = 0;
			csiphy_dev->csiphy_info[index].lane_enable = lane_enable;
			goto reset_settings;
		}
		csiphy_dev->csiphy_info[index].lane_enable |= lane_enable;
		lane_assign >>= 4;
	}

	if (cam_cmd_csiphy_info->secure_mode == 1)
		cam_csiphy_update_secure_info(csiphy_dev,
			index);

	CAM_DBG(CAM_CSIPHY,
		"phy version:%d, phy_idx: %d, preamble_en: %u",
		csiphy_dev->hw_version,
		csiphy_dev->soc_info.index,
		csiphy_dev->preamble_enable);
	CAM_DBG(CAM_CSIPHY,
		"3phase:%d, combo mode:%d, secure mode:%d",
		csiphy_dev->csiphy_info[index].csiphy_3phase,
		csiphy_dev->combo_mode,
		cam_cmd_csiphy_info->secure_mode);
	CAM_DBG(CAM_CSIPHY,
		"lane_cnt: 0x%x, lane_assign: 0x%x, lane_enable: 0x%x, settle time:%llu, datarate:%llu",
		csiphy_dev->csiphy_info[index].lane_cnt,
		csiphy_dev->csiphy_info[index].lane_assign,
		csiphy_dev->csiphy_info[index].lane_enable,
		csiphy_dev->csiphy_info[index].settle_time,
		csiphy_dev->csiphy_info[index].data_rate);

	return rc;

reset_settings:
	cam_csiphy_reset_phyconfig_param(csiphy_dev, index);

	return rc;
}

void cam_csiphy_cphy_irq_config(struct csiphy_device *csiphy_dev)
{
	int32_t                        i;
	struct csiphy_reg_t           *csiphy_irq_reg;
	uint32_t num_of_irq_status_regs = 0;

	void __iomem *csiphybase =
		csiphy_dev->soc_info.reg_map[0].mem_base;

	num_of_irq_status_regs =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_interrupt_status_size;

	for (i = 0; i < num_of_irq_status_regs; i++) {
		csiphy_irq_reg = &csiphy_dev->ctrl_reg->csiphy_irq_reg[i];
		cam_io_w_mb(csiphy_irq_reg->reg_data,
			csiphybase + csiphy_irq_reg->reg_addr);

		if (csiphy_irq_reg->delay)
			usleep_range(csiphy_irq_reg->delay,
				csiphy_irq_reg->delay + 5);
	}
}

void cam_csiphy_cphy_irq_disable(struct csiphy_device *csiphy_dev)
{
	int32_t i;
	void __iomem *csiphybase =
		csiphy_dev->soc_info.reg_map[0].mem_base;
	uint32_t num_of_irq_status_regs = 0;

	num_of_irq_status_regs =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_interrupt_status_size;

	for (i = 0; i < num_of_irq_status_regs; i++)
		cam_io_w_mb(0x0, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_irq_reg[i].reg_addr);
}

irqreturn_t cam_csiphy_irq(int irq_num, void *data)
{
	struct csiphy_device *csiphy_dev =
		(struct csiphy_device *)data;
	struct cam_hw_soc_info *soc_info = NULL;
	struct csiphy_reg_parms_t *csiphy_reg = NULL;
	void __iomem *base = NULL;

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Invalid Args");
		return IRQ_NONE;
	}

	soc_info = &csiphy_dev->soc_info;
	base = csiphy_dev->soc_info.reg_map[0].mem_base;
	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;

	if (csiphy_dev->en_common_status_reg_dump) {
		cam_csiphy_common_status_reg_dump(csiphy_dev);
		cam_io_w_mb(0x1, base + csiphy_reg->mipi_csiphy_glbl_irq_cmd_addr);
		cam_io_w_mb(0x0, base + csiphy_reg->mipi_csiphy_glbl_irq_cmd_addr);
	}

	return IRQ_HANDLED;
}

static int cam_csiphy_cphy_get_data_rate_lane_idx(
	struct csiphy_device *csiphy_dev, int32_t index,
	uint16_t lane_assign,
	struct data_rate_reg_info_t *drate_settings)
{
	int rc = 0;
	int idx = -1;
	uint32_t lane_enable;

	rc = cam_csiphy_get_lane_enable(csiphy_dev, index,
		lane_assign, &lane_enable);
	if (rc) {
		CAM_ERR(CAM_CSIPHY,
			"Wrong configuration for lane_assign: %u", lane_assign);
		return rc;
	}

	for (idx =  0; idx < CAM_CSIPHY_MAX_CPHY_LANES; idx++) {
		if (lane_enable & drate_settings->per_lane_info[idx].lane_identifier)
			return idx;
	}

	if (idx == CAM_CSIPHY_MAX_CPHY_LANES) {
		CAM_ERR(CAM_CSIPHY,
			"Lane not found in datarate table");
		rc = -EINVAL;
	}

	return rc;
}

static int cam_csiphy_cphy_data_rate_config(
	struct csiphy_device *csiphy_device, int32_t idx)
{
	int i = 0;
	int lane_idx = -1;
	int data_rate_idx = -1;
	uint64_t required_phy_data_rate = 0;
	void __iomem *csiphybase = NULL;
	ssize_t num_data_rates = 0;
	struct data_rate_settings_t *settings_table = NULL;
	struct csiphy_cphy_per_lane_info *per_lane = NULL;
	uint32_t lane_enable = 0;
	uint8_t lane_cnt = 0;
	uint16_t lane_assign = 0;
	uint64_t intermediate_var = 0;
	uint16_t settle_cnt = 0;
	uint32_t reg_addr = 0, reg_data = 0, reg_param_type = 0;
	uint8_t  skew_cal_enable = 0;
	int32_t  delay = 0;

	if ((csiphy_device == NULL) || (csiphy_device->ctrl_reg == NULL)) {
		CAM_ERR(CAM_CSIPHY, "Device is NULL");
		return -EINVAL;
	}

	if (csiphy_device->ctrl_reg->data_rates_settings_table == NULL) {
		CAM_DBG(CAM_CSIPHY,
			"Data rate specific register table not available");
		return 0;
	}

	required_phy_data_rate = csiphy_device->csiphy_info[idx].data_rate;
	csiphybase =
		csiphy_device->soc_info.reg_map[0].mem_base;
	settings_table =
		csiphy_device->ctrl_reg->data_rates_settings_table;
	num_data_rates =
		settings_table->num_data_rate_settings;
	lane_cnt = csiphy_device->csiphy_info[idx].lane_cnt;

	intermediate_var = csiphy_device->csiphy_info[idx].settle_time;
	do_div(intermediate_var, 200000000);
	settle_cnt = intermediate_var;
	skew_cal_enable = csiphy_device->csiphy_info[idx].mipi_flags;

	CAM_DBG(CAM_CSIPHY, "required data rate : %llu", required_phy_data_rate);
	for (data_rate_idx = 0; data_rate_idx < num_data_rates;
			data_rate_idx++) {
		struct data_rate_reg_info_t *drate_settings =
			settings_table->data_rate_settings;
		uint64_t supported_phy_bw = drate_settings[data_rate_idx].bandwidth;
		ssize_t  num_reg_entries =
			drate_settings[data_rate_idx].data_rate_reg_array_size;

		if ((required_phy_data_rate > supported_phy_bw) &&
			(data_rate_idx < (num_data_rates - 1))) {
			CAM_DBG(CAM_CSIPHY,
				"Skipping table [%d] with BW: %llu, Required data_rate: %llu",
				data_rate_idx, supported_phy_bw, required_phy_data_rate);
			continue;
		}

		CAM_DBG(CAM_CSIPHY, "table[%d] BW : %llu Selected",
			data_rate_idx, supported_phy_bw);
		lane_enable = csiphy_device->csiphy_info[idx].lane_enable;
		lane_assign = csiphy_device->csiphy_info[idx].lane_assign;
		lane_idx = -1;

		while (lane_cnt--) {
			lane_idx = cam_csiphy_cphy_get_data_rate_lane_idx(
				csiphy_device, idx, (lane_assign & 0xF),
				&drate_settings[data_rate_idx]);
			if (lane_idx < 0) {
				CAM_ERR(CAM_CSIPHY,
					"Lane_assign %u failed to find the lane for datarate_idx: %d",
					lane_assign, data_rate_idx);
				return -EINVAL;
			}

			lane_assign >>= 4;
			per_lane = &drate_settings[data_rate_idx].per_lane_info[lane_idx];

			for (i = 0; i < num_reg_entries; i++) {
				reg_addr = per_lane->csiphy_data_rate_regs[i]
					.reg_addr;
				reg_data = per_lane->csiphy_data_rate_regs[i]
					.reg_data;
				reg_param_type =
					per_lane->csiphy_data_rate_regs[i]
					.csiphy_param_type;
				delay = per_lane->csiphy_data_rate_regs[i]
					.delay;
				CAM_DBG(CAM_CSIPHY,
					"param_type: %d writing reg : %x val : %x delay: %dus",
					reg_param_type, reg_addr, reg_data,
					delay);
				switch (reg_param_type) {
				case CSIPHY_DEFAULT_PARAMS:
					cam_io_w_mb(reg_data,
						csiphybase + reg_addr);
				break;
				case CSIPHY_SETTLE_CNT_LOWER_BYTE:
					cam_io_w_mb(settle_cnt & 0xFF,
						csiphybase + reg_addr);
				break;
				case CSIPHY_SETTLE_CNT_HIGHER_BYTE:
					cam_io_w_mb((settle_cnt >> 8) & 0xFF,
						csiphybase + reg_addr);
				break;
				case CSIPHY_SKEW_CAL:
				if (skew_cal_enable)
					cam_io_w_mb(reg_data,
						csiphybase + reg_addr);
				break;
				default:
					CAM_DBG(CAM_CSIPHY, "Do Nothing");
				break;
				}
				if (delay > 0)
					usleep_range(delay, delay + 5);
			}
		}
		break;
	}

	return 0;
}

static int __cam_csiphy_prgm_bist_reg(struct csiphy_device *csiphy_dev, bool is_3phase)
{
	int i = 0;
	int bist_arr_size = csiphy_dev->ctrl_reg->csiphy_bist_reg->num_data_settings;
	struct csiphy_reg_t *csiphy_common_reg = NULL;
	void __iomem *csiphybase = NULL;

	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;

	for (i = 0; i < bist_arr_size; i++) {
		csiphy_common_reg = &csiphy_dev->ctrl_reg->csiphy_bist_reg->bist_arry[i];
		switch (csiphy_common_reg->csiphy_param_type) {
		case CSIPHY_3PH_REGS:
			if (is_3phase)
				cam_io_w_mb(csiphy_common_reg->reg_data,
					csiphybase + csiphy_common_reg->reg_addr);
		break;
		case CSIPHY_2PH_REGS:
			if (!is_3phase)
				cam_io_w_mb(csiphy_common_reg->reg_data,
						csiphybase + csiphy_common_reg->reg_addr);
		break;
		default:
			cam_io_w_mb(csiphy_common_reg->reg_data,
				csiphybase + csiphy_common_reg->reg_addr);
		break;
		}
	}

	return 0;
}

int32_t cam_csiphy_config_dev(struct csiphy_device *csiphy_dev,
	int32_t dev_handle)
{
	int32_t      rc = 0;
	uint32_t     lane_enable = 0;
	uint32_t     size = 0;
	uint16_t     i = 0, cfg_size = 0;
	uint16_t     lane_assign = 0;
	uint8_t      lane_cnt;
	int          max_lanes = 0;
	uint16_t     settle_cnt = 0;
	uint8_t      skew_cal_enable = 0;
	uint64_t     intermediate_var;
	uint8_t      lane_pos = 0;
	int          index;
	void __iomem *csiphybase;
	struct csiphy_reg_t *csiphy_common_reg = NULL;
	struct csiphy_reg_t (*reg_array)[MAX_SETTINGS_PER_LANE];
	bool         is_3phase = false;
	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;

	CAM_DBG(CAM_CSIPHY, "ENTER");
	if (!csiphybase) {
		CAM_ERR(CAM_CSIPHY, "csiphybase NULL");
		return -EINVAL;
	}

	index = cam_csiphy_get_instance_offset(csiphy_dev, dev_handle);
	if (index < 0 || index >= csiphy_dev->session_max_device_support) {
		CAM_ERR(CAM_CSIPHY, "index is invalid: %d", index);
		return -EINVAL;
	}

	CAM_DBG(CAM_CSIPHY,
		"Index: %d: expected dev_hdl: 0x%x : derived dev_hdl: 0x%x",
		index, dev_handle,
		csiphy_dev->csiphy_info[index].hdl_data.device_hdl);

	if (csiphy_dev->csiphy_info[index].csiphy_3phase)
		is_3phase = true;

	if (csiphy_dev->combo_mode) {
		/* for CPHY(3Phase) or DPHY(2Phase) combo mode selection */
		if (is_3phase) {
			/* CPHY combo mode */
			if (csiphy_dev->ctrl_reg->csiphy_3ph_combo_reg) {
				reg_array = csiphy_dev->ctrl_reg
					->csiphy_3ph_combo_reg;
			} else {
				CAM_WARN(CAM_CSIPHY,
					"CPHY combo mode reg settings not found");
				reg_array =
					csiphy_dev->ctrl_reg->csiphy_3ph_reg;
			}
			cfg_size = csiphy_dev->ctrl_reg->csiphy_reg
				.csiphy_3ph_config_array_size;
			max_lanes = CAM_CSIPHY_MAX_CPHY_LANES;
		} else {
			/* DPHY combo mode*/
			if (csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg) {
				reg_array = csiphy_dev
					->ctrl_reg->csiphy_2ph_combo_mode_reg;
			} else {
				CAM_WARN(CAM_CSIPHY,
					"DPHY combo mode reg settings not found");
				reg_array = csiphy_dev
					->ctrl_reg->csiphy_2ph_reg;
			}
			cfg_size = csiphy_dev->ctrl_reg->csiphy_reg
					.csiphy_2ph_config_array_size;
			max_lanes = MAX_LANES;
		}
	} else if (csiphy_dev->cphy_dphy_combo_mode) {
		/* for CPHY and DPHY combo mode selection */
		if (csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg) {
			reg_array = csiphy_dev
				->ctrl_reg->csiphy_2ph_3ph_mode_reg;
			cfg_size = csiphy_dev->ctrl_reg->csiphy_reg
				.csiphy_2ph_3ph_config_array_size;
			max_lanes = CAM_CSIPHY_MAX_CPHY_DPHY_COMBO_LN;
		} else {
			reg_array = csiphy_dev->ctrl_reg->csiphy_3ph_reg;
			cfg_size =
				csiphy_dev->ctrl_reg->csiphy_reg
					.csiphy_3ph_config_array_size;
			CAM_WARN(CAM_CSIPHY,
					"Unsupported configuration, Falling back to CPHY mission mode");
			max_lanes = CAM_CSIPHY_MAX_CPHY_LANES;
		}
	} else {
		/* for CPHY(3Phase) or DPHY(2Phase) Non combe mode selection */
		if (is_3phase) {
			CAM_DBG(CAM_CSIPHY,
				"3phase Non combo mode reg array selected");
			reg_array = csiphy_dev->ctrl_reg->csiphy_3ph_reg;
			max_lanes = CAM_CSIPHY_MAX_CPHY_LANES;
			cfg_size = csiphy_dev->ctrl_reg->csiphy_reg
				.csiphy_3ph_config_array_size;
		} else {
			CAM_DBG(CAM_CSIPHY,
				"2PHASE Non combo mode reg array selected");
			reg_array = csiphy_dev->ctrl_reg->csiphy_2ph_reg;
			cfg_size = csiphy_dev->ctrl_reg->csiphy_reg
				.csiphy_2ph_config_array_size;
			max_lanes = MAX_LANES;
		}
	}

	lane_cnt = csiphy_dev->csiphy_info[index].lane_cnt;
	lane_assign = csiphy_dev->csiphy_info[index].lane_assign;
	lane_enable = csiphy_dev->csiphy_info[index].lane_enable;

	size = csiphy_dev->ctrl_reg->csiphy_reg.csiphy_common_array_size;
	for (i = 0; i < size; i++) {
		csiphy_common_reg = &csiphy_dev->ctrl_reg->csiphy_common_reg[i];
		switch (csiphy_common_reg->csiphy_param_type) {
		case CSIPHY_LANE_ENABLE:
			CAM_DBG(CAM_CSIPHY, "LANE_ENABLE: 0x%x", lane_enable);
			cam_io_w_mb(lane_enable,
				csiphybase + csiphy_common_reg->reg_addr);
			break;
		case CSIPHY_DEFAULT_PARAMS:
			cam_io_w_mb(csiphy_common_reg->reg_data,
				csiphybase + csiphy_common_reg->reg_addr);
			break;
		case CSIPHY_2PH_REGS:
			if (!is_3phase) {
				cam_io_w_mb(csiphy_common_reg->reg_data,
					csiphybase +
					csiphy_common_reg->reg_addr);
			}
			break;
		case CSIPHY_3PH_REGS:
			if (is_3phase) {
				cam_io_w_mb(csiphy_common_reg->reg_data,
					csiphybase +
					csiphy_common_reg->reg_addr);
			}
			break;
		default:
			break;
		}
		if (csiphy_common_reg->delay > 0)
			usleep_range(csiphy_common_reg->delay,
				csiphy_common_reg->delay + 5);
	}

	if (csiphy_dev->csiphy_info[index].csiphy_3phase) {
		rc = cam_csiphy_cphy_data_rate_config(csiphy_dev, index);
		if (rc) {
			CAM_ERR(CAM_CSIPHY,
				"Date rate specific configuration failed rc: %d",
				rc);
			return rc;
		}
	}

	intermediate_var = csiphy_dev->csiphy_info[index].settle_time;
	do_div(intermediate_var, 200000000);
	settle_cnt = intermediate_var;
	skew_cal_enable = csiphy_dev->csiphy_info[index].mipi_flags;

	for (lane_pos = 0; lane_pos < max_lanes; lane_pos++) {
		CAM_DBG(CAM_CSIPHY, "lane_pos: %d is configuring", lane_pos);
		for (i = 0; i < cfg_size; i++) {
			switch (reg_array[lane_pos][i].csiphy_param_type) {
			case CSIPHY_LANE_ENABLE:
				cam_io_w_mb(lane_enable,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_DEFAULT_PARAMS:
				cam_io_w_mb(reg_array[lane_pos][i].reg_data,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_SETTLE_CNT_LOWER_BYTE:
				cam_io_w_mb(settle_cnt & 0xFF,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_SETTLE_CNT_HIGHER_BYTE:
				cam_io_w_mb((settle_cnt >> 8) & 0xFF,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_SKEW_CAL:
			if (skew_cal_enable)
				cam_io_w_mb(reg_array[lane_pos][i].reg_data,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			default:
				CAM_DBG(CAM_CSIPHY, "Do Nothing");
			break;
			}

			if (reg_array[lane_pos][i].delay > 0) {
				usleep_range(reg_array[lane_pos][i].delay,
					reg_array[lane_pos][i].delay + 5);
			}
		}
	}

	if (csiphy_dev->preamble_enable)
		__cam_csiphy_prgm_bist_reg(csiphy_dev, is_3phase);

	cam_csiphy_cphy_irq_config(csiphy_dev);

	CAM_DBG(CAM_CSIPHY, "EXIT");
	return rc;
}

void cam_csiphy_shutdown(struct csiphy_device *csiphy_dev)
{
	struct cam_hw_soc_info *soc_info;
	struct csiphy_reg_parms_t *csiphy_reg;
	int32_t i = 0;

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_INIT)
		return;

	if (!csiphy_dev->acquire_count)
		return;

	if (csiphy_dev->acquire_count >= CSIPHY_MAX_INSTANCES_PER_PHY) {
		CAM_WARN(CAM_CSIPHY, "acquire count is invalid: %u",
			csiphy_dev->acquire_count);
		csiphy_dev->acquire_count =
			CSIPHY_MAX_INSTANCES_PER_PHY;
	}

	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_START) {
		soc_info = &csiphy_dev->soc_info;

		for (i = 0; i < csiphy_dev->acquire_count; i++) {
			if (csiphy_dev->csiphy_info[i].secure_mode)
				cam_csiphy_notify_secure_mode(
					csiphy_dev,
					CAM_SECURE_MODE_NON_SECURE, i);

			csiphy_dev->csiphy_info[i].secure_mode =
				CAM_SECURE_MODE_NON_SECURE;

			cam_csiphy_reset_phyconfig_param(csiphy_dev, i);
		}

		if (csiphy_dev->prgm_cmn_reg_across_csiphy) {
			mutex_lock(&active_csiphy_cnt_mutex);
			active_csiphy_hw_cnt--;
			mutex_unlock(&active_csiphy_cnt_mutex);

			cam_csiphy_prgm_cmn_data(csiphy_dev, true);
		}

		cam_csiphy_reset(csiphy_dev);
		cam_soc_util_disable_platform_resource(soc_info, true, true);

		//deleted by xiaomi
		//cam_cpas_stop(csiphy_dev->cpas_handle);
		csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;
	}

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_ACQUIRE) {
		for (i = 0; i < csiphy_dev->acquire_count; i++) {
			if (csiphy_dev->csiphy_info[i].hdl_data.device_hdl
				!= -1)
				cam_destroy_device_hdl(
				csiphy_dev->csiphy_info[i]
				.hdl_data.device_hdl);
			csiphy_dev->csiphy_info[i].hdl_data.device_hdl = -1;
			csiphy_dev->csiphy_info[i].hdl_data.session_hdl = -1;
		}
	}

	//xiaomi: force stop cpas
	cam_cpas_stop(csiphy_dev->cpas_handle);
	csiphy_dev->ref_count = 0;
	csiphy_dev->acquire_count = 0;
	csiphy_dev->start_dev_count = 0;
	csiphy_dev->csiphy_state = CAM_CSIPHY_INIT;
}

static int32_t cam_csiphy_external_cmd(struct csiphy_device *csiphy_dev,
	struct cam_config_dev_cmd *p_submit_cmd)
{
	struct cam_csiphy_info cam_cmd_csiphy_info;
	int32_t rc = 0;
	int32_t  index = -1;

	if (copy_from_user(&cam_cmd_csiphy_info,
		u64_to_user_ptr(p_submit_cmd->packet_handle),
		sizeof(struct cam_csiphy_info))) {
		CAM_ERR(CAM_CSIPHY, "failed to copy cam_csiphy_info\n");
		rc = -EFAULT;
	} else {
		index = cam_csiphy_get_instance_offset(csiphy_dev,
			p_submit_cmd->dev_handle);
		if (index < 0 ||
			index >= csiphy_dev->session_max_device_support) {
			CAM_ERR(CAM_CSIPHY, "index is invalid: %d", index);
			return -EINVAL;
		}

		csiphy_dev->csiphy_info[index].lane_cnt =
			cam_cmd_csiphy_info.lane_cnt;
		csiphy_dev->csiphy_info[index].lane_assign =
			cam_cmd_csiphy_info.lane_assign;
		csiphy_dev->csiphy_info[index].settle_time =
			cam_cmd_csiphy_info.settle_time;
		csiphy_dev->csiphy_info[index].data_rate =
			cam_cmd_csiphy_info.data_rate;
		CAM_DBG(CAM_CSIPHY,
			"%s CONFIG_DEV_EXT settle_time= %lld lane_cnt=%d",
			__func__,
			csiphy_dev->csiphy_info[index].settle_time,
			csiphy_dev->csiphy_info[index].lane_cnt);
	}

	return rc;
}

static int cam_csiphy_update_lane(
	struct csiphy_device *csiphy, int index, bool enable)
{
	int i = 0;
	uint32_t lane_enable = 0;
	uint32_t size = 0;
	uint16_t lane_assign;
	void __iomem *base_address;
	struct csiphy_reg_t *csiphy_common_reg = NULL;

	base_address = csiphy->soc_info.reg_map[0].mem_base;
	size = csiphy->ctrl_reg->csiphy_reg.csiphy_common_array_size;

	for (i = 0; i < size; i++) {
		csiphy_common_reg = &csiphy->ctrl_reg->csiphy_common_reg[i];
		if (csiphy_common_reg->csiphy_param_type == CSIPHY_LANE_ENABLE) {
			CAM_DBG(CAM_CSIPHY, "LANE_ENABLE: %d", lane_enable);
			lane_enable = cam_io_r(base_address + csiphy_common_reg->reg_addr);
			break;
		}
	}

	if (i == size) {
		CAM_ERR_RATE_LIMIT(CAM_CSIPHY, "Couldnt find CSIPHY_LANE_ENABLE");
		return -EINVAL;
	}

	lane_assign = csiphy->csiphy_info[index].lane_assign;

	if (enable)
		lane_enable |= csiphy->csiphy_info[index].lane_enable;
	else
		lane_enable &= ~csiphy->csiphy_info[index].lane_enable;

	CAM_DBG(CAM_CSIPHY, "lane_assign: 0x%x, lane_enable: 0x%x",
		lane_assign, lane_enable);

	if (csiphy_common_reg->csiphy_param_type == CSIPHY_LANE_ENABLE) {
		cam_io_w_mb(lane_enable, base_address + csiphy_common_reg->reg_addr);
		if (csiphy_common_reg->delay)
			usleep_range(csiphy_common_reg->delay, csiphy_common_reg->delay + 5);

		return 0;
	}

	return -EINVAL;
}

static int __csiphy_cpas_configure_for_main_or_aon(
	bool get_access, uint32_t cpas_handle,
	struct cam_csiphy_aon_sel_params_t *aon_sel_params)
{
	uint32_t aon_config = 0;

	cam_cpas_reg_read(cpas_handle, CAM_CPAS_REG_CPASTOP,
		aon_sel_params->aon_cam_sel_offset,
		true, &aon_config);

	if (get_access) {
		aon_config &= ~(aon_sel_params->cam_sel_mask |
			aon_sel_params->mclk_sel_mask);
		CAM_DBG(CAM_CSIPHY,
			"Selecting MainCamera over AON Camera");
	} else if (!get_access) {
		aon_config |= (aon_sel_params->cam_sel_mask |
			aon_sel_params->mclk_sel_mask);
		CAM_DBG(CAM_CSIPHY,
			"Releasing MainCamera to AON Camera");
	}

	CAM_DBG(CAM_CSIPHY, "value of aon_config = %u", aon_config);
	if (cam_cpas_reg_write(cpas_handle, CAM_CPAS_REG_CPASTOP,
		aon_sel_params->aon_cam_sel_offset,
		true, aon_config)) {
		CAM_ERR(CAM_CSIPHY,
				"CPAS AON sel register write failed");
	}

	return 0;
}

static int cam_csiphy_cpas_ops(
	uint32_t cpas_handle, bool start)
{
	int rc = 0;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};

	if (start) {
		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = CAM_LOWSVS_VOTE;
		axi_vote.num_paths = 1;
		axi_vote.axi_path[0].path_data_type =
			CAM_AXI_PATH_DATA_ALL;
		axi_vote.axi_path[0].transac_type =
			CAM_AXI_TRANSACTION_WRITE;
		axi_vote.axi_path[0].camnoc_bw =
			CAM_CPAS_DEFAULT_AXI_BW;
		axi_vote.axi_path[0].mnoc_ab_bw =
			CAM_CPAS_DEFAULT_AXI_BW;
		axi_vote.axi_path[0].mnoc_ib_bw =
			CAM_CPAS_DEFAULT_AXI_BW;

		rc = cam_cpas_start(cpas_handle,
			&ahb_vote, &axi_vote);
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "voting CPAS: %d", rc);
			return rc;
		}
		CAM_DBG(CAM_CSIPHY, "CPAS START");
	} else {
		rc = cam_cpas_stop(cpas_handle);
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "de-voting CPAS: %d", rc);
			return rc;
		}
		CAM_DBG(CAM_CSIPHY, "CPAS STOPPED");
	}

	return rc;
}

int cam_csiphy_util_update_aon_registration
	(uint32_t phy_idx, bool is_aon_user)
{
	/* aon support enable for the sensor associated with phy idx*/
	if (phy_idx >= MAX_CSIPHY) {
		CAM_ERR(CAM_CSIPHY,
			"Invalid PHY index: %u", phy_idx);
		return -EINVAL;
	}

	if (!g_phy_data[phy_idx].base_address) {
		CAM_ERR(CAM_CSIPHY, "Invalid PHY idx: %d from Sensor user", phy_idx);
		return -EINVAL;
	}

	g_phy_data[phy_idx].enable_aon_support = is_aon_user;

	return 0;
}

int cam_csiphy_util_update_aon_ops(
	bool get_access, uint32_t phy_idx)
{
	uint32_t cpas_hdl = 0;
	struct cam_csiphy_aon_sel_params_t *aon_sel_params;
	int rc = 0;

	if (phy_idx >= MAX_CSIPHY) {
		CAM_ERR(CAM_CSIPHY, "Null device");
		return -ENODEV;
	}

	if (!g_phy_data[phy_idx].base_address) {
		CAM_ERR(CAM_CSIPHY, "phy_idx: %d is not supported", phy_idx);
		return -EINVAL;
	}

	if (!g_phy_data[phy_idx].aon_sel_param) {
		CAM_ERR(CAM_CSIPHY, "AON select parameters are null");
		return -EINVAL;
	}

	cpas_hdl = g_phy_data[phy_idx].cpas_handle;
	aon_sel_params = g_phy_data[phy_idx].aon_sel_param;

	rc = cam_csiphy_cpas_ops(cpas_hdl, true);
	if (rc) {
		if (rc == -EPERM) {
			CAM_WARN(CAM_CSIPHY,
				"CPHY: %d is already in start state");
		} else {
			CAM_ERR(CAM_CSIPHY, "voting CPAS: %d failed", rc);
			return rc;
		}
	}

	CAM_DBG(CAM_CSIPHY, "PHY idx: %d, AON_support is %s", phy_idx,
		(get_access) ? "enable" : "disable");
	rc = __csiphy_cpas_configure_for_main_or_aon(
			get_access, cpas_hdl, aon_sel_params);
	if (rc) {
		CAM_ERR(CAM_CSIPHY, "Configuration for AON ops failed: rc: %d",
			rc);
		cam_csiphy_cpas_ops(cpas_hdl, false);
		return rc;
	}

	if (rc != -EPERM)
		cam_csiphy_cpas_ops(cpas_hdl, false);

	return rc;
}

static void __cam_csiphy_read_2phase_bist_counter_status(
	struct csiphy_device *csiphy_dev, uint32_t *counter)
{
	int i = 0, lane_count;
	int bist_status_arr_size =
		csiphy_dev->ctrl_reg->csiphy_bist_reg->number_of_counters;
	uint32_t base_offset = 0;
	void __iomem *phy_base = NULL;
	uint32_t val = 0;
	uint32_t offset_betwn_lane = 0;
	struct bist_reg_settings_t *bist_reg = NULL;

	phy_base = csiphy_dev->soc_info.reg_map[0].mem_base;
	bist_reg = csiphy_dev->ctrl_reg->csiphy_bist_reg;
	offset_betwn_lane =
		csiphy_dev->ctrl_reg->csiphy_reg.size_offset_betn_lanes;

	for (i = 0; i < bist_status_arr_size; i++) {
		base_offset = bist_reg->bist_counter_2ph_base_offset + (0x4 * i);
		val = 0;
		for (lane_count = 0; lane_count < CAM_CSIPHY_MAX_DPHY_LANES; lane_count++) {
			CAM_DBG(CAM_CSIPHY, "value to be read from addr: 0x%x is 0x%x",
				base_offset + (lane_count * offset_betwn_lane),
				(cam_io_r(phy_base + base_offset + (lane_count * offset_betwn_lane))));
			val |= ((cam_io_r(phy_base + base_offset + (lane_count * offset_betwn_lane))));
		}
		*counter |= (val << (i * 8));
		CAM_DBG(CAM_CSIPHY, "COUNTER VALUE is 0x%x", *counter);
	}

	return;
}

static void __cam_csiphy_get_2phase_pattern_status(
	struct csiphy_device *csiphy_dev)
{
	int i = 0;
	int bist_status_arr_size =
		csiphy_dev->ctrl_reg->csiphy_bist_reg->num_status_reg;
	struct csiphy_reg_t *csiphy_common_reg = NULL;
	void __iomem *csiphybase = NULL;
	uint32_t status = 0;
	uint32_t counter = 0;
	struct bist_reg_settings_t *bist_reg = NULL;

	CAM_DBG(CAM_CSIPHY, "ENTER");
	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;
	bist_reg = csiphy_dev->ctrl_reg->csiphy_bist_reg;

	/* This loop is to read every lane status value
	 * in case if loop breaks with only last lane.
	 */
	for (i = 0; i < bist_status_arr_size; i++) {
		csiphy_common_reg = &bist_reg->bist_status_arr[i];
		switch (csiphy_common_reg->csiphy_param_type) {
		case CSIPHY_2PH_REGS:
			status |= cam_io_r(csiphybase + csiphy_common_reg->reg_addr);
		break;
		}
	}

	/* Read the Counter value for possible corrupted headers */
	__cam_csiphy_read_2phase_bist_counter_status(csiphy_dev, &counter);

	if ((status & PREAMBLE_PATTERN_BIST_DONE) &&
		(status & bist_reg->error_status_val_2ph)) {
		/**
		 * This condition happen when CSIPHY try to read status after sensor
		 * streamoff. In this case error bit is set due to postamble detection
		 * which is bit(4). In this scenraio this is not consider as an error.
		 * We need to check for status2/3 counter value to determine if there are
		 * more header that is corrupted than 2. Counter always shows value of 2
		 * with postamble packet.
		 *
		 */
		if (counter <= PREAMBLE_MAX_ERR_COUNT_ALLOWED) {
			CAM_INFO(CAM_CSIPHY,
				"PN9 Pattern rxced succesfully:: counter value: 0x%x, Status0: 0x%x",
				counter, status);
		} else {
			CAM_INFO(CAM_CSIPHY,
				"PN9 Pattern is corrupted:: counter value: 0x%x, Status0: 0x%x",
				counter, status);
		}
	} else if ((status & PREAMBLE_PATTERN_BIST_DONE) &&
		!(status & bist_reg->error_status_val_2ph)) {
		/**
		 * This condition happen when CSIPHY try to read status with some counter
		 * value is set to check against. In this case error bit is not expected
		 * to be set.
		 */
		CAM_INFO(CAM_CSIPHY,
			"PN9 Pattern rxced succesfully:: counter value: 0x%x, Status0: 0x%x",
			counter, status);
	} else {
		CAM_INFO(CAM_CSIPHY,
			"PN9 Pattern is corrupted:: counter value: 0x%x Status0: 0x%x",
			counter, status);
	}

	return;
}

static void __cam_csiphy_2ph_status_checker_ops(
	struct csiphy_device *csiphy_dev, bool set)
{
	int i = 0;
	void __iomem *csiphybase = NULL;
	uint32_t base_offset = 0;
	uint32_t read_back_value;
	uint32_t lane_offset = 0;
	uint32_t offset_betwn_lane = 0;
	struct bist_reg_settings_t *bist_reg = NULL;

	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;
	bist_reg = csiphy_dev->ctrl_reg->csiphy_bist_reg;
	base_offset = bist_reg->set_status_update_2ph_base_offset;
	offset_betwn_lane =
		csiphy_dev->ctrl_reg->csiphy_reg.size_offset_betn_lanes;

	/* Set checker bit to read the correct status1 value */
	for (i = 0; i < CAM_CSIPHY_MAX_DPHY_LANES; i++) {
		lane_offset = base_offset + (i * offset_betwn_lane);
		read_back_value = cam_io_r(csiphybase + lane_offset);
		set ? (read_back_value |= PREAMBLE_PATTERN_SET_CHECKER) :
			(read_back_value &= ~PREAMBLE_PATTERN_SET_CHECKER);
		cam_io_w_mb(read_back_value, csiphybase + lane_offset);
	}
}

static void __cam_csiphy_read_3phase_bist_counter_status(
	struct csiphy_device *csiphy_dev, uint32_t *counter)
{
	int i = 0, lane_count;
	int bist_status_arr_size =
		csiphy_dev->ctrl_reg->csiphy_bist_reg->number_of_counters;
	uint32_t base_offset = 0;
	void __iomem *phy_base = NULL;
	uint32_t val = 0;
	uint32_t offset_betwn_lane = 0;
	struct bist_reg_settings_t *bist_reg = NULL;

	phy_base = csiphy_dev->soc_info.reg_map[0].mem_base;
	bist_reg = csiphy_dev->ctrl_reg->csiphy_bist_reg;
	offset_betwn_lane =
		csiphy_dev->ctrl_reg->csiphy_reg.size_offset_betn_lanes;

	for (i = 0; i < bist_status_arr_size; i++) {
		base_offset = bist_reg->bist_counter_3ph_base_offset + (0x4 * i);
		val = 0;
		for (lane_count = 0; lane_count < CAM_CSIPHY_MAX_CPHY_LANES; lane_count++) {
			CAM_DBG(CAM_CSIPHY, "value to be read from addr: 0x%x is 0x%x",
				base_offset + (lane_count * offset_betwn_lane),
				(cam_io_r(phy_base + base_offset + (lane_count * offset_betwn_lane))));
			val |= ((cam_io_r(phy_base + base_offset + (lane_count * offset_betwn_lane))));
		}

		*counter |= (val << (i * 8));
		CAM_DBG(CAM_CSIPHY, "COUNTER VALUE is 0x%x", *counter);
	}

	return;
}

static void __cam_csiphy_get_3phase_pattern_status(
	struct csiphy_device *csiphy_dev)
{
	int i = 0;
	int bist_status_arr_size =
		csiphy_dev->ctrl_reg->csiphy_bist_reg->num_status_reg;
	struct csiphy_reg_t *csiphy_common_reg = NULL;
	void __iomem *csiphybase = NULL;
	uint32_t base_offset = 0;
	uint32_t lane_offset = 0;
	uint32_t status1 = 0, status0 = 0;
	uint32_t counter = 0;
	uint32_t offset_betwn_lane = 0;
	struct bist_reg_settings_t *bist_reg = NULL;

	CAM_DBG(CAM_CSIPHY, "ENTER");
	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;
	bist_reg = csiphy_dev->ctrl_reg->csiphy_bist_reg;
	base_offset = bist_reg->bist_sensor_data_3ph_status_base_offset;
	offset_betwn_lane =
		csiphy_dev->ctrl_reg->csiphy_reg.size_offset_betn_lanes;

	/* This loop is to read every lane status value
	 * in case if loop breaks with only last lane.
	 */
	for (i = 0; i < bist_status_arr_size; i++) {
		csiphy_common_reg = &bist_reg->bist_status_arr[i];
		switch (csiphy_common_reg->csiphy_param_type) {
		case CSIPHY_3PH_REGS:
			status1 |= cam_io_r(csiphybase + csiphy_common_reg->reg_addr);
		break;
		}
	}

	/* Read Status0 value to detect sensor related communication */
	for (i = 0; i < CAM_CSIPHY_MAX_CPHY_LANES; i++) {
		lane_offset = base_offset + (i * offset_betwn_lane);
		status0 |= cam_io_r(csiphybase + lane_offset);
	}


	/* Read the Counter value for possible corrupted headers */
	__cam_csiphy_read_3phase_bist_counter_status(csiphy_dev, &counter);

	if ((status1 & PREAMBLE_PATTERN_BIST_DONE) &&
		(status1 & bist_reg->error_status_val_3ph)) {
		/**
		 * This condition happen when CSIPHY try to read status after sensor
		 * streamoff. In this case error bit is set due to postamble detection
		 * which is bit(4). In this scenraio this is not consider as an error.
		 * We need to check for status2/3 counter value to determine if there are
		 * more header that is corrupted than 2. Counter always shows value of 2
		 * with postamble packet.
		 *
		 */
		if (counter <= PREAMBLE_MAX_ERR_COUNT_ALLOWED) {
			CAM_INFO(CAM_CSIPHY,
				"PN9 Pattern rxced succesfully after sensor streamoff:: counter value: 0x%x Status1: 0x%x Status0: 0x%x",
				counter, status1, status0);
		} else {
			CAM_INFO(CAM_CSIPHY,
				"PN9 Pattern is corrupted:: counter value: 0x%x Status1: 0x%x Status0: 0x%x",
				counter, status1, status0);
		}
	} else if ((status1 & PREAMBLE_PATTERN_BIST_DONE) &&
		!(status1 & bist_reg->error_status_val_3ph)) {
		/**
		 * This condition happen when CSIPHY try to read status with some counter
		 * value is set to check against. In this case error bit is not expected
		 * to be set.
		 */
		CAM_INFO(CAM_CSIPHY,
			"PN9 Pattern rxced succesfully before sensor streamoff:: counter value: 0x%x Status1: 0x%x Status0: 0x%x",
			counter, status1, status0);
	} else {
		CAM_INFO(CAM_CSIPHY,
			"PN9 Pattern is corrupted:: counter value: 0x%x Status1: 0x%x Status0: 0x%x",
			counter, status1, status0);
	}

	return;
}

static void __cam_csiphy_3ph_status_checker_ops(
	struct csiphy_device *csiphy_dev, bool set)
{
	int i = 0;
	void __iomem *csiphybase = NULL;
	uint32_t base_offset = 0;
	uint32_t read_back_value;
	uint32_t lane_offset = 0;
	uint32_t offset_betwn_lane = 0;

	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;
	base_offset =
		csiphy_dev->ctrl_reg->csiphy_bist_reg->set_status_update_3ph_base_offset;
	offset_betwn_lane =
		csiphy_dev->ctrl_reg->csiphy_reg.size_offset_betn_lanes;
	/* Set checker bit to read the correct status1 value */
	for (i = 0; i < CAM_CSIPHY_MAX_CPHY_LANES; i++) {
		lane_offset = base_offset + (i * offset_betwn_lane);
		read_back_value = cam_io_r(csiphybase + lane_offset);
		set ? (read_back_value |= PREAMBLE_PATTERN_SET_CHECKER) :
			(read_back_value &= ~PREAMBLE_PATTERN_SET_CHECKER);
		cam_io_w_mb(read_back_value, csiphybase + lane_offset);
	}
}

static void __cam_csiphy_get_preamble_status(
	struct csiphy_device *csiphy_dev, int offset)
{
	bool is_3phase = false;

	is_3phase = csiphy_dev->csiphy_info[offset].csiphy_3phase;

	if (is_3phase) {
		__cam_csiphy_3ph_status_checker_ops(csiphy_dev, true);
		__cam_csiphy_get_3phase_pattern_status(csiphy_dev);
		__cam_csiphy_3ph_status_checker_ops(csiphy_dev, false);
	} else {
		__cam_csiphy_2ph_status_checker_ops(csiphy_dev, true);
		__cam_csiphy_get_2phase_pattern_status(csiphy_dev);
		__cam_csiphy_2ph_status_checker_ops(csiphy_dev, false);
	}
	return;
}

int32_t cam_csiphy_core_cfg(void *phy_dev,
			void *arg)
{
	struct cam_control   *cmd = (struct cam_control *)arg;
	struct csiphy_device *csiphy_dev = (struct csiphy_device *)phy_dev;
	struct cam_cphy_dphy_status_reg_params_t *status_reg_ptr;
	struct csiphy_reg_parms_t *csiphy_reg;
	struct cam_hw_soc_info *soc_info;
	uint32_t      cphy_trio_status;
	void __iomem *csiphybase;
	int32_t              rc = 0;
	uint32_t             i;

	if (!csiphy_dev || !cmd) {
		CAM_ERR(CAM_CSIPHY, "Invalid input args");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_CSIPHY, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	soc_info = &csiphy_dev->soc_info;
	if (!soc_info) {
		CAM_ERR(CAM_CSIPHY, "Null Soc_info");
		return -EINVAL;
	}

	if (!g_phy_data[soc_info->index].base_address) {
		CAM_ERR(CAM_CSIPHY, "CSIPHY hw is not avaialble at index: %d",
			soc_info->index);
		return -EINVAL;
	}

	csiphybase = soc_info->reg_map[0].mem_base;
	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;
	status_reg_ptr = csiphy_reg->status_reg_params;
	CAM_DBG(CAM_CSIPHY, "Opcode received: %d", cmd->op_code);
	mutex_lock(&csiphy_dev->mutex);
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev csiphy_acq_dev;
		struct cam_csiphy_acquire_dev_info csiphy_acq_params;
		int index;
		struct cam_create_dev_hdl bridge_params;

		CAM_DBG(CAM_CSIPHY, "ACQUIRE_CNT: %d COMBO_MODE: %d",
			csiphy_dev->acquire_count,
			csiphy_dev->combo_mode);
		if ((csiphy_dev->csiphy_state == CAM_CSIPHY_START) &&
			(csiphy_dev->combo_mode == 0) &&
			(csiphy_dev->acquire_count > 0)) {
			CAM_ERR(CAM_CSIPHY,
				"NonComboMode does not support multiple acquire: Acquire_count: %d",
				csiphy_dev->acquire_count);
			rc = -EINVAL;
			goto release_mutex;
		}

		if ((csiphy_dev->acquire_count) &&
			(csiphy_dev->acquire_count >=
			csiphy_dev->session_max_device_support)) {
			CAM_ERR(CAM_CSIPHY,
				"Max acquires are allowed in combo mode: %d",
				csiphy_dev->session_max_device_support);
			rc = -EINVAL;
			goto release_mutex;
		}

		rc = copy_from_user(&csiphy_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(csiphy_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			goto release_mutex;
		}

		csiphy_acq_params.combo_mode = 0;

		if (copy_from_user(&csiphy_acq_params,
			u64_to_user_ptr(csiphy_acq_dev.info_handle),
			sizeof(csiphy_acq_params))) {
			CAM_ERR(CAM_CSIPHY,
				"Failed copying from User");
			goto release_mutex;
		}

		if (csiphy_acq_params.combo_mode &&
			csiphy_acq_params.cphy_dphy_combo_mode) {
			CAM_ERR(CAM_CSIPHY,
				"Cannot support both Combo_mode and cphy_dphy_combo_mode");
			rc = -EINVAL;
			goto release_mutex;
		}

		if (csiphy_acq_params.combo_mode == 1) {
			CAM_DBG(CAM_CSIPHY, "combo mode stream detected");
			csiphy_dev->combo_mode = 1;
			if (csiphy_acq_params.csiphy_3phase) {
				CAM_DBG(CAM_CSIPHY, "3Phase ComboMode");
				csiphy_dev->session_max_device_support =
					CSIPHY_MAX_INSTANCES_PER_PHY;
			} else {
				csiphy_dev->session_max_device_support =
					CSIPHY_MAX_INSTANCES_PER_PHY - 1;
				CAM_DBG(CAM_CSIPHY, "2Phase ComboMode");
			}
		}
		if (csiphy_acq_params.cphy_dphy_combo_mode == 1) {
			CAM_DBG(CAM_CSIPHY,
				"cphy_dphy_combo_mode stream detected");
			csiphy_dev->cphy_dphy_combo_mode = 1;
			csiphy_dev->session_max_device_support =
				CSIPHY_MAX_INSTANCES_PER_PHY - 1;
		}

		if (!csiphy_acq_params.combo_mode &&
			!csiphy_acq_params.cphy_dphy_combo_mode) {
			CAM_DBG(CAM_CSIPHY, "Non Combo Mode stream");
			csiphy_dev->session_max_device_support = 1;
		}

		bridge_params.ops = NULL;
		bridge_params.session_hdl = csiphy_acq_dev.session_handle;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = csiphy_dev;
		bridge_params.dev_id = CAM_CSIPHY;
		index = csiphy_dev->acquire_count;
		csiphy_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		csiphy_dev->csiphy_info[index].hdl_data.device_hdl =
			csiphy_acq_dev.device_handle;
		csiphy_dev->csiphy_info[index].hdl_data.session_hdl =
			csiphy_acq_dev.session_handle;
		csiphy_dev->csiphy_info[index].csiphy_3phase =
			csiphy_acq_params.csiphy_3phase;

		CAM_DBG(CAM_CSIPHY, "Add dev_handle:0x%x at index: %d ",
			csiphy_dev->csiphy_info[index].hdl_data.device_hdl,
			index);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
				&csiphy_acq_dev,
				sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			rc = -EINVAL;
			goto release_mutex;
		}

		if (!csiphy_dev->acquire_count) {
			g_phy_data[soc_info->index].is_3phase = csiphy_acq_params.csiphy_3phase;
			CAM_DBG(CAM_CSIPHY,
					"g_csiphy data is updated for index: %d is_3phase: %u",
					soc_info->index,
					g_phy_data[soc_info->index].is_3phase);
		}

		if (g_phy_data[soc_info->index].enable_aon_support) {
			rc = cam_csiphy_util_update_aon_ops(true, soc_info->index);
			if (rc) {
				CAM_ERR(CAM_CSIPHY,
					"Error in setting up AON operation for phy_idx: %d, rc: %d",
					soc_info->index, rc);
				goto release_mutex;
			}
		}

		csiphy_dev->acquire_count++;

		if (csiphy_dev->csiphy_state == CAM_CSIPHY_INIT)
			csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;

		CAM_INFO(CAM_CSIPHY,
			"CAM_ACQUIRE_DEV: %u Type: %s acquire_count: %d combo: %u cphy+dphy combo: %u",
			soc_info->index,
			g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY",
			csiphy_dev->acquire_count,
			csiphy_dev->combo_mode,
			csiphy_dev->cphy_dphy_combo_mode);
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_csiphy_query_cap csiphy_cap = {0};

		cam_csiphy_query_cap(csiphy_dev, &csiphy_cap);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&csiphy_cap, sizeof(struct cam_csiphy_query_cap))) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			rc = -EINVAL;
			goto release_mutex;
		}
	}
		break;
	case CAM_STOP_DEV: {
		int32_t offset, rc = 0;
		struct cam_start_stop_dev_cmd config;

		rc = copy_from_user(&config, (void __user *)cmd->handle,
					sizeof(config));
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			goto release_mutex;
		}

		if (csiphy_dev->csiphy_state != CAM_CSIPHY_START) {
			CAM_ERR(CAM_CSIPHY, "Csiphy:%d Not in right state to stop : %d",
				soc_info->index, csiphy_dev->csiphy_state);
			goto release_mutex;
		}

		offset = cam_csiphy_get_instance_offset(csiphy_dev,
			config.dev_handle);
		if (offset < 0 ||
			offset >= csiphy_dev->session_max_device_support) {
			CAM_ERR(CAM_CSIPHY, "Index is invalid: %d", offset);
			goto release_mutex;
		}

		if (--csiphy_dev->start_dev_count) {
			if (csiphy_dev->csiphy_info[offset].secure_mode)
				cam_csiphy_notify_secure_mode(
					csiphy_dev,
					CAM_SECURE_MODE_NON_SECURE, offset);

			csiphy_dev->csiphy_info[offset].secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
			csiphy_dev->csiphy_info[offset].csiphy_cpas_cp_reg_mask
				= 0;

			cam_csiphy_update_lane(csiphy_dev, offset, false);

			CAM_INFO(CAM_CSIPHY,
				"CAM_STOP_PHYDEV: %d, Type: %s, dev_cnt: %u, slot: %d, Datarate: %llu, Settletime: %llu",
				soc_info->index,
				g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY",
				csiphy_dev->start_dev_count,
				offset,
				csiphy_dev->csiphy_info[offset].data_rate,
				csiphy_dev->csiphy_info[offset].settle_time);

			goto release_mutex;
		}

		if (csiphy_dev->csiphy_info[offset].secure_mode)
			cam_csiphy_notify_secure_mode(
				csiphy_dev,
				CAM_SECURE_MODE_NON_SECURE, offset);

		csiphy_dev->csiphy_info[offset].secure_mode =
			CAM_SECURE_MODE_NON_SECURE;

		csiphy_dev->csiphy_info[offset].csiphy_cpas_cp_reg_mask = 0x0;

		if (csiphy_dev->prgm_cmn_reg_across_csiphy) {
			mutex_lock(&active_csiphy_cnt_mutex);
			active_csiphy_hw_cnt--;
			mutex_unlock(&active_csiphy_cnt_mutex);

			cam_csiphy_prgm_cmn_data(csiphy_dev, true);
		}

		if (csiphy_dev->preamble_enable)
			__cam_csiphy_get_preamble_status(csiphy_dev, offset);

		rc = cam_csiphy_disable_hw(csiphy_dev);
		if (rc < 0)
			CAM_ERR(CAM_CSIPHY, "Failed in csiphy release");

		if (cam_csiphy_cpas_ops(csiphy_dev->cpas_handle, false)) {
			CAM_ERR(CAM_CSIPHY, "Failed in de-voting CPAS");
			rc = -EFAULT;
		}

		csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;

		CAM_INFO(CAM_CSIPHY,
			"CAM_STOP_PHYDEV: %u, Type: %s, slot: %d, Datarate: %llu, Settletime: %llu",
			soc_info->index,
			g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY",
			offset,
			csiphy_dev->csiphy_info[offset].data_rate,
			csiphy_dev->csiphy_info[offset].settle_time);
	}
		break;
	case CAM_RELEASE_DEV: {
		int32_t offset;
		struct cam_release_dev_cmd release;

		if (!csiphy_dev->acquire_count) {
			CAM_ERR(CAM_CSIPHY, "No valid devices to release");
			rc = -EINVAL;
			goto release_mutex;
		}

		if (copy_from_user(&release,
			u64_to_user_ptr(cmd->handle),
			sizeof(release))) {
			rc = -EFAULT;
			goto release_mutex;
		}

		offset = cam_csiphy_get_instance_offset(csiphy_dev,
			release.dev_handle);
		if (offset < 0 ||
			offset >= csiphy_dev->session_max_device_support) {
			CAM_ERR(CAM_CSIPHY, "index is invalid: %d", offset);
			goto release_mutex;
		}

		if (csiphy_dev->csiphy_info[offset].secure_mode)
			cam_csiphy_notify_secure_mode(
				csiphy_dev,
				CAM_SECURE_MODE_NON_SECURE, offset);

		csiphy_dev->csiphy_info[offset].secure_mode =
			CAM_SECURE_MODE_NON_SECURE;

		csiphy_dev->csiphy_cpas_cp_reg_mask[offset] = 0x0;
		csiphy_dev->preamble_enable = 0;

		rc = cam_destroy_device_hdl(release.dev_handle);
		if (rc < 0)
			CAM_ERR(CAM_CSIPHY, "destroying the device hdl");
		csiphy_dev->csiphy_info[offset].hdl_data.device_hdl = -1;
		csiphy_dev->csiphy_info[offset].hdl_data.session_hdl = -1;

		cam_csiphy_reset_phyconfig_param(csiphy_dev, offset);

		if (csiphy_dev->acquire_count) {
			csiphy_dev->acquire_count--;
			CAM_DBG(CAM_CSIPHY, "Acquire_cnt: %d",
				csiphy_dev->acquire_count);
		}

		if (csiphy_dev->acquire_count == 0) {
			CAM_DBG(CAM_CSIPHY, "All PHY devices released");
			if (g_phy_data[soc_info->index].enable_aon_support) {
				rc = cam_csiphy_util_update_aon_ops(false, soc_info->index);
				if (rc) {
					CAM_WARN(CAM_CSIPHY,
						"Error in releasing AON operation for phy_idx: %d, rc: %d",
						csiphy_dev->soc_info.index, rc);
					rc = 0;
				}
			}
			csiphy_dev->combo_mode = 0;
			csiphy_dev->csiphy_state = CAM_CSIPHY_INIT;
		}

		CAM_DBG(CAM_CSIPHY, "CAM_RELEASE_PHYDEV: %u Type: %s",
			soc_info->index,
			g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY");

		break;
	}
	case CAM_CONFIG_DEV: {
		struct cam_config_dev_cmd config;

		CAM_DBG(CAM_CSIPHY, "CONFIG_DEV Called");
		if (copy_from_user(&config,
			u64_to_user_ptr(cmd->handle),
					sizeof(config))) {
			rc = -EFAULT;
		} else {
			rc = cam_cmd_buf_parser(csiphy_dev, &config);
			if (rc < 0) {
				CAM_ERR(CAM_CSIPHY, "Fail in cmd buf parser");
				goto release_mutex;
			}
		}
		break;
	}
	case CAM_START_DEV: {
		struct cam_start_stop_dev_cmd config;
		int32_t offset;
		int clk_vote_level = -1;

		CAM_DBG(CAM_CSIPHY, "START_DEV Called");
		rc = copy_from_user(&config, (void __user *)cmd->handle,
			sizeof(config));
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			goto release_mutex;
		}

		if ((csiphy_dev->csiphy_state == CAM_CSIPHY_START) &&
			(csiphy_dev->start_dev_count >
			csiphy_dev->session_max_device_support)) {
			CAM_ERR(CAM_CSIPHY,
				"Invalid start count: %d, Max supported devices: %u",
				csiphy_dev->start_dev_count,
				csiphy_dev->session_max_device_support);
			rc = -EINVAL;
			goto release_mutex;
		}

		offset = cam_csiphy_get_instance_offset(csiphy_dev,
			config.dev_handle);
		if (offset < 0 ||
			offset >= csiphy_dev->session_max_device_support) {
			CAM_ERR(CAM_CSIPHY, "index is invalid: %d", offset);
			goto release_mutex;
		}

		if (csiphy_dev->start_dev_count) {
			clk_vote_level =
				csiphy_dev->ctrl_reg->getclockvoting(
					csiphy_dev, offset);
			rc = cam_soc_util_set_clk_rate_level(
				&csiphy_dev->soc_info, clk_vote_level, false);
			if (rc) {
				CAM_WARN(CAM_CSIPHY,
					"Failed to set the clk_rate level: %d",
					clk_vote_level);
				rc = 0;
			}

			if (csiphy_dev->csiphy_info[offset].secure_mode == 1) {
				if (!cam_cpas_is_feature_supported(
					CAM_CPAS_SECURE_CAMERA_ENABLE,
					CAM_CPAS_HW_IDX_ANY, NULL)) {
					CAM_ERR(CAM_CSIPHY,
						"sec_cam: camera fuse bit not set");
					goto release_mutex;
				}

				rc = cam_csiphy_notify_secure_mode(csiphy_dev,
					CAM_SECURE_MODE_SECURE, offset);
				if (rc < 0) {
					csiphy_dev->csiphy_info[offset]
						.secure_mode =
						CAM_SECURE_MODE_NON_SECURE;
					CAM_ERR(CAM_CSIPHY,
						"sec_cam: notify failed: rc: %d",
						rc);
					goto release_mutex;
				}
			}

			if (csiphy_dev->csiphy_info[offset].csiphy_3phase) {
				rc = cam_csiphy_cphy_data_rate_config(
					csiphy_dev, offset);
				if (rc) {
					CAM_ERR(CAM_CSIPHY,
						"Data rate specific configuration failed rc: %d",
						rc);
					goto release_mutex;
				}
			}

			rc = cam_csiphy_update_lane(csiphy_dev, offset, true);
			if (rc) {
				CAM_ERR(CAM_CSIPHY,
					"Update enable lane failed, rc: %d", rc);
				goto release_mutex;
			}

			if (csiphy_dev->en_full_phy_reg_dump)
				cam_csiphy_reg_dump(&csiphy_dev->soc_info);

			csiphy_dev->start_dev_count++;

			CAM_INFO(CAM_CSIPHY,
				"CAM_START_PHYDEV: %d, Type: %s, dev_cnt: %u, slot: %d, combo: %u, cphy+dphy: %u, sec_mode: %d, Datarate: %llu, Settletime: %llu",
				soc_info->index,
				g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY",
				csiphy_dev->start_dev_count,
				offset,
				csiphy_dev->combo_mode,
				csiphy_dev->cphy_dphy_combo_mode,
				csiphy_dev->csiphy_info[offset].secure_mode,
				csiphy_dev->csiphy_info[offset].data_rate,
				csiphy_dev->csiphy_info[offset].settle_time);

			goto release_mutex;
		}

		rc = cam_csiphy_cpas_ops(csiphy_dev->cpas_handle, true);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "voting CPAS: %d", rc);
			goto release_mutex;
		}

		if (csiphy_dev->csiphy_info[offset].secure_mode == 1) {
			if (!cam_cpas_is_feature_supported(
					CAM_CPAS_SECURE_CAMERA_ENABLE,
					CAM_CPAS_HW_IDX_ANY, NULL)) {
				CAM_ERR(CAM_CSIPHY,
					"sec_cam: camera fuse bit not set");
				rc = -EINVAL;
				goto cpas_stop;
			}

			rc = cam_csiphy_notify_secure_mode(
				csiphy_dev,
				CAM_SECURE_MODE_SECURE, offset);
			if (rc < 0) {
				csiphy_dev->csiphy_info[offset].secure_mode =
					CAM_SECURE_MODE_NON_SECURE;
				goto cpas_stop;
			}
		}

		rc = cam_csiphy_enable_hw(csiphy_dev, offset);
		if (rc != 0) {
			CAM_ERR(CAM_CSIPHY, "cam_csiphy_enable_hw failed");
			goto cpas_stop;
		}

		if (csiphy_dev->prgm_cmn_reg_across_csiphy) {
			cam_csiphy_prgm_cmn_data(csiphy_dev, false);

			mutex_lock(&active_csiphy_cnt_mutex);
			active_csiphy_hw_cnt++;
			mutex_unlock(&active_csiphy_cnt_mutex);
		}

		rc = cam_csiphy_config_dev(csiphy_dev, config.dev_handle);
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "cam_csiphy_config_dev failed");
			cam_csiphy_disable_hw(csiphy_dev);
			goto hw_cnt_decrement;
		}

		if (csiphy_onthego_reg_count)
			cam_csiphy_apply_onthego_reg_values(csiphybase, soc_info->index);

		cam_csiphy_release_from_reset_state(csiphy_dev, csiphybase, offset);

		if (g_phy_data[soc_info->index].is_3phase && status_reg_ptr) {
			for (i = 0; i < CAM_CSIPHY_MAX_CPHY_LANES; i++) {
				if (status_reg_ptr->cphy_lane_status[i]) {
					cphy_trio_status = cam_io_r_mb(csiphybase +
						status_reg_ptr->cphy_lane_status[i]);

					cphy_trio_status &= 0x1F;
					if (cphy_trio_status == 0 || cphy_trio_status == 8) {
						CAM_DBG(CAM_CSIPHY,
							"Reg_offset: 0x%x, cphy_trio%d_status = 0x%x",
							status_reg_ptr->cphy_lane_status[i],
							i, cphy_trio_status);
					} else {
						CAM_WARN(CAM_CSIPHY,
							"Reg_offset: 0x%x, Cphy_trio%d_status = 0x%x",
							status_reg_ptr->cphy_lane_status[i],
							i, cphy_trio_status);
					}
				}
			}
		}

		if (csiphy_dev->en_full_phy_reg_dump)
			cam_csiphy_reg_dump(&csiphy_dev->soc_info);

		if (csiphy_dev->en_lane_status_reg_dump) {
			usleep_range(50000, 50005);
			CAM_INFO(CAM_CSIPHY, "Status Reg Dump after config");
			cam_csiphy_dump_status_reg(csiphy_dev);
		}

		csiphy_dev->start_dev_count++;
		csiphy_dev->csiphy_state = CAM_CSIPHY_START;

		CAM_INFO(CAM_CSIPHY,
			"CAM_START_PHYDEV: %d, Type: %s, slot: %d, sec_mode: %d, Datarate: %llu, Settletime: %llu, combo: %u, cphy+dphy: %u",
			soc_info->index,
			g_phy_data[soc_info->index].is_3phase ? "CPHY" : "DPHY",
			offset,
			csiphy_dev->csiphy_info[offset].secure_mode,
			csiphy_dev->csiphy_info[offset].data_rate,
			csiphy_dev->csiphy_info[offset].settle_time,
			csiphy_dev->combo_mode,
			csiphy_dev->cphy_dphy_combo_mode);
	}
		break;
	case CAM_CONFIG_DEV_EXTERNAL: {
		struct cam_config_dev_cmd submit_cmd;

		if (copy_from_user(&submit_cmd,
			u64_to_user_ptr(cmd->handle),
			sizeof(struct cam_config_dev_cmd))) {
			CAM_ERR(CAM_CSIPHY, "failed copy config ext\n");
			rc = -EFAULT;
			goto release_mutex;
		} else {
			rc = cam_csiphy_external_cmd(csiphy_dev, &submit_cmd);
			if (rc) {
				CAM_ERR(CAM_CSIPHY,
					"exteranal command configuration failed rc: %d",
					rc);
				goto release_mutex;
			}
		}
		break;
	}
	default:
		CAM_ERR(CAM_CSIPHY, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

	mutex_unlock(&csiphy_dev->mutex);
	return rc;

hw_cnt_decrement:
	if (csiphy_dev->prgm_cmn_reg_across_csiphy) {
		mutex_lock(&active_csiphy_cnt_mutex);
		active_csiphy_hw_cnt--;
		mutex_unlock(&active_csiphy_cnt_mutex);
	}

cpas_stop:
	if (cam_csiphy_cpas_ops(csiphy_dev->cpas_handle, false))
		CAM_ERR(CAM_CSIPHY, "cpas stop failed");
release_mutex:
	mutex_unlock(&csiphy_dev->mutex);

	return rc;
}

int cam_csiphy_register_baseaddress(struct csiphy_device *csiphy_dev)
{
	int phy_idx;

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Data is NULL");
		return -EINVAL;
	}

	if (csiphy_dev->soc_info.index >= MAX_CSIPHY) {
		CAM_ERR(CAM_CSIPHY, "Invalid soc index: %u Max soc index: %u",
			csiphy_dev->soc_info.index, MAX_CSIPHY);
		return -EINVAL;
	}

	phy_idx = csiphy_dev->soc_info.index;
	g_phy_data[phy_idx].base_address =
		csiphy_dev->soc_info.reg_map[0].mem_base;
	g_phy_data[phy_idx].cpas_handle =
		csiphy_dev->cpas_handle;
	g_phy_data[phy_idx].aon_sel_param =
		csiphy_dev->ctrl_reg->csiphy_reg.aon_sel_params;
	g_phy_data[phy_idx].enable_aon_support = false;

	return 0;
}
