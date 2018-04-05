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

#include "cam_lrme_hw_core.h"
#include "cam_lrme_hw_soc.h"
#include "cam_smmu_api.h"

static void cam_lrme_cdm_write_reg_val_pair(uint32_t *buffer,
	uint32_t *index, uint32_t reg_offset, uint32_t reg_value)
{
	buffer[(*index)++] = reg_offset;
	buffer[(*index)++] = reg_value;
}

static void cam_lrme_hw_util_fill_fe_reg(struct cam_lrme_hw_io_buffer *io_buf,
	uint32_t index, uint32_t *reg_val_pair, uint32_t *num_cmd,
	struct cam_lrme_hw_info *hw_info)
{
	uint32_t reg_val;

	/* 1. config buffer size */
	reg_val = io_buf->io_cfg->planes[0].width;
	reg_val |= (io_buf->io_cfg->planes[0].height << 16);
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_rd_reg.bus_client_reg[index].rd_buffer_size,
		reg_val);

	CAM_DBG(CAM_LRME,
		"width %d", io_buf->io_cfg->planes[0].width);
	CAM_DBG(CAM_LRME,
		"height %d", io_buf->io_cfg->planes[0].height);

	/* 2. config image address */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_rd_reg.bus_client_reg[index].addr_image,
		io_buf->io_addr[0]);

	CAM_DBG(CAM_LRME, "io addr %llu", io_buf->io_addr[0]);

	/* 3. config stride */
	reg_val = io_buf->io_cfg->planes[0].plane_stride;
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_rd_reg.bus_client_reg[index].rd_stride,
		reg_val);

	CAM_DBG(CAM_LRME, "plane_stride %d",
		io_buf->io_cfg->planes[0].plane_stride);

	/* 4. enable client */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_rd_reg.bus_client_reg[index].core_cfg, 0x1);

	/* 5. unpack_cfg */
	if (io_buf->io_cfg->format == CAM_FORMAT_PD10)
		cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
			hw_info->bus_rd_reg.bus_client_reg[index].unpack_cfg_0,
			0x0);
	else if (io_buf->io_cfg->format == CAM_FORMAT_Y_ONLY)
		cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
			hw_info->bus_rd_reg.bus_client_reg[index].unpack_cfg_0,
			0x1);
	else if (io_buf->io_cfg->format == CAM_FORMAT_PLAIN16_10)
		cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
			hw_info->bus_rd_reg.bus_client_reg[index].unpack_cfg_0,
			0x22);
	else
		CAM_ERR(CAM_LRME, "Unsupported format %d",
			io_buf->io_cfg->format);
}

static void cam_lrme_hw_util_fill_we_reg(struct cam_lrme_hw_io_buffer *io_buf,
	uint32_t index, uint32_t *reg_val_pair, uint32_t *num_cmd,
	struct cam_lrme_hw_info *hw_info)
{
	/* config client mode */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].cfg,
		0x1);

	/* image address */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].addr_image,
		io_buf->io_addr[0]);
	CAM_DBG(CAM_LRME, "io addr %llu", io_buf->io_addr[0]);

	/* buffer width and height */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].buffer_width_cfg,
		io_buf->io_cfg->planes[0].width);
	CAM_DBG(CAM_LRME, "width %d", io_buf->io_cfg->planes[0].width);

	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].buffer_height_cfg,
		io_buf->io_cfg->planes[0].height);
	CAM_DBG(CAM_LRME, "height %d", io_buf->io_cfg->planes[0].height);

	/* packer cfg */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].packer_cfg,
		(index == 0) ? 0x1 : 0x5);

	/* client stride */
	cam_lrme_cdm_write_reg_val_pair(reg_val_pair, num_cmd,
		hw_info->bus_wr_reg.bus_client_reg[index].wr_stride,
		io_buf->io_cfg->planes[0].meta_stride);
	CAM_DBG(CAM_LRME, "plane_stride %d",
		io_buf->io_cfg->planes[0].plane_stride);
}


static int cam_lrme_hw_util_process_config_hw(struct cam_hw_info *lrme_hw,
	struct cam_lrme_hw_cmd_config_args *config_args)
{
	int i;
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_cdm_info *hw_cdm_info;
	uint32_t *cmd_buf_addr = config_args->cmd_buf_addr;
	uint32_t reg_val_pair[CAM_LRME_MAX_REG_PAIR_NUM];
	struct cam_lrme_hw_io_buffer *io_buf;
	struct cam_lrme_hw_info *hw_info =
		((struct cam_lrme_core *)lrme_hw->core_info)->hw_info;
	uint32_t num_cmd = 0;
	uint32_t size;
	uint32_t mem_base, available_size = config_args->size;
	uint32_t output_res_mask = 0, input_res_mask = 0;


	if (!cmd_buf_addr) {
		CAM_ERR(CAM_LRME, "Invalid input args");
		return -EINVAL;
	}

	hw_cdm_info =
		((struct cam_lrme_core *)lrme_hw->core_info)->hw_cdm_info;

