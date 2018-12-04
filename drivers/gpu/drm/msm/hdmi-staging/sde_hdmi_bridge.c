/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

/*
 * Add these register definitions to support the latest chipsets. These
 * are derived from hdmi.xml.h and are going to be replaced by a chipset
 * based mask approach.
 */
#define SDE_HDMI_ACTIVE_HSYNC_START__MASK 0x00001fff
static inline uint32_t SDE_HDMI_ACTIVE_HSYNC_START(uint32_t val)
{
	return ((val) << HDMI_ACTIVE_HSYNC_START__SHIFT) &
		SDE_HDMI_ACTIVE_HSYNC_START__MASK;
}
#define SDE_HDMI_ACTIVE_HSYNC_END__MASK 0x1fff0000
static inline uint32_t SDE_HDMI_ACTIVE_HSYNC_END(uint32_t val)
{
	return ((val) << HDMI_ACTIVE_HSYNC_END__SHIFT) &
		SDE_HDMI_ACTIVE_HSYNC_END__MASK;
}

#define SDE_HDMI_ACTIVE_VSYNC_START__MASK 0x00001fff
static inline uint32_t SDE_HDMI_ACTIVE_VSYNC_START(uint32_t val)
{
	return ((val) << HDMI_ACTIVE_VSYNC_START__SHIFT) &
		SDE_HDMI_ACTIVE_VSYNC_START__MASK;
}
#define SDE_HDMI_ACTIVE_VSYNC_END__MASK 0x1fff0000
static inline uint32_t SDE_HDMI_ACTIVE_VSYNC_END(uint32_t val)
{
	return ((val) << HDMI_ACTIVE_VSYNC_END__SHIFT) &
		SDE_HDMI_ACTIVE_VSYNC_END__MASK;
}

#define SDE_HDMI_VSYNC_ACTIVE_F2_START__MASK 0x00001fff
static inline uint32_t SDE_HDMI_VSYNC_ACTIVE_F2_START(uint32_t val)
{
	return ((val) << HDMI_VSYNC_ACTIVE_F2_START__SHIFT) &
		SDE_HDMI_VSYNC_ACTIVE_F2_START__MASK;
}
#define SDE_HDMI_VSYNC_ACTIVE_F2_END__MASK 0x1fff0000
static inline uint32_t SDE_HDMI_VSYNC_ACTIVE_F2_END(uint32_t val)
{
	return ((val) << HDMI_VSYNC_ACTIVE_F2_END__SHIFT) &
		SDE_HDMI_VSYNC_ACTIVE_F2_END__MASK;
}

#define SDE_HDMI_TOTAL_H_TOTAL__MASK 0x00001fff
static inline uint32_t SDE_HDMI_TOTAL_H_TOTAL(uint32_t val)
{
	return ((val) << HDMI_TOTAL_H_TOTAL__SHIFT) &
		SDE_HDMI_TOTAL_H_TOTAL__MASK;
}

#define SDE_HDMI_TOTAL_V_TOTAL__MASK 0x1fff0000
static inline uint32_t SDE_HDMI_TOTAL_V_TOTAL(uint32_t val)
{
	return ((val) << HDMI_TOTAL_V_TOTAL__SHIFT) &
		SDE_HDMI_TOTAL_V_TOTAL__MASK;
}

#define SDE_HDMI_VSYNC_TOTAL_F2_V_TOTAL__MASK 0x00001fff
static inline uint32_t SDE_HDMI_VSYNC_TOTAL_F2_V_TOTAL(uint32_t val)
{
	return ((val) << HDMI_VSYNC_TOTAL_F2_V_TOTAL__SHIFT) &
		SDE_HDMI_VSYNC_TOTAL_F2_V_TOTAL__MASK;
}

struct sde_hdmi_bridge {
	struct drm_bridge base;
	struct hdmi *hdmi;
	struct sde_hdmi *display;
};
#define to_hdmi_bridge(x) container_of(x, struct sde_hdmi_bridge, base)

