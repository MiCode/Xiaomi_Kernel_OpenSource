// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"
#include "msm_vidc_buffer_calculations.h"
#include "msm_vidc_bus.h"
#include "vidc_hfi.h"

#define MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR (4 << 16)

#define MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO (1 << 16)
#define MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO (5 << 16)

#define MSM_VIDC_SESSION_INACTIVE_THRESHOLD_MS 1000

static int msm_vidc_decide_work_mode_ar50_lt(struct msm_vidc_inst *inst);
static unsigned long msm_vidc_calc_freq_ar50_lt(struct msm_vidc_inst *inst,
	u32 filled_len);
static unsigned long msm_vidc_calc_freq_iris2(struct msm_vidc_inst *inst,
	u32 filled_len);

struct msm_vidc_core_ops core_ops_ar50_lt = {
	.calc_freq = msm_vidc_calc_freq_ar50_lt,
	.decide_work_route = NULL,
	.decide_work_mode = msm_vidc_decide_work_mode_ar50_lt,
	.decide_core_and_power_mode =
		msm_vidc_decide_core_and_power_mode_ar50lt,
	.calc_bw = calc_bw_ar50lt,
};

struct msm_vidc_core_ops core_ops_iris2 = {
	.calc_freq = msm_vidc_calc_freq_iris2,
	.decide_work_route = msm_vidc_decide_work_route_iris2,
	.decide_work_mode = msm_vidc_decide_work_mode_iris2,
	.decide_core_and_power_mode = msm_vidc_decide_core_and_power_mode_iris2,
	.calc_bw = calc_bw_iris2,
};

static inline unsigned long get_ubwc_compression_ratio(
	struct ubwc_cr_stats_info_type ubwc_stats_info)
{
	unsigned long sum = 0, weighted_sum = 0;
	unsigned long compression_ratio = 0;

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

bool res_is_less_than(u32 width, u32 height,
			u32 ref_width, u32 ref_height)
{
	u32 num_mbs = NUM_MBS_PER_FRAME(height, width);
	u32 max_side = max(ref_width, ref_height);

	if (num_mbs < NUM_MBS_PER_FRAME(ref_height, ref_width) &&
		width < max_side &&
		height < max_side)
		return true;
	else
		return false;
}

bool res_is_greater_than(u32 width, u32 height,
				u32 ref_width, u32 ref_height)
{
	u32 num_mbs = NUM_MBS_PER_FRAME(height, width);
	u32 max_side = max(ref_width, ref_height);

	if (num_mbs > NUM_MBS_PER_FRAME(ref_height, ref_width) ||
		width > max_side ||
		height > max_side)
		return true;
	else
		return false;
}

bool res_is_greater_than_or_equal_to(u32 width, u32 height,
				u32 ref_width, u32 ref_height)
{
	u32 num_mbs = NUM_MBS_PER_FRAME(height, width);
	u32 max_side = max(ref_width, ref_height);

	if (num_mbs >= NUM_MBS_PER_FRAME(ref_height, ref_width) ||
		width >= max_side ||
		height >= max_side)
		return true;
	else
		return false;
}

bool res_is_less_than_or_equal_to(u32 width, u32 height,
				u32 ref_width, u32 ref_height)
{
	u32 num_mbs = NUM_MBS_PER_FRAME(height, width);
	u32 max_side = max(ref_width, ref_height);

	if (num_mbs <= NUM_MBS_PER_FRAME(ref_height, ref_width) &&
		width <= max_side &&
		height <= max_side)
		return true;
	else
		return false;
}

bool is_vpp_delay_allowed(struct msm_vidc_inst *inst)
{
	u32 codec = get_v4l2_codec(inst);
	u32 mbpf = msm_vidc_get_mbs_per_frame(inst);

	return (inst->core->resources.has_vpp_delay &&
		is_decode_session(inst) &&
		!is_thumbnail_session(inst) &&
		(mbpf >= NUM_MBS_PER_FRAME(7680, 3840)) &&
		(codec == V4L2_PIX_FMT_H264
		|| codec == V4L2_PIX_FMT_HEVC));
}

int msm_vidc_get_mbs_per_frame(struct msm_vidc_inst *inst)
{
	int height, width;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;

	out_f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	inp_f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	height = max(out_f->fmt.pix_mp.height,
			inp_f->fmt.pix_mp.height);
	width = max(out_f->fmt.pix_mp.width,
			inp_f->fmt.pix_mp.width);

	return NUM_MBS_PER_FRAME(height, width);
}

int msm_vidc_get_fps(struct msm_vidc_inst *inst)
{
	int fps;

	if (inst->clk_data.operating_rate > inst->clk_data.frame_rate)
		fps = (inst->clk_data.operating_rate >> 16) ?
			(inst->clk_data.operating_rate >> 16) : 1;
	else
		fps = inst->clk_data.frame_rate >> 16;

	return fps;
}

static inline bool is_active_session(u64 prev, u64 curr)
{
	u64 ts_delta;

	if (!prev || !curr)
		return true;

	ts_delta = (prev < curr) ? curr - prev : prev - curr;

	return ((ts_delta / NSEC_PER_MSEC) <=
			MSM_VIDC_SESSION_INACTIVE_THRESHOLD_MS);
}

void update_recon_stats(struct msm_vidc_inst *inst,
	struct recon_stats_type *recon_stats)
{
	struct v4l2_ctrl *ctrl;
	struct recon_buf *binfo;
	u32 CR = 0, CF = 0;
	u32 frame_size;

	if (inst->core->resources.ubwc_stats_in_fbd == 1)
		return;

	/* do not consider recon stats in case of superframe */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
	if (ctrl->val)
		return;

	CR = get_ubwc_compression_ratio(recon_stats->ubwc_stats_info);

	frame_size = (msm_vidc_get_mbs_per_frame(inst) / (32 * 8) * 3) / 2;

