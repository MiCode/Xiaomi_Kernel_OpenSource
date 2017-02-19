/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[sde_rsc:%s:%d]: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/component.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/module.h>

#include <soc/qcom/rpmh.h>
#include <drm/drmP.h>
#include <drm/drm_irq.h>
#include "sde_rsc.h"

/* this time is ~0.02ms */
#define RSC_BACKOFF_TIME_NS		 20000

/* next two values should be same based on doc */

/* this time is ~0.2ms */
#define RSC_MODE_THRESHOLD_TIME_IN_NS	200000
/* this time is ~0.2ms */
#define RSC_TIME_SLOT_0_NS		200000

#define DEFAULT_PANEL_FPS		60
#define DEFAULT_PANEL_JITTER		5
#define DEFAULT_PANEL_PREFILL_LINES	16
#define DEFAULT_PANEL_VTOTAL		(480 + DEFAULT_PANEL_PREFILL_LINES)
#define TICKS_IN_NANO_SECOND		1000000000

#define MAX_BUFFER_SIZE 256

#define TRY_CMD_MODE_SWITCH		0xFFFF

static struct sde_rsc_priv *rsc_prv_list[MAX_RSC_COUNT];

/**
 * sde_rsc_client_create() - create the client for sde rsc.
 * Different displays like DSI, HDMI, DP, WB, etc should call this
 * api to register their vote for rpmh. They still need to vote for
 * power handle to get the clocks.

 * @rsc_index:   A client will be created on this RSC. As of now only
 *               SDE_RSC_INDEX is valid rsc index.
 * @name:	 Caller needs to provide some valid string to identify
 *               the client. "primary", "dp", "hdmi" are suggested name.
 * @is_primary:	 Caller needs to provide information if client is primary
 *               or not. Primary client votes will be redirected to
 *               display rsc.
 *
 * Return: client node pointer.
 */
