// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_clocks.h"

#define MSM_CVP_MIN_UBWC_COMPLEXITY_FACTOR (1 << 16)
#define MSM_CVP_MAX_UBWC_COMPLEXITY_FACTOR (4 << 16)

#define MSM_CVP_MIN_UBWC_COMPRESSION_RATIO (1 << 16)
#define MSM_CVP_MAX_UBWC_COMPRESSION_RATIO (5 << 16)

static unsigned long msm_cvp_calc_freq(struct msm_cvp_inst *inst,
	u32 filled_len);

struct msm_cvp_core_ops cvp_core_ops_vpu5 = {
	.calc_freq = msm_cvp_calc_freq,
	.decide_work_route = msm_cvp_decide_work_route,
	.decide_work_mode = msm_cvp_decide_work_mode,
};

static inline void msm_dcvs_print_dcvs_stats(struct cvp_clock_data *dcvs)
{
	dprintk(CVP_PROF,
		"DCVS: Load_Low %d, Load Norm %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_norm,
		dcvs->load_high);

	dprintk(CVP_PROF,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold, dcvs->max_threshold);
}

static int msm_cvp_get_fps(struct msm_cvp_inst *inst)
{
	return 0;
}

int msm_cvp_comm_vote_bus(struct msm_cvp_core *core)
{
	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	if (!core->resources.bus_devfreq_on)
		return 0;

	return 0;
}

static int msm_dcvs_scale_clocks(struct msm_cvp_inst *inst,
		unsigned long freq)
{
	int rc = 0;
	int bufs_with_fw = 0;
	int bufs_with_client = 0;
	struct cvp_hal_buffer_requirements *buf_reqs;
	struct cvp_clock_data *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}

	/* assume no increment or decrement is required initially */
	inst->clk_data.dcvs_flags = 0;

	if (!inst->clk_data.dcvs_mode) {
		dprintk(CVP_DBG, "Skip DCVS (dcvs %d)\n",
			inst->clk_data.dcvs_mode);
		/* update load (freq) with normal value */
		inst->clk_data.load = inst->clk_data.load_norm;
		return 0;
	}

	dcvs = &inst->clk_data;
	/* +1 as one buffer is going to be queued after the function */
	bufs_with_fw += 1;

	buf_reqs = get_cvp_buff_req_buffer(inst, dcvs->buffer_type);
	if (!buf_reqs) {
		dprintk(CVP_ERR, "%s: invalid buf type %d\n",
			__func__, dcvs->buffer_type);
		return -EINVAL;
	}
	bufs_with_client = buf_reqs->buffer_count_actual - bufs_with_fw;

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

	if (bufs_with_client <= dcvs->max_threshold) {
		dcvs->load = dcvs->load_high;
		dcvs->dcvs_flags |= MSM_CVP_DCVS_INCR;
	} else if (bufs_with_fw < buf_reqs->buffer_count_min) {
		dcvs->load = dcvs->load_low;
		dcvs->dcvs_flags |= MSM_CVP_DCVS_DECR;
	} else {
		dcvs->load = dcvs->load_norm;
		dcvs->dcvs_flags = 0;
	}

	dprintk(CVP_PROF,
		"DCVS: %x : total bufs %d outside fw %d max threshold %d with fw %d min bufs %d flags %#x\n",
		hash32_ptr(inst->session), buf_reqs->buffer_count_actual,
		bufs_with_client, dcvs->max_threshold, bufs_with_fw,
		buf_reqs->buffer_count_min, dcvs->dcvs_flags);
	return rc;
}

