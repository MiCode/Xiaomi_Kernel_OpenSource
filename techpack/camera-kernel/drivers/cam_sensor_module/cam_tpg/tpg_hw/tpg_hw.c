// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "tpg_hw.h"

#define BYTES_PER_REGISTER           4
#define NUM_REGISTER_PER_LINE        4
#define REG_OFFSET(__start, __i)    ((__start) + ((__i) * BYTES_PER_REGISTER))

static int cam_io_tpg_dump(void __iomem *base_addr,
	uint32_t start_offset, int size)
{
	char          line_str[128];
	char         *p_str;
	int           i;
	uint32_t      data;

	CAM_DBG(CAM_TPG, "addr=%pK offset=0x%x size=%d",
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
		data = cam_io_r(base_addr + REG_OFFSET(start_offset, i));
		snprintf(p_str, 9, "%08x ", data);
		p_str += 8;
		if ((i + 1) % NUM_REGISTER_PER_LINE == 0) {
			CAM_DBG(CAM_TPG, "%s", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CAM_ERR(CAM_TPG, "%s", line_str);

	return 0;
}

int32_t cam_tpg_mem_dmp(struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0;
	resource_size_t size = 0;
	void __iomem *addr = NULL;

	if (!soc_info) {
		rc = -EINVAL;
		CAM_ERR(CAM_TPG, "invalid input %d", rc);
		return rc;
	}
	addr = soc_info->reg_map[0].mem_base;
	size = resource_size(soc_info->mem_block[0]);
	rc = cam_io_tpg_dump(addr, 0, (size >> 2));
	if (rc < 0) {
		CAM_ERR(CAM_TPG, "generating dump failed %d", rc);
	}
	return rc;
}


#define __TPG_DEBUG_DUMP__
#ifdef __TPG_DEBUG_DUMP__
static const char * const tpg_phy_type_strings[] = {
	"TPG_PHY_TYPE_INVALID",
	"TPG_PHY_TYPE_DPHY",
	"TPG_PHY_TYPE_CPHY",
	"TPG_PHY_TYPE_MAX"
};

static const char * const tpg_interleaving_format_string[] = {
	"TPG_INTERLEAVING_FORMAT_INVALID",
	"TPG_INTERLEAVING_FORMAT_FRAME",
	"TPG_INTERLEAVING_FORMAT_LINE",
	"TPG_INTERLEAVING_FORMAT_MAX"
};

static const char * const tpg_shutter_type_strings[] = {
	"TPG_SHUTTER_TYPE_INVALID",
	"TPG_SHUTTER_TYPE_ROLLING",
	"TPG_SHUTTER_TYPE_GLOBAL",
	"TPG_SHUTTER_TYPE_MAX"
};

static const char *const tpg_pattern_type_strings[] = {
	"TPG_PATTERN_INVALID",
	"TPG_PATTERN_REAL_IMAGE",
	"TPG_PATTERN_RANDOM_PIXL",
	"TPG_PATTERN_RANDOM_INCREMENTING_PIXEL",
	"TPG_PATTERN_COLOR_BAR",
	"TPG_PATTERN_ALTERNATING_55_AA",
	"TPG_PATTERN_ALTERNATING_USER_DEFINED",
	"TPG_PATTERN_MAX"
};

static const char *const tpg_color_bar_mode_strings[] = {
	"TPG_COLOR_BAR_MODE_INVALID",
	"TPG_COLOR_BAR_MODE_NORMAL",
	"TPG_COLOR_BAR_MODE_SPLIT",
	"TPG_COLOR_BAR_MODE_ROTATING",
	"TPG_COLOR_BAR_MODE_MAX"
};

static const char *const tpg_stream_type_strings[] = {
	"TPG_STREAM_TYPE_INVALID",
	"TPG_STREAM_TYPE_IMAGE",
	"TPG_STREAM_TYPE_PDAF",
	"TPG_STREAM_TYPE_META",
	"TPG_STREAM_TYPE_MAX"
};

static const char *const tpg_image_format_type_strings[] = {
	"TPG_IMAGE_FORMAT_INVALID",
	"TPG_IMAGE_FORMAT_BAYER",
	"TPG_IMAGE_FORMAT_QCFA",
	"TPG_IMAGE_FORMAT_YUV",
	"TPG_IMAGE_FORMAT_JPEG",
	"TPG_IMAGE_FORMAT_MAX"
};
#endif

int dump_global_configs(int idx,
		struct tpg_global_config_t *global)
{
#ifdef __TPG_DEBUG_DUMP__
	CAM_DBG(CAM_TPG, "TPG[%d] phy_type            : %s",
			idx,
			tpg_phy_type_strings[global->phy_type]);
	CAM_DBG(CAM_TPG, "TPG[%d] lane_count          : %d",
			idx,
			global->lane_count);
	CAM_DBG(CAM_TPG, "TPG[%d] interleaving_format : %s",
			idx,
			tpg_interleaving_format_string[global->interleaving_format]);
	CAM_DBG(CAM_TPG, "TPG[%d] phy_mode            : %d",
			idx,
			global->phy_mode);
	CAM_DBG(CAM_TPG, "TPG[%d] shutter_type        : %s",
			idx,
			tpg_shutter_type_strings[global->shutter_type]);
	CAM_DBG(CAM_TPG, "TPG[%d] skip pattern        : 0x%x",
			idx,
			global->skip_pattern);
	CAM_DBG(CAM_TPG, "TPG[%d] tpg clock           : %d",
			idx,
			global->tpg_clock);
#endif
	return 0;
}

int dump_stream_configs(int hw_idx,
		int stream_idx,
		struct tpg_stream_config_t *stream)
{
#ifdef __TPG_DEBUG_DUMP__
	CAM_DBG(CAM_TPG, "TPG[%d][%d] pattern_type    : %s",
			hw_idx,
			stream_idx,
			tpg_pattern_type_strings[stream->pattern_type]);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] cb_mode         : %s",
			hw_idx,
			stream_idx,
			tpg_color_bar_mode_strings[stream->cb_mode]);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] frame_count     : %d",
			hw_idx,
			stream_idx,
			stream->frame_count);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] stream_type     : %s",
			hw_idx,
			stream_idx,
			tpg_stream_type_strings[stream->stream_type]);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] left            : %d",
			hw_idx,
			stream_idx,
			stream->stream_dimension.left);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] top             : %d",
			hw_idx,
			stream_idx,
			stream->stream_dimension.top);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] width           : %d",
			hw_idx,
			stream_idx,
			stream->stream_dimension.width);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] height          : %d",
			hw_idx,
			stream_idx,
			stream->stream_dimension.height);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] pixel_depth     : %d",
			hw_idx,
			stream_idx,
			stream->pixel_depth);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] cfa_arrangement : %d",
			hw_idx,
			stream_idx,
			stream->cfa_arrangement);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] output_format   : %s",
			hw_idx,
			stream_idx,
		tpg_image_format_type_strings[stream->output_format]);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] vc              : 0x%x",
			hw_idx,
			stream_idx,
			stream->vc);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] dt              : 0x%x",
			hw_idx,
			stream_idx,
			stream->dt);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] hbi             : %d",
			hw_idx,
			stream_idx,
			stream->hbi);
	CAM_DBG(CAM_TPG, "TPG[%d][%d] vbi             : %d",
			hw_idx,
			stream_idx,
			stream->vbi);
