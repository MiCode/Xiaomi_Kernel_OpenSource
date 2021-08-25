/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include "cam_fd_hw_core.h"
#include "cam_fd_hw_soc.h"
#include "cam_trace.h"

#define CAM_FD_REG_VAL_PAIR_SIZE 256

static uint32_t cam_fd_cdm_write_reg_val_pair(uint32_t *buffer,
	uint32_t index, uint32_t reg_offset, uint32_t reg_value)
{
	buffer[index++] = reg_offset;
	buffer[index++] = reg_value;

	CAM_DBG(CAM_FD, "FD_CDM_CMD: Base[FD_CORE] Offset[0x%8x] Value[0x%8x]",
		reg_offset, reg_value);

	return index;
}

static void cam_fd_hw_util_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint64_t cookie)
{
	trace_cam_cdm_cb("FD", status);
	CAM_DBG(CAM_FD, "CDM hdl=%x, udata=%pK, status=%d, cookie=%llu",
		handle, userdata, status, cookie);
}

static void cam_fd_hw_util_enable_power_on_settings(struct cam_hw_info *fd_hw)
{
	struct cam_hw_soc_info *soc_info = &fd_hw->soc_info;
	struct cam_fd_hw_static_info *hw_static_info =
		((struct cam_fd_core *)fd_hw->core_info)->hw_static_info;

	if (hw_static_info->enable_errata_wa.single_irq_only == false) {
		/* Enable IRQs here */
		cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
			hw_static_info->wrapper_regs.irq_mask,
			hw_static_info->irq_mask);
	}

	/* QoS settings */
	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.vbif_req_priority,
		hw_static_info->qos_priority);
	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.vbif_priority_level,
		hw_static_info->qos_priority_level);
}

int cam_fd_hw_util_get_hw_caps(struct cam_hw_info *fd_hw,
	struct cam_fd_hw_caps *hw_caps)
{
	struct cam_hw_soc_info *soc_info = &fd_hw->soc_info;
	struct cam_fd_hw_static_info *hw_static_info =
		((struct cam_fd_core *)fd_hw->core_info)->hw_static_info;
	uint32_t reg_value;

	if (!hw_static_info) {
		CAM_ERR(CAM_FD, "Invalid hw info data");
		return -EINVAL;
	}

	reg_value = cam_fd_soc_register_read(soc_info, CAM_FD_REG_CORE,
		hw_static_info->core_regs.version);
	hw_caps->core_version.major =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf00, 0x8);
	hw_caps->core_version.minor =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0, 0x4);
	hw_caps->core_version.incr =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf, 0x0);

	reg_value = cam_fd_soc_register_read(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.wrapper_version);
	hw_caps->wrapper_version.major =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1c);
	hw_caps->wrapper_version.minor =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->wrapper_version.incr =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	hw_caps->raw_results_available =
		hw_static_info->results.raw_results_available;
	hw_caps->supported_modes = hw_static_info->supported_modes;

	CAM_DBG(CAM_FD, "core:%d.%d.%d wrapper:%d.%d.%d intermediate:%d",
		hw_caps->core_version.major, hw_caps->core_version.minor,
		hw_caps->core_version.incr, hw_caps->wrapper_version.major,
		hw_caps->wrapper_version.minor, hw_caps->wrapper_version.incr,
		hw_caps->raw_results_available);

	return 0;
}

static int cam_fd_hw_util_fdwrapper_sync_reset(struct cam_hw_info *fd_hw)
{
	struct cam_fd_core *fd_core = (struct cam_fd_core *)fd_hw->core_info;
	struct cam_fd_hw_static_info *hw_static_info = fd_core->hw_static_info;
	struct cam_hw_soc_info *soc_info = &fd_hw->soc_info;
	long time_left;

	/* Before triggering reset to HW, clear the reset complete */
	reinit_completion(&fd_core->reset_complete);

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_CORE,
		hw_static_info->core_regs.control, 0x1);

	if (hw_static_info->enable_errata_wa.single_irq_only) {
		cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
			hw_static_info->wrapper_regs.irq_mask,
			CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_RESET_DONE));
	}

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.sw_reset, 0x1);

	time_left = wait_for_completion_timeout(&fd_core->reset_complete,
		msecs_to_jiffies(CAM_FD_HW_HALT_RESET_TIMEOUT));
	if (time_left <= 0)
		CAM_WARN(CAM_FD, "HW reset timeout time_left=%ld", time_left);

	CAM_DBG(CAM_FD, "FD Wrapper SW Sync Reset complete");

	return 0;
}


static int cam_fd_hw_util_fdwrapper_halt(struct cam_hw_info *fd_hw)
{
	struct cam_fd_core *fd_core = (struct cam_fd_core *)fd_hw->core_info;
	struct cam_fd_hw_static_info *hw_static_info = fd_core->hw_static_info;
	struct cam_hw_soc_info *soc_info = &fd_hw->soc_info;
	long time_left;

	/* Before triggering halt to HW, clear halt complete */
	reinit_completion(&fd_core->halt_complete);

	if (hw_static_info->enable_errata_wa.single_irq_only) {
		cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
			hw_static_info->wrapper_regs.irq_mask,
			CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_HALT_DONE));
	}

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.hw_stop, 0x1);

	time_left = wait_for_completion_timeout(&fd_core->halt_complete,
		msecs_to_jiffies(CAM_FD_HW_HALT_RESET_TIMEOUT));
	if (time_left <= 0)
		CAM_WARN(CAM_FD, "HW halt timeout time_left=%ld", time_left);

	CAM_DBG(CAM_FD, "FD Wrapper Halt complete");

	return 0;
}