	for (i = 0; i < CAM_LRME_MAX_IO_BUFFER; i++) {
		io_buf = &config_args->input_buf[i];

		if (io_buf->valid == false)
			break;

		if (io_buf->io_cfg->direction != CAM_BUF_INPUT) {
			CAM_ERR(CAM_LRME, "Incorrect direction %d %d",
				io_buf->io_cfg->direction, CAM_BUF_INPUT);
			return -EINVAL;
		}
		CAM_DBG(CAM_LRME,
			"resource_type %d", io_buf->io_cfg->resource_type);

		switch (io_buf->io_cfg->resource_type) {
		case CAM_LRME_IO_TYPE_TAR:
			cam_lrme_hw_util_fill_fe_reg(io_buf, 0, reg_val_pair,
				&num_cmd, hw_info);

			input_res_mask |= CAM_LRME_INPUT_PORT_TYPE_TAR;
			break;
		case CAM_LRME_IO_TYPE_REF:
			cam_lrme_hw_util_fill_fe_reg(io_buf, 1, reg_val_pair,
				&num_cmd, hw_info);

			input_res_mask |= CAM_LRME_INPUT_PORT_TYPE_REF;
			break;
		default:
			CAM_ERR(CAM_LRME, "wrong resource_type %d",
				io_buf->io_cfg->resource_type);
			return -EINVAL;
		}
	}

	for (i = 0; i < CAM_LRME_BUS_RD_MAX_CLIENTS; i++)
		if (!((input_res_mask >> i) & 0x1))
			cam_lrme_cdm_write_reg_val_pair(reg_val_pair, &num_cmd,
				hw_info->bus_rd_reg.bus_client_reg[i].core_cfg,
				0x0);

	for (i = 0; i < CAM_LRME_MAX_IO_BUFFER; i++) {
		io_buf = &config_args->output_buf[i];

		if (io_buf->valid == false)
			break;

		if (io_buf->io_cfg->direction != CAM_BUF_OUTPUT) {
			CAM_ERR(CAM_LRME, "Incorrect direction %d %d",
				io_buf->io_cfg->direction, CAM_BUF_INPUT);
			return -EINVAL;
		}

		CAM_DBG(CAM_LRME, "resource_type %d",
			io_buf->io_cfg->resource_type);
		switch (io_buf->io_cfg->resource_type) {
		case CAM_LRME_IO_TYPE_DS2:
			cam_lrme_hw_util_fill_we_reg(io_buf, 0, reg_val_pair,
				&num_cmd, hw_info);

			output_res_mask |= CAM_LRME_OUTPUT_PORT_TYPE_DS2;
			break;
		case CAM_LRME_IO_TYPE_RES:
			cam_lrme_hw_util_fill_we_reg(io_buf, 1, reg_val_pair,
				&num_cmd, hw_info);

			output_res_mask |= CAM_LRME_OUTPUT_PORT_TYPE_RES;
			break;

		default:
			CAM_ERR(CAM_LRME, "wrong resource_type %d",
				io_buf->io_cfg->resource_type);
			return -EINVAL;
		}
	}

	for (i = 0; i < CAM_LRME_BUS_RD_MAX_CLIENTS; i++)
		if (!((output_res_mask >> i) & 0x1))
			cam_lrme_cdm_write_reg_val_pair(reg_val_pair, &num_cmd,
				hw_info->bus_wr_reg.bus_client_reg[i].cfg, 0x0);

	if (output_res_mask) {
		/* write composite mask */
		cam_lrme_cdm_write_reg_val_pair(reg_val_pair, &num_cmd,
			hw_info->bus_wr_reg.common_reg.composite_mask_0,
			output_res_mask);
	}

	size = hw_cdm_info->cdm_ops->cdm_required_size_changebase();
	if ((size * 4) > available_size) {
		CAM_ERR(CAM_LRME, "buf size:%d is not sufficient, expected: %d",
			available_size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(soc_info, CAM_LRME_BASE_IDX);

	hw_cdm_info->cdm_ops->cdm_write_changebase(cmd_buf_addr, mem_base);
	cmd_buf_addr += size;
	available_size -= (size * 4);

	size = hw_cdm_info->cdm_ops->cdm_required_size_reg_random(
		num_cmd / 2);

	if ((size * 4) > available_size) {
		CAM_ERR(CAM_LRME, "buf size:%d is not sufficient, expected: %d",
			available_size, size);
		return -ENOMEM;
	}

	hw_cdm_info->cdm_ops->cdm_write_regrandom(cmd_buf_addr, num_cmd / 2,
		reg_val_pair);
	cmd_buf_addr += size;
	available_size -= (size * 4);

	config_args->config_buf_size =
		config_args->size - available_size;

	return 0;
}

static int cam_lrme_hw_util_submit_go(struct cam_hw_info *lrme_hw)
{
	struct cam_lrme_core *lrme_core;
	struct cam_hw_soc_info *soc_info;
	struct cam_lrme_hw_info   *hw_info;

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	hw_info = lrme_core->hw_info;
	soc_info = &lrme_hw->soc_info;

	cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
		hw_info->bus_rd_reg.common_reg.cmd);

	return 0;
}

static int cam_lrme_hw_util_reset(struct cam_hw_info *lrme_hw,
	uint32_t reset_type)
{
	struct cam_lrme_core *lrme_core;
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_hw_info *hw_info;
	long time_left;

	lrme_core = lrme_hw->core_info;
	hw_info = lrme_core->hw_info;

	switch (reset_type) {
	case CAM_LRME_HW_RESET_TYPE_HW_RESET:
		reinit_completion(&lrme_core->reset_complete);
		cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
			hw_info->titan_reg.top_rst_cmd);
		time_left = wait_for_completion_timeout(
			&lrme_core->reset_complete,
			msecs_to_jiffies(CAM_LRME_HW_RESET_TIMEOUT));
		if (time_left <= 0) {
			CAM_ERR(CAM_LRME,
				"HW reset wait failed time_left=%ld",
				time_left);
			return -ETIMEDOUT;
		}
		break;
	case CAM_LRME_HW_RESET_TYPE_SW_RESET:
		cam_io_w_mb(0x3, soc_info->reg_map[0].mem_base +
			hw_info->bus_wr_reg.common_reg.sw_reset);
		cam_io_w_mb(0x3, soc_info->reg_map[0].mem_base +
			hw_info->bus_rd_reg.common_reg.sw_reset);
		reinit_completion(&lrme_core->reset_complete);
		cam_io_w_mb(0x2, soc_info->reg_map[0].mem_base +
			hw_info->titan_reg.top_rst_cmd);
		time_left = wait_for_completion_timeout(
			&lrme_core->reset_complete,
			msecs_to_jiffies(CAM_LRME_HW_RESET_TIMEOUT));
		if (time_left <= 0) {
			CAM_ERR(CAM_LRME,
				"SW reset wait failed time_left=%ld",
				time_left);
			return -ETIMEDOUT;
		}
		break;
	}