#endif
	return 0;
}


static int tpg_hw_soc_disable(struct tpg_hw *hw)
{
	int rc = 0;

	if (!hw || !hw->soc_info) {
		CAM_ERR(CAM_TPG, "Error Invalid params");
		return -EINVAL;
	}

	rc = cam_soc_util_disable_platform_resource(hw->soc_info, true, false);
	if (rc)
		CAM_ERR(CAM_TPG, "TPG[%d] Disable platform failed %d",
				hw->hw_idx, rc);

	if ((rc = cam_cpas_stop(hw->cpas_handle))) {
		CAM_ERR(CAM_TPG, "TPG[%d] CPAS stop failed",
				hw->hw_idx);
	} else {
		hw->state = TPG_HW_STATE_HW_DISABLED;
	}

	return rc;
}

static int tpg_hw_soc_enable(
	struct tpg_hw *hw,
	enum cam_vote_level clk_level)
{
	int rc = 0;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_WRITE;

	axi_vote.axi_path[0].camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ib_bw = CAM_CPAS_DEFAULT_AXI_BW;

	CAM_DBG(CAM_TPG, "TPG[%d] camnoc_bw:%lld mnoc_ab_bw:%lld mnoc_ib_bw:%lld ",
		hw->hw_idx,
		axi_vote.axi_path[0].camnoc_bw,
		axi_vote.axi_path[0].mnoc_ab_bw,
		axi_vote.axi_path[0].mnoc_ib_bw);

