/*
 * Copyright (C) 2015, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created on 22 Jan 2015
 * Author: Durgadoss R <durgadoss.r@intel.com>
 */

#define pr_fmt(fmt) "vlv_psr: " fmt

#include <linux/pci.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_sp_plane.h>
#include <core/common/dp/gen_dp_pipe.h>

/* CHV PSR related functions */
static bool vlv_psr_match_conditions(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;
	struct vlv_pipe *pipe = &disp->pipe;
	int i;
	u32 pipe_plane_stat = disp->config->status.pipe_plane_status;

	psr->source_ok = false;

	if (disp->type != INTEL_PIPE_EDP) {
		pr_info("PSR only for eDP\n");
		return false;
	}

	/*
	 * Bits[31:29] in pipe_plane_status indicate active pipes.
	 * For PSR, only one pipe should be active. In CHV, eDP
	 * can only be connected on pipe A/B. That leaves us with
	 *
	 *	Bits:	31	30	29
	 *	Pipes:	A	B	C
	 *		-----------------
	 *	Values:	0	0	1
	 *		0	1	0
	 *		1	0	0
	 *		-----------------
	 * remaining values (of all possible 2^3 values) 2 and 4.
	 * So, we check if the value obtained from Bits[31:29]
	 * is 2 or 4. Otherwise, return false saying more than
	 * one pipe is active.
	 */
	pipe_plane_stat = (pipe_plane_stat >> 29) & 0x7;
	if (pipe_plane_stat != 2 && pipe_plane_stat != 4) {
		pr_info("More than one pipe active:%x\n", pipe_plane_stat);
		return false;
	}

	/* Enable PSR only when primary plane is active */
	for (i = 0; i < VLV_NUM_SPRITES; i++) {
		if (vlv_sp_plane_is_enabled(&disp->splane[i])) {
			pr_info("Pipe %d Sprite enabled\n", pipe->pipe_id);
			return false;
		}
	}

	psr->source_ok = true;
	return true;
}

static bool intel_vlv_psr_active(struct vlv_pipe *pipe, bool in_progress)
{
	u32 val = REG_READ(pipe->psr_sts_offset) & VLV_PSR_STATE_MASK;

	/*
	 * We treat 'PSR active' as three different cases:
	 * i. When we are enabling PSR, if it is in transition
	 *    we consider that as 'active'. Because, we do not
	 *    want to re-enable, since it was already enabled.
	 * ii.When we are disabling PSR, we would like to
	 *    disable even when we are 'Transitioning to
	 *    active'. (We may be disabling pipe etc..)
	 * iii.When we are just 'exiting' PSR (to be re-enabled
	 *    soon, without changes in pipe configurations),
	 *    we return false since we have to gracefully exit
	 *    (which means going to state 3 and then exiting)
	 */
	if (val == VLV_PSR_TRANSIT_TO_ACTIVE) {
		if (in_progress)
			pr_info("PSR Transition in Progress\n");

		return in_progress;
	}

	return val == VLV_PSR_ACTIVE_NO_RFB || val == VLV_PSR_ACTIVE_SFU;
}

static bool intel_vlv_psr_exited(struct vlv_pipe *pipe)
{
	u32 val = REG_READ(pipe->psr_sts_offset) & VLV_PSR_STATE_MASK;

	return val == VLV_PSR_EXIT || val == VLV_PSR_INACTIVE;
}

static void vlv_psr_enable_sink(struct intel_pipeline *pipeline)
{
	int ret;
	u8 buf[1];
	struct dp_aux_msg msg = {0};

	msg.address = DP_PSR_EN_CFG;
	msg.size = 1;
	msg.buffer = &buf[0];

	/* Enable PSR in Sink */
	msg.request = DP_AUX_NATIVE_READ;
	ret = vlv_aux_transfer(pipeline, &msg);
	if (ret < 0) {
		pr_err("PSR dpcd read failed:%d\n", ret);
		return;
	}

	buf[0] |= DP_PSR_ENABLE | DP_PSR_MAIN_LINK_ACTIVE;
	msg.request = DP_AUX_NATIVE_WRITE;
	vlv_aux_transfer(pipeline, &msg);
}