	return 0;
}

int cam_lrme_hw_util_get_caps(struct cam_hw_info *lrme_hw,
	struct cam_lrme_dev_cap *hw_caps)
{
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_hw_info *hw_info =
		((struct cam_lrme_core *)lrme_hw->core_info)->hw_info;
	uint32_t reg_value;

	if (!hw_info) {
		CAM_ERR(CAM_LRME, "Invalid hw info data");
		return -EINVAL;
	}

	reg_value = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		hw_info->clc_reg.clc_hw_version);
	hw_caps->clc_hw_version.gen =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1C);
	hw_caps->clc_hw_version.rev =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->clc_hw_version.step =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		hw_info->bus_rd_reg.common_reg.hw_version);
	hw_caps->bus_rd_hw_version.gen =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1C);
	hw_caps->bus_rd_hw_version.rev =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->bus_rd_hw_version.step =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.hw_version);
	hw_caps->bus_wr_hw_version.gen =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1C);
	hw_caps->bus_wr_hw_version.rev =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->bus_wr_hw_version.step =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		hw_info->titan_reg.top_hw_version);
	hw_caps->top_hw_version.gen =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1C);
	hw_caps->top_hw_version.rev =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->top_hw_version.step =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[0].mem_base +
		hw_info->titan_reg.top_titan_version);
	hw_caps->top_titan_version.gen =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1C);
	hw_caps->top_titan_version.rev =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->top_titan_version.step =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	return 0;
}

static int cam_lrme_hw_util_submit_req(struct cam_lrme_core *lrme_core,
	struct cam_lrme_frame_request *frame_req)
{
	struct cam_lrme_cdm_info *hw_cdm_info =
		lrme_core->hw_cdm_info;
	struct cam_cdm_bl_request *cdm_cmd = hw_cdm_info->cdm_cmd;
	struct cam_hw_update_entry *cmd;
	int i, rc = 0;

	if (frame_req->num_hw_update_entries > 0) {
		cdm_cmd->cmd_arrary_count = frame_req->num_hw_update_entries;
		cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
		cdm_cmd->flag = false;
		cdm_cmd->userdata = NULL;
		cdm_cmd->cookie = 0;

		for (i = 0; i <= frame_req->num_hw_update_entries; i++) {
			cmd = (frame_req->hw_update_entries + i);
			cdm_cmd->cmd[i].bl_addr.mem_handle = cmd->handle;
			cdm_cmd->cmd[i].offset = cmd->offset;
			cdm_cmd->cmd[i].len = cmd->len;
		}

		rc = cam_cdm_submit_bls(hw_cdm_info->cdm_handle, cdm_cmd);
		if (rc) {
			CAM_ERR(CAM_LRME, "Failed to submit cdm commands");
			return -EINVAL;
		}
	} else {
		CAM_ERR(CAM_LRME, "No hw update entry");
		rc = -EINVAL;
	}

	return rc;
}

static int cam_lrme_hw_util_flush_ctx(struct cam_hw_info *lrme_hw,
	void *ctxt_to_hw_map)
{
	int rc = -ENODEV;
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	struct cam_lrme_hw_cb_args cb_args;
	struct cam_lrme_frame_request *req_proc, *req_submit;
	struct cam_lrme_hw_submit_args submit_args;

	rc = cam_lrme_hw_util_reset(lrme_hw, CAM_LRME_HW_RESET_TYPE_HW_RESET);
	if (rc) {
		CAM_ERR(CAM_LRME, "reset failed");
		return rc;
	}

	lrme_core->state = CAM_LRME_CORE_STATE_IDLE;
	req_proc = lrme_core->req_proc;
	req_submit = lrme_core->req_submit;
	lrme_core->req_proc = NULL;
	lrme_core->req_submit = NULL;

	if (req_submit && req_submit->ctxt_to_hw_map == ctxt_to_hw_map) {
		cb_args.cb_type = CAM_LRME_CB_PUT_FRAME;
		cb_args.frame_req = req_submit;
		if (lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb)
			lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(
				lrme_core->hw_mgr_cb.data, &cb_args);
	} else if (req_submit) {
		submit_args.frame_req = req_submit;
		submit_args.hw_update_entries = req_submit->hw_update_entries;
		submit_args.num_hw_update_entries =
			req_submit->num_hw_update_entries;
		rc = cam_lrme_hw_util_submit_req(lrme_core, req_submit);
		if (rc)
			CAM_ERR(CAM_LRME, "Submit failed");
		lrme_core->req_submit = req_submit;
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
	}

	if (req_proc && req_proc->ctxt_to_hw_map == ctxt_to_hw_map) {
		cb_args.cb_type = CAM_LRME_CB_PUT_FRAME;
		cb_args.frame_req = req_proc;
		if (lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb)
			lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(
				lrme_core->hw_mgr_cb.data, &cb_args);
	} else if (req_proc) {
		submit_args.frame_req = req_proc;
		submit_args.hw_update_entries = req_proc->hw_update_entries;
		submit_args.num_hw_update_entries =
			req_proc->num_hw_update_entries;
		rc = cam_lrme_hw_util_submit_req(lrme_core, req_proc);
		if (rc)
			CAM_ERR(CAM_LRME, "Submit failed");
		lrme_core->req_submit = req_proc;
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
	}

	return rc;
}

