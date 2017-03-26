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

#define IS_VALID_DCVS_SESSION(__cur_mbpf, __min_mbpf) \
		((__cur_mbpf) >= (__min_mbpf))

static bool msm_dcvs_check_supported(struct msm_vidc_inst *inst);
static int msm_dcvs_enc_scale_clocks(struct msm_vidc_inst *inst);
static int msm_dcvs_dec_scale_clocks(struct msm_vidc_inst *inst, bool fbd);

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
}


static unsigned long msm_vidc_get_highest_freq(struct msm_vidc_inst *inst)
{
	struct vidc_freq_data *temp;
	unsigned long freq = 0;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry(temp, &inst->freqs.list, list) {
		freq = max(freq, temp->freq);
	}
	mutex_unlock(&inst->freqs.lock);

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

	if (inst->count.fbd < DCVS_FTB_WINDOW) {
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

	freq = msm_vidc_get_highest_freq(inst);

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
	inst->dcvs_mode = msm_dcvs_check_supported(inst);
	return 0;
}

static inline int msm_dcvs_count_active_instances(struct msm_vidc_core *core,
	enum session_type session_type)
{
	int active_instances = 0;
	struct msm_vidc_inst *temp = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	/* DCVS condition is as following
	 * Decoder DCVS : Only for ONE decoder session.
	 * Encoder DCVS : Only for ONE encoder session + ONE decoder session
	 */
	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->state >= MSM_VIDC_OPEN_DONE &&
			temp->state < MSM_VIDC_STOP_DONE &&
			(temp->session_type == session_type ||
			 temp->session_type == MSM_VIDC_ENCODER))
			active_instances++;
	}
	mutex_unlock(&core->lock);
	return active_instances;
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

static void msm_dcvs_update_dcvs_params(int idx, struct msm_vidc_inst *inst)
{
	struct dcvs_stats *dcvs = NULL;
	struct msm_vidc_platform_resources *res = NULL;
	struct dcvs_table *table = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}

	dcvs = &inst->dcvs;
	res = &inst->core->resources;
	table = res->dcvs_tbl;

	dcvs->load_low = table[idx].load_low;
	dcvs->load_high = table[idx].load_high;
	dcvs->supported_codecs = table[idx].supported_codecs;
}

static void msm_dcvs_enc_check_and_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (inst->session_type == MSM_VIDC_ENCODER &&
		msm_vidc_enc_dcvs_mode) {
		rc = msm_dcvs_enc_scale_clocks(inst);
		if (rc) {
			dprintk(VIDC_DBG,
				"ENC_DCVS: error while scaling clocks\n");
		}
	}
}

static void msm_dcvs_dec_check_and_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (inst->session_type == MSM_VIDC_DECODER &&
		msm_vidc_dec_dcvs_mode) {
		msm_dcvs_monitor_buffer(inst);
		rc = msm_dcvs_dec_scale_clocks(inst, false);
		if (rc) {
			dprintk(VIDC_ERR,
					"%s: Failed to scale clocks in DCVS: %d\n",
					__func__, rc);
		}
	}
}

void msm_dcvs_check_and_scale_clocks(struct msm_vidc_inst *inst, bool is_etb)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}
	msm_dcvs_try_enable(inst);
	if (!inst->dcvs_mode) {
		dprintk(VIDC_DBG, "DCVS is not enabled\n");
		return;
	}

	if (is_etb)
		msm_dcvs_enc_check_and_scale_clocks(inst);
	else
		msm_dcvs_dec_check_and_scale_clocks(inst);
}

static inline int get_pending_bufs_fw(struct msm_vidc_inst *inst)
{
	int fw_out_qsize = 0, buffers_in_driver = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return -EINVAL;
	}

	if (inst->state >= MSM_VIDC_OPEN_DONE &&
		inst->state < MSM_VIDC_STOP_DONE) {
		fw_out_qsize = inst->count.ftb - inst->count.fbd;
		buffers_in_driver = inst->buffers_held_in_driver;
	}

	return fw_out_qsize + buffers_in_driver;
}