static int cam_fd_hw_util_processcmd_prestart(struct cam_hw_info *fd_hw,
	struct cam_fd_hw_cmd_prestart_args *prestart_args)
{
	struct cam_hw_soc_info *soc_info = &fd_hw->soc_info;
	struct cam_fd_hw_static_info *hw_static_info =
		((struct cam_fd_core *)fd_hw->core_info)->hw_static_info;
	struct cam_fd_ctx_hw_private *ctx_hw_private =
		prestart_args->ctx_hw_private;
	uint32_t size, size_required = 0;
	uint32_t mem_base;
	uint32_t *cmd_buf_addr = prestart_args->cmd_buf_addr;
	uint32_t reg_val_pair[CAM_FD_REG_VAL_PAIR_SIZE];
	uint32_t num_cmds = 0;
	int i;
	struct cam_fd_hw_io_buffer *io_buf;
	struct cam_fd_hw_req_private *req_private;
	uint32_t available_size = prestart_args->size;
	bool work_buffer_configured = false;

	if (!ctx_hw_private || !cmd_buf_addr) {
		CAM_ERR(CAM_FD, "Invalid input prestart args %pK %pK",
			ctx_hw_private, cmd_buf_addr);
		return -EINVAL;
	}

	if (prestart_args->get_raw_results &&
		!hw_static_info->results.raw_results_available) {
		CAM_ERR(CAM_FD, "Raw results not supported %d %d",
			prestart_args->get_raw_results,
			hw_static_info->results.raw_results_available);
		return -EINVAL;
	}

	req_private = &prestart_args->hw_req_private;
	req_private->ctx_hw_private = prestart_args->ctx_hw_private;
	req_private->request_id = prestart_args->request_id;
	req_private->get_raw_results = prestart_args->get_raw_results;
	req_private->fd_results = NULL;
	req_private->raw_results = NULL;

	/* Start preparing CDM register values that KMD has to insert */
	num_cmds = cam_fd_cdm_write_reg_val_pair(reg_val_pair, num_cmds,
		hw_static_info->core_regs.control, 0x1);
	num_cmds = cam_fd_cdm_write_reg_val_pair(reg_val_pair, num_cmds,
		hw_static_info->core_regs.control, 0x0);

	for (i = 0; i < CAM_FD_MAX_IO_BUFFERS; i++) {
		io_buf = &prestart_args->input_buf[i];

		if (io_buf->valid == false)
			break;

		if (io_buf->io_cfg->direction != CAM_BUF_INPUT) {
			CAM_ERR(CAM_FD, "Incorrect direction %d %d",
				io_buf->io_cfg->direction, CAM_BUF_INPUT);
			return -EINVAL;
		}

		switch (io_buf->io_cfg->resource_type) {
		case CAM_FD_INPUT_PORT_ID_IMAGE: {
			if ((num_cmds + 2) > CAM_FD_REG_VAL_PAIR_SIZE) {
				CAM_ERR(CAM_FD,
					"Invalid reg_val pair size %d, %d",
					num_cmds, CAM_FD_REG_VAL_PAIR_SIZE);
				return -EINVAL;
			}

			num_cmds = cam_fd_cdm_write_reg_val_pair(
				reg_val_pair, num_cmds,
				hw_static_info->core_regs.image_addr,
				io_buf->io_addr[0]);
			break;
		}
		default:
			CAM_ERR(CAM_FD, "Invalid resource type %d",
				io_buf->io_cfg->resource_type);
			return -EINVAL;
		}
	}

