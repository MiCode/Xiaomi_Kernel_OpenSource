/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include "sde_kms.h"
#include "sde_hdmi.h"
#include "hdmi.h"

struct sde_hdmi_bridge {
	struct drm_bridge base;
	struct hdmi *hdmi;
};
#define to_hdmi_bridge(x) container_of(x, struct sde_hdmi_bridge, base)

/* TX major version that supports scrambling */
#define HDMI_TX_SCRAMBLER_MIN_TX_VERSION 0x04
#define HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ 340000
#define HDMI_TX_SCRAMBLER_TIMEOUT_MSEC 200
/* default hsyncs for 4k@60 for 200ms */
#define HDMI_DEFAULT_TIMEOUT_HSYNC 28571

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

static int _sde_hdmi_bridge_ddc_clear_irq(struct hdmi *hdmi,
	char *what)
{
	u32 ddc_int_ctrl, ddc_status, in_use, timeout;
	u32 sw_done_mask = BIT(2);
	u32 sw_done_ack  = BIT(1);
	u32 in_use_by_sw = BIT(0);
	u32 in_use_by_hw = BIT(1);

	/* clear and enable interrutps */
	ddc_int_ctrl = sw_done_mask | sw_done_ack;

	hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL, ddc_int_ctrl);

	/* wait until DDC HW is free */
	timeout = 100;
	do {
		ddc_status = hdmi_read(hdmi, REG_HDMI_DDC_HW_STATUS);
		in_use = ddc_status & (in_use_by_sw | in_use_by_hw);
		if (in_use) {
			SDE_DEBUG("ddc is in use by %s, timeout(%d)\n",
				ddc_status & in_use_by_sw ? "sw" : "hw",
				timeout);
			udelay(100);
		}
	} while (in_use && --timeout);

	if (!timeout) {
		SDE_ERROR("%s: timedout\n", what);
		return -ETIMEDOUT;
	}

	return 0;
}

static int _sde_hdmi_bridge_scrambler_ddc_check_status(struct hdmi *hdmi)
{
	int rc = 0;
	u32 reg_val;

	/* check for errors and clear status */
	reg_val = hdmi_read(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_STATUS);

	if (reg_val & BIT(4)) {
		SDE_ERROR("ddc aborted\n");
		reg_val |= BIT(5);
		rc = -ECONNABORTED;
	}

	if (reg_val & BIT(8)) {
		SDE_ERROR("timed out\n");
		reg_val |= BIT(9);
		rc = -ETIMEDOUT;
	}

	if (reg_val & BIT(12)) {
		SDE_ERROR("NACK0\n");
		reg_val |= BIT(13);
		rc = -EIO;
	}

	if (reg_val & BIT(14)) {
		SDE_ERROR("NACK1\n");
		reg_val |= BIT(15);
		rc = -EIO;
	}

	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_STATUS, reg_val);

	return rc;
}

static void _sde_hdmi_bridge_scrambler_ddc_reset(struct hdmi *hdmi)
{
	u32 reg_val;

	/* clear ack and disable interrupts */
	reg_val = BIT(14) | BIT(9) | BIT(5) | BIT(1);
	hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL2, reg_val);

	/* Reset DDC timers */
	reg_val = BIT(0) | hdmi_read(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);

	reg_val = hdmi_read(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL);
	reg_val &= ~BIT(0);
	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_CTRL, reg_val);
}

static void _sde_hdmi_bridge_scrambler_ddc_disable(struct hdmi *hdmi)
{
	u32 reg_val;

	_sde_hdmi_bridge_scrambler_ddc_reset(hdmi);

	/* Disable HW DDC access to RxStatus register */
	reg_val = hdmi_read(hdmi, REG_HDMI_HW_DDC_CTRL);
	reg_val &= ~(BIT(8) | BIT(9));

	hdmi_write(hdmi, REG_HDMI_HW_DDC_CTRL, reg_val);
}

static int _sde_hdmi_bridge_scrambler_status_timer_setup(struct hdmi *hdmi,
		u32 timeout_hsync)
{
	u32 reg_val;
	int rc;

	_sde_hdmi_bridge_ddc_clear_irq(hdmi, "scrambler");

	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL,
			timeout_hsync);
	hdmi_write(hdmi, REG_HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL2,
			timeout_hsync);

	reg_val = hdmi_read(hdmi, REG_HDMI_DDC_INT_CTRL5);
	reg_val |= BIT(10);
	hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL5, reg_val);

	reg_val = hdmi_read(hdmi, REG_HDMI_DDC_INT_CTRL2);
	/* Trigger interrupt if scrambler status is 0 or DDC failure */
	reg_val |= BIT(10);
	reg_val &= ~(BIT(15) | BIT(16));
	reg_val |= BIT(16);
	hdmi_write(hdmi, REG_HDMI_DDC_INT_CTRL2, reg_val);

	/* Enable DDC access */
	reg_val = hdmi_read(hdmi, REG_HDMI_HW_DDC_CTRL);

	reg_val &= ~(BIT(8) | BIT(9));
	reg_val |= BIT(8);
	hdmi_write(hdmi, REG_HDMI_HW_DDC_CTRL, reg_val);

	/* WAIT for 200ms as per HDMI 2.0 standard for sink to respond */
	msleep(200);

	/* clear the scrambler status */
	rc = _sde_hdmi_bridge_scrambler_ddc_check_status(hdmi);
	if (rc)
		SDE_ERROR("scrambling ddc error %d\n", rc);

	_sde_hdmi_bridge_scrambler_ddc_disable(hdmi);

	return rc;
}

