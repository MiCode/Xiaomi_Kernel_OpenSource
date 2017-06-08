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
 *
 */

#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

int msm_comm_vote_bus(struct msm_vidc_core *core)
{
	int rc = 0, vote_data_count = 0, i = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;
	struct vidc_bus_vote_data *vote_data = NULL;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list)
		++vote_data_count;

	vote_data = kcalloc(vote_data_count, sizeof(*vote_data),
			GFP_TEMPORARY);
	if (!vote_data) {
		dprintk(VIDC_ERR, "%s: failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail_alloc;
	}

	list_for_each_entry(inst, &core->instances, list) {
		int codec = 0, yuv = 0;

		codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT].fourcc :
			inst->fmts[CAPTURE_PORT].fourcc;

		yuv = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[CAPTURE_PORT].fourcc :
			inst->fmts[OUTPUT_PORT].fourcc;

		vote_data[i].domain = get_hal_domain(inst->session_type);
		vote_data[i].codec = get_hal_codec(codec);
		vote_data[i].width =  max(inst->prop.width[CAPTURE_PORT],
				inst->prop.width[OUTPUT_PORT]);
		vote_data[i].height = max(inst->prop.height[CAPTURE_PORT],
				inst->prop.height[OUTPUT_PORT]);

		if (inst->clk_data.operating_rate)
			vote_data[i].fps =
				(inst->clk_data.operating_rate >> 16) ?
				inst->clk_data.operating_rate >> 16 : 1;
		else
			vote_data[i].fps = inst->prop.fps;

		if (!msm_vidc_clock_scaling ||
			inst->clk_data.buffer_counter < DCVS_FTB_WINDOW)
			vote_data[i].power_mode = VIDC_POWER_TURBO;

		/*
		 * TODO: support for OBP-DBP split mode hasn't been yet
		 * implemented, once it is, this part of code needs to be
		 * revisited since passing in accurate information to the bus
		 * governor will drastically reduce bandwidth
		 */
		//vote_data[i].color_formats[0] = get_hal_uncompressed(yuv);
		vote_data[i].num_formats = 1;
		i++;
	}
	mutex_unlock(&core->lock);

	rc = call_hfi_op(hdev, vote_bus, hdev->hfi_device_data, vote_data,
			vote_data_count);
	if (rc)
		dprintk(VIDC_ERR, "Failed to scale bus: %d\n", rc);

	kfree(vote_data);
	return rc;

fail_alloc:
	mutex_unlock(&core->lock);
	return rc;
}

static inline int get_pending_bufs_fw(struct msm_vidc_inst *inst)
{
	int fw_out_qsize = 0, buffers_in_driver = 0;

	/*
	 * DCVS always operates on Uncompressed buffers.
	 * For Decoders, FTB and Encoders, ETB.
	 */

	if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_STOP_DONE) {
		if (inst->session_type == MSM_VIDC_DECODER)
			fw_out_qsize = inst->count.ftb - inst->count.fbd;
		else
			fw_out_qsize = inst->count.etb - inst->count.ebd;

		buffers_in_driver = inst->buffers_held_in_driver;
	}

	return fw_out_qsize + buffers_in_driver;
}

static int msm_dcvs_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fw_pending_bufs = 0;
	int total_output_buf = 0;
	int buffers_outside_fw = 0;
	struct msm_vidc_core *core;
	struct hal_buffer_requirements *output_buf_req;
	struct clock_data *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->clk_data.dcvs_mode) {
		dprintk(VIDC_DBG, "DCVS is not enabled\n");
		return 0;
	}

	dcvs = &inst->clk_data;

	core = inst->core;
	mutex_lock(&inst->lock);
	fw_pending_bufs = get_pending_bufs_fw(inst);

	output_buf_req = get_buff_req_buffer(inst,
			dcvs->buffer_type);
	mutex_unlock(&inst->lock);
	if (!output_buf_req) {
		dprintk(VIDC_ERR,
				"%s: No buffer requirement for buffer type %x\n",
				__func__, dcvs->buffer_type);
		return -EINVAL;
	}

	/* Total number of output buffers */
	total_output_buf = output_buf_req->buffer_count_actual;

	/* Buffers outside FW are with display */
	buffers_outside_fw = total_output_buf - fw_pending_bufs;
	dprintk(VIDC_DBG,
		"Counts : total_output_buf = %d fw_pending_bufs = %d buffers_outside_fw = %d\n",
		total_output_buf, fw_pending_bufs, buffers_outside_fw);

	if (buffers_outside_fw >=  dcvs->min_threshold &&
			dcvs->load > dcvs->load_low) {
		dcvs->load = dcvs->load_low;
	} else if (buffers_outside_fw < dcvs->min_threshold &&
			dcvs->load == dcvs->load_low) {
		dcvs->load = dcvs->load_high;
	}
	return rc;
}