struct sde_rsc_client *sde_rsc_client_create(u32 rsc_index, char *client_name,
	bool is_primary_client)
{
	struct sde_rsc_client *client;
	struct sde_rsc_priv *rsc;

	if (!client_name) {
		pr_err("client name is null- not supported\n");
		return ERR_PTR(-EINVAL);
	} else if (rsc_index >= MAX_RSC_COUNT) {
		pr_err("invalid rsc index\n");
		return ERR_PTR(-EINVAL);
	} else if (!rsc_prv_list[rsc_index]) {
		pr_err("rsc not probed yet or not available\n");
		return ERR_PTR(-EINVAL);
	}

	rsc = rsc_prv_list[rsc_index];
	client = kzalloc(sizeof(struct sde_rsc_client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&rsc->client_lock);
	strlcpy(client->name, client_name, MAX_RSC_CLIENT_NAME_LEN);
	client->current_state = SDE_RSC_IDLE_STATE;
	client->rsc_index = rsc_index;
	if (is_primary_client)
		rsc->primary_client = client;
	pr_debug("client %s rsc index:%d primary:%d\n", client_name,
						rsc_index, is_primary_client);

	list_add(&client->list, &rsc->client_list);
	mutex_unlock(&rsc->client_lock);

	return client;
}

/**
 * sde_rsc_client_destroy() - Destroy the sde rsc client.
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 *
 * Return: none
 */
void sde_rsc_client_destroy(struct sde_rsc_client *client)
{
	struct sde_rsc_priv *rsc;

	if (!client) {
		pr_debug("invalid client\n");
		goto end;
	} else if (client->rsc_index >= MAX_RSC_COUNT) {
		pr_err("invalid rsc index\n");
		goto end;
	}

	pr_debug("client %s destroyed\n", client->name);
	rsc = rsc_prv_list[client->rsc_index];
	if (!rsc)
		goto end;

	mutex_lock(&rsc->client_lock);
	if (client->current_state != SDE_RSC_IDLE_STATE)
		sde_rsc_client_state_update(client, SDE_RSC_IDLE_STATE,
								NULL, -1);
	list_del_init(&client->list);
	mutex_unlock(&rsc->client_lock);

	kfree(client);
end:
	return;
}

static u32 sde_rsc_timer_calculate(struct sde_rsc_priv *rsc,
	struct sde_rsc_cmd_config *cmd_config)
{
	const u32 cxo_period_ns = 52;
	u64 rsc_backoff_time_ns = RSC_BACKOFF_TIME_NS;
	u64 rsc_mode_threshold_time_ns = RSC_MODE_THRESHOLD_TIME_IN_NS;
	u64 rsc_time_slot_0_ns = RSC_TIME_SLOT_0_NS;
	u64 rsc_time_slot_1_ns;
	const u64 pdc_jitter = 20; /* 20% more */

	u64 frame_time_ns, frame_jitter;
	u64 line_time_ns, prefill_time_ns;
	u64 pdc_backoff_time_ns;
	s64 total;
	int ret = 0;

	if (cmd_config)
		memcpy(&rsc->cmd_config, cmd_config, sizeof(*cmd_config));

	/* calculate for 640x480 60 fps resolution by default */
	if (!rsc->cmd_config.fps)
		rsc->cmd_config.fps = DEFAULT_PANEL_FPS;
	if (!rsc->cmd_config.jitter)
		rsc->cmd_config.jitter = DEFAULT_PANEL_JITTER;
	if (!rsc->cmd_config.vtotal)
		rsc->cmd_config.vtotal = DEFAULT_PANEL_VTOTAL;
	if (!rsc->cmd_config.prefill_lines)
		rsc->cmd_config.prefill_lines = DEFAULT_PANEL_PREFILL_LINES;
	pr_debug("frame fps:%d jitter:%d vtotal:%d prefill lines:%d\n",
		rsc->cmd_config.fps, rsc->cmd_config.jitter,
		rsc->cmd_config.vtotal, rsc->cmd_config.prefill_lines);

	/* 1 nano second */
	frame_time_ns = TICKS_IN_NANO_SECOND;
	frame_time_ns = div_u64(frame_time_ns, rsc->cmd_config.fps);

	frame_jitter = frame_time_ns * rsc->cmd_config.jitter;
	/* convert it to percentage */
	frame_jitter = div_u64(frame_jitter, 100);

	line_time_ns = frame_time_ns;
	line_time_ns = div_u64(line_time_ns, rsc->cmd_config.vtotal);
	prefill_time_ns = line_time_ns * rsc->cmd_config.prefill_lines;

	total = frame_time_ns - frame_jitter - prefill_time_ns;
	if (total < 0) {
		pr_err("invalid total time period time:%llu jiter_time:%llu blanking time:%llu\n",
			frame_time_ns, frame_jitter, prefill_time_ns);
		total = 0;
	}

	total = div_u64(total, cxo_period_ns);
	rsc->timer_config.static_wakeup_time_ns = total;

	pr_debug("frame time:%llu frame jiter_time:%llu\n",
			frame_time_ns, frame_jitter);
	pr_debug("line time:%llu prefill time ps:%llu\n",
			line_time_ns, prefill_time_ns);
	pr_debug("static wakeup time:%lld cxo:%u\n", total, cxo_period_ns);

	pdc_backoff_time_ns = rsc_backoff_time_ns;
	rsc_backoff_time_ns = div_u64(rsc_backoff_time_ns, cxo_period_ns);
	rsc->timer_config.rsc_backoff_time_ns = (u32) rsc_backoff_time_ns;

	pdc_backoff_time_ns *= pdc_jitter;
	pdc_backoff_time_ns = div_u64(pdc_backoff_time_ns, 100);
	rsc->timer_config.pdc_backoff_time_ns = (u32) pdc_backoff_time_ns;

	rsc_mode_threshold_time_ns =
			div_u64(rsc_mode_threshold_time_ns, cxo_period_ns);
	rsc->timer_config.rsc_mode_threshold_time_ns
					= (u32) rsc_mode_threshold_time_ns;

	/* time_slot_0 for mode0 latency */
	rsc_time_slot_0_ns = div_u64(rsc_time_slot_0_ns, cxo_period_ns);
	rsc->timer_config.rsc_time_slot_0_ns = (u32) rsc_time_slot_0_ns;

	/* time_slot_1 for mode1 latency */
	rsc_time_slot_1_ns = frame_time_ns;
	rsc_time_slot_1_ns = div_u64(rsc_time_slot_1_ns, cxo_period_ns);
	rsc->timer_config.rsc_time_slot_1_ns = (u32) rsc_time_slot_1_ns;

	/* mode 2 is infinite */
	rsc->timer_config.rsc_time_slot_2_ns = 0xFFFFFFFF;

	if (rsc->hw_ops.init) {
		ret = rsc->hw_ops.init(rsc);
		if (ret)
			pr_err("sde rsc: hw init failed ret:%d\n", ret);
	}

	return ret;
}

static int sde_rsc_switch_to_idle(struct sde_rsc_priv *rsc)
{
	struct sde_rsc_client *client;
	int rc = 0;

	list_for_each_entry(client, &rsc->client_list, list)
		if (client->current_state != SDE_RSC_IDLE_STATE)
			return TRY_CMD_MODE_SWITCH;

	if (rsc->hw_ops.state_update)
		rc = rsc->hw_ops.state_update(rsc, SDE_RSC_IDLE_STATE);

	return rc;
}

static bool sde_rsc_switch_to_cmd(struct sde_rsc_priv *rsc,
	struct sde_rsc_cmd_config *config,
	struct sde_rsc_client *caller_client, bool wait_req)
{
	struct sde_rsc_client *client;
	int rc = 0;

	if (!rsc->primary_client) {
		pr_err("primary client not available for cmd state switch\n");
		rc = -EINVAL;
		goto end;
	} else if (caller_client != rsc->primary_client) {
		pr_err("primary client state:%d not cmd state request\n",
			rsc->primary_client->current_state);
		rc = -EINVAL;
		goto end;
	}

	/* update timers - might not be available at next switch */
	if (config)
		sde_rsc_timer_calculate(rsc, config);

	/* any one client in video state blocks the cmd state switch */
	list_for_each_entry(client, &rsc->client_list, list)
		if (client->current_state == SDE_RSC_VID_STATE)
			goto end;

	if (rsc->hw_ops.state_update)
		rc = rsc->hw_ops.state_update(rsc, SDE_RSC_CMD_STATE);

	/* wait for vsync */
	if (!rc && wait_req)
		drm_wait_one_vblank(rsc->master_drm,
						rsc->primary_client->crtc_id);
end:
	return rc;
}

static bool sde_rsc_switch_to_vid(struct sde_rsc_priv *rsc,
	struct sde_rsc_cmd_config *config,
	struct sde_rsc_client *caller_client, bool wait_req)
{
	int rc = 0;

	/* update timers - might not be available at next switch */
	if (config && (caller_client == rsc->primary_client))
		sde_rsc_timer_calculate(rsc, config);

	/* video state switch should be done immediately */
	if (rsc->hw_ops.state_update)
		rc = rsc->hw_ops.state_update(rsc, SDE_RSC_VID_STATE);

	/* wait for vsync */
	if (!rc && rsc->primary_client && wait_req)
		drm_wait_one_vblank(rsc->master_drm,
						rsc->primary_client->crtc_id);
	return rc;
}

/**
 * sde_rsc_client_state_update() - rsc client state update
 * Video mode and command mode are supported as modes. A client need to
 * set this property during panel config time. A switching client can set the
 * property to change the state
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @state:	 Client state - video/cmd
 * @config:	 fps, vtotal, porches, etc configuration for command mode
 *               panel
 * @crtc_id:	 current client's crtc id
 *
 * Return: error code.
 */
int sde_rsc_client_state_update(struct sde_rsc_client *caller_client,
	enum sde_rsc_state state,
	struct sde_rsc_cmd_config *config, int crtc_id)
{
	int rc = 0;
	struct sde_rsc_priv *rsc;
	bool wait_requested = false;

	if (!caller_client) {
		pr_err("invalid client for rsc state update\n");
		return -EINVAL;
	} else if (caller_client->rsc_index >= MAX_RSC_COUNT) {
		pr_err("invalid rsc index\n");
		return -EINVAL;
	}

	rsc = rsc_prv_list[caller_client->rsc_index];
	if (!rsc)
		return -EINVAL;

	mutex_lock(&rsc->client_lock);
	caller_client->crtc_id = crtc_id;
	caller_client->current_state = state;

	if (rsc->master_drm == NULL) {
		pr_err("invalid master component binding\n");
		rc = -EINVAL;
		goto end;
	} else if ((rsc->current_state == state) &&
				(state != SDE_RSC_CMD_UPDATE_STATE)) {
		pr_debug("no state change: %d\n", state);
		goto end;
	}

	pr_debug("%pS: rsc state:%d request client:%s state:%d\n",
		__builtin_return_address(0), rsc->current_state,
		caller_client->name, state);

	wait_requested = (rsc->current_state != SDE_RSC_IDLE_STATE);

	if (rsc->power_collapse)
		sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	switch (state) {
	case SDE_RSC_IDLE_STATE:
		rc = sde_rsc_switch_to_idle(rsc);
		/* video state client might be exiting; try cmd state switch */
		if (rc == TRY_CMD_MODE_SWITCH)
			rc = sde_rsc_switch_to_cmd(rsc, NULL,
					rsc->primary_client, wait_requested);
		break;

	case SDE_RSC_CMD_STATE:
	case SDE_RSC_CMD_UPDATE_STATE:
		rc = sde_rsc_switch_to_cmd(rsc, config, caller_client,
								wait_requested);
		break;

	case SDE_RSC_VID_STATE:
		rc = sde_rsc_switch_to_vid(rsc, config, caller_client,
								wait_requested);
		break;

	default:
		pr_err("invalid state handling %d\n", state);
		break;
	}

	if (rc) {
		pr_err("state update failed rc:%d\n", rc);
		goto end;
	}

	pr_debug("state switch successfully complete: %d\n", state);
	rsc->current_state = state;

	if (rsc->power_collapse)
		sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
end:
	mutex_unlock(&rsc->client_lock);
	return rc;
}

/**
 * sde_rsc_client_vote() - ab/ib vote from rsc client
 *
 * @client:	 Client pointer provided by sde_rsc_client_create().
 * @ab:		 aggregated bandwidth vote from client.
 * @ib:		 instant bandwidth vote from client.
 *
 * Return: error code.
 */
int sde_rsc_client_vote(struct sde_rsc_client *caller_client,
	u64 ab_vote, u64 ib_vote)
{
	int rc = 0;
	struct sde_rsc_priv *rsc;
	bool amc_mode = false;
	enum rpmh_state state;

	if (!caller_client) {
		pr_err("invalid client for ab/ib vote\n");
		return -EINVAL;
	} else if (caller_client->rsc_index >= MAX_RSC_COUNT) {
		pr_err("invalid rsc index\n");
		return -EINVAL;
	}

	rsc = rsc_prv_list[caller_client->rsc_index];
	if (!rsc)
		return -EINVAL;

	if (caller_client != rsc->primary_client) {
		pr_err("only primary client can use sde rsc:: curr client name:%s\n",
							caller_client->name);
		return -EINVAL;
	}
	pr_debug("client:%s ab:%llu ib:%llu\n",
			caller_client->name, ab_vote, ib_vote);

	mutex_lock(&rsc->client_lock);
	if ((caller_client->current_state == SDE_RSC_IDLE_STATE) ||
		(rsc->current_state == SDE_RSC_IDLE_STATE)) {

		pr_err("invalid state: client state:%d rsc state:%d\n",
			caller_client->current_state, rsc->current_state);
		rc = -EINVAL;
		goto end;
	}

	if (rsc->hw_ops.is_amc_mode)
		amc_mode = rsc->hw_ops.is_amc_mode(rsc);

	if (rsc->current_state == SDE_RSC_CMD_STATE)
		state = RPMH_WAKE_ONLY_STATE;
	else if (amc_mode)
		state = RPMH_ACTIVE_ONLY_STATE;
	else
		state = RPMH_AWAKE_STATE;

	if (rsc->hw_ops.tcs_wait) {
		rc = rsc->hw_ops.tcs_wait(rsc);
		if (rc) {
			pr_err("tcs is still busy; can't send command\n");
			if (rsc->hw_ops.tcs_use_ok)
				rsc->hw_ops.tcs_use_ok(rsc);
			goto end;
		}
	}

	sde_power_data_bus_set_quota(&rsc->phandle, rsc->pclient,
		SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT, ab_vote, ib_vote);

	if (rsc->hw_ops.tcs_use_ok)
		rsc->hw_ops.tcs_use_ok(rsc);

end:
	mutex_unlock(&rsc->client_lock);
	return rc;
}

static int _sde_debugfs_status_show(struct seq_file *s, void *data)
{
	struct sde_rsc_priv *rsc;
	struct sde_rsc_client *client;
	int ret;

	if (!s || !s->private)
		return -EINVAL;

	rsc = s->private;

	mutex_lock(&rsc->client_lock);
	seq_printf(s, "rsc current state:%d\n", rsc->current_state);
	seq_printf(s, "wraper backoff time(ns):%d\n",
				rsc->timer_config.static_wakeup_time_ns);
	seq_printf(s, "rsc backoff time(ns):%d\n",
				rsc->timer_config.rsc_backoff_time_ns);
	seq_printf(s, "pdc backoff time(ns):%d\n",
				rsc->timer_config.pdc_backoff_time_ns);
	seq_printf(s, "rsc mode threshold time(ns):%d\n",
				rsc->timer_config.rsc_mode_threshold_time_ns);
	seq_printf(s, "rsc time slot 0(ns):%d\n",
				rsc->timer_config.rsc_time_slot_0_ns);
	seq_printf(s, "rsc time slot 1(ns):%d\n",
				rsc->timer_config.rsc_time_slot_1_ns);
	seq_printf(s, "frame fps:%d jitter:%d vtotal:%d prefill lines:%d\n",
			rsc->cmd_config.fps, rsc->cmd_config.jitter,
			rsc->cmd_config.vtotal, rsc->cmd_config.prefill_lines);

	seq_puts(s, "\n");

	list_for_each_entry(client, &rsc->client_list, list)
		seq_printf(s, "\t client:%s state:%d\n",
				client->name, client->current_state);

	sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	if (rsc->hw_ops.debug_show) {
		ret = rsc->hw_ops.debug_show(s, rsc);
		if (ret)
			pr_err("sde rsc: hw debug failed ret:%d\n", ret);
	}

	sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	mutex_unlock(&rsc->client_lock);

	return 0;
}

static int _sde_debugfs_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, _sde_debugfs_status_show, inode->i_private);
}