static inline void msm_dcvs_print_dcvs_stats(struct dcvs_stats *dcvs)
{
	dprintk(VIDC_DBG,
		"DCVS: Load_Low %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_high);

	dprintk(VIDC_DBG,
		"DCVS: ThrDispBufLow %d, ThrDispBufHigh %d\n",
		dcvs->threshold_disp_buf_low,
		dcvs->threshold_disp_buf_high);

	dprintk(VIDC_DBG,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold, dcvs->max_threshold);
}

void msm_dcvs_init_load(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct hal_buffer_requirements *output_buf_req;
	struct dcvs_stats *dcvs;
	struct dcvs_table *table;
	struct msm_vidc_platform_resources *res = NULL;
	int i, num_rows, fourcc;

	dprintk(VIDC_DBG, "Init DCVS Load\n");

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}

	core = inst->core;
	dcvs = &inst->dcvs;
	res = &core->resources;
	dcvs->load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);

	num_rows = res->dcvs_tbl_size;
	table = res->dcvs_tbl;

	if (!num_rows || !table) {
		dprintk(VIDC_ERR,
				"%s: Dcvs table entry not found.\n", __func__);
		return;
	}

	fourcc = inst->session_type == MSM_VIDC_DECODER ?
				inst->fmts[OUTPUT_PORT].fourcc :
				inst->fmts[CAPTURE_PORT].fourcc;

	for (i = 0; i < num_rows; i++) {
		bool matches = msm_dcvs_check_codec_supported(
					fourcc,
					table[i].supported_codecs,
					inst->session_type);
		if (!matches)
			continue;

		if (dcvs->load > table[i].load) {
			msm_dcvs_update_dcvs_params(i, inst);
			break;
		}
	}

	if (inst->session_type == MSM_VIDC_ENCODER)
		goto print_stats;

	output_buf_req = get_buff_req_buffer(inst,
		msm_comm_get_hal_output_buffer(inst));

	if (!output_buf_req) {
		dprintk(VIDC_ERR,
			"%s: No buffer requirement for buffer type %x\n",
			__func__, HAL_BUFFER_OUTPUT);
		return;
	}

	dcvs->transition_turbo = false;

	/* calculating the min and max threshold */
	if (output_buf_req->buffer_count_actual) {
		dcvs->min_threshold = output_buf_req->buffer_count_actual -
			output_buf_req->buffer_count_min -
			msm_dcvs_get_extra_buff_count(inst) + 1;
		dcvs->max_threshold = output_buf_req->buffer_count_actual;
		if (dcvs->max_threshold <= dcvs->min_threshold)
			dcvs->max_threshold =
				dcvs->min_threshold + DCVS_BUFFER_SAFEGUARD;
		dcvs->threshold_disp_buf_low = dcvs->min_threshold;
		dcvs->threshold_disp_buf_high = dcvs->max_threshold;
	}

print_stats:
	msm_dcvs_print_dcvs_stats(dcvs);
}

void msm_dcvs_init(struct msm_vidc_inst *inst)
{
	dprintk(VIDC_DBG, "Init DCVS Struct\n");

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}

	inst->dcvs = (struct dcvs_stats){ {0} };
	inst->dcvs.threshold_disp_buf_high = DCVS_NOMINAL_THRESHOLD;
	inst->dcvs.threshold_disp_buf_low = DCVS_TURBO_THRESHOLD;
}