static void msm_vidc_update_freq_entry(struct msm_vidc_inst *inst,
	unsigned long freq, ion_phys_addr_t device_addr)
{
	struct vidc_freq_data *temp, *next;
	bool found = false;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry_safe(temp, next, &inst->freqs.list, list) {
		if (temp->device_addr == device_addr) {
			temp->freq = freq;
			found = true;
			break;
		}
	}

	if (!found) {
		temp = kzalloc(sizeof(*temp), GFP_KERNEL);
		temp->freq = freq;
		temp->device_addr = device_addr;
		list_add_tail(&temp->list, &inst->freqs.list);
	}
	mutex_unlock(&inst->freqs.lock);
}

// TODO this needs to be removed later and use queued_list

void msm_vidc_clear_freq_entry(struct msm_vidc_inst *inst,
	ion_phys_addr_t device_addr)
{
	struct vidc_freq_data *temp, *next;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry_safe(temp, next, &inst->freqs.list, list) {
		if (temp->device_addr == device_addr)
			temp->freq = 0;
	}
	mutex_unlock(&inst->freqs.lock);

	inst->clk_data.buffer_counter++;
}


static unsigned long msm_vidc_adjust_freq(struct msm_vidc_inst *inst)
{
	struct vidc_freq_data *temp;
	unsigned long freq = 0;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry(temp, &inst->freqs.list, list) {
		freq = max(freq, temp->freq);
	}
	mutex_unlock(&inst->freqs.lock);

	/* If current requirement is within DCVS limits, try DCVS. */

	if (freq < inst->clk_data.load_high) {
		dprintk(VIDC_DBG, "Calling DCVS now\n");
		// TODO calling DCVS here may reduce the residency. Re-visit.
		msm_dcvs_scale_clocks(inst);
		freq = inst->clk_data.load;
	}
	dprintk(VIDC_PROF, "%s Inst %pK : Freq = %lu\n", __func__, inst, freq);

	return freq;
}

void msm_comm_free_freq_table(struct msm_vidc_inst *inst)
{
	struct vidc_freq_data *temp, *next;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry_safe(temp, next, &inst->freqs.list, list) {
		list_del(&temp->list);
		kfree(temp);
	}
	INIT_LIST_HEAD(&inst->freqs.list);
	mutex_unlock(&inst->freqs.lock);
}


static inline int msm_dcvs_get_mbs_per_frame(struct msm_vidc_inst *inst)
{
	int height, width;

	if (!inst->in_reconfig) {
		height = max(inst->prop.height[CAPTURE_PORT],
			inst->prop.height[OUTPUT_PORT]);
		width = max(inst->prop.width[CAPTURE_PORT],
			inst->prop.width[OUTPUT_PORT]);
	} else {
		height = inst->reconfig_height;
		width = inst->reconfig_width;
	}

	return NUM_MBS_PER_FRAME(height, width);
}

