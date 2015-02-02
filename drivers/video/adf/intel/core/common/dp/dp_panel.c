/*
 * Copyright (C) 2014, Intel Corporation.
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
 * Create on 15 Dec 2014
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#include <core/intel_platform_config.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/common/dp/dp_panel.h>

#define EDID_LENGTH 128


#ifdef DP_USE_FALLBACK_EDID
static u8 raw_edid[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x10, 0xAC,
0x73, 0x40, 0x4C, 0x36, 0x33, 0x37, 0x24, 0x16, 0x01, 0x04,
0xA5, 0x33, 0x1D, 0x78, 0x3B, 0xDD, 0x45, 0xA3, 0x55, 0x4F,
0xA0, 0x27, 0x12, 0x50, 0x54, 0xA5, 0x4B, 0x00, 0x71, 0x4F,
0x81, 0x80, 0xD1, 0xC0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38,
0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0xFE, 0x1F, 0x11, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x57, 0x4D, 0x59,
0x4A, 0x57, 0x32, 0x39, 0x37, 0x37, 0x33, 0x36, 0x4C, 0x0A,
0x00, 0x00, 0x00, 0xFC, 0x00, 0x44, 0x45, 0x4C, 0x4C, 0x20,
0x55, 0x32, 0x33, 0x31, 0x32, 0x48, 0x4D, 0x0A, 0x00, 0x00,
0x00, 0xFD, 0x00, 0x38, 0x4C, 0x1E, 0x53, 0x11, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x98,
};

#endif

void dp_panel_copy_data_from_monitor(struct dp_panel *panel,
	struct hdmi_monitor *monitor)
{
	struct drm_mode_modeinfo *probed_modes = NULL;
	struct hdmi_mode_info *t, *mode;
	int count = 0;

	if (!monitor) {
		pr_err("%s: invalid pointer passed\n", __func__);
		return;
	}

	memcpy(&panel->eld, monitor->eld, sizeof(panel->eld));
	panel->is_hdmi = monitor->is_hdmi;
	panel->has_audio = monitor->has_audio;

	panel->probed_modes = monitor->probed_modes;
	panel->preferred_mode = monitor->preferred_mode;
	panel->screen_width_mm = monitor->screen_width_mm;
	panel->screen_height_mm = monitor->screen_height_mm;
	panel->video_code = monitor->video_code;
	panel->no_probed_modes = monitor->no_probed_modes;

	probed_modes = kzalloc(monitor->no_probed_modes *
			sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!probed_modes) {
		pr_err("%s OOM\n", __func__);
		return;
	}

	list_for_each_entry_safe(mode, t, &monitor->probed_modes, head) {
		memcpy(&probed_modes[count], &mode->drm_mode,
				sizeof(struct drm_mode_modeinfo));
		if (++count == monitor->no_probed_modes)
			break;
	}

	/* Update list */
	panel->modelist = probed_modes;
}

bool dp_panel_probe(struct dp_panel *panel, struct intel_pipeline *pipeline)
{
	/* bool live_status = false; */
	struct hdmi_monitor *monitor = NULL;
	struct i2c_adapter *adapter = vlv_get_i2c_adapter(pipeline);

	panel->edid = get_edid(adapter);
	if (!panel->edid) {
		pr_err("%s:Failed to read EDID\n", __func__);
#ifdef DP_USE_FALLBACK_EDID
		panel->edid = kzalloc(EDID_LENGTH, GFP_KERNEL);
		if (!panel->edid) {
			pr_err("%s: OOM(EDID)\n", __func__);
			return false;
		}

		/* Fallback to hard coded EDID */
		memcpy(panel->edid, raw_edid, EDID_LENGTH);
		pr_err("%s:falback EDID loaded\n", __func__);
#else
		/* live_status = false; */
#endif
	}

	/*
	 * HACK: to avoid duplication of edid parser logic,
	 * current edid parser takes hdmi_monitor only so call it with
	 * dummy pointer and copy the values if panel is detected
	 */
	monitor = intel_adf_hdmi_get_monitor(panel->edid);
	if (!monitor) {
		pr_err("%s: Failed mode parsing monitor\n", __func__);
		kfree(panel->edid);
		return false;
	}

	dp_panel_copy_data_from_monitor(panel, monitor);
	kfree(monitor);
	pr_info("%s: DP Connected\n", __func__);

	return true;
}


#define DP_LINK_ALIGNED (1 << 3)

static int dp_panel_get_dpcd(struct dp_panel *panel, u32 address,
			u8 *buffer, u32 size)
{
	struct intel_pipeline *pipeline = panel->pipeline;
	struct dp_aux_msg msg = {0};
	int err;

	/* read dpcd aux or i2c */
	msg.address = address;
	msg.request = DP_AUX_NATIVE_READ;
	msg.buffer = buffer;
	msg.size = (size_t) size;
	err = vlv_aux_transfer(pipeline, &msg);

	return err;
}