void msm_dcvs_monitor_buffer(struct msm_vidc_inst *inst)
{
	int new_ftb, i, prev_buf_count;
	int fw_pending_bufs, total_output_buf, buffers_outside_fw;
	struct dcvs_stats *dcvs;
	struct hal_buffer_requirements *output_buf_req;

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, inst);
		return;
	}
	dcvs = &inst->dcvs;

	mutex_lock(&inst->lock);
	output_buf_req = get_buff_req_buffer(inst,
				msm_comm_get_hal_output_buffer(inst));
	if (!output_buf_req) {
		dprintk(VIDC_ERR, "%s : Get output buffer req failed %pK\n",
			__func__, inst);
		mutex_unlock(&inst->lock);
		return;
	}

	total_output_buf = output_buf_req->buffer_count_actual;
	fw_pending_bufs = get_pending_bufs_fw(inst);
	mutex_unlock(&inst->lock);

	buffers_outside_fw = total_output_buf - fw_pending_bufs;
	dcvs->num_ftb[dcvs->ftb_index] = buffers_outside_fw;
	dcvs->ftb_index = (dcvs->ftb_index + 1) % DCVS_FTB_WINDOW;

	if (dcvs->ftb_counter < DCVS_FTB_WINDOW)
		dcvs->ftb_counter++;

	dprintk(VIDC_PROF,
		"DCVS: ftb_counter %d\n", dcvs->ftb_counter);

	if (dcvs->ftb_counter == DCVS_FTB_WINDOW) {
		new_ftb = 0;
		for (i = 0; i < dcvs->ftb_counter; i++) {
			if (dcvs->num_ftb[i] > new_ftb)
				new_ftb = dcvs->num_ftb[i];
		}

		dcvs->threshold_disp_buf_high = new_ftb;
		if (dcvs->threshold_disp_buf_high <=
			dcvs->threshold_disp_buf_low +
			DCVS_BUFFER_SAFEGUARD) {
			dcvs->threshold_disp_buf_high =
				dcvs->threshold_disp_buf_low +
				DCVS_BUFFER_SAFEGUARD
				+ (DCVS_BUFFER_SAFEGUARD == 0);
		}

		dcvs->threshold_disp_buf_high =
			clamp(dcvs->threshold_disp_buf_high,
				dcvs->min_threshold,
				dcvs->max_threshold);
	}

	if (dcvs->ftb_counter == DCVS_FTB_WINDOW &&
			dcvs->load == dcvs->load_low) {
		prev_buf_count =
			dcvs->num_ftb[((dcvs->ftb_index - 2 +
				DCVS_FTB_WINDOW) % DCVS_FTB_WINDOW)];
		if (prev_buf_count == dcvs->threshold_disp_buf_low &&
			buffers_outside_fw <= dcvs->threshold_disp_buf_low) {
			dcvs->transition_turbo = true;
		} else if (buffers_outside_fw > dcvs->threshold_disp_buf_low &&
			(buffers_outside_fw -
			 (prev_buf_count - buffers_outside_fw))
			< dcvs->threshold_disp_buf_low){
			dcvs->transition_turbo = true;
		}
	}

	dprintk(VIDC_PROF,
		"DCVS: total_output_buf %d buffers_outside_fw %d load %d transition_turbo %d\n",
		total_output_buf, buffers_outside_fw, dcvs->load_low,
		dcvs->transition_turbo);
}