static unsigned long msm_vidc_calc_freq(struct msm_vidc_inst *inst,
	u32 filled_len)
{
	unsigned long freq = 0;
	unsigned long vpp_cycles = 0, vsp_cycles = 0;
	u32 vpp_cycles_per_mb;
	u32 mbs_per_second;

	mbs_per_second = msm_comm_get_inst_load(inst,
		LOAD_CALC_NO_QUIRKS);

	/*
	 * Calculate vpp, vsp cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;

		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->clk_data.bitrate * 10) / 7;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		vpp_cycles = mbs_per_second * inst->clk_data.entry->vpp_cycles;

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;
		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->prop.fps * filled_len * 8 * 10) / 7;

	} else {
		// TODO return Min or Max ?
		dprintk(VIDC_ERR, "Unknown session type = %s\n", __func__);
		return freq;
	}

	freq = max(vpp_cycles, vsp_cycles);

	dprintk(VIDC_PROF, "%s Inst %pK : Freq = %lu\n", __func__, inst, freq);

	return freq;
}

static int msm_vidc_set_clocks(struct msm_vidc_core *core)
{
	struct hfi_device *hdev;
	unsigned long freq = 0, rate = 0;
	struct msm_vidc_inst *temp = NULL;
	int rc = 0, i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;

	hdev = core->device;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (!allowed_clks_tbl) {
		dprintk(VIDC_ERR,
			"%s Invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		freq += temp->clk_data.curr_freq;
	}
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq)
			break;
	}
	core->min_freq = freq;
	core->curr_freq = rate;
	mutex_unlock(&core->lock);

	dprintk(VIDC_PROF, "Min freq = %lu Current Freq = %lu\n",
		core->min_freq, core->curr_freq);
	rc = call_hfi_op(hdev, scale_clocks,
			hdev->hfi_device_data, core->curr_freq);

	return rc;
}

static unsigned long msm_vidc_max_freq(struct msm_vidc_core *core)
{
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	unsigned long freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	freq = allowed_clks_tbl[0].clock_rate;
	dprintk(VIDC_PROF, "Max rate = %lu", freq);

	return freq;
}

int msm_vidc_update_operating_rate(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;
	struct msm_vidc_inst *temp;
	struct msm_vidc_core *core;
	unsigned long max_freq, freq_left, ops_left, load, cycles, freq = 0;
	unsigned long mbs_per_second;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	mutex_lock(&core->lock);
	max_freq = msm_vidc_max_freq(core);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp == inst ||
				temp->state < MSM_VIDC_START_DONE ||
				temp->state >= MSM_VIDC_RELEASE_RESOURCES_DONE)
			continue;

		freq += temp->clk_data.min_freq;
	}

	freq_left = max_freq - freq;

	list_for_each_entry(temp, &core->instances, list) {

		if (!temp ||
				temp->state < MSM_VIDC_START_DONE ||
				temp->state >= MSM_VIDC_RELEASE_RESOURCES_DONE)
			continue;

		mbs_per_second = msm_comm_get_inst_load(temp,
		LOAD_CALC_NO_QUIRKS);

		cycles = temp->clk_data.entry->vpp_cycles;
		if (temp->session_type == MSM_VIDC_ENCODER)
			cycles = temp->flags & VIDC_LOW_POWER ?
				temp->clk_data.entry->low_power_cycles :
				cycles;

		load = cycles * mbs_per_second;

		ops_left = load ? (freq_left / load) : 0;
		/* Convert remaining operating rate to Q16 format */
		ops_left = ops_left << 16;

		ctrl = v4l2_ctrl_find(&temp->ctrl_handler,
			V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE);
		if (ctrl) {
			dprintk(VIDC_DBG,
				"%s: Before Range = %lld --> %lld\n",
				ctrl->name, ctrl->minimum, ctrl->maximum);
			dprintk(VIDC_DBG,
				"%s: Before Def value = %lld Cur val = %d\n",
				ctrl->name, ctrl->default_value, ctrl->val);
			v4l2_ctrl_modify_range(ctrl, ctrl->minimum,
				ctrl->val + ops_left, ctrl->step,
				ctrl->default_value);
			dprintk(VIDC_DBG,
				"%s: Updated Range = %lld --> %lld\n",
				ctrl->name, ctrl->minimum, ctrl->maximum);
			dprintk(VIDC_DBG,
				"%s: Updated Def value = %lld Cur val = %d\n",
				ctrl->name, ctrl->default_value, ctrl->val);
		}
	}
	mutex_unlock(&core->lock);

	return 0;
}

