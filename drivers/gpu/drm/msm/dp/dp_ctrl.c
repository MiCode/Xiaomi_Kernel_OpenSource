/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <drm/drm_fixed.h>

#include "dp_ctrl.h"

#define DP_MST_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#define DP_CTRL_INTR_READY_FOR_VIDEO     BIT(0)
#define DP_CTRL_INTR_IDLE_PATTERN_SENT  BIT(3)

#define DP_CTRL_INTR_MST_DP0_VCPF_SENT	BIT(0)
#define DP_CTRL_INTR_MST_DP1_VCPF_SENT	BIT(3)

/* dp state ctrl */
#define ST_TRAIN_PATTERN_1		BIT(0)
#define ST_TRAIN_PATTERN_2		BIT(1)
#define ST_TRAIN_PATTERN_3		BIT(2)
#define ST_TRAIN_PATTERN_4		BIT(3)
#define ST_SYMBOL_ERR_RATE_MEASUREMENT	BIT(4)
#define ST_PRBS7			BIT(5)
#define ST_CUSTOM_80_BIT_PATTERN	BIT(6)
#define ST_SEND_VIDEO			BIT(7)
#define ST_PUSH_IDLE			BIT(8)
#define MST_DP0_PUSH_VCPF		BIT(12)
#define MST_DP0_FORCE_VCPF		BIT(13)
#define MST_DP1_PUSH_VCPF		BIT(14)
#define MST_DP1_FORCE_VCPF		BIT(15)

#define MR_LINK_TRAINING1  0x8
#define MR_LINK_SYMBOL_ERM 0x80
#define MR_LINK_PRBS7 0x100
#define MR_LINK_CUSTOM80 0x200
#define MR_LINK_TRAINING4  0x40

struct dp_mst_ch_slot_info {
	u32 start_slot;
	u32 tot_slots;
};

struct dp_mst_channel_info {
	struct dp_mst_ch_slot_info slot_info[DP_STREAM_MAX];
};

struct dp_ctrl_private {
	struct dp_ctrl dp_ctrl;

	struct device *dev;
	struct dp_aux *aux;
	struct dp_panel *panel;
	struct dp_link *link;
	struct dp_power *power;
	struct dp_parser *parser;
	struct dp_catalog_ctrl *catalog;

	struct completion idle_comp;
	struct completion video_comp;

	bool orientation;
	bool power_on;
	bool mst_mode;
	bool fec_mode;

	atomic_t aborted;

	u32 vic;
	u32 stream_count;
	struct dp_mst_channel_info mst_ch_info;
};

enum notification_status {
	NOTIFY_UNKNOWN,
	NOTIFY_CONNECT,
	NOTIFY_DISCONNECT,
	NOTIFY_CONNECT_IRQ_HPD,
	NOTIFY_DISCONNECT_IRQ_HPD,
};

static void dp_ctrl_idle_patterns_sent(struct dp_ctrl_private *ctrl)
{
	pr_debug("idle_patterns_sent\n");
	complete(&ctrl->idle_comp);
}

static void dp_ctrl_video_ready(struct dp_ctrl_private *ctrl)
{
	pr_debug("dp_video_ready\n");
	complete(&ctrl->video_comp);
}

static void dp_ctrl_abort(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	atomic_set(&ctrl->aborted, 1);
}

static void dp_ctrl_state_ctrl(struct dp_ctrl_private *ctrl, u32 state)
{
	ctrl->catalog->state_ctrl(ctrl->catalog, state);
}

static void dp_ctrl_push_idle(struct dp_ctrl_private *ctrl,
				enum dp_stream_id strm)
{
	int const idle_pattern_completion_timeout_ms = HZ / 10;
	u32 state = 0x0;

	if (!ctrl->power_on)
		return;

	if (!ctrl->mst_mode) {
		state = ST_PUSH_IDLE;
		goto trigger_idle;
	}

	if (strm >= DP_STREAM_MAX) {
		pr_err("mst push idle, invalid stream:%d\n", strm);
		return;
	}

	state |= (strm == DP_STREAM_0) ? MST_DP0_PUSH_VCPF : MST_DP1_PUSH_VCPF;

trigger_idle:
	reinit_completion(&ctrl->idle_comp);
	dp_ctrl_state_ctrl(ctrl, state);

	if (!wait_for_completion_timeout(&ctrl->idle_comp,
			idle_pattern_completion_timeout_ms))
		pr_warn("time out\n");
	else
		pr_debug("mainlink off done\n");
}

/**
 * dp_ctrl_configure_source_link_params() - configures DP TX source params
 * @ctrl: Display Port Driver data
 * @enable: enable or disable DP transmitter
 *
 * Configures the DP transmitter source params including details such as lane
 * configuration, output format and sink/panel timing information.
 */
