/* Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
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
#include "msm_vidc_dcvs.h"

#define IS_VALID_DCVS_SESSION(__cur_mbpf, __min_mbpf) \
		((__cur_mbpf) >= (__min_mbpf))

static bool msm_dcvs_check_supported(struct msm_vidc_inst *inst);
static bool msm_dcvs_enc_check(struct msm_vidc_inst *inst);
static int msm_dcvs_enc_scale_clocks(struct msm_vidc_inst *inst);
static int msm_dcvs_dec_scale_clocks(struct msm_vidc_inst *inst, bool fbd);

static inline int msm_dcvs_get_mbs_per_frame(struct msm_vidc_inst *inst)
{
	int height, width;

	if (!inst->in_reconfig) {
		height = inst->prop.height[CAPTURE_PORT];
		width = inst->prop.width[CAPTURE_PORT];
	} else {
		height = inst->reconfig_height;
		width = inst->reconfig_width;
	}

	return NUM_MBS_PER_FRAME(height, width);
}

static inline int msm_dcvs_count_active_instances(struct msm_vidc_core *core)
{
	int active_instances = 0;
	struct msm_vidc_inst *inst = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid args: %p\n", __func__, core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_STOP_DONE)
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

static void msm_dcvs_update_dcvs_params(int idx, struct msm_vidc_inst *inst)
{
	struct dcvs_stats *dcvs = NULL;
	struct msm_vidc_platform_resources *res = NULL;
	struct dcvs_table *table = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
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

	if (inst->session_type == MSM_VIDC_ENCODER && msm_vidc_enc_dcvs_mode) {
		inst->dcvs_mode = msm_dcvs_check_supported(inst);
		dprintk(VIDC_DBG, "%s: session DCVS %s supported\n",
				__func__, inst->dcvs_mode ? "" : "not");

		if (inst->dcvs_mode) {
			rc = msm_dcvs_enc_scale_clocks(inst);
			if (rc) {
				dprintk(VIDC_DBG,
				"ENC_DCVS: error while scaling clocks\n");
			}
		}
	}
}

static void msm_dcvs_dec_check_and_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (inst->session_type != MSM_VIDC_DECODER || !msm_vidc_dec_dcvs_mode)
		return;

	if (msm_dcvs_check_supported(inst)) {
		inst->dcvs_mode = true;
		dprintk(VIDC_DBG,
			"%s: session DCVS supported, decode_dcvs_mode = %d\n",
			__func__, inst->dcvs_mode);
	} else {
		inst->dcvs_mode = false;
		dprintk(VIDC_DBG,
			"%s: session DCVS not supported, decode_dcvs_mode = %d\n",
			__func__, inst->dcvs_mode);
	}

	if (msm_vidc_dec_dcvs_mode && inst->dcvs_mode) {
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
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
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
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
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
				inst->fmts[OUTPUT_PORT]->fourcc :
				inst->fmts[CAPTURE_PORT]->fourcc;

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
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
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
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
		return;
	}
	dcvs = &inst->dcvs;

	mutex_lock(&inst->lock);
	output_buf_req = get_buff_req_buffer(inst,
				msm_comm_get_hal_output_buffer(inst));
	if (!output_buf_req) {
		dprintk(VIDC_ERR, "%s : Get output buffer req failed %p\n",
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

		rc = msm_comm_scale_clocks_load(core, dcvs->load,
				LOAD_CALC_NO_QUIRKS);
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

		rc = msm_comm_scale_clocks_load(core, dcvs->load,
				LOAD_CALC_NO_QUIRKS);
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

static bool msm_dcvs_enc_check(struct msm_vidc_inst *inst)
{
	int num_mbs_per_frame = 0;
	long int instance_load = 0;
	long int dcvs_limit = 0;
	bool dcvs_check_passed = false, is_codec_supported  = false;
	struct msm_vidc_platform_resources *res = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return dcvs_check_passed;
	}

	res = &inst->core->resources;
	if (!res->dcvs_limit) {
		dprintk(VIDC_ERR,
			"%s Dcvs limit table uninitialized\n", __func__);
		return false;
	}

	is_codec_supported =
		msm_dcvs_check_codec_supported(
				inst->fmts[CAPTURE_PORT]->fourcc,
				inst->dcvs.supported_codecs,
				inst->session_type);

	num_mbs_per_frame = msm_dcvs_get_mbs_per_frame(inst);
	instance_load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);
	dcvs_limit =
		(long int)res->dcvs_limit[inst->session_type].min_mbpf *
		res->dcvs_limit[inst->session_type].fps;

	if (msm_vidc_enc_dcvs_mode && is_codec_supported &&
		inst->dcvs.is_power_save_mode &&
		IS_VALID_DCVS_SESSION(num_mbs_per_frame,
			res->dcvs_limit[inst->session_type].min_mbpf) &&
		IS_VALID_DCVS_SESSION(instance_load, dcvs_limit)) {
		dcvs_check_passed = true;
	}
	return dcvs_check_passed;
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
	instance_count = msm_dcvs_count_active_instances(core);

	if (instance_count == 1 && inst->session_type == MSM_VIDC_DECODER &&
		!msm_comm_turbo_session(inst)) {
		num_mbs_per_frame = msm_dcvs_get_mbs_per_frame(inst);
		instance_load = msm_comm_get_inst_load(inst,
			LOAD_CALC_NO_QUIRKS);
		output_buf_req = get_buff_req_buffer(inst,
			msm_comm_get_hal_output_buffer(inst));
		dcvs_limit =
			(long int)res->dcvs_limit[inst->session_type].min_mbpf *
			res->dcvs_limit[inst->session_type].fps;
		is_codec_supported =
			msm_dcvs_check_codec_supported(
					inst->fmts[OUTPUT_PORT]->fourcc,
					inst->dcvs.supported_codecs,
					inst->session_type);
		if (!is_codec_supported ||
			!IS_VALID_DCVS_SESSION(num_mbs_per_frame,
				res->dcvs_limit[inst->session_type].min_mbpf) ||
			!IS_VALID_DCVS_SESSION(instance_load, dcvs_limit) ||
			inst->seqchanged_count > 1)
			return false;

		if (!output_buf_req) {
			dprintk(VIDC_ERR,
				"%s: No buffer requirement for buffer type %x\n",
				__func__, HAL_BUFFER_OUTPUT);
			return false;
		}
	} else if (instance_count == 1 &&
			inst->session_type == MSM_VIDC_ENCODER &&
			!msm_comm_turbo_session(inst)) {
		if (!msm_dcvs_enc_check(inst))
			return false;
	} else {
		/*
		* For multiple instance use case with 4K, clocks will be scaled
		* as per load in streamon, but the clocks may be scaled
		* down as DCVS is running for first playback instance
		* Rescaling the core clock for multiple instance use case
		*/
		if (!dcvs->is_clock_scaled) {
			if (!msm_comm_scale_clocks(core)) {
				dcvs->is_clock_scaled = true;
				dprintk(VIDC_DBG,
					"%s: Scaled clocks = %d\n",
					__func__, dcvs->is_clock_scaled);
			} else {
				dprintk(VIDC_DBG,
					"%s: Failed to Scale clocks. Perf might be impacted\n",
					__func__);
			}
		}
		/*
		* For multiple instance use case turn OFF DCVS algorithm
		* immediately
		*/
		if (instance_count > 1) {
			mutex_lock(&core->lock);
			list_for_each_entry(temp, &core->instances, list)
				temp->dcvs_mode = false;
			mutex_unlock(&core->lock);
		}

		return false;
	}

	return true;
}

int msm_dcvs_get_extra_buff_count(struct msm_vidc_inst *inst)
{
	int extra_buffer = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}

	if (inst->session_type == MSM_VIDC_ENCODER) {
		if (msm_dcvs_enc_check(inst))
			extra_buffer = DCVS_ENC_EXTRA_OUTPUT_BUFFERS;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		if (msm_dcvs_check_supported(inst))
			extra_buffer = DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
	}
	return extra_buffer;
}


void msm_dcvs_enc_set_power_save_mode(struct msm_vidc_inst *inst,
					bool is_power_save_mode)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return;
	}

	inst->dcvs.is_power_save_mode = is_power_save_mode;
}