int msm_comm_scale_clocks(struct msm_vidc_inst *inst)
{
	struct vb2_buf_entry *temp, *next;
	unsigned long freq = 0;
	u32 filled_len = 0;
	ion_phys_addr_t device_addr = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	mutex_lock(&inst->pendingq.lock);
	list_for_each_entry_safe(temp, next, &inst->pendingq.list, list) {
		if (temp->vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			filled_len = max(filled_len,
				temp->vb->planes[0].bytesused);
			device_addr = temp->vb->planes[0].m.userptr;
		}
	}
	mutex_unlock(&inst->pendingq.lock);

	if (!filled_len || !device_addr) {
		dprintk(VIDC_PROF, "No Change in frequency\n");
		goto decision_done;
	}

	freq = msm_vidc_calc_freq(inst, filled_len);

	msm_vidc_update_freq_entry(inst, freq, device_addr);

	freq = msm_vidc_adjust_freq(inst);

	inst->clk_data.min_freq = freq;

	if (inst->clk_data.buffer_counter < DCVS_FTB_WINDOW ||
		!msm_vidc_clock_scaling)
		inst->clk_data.curr_freq = msm_vidc_max_freq(inst->core);
	else
		inst->clk_data.curr_freq = freq;

decision_done:
	msm_vidc_set_clocks(inst->core);
	return 0;
}

int msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	hdev = core->device;

	if (msm_comm_scale_clocks(inst)) {
		dprintk(VIDC_WARN,
			"Failed to scale clocks. Performance might be impacted\n");
	}
	if (msm_comm_vote_bus(core)) {
		dprintk(VIDC_WARN,
			"Failed to scale DDR bus. Performance might be impacted\n");
	}
	return 0;
}

int msm_dcvs_try_enable(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: Invalid args: %p\n", __func__, inst);
		return -EINVAL;
	}

	if (!msm_vidc_clock_scaling ||
			inst->flags & VIDC_THUMBNAIL ||
			inst->clk_data.low_latency_mode) {
		dprintk(VIDC_PROF,
			"This session doesn't need DCVS : %pK\n",
				inst);
		inst->clk_data.extra_capture_buffer_count = 0;
		inst->clk_data.extra_output_buffer_count = 0;
		inst->clk_data.dcvs_mode = false;
		return false;
	}
	inst->clk_data.dcvs_mode = true;

	// TODO : Update with proper number based on on-target tuning.
	inst->clk_data.extra_capture_buffer_count =
		DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
	inst->clk_data.extra_output_buffer_count =
		DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
	return true;
}

static bool msm_dcvs_check_codec_supported(int fourcc,
		unsigned long codecs_supported, enum session_type type)
{
	int codec_bit, session_type_bit;
	bool codec_type, session_type;
	unsigned long session;

	session = VIDC_VOTE_DATA_SESSION_VAL(get_hal_codec(fourcc),
		get_hal_domain(type));

	if (!codecs_supported || !session)
		return false;

	/* ffs returns a 1 indexed, test_bit takes a 0 indexed...index */
	codec_bit = ffs(session) - 1;
	session_type_bit = codec_bit + 1;

	codec_type =
		test_bit(codec_bit, &codecs_supported) ==
		test_bit(codec_bit, &session);
	session_type =
		test_bit(session_type_bit, &codecs_supported) ==
		test_bit(session_type_bit, &session);

	return codec_type && session_type;
}

int msm_comm_init_clocks_and_bus_data(struct msm_vidc_inst *inst)
{
	int rc = 0, j = 0;
	struct clock_freq_table *clk_freq_tbl = NULL;
	struct clock_profile_entry *entry = NULL;
	int fourcc;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	clk_freq_tbl = &inst->core->resources.clock_freq_tbl;
	fourcc = inst->session_type == MSM_VIDC_DECODER ?
		inst->fmts[OUTPUT_PORT].fourcc :
		inst->fmts[CAPTURE_PORT].fourcc;

	for (j = 0; j < clk_freq_tbl->count; j++) {
		bool matched = false;

		entry = &clk_freq_tbl->clk_prof_entries[j];

		matched = msm_dcvs_check_codec_supported(
				fourcc,
				entry->codec_mask,
				inst->session_type);

		if (matched) {
			inst->clk_data.entry = entry;
			break;
		}
	}

	if (j == clk_freq_tbl->count) {
		dprintk(VIDC_ERR,
			"Failed : No matching clock entry found\n");
		rc = -EINVAL;
	}

	return rc;
}