static void dp_ctrl_configure_source_link_params(struct dp_ctrl_private *ctrl,
		bool enable)
{
	if (enable) {
		ctrl->catalog->lane_mapping(ctrl->catalog, ctrl->orientation,
						ctrl->parser->l_map);
		ctrl->catalog->lane_pnswap(ctrl->catalog,
						ctrl->parser->l_pnswap);
		ctrl->catalog->mst_config(ctrl->catalog, ctrl->mst_mode);
		ctrl->catalog->config_ctrl(ctrl->catalog,
				ctrl->link->link_params.lane_count);
		ctrl->catalog->mainlink_levels(ctrl->catalog,
				ctrl->link->link_params.lane_count);
		ctrl->catalog->mainlink_ctrl(ctrl->catalog, true);
	} else {
		ctrl->catalog->mainlink_ctrl(ctrl->catalog, false);
	}
}

static void dp_ctrl_wait4video_ready(struct dp_ctrl_private *ctrl)
{
	if (!wait_for_completion_timeout(&ctrl->video_comp, HZ / 2))
		pr_warn("SEND_VIDEO time out\n");
}

static int dp_ctrl_update_sink_vx_px(struct dp_ctrl_private *ctrl,
		u32 voltage_level, u32 pre_emphasis_level)
{
	int i;
	u8 buf[4];
	u32 max_level_reached = 0;

	if (voltage_level == DP_LINK_VOLTAGE_MAX) {
		pr_debug("max. voltage swing level reached %d\n",
				voltage_level);
		max_level_reached |= BIT(2);
	}

	if (pre_emphasis_level == DP_LINK_PRE_EMPHASIS_MAX) {
		pr_debug("max. pre-emphasis level reached %d\n",
				pre_emphasis_level);
		max_level_reached  |= BIT(5);
	}

	pre_emphasis_level <<= 3;

	for (i = 0; i < 4; i++)
		buf[i] = voltage_level | pre_emphasis_level | max_level_reached;

	pr_debug("sink: p|v=0x%x\n", voltage_level | pre_emphasis_level);
	return drm_dp_dpcd_write(ctrl->aux->drm_aux, 0x103, buf, 4);
}

static int dp_ctrl_update_vx_px(struct dp_ctrl_private *ctrl)
{
	struct dp_link *link = ctrl->link;

	ctrl->catalog->update_vx_px(ctrl->catalog,
		link->phy_params.v_level, link->phy_params.p_level);

	return dp_ctrl_update_sink_vx_px(ctrl, link->phy_params.v_level,
		link->phy_params.p_level);
}

static int dp_ctrl_train_pattern_set(struct dp_ctrl_private *ctrl,
		u8 pattern)
{
	u8 buf[4];

	pr_debug("sink: pattern=%x\n", pattern);

	buf[0] = pattern;
	return drm_dp_dpcd_write(ctrl->aux->drm_aux,
		DP_TRAINING_PATTERN_SET, buf, 1);
}

static int dp_ctrl_read_link_status(struct dp_ctrl_private *ctrl,
					u8 *link_status)
{
	int ret = 0, len;
	u32 const offset = DP_LANE_ALIGN_STATUS_UPDATED - DP_LANE0_1_STATUS;
	u32 link_status_read_max_retries = 100;

	while (--link_status_read_max_retries) {
		len = drm_dp_dpcd_read_link_status(ctrl->aux->drm_aux,
			link_status);
		if (len != DP_LINK_STATUS_SIZE) {
			pr_err("DP link status read failed, err: %d\n", len);
			ret = len;
			break;
		}

		if (!(link_status[offset] & DP_LINK_STATUS_UPDATED))
			break;
	}

	return ret;
}

static int dp_ctrl_link_train_1(struct dp_ctrl_private *ctrl)
{
	int tries, old_v_level, ret = 0;
	u8 link_status[DP_LINK_STATUS_SIZE];
	int const maximum_retries = 5;

	ctrl->aux->state &= ~DP_STATE_TRAIN_1_FAILED;
	ctrl->aux->state &= ~DP_STATE_TRAIN_1_SUCCEEDED;
	ctrl->aux->state |= DP_STATE_TRAIN_1_STARTED;

	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	ctrl->catalog->set_pattern(ctrl->catalog, 0x01);
	ret = dp_ctrl_train_pattern_set(ctrl, DP_TRAINING_PATTERN_1 |
		DP_LINK_SCRAMBLING_DISABLE); /* train_1 */
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	ret = dp_ctrl_update_vx_px(ctrl);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	tries = 0;
	old_v_level = ctrl->link->phy_params.v_level;
	while (1) {
		if (atomic_read(&ctrl->aborted)) {
			ret = -EINVAL;
			break;
		}

		drm_dp_link_train_clock_recovery_delay(ctrl->panel->dpcd);

		ret = dp_ctrl_read_link_status(ctrl, link_status);
		if (ret)
			break;

		if (drm_dp_clock_recovery_ok(link_status,
			ctrl->link->link_params.lane_count)) {
			break;
		}

		if (ctrl->link->phy_params.v_level == DP_LINK_VOLTAGE_MAX) {
			pr_err_ratelimited("max v_level reached\n");
			ret = -EAGAIN;
			break;
		}

		if (old_v_level == ctrl->link->phy_params.v_level) {
			tries++;
			if (tries >= maximum_retries) {
				pr_err("max tries reached\n");
				ret = -ETIMEDOUT;
				break;
			}
		} else {
			tries = 0;
			old_v_level = ctrl->link->phy_params.v_level;
		}

		pr_debug("clock recovery not done, adjusting vx px\n");

		ctrl->link->adjust_levels(ctrl->link, link_status);
		ret = dp_ctrl_update_vx_px(ctrl);
		if (ret <= 0) {
			ret = -EINVAL;
			break;
		}
	}
end:
	ctrl->aux->state &= ~DP_STATE_TRAIN_1_STARTED;

	if (ret)
		ctrl->aux->state |= DP_STATE_TRAIN_1_FAILED;
	else
		ctrl->aux->state |= DP_STATE_TRAIN_1_SUCCEEDED;

	return ret;
}

