/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
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

#include "drm_edid.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_hdmi.h"
#include "hdmi.h"

struct sde_hdmi_bridge {
	struct drm_bridge base;
	struct hdmi *hdmi;
};
#define to_hdmi_bridge(x) container_of(x, struct sde_hdmi_bridge, base)

/* for AVI program */
#define HDMI_AVI_INFOFRAME_BUFFER_SIZE \
	(HDMI_INFOFRAME_HEADER_SIZE + HDMI_AVI_INFOFRAME_SIZE)
#define HDMI_VS_INFOFRAME_BUFFER_SIZE (HDMI_INFOFRAME_HEADER_SIZE + 6)
#define HDMI_SPD_INFOFRAME_BUFFER_SIZE \
	(HDMI_INFOFRAME_HEADER_SIZE + HDMI_SPD_INFOFRAME_SIZE)
#define HDMI_DEFAULT_VENDOR_NAME "unknown"
#define HDMI_DEFAULT_PRODUCT_NAME "msm"
#define LEFT_SHIFT_BYTE(x) ((x) << 8)
#define LEFT_SHIFT_WORD(x) ((x) << 16)
#define LEFT_SHIFT_24BITS(x) ((x) << 24)
#define HDMI_AVI_IFRAME_LINE_NUMBER 1
#define HDMI_VENDOR_IFRAME_LINE_NUMBER 3

void _sde_hdmi_bridge_destroy(struct drm_bridge *bridge)
{
}

static void _sde_hdmi_bridge_power_on(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	const struct hdmi_platform_config *config = hdmi->config;
	int i, ret;

	for (i = 0; i < config->pwr_reg_cnt; i++) {
		ret = regulator_enable(hdmi->pwr_regs[i]);
		if (ret) {
			SDE_ERROR("failed to enable pwr regulator: %s (%d)\n",
					config->pwr_reg_names[i], ret);
		}
	}

	if (config->pwr_clk_cnt > 0) {
		DRM_DEBUG("pixclock: %lu", hdmi->pixclock);
		ret = clk_set_rate(hdmi->pwr_clks[0], hdmi->pixclock);
		if (ret) {
			SDE_ERROR("failed to set pixel clk: %s (%d)\n",
					config->pwr_clk_names[0], ret);
		}
	}

	for (i = 0; i < config->pwr_clk_cnt; i++) {
		ret = clk_prepare_enable(hdmi->pwr_clks[i]);
		if (ret) {
			SDE_ERROR("failed to enable pwr clk: %s (%d)\n",
					config->pwr_clk_names[i], ret);
		}
	}
}

static void _sde_hdmi_bridge_power_off(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	const struct hdmi_platform_config *config = hdmi->config;
	int i, ret;

	/* Wait for vsync */
	msleep(20);

	for (i = 0; i < config->pwr_clk_cnt; i++)
		clk_disable_unprepare(hdmi->pwr_clks[i]);

	for (i = 0; i < config->pwr_reg_cnt; i++) {
		ret = regulator_disable(hdmi->pwr_regs[i]);
		if (ret) {
			SDE_ERROR("failed to disable pwr regulator: %s (%d)\n",
					config->pwr_reg_names[i], ret);
		}
	}
}

static void _sde_hdmi_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct hdmi_phy *phy = hdmi->phy;

	DRM_DEBUG("power up");

	if (!hdmi->power_on) {
		_sde_hdmi_bridge_power_on(bridge);
		hdmi->power_on = true;
	}

	if (phy)
		phy->funcs->powerup(phy, hdmi->pixclock);

	sde_hdmi_set_mode(hdmi, true);

	if (hdmi->hdcp_ctrl && hdmi->is_hdcp_supported)
		hdmi_hdcp_ctrl_on(hdmi->hdcp_ctrl);

	sde_hdmi_ack_state(hdmi->connector, EXT_DISPLAY_CABLE_CONNECT);
}

static void sde_hdmi_force_update_audio(struct drm_connector *connector,
	enum drm_connector_status status)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;

	if (display && display->non_pluggable) {
		display->ext_audio_data.intf_ops.hpd(display->ext_pdev,
				display->ext_audio_data.type,
				status,
				MSM_EXT_DISP_HPD_AUDIO);
	}
}

