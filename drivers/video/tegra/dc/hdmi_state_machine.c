/*
 * hdmi_state_machine.c
 *
 * HDMI library support functions for Nvidia Tegra processors.
 *
 * Copyright (C) 2012-2013 Google - http://www.google.com/
 * Copyright (C) 2013, NVIDIA CORPORATION. All rights reserved.
 * Authors:	John Grossman <johngro@google.com>
 * Authors:	Mike J. Chen <mjchen@google.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <mach/dc.h>
#include <mach/fb.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <video/tegrafb.h>
#include "dc_priv.h"

#include "hdmi_state_machine.h"

/************************************************************
 *
 * state machine internal constants
 *
 ************************************************************/
#define MAX_EDID_READ_ATTEMPTS 5
#define HDMI_EDID_MAX_LENGTH 512

/* how long of an HPD drop before we consider it gone for good.
 * this is mostly a preference to work around monitors users
 * reported that occasionally drop HPD.
 */
#define HPD_DROP_TIMEOUT_MS 1500
#define CHECK_PLUG_STATE_DELAY_MS 10
#define CHECK_EDID_DELAY_MS 60

/************************************************************
 *
 * state machine internal state
 *
 ************************************************************/
static DEFINE_RT_MUTEX(work_lock);
static struct hdmi_state_machine_worker_data {
	struct delayed_work dwork;
	struct tegra_dc_hdmi_data *hdmi;
	int shutdown;
	int state;
	int edid_reads;
	int pending_hpd_evt;
} work_state;

/************************************************************
 *
 * state machine internal methods
 *
 ************************************************************/
static void hdmi_state_machine_sched_work_l(int resched_time)
{
	cancel_delayed_work(&work_state.dwork);
	if ((resched_time >= 0) && !work_state.shutdown)
		queue_delayed_work(system_nrt_wq,
				&work_state.dwork,
				msecs_to_jiffies(resched_time));
}

static const char * const state_names[] = {
	"Reset",
	"Check Plug",
	"Check EDID",
	"Disabled",
	"Enabled",
	"Wait for HPD reassert",
	"Recheck EDID"
};

static void hdmi_state_machine_set_state_l(int target_state, int resched_time)
{
	rt_mutex_lock(&work_lock);

	pr_info("%s: switching from state %d (%s) to state %d (%s)\n",
		__func__, work_state.state, state_names[work_state.state],
		target_state, state_names[target_state]);
	work_state.state = target_state;

	/* If the pending_hpd_evt flag is already set, don't bother to
	 * reschedule the state machine worker.  We should be able to assert
	 * that there is a worker callback already scheduled, and that it is
	 * scheduled to run immediately.  This is particularly important when
	 * making the transition to the steady state ENABLED or DISABLED states.
	 * If an HPD event occurs while the worker is in flight, after the
	 * worker checks the state of the pending HPD flag, and then the state
	 * machine transitions to ENABLE or DISABLED, the system would end up
	 * canceling the callback to handle the HPD event were it not for this
	 * check.
	 */
	if (!work_state.pending_hpd_evt)
		hdmi_state_machine_sched_work_l(resched_time);

	rt_mutex_unlock(&work_lock);
}

static void hdmi_state_machine_handle_hpd_l(int cur_hpd)
{
	int tgt_state, timeout;

	if ((HDMI_STATE_DONE_ENABLED == work_state.state) && !cur_hpd) {
		/* Did HPD drop while we were in DONE_ENABLED?  If so, hold
		 * steady and wait to see if it comes back.
		 */
		tgt_state = HDMI_STATE_DONE_WAIT_FOR_HPD_REASSERT;
		timeout = HPD_DROP_TIMEOUT_MS;
	} else
	if (HDMI_STATE_DONE_WAIT_FOR_HPD_REASSERT == work_state.state &&
		cur_hpd) {
		/* Looks like HPD dropped and eventually came back.  Re-read the
		 * EDID and reset the system only if the EDID has changed.
		 */
		work_state.edid_reads = 0;
		tgt_state = HDMI_STATE_DONE_RECHECK_EDID;
		timeout = CHECK_EDID_DELAY_MS;
	} else {
		/* Looks like there was HPD activity while we were neither
		 * waiting for it to go away during steady state output, nor
		 * looking for it to come back after such an event.  Wait until
		 * HPD has been steady for at least 40 mSec, then restart the
		 * state machine.
		 */
		tgt_state = HDMI_STATE_RESET;
		timeout = 40;
	}

	hdmi_state_machine_set_state_l(tgt_state, timeout);
}