static int cam_lrme_hw_util_flush_req(struct cam_hw_info *lrme_hw,
	struct cam_lrme_frame_request *req_to_flush)
{
	int rc = -ENODEV;
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	struct cam_lrme_hw_cb_args cb_args;
	struct cam_lrme_frame_request *req_proc, *req_submit;
	struct cam_lrme_hw_submit_args submit_args;

	rc = cam_lrme_hw_util_reset(lrme_hw, CAM_LRME_HW_RESET_TYPE_HW_RESET);
	if (rc) {
		CAM_ERR(CAM_LRME, "reset failed");
		return rc;
	}

	lrme_core->state = CAM_LRME_CORE_STATE_IDLE;
	req_proc = lrme_core->req_proc;
	req_submit = lrme_core->req_submit;
	lrme_core->req_proc = NULL;
	lrme_core->req_submit = NULL;

	if (req_submit && req_submit == req_to_flush) {
		cb_args.cb_type = CAM_LRME_CB_PUT_FRAME;
		cb_args.frame_req = req_submit;
		if (lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb)
			lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(
				lrme_core->hw_mgr_cb.data, &cb_args);
	} else if (req_submit) {
		submit_args.frame_req = req_submit;
		submit_args.hw_update_entries = req_submit->hw_update_entries;
		submit_args.num_hw_update_entries =
			req_submit->num_hw_update_entries;
		rc = cam_lrme_hw_util_submit_req(lrme_core, req_submit);
		if (rc)
			CAM_ERR(CAM_LRME, "Submit failed");
		lrme_core->req_submit = req_submit;
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
	}

	if (req_proc && req_proc == req_to_flush) {
		cb_args.cb_type = CAM_LRME_CB_PUT_FRAME;
		cb_args.frame_req = req_proc;
		if (lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb)
			lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(
				lrme_core->hw_mgr_cb.data, &cb_args);
	} else if (req_proc) {
		submit_args.frame_req = req_proc;
		submit_args.hw_update_entries = req_proc->hw_update_entries;
		submit_args.num_hw_update_entries =
			req_proc->num_hw_update_entries;
		rc = cam_lrme_hw_util_submit_req(lrme_core, req_proc);
		if (rc)
			CAM_ERR(CAM_LRME, "Submit failed");
		lrme_core->req_submit = req_proc;
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
	}

	return rc;
}


static int cam_lrme_hw_util_process_err(struct cam_hw_info *lrme_hw)
{
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	struct cam_lrme_frame_request *req_proc, *req_submit;
	struct cam_lrme_hw_cb_args cb_args;
	int rc;

	req_proc = lrme_core->req_proc;
	req_submit = lrme_core->req_submit;
	cb_args.cb_type = CAM_LRME_CB_ERROR;

	if ((lrme_core->state != CAM_LRME_CORE_STATE_PROCESSING) &&
		(lrme_core->state != CAM_LRME_CORE_STATE_REQ_PENDING) &&
		(lrme_core->state != CAM_LRME_CORE_STATE_REQ_PROC_PEND)) {
		CAM_ERR(CAM_LRME, "Get error irq in wrong state %d",
			lrme_core->state);
	}

	CAM_ERR_RATE_LIMIT(CAM_LRME, "Start recovery");
	lrme_core->state = CAM_LRME_CORE_STATE_RECOVERY;
	rc = cam_lrme_hw_util_reset(lrme_hw, CAM_LRME_HW_RESET_TYPE_HW_RESET);
	if (rc)
		CAM_ERR(CAM_LRME, "Failed to reset");

	lrme_core->req_proc = NULL;
	lrme_core->req_submit = NULL;
	if (!rc)
		lrme_core->state = CAM_LRME_CORE_STATE_IDLE;

	cb_args.frame_req = req_proc;
	lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(lrme_core->hw_mgr_cb.data,
		&cb_args);

	cb_args.frame_req = req_submit;
	lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(lrme_core->hw_mgr_cb.data,
		&cb_args);

	return rc;
}

static int cam_lrme_hw_util_process_reg_update(
	struct cam_hw_info *lrme_hw, struct cam_lrme_hw_cb_args *cb_args)
{
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	int rc = 0;

	cb_args->cb_type |= CAM_LRME_CB_COMP_REG_UPDATE;
	if (lrme_core->state == CAM_LRME_CORE_STATE_REQ_PENDING) {
		lrme_core->state = CAM_LRME_CORE_STATE_PROCESSING;
	} else {
		CAM_ERR(CAM_LRME, "Reg update in wrong state %d",
			lrme_core->state);
		rc = cam_lrme_hw_util_process_err(lrme_hw);
		if (rc)
			CAM_ERR(CAM_LRME, "Failed to reset");
		return -EINVAL;
	}