static int msm_dcvs_enc_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0, fw_pending_bufs = 0, total_input_buf = 0;
	struct msm_vidc_core *core;
	struct dcvs_stats *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	dcvs = &inst->dcvs;

	mutex_lock(&inst->lock);
	total_input_buf = inst->buff_req.buffer[0].buffer_count_actual;
	fw_pending_bufs = (inst->count.etb - inst->count.ebd);
	mutex_unlock(&inst->lock);

	dprintk(VIDC_PROF,
		"DCVS: total_input_buf %d, fw_pending_bufs %d\n",
		total_input_buf, fw_pending_bufs);

	if (dcvs->etb_counter < total_input_buf) {
		dcvs->etb_counter++;
		if (dcvs->etb_counter != total_input_buf)
			return rc;
	}

	dprintk(VIDC_PROF,
		"DCVS: total_input_buf %d, fw_pending_bufs %d etb_counter %d  dcvs->load %d\n",
		total_input_buf, fw_pending_bufs,
		dcvs->etb_counter, dcvs->load);

	if (fw_pending_bufs <= DCVS_ENC_LOW_THR &&
		dcvs->load > dcvs->load_low) {
		dcvs->load = dcvs->load_low;
		dcvs->prev_freq_lowered = true;
	} else {
		dcvs->prev_freq_lowered = false;
	}

	if (fw_pending_bufs >= DCVS_ENC_HIGH_THR &&
		dcvs->load <= dcvs->load_low) {
		dcvs->load = dcvs->load_high;
		dcvs->prev_freq_increased = true;
	} else {
		dcvs->prev_freq_increased = false;
	}

	if (dcvs->prev_freq_lowered || dcvs->prev_freq_increased) {
		dprintk(VIDC_PROF,
			"DCVS: (Scaling Clock %s)  etb clock set = %d total_input_buf = %d fw_pending_bufs %d\n",
			dcvs->prev_freq_lowered ? "Lower" : "Higher",
			dcvs->load, total_input_buf, fw_pending_bufs);

		if (rc) {
			dprintk(VIDC_PROF,
				"Failed to set clock rate in FBD: %d\n", rc);
		}
	} else {
		dprintk(VIDC_PROF,
			"DCVS: etb clock load_old = %d total_input_buf = %d fw_pending_bufs %d\n",
			dcvs->load, total_input_buf, fw_pending_bufs);
	}

	return rc;
}


/*
 * In DCVS scale_clocks will be done both in qbuf and FBD
 * 1 indicates call made from fbd that lowers clock
 * 0 indicates call made from qbuf that increases clock
 * based on DCVS algorithm
 */

static int msm_dcvs_dec_scale_clocks(struct msm_vidc_inst *inst, bool fbd)
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
	core = inst->core;
	dcvs = &inst->dcvs;
	mutex_lock(&inst->lock);
	fw_pending_bufs = get_pending_bufs_fw(inst);

	output_buf_req = get_buff_req_buffer(inst,
		msm_comm_get_hal_output_buffer(inst));
	mutex_unlock(&inst->lock);
	if (!output_buf_req) {
		dprintk(VIDC_ERR,
			"%s: No buffer requirement for buffer type %x\n",
			__func__, HAL_BUFFER_OUTPUT);
		return -EINVAL;
	}

	/* Total number of output buffers */
	total_output_buf = output_buf_req->buffer_count_actual;

	/* Buffers outside FW are with display */
	buffers_outside_fw = total_output_buf - fw_pending_bufs;

	if (buffers_outside_fw >= dcvs->threshold_disp_buf_high &&
		!dcvs->prev_freq_increased &&
		dcvs->load > dcvs->load_low) {
		dcvs->load = dcvs->load_low;
		dcvs->prev_freq_lowered = true;
		dcvs->prev_freq_increased = false;
	} else if (dcvs->transition_turbo && dcvs->load == dcvs->load_low) {
		dcvs->load = dcvs->load_high;
		dcvs->prev_freq_increased = true;
		dcvs->prev_freq_lowered = false;
		dcvs->transition_turbo = false;
	} else {
		dcvs->prev_freq_increased = false;
		dcvs->prev_freq_lowered = false;
	}

	if (dcvs->prev_freq_lowered || dcvs->prev_freq_increased) {
		dprintk(VIDC_PROF,
			"DCVS: clock set = %d tot_output_buf = %d buffers_outside_fw %d threshold_high %d transition_turbo %d\n",
			dcvs->load, total_output_buf, buffers_outside_fw,
			dcvs->threshold_disp_buf_high, dcvs->transition_turbo);

		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set clock rate in FBD: %d\n", rc);
		}
	} else {
		dprintk(VIDC_PROF,
			"DCVS: clock old = %d tot_output_buf = %d buffers_outside_fw %d threshold_high %d transition_turbo %d\n",
			dcvs->load, total_output_buf, buffers_outside_fw,
			dcvs->threshold_disp_buf_high, dcvs->transition_turbo);
	}
	return rc;
}