	if (frame_size)
		CF = recon_stats->complexity_number / frame_size;
	else
		CF = MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR;
	mutex_lock(&inst->refbufs.lock);
	list_for_each_entry(binfo, &inst->refbufs.list, list) {
		if (binfo->buffer_index ==
				recon_stats->buffer_index) {
			binfo->CR = CR;
			binfo->CF = CF;
			break;
		}
	}
	mutex_unlock(&inst->refbufs.lock);
}

static int fill_dynamic_stats(struct msm_vidc_inst *inst,
	struct vidc_bus_vote_data *vote_data)
{
	struct recon_buf *binfo, *nextb;
	struct vidc_input_cr_data *temp, *next;
	u32 max_cr = MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO;
	u32 max_cf = MSM_VIDC_MIN_UBWC_COMPLEXITY_FACTOR;
	u32 max_input_cr = MSM_VIDC_MIN_UBWC_COMPRESSION_RATIO;
	u32 min_cf = MSM_VIDC_MAX_UBWC_COMPLEXITY_FACTOR;
	u32 min_input_cr = MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO;
	u32 min_cr = MSM_VIDC_MAX_UBWC_COMPRESSION_RATIO;

	if (inst->core->resources.ubwc_stats_in_fbd == 1) {
		mutex_lock(&inst->ubwc_stats_lock);
		if (inst->ubwc_stats.is_valid == 1) {
			min_cr = inst->ubwc_stats.worst_cr;
			max_cf = inst->ubwc_stats.worst_cf;
			min_input_cr = inst->ubwc_stats.worst_cr;
		}
		mutex_unlock(&inst->ubwc_stats_lock);
	} else {
		mutex_lock(&inst->refbufs.lock);
		list_for_each_entry_safe(binfo, nextb,
						&inst->refbufs.list, list) {
			if (binfo->CR) {
				min_cr = min(min_cr, binfo->CR);
				max_cr = max(max_cr, binfo->CR);
			}
			if (binfo->CF) {
				min_cf = min(min_cf, binfo->CF);
				max_cf = max(max_cf, binfo->CF);
			}
		}
		mutex_unlock(&inst->refbufs.lock);

		mutex_lock(&inst->input_crs.lock);
		list_for_each_entry_safe(temp, next,
						&inst->input_crs.list, list) {
			min_input_cr = min(min_input_cr, temp->input_cr);
			max_input_cr = max(max_input_cr, temp->input_cr);
		}
		mutex_unlock(&inst->input_crs.lock);
	}

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

	s_vpr_p(inst->sid,
		"Input CR = %d Recon CR = %d Complexity Factor = %d\n",
		vote_data->input_cr, vote_data->compression_ratio,
		vote_data->complexity_factor);