/* TX major version that supports scrambling */
#define HDMI_TX_SCRAMBLER_MIN_TX_VERSION 0x04
#define HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ 340000
#define HDMI_TX_SCRAMBLER_TIMEOUT_MSEC 200


#define HDMI_SPD_INFOFRAME_BUFFER_SIZE \
	(HDMI_INFOFRAME_HEADER_SIZE + HDMI_SPD_INFOFRAME_SIZE)
#define HDMI_DEFAULT_VENDOR_NAME "unknown"
#define HDMI_DEFAULT_PRODUCT_NAME "msm"
#define HDMI_AVI_IFRAME_LINE_NUMBER 1
#define HDMI_VENDOR_IFRAME_LINE_NUMBER 3

void _sde_hdmi_bridge_destroy(struct drm_bridge *bridge)
{
}

static void sde_hdmi_clear_hdr_info(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct drm_connector *connector = hdmi->connector;

	connector->hdr_eotf = SDE_HDMI_HDR_EOTF_NONE;
	connector->hdr_metadata_type_one = false;
	connector->hdr_max_luminance = SDE_HDMI_HDR_LUMINANCE_NONE;
	connector->hdr_avg_luminance = SDE_HDMI_HDR_LUMINANCE_NONE;
	connector->hdr_min_luminance = SDE_HDMI_HDR_LUMINANCE_NONE;
	connector->hdr_supported = false;
}

static void sde_hdmi_clear_colorimetry(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct drm_connector *connector = hdmi->connector;

	connector->color_enc_fmt = 0;
}

static void sde_hdmi_clear_vsdb_info(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct drm_connector *connector = hdmi->connector;

	connector->max_tmds_clock = 0;
	connector->latency_present[0] = false;
	connector->latency_present[1] = false;
	connector->video_latency[0] = false;
	connector->video_latency[1] = false;
	connector->audio_latency[0] = false;
	connector->audio_latency[1] = false;
}

static void sde_hdmi_clear_hf_vsdb_info(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct drm_connector *connector = hdmi->connector;

	connector->max_tmds_char = 0;
	connector->scdc_present = false;
	connector->rr_capable = false;
	connector->supports_scramble = false;
	connector->flags_3d = 0;
}

static void sde_hdmi_clear_vcdb_info(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct drm_connector *connector = hdmi->connector;

	connector->pt_scan_info = 0;
	connector->it_scan_info = 0;
	connector->ce_scan_info = 0;
	connector->rgb_qs = false;
	connector->yuv_qs = false;
}

static void sde_hdmi_clear_vsdbs(struct drm_bridge *bridge)
{
	/* Clear fields of HDMI VSDB */
	sde_hdmi_clear_vsdb_info(bridge);
	/* Clear fields of HDMI forum VSDB */
	sde_hdmi_clear_hf_vsdb_info(bridge);
}

static int _sde_hdmi_bridge_power_on(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	const struct hdmi_platform_config *config = hdmi->config;
	int i, ret = 0;
	struct sde_hdmi *display = sde_hdmi_bridge->display;

	if ((display->non_pluggable) && (!hdmi->power_on)) {
		ret = sde_hdmi_core_enable(display);
		if (ret) {
			SDE_ERROR("failed to enable HDMI core (%d)\n", ret);
			goto err_core_enable;
		}
	}

	for (i = 0; i < config->pwr_reg_cnt; i++) {
		ret = regulator_enable(hdmi->pwr_regs[i]);
		if (ret) {
			SDE_ERROR("failed to enable pwr regulator: %s (%d)\n",
					config->pwr_reg_names[i], ret);
			goto err_regulator_enable;
		}
	}

	if (config->pwr_clk_cnt > 0 && hdmi->pixclock) {
		DRM_DEBUG("pixclock: %lu", hdmi->pixclock);
		ret = clk_set_rate(hdmi->pwr_clks[0], hdmi->pixclock);
		if (ret) {
			pr_warn("failed to set pixclock: %s %ld (%d)\n",
				config->pwr_clk_names[0],
				hdmi->pixclock, ret);
		}
	}

	for (i = 0; i < config->pwr_clk_cnt; i++) {
		ret = clk_prepare_enable(hdmi->pwr_clks[i]);
		if (ret) {
			SDE_ERROR("failed to enable pwr clk: %s (%d)\n",
					config->pwr_clk_names[i], ret);
			goto err_prepare_enable;
		}
	}
	goto exit;

err_prepare_enable:
	for (i = 0; i < config->pwr_clk_cnt; i++)
		clk_disable_unprepare(hdmi->pwr_clks[i]);
err_regulator_enable:
	for (i = 0; i < config->pwr_reg_cnt; i++)
		regulator_disable(hdmi->pwr_regs[i]);
err_core_enable:
	if (display->non_pluggable)
		sde_hdmi_core_disable(display);
exit:
	return ret;
}