static bool msm_dcvs_check_supported(struct msm_vidc_inst *inst)
{
	int num_mbs_per_frame = 0, instance_count = 0;
	long int instance_load = 0;
	long int dcvs_limit = 0;
	struct msm_vidc_inst *temp = NULL;
	struct msm_vidc_core *core;
	struct hal_buffer_requirements *output_buf_req;
	struct dcvs_stats *dcvs;
	bool is_codec_supported = false;
	bool is_dcvs_supported = true;
	struct msm_vidc_platform_resources *res = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_WARN, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	dcvs = &inst->dcvs;
	res = &core->resources;

	if (!res->dcvs_limit) {
		dprintk(VIDC_WARN,
				"%s: dcvs limit table not found\n", __func__);
		return false;
	}
	instance_count = msm_dcvs_count_active_instances(core,
		inst->session_type);
	num_mbs_per_frame = msm_dcvs_get_mbs_per_frame(inst);
	instance_load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);
	dcvs_limit =
		(long int)res->dcvs_limit[inst->session_type].min_mbpf *
		res->dcvs_limit[inst->session_type].fps;
	inst->dcvs.extra_buffer_count = 0;

	if (!IS_VALID_DCVS_SESSION(num_mbs_per_frame,
				res->dcvs_limit[inst->session_type].min_mbpf)) {
		inst->dcvs.extra_buffer_count = 0;
		is_dcvs_supported = false;
		goto dcvs_decision_done;

	}

	if (inst->session_type == MSM_VIDC_DECODER) {
		inst->dcvs.extra_buffer_count = DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
		output_buf_req = get_buff_req_buffer(inst,
				msm_comm_get_hal_output_buffer(inst));
		if (!output_buf_req) {
			dprintk(VIDC_ERR,
					"%s: No buffer requirement for buffer type %x\n",
					__func__, HAL_BUFFER_OUTPUT);
			return false;
		}
		is_codec_supported =
			msm_dcvs_check_codec_supported(
				inst->fmts[OUTPUT_PORT].fourcc,
				inst->dcvs.supported_codecs,
				inst->session_type);
		if (!is_codec_supported ||
				!msm_vidc_dec_dcvs_mode) {
			inst->dcvs.extra_buffer_count = 0;
			is_dcvs_supported = false;
			goto dcvs_decision_done;
		}
		if (msm_comm_turbo_session(inst) ||
			!IS_VALID_DCVS_SESSION(instance_load, dcvs_limit) ||
			instance_count > 1)
			is_dcvs_supported = false;
	}
	if (inst->session_type == MSM_VIDC_ENCODER) {
		inst->dcvs.extra_buffer_count = DCVS_ENC_EXTRA_OUTPUT_BUFFERS;
		is_codec_supported =
			msm_dcvs_check_codec_supported(
				inst->fmts[CAPTURE_PORT].fourcc,
				inst->dcvs.supported_codecs,
				inst->session_type);
		if (!is_codec_supported ||
				!msm_vidc_enc_dcvs_mode) {
			inst->dcvs.extra_buffer_count = 0;
			is_dcvs_supported = false;
			goto dcvs_decision_done;
		}
		if (msm_comm_turbo_session(inst) ||
			!IS_VALID_DCVS_SESSION(instance_load, dcvs_limit) ||
				instance_count > 1)
			is_dcvs_supported = false;
	}
dcvs_decision_done:
	if (!is_dcvs_supported) {
		msm_comm_scale_clocks(inst);
		if (instance_count > 1) {
			mutex_lock(&core->lock);
			list_for_each_entry(temp, &core->instances, list)
				temp->dcvs_mode = false;
			mutex_unlock(&core->lock);
		}
	}
	return is_dcvs_supported;
}

int msm_dcvs_get_extra_buff_count(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}

	return inst->dcvs.extra_buffer_count;
}