/************************************************************
 *
 * internal state handlers and dispatch table
 *
 ************************************************************/
static void hdmi_disable_l(struct tegra_dc_hdmi_data *hdmi, bool power_gate)
{
#ifdef CONFIG_SWITCH
	switch_set_state(&hdmi->audio_switch, 0);
	pr_info("%s: audio_switch 0\n", __func__);
	switch_set_state(&hdmi->hpd_switch, 0);
	pr_info("%s: hpd_switch 0\n", __func__);
#endif
	tegra_nvhdcp_set_plug(hdmi->nvhdcp, 0);
	if (hdmi->dc->connected) {
		pr_info("HDMI from connected to disconnected\n");
		hdmi->dc->connected = false;
		tegra_dc_disable(hdmi->dc);
		tegra_fb_update_monspecs(hdmi->dc->fb, NULL, NULL);
		tegra_dc_ext_process_hotplug(hdmi->dc->ndev->id);
	}
	if (power_gate && tegra_powergate_is_powered(hdmi->dc->powergate_id))
		tegra_dc_powergate_locked(hdmi->dc);
}

static void handle_reset_l(struct tegra_dc_hdmi_data *hdmi)
{
	/* Were we just reset?  If so, shut everything down, then schedule a
	 * check of the plug state in the near future.
	 */
	hdmi_disable_l(hdmi, false);
	hdmi_state_machine_set_state_l(HDMI_STATE_CHECK_PLUG_STATE,
				       CHECK_PLUG_STATE_DELAY_MS);
}

static void handle_check_plug_state_l(struct tegra_dc_hdmi_data *hdmi)
{
	if (tegra_dc_hpd(work_state.hdmi->dc)) {
		/* Looks like there is something plugged in.
		 * Get ready to read the sink's EDID information.
		 */
		work_state.edid_reads = 0;

		hdmi_state_machine_set_state_l(HDMI_STATE_CHECK_EDID,
					       CHECK_EDID_DELAY_MS);
	} else {
		/* nothing plugged in, so we are finished.  Go to the
		 * DONE_DISABLED state and stay there until the next HPD event.
		 * */
		hdmi_disable_l(hdmi, true);
		hdmi_state_machine_set_state_l(HDMI_STATE_DONE_DISABLED, -1);
	}
}

static void handle_check_edid_l(struct tegra_dc_hdmi_data *hdmi)
{
	struct fb_monspecs specs;
#ifdef CONFIG_SWITCH
	int state;
#endif

	memset(&specs, 0, sizeof(specs));
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	/* Set default videomode on dc before enabling it*/
	tegra_dc_set_default_videomode(hdmi->dc);
#endif

	if (!tegra_dc_hpd(work_state.hdmi->dc)) {
		/* hpd dropped - stop EDID read */
		pr_info("hpd == 0, aborting EDID read\n");
		goto end_disabled;
	}

	if (tegra_edid_get_monspecs(hdmi->edid, &specs)) {
		/* Failed to read EDID.  If we still have retry attempts left,
		 * schedule another attempt.  Otherwise give up and just go to
		 * the disabled state.
		 */
		work_state.edid_reads++;
		if (work_state.edid_reads >= MAX_EDID_READ_ATTEMPTS) {
			pr_info("Failed to read EDID after %d times. Giving up.\n",
				work_state.edid_reads);
			goto end_disabled;
		} else {
			hdmi_state_machine_set_state_l(HDMI_STATE_CHECK_EDID,
						       CHECK_EDID_DELAY_MS);
		}

		return;
	}

	if (tegra_edid_get_eld(hdmi->edid, &hdmi->eld) < 0) {
		pr_err("error populating eld\n");
		goto end_disabled;
	}
	hdmi->eld_retrieved = true;

	pr_info("panel size %d by %d\n", specs.max_x, specs.max_y);

	/* monitors like to lie about these but they are still useful for
	 * detecting aspect ratios
	 */
	hdmi->dc->out->h_size = specs.max_x * 1000;
	hdmi->dc->out->v_size = specs.max_y * 1000;

	hdmi->dvi = !(specs.misc & FB_MISC_HDMI);

	tegra_fb_update_monspecs(hdmi->dc->fb, &specs,
		tegra_dc_hdmi_mode_filter);

#ifdef CONFIG_SWITCH
	state = tegra_edid_audio_supported(hdmi->edid) ? 1 : 0;
	switch_set_state(&hdmi->audio_switch, state);
	pr_info("%s: audio_switch %d\n", __func__, state);
	switch_set_state(&hdmi->hpd_switch, 1);
	pr_info("Display connected, hpd_switch 1\n");
#endif
	hdmi->dc->connected = true;
	tegra_dc_ext_process_hotplug(hdmi->dc->ndev->id);
	hdmi_state_machine_set_state_l(HDMI_STATE_DONE_ENABLED, -1);

	return;

end_disabled:
	hdmi->eld_retrieved = false;
	hdmi_disable_l(hdmi, true);
	hdmi_state_machine_set_state_l(HDMI_STATE_DONE_DISABLED, -1);
}