	return 0;
}

int msm_comm_set_buses(struct msm_vidc_core *core, u32 sid)
{
	int rc = 0;
	struct msm_vidc_inst *inst = NULL;
	struct hfi_device *hdev;
	unsigned long total_bw_ddr = 0, total_bw_llcc = 0;
	u64 curr_time_ns;

	if (!core || !core->device) {
		s_vpr_e(sid, "%s: Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}
	hdev = core->device;
	curr_time_ns = ktime_get_ns();

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		struct msm_vidc_buffer *temp, *next;
		u32 filled_len = 0;
		u32 device_addr = 0;

		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry_safe(temp, next,
				&inst->registeredbufs.list, list) {
			if (temp->vvb.vb2_buf.type == INPUT_MPLANE) {
				filled_len = max(filled_len,
					temp->vvb.vb2_buf.planes[0].bytesused);
				device_addr = temp->smem[0].device_addr;
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);

		if (!filled_len || !device_addr) {
			s_vpr_l(sid, "%s: no input\n", __func__);
			continue;
		}

		/* skip inactive session bus bandwidth */
		if (!is_active_session(inst->last_qbuf_time_ns, curr_time_ns)) {
			inst->active = false;
			continue;
		}

		if (inst->bus_data.power_mode == VIDC_POWER_TURBO) {
			total_bw_ddr = total_bw_llcc = INT_MAX;
			break;
		}
		total_bw_ddr += inst->bus_data.calc_bw_ddr;
		total_bw_llcc += inst->bus_data.calc_bw_llcc;
	}
	mutex_unlock(&core->lock);

	rc = call_hfi_op(hdev, vote_bus, hdev->hfi_device_data,
		total_bw_ddr, total_bw_llcc, sid);

	return rc;
}

int msm_comm_vote_bus(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct vidc_bus_vote_data *vote_data = NULL;
	bool is_turbo = false;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;
	struct msm_vidc_buffer *temp, *next;
	u32 filled_len = 0;
	u32 device_addr = 0;
	int codec = 0;

	if (!inst || !inst->core) {
		d_vpr_e("%s: Invalid args: %pK\n", __func__, inst);
		return -EINVAL;
	}
	core = inst->core;
	vote_data = &inst->bus_data;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(temp, next,
			&inst->registeredbufs.list, list) {
		if (temp->vvb.vb2_buf.type == INPUT_MPLANE) {
			filled_len = max(filled_len,
				temp->vvb.vb2_buf.planes[0].bytesused);
			device_addr = temp->smem[0].device_addr;
		}
		if (inst->session_type == MSM_VIDC_ENCODER &&
			(temp->vvb.flags & V4L2_BUF_FLAG_PERF_MODE)) {
			is_turbo = true;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	if (!filled_len || !device_addr) {
		s_vpr_l(inst->sid, "%s: no input\n", __func__);
		return 0;
	}

	vote_data->sid = inst->sid;
	vote_data->domain = get_hal_domain(inst->session_type, inst->sid);
	vote_data->power_mode = 0;
	if (inst->clk_data.buffer_counter < DCVS_FTB_WINDOW)
		vote_data->power_mode = VIDC_POWER_TURBO;
	if (msm_vidc_clock_voting || is_turbo || is_turbo_session(inst))
		vote_data->power_mode = VIDC_POWER_TURBO;

	if (vote_data->power_mode != VIDC_POWER_TURBO) {
		out_f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		inp_f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		switch (inst->session_type) {
		case MSM_VIDC_DECODER:
			codec = inp_f->fmt.pix_mp.pixelformat;
			break;
		case MSM_VIDC_ENCODER:
			codec = out_f->fmt.pix_mp.pixelformat;
			break;
		default:
			s_vpr_e(inst->sid, "%s: invalid session_type %#x\n",
				__func__, inst->session_type);
			break;
		}

		vote_data->codec = get_hal_codec(codec, inst->sid);
		vote_data->input_width = inp_f->fmt.pix_mp.width;
		vote_data->input_height = inp_f->fmt.pix_mp.height;
		vote_data->output_width = out_f->fmt.pix_mp.width;
		vote_data->output_height = out_f->fmt.pix_mp.height;
		vote_data->lcu_size = (codec == V4L2_PIX_FMT_HEVC ||
				codec == V4L2_PIX_FMT_VP9) ? 32 : 16;

		vote_data->fps = msm_vidc_get_fps(inst);
		if (inst->session_type == MSM_VIDC_ENCODER) {
			vote_data->bitrate = inst->clk_data.bitrate;
			vote_data->rotation =
				msm_comm_g_ctrl_for_id(inst, V4L2_CID_ROTATE);
			vote_data->b_frames_enabled =
				msm_comm_g_ctrl_for_id(inst,
					V4L2_CID_MPEG_VIDEO_B_FRAMES) != 0;
			/* scale bitrate if operating rate is larger than fps */
			if (vote_data->fps > (inst->clk_data.frame_rate >> 16)
				&& (inst->clk_data.frame_rate >> 16)) {
				vote_data->bitrate = vote_data->bitrate /
				(inst->clk_data.frame_rate >> 16) *
				vote_data->fps;
			}
		} else if (inst->session_type == MSM_VIDC_DECODER) {
			vote_data->bitrate =
				filled_len * vote_data->fps * 8;
		}

		if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_PRIMARY) {
			vote_data->color_formats[0] =
				msm_comm_get_hfi_uncompressed(
				inst->clk_data.opb_fourcc, inst->sid);
			vote_data->num_formats = 1;
		} else {
			vote_data->color_formats[0] =
				msm_comm_get_hfi_uncompressed(
				inst->clk_data.dpb_fourcc, inst->sid);
			vote_data->color_formats[1] =
				msm_comm_get_hfi_uncompressed(
				inst->clk_data.opb_fourcc, inst->sid);
			vote_data->num_formats = 2;
		}
		vote_data->work_mode = inst->clk_data.work_mode;
		fill_dynamic_stats(inst, vote_data);

		if (core->resources.sys_cache_res_set)
			vote_data->use_sys_cache = true;

		vote_data->num_vpp_pipes =
			inst->core->platform_data->num_vpp_pipes;

		call_core_op(core, calc_bw, vote_data);
	}

	rc = msm_comm_set_buses(core, inst->sid);

	return rc;
}

static int msm_dcvs_scale_clocks(struct msm_vidc_inst *inst,
		unsigned long freq)
{
	int rc = 0;
	int bufs_with_fw = 0;
	struct msm_vidc_format *fmt;
	struct clock_data *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (!inst->clk_data.dcvs_mode || inst->batch.enable) {
		s_vpr_l(inst->sid, "Skip DCVS (dcvs %d, batching %d)\n",
			inst->clk_data.dcvs_mode, inst->batch.enable);
		inst->clk_data.dcvs_flags = 0;
		return 0;
	}

	dcvs = &inst->clk_data;

	if (is_decode_session(inst)) {
		bufs_with_fw = msm_comm_num_queued_bufs(inst, OUTPUT_MPLANE);
		fmt = &inst->fmts[OUTPUT_PORT];
	} else {
		bufs_with_fw = msm_comm_num_queued_bufs(inst, INPUT_MPLANE);
		fmt = &inst->fmts[INPUT_PORT];
	}

	/* +1 as one buffer is going to be queued after the function */
	bufs_with_fw += 1;

	/*
	 * DCVS decides clock level based on below algo

	 * Limits :
	 * min_threshold : Buffers required for reference by FW.
	 * nom_threshold : Midpoint of Min and Max thresholds
	 * max_threshold : Min Threshold + DCVS extra buffers, allocated
	 *				   for smooth flow.
	 * 1) When buffers outside FW are reaching client's extra buffers,
	 *    FW is slow and will impact pipeline, Increase clock.
	 * 2) When pending buffers with FW are less than FW requested,
	 *    pipeline has cushion to absorb FW slowness, Decrease clocks.
	 * 3) When DCVS has engaged(Inc or Dec) and pending buffers with FW
	 *    transitions past the nom_threshold, switch to calculated load.
	 *    This smoothens the clock transitions.
	 * 4) Otherwise maintain previous Load config.
	 */

	if (bufs_with_fw >= dcvs->max_threshold) {
		dcvs->dcvs_flags = MSM_VIDC_DCVS_INCR;
	} else if (bufs_with_fw < dcvs->min_threshold) {
		dcvs->dcvs_flags = MSM_VIDC_DCVS_DECR;
	} else if ((dcvs->dcvs_flags & MSM_VIDC_DCVS_DECR &&
		    bufs_with_fw >= dcvs->nom_threshold) ||
		   (dcvs->dcvs_flags & MSM_VIDC_DCVS_INCR &&
		    bufs_with_fw <= dcvs->nom_threshold) ||
		   (inst->session_type == MSM_VIDC_ENCODER &&
		    dcvs->dcvs_flags & MSM_VIDC_DCVS_DECR &&
		    bufs_with_fw >= dcvs->min_threshold))
		dcvs->dcvs_flags = 0;

	s_vpr_p(inst->sid, "DCVS: bufs_with_fw %d Th[%d %d %d] Flag %#x\n",
		bufs_with_fw, dcvs->min_threshold,
		dcvs->nom_threshold, dcvs->max_threshold,
		dcvs->dcvs_flags);

	return rc;
}

static unsigned long msm_vidc_max_freq(struct msm_vidc_core *core, u32 sid)
{
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	unsigned long freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	freq = allowed_clks_tbl[0].clock_rate;
	s_vpr_l(sid, "Max rate = %lu\n", freq);
	return freq;
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
			s_vpr_e(inst->sid, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		temp->index = index;
		temp->input_cr = cr;
		list_add_tail(&temp->list, &inst->input_crs.list);
	}
exit:
	mutex_unlock(&inst->input_crs.lock);
}

static unsigned long msm_vidc_calc_freq_ar50_lt(struct msm_vidc_inst *inst,
	u32 filled_len)
{
	u64 freq = 0, vpp_cycles = 0, vsp_cycles = 0;
	u64 fw_cycles = 0, fw_vpp_cycles = 0;
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
							 LOAD_POWER);

	fps = msm_vidc_get_fps(inst);

	/*
	 * Calculate vpp, vsp cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	fw_cycles = fps * inst->core->resources.fw_cycles;
	fw_vpp_cycles = fps * inst->core->resources.fw_vpp_cycles;

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

		vpp_cycles = mbs_per_second * vpp_cycles_per_mb;
		/* 21 / 20 is minimum overhead factor */
		vpp_cycles += max(vpp_cycles / 20, fw_vpp_cycles);

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;

		/* 10 / 7 is overhead factor */
		vsp_cycles += (inst->clk_data.bitrate * 10) / 7;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		vpp_cycles = mbs_per_second * inst->clk_data.entry->vpp_cycles;
		/* 21 / 20 is minimum overhead factor */
		vpp_cycles += max(vpp_cycles / 20, fw_vpp_cycles);

		vsp_cycles = mbs_per_second * inst->clk_data.entry->vsp_cycles;
		/* 10 / 7 is overhead factor */
		vsp_cycles += div_u64((fps * filled_len * 8 * 10), 7);

	} else {
		s_vpr_e(inst->sid, "%s: Unknown session type\n", __func__);
		return msm_vidc_max_freq(inst->core, inst->sid);
	}

	freq = max(vpp_cycles, vsp_cycles);
	freq = max(freq, fw_cycles);

	s_vpr_l(inst->sid, "Update DCVS Load\n");
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq)
			break;
	}