	for (i = 0; i < CAM_FD_MAX_IO_BUFFERS; i++) {
		io_buf = &prestart_args->output_buf[i];

		if (io_buf->valid == false)
			break;

		if (io_buf->io_cfg->direction != CAM_BUF_OUTPUT) {
			CAM_ERR(CAM_FD, "Incorrect direction %d %d",
				io_buf->io_cfg->direction, CAM_BUF_INPUT);
			return -EINVAL;
		}

		switch (io_buf->io_cfg->resource_type) {
		case CAM_FD_OUTPUT_PORT_ID_RESULTS: {
			uint32_t face_results_offset;

			size_required = hw_static_info->results.max_faces *
				hw_static_info->results.per_face_entries * 4;

			if (io_buf->io_cfg->planes[0].plane_stride <
				size_required) {
				CAM_ERR(CAM_FD, "Invalid results size %d %d",
					io_buf->io_cfg->planes[0].plane_stride,
					size_required);
				return -EINVAL;
			}

			req_private->fd_results =
				(struct cam_fd_results *)io_buf->cpu_addr[0];

			face_results_offset =
				(uint8_t *)&req_private->fd_results->faces[0] -
				(uint8_t *)req_private->fd_results;

			if (hw_static_info->ro_mode_supported) {
				if ((num_cmds + 4) > CAM_FD_REG_VAL_PAIR_SIZE) {
					CAM_ERR(CAM_FD,
						"Invalid reg_val size %d, %d",
						num_cmds,
						CAM_FD_REG_VAL_PAIR_SIZE);
					return -EINVAL;
				}
				/*
				 * Face data actually starts 16bytes later in
				 * the io buffer  Check cam_fd_results.
				 */
				num_cmds = cam_fd_cdm_write_reg_val_pair(
					reg_val_pair, num_cmds,
					hw_static_info->core_regs.result_addr,
					io_buf->io_addr[0] +
					face_results_offset);
				num_cmds = cam_fd_cdm_write_reg_val_pair(
					reg_val_pair, num_cmds,
					hw_static_info->core_regs.ro_mode,
					0x1);

				req_private->ro_mode_enabled = true;
			} else {
				req_private->ro_mode_enabled = false;
			}
			break;
		}
		case CAM_FD_OUTPUT_PORT_ID_RAW_RESULTS: {
			size_required =
				hw_static_info->results.raw_results_entries *
				sizeof(uint32_t);

			if (io_buf->io_cfg->planes[0].plane_stride <
				size_required) {
				CAM_ERR(CAM_FD, "Invalid results size %d %d",
					io_buf->io_cfg->planes[0].plane_stride,
					size_required);
				return -EINVAL;
			}

			req_private->raw_results =
				(uint32_t *)io_buf->cpu_addr[0];
			break;
		}
		case CAM_FD_OUTPUT_PORT_ID_WORK_BUFFER: {
			if ((num_cmds + 2) > CAM_FD_REG_VAL_PAIR_SIZE) {
				CAM_ERR(CAM_FD,
					"Invalid reg_val pair size %d, %d",
					num_cmds, CAM_FD_REG_VAL_PAIR_SIZE);
				return -EINVAL;
			}

			num_cmds = cam_fd_cdm_write_reg_val_pair(
				reg_val_pair, num_cmds,
				hw_static_info->core_regs.work_addr,
				io_buf->io_addr[0]);

			work_buffer_configured = true;
			break;
		}
		default:
			CAM_ERR(CAM_FD, "Invalid resource type %d",
				io_buf->io_cfg->resource_type);
			return -EINVAL;
		}
	}

	if (!req_private->fd_results || !work_buffer_configured) {
		CAM_ERR(CAM_FD, "Invalid IO Buffers results=%pK work=%d",
			req_private->fd_results, work_buffer_configured);
		return -EINVAL;
	}

	/* First insert CHANGE_BASE command */
	size = ctx_hw_private->cdm_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > available_size) {
		CAM_ERR(CAM_FD, "buf size:%d is not sufficient, expected: %d",
			prestart_args->size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(soc_info,
		((struct cam_fd_soc_private *)soc_info->soc_private)->
		regbase_index[CAM_FD_REG_CORE]);

	ctx_hw_private->cdm_ops->cdm_write_changebase(cmd_buf_addr, mem_base);
	cmd_buf_addr += size;
	available_size -= (size * 4);

	size = ctx_hw_private->cdm_ops->cdm_required_size_reg_random(
		num_cmds/2);
	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > available_size) {
		CAM_ERR(CAM_FD, "Insufficient size:%d , expected size:%d",
			available_size, size);
		return -ENOMEM;
	}
	ctx_hw_private->cdm_ops->cdm_write_regrandom(cmd_buf_addr, num_cmds/2,
		reg_val_pair);
	cmd_buf_addr += size;
	available_size -= (size * 4);

	/* Update pre_config_buf_size in bytes */
	prestart_args->pre_config_buf_size =
		prestart_args->size - available_size;

	/* Insert start trigger command into CDM as post config commands. */
	num_cmds = cam_fd_cdm_write_reg_val_pair(reg_val_pair, 0,
		hw_static_info->core_regs.control, 0x2);
	size = ctx_hw_private->cdm_ops->cdm_required_size_reg_random(
		num_cmds/2);
	if ((size * 4) > available_size) {
		CAM_ERR(CAM_FD, "Insufficient size:%d , expected size:%d",
			available_size, size);
		return -ENOMEM;
	}
	ctx_hw_private->cdm_ops->cdm_write_regrandom(cmd_buf_addr, num_cmds/2,
		reg_val_pair);
	cmd_buf_addr += size;
	available_size -= (size * 4);

	prestart_args->post_config_buf_size = size * 4;

	CAM_DBG(CAM_FD, "PreConfig [%pK %d], PostConfig[%pK %d]",
		prestart_args->cmd_buf_addr, prestart_args->pre_config_buf_size,
		cmd_buf_addr, prestart_args->post_config_buf_size);

	for (i = 0; i < (prestart_args->pre_config_buf_size +
		prestart_args->post_config_buf_size) / 4; i++)
		CAM_DBG(CAM_FD, "CDM KMD Commands [%d] : [%pK] [0x%x]", i,
			&prestart_args->cmd_buf_addr[i],
			prestart_args->cmd_buf_addr[i]);

	return 0;
}