static inline void msm_dcvs_print_dcvs_stats(struct clock_data *dcvs)
{
	dprintk(VIDC_DBG,
		"DCVS: Load_Low %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_high);

	dprintk(VIDC_DBG,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold, dcvs->max_threshold);
}

void msm_clock_data_reset(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	int i = 0, rc = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 total_freq = 0, rate = 0, load;
	int cycles;
	struct clock_data *dcvs;

	dprintk(VIDC_DBG, "Init DCVS Load\n");

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return;
	}

	core = inst->core;
	dcvs = &inst->clk_data;
	load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);
	cycles = inst->clk_data.entry->vpp_cycles;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		cycles = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			cycles;

		dcvs->buffer_type = HAL_BUFFER_INPUT;
		// TODO : Update with proper no based on Buffer counts change.
		dcvs->min_threshold = 7;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		dcvs->buffer_type = msm_comm_get_hal_output_buffer(inst);
		// TODO : Update with proper no based on Buffer counts change.
		dcvs->min_threshold = 4;
	} else {
		return;
	}

	total_freq = cycles * load;

	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= total_freq)
			break;
	}

	dcvs->load = dcvs->load_high = rate;
	dcvs->load_low = allowed_clks_tbl[i+1].clock_rate;

	inst->clk_data.buffer_counter = 0;

	msm_dcvs_print_dcvs_stats(dcvs);

	msm_vidc_update_operating_rate(inst);

	rc = msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR, "%s Failed to scale Clocks and Bus\n",
			__func__);
}

int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}

	return buffer_type == HAL_BUFFER_INPUT ?
		inst->clk_data.extra_output_buffer_count :
		inst->clk_data.extra_capture_buffer_count;
}

int msm_vidc_decide_work_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_video_work_mode pdata;
	struct hal_enable latency;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;
	if (inst->clk_data.low_latency_mode) {
		pdata.video_work_mode = VIDC_WORK_MODE_1;
		goto decision_done;
	}

	if (inst->session_type == MSM_VIDC_DECODER) {
		pdata.video_work_mode = VIDC_WORK_MODE_2;
		switch (inst->fmts[OUTPUT_PORT].fourcc) {
		case V4L2_PIX_FMT_MPEG2:
			pdata.video_work_mode = VIDC_WORK_MODE_1;
			break;
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_HEVC:
			if (inst->prop.height[OUTPUT_PORT] *
				inst->prop.width[OUTPUT_PORT] <=
					1280 * 720)
				pdata.video_work_mode = VIDC_WORK_MODE_1;
			break;
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		u32 rc_mode = 0;

		pdata.video_work_mode = VIDC_WORK_MODE_1;
		rc_mode =  msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL);
		if (rc_mode == V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_VFR ||
			rc_mode ==
				V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR)
			pdata.video_work_mode = VIDC_WORK_MODE_2;
	} else {
		return -EINVAL;
	}

decision_done:

	inst->clk_data.work_mode = pdata.video_work_mode;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HAL_PARAM_VIDEO_WORK_MODE,
			(void *)&pdata);
	if (rc)
		dprintk(VIDC_WARN,
				" Failed to configure Work Mode %pK\n", inst);

	/* For WORK_MODE_1, set Low Latency mode by default to HW. */

	if (inst->session_type == MSM_VIDC_ENCODER &&
			inst->clk_data.work_mode == VIDC_WORK_MODE_1) {
		latency.enable = 1;
		rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HAL_PARAM_VENC_LOW_LATENCY,
			(void *)&latency);
	}

	rc = msm_comm_scale_clocks_and_bus(inst);

	return rc;
}