	if (i < 0)
		i = 0;

	s_vpr_p(inst->sid, "%s: Inst %pK : Filled Len = %d Freq = %llu\n",
		__func__, inst, filled_len, freq);

	return (unsigned long) freq;
}

static unsigned long msm_vidc_calc_freq_iris2(struct msm_vidc_inst *inst,
	u32 filled_len)
{
	u64 vsp_cycles = 0, vpp_cycles = 0, fw_cycles = 0, freq = 0;
	u64 fw_vpp_cycles = 0;
	u32 vpp_cycles_per_mb;
	u32 mbs_per_second;
	struct msm_vidc_core *core = NULL;
	u32 fps;
	struct clock_data *dcvs = NULL;
	u32 operating_rate, vsp_factor_num = 1, vsp_factor_den = 1;
	u32 base_cycles = 0;
	u32 codec = 0;
	u64 bitrate = 0;

	core = inst->core;
	dcvs = &inst->clk_data;

	mbs_per_second = msm_comm_get_inst_load_per_core(inst,
							 LOAD_POWER);

	fps = msm_vidc_get_fps(inst);

	/*
	 * Calculate vpp, vsp, fw cycles separately for encoder and decoder.
	 * Even though, most part is common now, in future it may change
	 * between them.
	 */

	fw_cycles = fps * inst->core->resources.fw_cycles;
	fw_vpp_cycles = fps * inst->core->resources.fw_vpp_cycles;

	if (inst->session_type == MSM_VIDC_ENCODER) {
		vpp_cycles_per_mb = inst->flags & VIDC_LOW_POWER ?
			inst->clk_data.entry->low_power_cycles :
			inst->clk_data.entry->vpp_cycles;

		vpp_cycles = mbs_per_second * vpp_cycles_per_mb /
				inst->clk_data.work_route;
		/* Factor 1.25 for IbP and 1.375 for I1B2b1P GOP structure */
		if (is_hier_b_session(inst)) {
			vpp_cycles += (vpp_cycles / 4) + (vpp_cycles / 8);
		} else if (msm_comm_g_ctrl_for_id(inst,
					V4L2_CID_MPEG_VIDEO_B_FRAMES)) {
			vpp_cycles += vpp_cycles / 4;
		}

		/* 21 / 20 is minimum overhead factor */
		vpp_cycles += max(div_u64(vpp_cycles, 20), fw_vpp_cycles);
		/* 1.01 is multi-pipe overhead */
		if (inst->clk_data.work_route > 1)
			vpp_cycles += div_u64(vpp_cycles, 100);
		/*
		 * 1080p@480fps usecase needs exactly 338MHz
		 * without any margin left. Hence, adding 2 percent
		 * extra to bump it to next level (366MHz).
		 */
		if (fps == 480)
			vpp_cycles += div_u64(vpp_cycles * 2, 100);

		/*
		 * Add 5 percent extra for 720p@960fps use case
		 * to bump it to next level (366MHz).
		 */
		if (fps == 960)
			vpp_cycles += div_u64(vpp_cycles * 5, 100);

		/* VSP */
		/* bitrate is based on fps, scale it using operating rate */
		operating_rate = inst->clk_data.operating_rate >> 16;
		if (operating_rate > (inst->clk_data.frame_rate >> 16) &&
			(inst->clk_data.frame_rate >> 16)) {
			vsp_factor_num = operating_rate;
			vsp_factor_den = inst->clk_data.frame_rate >> 16;
		}
		vsp_cycles = div_u64(((u64)inst->clk_data.bitrate *
					vsp_factor_num), vsp_factor_den);

		codec = get_v4l2_codec(inst);
		base_cycles = inst->clk_data.entry->vsp_cycles;
		if (codec == V4L2_PIX_FMT_VP9) {
			vsp_cycles = div_u64(vsp_cycles * 170, 100);
		} else if (inst->entropy_mode == HFI_H264_ENTROPY_CABAC) {
			vsp_cycles = div_u64(vsp_cycles * 135, 100);
		} else {
			base_cycles = 0;
			vsp_cycles = div_u64(vsp_cycles, 2);
		}
		/* VSP FW Overhead 1.05 */
		vsp_cycles = div_u64(vsp_cycles * 21, 20);

		if (inst->clk_data.work_mode == HFI_WORKMODE_1)
			vsp_cycles = vsp_cycles * 3;

		vsp_cycles += mbs_per_second * base_cycles;

	} else if (inst->session_type == MSM_VIDC_DECODER) {
		/* VPP */
		vpp_cycles = mbs_per_second * inst->clk_data.entry->vpp_cycles /
				inst->clk_data.work_route;
		/* 21 / 20 is minimum overhead factor */
		vpp_cycles += max(vpp_cycles / 20, fw_vpp_cycles);
		/* 1.059 is multi-pipe overhead */
		if (inst->clk_data.work_route > 1)
			vpp_cycles += div_u64(vpp_cycles * 59, 1000);

		/* VSP */
		codec = get_v4l2_codec(inst);
		base_cycles = inst->has_bframe ?
				80 : inst->clk_data.entry->vsp_cycles;
		bitrate = fps * filled_len * 8;
		vsp_cycles = bitrate;

		if (codec == V4L2_PIX_FMT_VP9) {
			vsp_cycles = div_u64(vsp_cycles * 170, 100);
		} else if (inst->entropy_mode == HFI_H264_ENTROPY_CABAC) {
			vsp_cycles = div_u64(vsp_cycles * 135, 100);
		} else {
			base_cycles = 0;
			vsp_cycles = div_u64(vsp_cycles, 2);
		}
		/* VSP FW Overhead 1.05 */
		vsp_cycles = div_u64(vsp_cycles * 21, 20);

		if (inst->clk_data.work_mode == HFI_WORKMODE_1)
			vsp_cycles = vsp_cycles * 3;

		vsp_cycles += mbs_per_second * base_cycles;

		if (codec == V4L2_PIX_FMT_VP9 &&
		    inst->clk_data.work_mode == HFI_WORKMODE_2 &&
		    inst->clk_data.work_route == 4 &&
			bitrate > 90000000)
			vsp_cycles = msm_vidc_max_freq(inst->core, inst->sid);
	} else {
		s_vpr_e(inst->sid, "%s: Unknown session type\n", __func__);
		return msm_vidc_max_freq(inst->core, inst->sid);
	}

	freq = max(vpp_cycles, vsp_cycles);
	freq = max(freq, fw_cycles);

	s_vpr_p(inst->sid, "%s: inst %pK: filled len %d required freq %llu\n",
		__func__, inst, filled_len, freq);

	return (unsigned long) freq;
}

