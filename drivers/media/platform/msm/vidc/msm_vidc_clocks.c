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
 *
 */

#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

#define MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR (4 << 16)

#define MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO (5 << 16)

static unsigned long msm_vidc_calc_freq_ar50(struct msm_vidc_inst *inst,
	u32 filled_len);
static int msm_vidc_decide_work_mode_ar50(struct msm_vidc_inst *inst);
static unsigned long msm_vidc_calc_freq(struct msm_vidc_inst *inst,
	u32 filled_len);

struct msm_vidc_core_ops core_ops_vpu4 = {
	.calc_freq = msm_vidc_calc_freq_ar50,
	.decide_work_route = NULL,
	.decide_work_mode = msm_vidc_decide_work_mode_ar50,
};

struct msm_vidc_core_ops core_ops_vpu5 = {
	.calc_freq = msm_vidc_calc_freq,
	.decide_work_route = msm_vidc_decide_work_route,
	.decide_work_mode = msm_vidc_decide_work_mode,
};

static inline void msm_dcvs_print_dcvs_stats(struct clock_data *dcvs)
{
	dprintk(VIDC_PROF,
		"DCVS: Load_Low %d, Load Norm %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_norm,
		dcvs->load_high);

	dprintk(VIDC_PROF,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold, dcvs->max_threshold);
}

static inline unsigned long int get_ubwc_compression_ratio(
	struct ubwc_cr_stats_info_type ubwc_stats_info)
{
	unsigned long int sum = 0, weighted_sum = 0;
	unsigned long int compression_ratio = 1 << 16;

	weighted_sum =
		32  * ubwc_stats_info.cr_stats_info0 +
		64  * ubwc_stats_info.cr_stats_info1 +
		96  * ubwc_stats_info.cr_stats_info2 +
		128 * ubwc_stats_info.cr_stats_info3 +
		160 * ubwc_stats_info.cr_stats_info4 +
		192 * ubwc_stats_info.cr_stats_info5 +
		256 * ubwc_stats_info.cr_stats_info6;

	sum =
		ubwc_stats_info.cr_stats_info0 +
		ubwc_stats_info.cr_stats_info1 +
		ubwc_stats_info.cr_stats_info2 +
		ubwc_stats_info.cr_stats_info3 +
		ubwc_stats_info.cr_stats_info4 +
		ubwc_stats_info.cr_stats_info5 +
		ubwc_stats_info.cr_stats_info6;

	compression_ratio = (weighted_sum && sum) ?
		((256 * sum) << 16) / weighted_sum : compression_ratio;

	return compression_ratio;
}

int msm_vidc_get_mbs_per_frame(struct msm_vidc_inst *inst)
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

static int msm_vidc_get_fps(struct msm_vidc_inst *inst)
{
	int fps;

	if ((inst->clk_data.operating_rate >> 16) > inst->prop.fps)
		fps = (inst->clk_data.operating_rate >> 16) ?
			(inst->clk_data.operating_rate >> 16) : 1;
	else
		fps = inst->prop.fps;

	return fps;
}

void update_recon_stats(struct msm_vidc_inst *inst,
	struct recon_stats_type *recon_stats)
{
	struct recon_buf *binfo;
	u32 CR = 0, CF = 0;
	u32 frame_size;

	CR = get_ubwc_compression_ratio(recon_stats->ubwc_stats_info);

	frame_size = (msm_vidc_get_mbs_per_frame(inst) / (32 * 8) * 3) / 2;

	if (frame_size)
		CF = recon_stats->complexity_number / frame_size;
	else
		CF = MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR;

	mutex_lock(&inst->reconbufs.lock);
	list_for_each_entry(binfo, &inst->reconbufs.list, list) {
		if (binfo->buffer_index ==
				recon_stats->buffer_index) {
			binfo->CR = CR;
			binfo->CF = CF;
		}
	}
	mutex_unlock(&inst->reconbufs.lock);
}

static int fill_dynamic_stats(struct msm_vidc_inst *inst,
	struct vidc_bus_vote_data *vote_data)
{
	struct recon_buf *binfo, *nextb;
	struct vidc_input_cr_data *temp, *next;
	u32 min_cf = 0, max_cf = 0;
	u32 min_input_cr = 0, max_input_cr = 0, min_cr = 0, max_cr = 0;

	mutex_lock(&inst->reconbufs.lock);
	list_for_each_entry_safe(binfo, nextb, &inst->reconbufs.list, list) {
		min_cr = min(min_cr, binfo->CR);
		max_cr = max(max_cr, binfo->CR);
		min_cf = min(min_cf, binfo->CF);
		max_cf = max(max_cf, binfo->CF);
	}
	mutex_unlock(&inst->reconbufs.lock);

	mutex_lock(&inst->input_crs.lock);
	list_for_each_entry_safe(temp, next, &inst->input_crs.list, list) {
		min_input_cr = min(min_input_cr, temp->input_cr);
		max_input_cr = max(max_input_cr, temp->input_cr);
	}
	mutex_unlock(&inst->input_crs.lock);