static int _sde_hdmi_bridge_power_off(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	const struct hdmi_platform_config *config = hdmi->config;
	struct sde_hdmi *display = sde_hdmi_bridge->display;
	int i, ret = 0;

	/* Wait for vsync */
	msleep(20);

	for (i = 0; i < config->pwr_clk_cnt; i++)
		clk_disable_unprepare(hdmi->pwr_clks[i]);

	for (i = 0; i < config->pwr_reg_cnt; i++) {
		ret = regulator_disable(hdmi->pwr_regs[i]);
		if (ret)
			SDE_ERROR("failed to disable pwr regulator: %s (%d)\n",
					config->pwr_reg_names[i], ret);
	}

	if (display->non_pluggable)
		sde_hdmi_core_disable(display);

	return ret;
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

static int _sde_hdmi_bridge_scrambler_status_timer_setup(struct hdmi *hdmi,
			u32 timeout_hsync)
{
	u32 reg_val;
	int rc;
	struct sde_connector *c_conn;
	struct drm_connector *connector = NULL;
	struct sde_hdmi *display;

	if (!hdmi) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}
	connector = hdmi->connector;
	c_conn = to_sde_connector(hdmi->connector);
	display = (struct sde_hdmi *)c_conn->display;

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

	_sde_hdmi_scrambler_ddc_disable((void *)display);

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

static int _sde_hdmi_bridge_setup_scrambler(struct hdmi *hdmi,
			struct drm_display_mode *mode)
{
	int rc = 0;
	int timeout_hsync;
	u32 reg_val = 0;
	u32 tmds_clock_ratio = 0;
	bool scrambler_on = false;
	struct sde_connector *c_conn;
	struct drm_connector *connector = NULL;
	struct sde_hdmi *display;

	if (!hdmi || !mode) {
		SDE_ERROR("invalid input\n");
		return -EINVAL;
	}
	connector = hdmi->connector;
	c_conn = to_sde_connector(hdmi->connector);
	display = (struct sde_hdmi *)c_conn->display;

	/* Read HDMI version */
	reg_val = hdmi_read(hdmi, REG_HDMI_VERSION);
	reg_val = (reg_val & 0xF0000000) >> 28;
	/* Scrambling is supported from HDMI TX 4.0 */
	if (reg_val < HDMI_TX_SCRAMBLER_MIN_TX_VERSION) {
		DRM_INFO("scrambling not supported by tx\n");
		return 0;
	}

	/* use actual clock instead of mode clock */
	if (hdmi->pixclock >
		HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ * HDMI_KHZ_TO_HZ) {
		scrambler_on = true;
		tmds_clock_ratio = 1;
	} else {
		tmds_clock_ratio = 0;
		scrambler_on = connector->supports_scramble;
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
		timeout_hsync = _sde_hdmi_get_timeout_in_hysnc(
						(void *)display,
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
		/* reset tmds clock ratio */
		rc = sde_hdmi_scdc_write(hdmi,
				HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
				tmds_clock_ratio);
		/* scdc write can fail if sink doesn't support SCDC */
		if (rc && connector->scdc_present)
			SDE_ERROR("SCDC present, TMDS clk ratio err\n");

		sde_hdmi_scdc_write(hdmi, HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x0);
		reg_val = hdmi_read(hdmi, REG_HDMI_CTRL);
		reg_val &= ~BIT(28); /* Unset SCRAMBLER_EN bit */
		hdmi_write(hdmi, REG_HDMI_CTRL, reg_val);
	}
	return rc;
}

static void _sde_hdmi_bridge_setup_deep_color(struct hdmi *hdmi)
{
	struct drm_connector *connector = hdmi->connector;
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;
	u32 hdmi_ctrl_reg, vbi_pkt_reg;

	SDE_DEBUG("Deep Color: %s\n", display->dc_enable ? "On" : "Off");

	if (display->dc_enable) {
		hdmi_ctrl_reg = hdmi_read(hdmi, REG_HDMI_CTRL);

		/* GC CD override */
		hdmi_ctrl_reg |= BIT(27);

		/* enable deep color for RGB888/YUV444/YUV420 30 bits */
		hdmi_ctrl_reg |= BIT(24);
		hdmi_write(hdmi, REG_HDMI_CTRL, hdmi_ctrl_reg);
		/* Enable GC_CONT and GC_SEND in General Control Packet
		 * (GCP) register so that deep color data is
		 * transmitted to the sink on every frame, allowing
		 * the sink to decode the data correctly.
		 *
		 * GC_CONT: 0x1 - Send GCP on every frame
		 * GC_SEND: 0x1 - Enable GCP Transmission
		 */
		vbi_pkt_reg = hdmi_read(hdmi, REG_HDMI_VBI_PKT_CTRL);
		vbi_pkt_reg |= BIT(5) | BIT(4);
		hdmi_write(hdmi, REG_HDMI_VBI_PKT_CTRL, vbi_pkt_reg);
	} else {
		hdmi_ctrl_reg = hdmi_read(hdmi, REG_HDMI_CTRL);

		/* disable GC CD override */
		hdmi_ctrl_reg &= ~BIT(27);
		/* disable deep color for RGB888/YUV444/YUV420 30 bits */
		hdmi_ctrl_reg &= ~BIT(24);
		hdmi_write(hdmi, REG_HDMI_CTRL, hdmi_ctrl_reg);

		/* disable the GC packet sending */
		vbi_pkt_reg = hdmi_read(hdmi, REG_HDMI_VBI_PKT_CTRL);
		vbi_pkt_reg &= ~(BIT(5) | BIT(4));
		hdmi_write(hdmi, REG_HDMI_VBI_PKT_CTRL, vbi_pkt_reg);
	}
}

static void _sde_hdmi_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct hdmi_phy *phy = hdmi->phy;
	struct sde_hdmi *display = sde_hdmi_bridge->display;

	DRM_DEBUG("power up");

	if (!hdmi->power_on) {
		if (_sde_hdmi_bridge_power_on(bridge)) {
			DEV_ERR("failed to power on bridge\n");
			return;
		}
		hdmi->power_on = true;
	}
	if (!display->skip_ddc)
		_sde_hdmi_bridge_setup_scrambler(hdmi, &display->mode);

	if (phy)
		phy->funcs->powerup(phy, hdmi->pixclock);

	sde_hdmi_set_mode(hdmi, true);

	if (hdmi->hdcp_ctrl && hdmi->is_hdcp_supported)
		hdmi_hdcp_ctrl_on(hdmi->hdcp_ctrl);

	mutex_lock(&display->display_lock);
	if (display->codec_ready)
		sde_hdmi_notify_clients(display, display->connected);
	else
		display->client_notify_pending = true;
	mutex_unlock(&display->display_lock);
}

static void sde_hdmi_update_hdcp_info(struct drm_connector *connector)
{
	void *fd = NULL;
	struct sde_hdcp_ops *ops = NULL;
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;

	if (!display) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	if (display->skip_ddc) {
		display->sink_hdcp22_support = false;
		display->hdcp22_present = false;
	} else {
		/* check first if hdcp2p2 is supported */
		fd = display->hdcp_feat_data[SDE_HDCP_2P2];
		if (fd)
			ops = sde_hdmi_hdcp2p2_start(fd);

		/* If ops is true, sink supports hdcp */
		if (ops)
			display->sink_hdcp22_support = true;

		if (ops && ops->feature_supported)
			display->hdcp22_present = ops->feature_supported(fd);
		else
			display->hdcp22_present = false;
	}
	/* if hdcp22_present is true, src supports hdcp 2p2 */
	if (display->hdcp22_present)
		display->src_hdcp22_support = true;

	if (!display->hdcp22_present) {
		if (display->hdcp1_use_sw_keys) {
			display->hdcp14_present =
				hdcp1_check_if_supported_load_app();
		}
		if (display->hdcp14_present) {
			fd = display->hdcp_feat_data[SDE_HDCP_1x];
			if (fd)
				ops = sde_hdcp_1x_start(fd);
		}
	}

	if (display->sink_hdcp22_support)
		display->sink_hdcp_ver = SDE_HDMI_HDCP_22;
	else
		display->sink_hdcp_ver = SDE_HDMI_HDCP_14;

	/* update internal data about hdcp */
	display->hdcp_data = fd;
	display->hdcp_ops = ops;
}

static void _sde_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct sde_hdmi *display = sde_hdmi_bridge->display;

	/* need to update hdcp info here to ensure right HDCP support*/
	sde_hdmi_update_hdcp_info(hdmi->connector);

	/* start HDCP authentication */
	sde_hdmi_start_hdcp(hdmi->connector);

	/* reset HDR state */
	display->curr_hdr_state = HDR_DISABLE;
}

