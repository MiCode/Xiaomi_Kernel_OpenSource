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

	if (!core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle: %pK\n",
				__func__, hdev);
		return -EINVAL;
	}

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

		if (inst->operating_rate)
			vote_data[i].fps = (inst->operating_rate >> 16) ?
				inst->operating_rate >> 16 : 1;
		else
			vote_data[i].fps = inst->prop.fps;

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
	struct dcvs_stats *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}
	if (!inst->dcvs_mode) {
		dprintk(VIDC_DBG, "DCVS is not enabled\n");
		return 0;
	}

	dcvs = &inst->dcvs;

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

	inst->dcvs.buffer_counter++;
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

	if (freq < inst->dcvs.load_high) {
		dprintk(VIDC_DBG, "Calling DCVS now\n");
		// TODO calling DCVS here may reduce the residency. Re-visit.
		msm_dcvs_scale_clocks(inst);
		freq = inst->dcvs.load;
	}

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
	u32 mbs_per_frame;

	mbs_per_frame = msm_dcvs_get_mbs_per_frame(inst);

	/*
	 * Calculate vpp, vsp cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->entry->low_power_cycles :
			inst->entry->vpp_cycles;

		vsp_cycles = mbs_per_frame * inst->entry->vsp_cycles;

		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->bitrate * 10) / 7;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		vpp_cycles = mbs_per_frame * inst->entry->vpp_cycles;

		vsp_cycles = mbs_per_frame * inst->entry->vsp_cycles;
		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->prop.fps * filled_len * 8 * 10) / 7;

	} else {
		// TODO return Min or Max ?
		dprintk(VIDC_ERR, "Unknown session type = %s\n", __func__);
		return freq;
	}

	freq = max(vpp_cycles, vsp_cycles);

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
	if (!hdev || !allowed_clks_tbl) {
		dprintk(VIDC_ERR,
			"%s Invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		freq += temp->freq;
	}
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq)
			break;
	}
	mutex_unlock(&core->lock);

	core->freq = rate;
	dprintk(VIDC_PROF, "Voting for freq = %lu", freq);
	rc = call_hfi_op(hdev, scale_clocks,
			hdev->hfi_device_data, rate);

	return rc;
}

static unsigned long msm_vidc_max_freq(struct msm_vidc_inst *inst)
{
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	unsigned long freq = 0;

	allowed_clks_tbl = inst->core->resources.allowed_clks_tbl;
	freq = allowed_clks_tbl[0].clock_rate;
	dprintk(VIDC_PROF, "Max rate = %lu", freq);

	return freq;
}

int msm_comm_scale_clocks(struct msm_vidc_inst *inst)
{
	struct vb2_buf_entry *temp, *next;
	unsigned long freq = 0;
	u32 filled_len = 0;
	ion_phys_addr_t device_addr = 0;

	if (inst->dcvs.buffer_counter < DCVS_FTB_WINDOW) {
		freq = msm_vidc_max_freq(inst);
		goto decision_done;
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
		freq = inst->freq;
		goto decision_done;
	}

	freq = msm_vidc_calc_freq(inst, filled_len);

	msm_vidc_update_freq_entry(inst, freq, device_addr);

	freq = msm_vidc_adjust_freq(inst);

decision_done:
	inst->freq = freq;
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
	if (inst->flags & VIDC_THUMBNAIL) {
		dprintk(VIDC_PROF, "Thumbnail sessions don't need DCVS : %pK\n",
			inst);
		return false;
	}
	inst->dcvs_mode = true;

	// TODO : Update with proper number based on on-target tuning.
	inst->dcvs.extra_buffer_count = DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
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
			inst->entry = entry;
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

static inline void msm_dcvs_print_dcvs_stats(struct dcvs_stats *dcvs)
{
	dprintk(VIDC_DBG,
		"DCVS: Load_Low %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_high);

	dprintk(VIDC_DBG,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold, dcvs->max_threshold);
}

void msm_dcvs_init(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 total_freq = 0, rate = 0, load;
	int cycles;
	struct dcvs_stats *dcvs;

	dprintk(VIDC_DBG, "Init DCVS Load\n");

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}

	core = inst->core;
	dcvs = &inst->dcvs;
	inst->dcvs = (struct dcvs_stats){0};
	load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);
	cycles = inst->entry->vpp_cycles;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		cycles = inst->flags & VIDC_LOW_POWER ?
			inst->entry->low_power_cycles :
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

	msm_dcvs_print_dcvs_stats(dcvs);
}

int msm_dcvs_get_extra_buff_count(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}

	return inst->dcvs.extra_buffer_count;
}