	/* Sanitize CF values from HW . */
	max_cf = min_t(u32, max_cf, MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR);
	min_cf = max_t(u32, min_cf, MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR);
	max_cr = min_t(u32, max_cr, MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO);
	min_cr = max_t(u32, min_cr, MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO);
	max_input_cr = min_t(u32,
		max_input_cr, MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO);
	min_input_cr = max_t(u32,
		min_input_cr, MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO);

	vote_data->compression_ratio = min_cr;
	vote_data->complexity_factor = max_cf;
	vote_data->input_cr = min_input_cr;
	vote_data->use_dpb_read = false;

	/* Check if driver can vote for lower bus BW */
	if (inst->clk_data.load < inst->clk_data.load_norm) {
		vote_data->compression_ratio = max_cr;
		vote_data->complexity_factor = min_cf;
		vote_data->input_cr = max_input_cr;
		vote_data->use_dpb_read = true;
	}

	dprintk(VIDC_PROF,
		"Input CR = %d Recon CR = %d Complexity Factor = %d\n",
			vote_data->input_cr, vote_data->compression_ratio,
			vote_data->complexity_factor);

	return 0;
}

int msm_comm_vote_bus(struct msm_vidc_core *core)
{
	int rc = 0, vote_data_count = 0, i = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;
	struct vidc_bus_vote_data *vote_data = NULL;
	bool is_turbo = false;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}
	hdev = core->device;

	vote_data = kzalloc(sizeof(struct vidc_bus_vote_data) *
			MAX_SUPPORTED_INSTANCES, GFP_ATOMIC);
	if (!vote_data) {
		dprintk(VIDC_DBG,
			"vote_data allocation with GFP_ATOMIC failed\n");
		vote_data = kzalloc(sizeof(struct vidc_bus_vote_data) *
			MAX_SUPPORTED_INSTANCES, GFP_KERNEL);
		if (!vote_data) {
			dprintk(VIDC_DBG,
				"vote_data allocation failed\n");
			return -EINVAL;
		}
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		int codec = 0;
		struct msm_vidc_buffer *temp, *next;
		u32 filled_len = 0;
		u32 device_addr = 0;

		if (!inst) {
			dprintk(VIDC_ERR, "%s Invalid args\n",
				__func__);
			mutex_unlock(&core->lock);
			return -EINVAL;
		}

		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry_safe(temp, next,
				&inst->registeredbufs.list, list) {
			if (temp->vvb.vb2_buf.type ==
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
					temp->flags & MSM_VIDC_FLAG_DEFERRED) {
				filled_len = max(filled_len,
					temp->vvb.vb2_buf.planes[0].bytesused);
				device_addr = temp->smem[0].device_addr;
			}
			if (inst->session_type == MSM_VIDC_ENCODER &&
				(temp->vvb.flags &
				V4L2_QCOM_BUF_FLAG_PERF_MODE)) {
				is_turbo = true;
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);

		if ((!filled_len || !device_addr) &&
			(inst->session_type != MSM_VIDC_CVP)) {
			dprintk(VIDC_DBG, "%s No ETBs\n", __func__);
			continue;
		}

		++vote_data_count;

		switch (inst->session_type) {
		case MSM_VIDC_DECODER:
			codec = inst->fmts[OUTPUT_PORT].fourcc;
			break;
		case MSM_VIDC_ENCODER:
			codec = inst->fmts[CAPTURE_PORT].fourcc;
			break;
		case MSM_VIDC_CVP:
			codec = V4L2_PIX_FMT_CVP;
			break;
		default:
			dprintk(VIDC_ERR, "%s: invalid session_type %#x\n",
				__func__, inst->session_type);
			break;
		}

		memset(&(vote_data[i]), 0x0, sizeof(struct vidc_bus_vote_data));

		vote_data[i].domain = get_hal_domain(inst->session_type);
		vote_data[i].codec = get_hal_codec(codec);
		vote_data[i].input_width =  max(inst->prop.width[OUTPUT_PORT],
				inst->prop.width[OUTPUT_PORT]);
		vote_data[i].input_height = max(inst->prop.height[OUTPUT_PORT],
				inst->prop.height[OUTPUT_PORT]);
		vote_data[i].output_width =  max(inst->prop.width[CAPTURE_PORT],
				inst->prop.width[OUTPUT_PORT]);
		vote_data[i].output_height =
				max(inst->prop.height[CAPTURE_PORT],
				inst->prop.height[OUTPUT_PORT]);
		vote_data[i].lcu_size = codec == V4L2_PIX_FMT_HEVC ? 32 : 16;
		vote_data[i].b_frames_enabled =
			msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES) != 0;

		vote_data[i].fps = msm_vidc_get_fps(inst);

		vote_data[i].power_mode = 0;
		if (msm_vidc_clock_voting || is_turbo ||
			inst->clk_data.buffer_counter < DCVS_FTB_WINDOW)
			vote_data[i].power_mode = VIDC_POWER_TURBO;

		if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_PRIMARY) {
			vote_data[i].color_formats[0] =
				msm_comm_get_hal_uncompressed(
				inst->clk_data.opb_fourcc);
			vote_data[i].num_formats = 1;
		} else {
			vote_data[i].color_formats[0] =
				msm_comm_get_hal_uncompressed(
				inst->clk_data.dpb_fourcc);
			vote_data[i].color_formats[1] =
				msm_comm_get_hal_uncompressed(
				inst->clk_data.opb_fourcc);
			vote_data[i].num_formats = 2;
		}
		vote_data[i].work_mode = inst->clk_data.work_mode;
		fill_dynamic_stats(inst, &vote_data[i]);

		if (core->resources.sys_cache_res_set)
			vote_data[i].use_sys_cache = true;

		if (inst->session_type == MSM_VIDC_CVP) {
			vote_data[i].domain =
				get_hal_domain(inst->session_type);
			vote_data[i].ddr_bw = inst->clk_data.ddr_bw;
			vote_data[i].sys_cache_bw =
				inst->clk_data.sys_cache_bw;
		}

		i++;
	}
	mutex_unlock(&core->lock);
	if (vote_data_count)
		rc = call_hfi_op(hdev, vote_bus, hdev->hfi_device_data,
			vote_data, vote_data_count);

	kfree(vote_data);
	return rc;
}