static void handle_wait_for_hpd_reassert_l(struct tegra_dc_hdmi_data *hdmi)
{
	/* Looks like HPD dropped and really did stay low.  Go ahead and reset
	 * the system.
	 */
	hdmi_state_machine_set_state_l(HDMI_STATE_RESET, 0);
}

/* returns bytes read, or negative error */
static int read_edid_into_buffer(struct tegra_dc_hdmi_data *hdmi,
				 u8 *edid_data, size_t edid_data_len)
{
	int err, i;
	int extension_blocks;
	int max_ext_blocks = (edid_data_len / 128) - 1;

	err = tegra_edid_read_block(hdmi->edid, 0, edid_data);
	if (err) {
		pr_err("tegra_edid_read_block(0) returned err %d\n", err);
		return err;
	}
	extension_blocks = edid_data[0x7e];
	pr_info("%s: extension_blocks = %d, max_ext_blocks = %d\n",
		__func__, extension_blocks, max_ext_blocks);
	if (extension_blocks > max_ext_blocks)
		extension_blocks = max_ext_blocks;
	for (i = 1; i <= extension_blocks; i++) {
		err = tegra_edid_read_block(hdmi->edid, i, edid_data + i * 128);
		if (err) {
			pr_err("tegra_edid_read_block(%d) returned err %d\n",
				i, err);
			return err;
		}
	}
	return i * 128;
}

/* re-read the edid and check to see if it has changed.  Return 0 on a
 * successful E-EDID read, or a non-zero error code on failure.  If we succeed,
 * set match to 1 if the old E-EDID matches the new E-EDID.  Otherwise, set
 * match to 0. */
static int hdmi_recheck_edid(struct tegra_dc_hdmi_data *hdmi, int *match)
{
	int ret;
	u8 tmp[HDMI_EDID_MAX_LENGTH] = {0};

	ret = read_edid_into_buffer(hdmi, tmp, sizeof(tmp));
	pr_info("%s: read_edid_into_buffer() returned %d\n", __func__, ret);
	if (ret > 0) {
		struct tegra_dc_edid *data = tegra_edid_get_data(hdmi->edid);
		pr_info("old edid len = %d\n", data->len);
		*match = ((ret == data->len) &&
			  !memcmp(tmp, data->buf, data->len));
		if (*match == 0) {
			print_hex_dump(KERN_INFO, "tmp :", DUMP_PREFIX_ADDRESS,
				       16, 4, tmp, ret, true);
			print_hex_dump(KERN_INFO, "data:", DUMP_PREFIX_ADDRESS,
				       16, 4, data->buf, data->len, true);
		}
		tegra_edid_put_data(data);
		ret = 0;
	}

	return ret;
}