static void vlv_psr_enable_source(struct vlv_pipe *pipe)
{
	int reg, val, mask;

	/* Enable PSR interrupts */
	reg = (pipe->pipe_id == PIPE_A) ? VLV_DPFLIPSTAT : pipe->status_offset;
	mask = (pipe->pipe_id == PIPE_A) ? PIPE_PSR_INT_EN :
				PIPE_B_PSR_INTERRUPT_ENABLE_VLV;
	REG_WRITE(reg, REG_READ(reg) | mask);

	/* Disable Clock gating */
	REG_WRITE(VLV_PSR_CLK_GATE, CLK_GATE_DISABLE_ALL);

	/* Set frequency to send SDP frame */
	val = REG_READ(pipe->psr_vsc_sdp_offset);
	val &= ~VLV_PSR_SDP_SEND_FREQ_MASK;
	val |= VLV_PSR_SDP_SEND_EVFRAME;
	REG_WRITE(pipe->psr_vsc_sdp_offset, val);

	val = REG_READ(pipe->psr_ctrl_offset);

	/* Set hardware mode */
	val = (val & ~VLV_PSR_MODE_MASK) |
			(VLV_PSR_HW_MODE << VLV_PSR_MODE_SHIFT);
	/* Set idle frame threshold */
	val = (val & ~VLV_PSR_IDENTICAL_FRAMES_MASK) |
		(VLV_PSR_IDENTICAL_FRAMES << VLV_PSR_IDENTICAL_FRAMES_SHIFT);

	/* Set source transmitter to active */
	val = val | VLV_PSR_SRC_TRANSMITTER_STATE;

	/* Enable PSR in source */
	val = val | VLV_PSR_ENABLE;

	REG_WRITE(pipe->psr_ctrl_offset, val);
}

static void vlv_psr_do_enable(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;
	struct vlv_pipe *pipe = &disp->pipe;

	if (psr->enabled)
		return;

	if (!vlv_psr_match_conditions(pipeline))
		return;

	/* Need to check for state 2 */
	if (intel_vlv_psr_active(pipe, true))
		return;

	psr->enabled = dp_pipe;

	vlv_psr_enable_sink(pipeline);

	vlv_psr_enable_source(pipe);
}

static bool vlv_psr_do_exit(struct intel_pipeline *pipeline, bool disable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;
	struct vlv_dp_port *dp_port = &disp->port.dp_port;
	struct vlv_pll *pll = &disp->pll;
	struct vlv_pipe *pipe = &disp->pipe;
	struct dp_panel *panel = &dp_pipe->panel;
	int reg = pipe->psr_ctrl_offset;
	int val, count = 0;
	bool ret = false;
	bool main_link_on = false;

	if (!psr->enabled)
		return false;

	if (!intel_vlv_psr_active(pipe, disable))
		return false;

	if (REG_READ(pipe->psr_sts_offset) & VLV_PSR_IN_TRANSITION) {
		pr_info("Waiting for Sink to commit to PSR\n");
		udelay(250);
	}

	/*
	 * Source might have just entered into PSR Active state (3).
	 * Sink needs a setup time of 330us to enter into a valid
	 * PSR state (according to eDP specv1.3). And we can do a
	 * graceful exit only when Sink is in a valid PSR state.
	 * So, provide that delay if required. If we do not,
	 * we will see a failure in the link training below.
	 *
	 * Experiments show that a 330 us delay is not sufficient
	 * here. So, make it to 1 ms.
	 */
	if (jiffies_to_usecs(jiffies - psr->entry_ts) < 1000)
		udelay(VLV_PSR_ENABLE_DELAY);

	/* Check the main link status */
	val = REG_READ(reg);
	main_link_on = val & VLV_PSR_SRC_TRANSMITTER_STATE;

	/* Set idle pattern */
	if (!main_link_on) {
		vlv_dp_port_set_link_pattern(dp_port,
					DP_PORT_IDLE_PATTERN_SET);
	}

	/* Disable PSR by clearing the active entry bit and changing mode */
	val = REG_READ(reg);
	val &= ~VLV_PSR_ACTIVE_ENTRY;
	val &= ~VLV_PSR_ENABLE;
	val &= ~VLV_PSR_MODE_MASK;
	REG_WRITE(reg, val);

	psr->enabled = NULL;

	/* Wait for 40 ms max to exit PSR state */
	while ((count < 400) && !intel_vlv_psr_exited(pipe)) {
		udelay(100);
		count++;
	}

	/* Use Reset bit to force exit out of PSR */
	if (count == 400) {
		pr_err("Error waiting to exit PSR. Resetting\n");
		val = REG_READ(reg);
		REG_WRITE(reg, val | VLV_PSR_RESET);
	}

	/* We dont need to train link if it's already on */
	if (main_link_on)
		goto done;

	/* CHV HW requires a wait of 60us before and after enabling port */
	udelay(60);

	/* Enable Port */
	vlv_dp_port_enable(dp_port, dp_pipe->current_mode.flags,
			&pipeline->params);

	/* Wait for PLL to get Locked */
	if (wait_for(((REG_READ(pll->offset) & DPLL_LOCK_VLV)
			== DPLL_LOCK_VLV), 4))
		pr_err("DPLL %d failed to lock\n", pipe->pipe_id);

	/* Wait for DPIO PHY status to be up */
	vlv_pll_wait_for_port_ready(dp_port->port_id);
	udelay(60);

	/* train_link() method takes care of waking up sink */
	ret = dp_panel_train_link(panel, &dp_pipe->link_params);
	if (!ret)
		pr_err("eDP Link training failed:%d\n", ret);

done:
	cancel_delayed_work(&psr->work);
	return true;
}