	lrme_core->req_proc = lrme_core->req_submit;
	lrme_core->req_submit = NULL;

	return 0;
}

static int cam_lrme_hw_util_process_idle(
	struct cam_hw_info *lrme_hw, struct cam_lrme_hw_cb_args *cb_args)
{
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	int rc = 0;

	cb_args->cb_type |= CAM_LRME_CB_BUF_DONE;
	switch (lrme_core->state) {
	case CAM_LRME_CORE_STATE_REQ_PROC_PEND:
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
		break;

	case CAM_LRME_CORE_STATE_PROCESSING:
		lrme_core->state = CAM_LRME_CORE_STATE_IDLE;
		break;

	default:
		CAM_ERR(CAM_LRME, "Idle in wrong state %d",
			lrme_core->state);
		rc = cam_lrme_hw_util_process_err(lrme_hw);
		return rc;
	}
	cb_args->frame_req = lrme_core->req_proc;
	lrme_core->req_proc = NULL;

	return 0;
}

void cam_lrme_set_irq(struct cam_hw_info *lrme_hw,
	enum cam_lrme_irq_set set)
{
	struct cam_hw_soc_info *soc_info = &lrme_hw->soc_info;
	struct cam_lrme_core *lrme_core = lrme_hw->core_info;
	struct cam_lrme_hw_info *hw_info = lrme_core->hw_info;

	switch (set) {
	case CAM_LRME_IRQ_ENABLE:
		cam_io_w_mb(0xFFFF,
			soc_info->reg_map[0].mem_base +
			hw_info->titan_reg.top_irq_mask);
		cam_io_w_mb(0xFFFF,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_wr_reg.common_reg.irq_mask_0);
		cam_io_w_mb(0xFFFF,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_wr_reg.common_reg.irq_mask_1);
		cam_io_w_mb(0xFFFF,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_rd_reg.common_reg.irq_mask);
		break;

	case CAM_LRME_IRQ_DISABLE:
		cam_io_w_mb(0x0,
			soc_info->reg_map[0].mem_base +
			hw_info->titan_reg.top_irq_mask);
		cam_io_w_mb(0x0,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_wr_reg.common_reg.irq_mask_0);
		cam_io_w_mb(0x0,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_wr_reg.common_reg.irq_mask_1);
		cam_io_w_mb(0x0,
			soc_info->reg_map[0].mem_base +
			hw_info->bus_rd_reg.common_reg.irq_mask);
		break;
	}
}


int cam_lrme_hw_process_irq(void *priv, void *data)
{
	struct cam_lrme_hw_work_data *work_data;
	struct cam_hw_info *lrme_hw;
	struct cam_lrme_core *lrme_core;
	int rc = 0;
	uint32_t top_irq_status, fe_irq_status;
	uint32_t *we_irq_status;
	struct cam_lrme_hw_cb_args cb_args;

	if (!data || !priv) {
		CAM_ERR(CAM_LRME, "Invalid data %pK %pK", data, priv);
		return -EINVAL;
	}

	memset(&cb_args, 0, sizeof(struct cam_lrme_hw_cb_args));
	lrme_hw = (struct cam_hw_info *)priv;
	work_data = (struct cam_lrme_hw_work_data *)data;
	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	top_irq_status = work_data->top_irq_status;
	fe_irq_status = work_data->fe_irq_status;
	we_irq_status = work_data->we_irq_status;

	CAM_DBG(CAM_LRME,
		"top status %x, fe status %x, we status0 %x, we status1 %x",
		top_irq_status, fe_irq_status, we_irq_status[0],
		we_irq_status[1]);
	CAM_DBG(CAM_LRME, "Current state %d", lrme_core->state);

	mutex_lock(&lrme_hw->hw_mutex);

	if (top_irq_status & (1 << 3)) {
		CAM_DBG(CAM_LRME, "Error");
		rc = cam_lrme_hw_util_process_err(lrme_hw);
		if (rc)
			CAM_ERR(CAM_LRME, "Process error failed");
		goto end;
	}

	if (we_irq_status[0] & (1 << 1)) {
		CAM_DBG(CAM_LRME, "reg update");
		rc = cam_lrme_hw_util_process_reg_update(lrme_hw, &cb_args);
		if (rc) {
			CAM_ERR(CAM_LRME, "Process reg_update failed");
			goto end;
		}
	}

	if (top_irq_status & (1 << 4)) {
		CAM_DBG(CAM_LRME, "IDLE");
		if (!lrme_core->req_proc) {
			CAM_DBG(CAM_LRME, "No frame request to process idle");
			goto end;
		}
		rc = cam_lrme_hw_util_process_idle(lrme_hw, &cb_args);
		if (rc) {
			CAM_ERR(CAM_LRME, "Process idle failed");
			goto end;
		}
	}

	if (lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb) {
		lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb(
			lrme_core->hw_mgr_cb.data, &cb_args);
	} else {
		CAM_ERR(CAM_LRME, "No hw mgr cb");
		rc = -EINVAL;
	}

end:
	mutex_unlock(&lrme_hw->hw_mutex);
	return rc;
}