static void _sde_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct sde_hdmi *display = sde_hdmi_bridge->display;
	struct sde_connector_state *c_state;

	mutex_lock(&display->display_lock);

	if (!bridge) {
		SDE_ERROR("Invalid params\n");
		mutex_unlock(&display->display_lock);
		return;
	}

	hdmi->connector->hdr_eotf = 0;
	hdmi->connector->hdr_metadata_type_one = 0;
	hdmi->connector->hdr_max_luminance = 0;
	hdmi->connector->hdr_avg_luminance = 0;
	hdmi->connector->hdr_min_luminance = 0;

	c_state = to_sde_connector_state(hdmi->connector->state);
	memset(&c_state->hdr_ctrl.hdr_meta,
		0, sizeof(c_state->hdr_ctrl.hdr_meta));
	c_state->hdr_ctrl.hdr_state = HDR_DISABLE;

	display->pll_update_enable = false;
	display->sink_hdcp_ver = SDE_HDMI_HDCP_NONE;
	display->sink_hdcp22_support = false;

	if (sde_hdmi_tx_is_hdcp_enabled(display))
		sde_hdmi_hdcp_off(display);

	sde_hdmi_clear_hdr_info(bridge);
	/* Clear HDMI VSDB blocks info */
	sde_hdmi_clear_vsdbs(bridge);
	/* Clear HDMI VCDB block info */
	sde_hdmi_clear_vcdb_info(bridge);
	/* Clear HDMI colorimetry data block info */
	sde_hdmi_clear_colorimetry(bridge);

	mutex_unlock(&display->display_lock);
}