static inline int get_bufs_outside_fw(struct msm_vidc_inst *inst)
{
	u32 fw_out_qsize = 0, i = 0;
	struct vb2_queue *q = NULL;
	struct vb2_buffer *vb = NULL;

	/*
	 * DCVS always operates on Uncompressed buffers.
	 * For Decoders, FTB and Encoders, ETB.
	 */

	if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_STOP_DONE) {

		/*
		 * For decoder, there will be some frames with client
		 * but not to be displayed. Ex : VP9 DECODE_ONLY frames.
		 * Hence don't count them.
		 */

		if (inst->session_type == MSM_VIDC_DECODER) {
			q = &inst->bufq[CAPTURE_PORT].vb2_bufq;
			for (i = 0; i < q->num_buffers; i++) {
				vb = q->bufs[i];
				if (vb && vb->state != VB2_BUF_STATE_ACTIVE &&
						vb->planes[0].bytesused)
					fw_out_qsize++;
			}
		} else {
			q = &inst->bufq[OUTPUT_PORT].vb2_bufq;
			for (i = 0; i < q->num_buffers; i++) {
				vb = q->bufs[i];
				if (vb && vb->state != VB2_BUF_STATE_ACTIVE)
					fw_out_qsize++;
			}
		}
	}

	return fw_out_qsize;
}

static int msm_dcvs_scale_clocks(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fw_pending_bufs = 0;
	int total_output_buf = 0;
	int min_output_buf = 0;
	int buffers_outside_fw = 0;
	struct msm_vidc_core *core;
	struct hal_buffer_requirements *output_buf_req;
	struct clock_data *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}

	if (!inst->clk_data.dcvs_mode || inst->batch.enable) {
		dprintk(VIDC_DBG, "Skip DCVS (dcvs %d, batching %d)\n",
			inst->clk_data.dcvs_mode, inst->batch.enable);
		/* Request right clocks (load normal clocks) */
		inst->clk_data.load = inst->clk_data.load_norm;
		return 0;
	}

	dcvs = &inst->clk_data;

	core = inst->core;
	mutex_lock(&inst->lock);
	buffers_outside_fw = get_bufs_outside_fw(inst);

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

	min_output_buf = output_buf_req->buffer_count_min;

	/* Buffers outside Display are with FW. */
	fw_pending_bufs = total_output_buf - buffers_outside_fw;
	dprintk(VIDC_PROF,
		"Counts : total_output_buf = %d Min buffers = %d fw_pending_bufs = %d buffers_outside_fw = %d\n",
		total_output_buf, min_output_buf, fw_pending_bufs,
			buffers_outside_fw);

	/*
	 * PMS decides clock level based on below algo

	 * Limits :
	 * max_threshold : Client extra allocated buffers. Client
	 * reserves these buffers for it's smooth flow.
	 * min_output_buf : HW requested buffers for it's smooth
	 * flow of buffers.
	 * min_threshold : Driver requested extra buffers for PMS.

	 * 1) When buffers outside FW are reaching client's extra buffers,
	 *    FW is slow and will impact pipeline, Increase clock.
	 * 2) When pending buffers with FW are same as FW requested,
	 *    pipeline has cushion to absorb FW slowness, Decrease clocks.
	 * 3) When none of 1) or 2) FW is just fast enough to maintain
	 *    pipeline, request Right Clocks.
	 */

	if (buffers_outside_fw <= dcvs->max_threshold)
		dcvs->load = dcvs->load_high;
	else if (fw_pending_bufs < min_output_buf)
		dcvs->load = dcvs->load_low;
	else
		dcvs->load = dcvs->load_norm;

	return rc;
}

static void msm_vidc_update_freq_entry(struct msm_vidc_inst *inst,
	unsigned long freq, u32 device_addr, bool is_turbo)
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
		if (!temp) {
			dprintk(VIDC_WARN, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		temp->freq = freq;
		temp->device_addr = device_addr;
		list_add_tail(&temp->list, &inst->freqs.list);
	}
	temp->turbo = !!is_turbo;
exit:
	mutex_unlock(&inst->freqs.lock);
}