int cam_lrme_hw_start(void *hw_priv, void *hw_start_args, uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw = (struct cam_hw_info *)hw_priv;
	int rc = 0;
	struct cam_lrme_core *lrme_core;

	if (!lrme_hw) {
		CAM_ERR(CAM_LRME,
			"Invalid input params, lrme_hw %pK",
			lrme_hw);
		return -EINVAL;
	}

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;

	mutex_lock(&lrme_hw->hw_mutex);

	if (lrme_hw->open_count > 0) {
		lrme_hw->open_count++;
		CAM_DBG(CAM_LRME, "This device is activated before");
		goto unlock;
	}

	rc = cam_lrme_soc_enable_resources(lrme_hw);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to enable soc resources");
		goto unlock;
	}

	rc = cam_lrme_hw_util_reset(lrme_hw, CAM_LRME_HW_RESET_TYPE_HW_RESET);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to reset hw");
		goto disable_soc;
	}

	if (lrme_core->hw_cdm_info) {
		struct cam_lrme_cdm_info *hw_cdm_info =
			lrme_core->hw_cdm_info;

		rc = cam_cdm_stream_on(hw_cdm_info->cdm_handle);
		if (rc) {
			CAM_ERR(CAM_LRME, "Failed to stream on cdm");
			goto disable_soc;
		}
	}

	lrme_hw->hw_state = CAM_HW_STATE_POWER_UP;
	lrme_hw->open_count++;
	lrme_core->state = CAM_LRME_CORE_STATE_IDLE;

	CAM_DBG(CAM_LRME, "open count %d", lrme_hw->open_count);
	mutex_unlock(&lrme_hw->hw_mutex);
	return rc;

disable_soc:
	if (cam_lrme_soc_disable_resources(lrme_hw))
		CAM_ERR(CAM_LRME, "Error in disable soc resources");
unlock:
	CAM_DBG(CAM_LRME, "open count %d", lrme_hw->open_count);
	mutex_unlock(&lrme_hw->hw_mutex);
	return rc;
}

int cam_lrme_hw_stop(void *hw_priv, void *hw_stop_args, uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw = (struct cam_hw_info *)hw_priv;
	int rc = 0;
	struct cam_lrme_core *lrme_core;

	if (!lrme_hw) {
		CAM_ERR(CAM_LRME, "Invalid argument");
		return -EINVAL;
	}

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;

	mutex_lock(&lrme_hw->hw_mutex);

	if (lrme_hw->open_count == 0) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_ERR(CAM_LRME, "Error Unbalanced stop");
		return -EINVAL;
	}
	lrme_hw->open_count--;

	CAM_DBG(CAM_LRME, "open count %d", lrme_hw->open_count);

	if (lrme_hw->open_count)
		goto unlock;

	lrme_core->req_proc = NULL;
	lrme_core->req_submit = NULL;

	if (lrme_core->hw_cdm_info) {
		struct cam_lrme_cdm_info *hw_cdm_info =
			lrme_core->hw_cdm_info;

		rc = cam_cdm_stream_off(hw_cdm_info->cdm_handle);
		if (rc) {
			CAM_ERR(CAM_LRME,
				"Failed in CDM StreamOff, handle=0x%x, rc=%d",
				hw_cdm_info->cdm_handle, rc);
			goto unlock;
		}
	}

	rc = cam_lrme_soc_disable_resources(lrme_hw);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed in Disable SOC, rc=%d", rc);
		goto unlock;
	}

	lrme_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	if (lrme_core->state == CAM_LRME_CORE_STATE_IDLE) {
		lrme_core->state = CAM_LRME_CORE_STATE_INIT;
	} else {
		CAM_ERR(CAM_LRME, "HW in wrong state %d", lrme_core->state);
		return -EINVAL;
	}

unlock:
	mutex_unlock(&lrme_hw->hw_mutex);
	return rc;
}

int cam_lrme_hw_submit_req(void *hw_priv, void *hw_submit_args,
	uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw = (struct cam_hw_info *)hw_priv;
	struct cam_lrme_core *lrme_core;
	struct cam_lrme_hw_submit_args *args =
		(struct cam_lrme_hw_submit_args *)hw_submit_args;
	int rc = 0;
	struct cam_lrme_frame_request *frame_req;


	if (!hw_priv || !hw_submit_args) {
		CAM_ERR(CAM_LRME, "Invalid input");
		return -EINVAL;
	}

	if (sizeof(struct cam_lrme_hw_submit_args) != arg_size) {
		CAM_ERR(CAM_LRME,
			"size of args %lu, arg_size %d",
			sizeof(struct cam_lrme_hw_submit_args), arg_size);
		return -EINVAL;
	}

	frame_req = args->frame_req;

	mutex_lock(&lrme_hw->hw_mutex);

	if (lrme_hw->open_count == 0) {
		CAM_ERR(CAM_LRME, "HW is not open");
		mutex_unlock(&lrme_hw->hw_mutex);
		return -EINVAL;
	}

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	if (lrme_core->state != CAM_LRME_CORE_STATE_IDLE &&
		lrme_core->state != CAM_LRME_CORE_STATE_PROCESSING) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_DBG(CAM_LRME, "device busy, can not submit, state %d",
			lrme_core->state);
		return -EBUSY;
	}

	if (lrme_core->req_submit != NULL) {
		CAM_ERR(CAM_LRME, "req_submit is not NULL");
		return -EBUSY;
	}

	rc = cam_lrme_hw_util_submit_req(lrme_core, frame_req);
	if (rc) {
		CAM_ERR(CAM_LRME, "Submit req failed");
		goto error;
	}

	switch (lrme_core->state) {
	case CAM_LRME_CORE_STATE_PROCESSING:
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PROC_PEND;
		break;

	case CAM_LRME_CORE_STATE_IDLE:
		cam_lrme_hw_util_submit_go(lrme_hw);
		lrme_core->state = CAM_LRME_CORE_STATE_REQ_PENDING;
		break;

	default:
		CAM_ERR(CAM_LRME, "Wrong hw state");
		rc = -EINVAL;
		goto error;
	}

	lrme_core->req_submit = frame_req;
	mutex_unlock(&lrme_hw->hw_mutex);
	CAM_DBG(CAM_LRME, "Release lock, submit done for req %llu",
		frame_req->req_id);

	return 0;