static int _sde_debugfs_mode_ctrl_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t _sde_debugfs_mode_ctrl_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct sde_rsc_priv *rsc = file->private_data;
	char buffer[MAX_BUFFER_SIZE];
	int blen = 0;

	if (*ppos || !rsc || !rsc->hw_ops.mode_ctrl)
		return 0;

	mutex_lock(&rsc->client_lock);
	sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	blen = rsc->hw_ops.mode_ctrl(rsc, MODE_READ, buffer,
							MAX_BUFFER_SIZE, 0);

	sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	mutex_unlock(&rsc->client_lock);

	if (blen < 0)
		return 0;

	if (copy_to_user(buf, buffer, blen))
		return -EFAULT;

	*ppos += blen;
	return blen;
}

static ssize_t _sde_debugfs_mode_ctrl_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct sde_rsc_priv *rsc = file->private_data;
	char *input, *mode;
	u32 mode0_state = 0, mode1_state = 0, mode2_state = 0;

	if (!rsc || !rsc->hw_ops.mode_ctrl)
		return 0;

	input = kmalloc(count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		kfree(input);
		return -EFAULT;
	}
	input[count - 1] = '\0';

	mutex_lock(&rsc->client_lock);
	sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	mode = strnstr(input, "mode0=", strlen("mode0="));
	if (mode) {
		mode0_state = mode[0] - '0';
		mode0_state &= BIT(0);
		rsc->hw_ops.mode_ctrl(rsc, MODE0_UPDATE, NULL, 0, mode0_state);
		goto end;
	}

	mode = strnstr(input, "mode1=", strlen("mode1="));
	if (mode) {
		mode1_state = mode[0] - '0';
		mode1_state &= BIT(0);
		rsc->hw_ops.mode_ctrl(rsc, MODE1_UPDATE, NULL, 0, mode1_state);
		goto end;
	}

	mode = strnstr(input, "mode2=", strlen("mode2="));
	if (mode) {
		mode2_state = mode[0] - '0';
		mode2_state &= BIT(0);
		rsc->hw_ops.mode_ctrl(rsc, MODE2_UPDATE, NULL, 0, mode2_state);
	}