void msm_vidc_clear_freq_entry(struct msm_vidc_inst *inst,
	u32 device_addr)
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

static unsigned long msm_vidc_max_freq(struct msm_vidc_core *core)
{
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	unsigned long freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	freq = allowed_clks_tbl[0].clock_rate;
	dprintk(VIDC_PROF, "Max rate = %lu\n", freq);
	return freq;
}

static unsigned long msm_vidc_adjust_freq(struct msm_vidc_inst *inst)
{
	struct vidc_freq_data *temp;
	unsigned long freq = 0;
	bool is_turbo = false;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry(temp, &inst->freqs.list, list) {
		freq = max(freq, temp->freq);
		if (temp->turbo) {
			is_turbo = true;
			break;
		}
	}
	mutex_unlock(&inst->freqs.lock);

	if (is_turbo) {
		return msm_vidc_max_freq(inst->core);
	}
	/* If current requirement is within DCVS limits, try DCVS. */

	if (freq < inst->clk_data.load_norm) {
		dprintk(VIDC_DBG, "Calling DCVS now\n");
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

void msm_comm_free_input_cr_table(struct msm_vidc_inst *inst)
{
	struct vidc_input_cr_data *temp, *next;

	mutex_lock(&inst->input_crs.lock);
	list_for_each_entry_safe(temp, next, &inst->input_crs.list, list) {
		list_del(&temp->list);
		kfree(temp);
	}
	INIT_LIST_HEAD(&inst->input_crs.list);
	mutex_unlock(&inst->input_crs.lock);
}

void msm_comm_update_input_cr(struct msm_vidc_inst *inst,
	u32 index, u32 cr)
{
	struct vidc_input_cr_data *temp, *next;
	bool found = false;

	mutex_lock(&inst->input_crs.lock);
	list_for_each_entry_safe(temp, next, &inst->input_crs.list, list) {
		if (temp->index == index) {
			temp->input_cr = cr;
			found = true;
			break;
		}
	}

	if (!found) {
		temp = kzalloc(sizeof(*temp), GFP_KERNEL);
		if (!temp)  {
			dprintk(VIDC_WARN, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		temp->index = index;
		temp->input_cr = cr;
		list_add_tail(&temp->list, &inst->input_crs.list);
	}
exit:
	mutex_unlock(&inst->input_crs.lock);
}

static unsigned long msm_vidc_calc_freq_ar50(struct msm_vidc_inst *inst,
	u32 filled_len)
{
	unsigned long freq = 0;
	unsigned long vpp_cycles = 0, vsp_cycles = 0;
	u32 vpp_cycles_per_mb;
	u32 mbs_per_second;
	struct msm_vidc_core *core = NULL;
	int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 rate = 0, fps;
	struct clock_data *dcvs = NULL;

	core = inst->core;
	dcvs = &inst->clk_data;

	mbs_per_second = msm_comm_get_inst_load_per_core(inst,
		LOAD_CALC_NO_QUIRKS);

	fps = msm_vidc_get_fps(inst);

	/*
	 * Calculate vpp, vsp cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

		vpp_cycles = mbs_per_second * vpp_cycles_per_mb;

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;

		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->clk_data.bitrate * 10) / 7;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		vpp_cycles = mbs_per_second * inst->clk_data.entry->vpp_cycles;

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;
		/* 10 / 7 is overhead factor */
		vsp_cycles += ((fps * filled_len * 8) * 10) / 7;

	} else {
		dprintk(VIDC_ERR, "Unknown session type = %s\n", __func__);
		return msm_vidc_max_freq(inst->core);
	}

	freq = max(vpp_cycles, vsp_cycles);

	dprintk(VIDC_DBG, "Update DCVS Load\n");
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq)
			break;
	}

	dcvs->load_norm = rate;
	dcvs->load_low = i < (core->resources.allowed_clks_tbl_size - 1) ?
		allowed_clks_tbl[i+1].clock_rate : dcvs->load_norm;
	dcvs->load_high = i > 0 ? allowed_clks_tbl[i-1].clock_rate :
		dcvs->load_norm;

	msm_dcvs_print_dcvs_stats(dcvs);

	dprintk(VIDC_PROF, "%s Inst %pK : Filled Len = %d Freq = %lu\n",
		__func__, inst, filled_len, freq);

	return freq;
}

static unsigned long msm_vidc_calc_freq(struct msm_vidc_inst *inst,
	u32 filled_len)
{
	unsigned long freq = 0;
	unsigned long vpp_cycles = 0, vsp_cycles = 0, fw_cycles = 0;
	u32 vpp_cycles_per_mb;
	u32 mbs_per_second;
	struct msm_vidc_core *core = NULL;
	int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 rate = 0, fps;
	struct clock_data *dcvs = NULL;
	u32 operating_rate, vsp_factor_num = 10, vsp_factor_den = 7;

	core = inst->core;
	dcvs = &inst->clk_data;

	mbs_per_second = msm_comm_get_inst_load_per_core(inst,
		LOAD_CALC_NO_QUIRKS);

	fps = msm_vidc_get_fps(inst);

	/*
	 * Calculate vpp, vsp, fw cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

		vpp_cycles = mbs_per_second * vpp_cycles_per_mb;
		/* 21 / 20 is overhead factor */
		vpp_cycles = (vpp_cycles * 21)/
				(inst->clk_data.work_route * 20);

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;

		/* bitrate is based on fps, scale it using operating rate */
		operating_rate = inst->clk_data.operating_rate >> 16;
		if (operating_rate > inst->prop.fps && inst->prop.fps) {
			vsp_factor_num *= operating_rate;
			vsp_factor_den *= inst->prop.fps;
		}
		vsp_cycles += ((u64)inst->clk_data.bitrate * vsp_factor_num) /
				vsp_factor_den;

		fw_cycles = fps * inst->core->resources.fw_cycles;

	} else if (inst->session_type == MSM_VIDC_DECODER) {
		vpp_cycles = mbs_per_second * inst->clk_data.entry->vpp_cycles;
		/* 21 / 20 is overhead factor */
		vpp_cycles = (vpp_cycles * 21)/
				(inst->clk_data.work_route * 20);

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;

		/* 10 / 7 is overhead factor */
		vsp_cycles += ((fps * filled_len * 8) * 10) / 7;

		fw_cycles = fps * inst->core->resources.fw_cycles;

	} else {
		dprintk(VIDC_ERR, "Unknown session type = %s\n", __func__);
		return msm_vidc_max_freq(inst->core);
	}

	freq = max(vpp_cycles, vsp_cycles);
	freq = max(freq, fw_cycles);

	dprintk(VIDC_DBG, "Update DCVS Load\n");
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq)
			break;
	}

	dcvs->load_norm = rate;
	dcvs->load_low = i < (core->resources.allowed_clks_tbl_size - 1) ?
		allowed_clks_tbl[i+1].clock_rate : dcvs->load_norm;
	dcvs->load_high = i > 0 ? allowed_clks_tbl[i-1].clock_rate :
		dcvs->load_norm;

	msm_dcvs_print_dcvs_stats(dcvs);

	dprintk(VIDC_PROF, "%s Inst %pK : Filled Len = %d Freq = %lu\n",
		__func__, inst, filled_len, freq);

	return freq;
}