error:
	mutex_unlock(&lrme_hw->hw_mutex);

	return rc;

}

int cam_lrme_hw_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw = hw_priv;
	struct cam_lrme_core *lrme_core;
	struct cam_lrme_hw_reset_args *lrme_reset_args = reset_core_args;
	int rc;

	if (!hw_priv) {
		CAM_ERR(CAM_LRME, "Invalid input args");
		return -EINVAL;
	}

	if (!reset_core_args ||
		sizeof(struct cam_lrme_hw_reset_args) != arg_size) {
		CAM_ERR(CAM_LRME, "Invalid reset args");
		return -EINVAL;
	}

	lrme_core = lrme_hw->core_info;

	mutex_lock(&lrme_hw->hw_mutex);
	if (lrme_core->state == CAM_LRME_CORE_STATE_RECOVERY) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_ERR(CAM_LRME, "Reset not allowed in %d state",
			lrme_core->state);
		return -EINVAL;
	}

	lrme_core->state = CAM_LRME_CORE_STATE_RECOVERY;

	rc = cam_lrme_hw_util_reset(lrme_hw, lrme_reset_args->reset_type);
	if (rc) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_ERR(CAM_FD, "Failed to reset");
		return rc;
	}

	lrme_core->state = CAM_LRME_CORE_STATE_IDLE;

	mutex_unlock(&lrme_hw->hw_mutex);

	return 0;
}

int cam_lrme_hw_flush(void *hw_priv, void *hw_flush_args, uint32_t arg_size)
{
	struct cam_lrme_core         *lrme_core = NULL;
	struct cam_hw_info           *lrme_hw = hw_priv;
	struct cam_lrme_hw_flush_args *flush_args =
		(struct cam_lrme_hw_flush_args *)hw_flush_args;
	int rc = -ENODEV;

	if (!hw_priv) {
		CAM_ERR(CAM_LRME, "Invalid arguments %pK", hw_priv);
		return -EINVAL;
	}

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;

	mutex_lock(&lrme_hw->hw_mutex);

	if (lrme_core->state != CAM_LRME_CORE_STATE_PROCESSING &&
		lrme_core->state != CAM_LRME_CORE_STATE_REQ_PENDING &&
		lrme_core->state == CAM_LRME_CORE_STATE_REQ_PROC_PEND) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_DBG(CAM_LRME, "Stop not needed in %d state",
			lrme_core->state);
		return 0;
	}

	if (!lrme_core->req_proc && !lrme_core->req_submit) {
		mutex_unlock(&lrme_hw->hw_mutex);
		CAM_DBG(CAM_LRME, "no req in device");
		return 0;
	}

	switch (flush_args->flush_type) {
	case CAM_FLUSH_TYPE_ALL:
		if ((!lrme_core->req_submit ||
			lrme_core->req_submit->ctxt_to_hw_map !=
			flush_args->ctxt_to_hw_map) &&
			(!lrme_core->req_proc ||
			lrme_core->req_proc->ctxt_to_hw_map !=
			flush_args->ctxt_to_hw_map)) {
			mutex_unlock(&lrme_hw->hw_mutex);
			CAM_DBG(CAM_LRME, "hw running on different ctx");
			return 0;
		}
		rc = cam_lrme_hw_util_flush_ctx(lrme_hw,
			flush_args->ctxt_to_hw_map);
		if (rc)
			CAM_ERR(CAM_LRME, "Flush all failed");
		break;

	case CAM_FLUSH_TYPE_REQ:
		if ((!lrme_core->req_submit ||
			lrme_core->req_submit != flush_args->req_to_flush) &&
			(!lrme_core->req_proc ||
			lrme_core->req_proc != flush_args->req_to_flush)) {
			mutex_unlock(&lrme_hw->hw_mutex);
			CAM_DBG(CAM_LRME, "hw running on different ctx");
			return 0;
		}
		rc = cam_lrme_hw_util_flush_req(lrme_hw,
			flush_args->req_to_flush);
		if (rc)
			CAM_ERR(CAM_LRME, "Flush req failed");
		break;

	default:
		CAM_ERR(CAM_LRME, "Unsupported flush type");
		break;
	}

	mutex_unlock(&lrme_hw->hw_mutex);

	return rc;
}

int cam_lrme_hw_get_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw;
	struct cam_lrme_core *lrme_core;
	struct cam_lrme_dev_cap *lrme_hw_caps =
		(struct cam_lrme_dev_cap *)get_hw_cap_args;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_LRME, "Invalid input pointers %pK %pK",
			hw_priv, get_hw_cap_args);
		return -EINVAL;
	}

	lrme_hw = (struct cam_hw_info *)hw_priv;
	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	*lrme_hw_caps = lrme_core->hw_caps;

	return 0;
}