end:
	sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	mutex_unlock(&rsc->client_lock);

	pr_err("req: mode0:%d mode1:%d mode2:%d\n", mode0_state, mode1_state,
								mode2_state);
	kfree(input);
	return count;
}

static int _sde_debugfs_vsync_mode_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t _sde_debugfs_vsync_mode_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct sde_rsc_priv *rsc = file->private_data;
	char buffer[MAX_BUFFER_SIZE];
	int blen = 0;

	if (*ppos || !rsc || !rsc->hw_ops.hw_vsync)
		return 0;

	mutex_lock(&rsc->client_lock);
	sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	blen = rsc->hw_ops.hw_vsync(rsc, VSYNC_READ, buffer,
						MAX_BUFFER_SIZE, 0);

	sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	mutex_unlock(&rsc->client_lock);

	if (blen < 0)
		return 0;

	if (copy_to_user(buf, buffer, blen))
		return -EFAULT;

	*ppos += blen;
	return blen;
}

static ssize_t _sde_debugfs_vsync_mode_write(struct file *file,
			const char __user *p, size_t count, loff_t *ppos)
{
	struct sde_rsc_priv *rsc = file->private_data;
	char *input, *vsync_mode;
	u32 vsync_state = 0;

	if (!rsc || !rsc->hw_ops.hw_vsync)
		return 0;

	input = kmalloc(count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, p, count)) {
		kfree(input);
		return -EFAULT;
	}
	input[count - 1] = '\0';

	vsync_mode = strnstr(input, "vsync_mode=", strlen("vsync_mode="));
	if (vsync_mode) {
		vsync_state = vsync_mode[0] - '0';
		vsync_state &= 0x7;
	}

	mutex_lock(&rsc->client_lock);
	sde_power_resource_enable(&rsc->phandle, rsc->pclient, true);

	if (vsync_state)
		rsc->hw_ops.hw_vsync(rsc, VSYNC_ENABLE, NULL,
							0, vsync_state - 1);
	else
		rsc->hw_ops.hw_vsync(rsc, VSYNC_DISABLE, NULL, 0, 0);

	sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	mutex_unlock(&rsc->client_lock);

	kfree(input);
	return count;
}