int dp_panel_set_dpcd(struct dp_panel *panel, u32 address,
			u8 *buffer, u32 size)
{
	struct intel_pipeline *pipeline = panel->pipeline;
	struct dp_aux_msg msg = {0};
	int err;

	/* write dpcd aux or i2c */
	msg.address = address;
	msg.request = DP_AUX_NATIVE_WRITE;
	msg.buffer = buffer;
	msg.size = (size_t) size;
	err = vlv_aux_transfer(pipeline, &msg);

	return err;
}

int dp_panel_get_max_link_bw(struct dp_panel *panel)
{
	u8 link_rate = 0;

	if (panel->dpcd_start[DP_MAX_LINK_RATE] == 0) {
		dp_panel_get_dpcd(panel, DP_MAX_LINK_RATE, &link_rate, 1);
		panel->dpcd_start[DP_MAX_LINK_RATE] = link_rate;
	} else
		link_rate = panel->dpcd_start[DP_MAX_LINK_RATE];

	if (link_rate > DP_LINK_BW_2_7) {
		pr_err("limiting link bw to HBR from HBR2\n");
		link_rate = DP_LINK_BW_2_7;
	}

	return link_rate;
}

u32 dp_panel_get_max_lane_count(struct dp_panel *panel)
{
	u8 lane_count = 0;
	if (panel->dpcd_start[DP_MAX_LANE_COUNT] == 0) {
		dp_panel_get_dpcd(panel, DP_MAX_LANE_COUNT, &lane_count, 1);
		panel->dpcd_start[DP_MAX_LANE_COUNT] = lane_count;
	} else
		lane_count = panel->dpcd_start[DP_MAX_LANE_COUNT];

	lane_count &= DP_MAX_LANE_COUNT_MASK;

	return lane_count;
}

static bool dp_panel_set_vswing_premp(struct dp_panel *panel,
	struct link_params *params, bool max_vswing, bool max_preemp)
{
	u8 lane_set[4] = {0};
	int i = 0;

	for (i = 0; i < params->lane_count ; i++) {

		/* update vsync and preemp based on dp spec offsets */
		lane_set[i] = params->vswing;
		lane_set[i] |= ((max_vswing) ? 1 : 0) << 2;
		lane_set[i] |= params->preemp << DP_TRAIN_PRE_EMPHASIS_SHIFT;
		lane_set[i] |= ((max_preemp) ? 1 : 0) << 5;
	}

	/* program vswing and preemp to dpcd*/
	return dp_panel_set_dpcd(panel, DP_TRAINING_LANE0_SET,
			lane_set, params->lane_count);
}

static u32 dp_panel_get_link_status(struct dp_panel *panel,
	struct link_params *params)
{
	u32 link_status;
	u8 ret_status = 0, temp = 0;
	u8 val = 0, i = 0;

	/* read dpcd */
	dp_panel_get_dpcd(panel, DP_LANE0_1_STATUS, (u8 *)&link_status, 3);

	/* return value must be OR of all lanes */
	for (i = 0; i < params->lane_count; i++) {
		val = (u8)(link_status >> (i * 4));
		val = val & 0xF;
		temp = (DP_LANE_CR_DONE & val);
		temp |= (DP_LANE_CHANNEL_EQ_DONE & val);
		temp |= (DP_LANE_SYMBOL_LOCKED & val);
		if (i == 0)
			ret_status = temp;
		ret_status &= temp;
	}

	/* get dpcd 204 */
	val = (u8)(link_status >> 16);
	if (val & DP_INTERLANE_ALIGN_DONE)
		ret_status |= DP_LINK_ALIGNED;

	return ret_status;
}

static bool dp_panel_train(struct dp_panel *panel,
			struct link_params *params, bool eq)
{
	u8 train_pattern = DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE;
	bool max_vswing_reached = false;
	bool max_preemp_reached = false;
	enum vswing_level vswing = e0_4;
	u32 i = 0;
	u32 link_status = 0;
	bool ret = false;

	if (eq == true) {
		/* if eq write TP2/TP3 to port and dpcd */
		if (params->link_bw == DP_LINK_BW_5_4)
			train_pattern = DP_TRAINING_PATTERN_3 |
					DP_LINK_SCRAMBLING_DISABLE;
		else
			train_pattern = DP_TRAINING_PATTERN_2 |
					DP_LINK_SCRAMBLING_DISABLE;
	}

	/* write TP to port and dpcd */
	vlv_set_link_pattern(panel->pipeline, train_pattern);
	dp_panel_set_dpcd(panel, DP_TRAINING_PATTERN_SET, &train_pattern, 1);