static int dp_ctrl_link_rate_down_shift(struct dp_ctrl_private *ctrl)
{
	int ret = 0;

	if (!ctrl)
		return -EINVAL;

	switch (ctrl->link->link_params.bw_code) {
	case DP_LINK_BW_8_1:
		ctrl->link->link_params.bw_code = DP_LINK_BW_5_4;
		break;
	case DP_LINK_BW_5_4:
		ctrl->link->link_params.bw_code = DP_LINK_BW_2_7;
		break;
	case DP_LINK_BW_2_7:
	case DP_LINK_BW_1_62:
	default:
		ctrl->link->link_params.bw_code = DP_LINK_BW_1_62;
		break;
	};

	pr_debug("new bw code=0x%x\n", ctrl->link->link_params.bw_code);

	return ret;
}

static void dp_ctrl_clear_training_pattern(struct dp_ctrl_private *ctrl)
{
	dp_ctrl_train_pattern_set(ctrl, 0);
	drm_dp_link_train_channel_eq_delay(ctrl->panel->dpcd);
}

static int dp_ctrl_link_training_2(struct dp_ctrl_private *ctrl)
{
	int tries = 0, ret = 0;
	char pattern;
	int const maximum_retries = 5;
	u8 link_status[DP_LINK_STATUS_SIZE];

	ctrl->aux->state &= ~DP_STATE_TRAIN_2_FAILED;
	ctrl->aux->state &= ~DP_STATE_TRAIN_2_SUCCEEDED;
	ctrl->aux->state |= DP_STATE_TRAIN_2_STARTED;

	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	if (drm_dp_tps3_supported(ctrl->panel->dpcd))
		pattern = DP_TRAINING_PATTERN_3;
	else
		pattern = DP_TRAINING_PATTERN_2;

	ret = dp_ctrl_update_vx_px(ctrl);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}
	ctrl->catalog->set_pattern(ctrl->catalog, pattern);
	ret = dp_ctrl_train_pattern_set(ctrl,
		pattern | DP_RECOVERED_CLOCK_OUT_EN);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	do  {
		if (atomic_read(&ctrl->aborted)) {
			ret = -EINVAL;
			break;
		}

		drm_dp_link_train_channel_eq_delay(ctrl->panel->dpcd);

		ret = dp_ctrl_read_link_status(ctrl, link_status);
		if (ret)
			break;

		if (drm_dp_channel_eq_ok(link_status,
			ctrl->link->link_params.lane_count))
			break;

		if (tries > maximum_retries) {
			ret = -ETIMEDOUT;
			break;
		}
		tries++;

		ctrl->link->adjust_levels(ctrl->link, link_status);
		ret = dp_ctrl_update_vx_px(ctrl);
		if (ret <= 0) {
			ret = -EINVAL;
			break;
		}
	} while (1);
end:
	ctrl->aux->state &= ~DP_STATE_TRAIN_2_STARTED;

	if (ret)
		ctrl->aux->state |= DP_STATE_TRAIN_2_FAILED;
	else
		ctrl->aux->state |= DP_STATE_TRAIN_2_SUCCEEDED;
	return ret;
}

static int dp_ctrl_link_train(struct dp_ctrl_private *ctrl)
{
	int ret = 0;
	u8 encoding = 0x1;
	struct drm_dp_link link_info = {0};

	ctrl->link->phy_params.p_level = 0;
	ctrl->link->phy_params.v_level = 0;

	link_info.num_lanes = ctrl->link->link_params.lane_count;
	link_info.rate = drm_dp_bw_code_to_link_rate(
		ctrl->link->link_params.bw_code);
	link_info.capabilities = ctrl->panel->link_info.capabilities;

	ret = drm_dp_link_configure(ctrl->aux->drm_aux, &link_info);
	if (ret)
		goto end;

	ret = drm_dp_dpcd_write(ctrl->aux->drm_aux,
		DP_MAIN_LINK_CHANNEL_CODING_SET, &encoding, 1);
	if (ret <= 0) {
		ret = -EINVAL;
		goto end;
	}

	ret = dp_ctrl_link_train_1(ctrl);
	if (ret) {
		pr_err("link training #1 failed\n");
		goto end;
	}

	/* print success info as this is a result of user initiated action */
	pr_info("link training #1 successful\n");

	ret = dp_ctrl_link_training_2(ctrl);
	if (ret) {
		pr_err("link training #2 failed\n");
		goto end;
	}

	/* print success info as this is a result of user initiated action */
	pr_info("link training #2 successful\n");

end:
	dp_ctrl_state_ctrl(ctrl, 0);
	/* Make sure to clear the current pattern before starting a new one */
	wmb();

	dp_ctrl_clear_training_pattern(ctrl);
	return ret;
}