int msm_vidc_set_clocks(struct msm_vidc_core *core, u32 sid)
{
	struct hfi_device *hdev;
	unsigned long freq_core_1 = 0, freq_core_2 = 0, rate = 0;
	unsigned long freq_core_max = 0;
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_buffer *temp, *next;
	u32 device_addr, filled_len;
	int rc = 0, i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	bool increment, decrement;
	u64 curr_time_ns;

	hdev = core->device;
	curr_time_ns = ktime_get_ns();
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (!allowed_clks_tbl) {
		s_vpr_e(sid, "%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	increment = false;
	decrement = true;
	list_for_each_entry(inst, &core->instances, list) {
		device_addr = 0;
		filled_len = 0;
		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry_safe(temp, next,
				&inst->registeredbufs.list, list) {
			if (temp->vvb.vb2_buf.type == INPUT_MPLANE) {
				filled_len = max(filled_len,
					temp->vvb.vb2_buf.planes[0].bytesused);
				device_addr = temp->smem[0].device_addr;
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);

		if (!filled_len || !device_addr) {
			s_vpr_l(sid, "%s: no input\n", __func__);
			continue;
		}

		/* skip inactive session clock rate */
		if (!is_active_session(inst->last_qbuf_time_ns, curr_time_ns)) {
			inst->active = false;
			continue;
		}

		if (inst->clk_data.core_id == VIDC_CORE_ID_1)
			freq_core_1 += inst->clk_data.min_freq;
		else if (inst->clk_data.core_id == VIDC_CORE_ID_2)
			freq_core_2 += inst->clk_data.min_freq;
		else if (inst->clk_data.core_id == VIDC_CORE_ID_3) {
			freq_core_1 += inst->clk_data.min_freq;
			freq_core_2 += inst->clk_data.min_freq;
		}

		freq_core_max = max_t(unsigned long, freq_core_1, freq_core_2);

		if (msm_vidc_clock_voting) {
			s_vpr_l(sid, "msm_vidc_clock_voting %d\n",
				 msm_vidc_clock_voting);
			freq_core_max = msm_vidc_clock_voting;
			decrement = false;
			break;
		}

		/* increment even if one session requested for it */
		if (inst->clk_data.dcvs_flags & MSM_VIDC_DCVS_INCR)
			increment = true;
		/* decrement only if all sessions requested for it */
		if (!(inst->clk_data.dcvs_flags & MSM_VIDC_DCVS_DECR))
			decrement = false;
	}

	/*
	 * keep checking from lowest to highest rate until
	 * table rate >= requested rate
	 */
	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq_core_max)
			break;
	}

	if (i < 0)
		i = 0;

	if (increment) {
		if (i > 0)
			rate = allowed_clks_tbl[i-1].clock_rate;
	} else if (decrement) {
		if (i < (int) (core->resources.allowed_clks_tbl_size - 1))
			rate = allowed_clks_tbl[i+1].clock_rate;
	}

	core->min_freq = freq_core_max;
	core->curr_freq = rate;
	mutex_unlock(&core->lock);

	s_vpr_p(sid,
		"%s: clock rate %lu requested %lu increment %d decrement %d\n",
		__func__, core->curr_freq, core->min_freq,
		increment, decrement);
	rc = call_hfi_op(hdev, scale_clocks,
			hdev->hfi_device_data, core->curr_freq, sid);

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
		d_vpr_e("%s: Invalid args: Inst = %pK\n", __func__, inst);
		return -EINVAL;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(temp, next, &inst->registeredbufs.list, list) {
		if (temp->vvb.vb2_buf.type == INPUT_MPLANE) {
			filled_len = max(filled_len,
				temp->vvb.vb2_buf.planes[0].bytesused);
			if (temp->vvb.flags & V4L2_BUF_FLAG_PERF_MODE)
				is_turbo = true;
			device_addr = temp->smem[0].device_addr;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	if (!filled_len || !device_addr) {
		s_vpr_l(inst->sid, "%s: no input\n", __func__);
		return 0;
	}

	if (inst->clk_data.buffer_counter < DCVS_FTB_WINDOW || is_turbo ||
		is_turbo_session(inst)) {
		inst->clk_data.min_freq =
				msm_vidc_max_freq(inst->core, inst->sid);
		inst->clk_data.dcvs_flags = 0;
	} else if (msm_vidc_clock_voting) {
		inst->clk_data.min_freq = msm_vidc_clock_voting;
		inst->clk_data.dcvs_flags = 0;
	} else {
		freq = call_core_op(inst->core, calc_freq, inst, filled_len);
		inst->clk_data.min_freq = freq;
		msm_dcvs_scale_clocks(inst, freq);
	}

	msm_vidc_set_clocks(inst->core, inst->sid);

	return 0;
}

int msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst, bool do_bw_calc)
{
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	core = inst->core;
	hdev = core->device;

	if (!inst->active) {
		/* do not skip bw voting for inactive -> active session */
		do_bw_calc = true;
		inst->active = true;
	}

	if (msm_comm_scale_clocks(inst)) {
		s_vpr_e(inst->sid,
			"Failed to scale clocks. May impact performance\n");
	}

	if (do_bw_calc) {
		if (msm_comm_vote_bus(inst)) {
			s_vpr_e(inst->sid,
				"Failed to scale DDR bus. May impact perf\n");
		}
	}

	return 0;
}

int msm_dcvs_try_enable(struct msm_vidc_inst *inst)
{
	if (!inst || !inst->core) {
		d_vpr_e("%s: Invalid args: %pK\n", __func__, inst);
		return -EINVAL;
	}

	inst->clk_data.dcvs_mode =
		!(msm_vidc_clock_voting ||
			!inst->core->resources.dcvs ||
			inst->flags & VIDC_THUMBNAIL ||
			is_low_latency_hint(inst) ||
			inst->clk_data.low_latency_mode ||
			inst->batch.enable ||
			is_turbo_session(inst) ||
			inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ ||
			is_encode_batching(inst));

	s_vpr_hp(inst->sid, "DCVS %s: %pK\n",
		inst->clk_data.dcvs_mode ? "enabled" : "disabled", inst);

	return 0;
}

void msm_dcvs_reset(struct msm_vidc_inst *inst)
{
	struct msm_vidc_format *fmt;
	struct clock_data *dcvs;

	if (!inst) {
		d_vpr_e("%s: Invalid params\n", __func__);
		return;
	}

	dcvs = &inst->clk_data;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		fmt = &inst->fmts[INPUT_PORT];
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		fmt = &inst->fmts[OUTPUT_PORT];
	} else {
		s_vpr_e(inst->sid, "%s: invalid session type %#x\n",
			__func__, inst->session_type);
		return;
	}

	dcvs->min_threshold = fmt->count_min;
	if (inst->session_type == MSM_VIDC_ENCODER)
		dcvs->max_threshold =
		min((fmt->count_min + DCVS_ENC_EXTRA_INPUT_BUFFERS),
		fmt->count_actual);
	else
		dcvs->max_threshold =
		min((fmt->count_min + DCVS_DEC_EXTRA_OUTPUT_BUFFERS),
		fmt->count_actual);

	dcvs->dcvs_window =
		dcvs->max_threshold < dcvs->min_threshold ? 0 :
		dcvs->max_threshold - dcvs->min_threshold;
	dcvs->nom_threshold = dcvs->min_threshold +
		(dcvs->dcvs_window ?
		(dcvs->dcvs_window / 2) : 0);

	dcvs->dcvs_flags = 0;

	s_vpr_p(inst->sid, "DCVS: Th[%d %d %d] Flag %#x\n",
		dcvs->min_threshold,
		dcvs->nom_threshold, dcvs->max_threshold,
		dcvs->dcvs_flags);

}

int msm_comm_init_clocks_and_bus_data(struct msm_vidc_inst *inst)
{
	int rc = 0, j = 0;
	int fourcc, count;

	if (!inst || !inst->core) {
		d_vpr_e("%s: Invalid args: Inst = %pK\n",
				__func__, inst);
		return -EINVAL;
	}

	count = inst->core->resources.codec_data_count;
	fourcc = get_v4l2_codec(inst);

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

	if (!inst->clk_data.entry) {
		s_vpr_e(inst->sid, "%s: No match found\n", __func__);
		rc = -EINVAL;
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

	if (!inst || !inst->core || !inst->clk_data.entry) {
		d_vpr_e("%s: Invalid args: Inst = %pK\n",
			__func__, inst);
		return;
	}
	s_vpr_h(inst->sid, "Init DCVS Load\n");

	core = inst->core;
	load = msm_comm_get_inst_load_per_core(inst, LOAD_POWER);
	cycles = inst->clk_data.entry->vpp_cycles;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (inst->session_type == MSM_VIDC_ENCODER &&
		inst->flags & VIDC_LOW_POWER)
		cycles = inst->clk_data.entry->low_power_cycles;

	msm_dcvs_reset(inst);

	total_freq = cycles * load;

	for (i = core->resources.allowed_clks_tbl_size - 1; i >= 0; i--) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= total_freq)
			break;
	}

	if (i < 0)
		i = 0;

	inst->clk_data.buffer_counter = 0;
	inst->ubwc_stats.is_valid = 0;

	rc = msm_comm_scale_clocks_and_bus(inst, 1);

	if (rc)
		s_vpr_e(inst->sid, "%s: Failed to scale Clocks and Bus\n",
			__func__);
}