void vlv_psr_irq_handler(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;

	/*
	 * Cache the timestamp of PSR entry. This helps us to
	 * allow setup time when we do frequent PSR entry/exits.
	 */
	psr->entry_ts = jiffies;
	pr_info("PSR state: Active\n");
}

static bool is_psr_supported(struct dp_panel *panel)
{
	return panel->psr_dpcd[0] & DP_PSR_IS_SUPPORTED;
}

void vlv_edp_psr_update(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct dp_panel *panel = &dp_pipe->panel;
	struct edp_psr *psr = &dp_pipe->psr;
	bool flag;

	if (disp->type != INTEL_PIPE_EDP)
		return;

	if (!psr->setup_done)
		return;

	if (!is_psr_supported(panel)) {
		pr_debug("eDP Panel does not support PSR\n");
		return;
	}

	mutex_lock(&psr->lock);

	/*
	 * We mostly land here from on_post function which means
	 * means we just finished a page flip. PSR should have
	 * been disabled for that flip. If not, (may be it was
	 * busy entering) disable it here.
	 */
	if (psr->enabled)
		vlv_psr_do_exit(pipeline, false);

	flag = vlv_psr_match_conditions(pipeline);
	if (!psr->enabled && flag)
		schedule_delayed_work(&psr->work,
			msecs_to_jiffies(VLV_PSR_ENABLE_DELAY));

	mutex_unlock(&psr->lock);
}

void vlv_edp_psr_exit(struct intel_pipeline *pipeline, bool disable)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;

	if (!psr->setup_done)
		return;

	mutex_lock(&psr->lock);

	if (!psr->enabled) {
		mutex_unlock(&psr->lock);
		return;
	}

	vlv_psr_do_exit(pipeline, disable);

	mutex_unlock(&psr->lock);
}

void vlv_edp_psr_disable(struct intel_pipeline *pipeline)
{
	vlv_edp_psr_exit(pipeline, true);
}

static void psr_work_execute(struct work_struct *work)
{
	struct edp_psr *psr =
		container_of(work, struct edp_psr, work.work);

	mutex_lock(&psr->lock);

	/* If not initialized, exit. Should never really happen */
	if (!psr->setup_done) {
		pr_debug("PSR setup not done, but trying to enable\n");
		goto unlock;
	}

	vlv_psr_do_enable(psr->pipeline);

	/*
	 * There might be other workers trying to enable.
	 * Now that we have enabled, cancel them.
	 */
	cancel_delayed_work(&psr->work);
unlock:
	mutex_unlock(&psr->lock);
}

void vlv_edp_psr_init(struct intel_pipeline *pipeline)
{
	struct vlv_pipeline *disp = to_vlv_pipeline(pipeline);
	struct dp_pipe *dp_pipe = &disp->gen.dp;
	struct edp_psr *psr = &dp_pipe->psr;

	mutex_init(&psr->lock);

	psr->enabled = NULL;
	psr->pipeline = pipeline;

	INIT_DELAYED_WORK(&psr->work, psr_work_execute);

	psr->setup_done = true;
	pr_info("PSR initialization done\n");
}