static int cam_fd_hw_util_processcmd_frame_done(struct cam_hw_info *fd_hw,
	struct cam_fd_hw_frame_done_args *frame_done_args)
{
	struct cam_fd_core *fd_core = (struct cam_fd_core *)fd_hw->core_info;
	struct cam_fd_hw_static_info *hw_static_info = fd_core->hw_static_info;
	struct cam_fd_hw_req_private *req_private;
	uint32_t base, face_cnt;
	uint32_t *buffer;
	unsigned long flags;
	int i;

	mutex_lock(&fd_hw->hw_mutex);
	spin_lock_irqsave(&fd_core->spin_lock, flags);
	if ((fd_core->core_state != CAM_FD_CORE_STATE_IDLE) ||
		(fd_core->results_valid == false) ||
		!fd_core->hw_req_private) {
		CAM_ERR(CAM_FD,
			"Invalid state for results state=%d, results=%d %pK",
			fd_core->core_state, fd_core->results_valid,
			fd_core->hw_req_private);
		spin_unlock_irqrestore(&fd_core->spin_lock, flags);
		mutex_unlock(&fd_hw->hw_mutex);
		return -EINVAL;
	}
	fd_core->core_state = CAM_FD_CORE_STATE_READING_RESULTS;
	req_private = fd_core->hw_req_private;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	/*
	 * Copy the register value as is into output buffers.
	 * Wehter we are copying the output data by reading registers or
	 * programming output buffer directly to HW must be transparent to UMD.
	 * In case HW supports writing face count value directly into
	 * DDR memory in future, these values should match.
	 */
	req_private->fd_results->face_count =
		cam_fd_soc_register_read(&fd_hw->soc_info, CAM_FD_REG_CORE,
		hw_static_info->core_regs.result_cnt);

	face_cnt = req_private->fd_results->face_count & 0x3F;

	if (face_cnt > hw_static_info->results.max_faces) {
		CAM_WARN(CAM_FD, "Face count greater than max %d %d",
			face_cnt, hw_static_info->results.max_faces);
		face_cnt = hw_static_info->results.max_faces;
	}

	CAM_DBG(CAM_FD, "ReqID[%lld] Faces Detected = %d",
		req_private->request_id, face_cnt);

	/*
	 * We need to read the face data information from registers only
	 * if one of below is true
	 * 1. RO mode is not set. i.e FD HW doesn't write face data into
	 *    DDR memory
	 * 2. On the current chipset, results written into DDR memory by FD HW
	 *    are not gauranteed to be correct
	 */
	if (!req_private->ro_mode_enabled ||
		hw_static_info->enable_errata_wa.ro_mode_results_invalid) {
		buffer = (uint32_t *)&req_private->fd_results->faces[0];
		base = hw_static_info->core_regs.results_reg_base;

		/*
		 * Write register values as is into face data buffer.  Its UMD
		 * driver responsibility to interpret the data and extract face
		 * properties from output buffer. Think in case output buffer
		 * is directly programmed to HW, then KMD has no control to
		 * extract the face properties and UMD anyway has to extract
		 * face properties. So we follow the same approach and keep
		 * this transparent to UMD.
		 */
		for (i = 0;
			i < (face_cnt *
			hw_static_info->results.per_face_entries); i++) {
			*buffer = cam_fd_soc_register_read(&fd_hw->soc_info,
				CAM_FD_REG_CORE, base + (i * 0x4));
			CAM_DBG(CAM_FD, "FaceData[%d] : 0x%x", i / 4, *buffer);
			buffer++;
		}
	}

	if (req_private->get_raw_results &&
		req_private->raw_results &&
		hw_static_info->results.raw_results_available) {
		buffer = req_private->raw_results;
		base = hw_static_info->core_regs.raw_results_reg_base;

		for (i = 0;
			i < hw_static_info->results.raw_results_entries;
			i++) {
			*buffer = cam_fd_soc_register_read(&fd_hw->soc_info,
				CAM_FD_REG_CORE, base + (i * 0x4));
			CAM_DBG(CAM_FD, "RawData[%d] : 0x%x", i, *buffer);
			buffer++;
		}
	}

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_core->hw_req_private = NULL;
	fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);
	mutex_unlock(&fd_hw->hw_mutex);

	return 0;
}

