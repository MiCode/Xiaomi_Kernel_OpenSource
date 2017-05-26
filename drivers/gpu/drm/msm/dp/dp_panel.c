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

#define DP_LINK_RATE_MULTIPLIER	27000000

struct dp_panel_private {
	struct device *dev;
	struct dp_panel dp_panel;
	struct dp_aux *aux;
	struct dp_catalog_panel *catalog;
};

static int dp_panel_read_dpcd(struct dp_panel *dp_panel)
{
	u8 *bp;
	u8 data;
	u32 const addr = 0x0;
	u32 const len = 16;
	int rlen, rc = 0;
	struct dp_panel_private *panel;
	struct dp_panel_dpcd *cap;

	if (!dp_panel) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	cap = &dp_panel->dpcd;
	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	rlen = panel->aux->read(panel->aux, addr, len, AUX_NATIVE, &bp);
	if (rlen != len) {
		pr_err("dpcd read failed, rlen=%d\n", rlen);
		rc = -EINVAL;
		goto end;
	}

	memset(cap, 0, sizeof(*cap));

	data = *bp++; /* byte 0 */
	cap->major = (data >> 4) & 0x0f;
	cap->minor = data & 0x0f;
	pr_debug("version: %d.%d\n", cap->major, cap->minor);

	data = *bp++; /* byte 1 */
	/* 162, 270, 540, 810 MB, symbol rate, NOT bit rate */
	cap->max_link_rate = data;
	pr_debug("link_rate=%d\n", cap->max_link_rate);

	data = *bp++; /* byte 2 */
	if (data & BIT(7))
		cap->enhanced_frame++;

	if (data & 0x40) {
		cap->flags |=  DPCD_TPS3;
		pr_debug("pattern 3 supported\n");
	} else {
		pr_debug("pattern 3 not supported\n");
	}

	data &= 0x0f;
	cap->max_lane_count = data;
	pr_debug("lane_count=%d\n", cap->max_lane_count);

	data = *bp++; /* byte 3 */
	if (data & BIT(0)) {
		cap->flags |= DPCD_MAX_DOWNSPREAD_0_5;
		pr_debug("max_downspread\n");
	}

	if (data & BIT(6)) {
		cap->flags |= DPCD_NO_AUX_HANDSHAKE;
		pr_debug("NO Link Training\n");
	}

	data = *bp++; /* byte 4 */
	cap->num_rx_port = (data & BIT(0)) + 1;
	pr_debug("rx_ports=%d", cap->num_rx_port);

	data = *bp++; /* Byte 5: DOWN_STREAM_PORT_PRESENT */
	cap->downstream_port.dfp_present = data & BIT(0);
	cap->downstream_port.dfp_type = data & 0x6;
	cap->downstream_port.format_conversion = data & BIT(3);
	cap->downstream_port.detailed_cap_info_available = data & BIT(4);
	pr_debug("dfp_present = %d, dfp_type = %d\n",
			cap->downstream_port.dfp_present,
			cap->downstream_port.dfp_type);
	pr_debug("format_conversion = %d, detailed_cap_info_available = %d\n",
			cap->downstream_port.format_conversion,
			cap->downstream_port.detailed_cap_info_available);

	bp += 1;	/* Skip Byte 6 */
	rlen -= 1;

	data = *bp++; /* Byte 7: DOWN_STREAM_PORT_COUNT */
	cap->downstream_port.dfp_count = data & 0x7;
	cap->downstream_port.msa_timing_par_ignored = data & BIT(6);
	cap->downstream_port.oui_support = data & BIT(7);
	pr_debug("dfp_count = %d, msa_timing_par_ignored = %d\n",
			cap->downstream_port.dfp_count,
			cap->downstream_port.msa_timing_par_ignored);
	pr_debug("oui_support = %d\n", cap->downstream_port.oui_support);

	data = *bp++; /* byte 8 */
	if (data & BIT(1)) {
		cap->flags |= DPCD_PORT_0_EDID_PRESENTED;
		pr_debug("edid presented\n");
	}

	data = *bp++; /* byte 9 */
	cap->rx_port0_buf_size = (data + 1) * 32;
	pr_debug("lane_buf_size=%d\n", cap->rx_port0_buf_size);

	bp += 2; /* skip 10, 11 port1 capability */
	rlen -= 2;

	data = *bp++;	/* byte 12 */
	cap->i2c_speed_ctrl = data;
	if (cap->i2c_speed_ctrl > 0)
		pr_debug("i2c_rate=%d", cap->i2c_speed_ctrl);

	data = *bp++;	/* byte 13 */
	cap->scrambler_reset = data & BIT(0);
	pr_debug("scrambler_reset=%d\n", cap->scrambler_reset);

	if (data & BIT(1))
		cap->enhanced_frame++;

	pr_debug("enhanced_framing=%d\n", cap->enhanced_frame);

	data = *bp++; /* byte 14 */
	if (data == 0)
		cap->training_read_interval = 4000; /* us */
	else
		cap->training_read_interval = 4000 * data; /* us */
	pr_debug("training_interval=%d\n", cap->training_read_interval);
end:
	return rc;
}