int msm_vidc_decide_work_route_iris2(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_video_work_route pdata;
	bool is_legacy_cbr;
	u32 codec;
	uint32_t vpu;

	if (!inst || !inst->core || !inst->core->device ||
		!inst->core->platform_data) {
		d_vpr_e("%s: Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	vpu = inst->core->platform_data->vpu_ver;
	hdev = inst->core->device;
	is_legacy_cbr = inst->clk_data.is_legacy_cbr;
	pdata.video_work_route = inst->core->platform_data->num_vpp_pipes;

	if (vpu == VPU_VERSION_IRIS2_1) {
		pdata.video_work_route = 1;
		goto decision_done;
	}
	codec  = get_v4l2_codec(inst);
	if (inst->session_type == MSM_VIDC_DECODER) {
		if (codec == V4L2_PIX_FMT_MPEG2 ||
			inst->pic_struct != MSM_VIDC_PIC_STRUCT_PROGRESSIVE)
			pdata.video_work_route = 1;
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		u32 slice_mode, width, height;
		struct v4l2_format *f;

		slice_mode =  msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
		f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		height = f->fmt.pix_mp.height;
		width = f->fmt.pix_mp.width;

		if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES ||
			is_legacy_cbr) {
			pdata.video_work_route = 1;
		}
	} else {
		return -EINVAL;
	}

decision_done:
	s_vpr_h(inst->sid, "Configurng work route = %u",
			pdata.video_work_route);

	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HFI_PROPERTY_PARAM_WORK_ROUTE,
			(void *)&pdata, sizeof(pdata));
	if (rc)
		s_vpr_e(inst->sid, "Failed to configure work route\n");
	else
		inst->clk_data.work_route = pdata.video_work_route;

	return rc;
}