static int _sde_hdmi_bridge_setup_ddc_timers(struct hdmi *hdmi,
			u32 type, u32 to_in_num_lines)
{
	if (type >= HDMI_TX_DDC_TIMER_MAX) {
		SDE_ERROR("Invalid timer type %d\n", type);
		return -EINVAL;
	}

	switch (type) {
	case HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS:
		_sde_hdmi_bridge_scrambler_status_timer_setup(hdmi,
			to_in_num_lines);
		break;
	default:
		SDE_ERROR("%d type not supported\n", type);
		return -EINVAL;
	}

	return 0;
}

static inline int _sde_hdmi_bridge_get_timeout_in_hysnc(
	struct drm_display_mode *mode, u32 timeout_ms)
{
	/*
	 * pixel clock  = h_total * v_total * fps
	 * 1 sec = pixel clock number of pixels are transmitted.
	 * time taken by one line (h_total) = 1s / (v_total * fps).
	 * lines for give time = (time_ms * 1000) / (1000000 / (v_total * fps))
	 *                     = (time_ms * clock) / h_total
	 */

	return (timeout_ms * mode->clock / mode->htotal);
}

static int _sde_hdmi_bridge_setup_scrambler(struct hdmi *hdmi,
	struct drm_display_mode *mode)
{
	int rc = 0;
	int timeout_hsync;
	u32 reg_val = 0;
	u32 tmds_clock_ratio = 0;
	bool scrambler_on = false;

	if (!hdmi || !mode) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}

	/* Read HDMI version */
	reg_val = hdmi_read(hdmi, REG_HDMI_VERSION);
	reg_val = (reg_val & 0xF0000000) >> 28;
	/* Scrambling is supported from HDMI TX 4.0 */
	if (reg_val < HDMI_TX_SCRAMBLER_MIN_TX_VERSION) {
		DRM_INFO("scrambling not supported by tx\n");
		return 0;
	}

	if (mode->clock > HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ) {
		scrambler_on = true;
		tmds_clock_ratio = 1;
	}

	DRM_INFO("scrambler %s\n", scrambler_on ? "on" : "off");

	if (scrambler_on) {
		rc = sde_hdmi_scdc_write(hdmi,
			HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
			tmds_clock_ratio);
		if (rc) {
			SDE_ERROR("TMDS CLK RATIO ERR\n");
			return rc;
		}

		reg_val = hdmi_read(hdmi, REG_HDMI_CTRL);
		reg_val |= BIT(31); /* Enable Update DATAPATH_MODE */
		reg_val |= BIT(28); /* Set SCRAMBLER_EN bit */

		hdmi_write(hdmi, REG_HDMI_CTRL, reg_val);

		rc = sde_hdmi_scdc_write(hdmi,
			HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x1);
		if (rc) {
			SDE_ERROR("failed to enable scrambling\n");
			return rc;
		}

		/*
		 * Setup hardware to periodically check for scrambler
		 * status bit on the sink. Sink should set this bit
		 * with in 200ms after scrambler is enabled.
		 */
		timeout_hsync = _sde_hdmi_bridge_get_timeout_in_hysnc(
					mode,
					HDMI_TX_SCRAMBLER_TIMEOUT_MSEC);

		if (timeout_hsync <= 0) {
			SDE_ERROR("err in timeout hsync calc\n");
			timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
		}

		SDE_DEBUG("timeout for scrambling en: %d hsyncs\n",
			timeout_hsync);

		rc = _sde_hdmi_bridge_setup_ddc_timers(hdmi,
			HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS, timeout_hsync);
	} else {
		sde_hdmi_scdc_write(hdmi, HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x0);
		reg_val = hdmi_read(hdmi, REG_HDMI_CTRL);
		reg_val &= ~BIT(31); /* Disable Update DATAPATH_MODE */
		reg_val &= ~BIT(28); /* Unset SCRAMBLER_EN bit */
		hdmi_write(hdmi, REG_HDMI_CTRL, reg_val);
	}

	return rc;
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
		hdmi_audio_update(hdmi);
	}

	if (phy)
		phy->funcs->powerup(phy, hdmi->pixclock);

	sde_hdmi_set_mode(hdmi, true);

#ifdef CONFIG_DRM_MSM_HDCP
	if (hdmi->hdcp_ctrl)
		hdmi_hdcp_ctrl_on(hdmi->hdcp_ctrl);
#endif
}

static void _sde_hdmi_bridge_enable(struct drm_bridge *bridge)
{
}

static void _sde_hdmi_bridge_disable(struct drm_bridge *bridge)
{
}

static void _sde_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct hdmi_phy *phy = hdmi->phy;

#ifdef CONFIG_DRM_MSM_HDCP
	if (hdmi->hdcp_ctrl)
		hdmi_hdcp_ctrl_off(hdmi->hdcp_ctrl);
#endif

	DRM_DEBUG("power down");
	sde_hdmi_set_mode(hdmi, false);

	if (phy)
		phy->funcs->powerdown(phy);

	if (hdmi->power_on) {
		_sde_hdmi_bridge_power_off(bridge);
		hdmi->power_on = false;
		hdmi_audio_update(hdmi);
	}
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

	_sde_hdmi_bridge_setup_scrambler(hdmi, mode);

	hdmi_audio_update(hdmi);
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