/*
 * edid standard header bytes
 */
static u8 edid_hdr[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

static bool dp_panel_is_edid_header_valid(u8 *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_hdr); i++) {
		if (buf[i] != edid_hdr[i])
			return false;
	}

	return true;
}

static int dp_panel_validate_edid(u8 *bp, int len)
{
	int i;
	u8 csum = 0;
	u32 const size = 128;

	if (len < size) {
		pr_err("Error: len=%x\n", len);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		csum += *bp++;

	if (csum != 0) {
		pr_err("error: csum=0x%x\n", csum);
		return -EINVAL;
	}

	return 0;
}

static int dp_panel_read_edid(struct dp_panel *dp_panel)
{
	u8 *edid_buf;
	u32 checksum = 0;
	int rlen, ret = 0;
	int edid_blk = 0, blk_num = 0, retries = 10;
	u32 const segment_addr = 0x30;
	bool edid_parsing_done = false;
	struct dp_panel_private *panel;

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	ret = panel->aux->ready(panel->aux);
	if (!ret) {
		pr_err("aux chan NOT ready\n");
		goto end;
	}

	do {
		u8 segment;


		/*
		 * Write the segment first.
		 * Segment = 0, for blocks 0 and 1
		 * Segment = 1, for blocks 2 and 3
		 * Segment = 2, for blocks 3 and 4
		 * and so on ...
		 */
		segment = blk_num >> 1;

		panel->aux->write(panel->aux, segment_addr, 1, AUX_I2C,
					&segment);

		rlen = panel->aux->read(panel->aux, EDID_START_ADDRESS +
				(blk_num * EDID_BLOCK_SIZE),
				EDID_BLOCK_SIZE, AUX_I2C, &edid_buf);
		if (rlen != EDID_BLOCK_SIZE) {
			pr_err("invalid edid len: %d\n", rlen);
			continue;
		}

		pr_debug("=== EDID data ===\n");
		print_hex_dump(KERN_DEBUG, "EDID: ", DUMP_PREFIX_NONE, 16, 1,
			edid_buf, EDID_BLOCK_SIZE, false);

		pr_debug("blk_num=%d, rlen=%d\n", blk_num, rlen);

		if (dp_panel_is_edid_header_valid(edid_buf)) {
			ret = dp_panel_validate_edid(edid_buf, rlen);
			if (ret) {
				pr_err("corrupt edid block detected\n");
				goto end;
			}

			if (edid_parsing_done) {
				blk_num++;
				continue;
			}

			dp_panel->edid.ext_block_cnt = edid_buf[0x7E];
			edid_parsing_done = true;
			checksum = edid_buf[rlen - 1];
		} else {
			edid_blk++;
			blk_num++;
		}

		memcpy(dp_panel->edid.buf + (edid_blk * EDID_BLOCK_SIZE),
			edid_buf, EDID_BLOCK_SIZE);

		if (edid_blk == dp_panel->edid.ext_block_cnt)
			goto end;
	} while (retries--);
end:
	return ret;
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

static u8 dp_panel_get_link_rate(struct dp_panel *dp_panel)
{
	const u32 encoding_factx10 = 8;
	const u32 ln_to_link_ratio = 10;
	u32 min_link_rate, reminder = 0;
	u8 calc_link_rate = 0, lane_cnt;
	struct dp_panel_private *panel;
	struct dp_panel_info *pinfo;

	if (!dp_panel) {
		pr_err("invalid input\n");
		goto end;
	}

	panel = container_of(dp_panel, struct dp_panel_private, dp_panel);

	lane_cnt = dp_panel->dpcd.max_lane_count;
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

	if (min_link_rate <= DP_LINK_RATE_162)
		calc_link_rate = DP_LINK_RATE_162;
	else if (min_link_rate <= DP_LINK_RATE_270)
		calc_link_rate = DP_LINK_RATE_270;
	else if (min_link_rate <= DP_LINK_RATE_540)
		calc_link_rate = DP_LINK_RATE_540;
	else if (min_link_rate <= DP_LINK_RATE_810)
		calc_link_rate = DP_LINK_RATE_810;
	else {
		/* Cap the link rate to the max supported rate */
		pr_debug("link_rate = %d is unsupported\n", min_link_rate);
		calc_link_rate = DP_LINK_RATE_810;
	}

	if (calc_link_rate > dp_panel->dpcd.max_link_rate)
		calc_link_rate = dp_panel->dpcd.max_link_rate;

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

	dp_panel->edid.buf = devm_kzalloc(dev,
				sizeof(EDID_BLOCK_SIZE) * 4, GFP_KERNEL);

	dp_panel->init_info = dp_panel_init_panel_info;
	dp_panel->timing_cfg = dp_panel_timing_cfg;
	dp_panel->read_edid = dp_panel_read_edid;
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

	devm_kfree(panel->dev, dp_panel->edid.buf);
	devm_kfree(panel->dev, panel);
}