irqreturn_t cam_fd_hw_irq(int irq_num, void *data)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)data;
	struct cam_fd_core *fd_core;
	struct cam_hw_soc_info *soc_info;
	struct cam_fd_hw_static_info *hw_static_info;
	uint32_t reg_value;
	enum cam_fd_hw_irq_type irq_type = CAM_FD_IRQ_FRAME_DONE;
	uint32_t num_irqs = 0;

	if (!fd_hw) {
		CAM_ERR(CAM_FD, "Invalid data in IRQ callback");
		return IRQ_NONE;
	}

	fd_core = (struct cam_fd_core *) fd_hw->core_info;
	soc_info = &fd_hw->soc_info;
	hw_static_info = fd_core->hw_static_info;

	reg_value = cam_fd_soc_register_read(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.irq_status);

	CAM_DBG(CAM_FD, "FD IRQ status 0x%x", reg_value);

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.irq_clear,
		reg_value);

	if (reg_value & CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_HALT_DONE)) {
		complete_all(&fd_core->halt_complete);
		irq_type = CAM_FD_IRQ_HALT_DONE;
		num_irqs++;
	}

	if (reg_value & CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_RESET_DONE)) {
		complete_all(&fd_core->reset_complete);
		irq_type = CAM_FD_IRQ_RESET_DONE;
		num_irqs++;
	}

	if (reg_value & CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_FRAME_DONE)) {
		complete_all(&fd_core->processing_complete);
		irq_type = CAM_FD_IRQ_FRAME_DONE;
		num_irqs++;
	}

	/*
	 * We should never get an IRQ callback with no or more than one mask.
	 * Validate first to make sure nothing going wrong.
	 */
	if (num_irqs != 1) {
		CAM_ERR(CAM_FD,
			"Invalid number of IRQs, value=0x%x, num_irqs=%d",
			reg_value, num_irqs);
		return IRQ_NONE;
	}

	trace_cam_irq_activated("FD", irq_type);

	if (irq_type == CAM_FD_IRQ_HALT_DONE) {
		/*
		 * Do not send HALT IRQ callback to Hw Mgr,
		 * a reset would always follow
		 */
		return IRQ_HANDLED;
	}

	spin_lock(&fd_core->spin_lock);
	/* Do not change state to IDLE on HALT IRQ. Reset must follow halt */
	if ((irq_type == CAM_FD_IRQ_RESET_DONE) ||
		(irq_type == CAM_FD_IRQ_FRAME_DONE)) {

		fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
		if (irq_type == CAM_FD_IRQ_FRAME_DONE)
			fd_core->results_valid = true;

		CAM_DBG(CAM_FD, "FD IRQ type %d, state=%d",
			irq_type, fd_core->core_state);
	}
	spin_unlock(&fd_core->spin_lock);

	if (fd_core->irq_cb.cam_fd_hw_mgr_cb)
		fd_core->irq_cb.cam_fd_hw_mgr_cb(fd_core->irq_cb.data,
			irq_type);

	return IRQ_HANDLED;
}

int cam_fd_hw_get_hw_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	struct cam_fd_core *fd_core;
	struct cam_fd_hw_caps *fd_hw_caps =
		(struct cam_fd_hw_caps *)get_hw_cap_args;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_FD, "Invalid input pointers %pK %pK",
			hw_priv, get_hw_cap_args);
		return -EINVAL;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;
	*fd_hw_caps = fd_core->hw_caps;

	CAM_DBG(CAM_FD, "core:%d.%d wrapper:%d.%d mode:%d, raw:%d",
		fd_hw_caps->core_version.major,
		fd_hw_caps->core_version.minor,
		fd_hw_caps->wrapper_version.major,
		fd_hw_caps->wrapper_version.minor,
		fd_hw_caps->supported_modes,
		fd_hw_caps->raw_results_available);

	return 0;
}

int cam_fd_hw_init(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	struct cam_fd_core *fd_core;
	struct cam_fd_hw_init_args *init_args =
		(struct cam_fd_hw_init_args *)init_hw_args;
	int rc = 0;
	unsigned long flags;

	if (!fd_hw || !init_args) {
		CAM_ERR(CAM_FD, "Invalid argument %pK %pK", fd_hw, init_args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_fd_hw_init_args)) {
		CAM_ERR(CAM_FD, "Invalid arg size %u, %zu", arg_size,
			sizeof(struct cam_fd_hw_init_args));
		return -EINVAL;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;

	mutex_lock(&fd_hw->hw_mutex);
	CAM_DBG(CAM_FD, "FD HW Init ref count before %d", fd_hw->open_count);

	if (fd_hw->open_count > 0) {
		rc = 0;
		goto cdm_streamon;
	}

	rc = cam_fd_soc_enable_resources(&fd_hw->soc_info);
	if (rc) {
		CAM_ERR(CAM_FD, "Enable SOC failed, rc=%d", rc);
		goto unlock_return;
	}

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_hw->hw_state = CAM_HW_STATE_POWER_UP;
	fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	rc = cam_fd_hw_reset(hw_priv, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_FD, "Reset Failed, rc=%d", rc);
		goto disable_soc;
	}

	cam_fd_hw_util_enable_power_on_settings(fd_hw);

cdm_streamon:
	fd_hw->open_count++;
	CAM_DBG(CAM_FD, "FD HW Init ref count after %d", fd_hw->open_count);

	if (init_args->ctx_hw_private) {
		struct cam_fd_ctx_hw_private *ctx_hw_private =
			init_args->ctx_hw_private;

		rc = cam_cdm_stream_on(ctx_hw_private->cdm_handle);
		if (rc) {
			CAM_ERR(CAM_FD, "CDM StreamOn fail :handle=0x%x, rc=%d",
				ctx_hw_private->cdm_handle, rc);
			fd_hw->open_count--;
			if (!fd_hw->open_count)
				goto disable_soc;
		}
	}

	mutex_unlock(&fd_hw->hw_mutex);

	return rc;

disable_soc:
	if (cam_fd_soc_disable_resources(&fd_hw->soc_info))
		CAM_ERR(CAM_FD, "Error in disable soc resources");

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	fd_core->core_state = CAM_FD_CORE_STATE_POWERDOWN;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);
unlock_return:
	mutex_unlock(&fd_hw->hw_mutex);
	return rc;
}