	rc = cam_cpas_start(hw->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] CPAS start failed",
				hw->hw_idx);
		rc = -EFAULT;
		goto end;
	}

	rc = cam_soc_util_enable_platform_resource(hw->soc_info, true,
		clk_level, false);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] enable platform failed",
				hw->hw_idx);
		goto stop_cpas;
	}
	hw->state = TPG_HW_STATE_HW_ENABLED;

	return rc;
stop_cpas:
	cam_cpas_stop(hw->cpas_handle);
end:
	return rc;
}

static int tpg_hw_start_default_new(struct tpg_hw *hw)
{
	int i = 0;
	uint32_t stream_idx = 0;
	int num_vcs = 0;
	struct global_config_args globalargs = {0};
	if (!hw ||
		!hw->hw_info ||
		!hw->hw_info->ops ||
		!hw->hw_info->ops->process_cmd) {
		CAM_ERR(CAM_TPG, "Invalid argument");
		return -EINVAL;
	}

	dump_global_configs(hw->hw_idx, &hw->global_config);
	for(i = 0; i < hw->hw_info->max_vc_channels; i++) {
		int dt_slot = 0;
		struct vc_config_args vc_config = {0};
		struct list_head *pos = NULL, *pos_next = NULL;
		struct tpg_hw_stream *entry = NULL, *vc_stream_entry = NULL;

		if (hw->vc_slots[i].vc == -1)
			break;
		num_vcs++;
		vc_config.vc_slot = i;
		vc_config.num_dts = hw->vc_slots[i].stream_count;
		vc_stream_entry = list_first_entry(&hw->vc_slots[i].head,
			struct tpg_hw_stream, list);
		vc_config.stream  = &vc_stream_entry->stream;
		hw->hw_info->ops->process_cmd(hw,
				TPG_CONFIG_VC, &vc_config);

		list_for_each_safe(pos, pos_next, &hw->vc_slots[i].head) {
			struct dt_config_args dt_config = {0};
			entry = list_entry(pos, struct tpg_hw_stream, list);
			dump_stream_configs(hw->hw_idx,
				stream_idx++,
				&entry->stream);
			dt_config.vc_slot = i;
			dt_config.dt_slot = dt_slot++;
			dt_config.stream  = &entry->stream;
			hw->hw_info->ops->process_cmd(hw, TPG_CONFIG_DT, &dt_config);
		}
	}

	globalargs.num_vcs      = num_vcs;
	globalargs.globalconfig = &hw->global_config;
	hw->hw_info->ops->process_cmd(hw,
		TPG_CONFIG_CTRL, &globalargs);

	return 0;
}

int tpg_hw_dump_status(struct tpg_hw *hw)
{
	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_3:
		if (hw->hw_info->ops->dump_status)
			hw->hw_info->ops->dump_status(hw, NULL);
	default:
		CAM_WARN(CAM_TPG, "Hw version doesn't support status dump");
		break;
	}
	return 0;
}

int tpg_hw_start(struct tpg_hw *hw)
{
	int rc = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	mutex_lock(&hw->mutex);
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_0:
	case TPG_HW_VERSION_1_1:
		if (hw->hw_info->ops->start)
			hw->hw_info->ops->start(hw, NULL);
		break;
	case TPG_HW_VERSION_1_2:
	case TPG_HW_VERSION_1_3:
		if (hw->hw_info->ops->start)
			hw->hw_info->ops->start(hw, NULL);
		tpg_hw_start_default_new(hw);
		cam_tpg_mem_dmp(hw->soc_info);
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported HW Version",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&hw->mutex);
	return rc;
}

int tpg_hw_stop(struct tpg_hw *hw)
{
	int rc = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	mutex_lock(&hw->mutex);
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_0:
	case TPG_HW_VERSION_1_1:
	case TPG_HW_VERSION_1_2:
	case TPG_HW_VERSION_1_3:
		if (hw->hw_info->ops->stop)
			rc = hw->hw_info->ops->stop(hw, NULL);
		rc = tpg_hw_soc_disable(hw);
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported HW Version",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&hw->mutex);

	return rc;
}