static inline int msm_vidc_power_save_mode_enable(struct msm_vidc_inst *inst,
	bool enable)
{
	u32 rc = 0, mbs_per_frame;
	u32 prop_id = 0;
	void *pdata = NULL;
	struct hfi_device *hdev = NULL;
	enum hal_perf_mode venc_mode;

	hdev = inst->core->device;
	if (inst->session_type != MSM_VIDC_ENCODER) {
		dprintk(VIDC_DBG,
			"%s : Not an encoder session. Nothing to do\n",
				__func__);
		return 0;
	}
	mbs_per_frame = msm_dcvs_get_mbs_per_frame(inst);
	if (mbs_per_frame >= inst->core->resources.max_hq_mbs_per_frame ||
		inst->prop.fps >= inst->core->resources.max_hq_fps) {
		enable = true;
	}

	prop_id = HAL_CONFIG_VENC_PERF_MODE;
	venc_mode = enable ? HAL_PERF_MODE_POWER_SAVE :
		HAL_PERF_MODE_POWER_MAX_QUALITY;
	pdata = &venc_mode;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, prop_id, pdata);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: Failed to set power save mode for inst: %pK\n",
			__func__, inst);
		goto fail_power_mode_set;
	}
	inst->flags = enable ?
		inst->flags | VIDC_LOW_POWER :
		inst->flags & ~VIDC_LOW_POWER;

	dprintk(VIDC_PROF,
		"Power Save Mode for inst: %pK Enable = %d\n", inst, enable);
fail_power_mode_set:
	return rc;
}

static int msm_vidc_move_core_to_power_save_mode(struct msm_vidc_core *core,
	u32 core_id)
{
	struct msm_vidc_inst *inst = NULL;

	dprintk(VIDC_PROF, "Core %d : Moving all inst to LP mode\n", core_id);
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->clk_data.core_id == core_id &&
			inst->session_type == MSM_VIDC_ENCODER)
			msm_vidc_power_save_mode_enable(inst, true);
	}
	mutex_unlock(&core->lock);

	return 0;
}

static u32 get_core_load(struct msm_vidc_core *core,
	u32 core_id, bool lp_mode)
{
	struct msm_vidc_inst *inst = NULL;
	u32 current_inst_mbs_per_sec = 0, load = 0;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		u32 cycles, lp_cycles;

		if (!(inst->clk_data.core_id & core_id))
			continue;
		if (inst->session_type == MSM_VIDC_DECODER) {
			cycles = lp_cycles = inst->clk_data.entry->vpp_cycles;
		} else if (inst->session_type == MSM_VIDC_ENCODER) {
			lp_mode |= inst->flags & VIDC_LOW_POWER;
			cycles = lp_mode ?
				inst->clk_data.entry->low_power_cycles :
				inst->clk_data.entry->vpp_cycles;
		} else {
			continue;
		}
		if (inst->clk_data.core_id == 3)
			cycles = cycles / 2;

		current_inst_mbs_per_sec = msm_comm_get_inst_load(inst,
				LOAD_CALC_NO_QUIRKS);
		load += current_inst_mbs_per_sec * cycles;
	}
	mutex_unlock(&core->lock);

	return load;
}