int cam_fd_hw_deinit(void *hw_priv, void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = hw_priv;
	struct cam_fd_core *fd_core = NULL;
	struct cam_fd_hw_deinit_args *deinit_args =
		(struct cam_fd_hw_deinit_args *)deinit_hw_args;
	int rc = 0;
	unsigned long flags;

	if (!fd_hw || !deinit_hw_args) {
		CAM_ERR(CAM_FD, "Invalid argument");
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_fd_hw_deinit_args)) {
		CAM_ERR(CAM_FD, "Invalid arg size %u, %zu", arg_size,
			sizeof(struct cam_fd_hw_deinit_args));
		return -EINVAL;
	}

	mutex_lock(&fd_hw->hw_mutex);
	if (fd_hw->open_count == 0) {
		mutex_unlock(&fd_hw->hw_mutex);
		CAM_ERR(CAM_FD, "Error Unbalanced deinit");
		return -EFAULT;
	}

	fd_hw->open_count--;
	CAM_DBG(CAM_FD, "FD HW ref count=%d", fd_hw->open_count);

	if (fd_hw->open_count > 0) {
		rc = 0;
		goto positive_ref_cnt;
	}

	rc = cam_fd_soc_disable_resources(&fd_hw->soc_info);
	if (rc)
		CAM_ERR(CAM_FD, "Failed in Disable SOC, rc=%d", rc);

	fd_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	fd_core = (struct cam_fd_core *)fd_hw->core_info;

	/* With the ref_cnt correct, this should never happen */
	WARN_ON(!fd_core);

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_core->core_state = CAM_FD_CORE_STATE_POWERDOWN;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);
positive_ref_cnt:
	if (deinit_args->ctx_hw_private) {
		struct cam_fd_ctx_hw_private *ctx_hw_private =
			deinit_args->ctx_hw_private;

		rc = cam_cdm_stream_off(ctx_hw_private->cdm_handle);
		if (rc) {
			CAM_ERR(CAM_FD,
				"Failed in CDM StreamOff, handle=0x%x, rc=%d",
				ctx_hw_private->cdm_handle, rc);
		}
	}

	mutex_unlock(&fd_hw->hw_mutex);
	return rc;
}

int cam_fd_hw_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	struct cam_fd_core *fd_core;
	struct cam_fd_hw_static_info *hw_static_info;
	struct cam_hw_soc_info *soc_info;
	unsigned long flags;
	int rc;

	if (!fd_hw) {
		CAM_ERR(CAM_FD, "Invalid input handle");
		return -EINVAL;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;
	hw_static_info = fd_core->hw_static_info;
	soc_info = &fd_hw->soc_info;

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	if ((fd_core->core_state == CAM_FD_CORE_STATE_POWERDOWN) ||
		(fd_core->core_state == CAM_FD_CORE_STATE_RESET_PROGRESS)) {
		CAM_ERR(CAM_FD, "Reset not allowed in %d state",
			fd_core->core_state);
		spin_unlock_irqrestore(&fd_core->spin_lock, flags);
		return -EINVAL;
	}

	fd_core->results_valid = false;
	fd_core->core_state = CAM_FD_CORE_STATE_RESET_PROGRESS;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.cgc_disable, 0x1);

	rc = cam_fd_hw_util_fdwrapper_halt(fd_hw);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in HALT rc=%d", rc);
		return rc;
	}

	rc = cam_fd_hw_util_fdwrapper_sync_reset(fd_hw);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in RESET rc=%d", rc);
		return rc;
	}

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.cgc_disable, 0x0);

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	return rc;
}