static void _sde_hdmi_bridge_post_disable(struct drm_bridge *bridge)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct hdmi_phy *phy = hdmi->phy;
	struct sde_hdmi *display = sde_hdmi_bridge->display;

	sde_hdmi_notify_clients(display, display->connected);

	sde_hdmi_audio_off(hdmi);

	DRM_DEBUG("power down");
	sde_hdmi_set_mode(hdmi, false);

	if (phy)
		phy->funcs->powerdown(phy);

	/* HDMI teardown sequence */
	sde_hdmi_ctrl_reset(hdmi);

	if (hdmi->power_on) {
		_sde_hdmi_bridge_power_off(bridge);
		hdmi->power_on = false;
	}

	if (!display->non_pluggable) {
		/* Powering-on the controller for HPD */
		sde_hdmi_ctrl_cfg(hdmi, 1);
	}
}

static void _sde_hdmi_bridge_set_avi_infoframe(struct hdmi *hdmi,
	struct drm_display_mode *mode)
{
	u8 avi_iframe[HDMI_AVI_INFOFRAME_BUFFER_SIZE] = {0};
	u8 *avi_frame = &avi_iframe[HDMI_INFOFRAME_HEADER_SIZE];
	u8 checksum;
	u32 reg_val;
	u32 mode_fmt_flags = 0;
	struct hdmi_avi_infoframe info;
	struct drm_connector *connector;