irqreturn_t cam_lrme_hw_irq(int irq_num, void *data)
{
	struct cam_hw_info *lrme_hw;
	struct cam_lrme_core *lrme_core;
	struct cam_hw_soc_info *soc_info;
	struct cam_lrme_hw_info   *hw_info;
	struct crm_workq_task *task;
	struct cam_lrme_hw_work_data *work_data;
	uint32_t top_irq_status, fe_irq_status, we_irq_status0, we_irq_status1;
	int rc;

	if (!data) {
		CAM_ERR(CAM_LRME, "Invalid data in IRQ callback");
		return -EINVAL;
	}

	lrme_hw = (struct cam_hw_info *)data;
	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	soc_info = &lrme_hw->soc_info;
	hw_info = lrme_core->hw_info;

	top_irq_status = cam_io_r_mb(
		soc_info->reg_map[0].mem_base +
		hw_info->titan_reg.top_irq_status);
	CAM_DBG(CAM_LRME, "top_irq_status %x", top_irq_status);
	cam_io_w_mb(top_irq_status,
		soc_info->reg_map[0].mem_base +
		hw_info->titan_reg.top_irq_clear);
	top_irq_status &= CAM_LRME_TOP_IRQ_MASK;

	fe_irq_status = cam_io_r_mb(
		soc_info->reg_map[0].mem_base +
		hw_info->bus_rd_reg.common_reg.irq_status);
	CAM_DBG(CAM_LRME, "fe_irq_status %x", fe_irq_status);
	cam_io_w_mb(fe_irq_status,
		soc_info->reg_map[0].mem_base +
		hw_info->bus_rd_reg.common_reg.irq_clear);
	fe_irq_status &= CAM_LRME_FE_IRQ_MASK;

	we_irq_status0 = cam_io_r_mb(
		soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.irq_status_0);
	CAM_DBG(CAM_LRME, "we_irq_status[0] %x", we_irq_status0);
	cam_io_w_mb(we_irq_status0,
		soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.irq_clear_0);
	we_irq_status0 &= CAM_LRME_WE_IRQ_MASK_0;

	we_irq_status1 = cam_io_r_mb(
		soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.irq_status_1);
	CAM_DBG(CAM_LRME, "we_irq_status[1] %x", we_irq_status1);
	cam_io_w_mb(we_irq_status1,
		soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.irq_clear_1);
	we_irq_status1 &= CAM_LRME_WE_IRQ_MASK_1;

	cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
		hw_info->titan_reg.top_irq_cmd);
	cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
		hw_info->bus_wr_reg.common_reg.irq_cmd);
	cam_io_w_mb(0x1, soc_info->reg_map[0].mem_base +
		hw_info->bus_rd_reg.common_reg.irq_cmd);

	if (top_irq_status & 0x1) {
		complete(&lrme_core->reset_complete);
		top_irq_status &= (~0x1);
	}

	if (top_irq_status || fe_irq_status ||
		we_irq_status0 || we_irq_status1) {
		task = cam_req_mgr_workq_get_task(lrme_core->work);
		if (!task) {
			CAM_ERR(CAM_LRME, "no empty task available");
			return -ENOMEM;
		}
		work_data = (struct cam_lrme_hw_work_data *)task->payload;
		work_data->top_irq_status = top_irq_status;
		work_data->fe_irq_status = fe_irq_status;
		work_data->we_irq_status[0] = we_irq_status0;
		work_data->we_irq_status[1] = we_irq_status1;
		task->process_cb = cam_lrme_hw_process_irq;
		rc = cam_req_mgr_workq_enqueue_task(task, data,
			CRM_TASK_PRIORITY_0);
		if (rc)
			CAM_ERR(CAM_LRME,
				"Failed in enqueue work task, rc=%d", rc);
	}

	return IRQ_HANDLED;
}

int cam_lrme_hw_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *lrme_hw = (struct cam_hw_info *)hw_priv;
	int rc = 0;

	switch (cmd_type) {
	case CAM_LRME_HW_CMD_PREPARE_HW_UPDATE: {
		struct cam_lrme_hw_cmd_config_args *config_args;

		config_args = (struct cam_lrme_hw_cmd_config_args *)cmd_args;
		rc = cam_lrme_hw_util_process_config_hw(lrme_hw, config_args);
		break;
	}

	case CAM_LRME_HW_CMD_REGISTER_CB: {
		struct cam_lrme_hw_cmd_set_cb *cb_args;
		struct cam_lrme_device *hw_device;
		struct cam_lrme_core *lrme_core =
			(struct cam_lrme_core *)lrme_hw->core_info;
		cb_args = (struct cam_lrme_hw_cmd_set_cb *)cmd_args;
		lrme_core->hw_mgr_cb.cam_lrme_hw_mgr_cb =
			cb_args->cam_lrme_hw_mgr_cb;
		lrme_core->hw_mgr_cb.data = cb_args->data;
		hw_device = cb_args->data;
		rc = 0;
		break;
	}

	case CAM_LRME_HW_CMD_SUBMIT: {
		struct cam_lrme_hw_submit_args *submit_args;

		submit_args = (struct cam_lrme_hw_submit_args *)cmd_args;
		rc = cam_lrme_hw_submit_req(hw_priv,
			submit_args, arg_size);
		break;
	}

	default:
		break;
	}

	return rc;
}