int cam_fd_hw_start(void *hw_priv, void *hw_start_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	struct cam_fd_core *fd_core;
	struct cam_fd_hw_static_info *hw_static_info;
	struct cam_fd_hw_cmd_start_args *start_args =
		(struct cam_fd_hw_cmd_start_args *)hw_start_args;
	struct cam_fd_ctx_hw_private *ctx_hw_private;
	unsigned long flags;
	int rc;

	if (!hw_priv || !start_args) {
		CAM_ERR(CAM_FD, "Invalid input args %pK %pK", hw_priv,
			start_args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_fd_hw_cmd_start_args)) {
		CAM_ERR(CAM_FD, "Invalid arg size %u, %zu", arg_size,
			sizeof(struct cam_fd_hw_cmd_start_args));
		return -EINVAL;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;
	hw_static_info = fd_core->hw_static_info;

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	if (fd_core->core_state != CAM_FD_CORE_STATE_IDLE) {
		CAM_ERR(CAM_FD, "Cannot start in %d state",
			fd_core->core_state);
		spin_unlock_irqrestore(&fd_core->spin_lock, flags);
		return -EINVAL;
	}

	/*
	 * We are about to start FD HW processing, save the request
	 * private data which is being processed by HW. Once the frame
	 * processing is finished, process_cmd(FRAME_DONE) should be called
	 * with same hw_req_private as input.
	 */
	fd_core->hw_req_private = start_args->hw_req_private;
	fd_core->core_state = CAM_FD_CORE_STATE_PROCESSING;
	fd_core->results_valid = false;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	ctx_hw_private = start_args->ctx_hw_private;

	/* Before starting HW process, clear processing complete */
	reinit_completion(&fd_core->processing_complete);

	if (hw_static_info->enable_errata_wa.single_irq_only) {
		cam_fd_soc_register_write(&fd_hw->soc_info, CAM_FD_REG_WRAPPER,
			hw_static_info->wrapper_regs.irq_mask,
			CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_FRAME_DONE));
	}

	if (start_args->num_hw_update_entries > 0) {
		struct cam_cdm_bl_request *cdm_cmd = ctx_hw_private->cdm_cmd;
		struct cam_hw_update_entry *cmd;
		int i;

		cdm_cmd->cmd_arrary_count = start_args->num_hw_update_entries;
		cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
		cdm_cmd->flag = false;
		cdm_cmd->userdata = NULL;
		cdm_cmd->cookie = 0;

		for (i = 0 ; i <= start_args->num_hw_update_entries; i++) {
			cmd = (start_args->hw_update_entries + i);
			cdm_cmd->cmd[i].bl_addr.mem_handle = cmd->handle;
			cdm_cmd->cmd[i].offset = cmd->offset;
			cdm_cmd->cmd[i].len = cmd->len;
		}

		rc = cam_cdm_submit_bls(ctx_hw_private->cdm_handle, cdm_cmd);
		if (rc) {
			CAM_ERR(CAM_FD,
				"Failed to submit cdm commands, rc=%d", rc);
			goto error;
		}
	} else {
		CAM_ERR(CAM_FD, "Invalid number of hw update entries");
		rc = -EINVAL;
		goto error;
	}

	return 0;
error:
	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	return rc;
}

int cam_fd_hw_halt_reset(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	struct cam_fd_core *fd_core;
	struct cam_fd_hw_static_info *hw_static_info;
	struct cam_hw_soc_info *soc_info;
	unsigned long flags;
	int rc;

	if (!fd_hw) {
		CAM_ERR(CAM_FD, "Invalid input handle");
		return -EINVAL;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;
	hw_static_info = fd_core->hw_static_info;
	soc_info = &fd_hw->soc_info;

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	if ((fd_core->core_state == CAM_FD_CORE_STATE_POWERDOWN) ||
		(fd_core->core_state == CAM_FD_CORE_STATE_RESET_PROGRESS)) {
		CAM_ERR(CAM_FD, "Reset not allowed in %d state",
			fd_core->core_state);
		spin_unlock_irqrestore(&fd_core->spin_lock, flags);
		return -EINVAL;
	}

	fd_core->results_valid = false;
	fd_core->core_state = CAM_FD_CORE_STATE_RESET_PROGRESS;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.cgc_disable, 0x1);

	rc = cam_fd_hw_util_fdwrapper_halt(fd_hw);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in HALT rc=%d", rc);
		return rc;
	}

	/* HALT must be followed by RESET */
	rc = cam_fd_hw_util_fdwrapper_sync_reset(fd_hw);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in RESET rc=%d", rc);
		return rc;
	}

	cam_fd_soc_register_write(soc_info, CAM_FD_REG_WRAPPER,
		hw_static_info->wrapper_regs.cgc_disable, 0x0);

	spin_lock_irqsave(&fd_core->spin_lock, flags);
	fd_core->core_state = CAM_FD_CORE_STATE_IDLE;
	spin_unlock_irqrestore(&fd_core->spin_lock, flags);

	return rc;
}