int tpg_hw_acquire(struct tpg_hw *hw,
		struct tpg_hw_acquire_args *acquire)
{
	int rc = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;

	mutex_lock(&hw->mutex);
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_0:
	case TPG_HW_VERSION_1_1:
	case TPG_HW_VERSION_1_2:
	case TPG_HW_VERSION_1_3:
		// Start Cpas and enable required clocks
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported HW Version",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&hw->mutex);
	return rc;
}

int tpg_hw_release(struct tpg_hw *hw)
{
	int rc = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	mutex_lock(&hw->mutex);
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_0:
	case TPG_HW_VERSION_1_1:
	case TPG_HW_VERSION_1_2:
	case TPG_HW_VERSION_1_3:
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported HW Version",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&hw->mutex);
	return rc;
}

enum cam_vote_level get_tpg_clk_level(
		struct tpg_hw *hw)
{
	enum cam_vote_level cam_vote_level_index = 0;
	enum cam_vote_level last_valid_vote      = 0;
	uint64_t clk                             = 0;
	struct cam_hw_soc_info *soc_info         = NULL;

	soc_info = hw->soc_info;
	clk = hw->global_config.tpg_clock;

	for (cam_vote_level_index = 0;
			cam_vote_level_index < CAM_MAX_VOTE; cam_vote_level_index++) {
		if (soc_info->clk_level_valid[cam_vote_level_index] != true)
			continue;

		if (soc_info->clk_rate[cam_vote_level_index]
			[soc_info->src_clk_idx] >= clk) {
			CAM_INFO(CAM_TPG,
				"match detected %s : %llu:%d level : %d",
				soc_info->clk_name[soc_info->src_clk_idx],
				clk,
				soc_info->clk_rate[cam_vote_level_index]
				[soc_info->src_clk_idx],
				cam_vote_level_index);
			return cam_vote_level_index;
		}
		last_valid_vote = cam_vote_level_index;
	}
	return last_valid_vote;
}

static int tpg_hw_configure_init_settings(
		struct tpg_hw *hw,
		struct tpg_hw_initsettings *settings)
{
	int rc = 0;
	enum cam_vote_level clk_level = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	mutex_lock(&hw->mutex);
	switch (hw->hw_info->version) {
	case TPG_HW_VERSION_1_0:
	case TPG_HW_VERSION_1_1:
	case TPG_HW_VERSION_1_2:
	case TPG_HW_VERSION_1_3:
		if (!hw->soc_info)
			rc = -EINVAL;
		else {
			clk_level = get_tpg_clk_level(hw);
			rc = tpg_hw_soc_enable(hw, clk_level);
			if (hw->hw_info->ops->init)
				rc = hw->hw_info->ops->init(hw, settings);
		}
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported HW Version",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&hw->mutex);
	return rc;
}

int tpg_hw_config(
	struct tpg_hw *hw,
	enum tpg_hw_cmd_t config_cmd,
	void *config_args)
{
	int rc = 0;

	if (!hw || !hw->hw_info || !hw->hw_info->ops)
		return -EINVAL;
	switch (config_cmd) {
	case TPG_HW_CMD_INIT_CONFIG:
		//validate_stream_list(hw);
		tpg_hw_configure_init_settings(hw,
			(struct tpg_hw_initsettings *)config_args);
		break;
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Unsupported hw config command",
				hw->hw_idx);
		rc = -EINVAL;
		break;
	}
	return rc;
}

int tpg_hw_free_streams(struct tpg_hw *hw)
{
	struct list_head *pos = NULL, *pos_next = NULL;
	struct tpg_hw_stream *entry;
	int i = 0;

	if (!hw)
		return -EINVAL;

	mutex_lock(&hw->mutex);
	/* free up the streams */
	CAM_DBG(CAM_TPG, "TPG[%d] Freeing all the streams", hw->hw_idx);

	/* reset the slots */
	for(i = 0; i < hw->hw_info->max_vc_channels; i++) {
		hw->vc_slots[i].slot_id      =  i;
		hw->vc_slots[i].vc           = -1;
		hw->vc_slots[i].stream_count =  0;
		list_for_each_safe(pos, pos_next, &hw->vc_slots[i].head) {
			entry = list_entry(pos, struct tpg_hw_stream, list);
			list_del(pos);
			kfree(entry);
		}
		INIT_LIST_HEAD(&(hw->vc_slots[i].head));
	}
	hw->vc_count = 0;

	mutex_unlock(&hw->mutex);
	return 0;
}