int msm_vidc_decide_core_and_power_mode(struct msm_vidc_inst *inst)
{
	int rc = 0, hier_mode = 0;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	unsigned long max_freq, lp_cycles = 0;
	struct hal_videocores_usage_info core_info;
	u32 core0_load = 0, core1_load = 0, core0_lp_load = 0,
		core1_lp_load = 0;
	u32 current_inst_load = 0, current_inst_lp_load = 0,
		min_load = 0, min_lp_load = 0;
	u32 min_core_id, min_lp_core_id;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;
	max_freq = msm_vidc_max_freq(inst->core);
	inst->clk_data.core_id = 0;

	core0_load = get_core_load(core, VIDC_CORE_ID_1, false);
	core1_load = get_core_load(core, VIDC_CORE_ID_2, false);
	core0_lp_load = get_core_load(core, VIDC_CORE_ID_1, true);
	core1_lp_load = get_core_load(core, VIDC_CORE_ID_2, true);

	min_load = min(core0_load, core1_load);
	min_core_id = core0_load < core1_load ?
		VIDC_CORE_ID_1 : VIDC_CORE_ID_2;
	min_lp_load = min(core0_lp_load, core1_lp_load);
	min_lp_core_id = core0_lp_load < core1_lp_load ?
		VIDC_CORE_ID_1 : VIDC_CORE_ID_2;

	lp_cycles = inst->session_type == MSM_VIDC_ENCODER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

	current_inst_load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS) *
		inst->clk_data.entry->vpp_cycles;

	current_inst_lp_load = msm_comm_get_inst_load(inst,
		LOAD_CALC_NO_QUIRKS) * lp_cycles;

	dprintk(VIDC_DBG, "Core 0 Load = %d Core 1 Load = %d\n",
		 core0_load, core1_load);
	dprintk(VIDC_DBG, "Core 0 LP Load = %d Core 1 LP Load = %d\n",
		core0_lp_load, core1_lp_load);
	dprintk(VIDC_DBG, "Max Load = %lu\n", max_freq);
	dprintk(VIDC_DBG, "Current Load = %d Current LP Load = %d\n",
		current_inst_load, current_inst_lp_load);

	/* Hier mode can be normal HP or Hybrid HP. */

	hier_mode = msm_comm_g_ctrl_for_id(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HIER_P_NUM_LAYERS);
	hier_mode |= msm_comm_g_ctrl_for_id(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE);

	/* Try for preferred core based on settings. */
	if (inst->session_type == MSM_VIDC_ENCODER && hier_mode) {
		if (current_inst_load / 2 + core0_load <= max_freq &&
			current_inst_load / 2 + core1_load <= max_freq) {
			inst->clk_data.core_id = VIDC_CORE_ID_3;
			msm_vidc_power_save_mode_enable(inst, false);
			goto decision_done;
		}
	}

	if (inst->session_type == MSM_VIDC_ENCODER && hier_mode) {
		if (current_inst_lp_load / 2 +
				core0_lp_load <= max_freq &&
			current_inst_lp_load / 2 +
				core1_lp_load <= max_freq) {
			inst->clk_data.core_id = VIDC_CORE_ID_3;
			msm_vidc_power_save_mode_enable(inst, true);
			goto decision_done;
		}
	}

	if (current_inst_load + min_load < max_freq) {
		inst->clk_data.core_id = min_core_id;
		dprintk(VIDC_DBG,
			"Selected normally : Core ID = %d\n",
				inst->clk_data.core_id);
		msm_vidc_power_save_mode_enable(inst, false);
	} else if (current_inst_lp_load + min_load < max_freq) {
		/* Move current instance to LP and return */
		inst->clk_data.core_id = min_core_id;
		dprintk(VIDC_DBG,
			"Selected by moving current to LP : Core ID = %d\n",
				inst->clk_data.core_id);
		msm_vidc_power_save_mode_enable(inst, true);

	} else if (current_inst_lp_load + min_lp_load < max_freq) {
		/* Move all instances to LP mode and return */
		inst->clk_data.core_id = min_lp_core_id;
		dprintk(VIDC_DBG,
			"Moved all inst's to LP: Core ID = %d\n",
				inst->clk_data.core_id);
		msm_vidc_move_core_to_power_save_mode(core, min_lp_core_id);
	} else {
		rc = -EINVAL;
		dprintk(VIDC_ERR,
			"Sorry ... Core Can't support this load\n");
		return rc;
	}

decision_done:
	core_info.video_core_enable_mask = inst->clk_data.core_id;
	dprintk(VIDC_DBG,
		"Core Enable Mask %d\n", core_info.video_core_enable_mask);

	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session,
			HAL_PARAM_VIDEO_CORES_USAGE, &core_info);
	if (rc)
		dprintk(VIDC_WARN,
				" Failed to configure CORE ID %pK\n", inst);

	rc = msm_comm_scale_clocks_and_bus(inst);

	return rc;
}


