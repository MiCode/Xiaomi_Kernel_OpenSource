/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include "dp_panel.h"

enum {
	DP_LINK_RATE_MULTIPLIER = 27000000,
};

struct dp_panel_private {
	struct device *dev;
	struct dp_panel dp_panel;
	struct dp_aux *aux;
	struct dp_catalog_panel *catalog;
};

static int dp_panel_read_dpcd(struct dp_panel *dp_panel)
{
	int rlen, rc = 0;
	struct dp_panel_private *panel;
	struct drm_dp_link *dp_link;
	u8 major = 0, minor = 0;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	dp_link = &dp_panel->dp_link;

	rlen = drm_dp_dpcd_read(panel->aux->drm_aux, DP_DPCD_REV,
		dp_panel->dpcd, (DP_RECEIVER_CAP_SIZE + 1));
	if (rlen < (DP_RECEIVER_CAP_SIZE + 1)) {
		pr_err("dpcd read failed, rlen=%d\n", rlen);
		rc = -EINVAL;
		goto end;
	}

	dp_link->revision = dp_panel->dpcd[DP_DPCD_REV];

	major = (dp_link->revision >> 4) & 0x0f;
	minor = dp_link->revision & 0x0f;
	pr_debug("version: %d.%d\n", major, minor);

	dp_link->rate =
		drm_dp_bw_code_to_link_rate(dp_panel->dpcd[DP_MAX_LINK_RATE]);
	pr_debug("link_rate=%d\n", dp_link->rate);

	dp_link->num_lanes = dp_panel->dpcd[DP_MAX_LANE_COUNT] &
			DP_MAX_LANE_COUNT_MASK;
	pr_debug("lane_count=%d\n", dp_link->num_lanes);

	if (dp_panel->dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP)
		dp_link->capabilities |= DP_LINK_CAP_ENHANCED_FRAMING;

end:
	return rc;
}

static int dp_panel_timing_cfg(struct dp_panel *dp_panel)
{
	int rc = 0;
	u32 data, total_ver, total_hor;
	struct dp_catalog_panel *catalog;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
	catalog = panel->catalog;
	pinfo = &panel->dp_panel.pinfo;

	pr_debug("width=%d hporch= %d %d %d\n",
		pinfo->h_active, pinfo->h_back_porch,
		pinfo->h_front_porch, pinfo->h_sync_width);

	pr_debug("height=%d vporch= %d %d %d\n",
		pinfo->v_active, pinfo->v_back_porch,
		pinfo->v_front_porch, pinfo->v_sync_width);

	total_hor = pinfo->h_active + pinfo->h_back_porch +
		pinfo->h_front_porch + pinfo->h_sync_width;

	total_ver = pinfo->v_active + pinfo->v_back_porch +
			pinfo->v_front_porch + pinfo->v_sync_width;

	data = total_ver;
	data <<= 16;
	data |= total_hor;

	catalog->total = data;

	data = (pinfo->v_back_porch + pinfo->v_sync_width);
	data <<= 16;
	data |= (pinfo->h_back_porch + pinfo->h_sync_width);

	catalog->sync_start = data;

	data = pinfo->v_sync_width;
	data <<= 16;
	data |= (pinfo->v_active_low << 31);
	data |= pinfo->h_sync_width;
	data |= (pinfo->h_active_low << 15);

	catalog->width_blanking = data;

	data = pinfo->v_active;
	data <<= 16;
	data |= pinfo->h_active;

	catalog->dp_active = data;

	panel->catalog->timing_cfg(catalog);
end:
	return rc;
}

static int dp_panel_edid_register(struct dp_panel *dp_panel)
{
	int rc = 0;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dp_panel->edid_ctrl = sde_edid_init();
	if (!dp_panel->edid_ctrl) {
		pr_err("sde edid init for DP failed\n");
		rc = -ENOMEM;
		goto end;
	}
end:
	return rc;
}

static void dp_panel_edid_deregister(struct dp_panel *dp_panel)
{
	if (!dp_panel) {
		pr_err("invalid input\n");
		return;
	}

	sde_edid_deinit((void **)&dp_panel->edid_ctrl);
}

static int dp_panel_init_panel_info(struct dp_panel *dp_panel)
{
	int rc = 0;
	struct dp_panel_private *panel;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);
end:
	return rc;
}

static u32 dp_panel_get_link_rate(struct dp_panel *dp_panel)
{
	const u32 encoding_factx10 = 8;
	const u32 ln_to_link_ratio = 10;
	u32 min_link_rate, reminder = 0;
	u32 calc_link_rate = 0, lane_cnt, max_rate = 0;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	lane_cnt = dp_panel->dp_link.num_lanes;
	max_rate = drm_dp_link_rate_to_bw_code(dp_panel->dp_link.rate);
	pinfo = &dp_panel->pinfo;

	pinfo->bpp = 24;

	/*
	 * The max pixel clock supported is 675Mhz. The
	 * current calculations below will make sure
	 * the min_link_rate is within 32 bit limits.
	 * Any changes in the section of code should
	 * consider this limitation.
	 */
	min_link_rate = (u32)div_u64(pinfo->pixel_clk_khz * 1000,
				(lane_cnt * encoding_factx10));
	min_link_rate /= ln_to_link_ratio;
	min_link_rate = (min_link_rate * pinfo->bpp);
	min_link_rate = (u32)div_u64_rem(min_link_rate * 10,
				DP_LINK_RATE_MULTIPLIER, &reminder);

	/*
	 * To avoid any fractional values,
	 * increment the min_link_rate
	 */
	if (reminder)
		min_link_rate += 1;
	pr_debug("min_link_rate = %d\n", min_link_rate);

	if (min_link_rate <= DP_LINK_BW_1_62)
		calc_link_rate = DP_LINK_BW_1_62;
	else if (min_link_rate <= DP_LINK_BW_2_7)
		calc_link_rate = DP_LINK_BW_2_7;
	else if (min_link_rate <= DP_LINK_BW_5_4)
		calc_link_rate = DP_LINK_BW_5_4;
	else if (min_link_rate <= DP_LINK_RATE_810)
		calc_link_rate = DP_LINK_RATE_810;
	else {
		/* Cap the link rate to the max supported rate */
		pr_debug("link_rate = %d is unsupported\n", min_link_rate);
		calc_link_rate = DP_LINK_RATE_810;
	}

	if (calc_link_rate > max_rate)
		calc_link_rate = max_rate;

	pr_debug("calc_link_rate = 0x%x\n", calc_link_rate);
end:
	return calc_link_rate;
}

struct dp_panel *dp_panel_get(struct device *dev, struct dp_aux *aux,
				struct dp_catalog_panel *catalog)
{
	int rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel *dp_panel;

	if (!dev || !aux || !catalog) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel) {
		rc = -ENOMEM;
		goto error;
	}

	panel->dev = dev;
	panel->aux = aux;
	panel->catalog = catalog;

	dp_panel = &panel->dp_panel;

	dp_panel->sde_edid_register = dp_panel_edid_register;
	dp_panel->sde_edid_deregister = dp_panel_edid_deregister;
	dp_panel->init_info = dp_panel_init_panel_info;
	dp_panel->timing_cfg = dp_panel_timing_cfg;
	dp_panel->read_dpcd = dp_panel_read_dpcd;
	dp_panel->get_link_rate = dp_panel_get_link_rate;

	return dp_panel;
error:
	return ERR_PTR(rc);
}

void dp_panel_put(struct dp_panel *dp_panel)
{
	struct dp_panel_private *panel;

	if (!dp_panel)
		return;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	devm_kfree(panel->dev, panel);
}