int msm_vidc_set_clocks(struct msm_vidc_core *core)
{
	struct hfi_device *hdev;
	unsigned long freq_core_1 = 0, freq_core_2 = 0, rate = 0;
	unsigned long freq_core_max = 0;
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

		if (temp->clk_data.core_id == VIDC_CORE_ID_1)
			freq_core_1 += temp->clk_data.min_freq;
		else if (temp->clk_data.core_id == VIDC_CORE_ID_2)
			freq_core_2 += temp->clk_data.min_freq;
		else if (temp->clk_data.core_id == VIDC_CORE_ID_3) {
			freq_core_1 += temp->clk_data.min_freq;
			freq_core_2 += temp->clk_data.min_freq;
		}

		freq_core_max = max_t(unsigned long, freq_core_1, freq_core_2);

		if (msm_vidc_clock_voting) {
			dprintk(VIDC_PROF,
				"msm_vidc_clock_voting %d\n",
				 msm_vidc_clock_voting);
			freq_core_max = msm_vidc_clock_voting;
			break;
		}

		if (temp->clk_data.turbo_mode) {
			dprintk(VIDC_PROF,
				"Found an instance with Turbo request\n");
			freq_core_max = msm_vidc_max_freq(core);
			break;
		}
	}

	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq_core_max)
			break;
	}

	core->min_freq = freq_core_max;
	core->curr_freq = rate;
	mutex_unlock(&core->lock);

	dprintk(VIDC_PROF, "Min freq = %lu Current Freq = %lu\n",
		core->min_freq, core->curr_freq);
	rc = call_hfi_op(hdev, scale_clocks,
			hdev->hfi_device_data, core->curr_freq);

	return rc;
}

int msm_vidc_validate_operating_rate(struct msm_vidc_inst *inst,
	u32 operating_rate)
{
	struct msm_vidc_inst *temp;
	struct msm_vidc_core *core;
	unsigned long max_freq, freq_left, ops_left, load, cycles, freq = 0;
	unsigned long mbs_per_second;
	int rc = 0;
	u32 curr_operating_rate = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	curr_operating_rate = inst->clk_data.operating_rate >> 16;

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

	mbs_per_second = msm_comm_get_inst_load_per_core(inst,
		LOAD_CALC_NO_QUIRKS);

	cycles = inst->clk_data.entry->vpp_cycles;
	if (inst->session_type == MSM_VIDC_ENCODER)
		cycles = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			cycles;

	load = cycles * mbs_per_second;

	ops_left = load ? (freq_left / load) : 0;

	operating_rate = operating_rate >> 16;

	if ((curr_operating_rate * (1 + ops_left)) >= operating_rate ||
			msm_vidc_clock_voting ||
			inst->clk_data.buffer_counter < DCVS_FTB_WINDOW) {
		dprintk(VIDC_DBG,
			"Requestd operating rate is valid %u\n",
			operating_rate);
		rc = 0;
	} else {
		dprintk(VIDC_DBG,
			"Current load is high for requested settings. Cannot set operating rate to %u\n",
			operating_rate);
		rc = -EINVAL;
	}
	mutex_unlock(&core->lock);

	return rc;
}