static int dp_ctrl_setup_main_link(struct dp_ctrl_private *ctrl)
{
	int ret = 0;
	const unsigned int fec_cfg_dpcd = 0x120;

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN)
		goto end;

	/*
	 * As part of previous calls, DP controller state might have
	 * transitioned to PUSH_IDLE. In order to start transmitting a link
	 * training pattern, we have to first to a DP software reset.
	 */
	ctrl->catalog->reset(ctrl->catalog);

	if (ctrl->fec_mode)
		drm_dp_dpcd_writeb(ctrl->aux->drm_aux, fec_cfg_dpcd, 0x01);

	ret = dp_ctrl_link_train(ctrl);

end:
	return ret;
}

static void dp_ctrl_set_clock_rate(struct dp_ctrl_private *ctrl,
		char *name, enum dp_pm_type clk_type, u32 rate)
{
	u32 num = ctrl->parser->mp[clk_type].num_clk;
	struct dss_clk *cfg = ctrl->parser->mp[clk_type].clk_config;

	while (num && strcmp(cfg->clk_name, name)) {
		num--;
		cfg++;
	}

	pr_debug("setting rate=%d on clk=%s\n", rate, name);

	if (num)
		cfg->rate = rate;
	else
		pr_err("%s clock could not be set with rate %d\n", name, rate);
}

static int dp_ctrl_enable_link_clock(struct dp_ctrl_private *ctrl)
{
	int ret = 0;
	u32 rate = drm_dp_bw_code_to_link_rate(ctrl->link->link_params.bw_code);
	enum dp_pm_type type = DP_LINK_PM;

	pr_debug("rate=%d\n", rate);

	dp_ctrl_set_clock_rate(ctrl, "link_clk", type, rate);

	ret = ctrl->power->clk_enable(ctrl->power, type, true);
	if (ret) {
		pr_err("Unabled to start link clocks\n");
		ret = -EINVAL;
	}

	return ret;
}

static void dp_ctrl_disable_link_clock(struct dp_ctrl_private *ctrl)
{
	ctrl->power->clk_enable(ctrl->power, DP_LINK_PM, false);
}

static int dp_ctrl_link_setup(struct dp_ctrl_private *ctrl, bool shallow)
{
	int rc = -EINVAL;
	u32 link_train_max_retries = 100;
	struct dp_catalog_ctrl *catalog;
	struct dp_link_params *link_params;

	catalog = ctrl->catalog;
	link_params = &ctrl->link->link_params;

	catalog->phy_lane_cfg(catalog, ctrl->orientation,
				link_params->lane_count);

	do {
		pr_debug("bw_code=%d, lane_count=%d\n",
			link_params->bw_code, link_params->lane_count);

		rc = dp_ctrl_enable_link_clock(ctrl);
		if (rc)
			break;

		dp_ctrl_configure_source_link_params(ctrl, true);

		rc = dp_ctrl_setup_main_link(ctrl);
		if (!rc)
			break;

		/*
		 * Shallow means link training failure is not important.
		 * If it fails, we still keep the link clocks on.
		 * In this mode, the system expects DP to be up
		 * even though the cable is removed. Disconnect interrupt
		 * will eventually trigger and shutdown DP.
		 */
		if (shallow) {
			rc = 0;
			break;
		}

		dp_ctrl_link_rate_down_shift(ctrl);

		dp_ctrl_configure_source_link_params(ctrl, false);
		dp_ctrl_disable_link_clock(ctrl);

		/* hw recommended delays before retrying link training */
		msleep(20);
	} while (--link_train_max_retries && !atomic_read(&ctrl->aborted));

	return rc;
}

static int dp_ctrl_enable_stream_clocks(struct dp_ctrl_private *ctrl,
		struct dp_panel *dp_panel)
{
	int ret = 0;
	u32 pclk;
	enum dp_pm_type clk_type;
	char clk_name[32] = "";

	ret = ctrl->power->set_pixel_clk_parent(ctrl->power,
			dp_panel->stream_id);

	if (ret)
		return ret;

	if (dp_panel->stream_id == DP_STREAM_0) {
		clk_type = DP_STREAM0_PM;
		strlcpy(clk_name, "strm0_pixel_clk", 32);
	} else if (dp_panel->stream_id == DP_STREAM_1) {
		clk_type = DP_STREAM1_PM;
		strlcpy(clk_name, "strm1_pixel_clk", 32);
	} else {
		pr_err("Invalid stream:%d for clk enable\n",
				dp_panel->stream_id);
		return -EINVAL;
	}

	pclk = dp_panel->pinfo.widebus_en ?
		(dp_panel->pinfo.pixel_clk_khz >> 1) :
		(dp_panel->pinfo.pixel_clk_khz);

	dp_ctrl_set_clock_rate(ctrl, clk_name, clk_type, pclk);

	ret = ctrl->power->clk_enable(ctrl->power, clk_type, true);
	if (ret) {
		pr_err("Unabled to start stream:%d clocks\n",
				dp_panel->stream_id);
		ret = -EINVAL;
	}