int tpg_hw_copy_global_config(
	struct tpg_hw *hw,
	struct tpg_global_config_t *global)
{
	if (!hw || !global) {
		CAM_ERR(CAM_TPG, "invalid parameter");
		return -EINVAL;
	}

	mutex_lock(&hw->mutex);
	memcpy(&hw->global_config,
		global,
		sizeof(struct tpg_global_config_t));
	mutex_unlock(&hw->mutex);
	return 0;
}

static int assign_vc_slot(
	struct tpg_hw *hw,
	int  vc,
	struct tpg_hw_stream *stream
	)
{
	int rc = -EINVAL, i = 0, slot_matched = 0;

	if (!hw || !stream) {
		return -EINVAL;
	}

	for(i = 0; i < hw->hw_info->max_vc_channels; i++) {
		/* Found a matching slot */
		if(hw->vc_slots[i].vc == vc) {
			slot_matched = 1;
			if (hw->vc_slots[i].stream_count
					< hw->hw_info->max_dt_channels_per_vc) {
				list_add_tail(&stream->list, &hw->vc_slots[i].head);
				hw->vc_slots[i].stream_count++;
				hw->vc_slots[i].vc = vc;
				rc = 0;
				CAM_DBG(CAM_TPG, "vc[%d]dt[%d]=>slot[%d]", vc, stream->stream.dt, i);
				break;
			} else {

				/**
				 * already slot was assigned for this vc
				 * however this slot have been filled with
				 * full streams
				 */
				rc = -EINVAL;
				CAM_ERR(CAM_TPG, "vc[%d]dt[%d]=>slot[%d] is overlfown",
						vc, stream->stream.dt, i);
				break;
			}
		}

		/**
		 * none of the above slots matched, and now found an empty slot
		 * so assigning stream to that slot
		 */
		if (hw->vc_slots[i].vc == -1) {
			list_add_tail(&stream->list, &hw->vc_slots[i].head);
			hw->vc_slots[i].stream_count++;
			hw->vc_slots[i].vc = vc;
			hw->vc_count++;
			rc = 0;
			CAM_DBG(CAM_TPG, "vc[%d]dt[%d]=>slot[%d]", vc, stream->stream.dt, i);
			break;
		}
	}
	if ((slot_matched == 0) && (rc != 0)) {
		CAM_ERR(CAM_TPG, "No slot matched");
	}
	return rc;
}

int tpg_hw_reset(struct tpg_hw *hw)
{
	int rc = 0;
	if (!hw)
		return -EINVAL;

	/* free up the streams if any*/
	rc = tpg_hw_free_streams(hw);
	if (rc)
		CAM_ERR(CAM_TPG, "TPG[%d] Unable to free up the streams", hw->hw_idx);

	/* disable the hw */
	mutex_lock(&hw->mutex);
	if ((hw->state != TPG_HW_STATE_HW_DISABLED) &&
			cam_cpas_stop(hw->cpas_handle)) {
		CAM_ERR(CAM_TPG, "TPG[%d] CPAS stop failed",
				hw->hw_idx);
		rc = -EINVAL;
	} else {
		hw->state = TPG_HW_STATE_HW_DISABLED;
	}
	mutex_unlock(&hw->mutex);

	return rc;
}

int tpg_hw_add_stream(
	struct tpg_hw *hw,
	struct tpg_stream_config_t *cmd)
{
	int rc = 0;
	struct tpg_hw_stream *stream = NULL;
	if (!hw || !cmd) {
		CAM_ERR(CAM_TPG, "Invalid params");
		return -EINVAL;
	}

	mutex_lock(&hw->mutex);
	stream = kzalloc(sizeof(struct tpg_hw_stream), GFP_KERNEL);
	if (!stream) {
		CAM_ERR(CAM_TPG, "TPG[%d] stream allocation failed",
				hw->hw_idx);
		mutex_unlock(&hw->mutex);
		return -ENOMEM;
	}
	memcpy(&stream->stream,
		cmd,
		sizeof(struct tpg_stream_config_t));

	rc = assign_vc_slot(hw, stream->stream.vc, stream);
	mutex_unlock(&hw->mutex);
	return rc;
}