int msm_comm_scale_clocks(struct msm_vidc_inst *inst)
{
	struct msm_vidc_buffer *temp, *next;
	unsigned long freq = 0;
	u32 filled_len = 0;
	u32 device_addr = 0;
	bool is_turbo = false;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(temp, next, &inst->registeredbufs.list, list) {
		if (temp->vvb.vb2_buf.type ==
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
					temp->flags & MSM_VIDC_FLAG_DEFERRED) {
			filled_len = max(filled_len,
				temp->vvb.vb2_buf.planes[0].bytesused);
			if (inst->session_type == MSM_VIDC_ENCODER &&
				(temp->vvb.flags &
				 V4L2_QCOM_BUF_FLAG_PERF_MODE)) {
				is_turbo = true;
			}
			device_addr = temp->smem[0].device_addr;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	if (!filled_len || !device_addr) {
		dprintk(VIDC_DBG, "%s No ETBs\n", __func__);
		goto no_clock_change;
	}

	freq = call_core_op(inst->core, calc_freq, inst, filled_len);

	msm_vidc_update_freq_entry(inst, freq, device_addr, is_turbo);

	freq = msm_vidc_adjust_freq(inst);

	inst->clk_data.min_freq = freq;

	if (inst->clk_data.buffer_counter < DCVS_FTB_WINDOW ||
		msm_vidc_clock_voting)
		inst->clk_data.min_freq = msm_vidc_max_freq(inst->core);
	else
		inst->clk_data.min_freq = freq;

	msm_vidc_set_clocks(inst->core);

no_clock_change:
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

	if (msm_vidc_clock_voting ||
			inst->flags & VIDC_THUMBNAIL ||
			inst->clk_data.low_latency_mode ||
			inst->batch.enable) {
		dprintk(VIDC_PROF, "DCVS disabled: %pK\n", inst);
		inst->clk_data.dcvs_mode = false;
		return false;
	}
	inst->clk_data.dcvs_mode = true;
	dprintk(VIDC_PROF, "DCVS enabled: %pK\n", inst);

	return true;
}

int msm_comm_init_clocks_and_bus_data(struct msm_vidc_inst *inst)
{
	int rc = 0, j = 0;
	int fourcc, count;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
				__func__, inst);
		return -EINVAL;
	}
	count = inst->core->resources.codec_data_count;
	fourcc = inst->session_type == MSM_VIDC_DECODER ?
		inst->fmts[OUTPUT_PORT].fourcc :
		inst->fmts[CAPTURE_PORT].fourcc;

	for (j = 0; j < count; j++) {
		if (inst->core->resources.codec_data[j].session_type ==
				inst->session_type &&
				inst->core->resources.codec_data[j].fourcc ==
				fourcc) {
			inst->clk_data.entry =
				&inst->core->resources.codec_data[j];
			break;
		}
	}
	return rc;
}

void msm_clock_data_reset(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	int i = 0, rc = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 total_freq = 0, rate = 0, load;
	int cycles;
	struct clock_data *dcvs;
	struct hal_buffer_requirements *output_buf_req;

	dprintk(VIDC_DBG, "Init DCVS Load\n");

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return;
	}

	core = inst->core;
	dcvs = &inst->clk_data;
	load = msm_comm_get_inst_load_per_core(inst, LOAD_CALC_NO_QUIRKS);
	cycles = inst->clk_data.entry->vpp_cycles;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		cycles = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			cycles;

		dcvs->buffer_type = HAL_BUFFER_INPUT;
		dcvs->min_threshold =
			msm_vidc_get_extra_buff_count(inst, HAL_BUFFER_INPUT);
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		dcvs->buffer_type = msm_comm_get_hal_output_buffer(inst);
		output_buf_req = get_buff_req_buffer(inst,
				dcvs->buffer_type);
		if (!output_buf_req) {
			dprintk(VIDC_ERR,
				"%s: No bufer req for buffer type %x\n",
				__func__, dcvs->buffer_type);
			return;
		}
		dcvs->max_threshold = output_buf_req->buffer_count_actual -
			output_buf_req->buffer_count_min_host + 2;

		dcvs->min_threshold =
			msm_vidc_get_extra_buff_count(inst, dcvs->buffer_type);
	} else {
		return;
	}

	total_freq = cycles * load;

	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= total_freq)
			break;
	}

	dcvs->load = dcvs->load_norm = rate;

	dcvs->load_low = i < (core->resources.allowed_clks_tbl_size - 1) ?
		allowed_clks_tbl[i+1].clock_rate : dcvs->load_norm;
	dcvs->load_high = i > 0 ? allowed_clks_tbl[i-1].clock_rate :
		dcvs->load_norm;

	inst->clk_data.buffer_counter = 0;

	msm_dcvs_print_dcvs_stats(dcvs);

	rc = msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR, "%s Failed to scale Clocks and Bus\n",
			__func__);
}