int cam_fd_hw_reserve(void *hw_priv, void *hw_reserve_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	int rc = -EINVAL;
	struct cam_fd_ctx_hw_private *ctx_hw_private;
	struct cam_fd_hw_reserve_args *reserve_args =
		(struct cam_fd_hw_reserve_args *)hw_reserve_args;
	struct cam_cdm_acquire_data cdm_acquire;
	struct cam_cdm_bl_request *cdm_cmd;
	int i;

	if (!fd_hw || !reserve_args) {
		CAM_ERR(CAM_FD, "Invalid input %pK, %pK", fd_hw, reserve_args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_fd_hw_reserve_args)) {
		CAM_ERR(CAM_FD, "Invalid arg size %u, %zu", arg_size,
			sizeof(struct cam_fd_hw_reserve_args));
		return -EINVAL;
	}

	cdm_cmd = kzalloc(((sizeof(struct cam_cdm_bl_request)) +
			((CAM_FD_MAX_HW_ENTRIES - 1) *
			sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);
	if (!cdm_cmd)
		return -ENOMEM;

	ctx_hw_private = kzalloc(sizeof(struct cam_fd_ctx_hw_private),
		GFP_KERNEL);
	if (!ctx_hw_private) {
		kfree(cdm_cmd);
		return -ENOMEM;
	}

	memset(&cdm_acquire, 0, sizeof(cdm_acquire));
	strlcpy(cdm_acquire.identifier, "fd", sizeof("fd"));
	cdm_acquire.cell_index = fd_hw->soc_info.index;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = ctx_hw_private;
	cdm_acquire.cam_cdm_callback = cam_fd_hw_util_cdm_callback;
	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.base_array_cnt = fd_hw->soc_info.num_reg_map;
	for (i = 0; i < fd_hw->soc_info.num_reg_map; i++)
		cdm_acquire.base_array[i] = &fd_hw->soc_info.reg_map[i];

	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed to acquire the CDM HW");
		goto error;
	}

	ctx_hw_private->hw_ctx = reserve_args->hw_ctx;
	ctx_hw_private->fd_hw = fd_hw;
	ctx_hw_private->mode = reserve_args->mode;
	ctx_hw_private->cdm_handle = cdm_acquire.handle;
	ctx_hw_private->cdm_ops = cdm_acquire.ops;
	ctx_hw_private->cdm_cmd = cdm_cmd;

	reserve_args->ctx_hw_private = ctx_hw_private;

	CAM_DBG(CAM_FD, "private=%pK, hw_ctx=%pK, mode=%d, cdm_handle=0x%x",
		ctx_hw_private, ctx_hw_private->hw_ctx, ctx_hw_private->mode,
		ctx_hw_private->cdm_handle);

	return 0;
error:
	kfree(ctx_hw_private);
	kfree(cdm_cmd);
	return rc;
}

int cam_fd_hw_release(void *hw_priv, void *hw_release_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	int rc = -EINVAL;
	struct cam_fd_ctx_hw_private *ctx_hw_private;
	struct cam_fd_hw_release_args *release_args =
		(struct cam_fd_hw_release_args *)hw_release_args;

	if (!fd_hw || !release_args) {
		CAM_ERR(CAM_FD, "Invalid input %pK, %pK", fd_hw, release_args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_fd_hw_release_args)) {
		CAM_ERR(CAM_FD, "Invalid arg size %u, %zu", arg_size,
			sizeof(struct cam_fd_hw_release_args));
		return -EINVAL;
	}

	ctx_hw_private =
		(struct cam_fd_ctx_hw_private *)release_args->ctx_hw_private;

	rc = cam_cdm_release(ctx_hw_private->cdm_handle);
	if (rc)
		CAM_ERR(CAM_FD, "Release cdm handle failed, handle=0x%x, rc=%d",
			ctx_hw_private->cdm_handle, rc);

	kfree(ctx_hw_private->cdm_cmd);
	kfree(ctx_hw_private);
	release_args->ctx_hw_private = NULL;

	return 0;
}

int cam_fd_hw_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *fd_hw = (struct cam_hw_info *)hw_priv;
	int rc = -EINVAL;

	if (!hw_priv || !cmd_args ||
		(cmd_type >= CAM_FD_HW_CMD_MAX)) {
		CAM_ERR(CAM_FD, "Invalid arguments %pK %pK %d", hw_priv,
			cmd_args, cmd_type);
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_FD_HW_CMD_REGISTER_CALLBACK: {
		struct cam_fd_hw_cmd_set_irq_cb *irq_cb_args;
		struct cam_fd_core *fd_core =
			(struct cam_fd_core *)fd_hw->core_info;

		if (sizeof(struct cam_fd_hw_cmd_set_irq_cb) != arg_size) {
			CAM_ERR(CAM_FD, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		irq_cb_args = (struct cam_fd_hw_cmd_set_irq_cb *)cmd_args;
		fd_core->irq_cb.cam_fd_hw_mgr_cb =
			irq_cb_args->cam_fd_hw_mgr_cb;
		fd_core->irq_cb.data = irq_cb_args->data;
		rc = 0;
		break;
	}
	case CAM_FD_HW_CMD_PRESTART: {
		struct cam_fd_hw_cmd_prestart_args *prestart_args;

		if (sizeof(struct cam_fd_hw_cmd_prestart_args) != arg_size) {
			CAM_ERR(CAM_FD, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		prestart_args = (struct cam_fd_hw_cmd_prestart_args *)cmd_args;
		rc = cam_fd_hw_util_processcmd_prestart(fd_hw, prestart_args);
		break;
	}
	case CAM_FD_HW_CMD_FRAME_DONE: {
		struct cam_fd_hw_frame_done_args *cmd_frame_results;

		if (sizeof(struct cam_fd_hw_frame_done_args) !=
			arg_size) {
			CAM_ERR(CAM_FD, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		cmd_frame_results =
			(struct cam_fd_hw_frame_done_args *)cmd_args;
		rc = cam_fd_hw_util_processcmd_frame_done(fd_hw,
			cmd_frame_results);
		break;
	}
	default:
		break;
	}

	return rc;
}