	if (!hdmi || !mode) {
		SDE_ERROR("invalid input\n");
		return;
	}

	connector = hdmi->connector;

	if (!connector) {
		SDE_ERROR("invalid input\n");
		return;
	}

	/* Cache the format flags before clearing */
	mode_fmt_flags = mode->flags;
	/**
	 * Clear the RGB/YUV format flags before calling upstream API
	 * as the API also compares the flags and then returns a mode
	 */
	mode->flags &= ~SDE_DRM_MODE_FLAG_FMT_MASK;
	drm_hdmi_avi_infoframe_from_display_mode(&info, mode);
	/* Restore the format flags */
	mode->flags = mode_fmt_flags;

	if (mode->private_flags & MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420) {
		info.colorspace = HDMI_COLORSPACE_YUV420;
		/**
		 * If sink supports quantization select,
		 * override to full range
		 */
		if (connector->yuv_qs)
			info.ycc_quantization_range =
				HDMI_YCC_QUANTIZATION_RANGE_FULL;
	}

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

static inline void _sde_hdmi_save_mode(struct hdmi *hdmi,
	struct drm_display_mode *mode)
{
	struct sde_connector *c_conn = to_sde_connector(hdmi->connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;

	drm_mode_copy(&display->mode, mode);
}

static u32 _sde_hdmi_choose_best_format(struct hdmi *hdmi,
	struct drm_display_mode *mode)
{
	/*
	 * choose priority:
	 * 1. DC + RGB
	 * 2. DC + YUV
	 * 3. RGB
	 * 4. YUV
	 */
	int dc_format;
	struct drm_connector *connector = hdmi->connector;
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct sde_hdmi *display = (struct sde_hdmi *)c_conn->display;

	dc_format = sde_hdmi_sink_dc_support(connector, mode);
	if (dc_format & MSM_MODE_FLAG_RGB444_DC_ENABLE)
		return (MSM_MODE_FLAG_COLOR_FORMAT_RGB444
			| MSM_MODE_FLAG_RGB444_DC_ENABLE);
	else if (dc_format & MSM_MODE_FLAG_YUV420_DC_ENABLE)
		return (MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420
			| MSM_MODE_FLAG_YUV420_DC_ENABLE);
	else if (mode->flags & DRM_MODE_FLAG_SUPPORTS_RGB)
		return MSM_MODE_FLAG_COLOR_FORMAT_RGB444;
	else if (mode->flags & DRM_MODE_FLAG_SUPPORTS_YUV)
		return MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420;

	if (display && !display->non_pluggable)
		SDE_ERROR("Can't get available best display format\n");

	return MSM_MODE_FLAG_COLOR_FORMAT_RGB444;
}

static void _sde_hdmi_bridge_mode_set(struct drm_bridge *bridge,
		 struct drm_display_mode *mode,
		 struct drm_display_mode *adjusted_mode)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;
	struct sde_hdmi *display = sde_hdmi_bridge->display;
	int hstart, hend, vstart, vend;
	uint32_t frame_ctrl;
	u32 div = 0;

	mode = adjusted_mode;

	display->dc_enable = mode->private_flags &
				(MSM_MODE_FLAG_RGB444_DC_ENABLE |
				 MSM_MODE_FLAG_YUV420_DC_ENABLE);
	/* compute pixclock as per color format and bit depth */
	hdmi->pixclock = sde_hdmi_calc_pixclk(
				mode->clock * HDMI_KHZ_TO_HZ,
				mode->private_flags,
				display->dc_enable);
	SDE_DEBUG("Actual PCLK: %lu, Mode PCLK: %d\n",
		hdmi->pixclock, mode->clock);