static bool is_output_buffer(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		return buffer_type == HAL_BUFFER_OUTPUT2;
	} else {
		return buffer_type == HAL_BUFFER_OUTPUT;
	}
}

int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	int count = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}
	/*
	 * no extra buffers for thumbnail session because
	 * neither dcvs nor batching will be enabled
	 */
	if (is_thumbnail_session(inst))
		return 0;

	/* Add DCVS extra buffer count */
	if (inst->core->resources.dcvs) {
		if (is_decode_session(inst) &&
			is_output_buffer(inst, buffer_type)) {
			count += DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
		} else if ((is_encode_session(inst) &&
			buffer_type == HAL_BUFFER_INPUT)) {
			count += DCVS_ENC_EXTRA_INPUT_BUFFERS;
		}
	}

	/*
	 * if platform supports decode batching ensure minimum
	 * batch size count of extra buffers added on output port
	 */
	if (is_output_buffer(inst, buffer_type)) {
		if (inst->core->resources.decode_batching &&
			is_decode_session(inst) &&
			count < inst->batch.size)
			count = inst->batch.size;
	}

	return count;
}

int msm_vidc_decide_work_route(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_video_work_route pdata;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;

	pdata.video_work_route = 2;
	if (inst->session_type == MSM_VIDC_DECODER) {
		switch (inst->fmts[OUTPUT_PORT].fourcc) {
		case V4L2_PIX_FMT_MPEG2:
			pdata.video_work_route = 1;
			break;
		case V4L2_PIX_FMT_H264:
			if (inst->pic_struct !=
				MSM_VIDC_PIC_STRUCT_PROGRESSIVE)
				pdata.video_work_route = 1;
			break;
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		u32 slice_mode = 0;

		switch (inst->fmts[CAPTURE_PORT].fourcc) {
		case V4L2_PIX_FMT_VP8:
		case V4L2_PIX_FMT_TME:
			pdata.video_work_route = 1;
			goto decision_done;
		}

		slice_mode =  msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
		if (slice_mode ==
			V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
			pdata.video_work_route = 1;
		}
	} else {
		return -EINVAL;
	}

decision_done:

	inst->clk_data.work_route = pdata.video_work_route;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HAL_PARAM_VIDEO_WORK_ROUTE,
			(void *)&pdata);
	if (rc)
		dprintk(VIDC_WARN,
			" Failed to configure work route %pK\n", inst);

	return rc;
}