static int msm_vidc_decide_work_mode_ar50_lt(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_video_work_mode pdata;
	struct hfi_enable latency;
	struct v4l2_format *f;

	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	latency.enable = false;
	hdev = inst->core->device;
	if (inst->clk_data.low_latency_mode) {
		pdata.video_work_mode = HFI_WORKMODE_1;
		latency.enable = true;
		goto decision_done;
	}

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	if (inst->session_type == MSM_VIDC_DECODER) {
		pdata.video_work_mode = HFI_WORKMODE_2;
		switch (f->fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_MPEG2:
			pdata.video_work_mode = HFI_WORKMODE_1;
			break;
		case V4L2_PIX_FMT_H264:
		case V4L2_PIX_FMT_HEVC:
			if (f->fmt.pix_mp.height *
				f->fmt.pix_mp.width <= 1280 * 720)
				pdata.video_work_mode = HFI_WORKMODE_1;
			break;
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		pdata.video_work_mode = HFI_WORKMODE_1;
		/* For WORK_MODE_1, set Low Latency mode by default */
		latency.enable = true;
		if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR ||
				inst->rc_type ==
					V4L2_MPEG_VIDEO_BITRATE_MODE_MBR ||
				inst->rc_type ==
					V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR ||
				inst->rc_type ==
					V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) {
			pdata.video_work_mode = HFI_WORKMODE_2;
			latency.enable = false;
		}
	} else {
		return -EINVAL;
	}

decision_done:
	s_vpr_h(inst->sid, "Configuring work mode = %u low latency = %u",
			pdata.video_work_mode, latency.enable);
	inst->clk_data.work_mode = pdata.video_work_mode;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HFI_PROPERTY_PARAM_WORK_MODE,
			(void *)&pdata, sizeof(pdata));
	if (rc)
		s_vpr_e(inst->sid, "Failed to configure Work Mode\n");

	if (inst->session_type == MSM_VIDC_ENCODER && latency.enable) {
		rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session,
			HFI_PROPERTY_PARAM_VENC_LOW_LATENCY_MODE,
			(void *)&latency, sizeof(latency));
	}
	rc = msm_comm_scale_clocks_and_bus(inst, 1);

	return rc;
}

int msm_vidc_set_bse_vpp_delay(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	u32 delay = DEFAULT_BSE_VPP_DELAY;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	/* Set VPP delay only upto first reconfig */
	if (inst->first_reconfig_done) {
		s_vpr_hp(inst->sid, "%s: Skip bse-vpp\n", __func__);
		return 0;
	}

	if (in_port_reconfig(inst))
		inst->first_reconfig_done = 1;

	if (!inst->core->resources.has_vpp_delay ||
		!is_decode_session(inst) ||
		is_thumbnail_session(inst) ||
		is_heif_decoder(inst) ||
		inst->clk_data.work_mode != HFI_WORKMODE_2) {
		s_vpr_hp(inst->sid, "%s: Skip bse-vpp\n", __func__);
		return 0;
	}

	hdev = inst->core->device;

	if (is_vpp_delay_allowed(inst))
		delay = MAX_BSE_VPP_DELAY;

	/* DebugFS override [1-31] */
	if (msm_vidc_vpp_delay & 0x1F)
		delay = msm_vidc_vpp_delay & 0x1F;

	s_vpr_hp(inst->sid, "%s: bse-vpp delay %u\n", __func__, delay);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_VSP_VPP_DELAY, &delay,
		sizeof(u32));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
	else
		inst->bse_vpp_delay = delay;

	return rc;
}