	return ret;
}

static int dp_ctrl_disable_stream_clocks(struct dp_ctrl_private *ctrl,
		struct dp_panel *dp_panel)
{
	int ret = 0;

	if (dp_panel->stream_id == DP_STREAM_0) {
		return ctrl->power->clk_enable(ctrl->power,
				DP_STREAM0_PM, false);
	} else if (dp_panel->stream_id == DP_STREAM_1) {
		return ctrl->power->clk_enable(ctrl->power,
				DP_STREAM1_PM, false);
	} else {
		pr_err("Invalid stream:%d for clk disable\n",
				dp_panel->stream_id);
		ret = -EINVAL;
	}
	return ret;
}
static int dp_ctrl_host_init(struct dp_ctrl *dp_ctrl, bool flip, bool reset)
{
	struct dp_ctrl_private *ctrl;
	struct dp_catalog_ctrl *catalog;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->orientation = flip;
	catalog = ctrl->catalog;

	if (reset) {
		catalog->usb_reset(ctrl->catalog, flip);
		catalog->phy_reset(ctrl->catalog);
	}
	catalog->enable_irq(ctrl->catalog, true);
	atomic_set(&ctrl->aborted, 0);

	return 0;
}

/**
 * dp_ctrl_host_deinit() - Uninitialize DP controller
 * @ctrl: Display Port Driver data
 *
 * Perform required steps to uninitialize DP controller
 * and its resources.
 */
static void dp_ctrl_host_deinit(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->catalog->enable_irq(ctrl->catalog, false);

	pr_debug("Host deinitialized successfully\n");
}

static void dp_ctrl_send_video(struct dp_ctrl_private *ctrl)
{
	ctrl->catalog->state_ctrl(ctrl->catalog, ST_SEND_VIDEO);
}

static int dp_ctrl_link_maintenance(struct dp_ctrl *dp_ctrl)
{
	int ret = 0;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_COMPLETED;
	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_FAILED;

	if (!ctrl->power_on) {
		pr_err("ctrl off\n");
		ret = -EINVAL;
		goto end;
	}

	if (atomic_read(&ctrl->aborted))
		goto end;

	ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_STARTED;
	ret = dp_ctrl_setup_main_link(ctrl);
	ctrl->aux->state &= ~DP_STATE_LINK_MAINTENANCE_STARTED;

	if (ret) {
		ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_FAILED;
		goto end;
	}

	ctrl->aux->state |= DP_STATE_LINK_MAINTENANCE_COMPLETED;

	if (ctrl->stream_count) {
		dp_ctrl_send_video(ctrl);
		dp_ctrl_wait4video_ready(ctrl);
	}
end:
	return ret;
}

static void dp_ctrl_process_phy_test_request(struct dp_ctrl *dp_ctrl)
{
	int ret = 0;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl) {
		pr_err("Invalid input data\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->link->phy_params.phy_test_pattern_sel) {
		pr_debug("no test pattern selected by sink\n");
		return;
	}

	pr_debug("start\n");

	/*
	 * The global reset will need DP link ralated clocks to be
	 * running. Add the global reset just before disabling the
	 * link clocks and core clocks.
	 */
	ctrl->catalog->reset(ctrl->catalog);
	ctrl->dp_ctrl.stream_pre_off(&ctrl->dp_ctrl, ctrl->panel);
	ctrl->dp_ctrl.stream_off(&ctrl->dp_ctrl, ctrl->panel);
	ctrl->dp_ctrl.off(&ctrl->dp_ctrl);

	ctrl->aux->init(ctrl->aux, ctrl->parser->aux_cfg);

	ret = ctrl->dp_ctrl.on(&ctrl->dp_ctrl, ctrl->mst_mode,
					ctrl->fec_mode, false);
	if (ret)
		pr_err("failed to enable DP controller\n");

	ctrl->dp_ctrl.stream_on(&ctrl->dp_ctrl, ctrl->panel);
	pr_debug("end\n");
}

static void dp_ctrl_send_phy_test_pattern(struct dp_ctrl_private *ctrl)
{
	bool success = false;
	u32 pattern_sent = 0x0;
	u32 pattern_requested = ctrl->link->phy_params.phy_test_pattern_sel;

	dp_ctrl_update_vx_px(ctrl);
	ctrl->catalog->send_phy_pattern(ctrl->catalog, pattern_requested);
	ctrl->link->send_test_response(ctrl->link);

	pattern_sent = ctrl->catalog->read_phy_pattern(ctrl->catalog);
	pr_debug("pattern_request: %s. pattern_sent: 0x%x\n",
			dp_link_get_phy_test_pattern(pattern_requested),
			pattern_sent);

	switch (pattern_sent) {
	case MR_LINK_TRAINING1:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_D10_2_NO_SCRAMBLING)
			success = true;
		break;
	case MR_LINK_SYMBOL_ERM:
		if ((pattern_requested ==
				DP_TEST_PHY_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT)
			|| (pattern_requested ==
				DP_TEST_PHY_PATTERN_CP2520_PATTERN_1))
			success = true;
		break;
	case MR_LINK_PRBS7:
		if (pattern_requested == DP_TEST_PHY_PATTERN_PRBS7)
			success = true;
		break;
	case MR_LINK_CUSTOM80:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_80_BIT_CUSTOM_PATTERN)
			success = true;
		break;
	case MR_LINK_TRAINING4:
		if (pattern_requested ==
				DP_TEST_PHY_PATTERN_CP2520_PATTERN_3)
			success = true;
		break;
	default:
		success = false;
		break;
	}

	pr_debug("%s: %s\n", success ? "success" : "failed",
			dp_link_get_phy_test_pattern(pattern_requested));
}