static int msm_vidc_decide_work_mode_ar50(struct msm_vidc_inst *inst)
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
	} else if (inst->session_type == MSM_VIDC_ENCODER)
		pdata.video_work_mode = VIDC_WORK_MODE_1;
	else {
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

int msm_vidc_decide_work_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_video_work_mode pdata;
	struct hal_enable latency;
	u32 yuv_size = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;

	if (inst->session_type == MSM_VIDC_DECODER) {
		if (inst->clk_data.low_latency_mode) {
			pdata.video_work_mode = VIDC_WORK_MODE_1;
			goto decision_done;
		}
		pdata.video_work_mode = VIDC_WORK_MODE_2;
		switch (inst->fmts[OUTPUT_PORT].fourcc) {
		case V4L2_PIX_FMT_MPEG2:
			pdata.video_work_mode = VIDC_WORK_MODE_1;
			break;
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_HEVC:
		case V4L2_PIX_FMT_VP8:
		case V4L2_PIX_FMT_VP9:
			yuv_size = inst->prop.height[OUTPUT_PORT] *
				inst->prop.width[OUTPUT_PORT];
			if ((inst->pic_struct !=
				 MSM_VIDC_PIC_STRUCT_PROGRESSIVE) ||
				(yuv_size  <= 1280 * 720))
				pdata.video_work_mode = VIDC_WORK_MODE_1;
			break;
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		u32 codec = inst->fmts[CAPTURE_PORT].fourcc;
		u32 width = inst->prop.width[OUTPUT_PORT];

		pdata.video_work_mode = VIDC_WORK_MODE_2;

		if (codec == V4L2_PIX_FMT_H264 && width > 3840)
			goto decision_done;

		if (inst->clk_data.low_latency_mode) {
			pdata.video_work_mode = VIDC_WORK_MODE_1;
			goto decision_done;
		}

		switch (codec) {
		case V4L2_PIX_FMT_VP8:
		{
			if (width <= 3840)  {
				pdata.video_work_mode = VIDC_WORK_MODE_1;
				goto decision_done;
			}
			break;
		}
		case V4L2_PIX_FMT_TME:
		{
			pdata.video_work_mode = VIDC_WORK_MODE_1;
			goto decision_done;
		}
		}

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
	mbs_per_frame = msm_vidc_get_mbs_per_frame(inst);
	if (mbs_per_frame > inst->core->resources.max_hq_mbs_per_frame ||
		msm_vidc_get_fps(inst) > inst->core->resources.max_hq_fps) {
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
	u32 core_id, bool lp_mode, bool real_time)
{
	struct msm_vidc_inst *inst = NULL;
	u32 current_inst_mbs_per_sec = 0, load = 0;
	bool real_time_mode = false;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		u32 cycles, lp_cycles;

		real_time_mode = inst->flags & VIDC_REALTIME ? true : false;
		if (!(inst->clk_data.core_id & core_id))
			continue;
		if (real_time_mode != real_time)
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
		current_inst_mbs_per_sec = msm_comm_get_inst_load_per_core(inst,
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

	core0_load = get_core_load(core, VIDC_CORE_ID_1, false, true);
	core1_load = get_core_load(core, VIDC_CORE_ID_2, false, true);
	core0_lp_load = get_core_load(core, VIDC_CORE_ID_1, true, true);
	core1_lp_load = get_core_load(core, VIDC_CORE_ID_2, true, true);

	min_load = min(core0_load, core1_load);
	min_core_id = core0_load < core1_load ?
		VIDC_CORE_ID_1 : VIDC_CORE_ID_2;
	min_lp_load = min(core0_lp_load, core1_lp_load);
	min_lp_core_id = core0_lp_load < core1_lp_load ?
		VIDC_CORE_ID_1 : VIDC_CORE_ID_2;

	lp_cycles = inst->session_type == MSM_VIDC_ENCODER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;
	/*
	 * Incase there is only 1 core enabled, mark it as the core
	 * with min load. This ensures that this core is selected and
	 * video session is set to run on the enabled core.
	 */
	if (inst->capability.max_video_cores.max <= VIDC_CORE_ID_1) {
		min_core_id = min_lp_core_id = VIDC_CORE_ID_1;
		min_load = core0_load;
		min_lp_load = core0_lp_load;
	}

	current_inst_load = (msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS) *
		inst->clk_data.entry->vpp_cycles)/inst->clk_data.work_route;

	current_inst_lp_load = (msm_comm_get_inst_load(inst,
		LOAD_CALC_NO_QUIRKS) * lp_cycles)/inst->clk_data.work_route;

	dprintk(VIDC_DBG, "Core 0 RT Load = %d Core 1 RT Load = %d\n",
		 core0_load, core1_load);
	dprintk(VIDC_DBG, "Core 0 RT LP Load = %d Core 1 RT LP Load = %d\n",
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
	if (inst->session_type == MSM_VIDC_ENCODER && hier_mode &&
		inst->capability.max_video_cores.max >= VIDC_CORE_ID_3) {
		if (current_inst_load / 2 + core0_load <= max_freq &&
			current_inst_load / 2 + core1_load <= max_freq) {
			if (inst->clk_data.work_mode == VIDC_WORK_MODE_2) {
				inst->clk_data.core_id = VIDC_CORE_ID_3;
				msm_vidc_power_save_mode_enable(inst, false);
				goto decision_done;
			}
		}
	}

	if (inst->session_type == MSM_VIDC_ENCODER && hier_mode &&
		inst->capability.max_video_cores.max >= VIDC_CORE_ID_3) {
		if (current_inst_lp_load / 2 +
				core0_lp_load <= max_freq &&
			current_inst_lp_load / 2 +
				core1_lp_load <= max_freq) {
			if (inst->clk_data.work_mode == VIDC_WORK_MODE_2) {
				inst->clk_data.core_id = VIDC_CORE_ID_3;
				msm_vidc_power_save_mode_enable(inst, true);
				goto decision_done;
			}
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

	msm_print_core_status(core, VIDC_CORE_ID_1);
	msm_print_core_status(core, VIDC_CORE_ID_2);

	return rc;
}

void msm_vidc_init_core_clk_ops(struct msm_vidc_core *core)
{
	if (!core)
		return;

	if (core->platform_data->vpu_ver == VPU_VERSION_4)
		core->core_ops = &core_ops_vpu4;
	else
		core->core_ops = &core_ops_vpu5;
}

void msm_print_core_status(struct msm_vidc_core *core, u32 core_id)
{
	struct msm_vidc_inst *inst = NULL;

	dprintk(VIDC_PROF, "Instances running on core %u", core_id);
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {

		if ((inst->clk_data.core_id != core_id) &&
			(inst->clk_data.core_id != VIDC_CORE_ID_3))
			continue;

		dprintk(VIDC_PROF,
			"inst %pK (%4ux%4u) to (%4ux%4u) %3u %s %s %s %s %lu\n",
			inst,
			inst->prop.width[OUTPUT_PORT],
			inst->prop.height[OUTPUT_PORT],
			inst->prop.width[CAPTURE_PORT],
			inst->prop.height[CAPTURE_PORT],
			inst->prop.fps,
			inst->session_type == MSM_VIDC_ENCODER ? "ENC" : "DEC",
			inst->clk_data.work_mode == VIDC_WORK_MODE_1 ?
				"WORK_MODE_1" : "WORK_MODE_2",
			inst->flags & VIDC_LOW_POWER ? "LP" : "HQ",
			inst->flags & VIDC_REALTIME ? "RealTime" : "NonRTime",
			inst->clk_data.min_freq);
	}
	mutex_unlock(&core->lock);
}