static void handle_recheck_edid_l(struct tegra_dc_hdmi_data *hdmi)
{
	int match, tgt_state, timeout;

	tgt_state = HDMI_STATE_RESET;
	timeout = 0;

	if (hdmi_recheck_edid(hdmi, &match)) {
		/* Failed to read EDID.  If we still have retry attempts left,
		 * schedule another attempt.  Otherwise give up and reset;
		 */
		work_state.edid_reads++;
		if (work_state.edid_reads >= MAX_EDID_READ_ATTEMPTS) {
			pr_info("Failed to read EDID after %d times. Giving up.\n",
				work_state.edid_reads);
		} else {
			tgt_state = HDMI_STATE_DONE_RECHECK_EDID;
			timeout = CHECK_EDID_DELAY_MS;
		}
	} else {
		/* Successful read!  If the EDID is unchanged, just go back to
		 * the DONE_ENABLED state and do nothing.  If something changed,
		 * just reset the whole system.
		 */
		if (match) {
			pr_info("No EDID change after HPD bounce, taking no action.\n");
			tgt_state = HDMI_STATE_DONE_ENABLED;
			tegra_nvhdcp_set_plug(hdmi->nvhdcp, 0);
			tegra_nvhdcp_set_plug(hdmi->nvhdcp, 1);
			timeout = -1;
		} else {
			pr_info("EDID change after HPD bounce, resetting\n");
		}
	}

	hdmi_state_machine_set_state_l(tgt_state, timeout);
}

typedef void (*dispatch_func_t)(struct tegra_dc_hdmi_data *hdmi);
static const dispatch_func_t state_machine_dispatch[] = {
	handle_reset_l,			/* STATE_RESET */
	handle_check_plug_state_l,	/* STATE_CHECK_PLUG_STATE */
	handle_check_edid_l,		/* STATE_CHECK_EDID */
	NULL,				/* STATE_DONE_DISABLED */
	NULL,				/* STATE_DONE_ENABLED */
	handle_wait_for_hpd_reassert_l,	/* STATE_DONE_WAIT_FOR_HPD_REASSERT */
	handle_recheck_edid_l,		/* STATE_DONE_RECHECK_EDID */
};

/************************************************************
 *
 * main state machine worker function
 *
 ************************************************************/
static void hdmi_state_machine_worker(struct work_struct *work)
{
	int pending_hpd_evt, cur_hpd;

	/* Observe and clear the pending flag and latch the current HPD state.
	 */
	rt_mutex_lock(&work_lock);
	pending_hpd_evt = work_state.pending_hpd_evt;
	work_state.pending_hpd_evt = 0;
	cur_hpd = tegra_dc_hpd(work_state.hdmi->dc);
	rt_mutex_unlock(&work_lock);

	pr_info("%s (tid %p): state %d (%s), hpd %d, pending_hpd_evt %d\n",
		__func__, current, work_state.state,
		state_names[work_state.state], cur_hpd, pending_hpd_evt);

	if (pending_hpd_evt) {
		/* If we were woken up because of HPD activity, just schedule
		 * the next appropriate task and get out.
		 */
		hdmi_state_machine_handle_hpd_l(cur_hpd);
	} else if (work_state.state < ARRAY_SIZE(state_machine_dispatch)) {
		dispatch_func_t func = state_machine_dispatch[work_state.state];

		if (NULL == func)
			pr_warn("NULL state machine handler while in state %d; how did we end up here?",
				work_state.state);
		else
			func(work_state.hdmi);
	} else {
		pr_warn("hdmi state machine worker scheduled unexpected state %d",
			work_state.state);
	}
}

/************************************************************
 *
 * state machine API implementation
 *
 ************************************************************/
void hdmi_state_machine_init(struct tegra_dc_hdmi_data *hdmi)
{
	work_state.hdmi = hdmi;
	work_state.state = HDMI_STATE_RESET;
	work_state.pending_hpd_evt = 1;
	work_state.edid_reads = 0;
	work_state.shutdown = 0;
	INIT_DELAYED_WORK(&work_state.dwork, hdmi_state_machine_worker);
}

void hdmi_state_machine_shutdown(void)
{
	work_state.shutdown = 1;
	cancel_delayed_work_sync(&work_state.dwork);
}

void hdmi_state_machine_set_pending_hpd(void)
{
	rt_mutex_lock(&work_lock);

	/* We always schedule work any time there is a pending HPD event */
	work_state.pending_hpd_evt = 1;
	hdmi_state_machine_sched_work_l(0);

	rt_mutex_unlock(&work_lock);
}

int hdmi_state_machine_get_state(void)
{
	int ret;

	rt_mutex_lock(&work_lock);
	ret = work_state.state;
	rt_mutex_unlock(&work_lock);

	return ret;
}