	for (i = 0; i < 5 && max_vswing_reached == false; i++) {
		if (vswing != params->vswing) {
			/* reset counter for new voltage */
			i = 0;
			vswing = params->vswing;
		}
		max_preemp_reached = false;
		max_vswing_reached = false;

		/* if max voltage is reached try once */
		if (params->preemp >= panel->max_preemp)
			max_preemp_reached = true;

		if (params->vswing >= panel->max_vswing)
			max_vswing_reached = true;

		/* write vswing and preemp to port / dpcd */
		vlv_set_signal_levels(panel->pipeline, params);
		dp_panel_set_vswing_premp(panel, params,
			max_vswing_reached, max_preemp_reached);

		/* wait for delay */
		if (eq == false)
			mdelay(100);
		else
			mdelay(400);

		/* read CR */
		link_status = dp_panel_get_link_status(panel, params);
		if ((link_status & DP_LANE_CR_DONE) != 0) {
			if (eq == false) {
				/* this call was for CR alone , so exit */
				ret = true;
				break;
			}
		} else if (eq == true) {
			/* this was for EQ, CR should not fail, exit */
			ret = false;
			break;
		}

		/* if call was for eq check EQ, symbol locked & aligned */
		if (eq == true) {
			if ((link_status & DP_LANE_CHANNEL_EQ_DONE) &&
				(link_status & DP_LANE_SYMBOL_LOCKED) &&
				(link_status & DP_LINK_ALIGNED)) {
				ret = true;
				break;
			}
			/* some thing failed :( retry */
		}

		/* read parms requested by panel */
		vlv_get_adjust_train(panel->pipeline, params);
	} /* for */

	return ret;
}

bool dp_panel_fast_link_train(struct dp_panel *panel,
		struct link_params *params)
{
	return false;
}

bool dp_panel_train_link(struct dp_panel *panel, struct link_params *params)
{
	u8 link_config[2];
	bool ret = false;
	u8 tmp = 1, i;
	int err = 0;

	for (i = 0; i < 3; i++) {
		err = dp_panel_set_dpcd(panel, DP_SET_POWER, &tmp, 1);
		if (err == 1)
			break;
		mdelay(1);
	}

	link_config[0] = params->link_bw;
	link_config[1] = (params->lane_count | (1 << 7));

	/* write link bw and lane count */
	dp_panel_set_dpcd(panel, DP_LINK_BW_SET, link_config, 2);

	/* write downspread ctrl */
	link_config[0] = 0;
	link_config[1] = DP_SET_ANSI_8B10B;
	dp_panel_set_dpcd(panel, DP_DOWNSPREAD_CTRL, link_config, 2);

	/* start with lowest vswing & preemp */
	params->preemp = e0dB;
	params->vswing = e0_4;

	/* check for CR alone */
	ret = dp_panel_train(panel, params, false);
	if (ret == false) {
		pr_err("%s:%d CR Failed\n", __func__, __LINE__);
		goto err;
	}

	/* check for CR and EQ */
	ret = dp_panel_train(panel, params, true);
	if (ret == false) {
		pr_err("%s:%d EQ failed\n", __func__, __LINE__);
		goto err;
	}

	link_config[0] = DP_TRAINING_PATTERN_DISABLE;
	dp_panel_set_dpcd(panel, DP_TRAINING_PATTERN_SET,
			link_config, 1);
	vlv_set_link_pattern(panel->pipeline,
			DP_PORT_IDLE_PATTERN_SET);
	vlv_set_link_pattern(panel->pipeline,
			DP_TRAINING_PATTERN_DISABLE);

err:
	return ret;

}

/* dp_panel_init : assume that panel is connected when called */
bool dp_panel_init(struct dp_panel *panel, struct intel_pipeline *pipeline)
{
	int err = 0;

	panel->pipeline = pipeline;
	vlv_get_max_vswing_preemp(pipeline, &panel->max_vswing,
			&panel->max_preemp);

	/* read first 11 bytes from DPCD */
	err = dp_panel_get_dpcd(panel, DP_DPCD_REV,
			(u8 *)panel->dpcd_start, 11);
	pr_err("Received %d bytes for start panel %x %x\n", err,
			panel->dpcd_start[0], panel->dpcd_start[1]);

	return true;
}

bool dp_panel_destroy(struct dp_panel *panel)
{
	kfree(panel->edid);
	panel->edid = NULL;
	kfree(panel->modelist);
	panel->modelist = NULL;

	panel->preferred_mode = NULL;
	panel->screen_width_mm = 0;
	panel->screen_height_mm = 0;
	panel->video_code = 0;
	panel->no_probed_modes = 0;

	return true;
}