int msm_vidc_decide_work_mode_iris2(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_video_work_mode pdata;
	struct hfi_enable latency;
	u32 width, height;
	bool res_ok = false;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;

	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;
	pdata.video_work_mode = HFI_WORKMODE_2;
	latency.enable = inst->clk_data.low_latency_mode;
	out_f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	inp_f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	if (inst->session_type == MSM_VIDC_DECODER) {
		height = out_f->fmt.pix_mp.height;
		width = out_f->fmt.pix_mp.width;
		res_ok = res_is_less_than_or_equal_to(width, height, 1280, 720);
		if (inp_f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_MPEG2 ||
			inst->pic_struct != MSM_VIDC_PIC_STRUCT_PROGRESSIVE ||
			inst->clk_data.low_latency_mode || res_ok) {
			pdata.video_work_mode = HFI_WORKMODE_1;
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		height = inp_f->fmt.pix_mp.height;
		width = inp_f->fmt.pix_mp.width;
		res_ok = !res_is_greater_than(width, height, 4096, 2160);
		if (res_ok &&
			(inst->clk_data.low_latency_mode)) {
			pdata.video_work_mode = HFI_WORKMODE_1;
			/* For WORK_MODE_1, set Low Latency mode by default */
			latency.enable = true;
		}
		if (inst->rc_type == RATE_CONTROL_LOSSLESS || inst->all_intra) {
			pdata.video_work_mode = HFI_WORKMODE_2;
			latency.enable = false;
		}
	} else {
		return -EINVAL;
	}

	s_vpr_h(inst->sid, "Configuring work mode = %u low latency = %u",
			pdata.video_work_mode, latency.enable);

	if (inst->session_type == MSM_VIDC_ENCODER) {
		rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session,
			HFI_PROPERTY_PARAM_VENC_LOW_LATENCY_MODE,
			(void *)&latency, sizeof(latency));
		if (rc)
			s_vpr_e(inst->sid, "Failed to configure low latency\n");
		else
			inst->clk_data.low_latency_mode = latency.enable;
	}

	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, HFI_PROPERTY_PARAM_WORK_MODE,
			(void *)&pdata, sizeof(pdata));
	if (rc)
		s_vpr_e(inst->sid, "Failed to configure Work Mode\n");
	else
		inst->clk_data.work_mode = pdata.video_work_mode;

	return rc;
}

static inline int msm_vidc_power_save_mode_enable(struct msm_vidc_inst *inst,
	bool enable)
{
	u32 rc = 0;
	u32 prop_id = 0;
	void *pdata = NULL;
	struct hfi_device *hdev = NULL;
	u32 hfi_perf_mode;
	struct v4l2_ctrl *ctrl;

	hdev = inst->core->device;
	if (inst->session_type != MSM_VIDC_ENCODER) {
		s_vpr_l(inst->sid,
			"%s: Not an encoder session. Nothing to do\n",
			__func__);
		return 0;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_COMPLEXITY);
	if (!is_realtime_session(inst) && !ctrl->val)
		enable = true;
	prop_id = HFI_PROPERTY_CONFIG_VENC_PERF_MODE;
	hfi_perf_mode = enable ? HFI_VENC_PERFMODE_POWER_SAVE :
		HFI_VENC_PERFMODE_MAX_QUALITY;
	pdata = &hfi_perf_mode;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, prop_id, pdata,
			sizeof(hfi_perf_mode));
	if (rc) {
		s_vpr_e(inst->sid, "%s: Failed to set power save mode\n",
			__func__);
		return rc;
	}
	inst->flags = enable ?
		inst->flags | VIDC_LOW_POWER :
		inst->flags & ~VIDC_LOW_POWER;

	s_vpr_h(inst->sid,
		"Power Save Mode for inst: %pK Enable = %d\n", inst, enable);

	return rc;
}

int msm_vidc_decide_core_and_power_mode_ar50lt(struct msm_vidc_inst *inst)
{
	inst->clk_data.core_id = VIDC_CORE_ID_1;
	return 0;
}

int msm_vidc_decide_core_and_power_mode_iris2(struct msm_vidc_inst *inst)
{
	u32 mbpf, mbps, max_hq_mbpf, max_hq_mbps;
	bool enable = true;
	int rc = 0;

	inst->clk_data.core_id = VIDC_CORE_ID_1;

	mbpf = msm_vidc_get_mbs_per_frame(inst);
	mbps = mbpf * msm_vidc_get_fps(inst);
	max_hq_mbpf = inst->core->resources.max_hq_mbs_per_frame;
	max_hq_mbps = inst->core->resources.max_hq_mbs_per_sec;

	/* Power saving always disabled for CQ and LOSSLESS RC modes. */
	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ ||
		inst->rc_type == RATE_CONTROL_LOSSLESS ||
		(mbpf <= max_hq_mbpf && mbps <= max_hq_mbps))
		enable = false;

	rc = msm_vidc_power_save_mode_enable(inst, enable);
	msm_print_core_status(inst->core, VIDC_CORE_ID_1, inst->sid);

	return rc;
}

void msm_vidc_init_core_clk_ops(struct msm_vidc_core *core)
{
	uint32_t vpu;

	if (!core)
		return;

	vpu = core->platform_data->vpu_ver;

	if (vpu == VPU_VERSION_AR50_LITE)
		core->core_ops = &core_ops_ar50_lt;
	else
		core->core_ops = &core_ops_iris2;
}

void msm_print_core_status(struct msm_vidc_core *core, u32 core_id, u32 sid)
{
	struct msm_vidc_inst *inst = NULL;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;

	s_vpr_p(sid, "Instances running on core %u", core_id);
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if ((inst->clk_data.core_id != core_id) &&
			(inst->clk_data.core_id != VIDC_CORE_ID_3))
			continue;
		out_f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		inp_f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		s_vpr_p(sid,
			"inst %pK (%4ux%4u) to (%4ux%4u) %3u %s %s %u %s %s %lu\n",
			inst,
			inp_f->fmt.pix_mp.width,
			inp_f->fmt.pix_mp.height,
			out_f->fmt.pix_mp.width,
			out_f->fmt.pix_mp.height,
			inst->clk_data.frame_rate >> 16,
			inst->session_type == MSM_VIDC_ENCODER ? "ENC" : "DEC",
			inst->clk_data.work_mode == HFI_WORKMODE_1 ?
				"WORK_MODE_1" : "WORK_MODE_2",
			inst->clk_data.work_route,
			inst->flags & VIDC_LOW_POWER ? "LP" : "HQ",
			is_realtime_session(inst) ? "RealTime" : "NonRTime",
			inst->clk_data.min_freq);
	}
	mutex_unlock(&core->lock);
}