static void _sde_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;

	/* force update audio ops when there's no HPD event */
	sde_hdmi_force_update_audio(hdmi->connector,
		EXT_DISPLAY_CABLE_CONNECT);
}

static void _sde_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;

	/* force update audio ops when there's no HPD event */
	sde_hdmi_force_update_audio(hdmi->connector,
		EXT_DISPLAY_CABLE_DISCONNECT);
}

static void _sde_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct hdmi_phy *phy = hdmi->phy;

	if (hdmi->hdcp_ctrl && hdmi->is_hdcp_supported)
		hdmi_hdcp_ctrl_off(hdmi->hdcp_ctrl);

	sde_hdmi_audio_off(hdmi);

	DRM_DEBUG("power down");
	sde_hdmi_set_mode(hdmi, false);

	if (phy)
		phy->funcs->powerdown(phy);

	if (hdmi->power_on) {
		_sde_hdmi_bridge_power_off(bridge);
		hdmi->power_on = false;
	}

	sde_hdmi_ack_state(hdmi->connector, EXT_DISPLAY_CABLE_DISCONNECT);
}

static void _sde_hdmi_bridge_set_avi_infoframe(struct hdmi *hdmi,
	const struct drm_display_mode *mode)
{
	u8 avi_iframe[HDMI_AVI_INFOFRAME_BUFFER_SIZE] = {0};
	u8 *avi_frame = &avi_iframe[HDMI_INFOFRAME_HEADER_SIZE];
	u8 checksum;
	u32 reg_val;
	struct hdmi_avi_infoframe info;

	drm_hdmi_avi_infoframe_from_display_mode(&info, mode);
	hdmi_avi_infoframe_pack(&info, avi_iframe, sizeof(avi_iframe));
	checksum = avi_iframe[HDMI_INFOFRAME_HEADER_SIZE - 1];

	reg_val = checksum |
		LEFT_SHIFT_BYTE(avi_frame[0]) |
		LEFT_SHIFT_WORD(avi_frame[1]) |
		LEFT_SHIFT_24BITS(avi_frame[2]);
	hdmi_write(hdmi, REG_HDMI_AVI_INFO(0), reg_val);

	reg_val = avi_frame[3] |
		LEFT_SHIFT_BYTE(avi_frame[4]) |
		LEFT_SHIFT_WORD(avi_frame[5]) |
		LEFT_SHIFT_24BITS(avi_frame[6]);
	hdmi_write(hdmi, REG_HDMI_AVI_INFO(1), reg_val);

	reg_val = avi_frame[7] |
		LEFT_SHIFT_BYTE(avi_frame[8]) |
		LEFT_SHIFT_WORD(avi_frame[9]) |
		LEFT_SHIFT_24BITS(avi_frame[10]);
	hdmi_write(hdmi, REG_HDMI_AVI_INFO(2), reg_val);

	reg_val = avi_frame[11] |
		LEFT_SHIFT_BYTE(avi_frame[12]) |
		LEFT_SHIFT_24BITS(avi_iframe[1]);
	hdmi_write(hdmi, REG_HDMI_AVI_INFO(3), reg_val);

	/* AVI InfFrame enable (every frame) */
	hdmi_write(hdmi, REG_HDMI_INFOFRAME_CTRL0,
		hdmi_read(hdmi, REG_HDMI_INFOFRAME_CTRL0) | BIT(1) | BIT(0));

	reg_val = hdmi_read(hdmi, REG_HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F;
	reg_val |= HDMI_AVI_IFRAME_LINE_NUMBER;
	hdmi_write(hdmi, REG_HDMI_INFOFRAME_CTRL1, reg_val);
}

static void _sde_hdmi_bridge_set_vs_infoframe(struct hdmi *hdmi,
	const struct drm_display_mode *mode)
{
	u8 vs_iframe[HDMI_VS_INFOFRAME_BUFFER_SIZE] = {0};
	u32 reg_val;
	struct hdmi_vendor_infoframe info;
	int rc = 0;

	rc = drm_hdmi_vendor_infoframe_from_display_mode(&info, mode);
	if (rc < 0) {
		SDE_DEBUG("don't send vendor infoframe\n");
		return;
	}
	hdmi_vendor_infoframe_pack(&info, vs_iframe, sizeof(vs_iframe));

	reg_val = (info.s3d_struct << 24) | (info.vic << 16) |
			(vs_iframe[3] << 8) | (vs_iframe[7] << 5) |
			vs_iframe[2];
	hdmi_write(hdmi, REG_HDMI_VENSPEC_INFO0, reg_val);

	/* vendor specific info-frame enable (every frame) */
	hdmi_write(hdmi, REG_HDMI_INFOFRAME_CTRL0,
		hdmi_read(hdmi, REG_HDMI_INFOFRAME_CTRL0) | BIT(13) | BIT(12));

	reg_val = hdmi_read(hdmi, REG_HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F000000;
	reg_val |= (HDMI_VENDOR_IFRAME_LINE_NUMBER << 24);
	hdmi_write(hdmi, REG_HDMI_INFOFRAME_CTRL1, reg_val);
}

static void _sde_hdmi_bridge_set_spd_infoframe(struct hdmi *hdmi,
	const struct drm_display_mode *mode)
{
	u8 spd_iframe[HDMI_SPD_INFOFRAME_BUFFER_SIZE] = {0};
	u32 packet_payload, packet_control, packet_header;
	struct hdmi_spd_infoframe info;
	int i;

	/* Need to query vendor and product name from platform setup */
	hdmi_spd_infoframe_init(&info, HDMI_DEFAULT_VENDOR_NAME,
		HDMI_DEFAULT_PRODUCT_NAME);
	hdmi_spd_infoframe_pack(&info, spd_iframe, sizeof(spd_iframe));

	packet_header = spd_iframe[0]
			| LEFT_SHIFT_BYTE(spd_iframe[1] & 0x7f)
			| LEFT_SHIFT_WORD(spd_iframe[2] & 0x7f);
	hdmi_write(hdmi, REG_HDMI_GENERIC1_HDR, packet_header);

	for (i = 0; i < MAX_REG_HDMI_GENERIC1_INDEX; i++) {
		packet_payload = spd_iframe[3 + i * 4]
			| LEFT_SHIFT_BYTE(spd_iframe[4 + i * 4] & 0x7f)
			| LEFT_SHIFT_WORD(spd_iframe[5 + i * 4] & 0x7f)
			| LEFT_SHIFT_24BITS(spd_iframe[6 + i * 4] & 0x7f);
		hdmi_write(hdmi, REG_HDMI_GENERIC1(i), packet_payload);
	}

	packet_payload = (spd_iframe[27] & 0x7f)
			| LEFT_SHIFT_BYTE(spd_iframe[28] & 0x7f);
	hdmi_write(hdmi, REG_HDMI_GENERIC1(MAX_REG_HDMI_GENERIC1_INDEX),
		packet_payload);

	/*
	 * GENERIC1_LINE | GENERIC1_CONT | GENERIC1_SEND
	 * Setup HDMI TX generic packet control
	 * Enable this packet to transmit every frame
	 * Enable HDMI TX engine to transmit Generic packet 1
	 */
	packet_control = hdmi_read(hdmi, REG_HDMI_GEN_PKT_CTRL);
	packet_control |= ((0x1 << 24) | (1 << 5) | (1 << 4));
	hdmi_write(hdmi, REG_HDMI_GEN_PKT_CTRL, packet_control);
}

static void _sde_hdmi_bridge_mode_set(struct drm_bridge *bridge,
		 struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	int hstart, hend, vstart, vend;
	uint32_t frame_ctrl;

	mode = adjusted_mode;

	hdmi->pixclock = mode->clock * 1000;

	hstart = mode->htotal - mode->hsync_start;
	hend   = mode->htotal - mode->hsync_start + mode->hdisplay;

	vstart = mode->vtotal - mode->vsync_start - 1;
	vend   = mode->vtotal - mode->vsync_start + mode->vdisplay - 1;

	DRM_DEBUG(
		"htotal=%d, vtotal=%d, hstart=%d, hend=%d, vstart=%d, vend=%d",
		mode->htotal, mode->vtotal, hstart, hend, vstart, vend);

	hdmi_write(hdmi, REG_HDMI_TOTAL,
			HDMI_TOTAL_H_TOTAL(mode->htotal - 1) |
			HDMI_TOTAL_V_TOTAL(mode->vtotal - 1));

	hdmi_write(hdmi, REG_HDMI_ACTIVE_HSYNC,
			HDMI_ACTIVE_HSYNC_START(hstart) |
			HDMI_ACTIVE_HSYNC_END(hend));
	hdmi_write(hdmi, REG_HDMI_ACTIVE_VSYNC,
			HDMI_ACTIVE_VSYNC_START(vstart) |
			HDMI_ACTIVE_VSYNC_END(vend));

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		hdmi_write(hdmi, REG_HDMI_VSYNC_TOTAL_F2,
				HDMI_VSYNC_TOTAL_F2_V_TOTAL(mode->vtotal));
		hdmi_write(hdmi, REG_HDMI_VSYNC_ACTIVE_F2,
				HDMI_VSYNC_ACTIVE_F2_START(vstart + 1) |
				HDMI_VSYNC_ACTIVE_F2_END(vend + 1));
	} else {
		hdmi_write(hdmi, REG_HDMI_VSYNC_TOTAL_F2,
				HDMI_VSYNC_TOTAL_F2_V_TOTAL(0));
		hdmi_write(hdmi, REG_HDMI_VSYNC_ACTIVE_F2,
				HDMI_VSYNC_ACTIVE_F2_START(0) |
				HDMI_VSYNC_ACTIVE_F2_END(0));
	}

	frame_ctrl = 0;
	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		frame_ctrl |= HDMI_FRAME_CTRL_HSYNC_LOW;
	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		frame_ctrl |= HDMI_FRAME_CTRL_VSYNC_LOW;
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		frame_ctrl |= HDMI_FRAME_CTRL_INTERLACED_EN;
	DRM_DEBUG("frame_ctrl=%08x\n", frame_ctrl);
	hdmi_write(hdmi, REG_HDMI_FRAME_CTRL, frame_ctrl);

	/*
	 * Setup info frame
	 * Current drm_edid driver doesn't have all CEA formats defined in
	 * latest CEA-861(CTA-861) spec. So, don't check if mode is CEA mode
	 * in here. Once core framework is updated, the check needs to be
	 * added back.
	 */
	if (hdmi->hdmi_mode) {
		_sde_hdmi_bridge_set_avi_infoframe(hdmi, mode);
		_sde_hdmi_bridge_set_vs_infoframe(hdmi, mode);
		_sde_hdmi_bridge_set_spd_infoframe(hdmi, mode);
		DRM_DEBUG("hdmi setup info frame\n");
	}
}

static const struct drm_bridge_funcs _sde_hdmi_bridge_funcs = {
		.pre_enable = _sde_hdmi_bridge_pre_enable,
		.enable = _sde_hdmi_bridge_enable,
		.disable = _sde_hdmi_bridge_disable,
		.post_disable = _sde_hdmi_bridge_post_disable,
		.mode_set = _sde_hdmi_bridge_mode_set,
};


/* initialize bridge */
struct drm_bridge *sde_hdmi_bridge_init(struct hdmi *hdmi)
{
	struct drm_bridge *bridge = NULL;
	struct sde_hdmi_bridge *sde_hdmi_bridge;
	int ret;

	sde_hdmi_bridge = devm_kzalloc(hdmi->dev->dev,
			sizeof(*sde_hdmi_bridge), GFP_KERNEL);
	if (!sde_hdmi_bridge) {
		ret = -ENOMEM;
		goto fail;
	}

	sde_hdmi_bridge->hdmi = hdmi;

	bridge = &sde_hdmi_bridge->base;
	bridge->funcs = &_sde_hdmi_bridge_funcs;

	ret = drm_bridge_attach(hdmi->dev, bridge);
	if (ret)
		goto fail;

	return bridge;

fail:
	if (bridge)
		_sde_hdmi_bridge_destroy(bridge);

	return ERR_PTR(ret);
}