static void dp_ctrl_mst_calculate_rg(struct dp_ctrl_private *ctrl,
		struct dp_panel *panel, u32 *p_x_int, u32 *p_y_frac_enum)
{
	u64 min_slot_cnt, max_slot_cnt;
	u64 raw_target_sc, target_sc_fixp;
	u64 ts_denom, ts_enum, ts_int;
	u64 pclk = panel->pinfo.pixel_clk_khz;
	u64 lclk = panel->link_info.rate;
	u64 lanes = panel->link_info.num_lanes;
	u64 bpp = panel->pinfo.bpp;
	u64 pbn = panel->pbn;
	u64 numerator, denominator, temp, temp1, temp2;
	u32 x_int = 0, y_frac_enum = 0;
	u64 target_strm_sym, ts_int_fixp, ts_frac_fixp, y_frac_enum_fixp;

	if (panel->pinfo.comp_info.comp_ratio)
		bpp = panel->pinfo.comp_info.dsc_info.bpp;

	/* min_slot_cnt */
	numerator = pclk * bpp * 64 * 1000;
	denominator = lclk * lanes * 8 * 1000;
	min_slot_cnt = drm_fixp_from_fraction(numerator, denominator);

	/* max_slot_cnt */
	numerator = pbn * 54 * 1000;
	denominator = lclk * lanes;
	max_slot_cnt = drm_fixp_from_fraction(numerator, denominator);

	/* raw_target_sc */
	numerator = max_slot_cnt + min_slot_cnt;
	denominator = drm_fixp_from_fraction(2, 1);
	raw_target_sc = drm_fixp_div(numerator, denominator);

	pr_debug("raw_target_sc before overhead:0x%llx\n", raw_target_sc);
	pr_debug("dsc_overhead_fp:0x%llx\n", panel->pinfo.dsc_overhead_fp);

	/* apply fec and dsc overhead factor */
	if (panel->pinfo.dsc_overhead_fp)
		raw_target_sc = drm_fixp_mul(raw_target_sc,
					panel->pinfo.dsc_overhead_fp);

	if (panel->fec_overhead_fp)
		raw_target_sc = drm_fixp_mul(raw_target_sc,
					panel->fec_overhead_fp);

	pr_debug("raw_target_sc after overhead:0x%llx\n", raw_target_sc);

	/* target_sc */
	temp = drm_fixp_from_fraction(256 * lanes, 1);
	numerator = drm_fixp_mul(raw_target_sc, temp);
	denominator = drm_fixp_from_fraction(256 * lanes, 1);
	target_sc_fixp = drm_fixp_div(numerator, denominator);

	ts_enum = 256 * lanes;
	ts_denom = drm_fixp_from_fraction(256 * lanes, 1);
	ts_int = drm_fixp2int(target_sc_fixp);

	temp = drm_fixp2int_ceil(raw_target_sc);
	if (temp != ts_int) {
		temp = drm_fixp_from_fraction(ts_int, 1);
		temp1 = raw_target_sc - temp;
		temp2 = drm_fixp_mul(temp1, ts_denom);
		ts_enum = drm_fixp2int(temp2);
	}

	/* target_strm_sym */
	ts_int_fixp = drm_fixp_from_fraction(ts_int, 1);
	ts_frac_fixp = drm_fixp_from_fraction(ts_enum, drm_fixp2int(ts_denom));
	temp = ts_int_fixp + ts_frac_fixp;
	temp1 = drm_fixp_from_fraction(lanes, 1);
	target_strm_sym = drm_fixp_mul(temp, temp1);

	/* x_int */
	x_int = drm_fixp2int(target_strm_sym);

	/* y_enum_frac */
	temp = drm_fixp_from_fraction(x_int, 1);
	temp1 = target_strm_sym - temp;
	temp2 = drm_fixp_from_fraction(256, 1);
	y_frac_enum_fixp = drm_fixp_mul(temp1, temp2);

	temp1 = drm_fixp2int(y_frac_enum_fixp);
	temp2 = drm_fixp2int_ceil(y_frac_enum_fixp);

	y_frac_enum = (u32)((temp1 == temp2) ? temp1 : temp1 + 1);

	*p_x_int = x_int;
	*p_y_frac_enum = y_frac_enum;

	pr_debug("x_int: %d, y_frac_enum: %d\n", x_int, y_frac_enum);
}

static int dp_ctrl_mst_send_act(struct dp_ctrl_private *ctrl)
{
	bool act_complete;

	if (!ctrl->mst_mode)
		return 0;

	ctrl->catalog->trigger_act(ctrl->catalog);
	msleep(20); /* needs 1 frame time */

	ctrl->catalog->read_act_complete_sts(ctrl->catalog, &act_complete);

	if (!act_complete)
		pr_err("mst act trigger complete failed\n");
	else
		DP_MST_DEBUG("mst ACT trigger complete SUCCESS\n");

	return 0;
}