static const struct file_operations debugfs_status_fops = {
	.open =		_sde_debugfs_status_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static const struct file_operations mode_control_fops = {
	.open =		_sde_debugfs_mode_ctrl_open,
	.read =		_sde_debugfs_mode_ctrl_read,
	.write =	_sde_debugfs_mode_ctrl_write,
};

static const struct file_operations vsync_status_fops = {
	.open =		_sde_debugfs_vsync_mode_open,
	.read =		_sde_debugfs_vsync_mode_read,
	.write =	_sde_debugfs_vsync_mode_write,
};

static void _sde_rsc_init_debugfs(struct sde_rsc_priv *rsc, char *name)
{
	rsc->debugfs_root = debugfs_create_dir(name, NULL);
	if (!rsc->debugfs_root)
		return;

	/* don't error check these */
	debugfs_create_file("status", 0444, rsc->debugfs_root, rsc,
							&debugfs_status_fops);
	debugfs_create_file("mode_control", 0644, rsc->debugfs_root, rsc,
							&mode_control_fops);
	debugfs_create_file("vsync_mode", 0644, rsc->debugfs_root, rsc,
							&vsync_status_fops);
	debugfs_create_x32("debug_mode", 0644, rsc->debugfs_root,
							&rsc->debug_mode);
}

static void sde_rsc_deinit(struct platform_device *pdev,
					struct sde_rsc_priv *rsc)
{
	if (!rsc)
		return;

	if (rsc->pclient)
		sde_power_resource_enable(&rsc->phandle, rsc->pclient, false);
	if (rsc->fs)
		devm_regulator_put(rsc->fs);
	if (rsc->wrapper_io.base)
		msm_dss_iounmap(&rsc->wrapper_io);
	if (rsc->drv_io.base)
		msm_dss_iounmap(&rsc->drv_io);
	if (rsc->pclient)
		sde_power_client_destroy(&rsc->phandle, rsc->pclient);

	sde_power_resource_deinit(pdev, &rsc->phandle);
	debugfs_remove_recursive(rsc->debugfs_root);
	kfree(rsc);
}

/**
 * sde_rsc_bind - bind rsc device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int sde_rsc_bind(struct device *dev,
		struct device *master,
		void *data)
{
	struct sde_rsc_priv *rsc;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		return -EINVAL;
	}

	drm = dev_get_drvdata(master);
	rsc = platform_get_drvdata(pdev);
	if (!drm || !rsc) {
		pr_err("invalid param(s), drm %pK, rsc %pK\n",
				drm, rsc);
		return -EINVAL;
	}

	mutex_lock(&rsc->client_lock);
	rsc->master_drm = drm;
	mutex_unlock(&rsc->client_lock);

	return 0;
}

/**
 * sde_rsc_unbind - unbind rsc from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void sde_rsc_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct sde_rsc_priv *rsc;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	rsc = platform_get_drvdata(pdev);
	if (!rsc) {
		pr_err("invalid display rsc\n");
		return;
	}

	mutex_lock(&rsc->client_lock);
	rsc->master_drm = NULL;
	mutex_unlock(&rsc->client_lock);
}

static const struct component_ops sde_rsc_comp_ops = {
	.bind = sde_rsc_bind,
	.unbind = sde_rsc_unbind,
};

static int sde_rsc_probe(struct platform_device *pdev)
{
	int ret;
	struct sde_rsc_priv *rsc;
	static int counter;
	char  name[MAX_RSC_CLIENT_NAME_LEN];

	rsc = kzalloc(sizeof(*rsc), GFP_KERNEL);
	if (!rsc) {
		ret = -ENOMEM;
		goto rsc_alloc_fail;
	}

	platform_set_drvdata(pdev, rsc);
	of_property_read_u32(pdev->dev.of_node, "qcom,sde-rsc-version",
								&rsc->version);

	ret = sde_power_resource_init(pdev, &rsc->phandle);
	if (ret) {
		pr_err("sde rsc:power resource init failed ret:%d\n", ret);
		goto sde_rsc_fail;
	}

	rsc->pclient = sde_power_client_create(&rsc->phandle, "rsc");
	if (IS_ERR_OR_NULL(rsc->pclient)) {
		ret = PTR_ERR(rsc->pclient);
		rsc->pclient = NULL;
		pr_err("sde rsc:power client create failed ret:%d\n", ret);
		goto sde_rsc_fail;
	}

	ret = msm_dss_ioremap_byname(pdev, &rsc->wrapper_io, "wrapper");
	if (ret) {
		pr_err("sde rsc: wrapper io data mapping failed ret=%d\n", ret);
		goto sde_rsc_fail;
	}

	ret = msm_dss_ioremap_byname(pdev, &rsc->drv_io, "drv");
	if (ret) {
		pr_err("sde rsc: drv io data mapping failed ret:%d\n", ret);
		goto sde_rsc_fail;
	}

	rsc->fs = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(rsc->fs)) {
		rsc->fs = NULL;
		pr_err("unable to get regulator\n");
		goto sde_rsc_fail;
	}

	ret = sde_rsc_hw_register(rsc);
	if (ret) {
		pr_err("sde rsc: hw register failed ret:%d\n", ret);
		goto sde_rsc_fail;
	}

	/* these clocks are always on */
	if (sde_power_resource_enable(&rsc->phandle, rsc->pclient, true)) {
		pr_err("failed to enable sde rsc power resources\n");
		goto sde_rsc_fail;
	}

	if (sde_rsc_timer_calculate(rsc, NULL))
		goto sde_rsc_fail;

	INIT_LIST_HEAD(&rsc->client_list);
	mutex_init(&rsc->client_lock);

	pr_info("sde rsc index:%d probed successfully\n",
				SDE_RSC_INDEX + counter);

	rsc_prv_list[SDE_RSC_INDEX + counter] = rsc;
	snprintf(name, MAX_RSC_CLIENT_NAME_LEN, "%s%d", "sde_rsc", counter);
	_sde_rsc_init_debugfs(rsc, name);
	counter++;

	ret = component_add(&pdev->dev, &sde_rsc_comp_ops);
	if (ret)
		pr_debug("component add failed, ret=%d\n", ret);
	ret = 0;

	return ret;

sde_rsc_fail:
	sde_rsc_deinit(pdev, rsc);
rsc_alloc_fail:
	return ret;
}

static int sde_rsc_remove(struct platform_device *pdev)
{
	struct sde_rsc_priv *rsc = platform_get_drvdata(pdev);

	sde_rsc_deinit(pdev, rsc);
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,sde-rsc"},
	{}
};

MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver sde_rsc_platform_driver = {
	.probe      = sde_rsc_probe,
	.remove     = sde_rsc_remove,
	.driver     = {
		.name   = "sde_rsc",
		.of_match_table = dt_match,
	},
};

static int __init sde_rsc_register(void)
{
	return platform_driver_register(&sde_rsc_platform_driver);
}

static void __exit sde_rsc_unregister(void)
{
	platform_driver_unregister(&sde_rsc_platform_driver);
}

module_init(sde_rsc_register);
module_exit(sde_rsc_unregister);