static void msm_cvp_update_freq_entry(struct msm_cvp_inst *inst,
	unsigned long freq, u32 device_addr, bool is_turbo)
{
	struct cvp_freq_data *temp, *next;
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
			dprintk(CVP_WARN, "%s: malloc failure.\n", __func__);
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

static unsigned long msm_cvp_max_freq(struct msm_cvp_core *core)
{
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	unsigned long freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	freq = allowed_clks_tbl[0].clock_rate;
	dprintk(CVP_PROF, "Max rate = %lu\n", freq);
	return freq;
}

void msm_cvp_comm_free_freq_table(struct msm_cvp_inst *inst)
{
	struct cvp_freq_data *temp, *next;

	mutex_lock(&inst->freqs.lock);
	list_for_each_entry_safe(temp, next, &inst->freqs.list, list) {
		list_del(&temp->list);
		kfree(temp);
	}
	INIT_LIST_HEAD(&inst->freqs.list);
	mutex_unlock(&inst->freqs.lock);
}

static unsigned long msm_cvp_calc_freq(struct msm_cvp_inst *inst,
	u32 filled_len)
{
	unsigned long freq = 0;
	unsigned long vpp_cycles = 0, vsp_cycles = 0, fw_cycles = 0;
	u32 mbs_per_second;
	struct msm_cvp_core *core = NULL;
	int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 rate = 0, fps;
	struct cvp_clock_data *dcvs = NULL;

	core = inst->core;
	dcvs = &inst->clk_data;

	mbs_per_second = msm_cvp_comm_get_inst_load_per_core(inst,
		LOAD_CALC_NO_QUIRKS);

	fps = msm_cvp_get_fps(inst);

	dprintk(CVP_ERR, "Unknown session type = %s\n", __func__);
	return msm_cvp_max_freq(inst->core);

	freq = max(vpp_cycles, vsp_cycles);
	freq = max(freq, fw_cycles);

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

	dprintk(CVP_PROF,
		"%s: inst %pK: %x : filled len %d required freq %ld load_norm %u\n",
		__func__, inst, hash32_ptr(inst->session),
		filled_len, freq, dcvs->load_norm);

	return freq;
}

int msm_cvp_set_clocks(struct msm_cvp_core *core)
{
	struct cvp_hfi_device *hdev;
	unsigned long freq_core_1 = 0, freq_core_2 = 0, rate = 0;
	unsigned long freq_core_max = 0;
	struct msm_cvp_inst *temp = NULL;
	int rc = 0, i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	bool increment, decrement;

	hdev = core->device;
	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	if (!allowed_clks_tbl) {
		dprintk(CVP_ERR,
			"%s Invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	increment = false;
	decrement = true;
	list_for_each_entry(temp, &core->instances, list) {

		if (temp->clk_data.core_id == CVP_CORE_ID_1)
			freq_core_1 += temp->clk_data.min_freq;
		else if (temp->clk_data.core_id == CVP_CORE_ID_2)
			freq_core_2 += temp->clk_data.min_freq;
		else if (temp->clk_data.core_id == CVP_CORE_ID_3) {
			freq_core_1 += temp->clk_data.min_freq;
			freq_core_2 += temp->clk_data.min_freq;
		}

		freq_core_max = max_t(unsigned long, freq_core_1, freq_core_2);

		if (msm_cvp_clock_voting) {
			dprintk(CVP_PROF,
				"msm_cvp_clock_voting %d\n",
				 msm_cvp_clock_voting);
			freq_core_max = msm_cvp_clock_voting;
			break;
		}

		if (temp->clk_data.turbo_mode) {
			dprintk(CVP_PROF,
				"Found an instance with Turbo request\n");
			freq_core_max = msm_cvp_max_freq(core);
			break;
		}
		/* increment even if one session requested for it */
		if (temp->clk_data.dcvs_flags & MSM_CVP_DCVS_INCR)
			increment = true;
		/* decrement only if all sessions requested for it */
		if (!(temp->clk_data.dcvs_flags & MSM_CVP_DCVS_DECR))
			decrement = false;
	}

	/*
	 * keep checking from lowest to highest rate until
	 * table rate >= requested rate
	 */
	for (i = 0; i < core->resources.allowed_clks_tbl_size;  i++) {
		rate = allowed_clks_tbl[i].clock_rate;
		if (rate >= freq_core_max)
			break;
	}
	if (increment) {
		if (i > 0)
			rate = allowed_clks_tbl[i-1].clock_rate;
	} else if (decrement) {
		if (i < (core->resources.allowed_clks_tbl_size - 1))
			rate = allowed_clks_tbl[i+1].clock_rate;
	}

	core->min_freq = freq_core_max;
	core->curr_freq = rate;
	mutex_unlock(&core->lock);

	dprintk(CVP_PROF,
		"%s: clock rate %lu requested %lu increment %d decrement %d\n",
		__func__, core->curr_freq, core->min_freq,
		increment, decrement);
	rc = call_hfi_op(hdev, scale_clocks,
			hdev->hfi_device_data, core->curr_freq);

	return rc;
}

int msm_cvp_comm_scale_clocks(struct msm_cvp_inst *inst)
{
	unsigned long freq = 0;
	u32 filled_len = 0;
	u32 device_addr = 0;
	bool is_turbo = false;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s Invalid args: Inst = %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	if (!inst->core->resources.bus_devfreq_on)
		return 0;

	if (!filled_len || !device_addr) {
		dprintk(CVP_DBG, "%s no input for session %x\n",
			__func__, hash32_ptr(inst->session));
		goto no_clock_change;
	}

	freq = call_core_op(inst->core, calc_freq, inst, filled_len);
	inst->clk_data.min_freq = freq;
	/* update dcvs flags */
	msm_dcvs_scale_clocks(inst, freq);

	if (inst->clk_data.buffer_counter < DCVS_FTB_WINDOW || is_turbo ||
		msm_cvp_clock_voting) {
		inst->clk_data.min_freq = msm_cvp_max_freq(inst->core);
		inst->clk_data.dcvs_flags = 0;
	}

	msm_cvp_update_freq_entry(inst, freq, device_addr, is_turbo);

	msm_cvp_set_clocks(inst->core);

no_clock_change:
	return 0;
}

int msm_cvp_comm_scale_clocks_and_bus(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	struct cvp_hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	hdev = core->device;

	if (msm_cvp_comm_scale_clocks(inst)) {
		dprintk(CVP_WARN,
			"Failed to scale clocks. Performance might be impacted\n");
	}
	if (msm_cvp_comm_vote_bus(core)) {
		dprintk(CVP_WARN,
			"Failed to scale DDR bus. Performance might be impacted\n");
	}
	return 0;
}

int msm_cvp_dcvs_try_enable(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "%s: Invalid args: %p\n", __func__, inst);
		return -EINVAL;
	}

	if (msm_cvp_clock_voting ||
			inst->flags & CVP_THUMBNAIL ||
			inst->clk_data.low_latency_mode) {
		dprintk(CVP_PROF, "DCVS disabled: %pK\n", inst);
		inst->clk_data.dcvs_mode = false;
		return false;
	}
	inst->clk_data.dcvs_mode = true;
	dprintk(CVP_PROF, "DCVS enabled: %pK\n", inst);

	return true;
}

int msm_cvp_comm_init_clocks_and_bus_data(struct msm_cvp_inst *inst)
{
	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s Invalid args: Inst = %pK\n",
				__func__, inst);
		return -EINVAL;
	}

	if (inst->session_type == MSM_CVP_CORE) {
		dprintk(CVP_DBG, "%s: cvp session\n", __func__);
		return 0;
	}

	return 0;
}

int msm_cvp_decide_work_route(struct msm_cvp_inst *inst)
{
	return -EINVAL;
}

int msm_cvp_decide_work_mode(struct msm_cvp_inst *inst)
{
	return -EINVAL;
}

void msm_cvp_init_core_clk_ops(struct msm_cvp_core *core)
{
	if (!core)
		return;
	core->core_ops = &cvp_core_ops_vpu5;
}