static void dp_ctrl_mst_stream_setup(struct dp_ctrl_private *ctrl,
		struct dp_panel *panel)
{
	u32 x_int, y_frac_enum, lanes, bw_code;
	int i;

	if (!ctrl->mst_mode)
		return;

	DP_MST_DEBUG("mst stream channel allocation\n");

	for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
		ctrl->catalog->channel_alloc(ctrl->catalog,
				i,
				ctrl->mst_ch_info.slot_info[i].start_slot,
				ctrl->mst_ch_info.slot_info[i].tot_slots);
	}

	lanes = ctrl->link->link_params.lane_count;
	bw_code = ctrl->link->link_params.bw_code;

	dp_ctrl_mst_calculate_rg(ctrl, panel, &x_int, &y_frac_enum);

	ctrl->catalog->update_rg(ctrl->catalog, panel->stream_id,
			x_int, y_frac_enum);

	DP_MST_DEBUG("mst stream:%d, start_slot:%d, tot_slots:%d\n",
			panel->stream_id,
			panel->channel_start_slot, panel->channel_total_slots);

	DP_MST_DEBUG("mst lane_cnt:%d, bw:%d, x_int:%d, y_frac:%d\n",
			lanes, bw_code, x_int, y_frac_enum);
}

static void dp_ctrl_fec_dsc_setup(struct dp_ctrl_private *ctrl)
{
	u8 fec_sts = 0;
	int rlen;
	u32 dsc_enable;
	const unsigned int fec_sts_dpcd = 0x280;

	if (ctrl->stream_count || !ctrl->fec_mode)
		return;

	ctrl->catalog->fec_config(ctrl->catalog, ctrl->fec_mode);

	/* wait for controller to start fec sequence */
	usleep_range(900, 1000);
	drm_dp_dpcd_readb(ctrl->aux->drm_aux, fec_sts_dpcd, &fec_sts);
	pr_debug("sink fec status:%d\n", fec_sts);

	dsc_enable = ctrl->fec_mode ? 1 : 0;
	rlen = drm_dp_dpcd_writeb(ctrl->aux->drm_aux, DP_DSC_ENABLE,
			dsc_enable);
	if (rlen < 1)
		pr_debug("failed to enable sink dsc\n");
}

static int dp_ctrl_stream_on(struct dp_ctrl *dp_ctrl, struct dp_panel *panel)
{
	int rc = 0;
	bool link_ready = false;
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl || !panel) {
		return -EINVAL;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	rc = dp_ctrl_enable_stream_clocks(ctrl, panel);
	if (rc) {
		pr_err("failure on stream clock enable\n");
		return rc;
	}

	rc = panel->hw_cfg(panel, true);
	if (rc)
		return rc;

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
		dp_ctrl_send_phy_test_pattern(ctrl);
		return 0;
	}

	dp_ctrl_mst_stream_setup(ctrl, panel);

	dp_ctrl_send_video(ctrl);

	dp_ctrl_mst_send_act(ctrl);

	dp_ctrl_wait4video_ready(ctrl);

	dp_ctrl_fec_dsc_setup(ctrl);

	ctrl->stream_count++;

	link_ready = ctrl->catalog->mainlink_ready(ctrl->catalog);
	pr_debug("mainlink %s\n", link_ready ? "READY" : "NOT READY");

	return rc;
}

static void dp_ctrl_mst_stream_pre_off(struct dp_ctrl *dp_ctrl,
		struct dp_panel *panel)
{
	struct dp_ctrl_private *ctrl;
	bool act_complete;
	int i;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->mst_mode)
		return;

	for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
		ctrl->catalog->channel_alloc(ctrl->catalog,
				i,
				ctrl->mst_ch_info.slot_info[i].start_slot,
				ctrl->mst_ch_info.slot_info[i].tot_slots);
	}

	ctrl->catalog->trigger_act(ctrl->catalog);
	msleep(20); /* needs 1 frame time */
	ctrl->catalog->read_act_complete_sts(ctrl->catalog, &act_complete);

	if (!act_complete)
		pr_err("mst stream_off act trigger complete failed\n");
	else
		DP_MST_DEBUG("mst stream_off ACT trigger complete SUCCESS\n");
}

static void dp_ctrl_stream_pre_off(struct dp_ctrl *dp_ctrl,
		struct dp_panel *panel)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl || !panel) {
		pr_err("invalid input\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	dp_ctrl_push_idle(ctrl, panel->stream_id);

	dp_ctrl_mst_stream_pre_off(dp_ctrl, panel);
}

static void dp_ctrl_stream_off(struct dp_ctrl *dp_ctrl, struct dp_panel *panel)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl || !panel)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->power_on)
		return;

	panel->hw_cfg(panel, false);

	dp_ctrl_disable_stream_clocks(ctrl, panel);
	ctrl->stream_count--;
}