	if (mode->private_flags & MSM_MODE_FLAG_COLOR_FORMAT_YCBCR420)
		div = 1;

	hstart = (mode->htotal - mode->hsync_start) >> div;
	hend   = (mode->htotal - mode->hsync_start + mode->hdisplay) >> div;

	vstart = mode->vtotal - mode->vsync_start - 1;
	vend   = mode->vtotal - mode->vsync_start + mode->vdisplay - 1;

	SDE_DEBUG(
		"htotal=%d, vtotal=%d, hstart=%d, hend=%d, vstart=%d, vend=%d",
		mode->htotal, mode->vtotal, hstart, hend, vstart, vend);

	hdmi_write(hdmi, REG_HDMI_TOTAL,
			SDE_HDMI_TOTAL_H_TOTAL((mode->htotal >> div) - 1) |
			SDE_HDMI_TOTAL_V_TOTAL(mode->vtotal - 1));

	hdmi_write(hdmi, REG_HDMI_ACTIVE_HSYNC,
			SDE_HDMI_ACTIVE_HSYNC_START(hstart) |
			SDE_HDMI_ACTIVE_HSYNC_END(hend));
	hdmi_write(hdmi, REG_HDMI_ACTIVE_VSYNC,
			SDE_HDMI_ACTIVE_VSYNC_START(vstart) |
			SDE_HDMI_ACTIVE_VSYNC_END(vend));

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		hdmi_write(hdmi, REG_HDMI_VSYNC_TOTAL_F2,
				SDE_HDMI_VSYNC_TOTAL_F2_V_TOTAL(mode->vtotal));
		hdmi_write(hdmi, REG_HDMI_VSYNC_ACTIVE_F2,
				SDE_HDMI_VSYNC_ACTIVE_F2_START(vstart + 1) |
				SDE_HDMI_VSYNC_ACTIVE_F2_END(vend + 1));
	} else {
		hdmi_write(hdmi, REG_HDMI_VSYNC_TOTAL_F2,
				SDE_HDMI_VSYNC_TOTAL_F2_V_TOTAL(0));
		hdmi_write(hdmi, REG_HDMI_VSYNC_ACTIVE_F2,
				SDE_HDMI_VSYNC_ACTIVE_F2_START(0) |
				SDE_HDMI_VSYNC_ACTIVE_F2_END(0));
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

	_sde_hdmi_save_mode(hdmi, mode);
	_sde_hdmi_bridge_setup_deep_color(hdmi);
}

static bool _sde_hdmi_bridge_mode_fixup(struct drm_bridge *bridge,
	 const struct drm_display_mode *mode,
	 struct drm_display_mode *adjusted_mode)
{
	struct sde_hdmi_bridge *sde_hdmi_bridge = to_hdmi_bridge(bridge);
	struct hdmi *hdmi = sde_hdmi_bridge->hdmi;

	/* Clear the private flags before assigning new one */
	adjusted_mode->private_flags = 0;

	adjusted_mode->private_flags |=
		_sde_hdmi_choose_best_format(hdmi, adjusted_mode);
	SDE_DEBUG("Adjusted mode private flags: 0x%x\n",
		  adjusted_mode->private_flags);

	return true;
}

void sde_hdmi_bridge_power_on(struct drm_bridge *bridge)
{
	_sde_hdmi_bridge_power_on(bridge);
}

static const struct drm_bridge_funcs _sde_hdmi_bridge_funcs = {
		.pre_enable = _sde_hdmi_bridge_pre_enable,
		.enable = _sde_hdmi_bridge_enable,
		.disable = _sde_hdmi_bridge_disable,
		.post_disable = _sde_hdmi_bridge_post_disable,
		.mode_set = _sde_hdmi_bridge_mode_set,
		.mode_fixup = _sde_hdmi_bridge_mode_fixup,
};


/* initialize bridge */
struct drm_bridge *sde_hdmi_bridge_init(struct hdmi *hdmi,
			struct sde_hdmi *display)
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
	sde_hdmi_bridge->display = display;

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