static int dp_ctrl_on(struct dp_ctrl *dp_ctrl, bool mst_mode,
				bool fec_mode, bool shallow)
{
	int rc = 0;
	struct dp_ctrl_private *ctrl;
	u32 rate = 0;

	if (!dp_ctrl) {
		rc = -EINVAL;
		goto end;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (ctrl->power_on)
		goto end;

	if (atomic_read(&ctrl->aborted)) {
		rc = -EPERM;
		goto end;
	}

	ctrl->mst_mode = mst_mode;
	ctrl->fec_mode = fec_mode;
	rate = ctrl->panel->link_info.rate;

	if (ctrl->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
		pr_debug("using phy test link parameters\n");
	} else {
		ctrl->link->link_params.bw_code =
			drm_dp_link_rate_to_bw_code(rate);
		ctrl->link->link_params.lane_count =
			ctrl->panel->link_info.num_lanes;
	}

	pr_debug("bw_code=%d, lane_count=%d\n",
		ctrl->link->link_params.bw_code,
		ctrl->link->link_params.lane_count);

	rc = dp_ctrl_link_setup(ctrl, shallow);
	if (rc)
		goto end;

	ctrl->power_on = true;
end:
	return rc;
}

static void dp_ctrl_off(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	if (!ctrl->power_on)
		return;

	dp_ctrl_configure_source_link_params(ctrl, false);
	ctrl->catalog->reset(ctrl->catalog);

	/* Make sure DP is disabled before clk disable */
	wmb();

	dp_ctrl_disable_link_clock(ctrl);

	ctrl->mst_mode = false;
	ctrl->fec_mode = false;
	ctrl->power_on = false;
	memset(&ctrl->mst_ch_info, 0, sizeof(ctrl->mst_ch_info));
	pr_debug("DP off done\n");
}

static void dp_ctrl_set_mst_channel_info(struct dp_ctrl *dp_ctrl,
		enum dp_stream_id strm,
		u32 start_slot, u32 tot_slots)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl || strm >= DP_STREAM_MAX) {
		pr_err("invalid input\n");
		return;
	}

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->mst_ch_info.slot_info[strm].start_slot = start_slot;
	ctrl->mst_ch_info.slot_info[strm].tot_slots = tot_slots;
}

static void dp_ctrl_isr(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	ctrl->catalog->get_interrupt(ctrl->catalog);

	if (ctrl->catalog->isr & DP_CTRL_INTR_READY_FOR_VIDEO)
		dp_ctrl_video_ready(ctrl);

	if (ctrl->catalog->isr & DP_CTRL_INTR_IDLE_PATTERN_SENT)
		dp_ctrl_idle_patterns_sent(ctrl);

	if (ctrl->catalog->isr5 & DP_CTRL_INTR_MST_DP0_VCPF_SENT)
		dp_ctrl_idle_patterns_sent(ctrl);

	if (ctrl->catalog->isr5 & DP_CTRL_INTR_MST_DP1_VCPF_SENT)
		dp_ctrl_idle_patterns_sent(ctrl);
}

struct dp_ctrl *dp_ctrl_get(struct dp_ctrl_in *in)
{
	int rc = 0;
	struct dp_ctrl_private *ctrl;
	struct dp_ctrl *dp_ctrl;

	if (!in->dev || !in->panel || !in->aux ||
	    !in->link || !in->catalog) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	ctrl = devm_kzalloc(in->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		rc = -ENOMEM;
		goto error;
	}

	init_completion(&ctrl->idle_comp);
	init_completion(&ctrl->video_comp);

	/* in parameters */
	ctrl->parser   = in->parser;
	ctrl->panel    = in->panel;
	ctrl->power    = in->power;
	ctrl->aux      = in->aux;
	ctrl->link     = in->link;
	ctrl->catalog  = in->catalog;
	ctrl->dev  = in->dev;
	ctrl->mst_mode = false;
	ctrl->fec_mode = false;

	dp_ctrl = &ctrl->dp_ctrl;

	/* out parameters */
	dp_ctrl->init      = dp_ctrl_host_init;
	dp_ctrl->deinit    = dp_ctrl_host_deinit;
	dp_ctrl->on        = dp_ctrl_on;
	dp_ctrl->off       = dp_ctrl_off;
	dp_ctrl->abort     = dp_ctrl_abort;
	dp_ctrl->isr       = dp_ctrl_isr;
	dp_ctrl->link_maintenance = dp_ctrl_link_maintenance;
	dp_ctrl->process_phy_test_request = dp_ctrl_process_phy_test_request;
	dp_ctrl->stream_on = dp_ctrl_stream_on;
	dp_ctrl->stream_off = dp_ctrl_stream_off;
	dp_ctrl->stream_pre_off = dp_ctrl_stream_pre_off;
	dp_ctrl->set_mst_channel_info = dp_ctrl_set_mst_channel_info;

	return dp_ctrl;
error:
	return ERR_PTR(rc);
}

void dp_ctrl_put(struct dp_ctrl *dp_ctrl)
{
	struct dp_ctrl_private *ctrl;

	if (!dp_ctrl)
		return;

	ctrl = container_of(dp_ctrl, struct dp_ctrl_private, dp_ctrl);

	devm_kfree(ctrl->dev, ctrl);
}
