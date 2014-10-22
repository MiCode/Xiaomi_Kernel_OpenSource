/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Jani Nikula <jani.nikula@intel.com>
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/i915_drm.h>
#include <linux/slab.h>
#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"
#include <linux/mfd/intel_soc_pmic.h>

/* the sub-encoders aka panel drivers */
static struct intel_dsi_device intel_dsi_devices[] = {
	{
		.panel_id = MIPI_DSI_GENERIC_PANEL_ID,
		.name = "vbt-generic-dsi-vid-mode-display",
		.dev_ops = &vbt_generic_dsi_display_ops,
	},
};

static void band_gap_reset(struct drm_i915_private *dev_priv)
{
	mutex_lock(&dev_priv->dpio_lock);

	vlv_flisdsi_write(dev_priv, 0x08, 0x0001);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0005);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0025);
	udelay(150);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0000);
	vlv_flisdsi_write(dev_priv, 0x08, 0x0000);

	mutex_unlock(&dev_priv->dpio_lock);
}

struct intel_dsi *intel_attached_dsi(struct drm_connector *connector)
{
	return container_of(intel_attached_encoder(connector),
			    struct intel_dsi, base);
}

static void intel_dsi_hot_plug(struct intel_encoder *encoder)
{
	DRM_DEBUG_KMS("\n");
}

static bool intel_dsi_compute_config(struct intel_encoder *encoder,
				     struct intel_crtc_config *config)
{
	struct intel_dsi *intel_dsi = container_of(encoder, struct intel_dsi,
						   base);
	struct intel_connector *intel_connector = intel_dsi->attached_connector;
	struct drm_display_mode *fixed_mode = intel_connector->panel.fixed_mode;
	struct drm_display_mode *adjusted_mode = &config->adjusted_mode;
	struct drm_display_mode *mode = &config->requested_mode;
	struct intel_crtc *intel_crtc = encoder->new_crtc;
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	DRM_DEBUG_KMS("\n");

	if (fixed_mode)
		intel_fixed_panel_mode(fixed_mode, adjusted_mode);

	/*
	 * Panel native resolution and desired mode can be different in
	 * these two cases:
	 * 1. Generic driver specifies scaling reqd flag.
	 * 2. For restricted bandwidth devices
	 */
	if (dev_priv->scaling_reqd || i915.limitbw)  {
		intel_connector->panel.fitting_mode = AUTOSCALE;
		DRM_DEBUG("Enabling PF scaling required flag set\n");
	}

	if (IS_VALLEYVIEW(dev)) {
		intel_gmch_panel_fitting(intel_crtc, config,
			intel_connector->panel.fitting_mode);
	}

	if (intel_dsi->dev.dev_ops->mode_fixup)
		return intel_dsi->dev.dev_ops->mode_fixup(&intel_dsi->dev,
							  mode, adjusted_mode);

	return true;
}

static void intel_dsi_mode_set(struct intel_encoder *intel_encoder);
static void intel_dsi_pre_pll_enable(struct intel_encoder *encoder)
{
	DRM_DEBUG_KMS("\n");

	intel_dsi_mode_set(encoder);

	vlv_enable_dsi_pll(encoder);
}

static void intel_dsi_write_dev_rdy_on_A_and_C(struct intel_encoder *encoder,
								u32 val)
{
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	int pipe = intel_crtc->pipe;

	if (intel_dsi->dual_link) {
		I915_WRITE(MIPI_DEVICE_READY(PIPE_A), val);
		I915_WRITE(MIPI_DEVICE_READY(PIPE_B), val);
	} else
		I915_WRITE(MIPI_DEVICE_READY(pipe), val);
}

static void intel_dsi_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	int pipe = intel_crtc->pipe;
	u32 val;

	DRM_DEBUG_KMS("\n");

	mutex_lock(&dev_priv->dpio_lock);
	/* program rcomp for compliance, reduce from 50 ohms to 45 ohms
	 * needed everytime after power gate */
	vlv_flisdsi_write(dev_priv, 0x04, 0x0004);
	mutex_unlock(&dev_priv->dpio_lock);

	/* bandgap reset is needed after everytime we do power gate */
	band_gap_reset(dev_priv);

	I915_WRITE(MIPI_DEVICE_READY(pipe), ULPS_STATE_ENTER);
	usleep_range(2500, 3000);

	val = I915_READ(MIPI_PORT_CTRL(0));

	/*
	 * Putting delay of 180 phase shift for MIPI Port A & Port C
	 * Values given by SV team
	 */
	if (IS_CHERRYVIEW(dev_priv->dev) && STEP_TO(STEP_A2))
		val |= pipe ? DELAY_180_PHASE_SHIFT_MIPIC :
				DELAY_180_PHASE_SHIFT_MIPIA;

	intel_dsi_write_dev_rdy_on_A_and_C(encoder, DEVICE_READY |
							ULPS_STATE_ENTER);

	/* wait for LP state to go 00 */
	usleep_range(2000, 2500);
	I915_WRITE(MIPI_PORT_CTRL(0), val | LP_OUTPUT_HOLD);
	usleep_range(1000, 1500);
	intel_dsi_write_dev_rdy_on_A_and_C(encoder, DEVICE_READY |
							ULPS_STATE_EXIT);

	/* wait for LP state to goto 11 */
	usleep_range(2000, 2500);
	intel_dsi_write_dev_rdy_on_A_and_C(encoder, DEVICE_READY);

	/* wait for dsi controller hw enumeration */
	usleep_range(2000, 2500);
}

static void intel_dsi_port_enable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	int pipe = intel_crtc->pipe;
	u32 temp, port_control = 0;
	int count = 1;

	/* Configure the ports */

	temp = I915_READ(MIPI_PORT_CTRL(0));
	temp = temp | intel_dsi->port_bits;
	I915_WRITE(MIPI_PORT_CTRL(0), temp);

	if (intel_dsi->dual_link) {
		temp |= pipe ? LANE_CONFIGURATION_DUAL_LINK_B :
					LANE_CONFIGURATION_DUAL_LINK_A;

		port_control = (intel_dsi->dual_link - 1)
					<< DUAL_LINK_MODE_SHIFT;
		temp = temp | port_control;

		I915_WRITE(MIPI_PORT_CTRL(0), temp);

		if (pipe && IS_CHERRYVIEW(dev_priv->dev) && STEP_TO(STEP_B3)) {
			intel_dsi->port_ctrl_reg_val |= port_control;
			I915_WRITE(MIPI_PORT_CTRL(1),
						intel_dsi->port_ctrl_reg_val);
		} else {
			temp = I915_READ(MIPI_PORT_CTRL(1));
			temp = temp | port_control;
			I915_WRITE(MIPI_PORT_CTRL(1), temp);
		}

		if ((intel_dsi->dual_link & MIPI_DUAL_LINK_FRONT_BACK) &&
			((IS_CHERRYVIEW(dev_priv->dev) && STEP_FROM(STEP_B0)) ||
			IS_VALLEYVIEW(dev)))
			I915_WRITE_BITS(VLV_CHICKEN_3, intel_dsi->pixel_overlap
					<< PIXEL_OVERLAP_CNT_SHIFT,
					PIXEL_OVERLAP_CNT_MASK);

		count = 2;
		pipe = PIPE_A;

	}

	/* Enable the ports */

	do {
		if (pipe && IS_CHERRYVIEW(dev_priv->dev) && STEP_TO(STEP_B3)) {
			intel_dsi->port_ctrl_reg_val |= is_cmd_mode(intel_dsi) ?
					TEARING_EFFECT_GPIO : DPI_ENABLE;
			I915_WRITE(MIPI_PORT_CTRL(pipe),
						intel_dsi->port_ctrl_reg_val);
		} else {
			temp = I915_READ(MIPI_PORT_CTRL(pipe));
			temp |= is_cmd_mode(intel_dsi) ?
					TEARING_EFFECT_GPIO : DPI_ENABLE;
			I915_WRITE(MIPI_PORT_CTRL(pipe), temp);
		}

		pipe = PIPE_B;
	} while (--count > 0);
}

static void intel_dsi_enable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	struct intel_connector *intel_connector = intel_dsi->attached_connector;
	int pipe = intel_crtc->pipe;

	DRM_DEBUG_KMS("\n");

	if (is_cmd_mode(intel_dsi))
		I915_WRITE(MIPI_MAX_RETURN_PKT_SIZE(pipe), 8 * 4);
	else {
		dpi_send_cmd(intel_dsi, TURN_ON, DPI_LP_MODE_EN);
		usleep_range(1500, 2000);
	}

	if (intel_dsi->dev.dev_ops->enable)
		intel_dsi->dev.dev_ops->enable(&intel_dsi->dev);

	wait_for_dsi_fifo_empty(intel_dsi);
	intel_dsi_port_enable(encoder);

	if (intel_dsi->backlight_on_delay >= 20)
		msleep(intel_dsi->backlight_on_delay);
	else
		usleep_range(intel_dsi->backlight_on_delay * 1000,
				(intel_dsi->backlight_on_delay * 1000) + 500);

	if (dev_priv->display.enable_backlight)
		dev_priv->display.enable_backlight(intel_connector);

}

static void intel_dsi_soc_power_on(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	/*  cabc disable */
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_VDDEN_GPIONC_9_PCONF0, 0x2000CC00);
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_VDDEN_GPIONC_9_PAD, 0x00000004);

	/* panel enable */
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PCONF0, 0x2000CC00);
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PAD, 0x00000005);
	udelay(500);

}

static void intel_dsi_pmic_power_on(struct intel_dsi_device *dsi)
{
	intel_soc_pmic_writeb(PMIC_PANEL_EN, 0x01);
}

static void intel_dsi_pre_enable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	enum pipe pipe = intel_crtc->pipe;
	u32 tmp;

	DRM_DEBUG_KMS("\n");

	if (intel_dsi->gem_obj == NULL && is_cmd_mode(intel_dsi)) {
		intel_dsi->gem_obj = i915_gem_alloc_object(dev, 4096);
		if (intel_dsi->gem_obj == NULL) {
			DRM_ERROR("Failed to allocate seqno page\n");
			return;
		}

		i915_gem_object_set_cache_level(intel_dsi->gem_obj,
							I915_CACHE_LLC);

		if (i915_gem_obj_ggtt_pin(intel_dsi->gem_obj, 4096, 0)) {
			DRM_ERROR("MIPI command buffer GTT pin failed");
			return;
		}

		intel_dsi->cmd_buff =
				kmap(sg_page(intel_dsi->gem_obj->pages->sgl));
		intel_dsi->cmd_buff_phy_addr = page_to_phys(
				sg_page(intel_dsi->gem_obj->pages->sgl));
	}

	/* Panel Enable */
	if (intel_dsi->dev.dev_ops->power_on)
		intel_dsi->dev.dev_ops->power_on(&intel_dsi->dev);

	msleep(intel_dsi->panel_on_delay);

	/* Disable DPOunit clock gating, can stall pipe
	 * and we need DPLL REFA always enabled */
	tmp = I915_READ(DPLL(pipe));
	tmp |= DPLL_REFA_CLK_ENABLE_VLV;
	I915_WRITE(DPLL(pipe), tmp);

	tmp = I915_READ(DSPCLK_GATE_D);
	tmp |= DPOUNIT_CLOCK_GATE_DISABLE;
	I915_WRITE(DSPCLK_GATE_D, tmp);

	/* put device in ready state */
	intel_dsi_device_ready(encoder);

	msleep(intel_dsi->panel_on_delay);

	if (intel_dsi->dev.dev_ops->panel_reset)
		intel_dsi->dev.dev_ops->panel_reset(&intel_dsi->dev);

	if (intel_dsi->dev.dev_ops->send_otp_cmds)
		intel_dsi->dev.dev_ops->send_otp_cmds(&intel_dsi->dev);

	if (is_cmd_mode(intel_dsi) && intel_dsi->dev.dev_ops->tear_on)
		intel_dsi->dev.dev_ops->tear_on(&intel_dsi->dev);

	wait_for_dsi_fifo_empty(intel_dsi);

	/* Enable port in pre-enable phase itself because as per hw team
	 * recommendation, port should be enabled befor plane & pipe */
	intel_dsi_enable(encoder);
}

static void intel_dsi_enable_nop(struct intel_encoder *encoder)
{
	DRM_DEBUG_KMS("\n");

	/* for DSI port enable has to be done before pipe
	 * and plane enable, so port enable is done in
	 * pre_enable phase itself unlike other encoders
	 */
}

static void intel_dsi_pre_disable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	struct intel_connector *intel_connector = intel_dsi->attached_connector;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	int pipe = intel_crtc->pipe;

	DRM_DEBUG_KMS("\n");

	if (is_cmd_mode(intel_dsi)) {
		dev->driver->disable_vblank(dev, pipe);

		/*
		 * Make sure that the last frame is sent otherwise pipe can get
		 * stuck. Currently providing delay time for ~2 vblanks
		 * assuming 60fps.
		 */
		mdelay(40);
	}

	if (dev_priv->display.disable_backlight)
		dev_priv->display.disable_backlight(intel_connector);

	if (intel_dsi->backlight_off_delay >= 20)
		msleep(intel_dsi->backlight_off_delay);
	else
		usleep_range(intel_dsi->backlight_off_delay * 1000,
				(intel_dsi->backlight_off_delay * 1000) + 500);

	if (is_vid_mode(intel_dsi)) {
		/* Send Shutdown command to the panel in LP mode */
		dpi_send_cmd(intel_dsi, SHUTDOWN, DPI_LP_MODE_EN);
		usleep_range(2000, 2500);
	}
}

static void intel_dsi_port_disable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	int pipe = intel_crtc->pipe;

	wait_for_dsi_fifo_empty(intel_dsi);

	dev_priv->video_disabled = false;

	if (is_cmd_mode(intel_dsi) && pipe && IS_CHERRYVIEW(dev_priv->dev)
						&& STEP_TO(STEP_B3)) {
		static int once_done;
		if (once_done == false) {
			once_done = true;
			dev_priv->video_disabled = true;
		}
	} else if (is_cmd_mode(intel_dsi) &&
			(I915_READ(MIPI_PORT_CTRL(pipe)) & DPI_ENABLE))
		dev_priv->video_disabled = true;

	if (pipe && IS_CHERRYVIEW(dev_priv->dev) && STEP_TO(STEP_B3)) {
		/* cht hw issue that MIPI port C reg cannot be read */
		I915_WRITE(MIPI_PORT_CTRL(pipe), 0);
		intel_dsi->port_ctrl_reg_val = 0;
	} else {
		I915_WRITE(MIPI_PORT_CTRL(pipe), 0);
		POSTING_READ(MIPI_PORT_CTRL(pipe));
	}

	if (intel_dsi->dual_link) {
		I915_WRITE(MIPI_PORT_CTRL(pipe ? 0 : 1), 0);
		POSTING_READ(MIPI_PORT_CTRL(pipe ? 0 : 1));
	}
}

static void intel_dsi_disable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->base.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	int pipe = intel_crtc->pipe;
	u32 temp;
	int count = 1;

	DRM_DEBUG_KMS("\n");

	intel_dsi_port_disable(encoder);
	usleep_range(2000, 2500);

	if (intel_dsi->dual_link) {
		count = 2;
		pipe = PIPE_A;
	}

	do {
		/* Panel commands can be sent when clock is in LP11 */
		I915_WRITE(MIPI_DEVICE_READY(pipe), 0x0);

		temp = I915_READ(MIPI_CTRL(pipe));
		temp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
		I915_WRITE(MIPI_CTRL(pipe), temp |
				intel_dsi->escape_clk_div <<
				ESCAPE_CLOCK_DIVIDER_SHIFT);

		I915_WRITE(MIPI_EOT_DISABLE(pipe), CLOCKSTOP);

		temp = I915_READ(MIPI_DSI_FUNC_PRG(pipe));
		temp &= ~VID_MODE_FORMAT_MASK;
		I915_WRITE(MIPI_DSI_FUNC_PRG(pipe), temp);

		I915_WRITE(MIPI_DEVICE_READY(pipe), 0x1);

		/* For Port C for dual link */
		if (intel_dsi->dual_link)
			pipe = PIPE_B;
	} while (--count > 0);

	/*
	 * if disable packets are sent before sending shutdown packet
	 * then in some next enable sequence send turn on packet error
	 * is observed
	 */
	if (intel_dsi->dev.dev_ops->disable)
		intel_dsi->dev.dev_ops->disable(&intel_dsi->dev);

	wait_for_dsi_fifo_empty(intel_dsi);
}

static void intel_dsi_clear_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;
	u32 val;

	DRM_DEBUG_KMS("\n");

	intel_dsi_write_dev_rdy_on_A_and_C(encoder, ULPS_STATE_ENTER |
								DEVICE_READY);

	/* wait for LP state to go 00 */
	usleep_range(2000, 2500);

	intel_dsi_write_dev_rdy_on_A_and_C(encoder, ULPS_STATE_EXIT |
								DEVICE_READY);

	/* wait for LP state to goto 11 */
	usleep_range(2000, 2500);

	intel_dsi_write_dev_rdy_on_A_and_C(encoder, ULPS_STATE_ENTER |
								DEVICE_READY);

	/* wait for LP state to go 00 */
	usleep_range(2000, 2500);

	val = I915_READ(MIPI_PORT_CTRL(0));
	I915_WRITE(MIPI_PORT_CTRL(0), val & ~LP_OUTPUT_HOLD);

	/* wait to latch the state */
	usleep_range(1000, 1500);

	if (wait_for(((I915_READ(MIPI_PORT_CTRL(0)) & AFE_LATCHOUT)
					== 0x00000), 30))
		DRM_ERROR("DSI LP not going Low\n");

	intel_dsi_write_dev_rdy_on_A_and_C(encoder, 0x00);

	/* wait for dsi controller to be off */
	usleep_range(2000, 2500);

	vlv_disable_dsi_pll(encoder);
}

static void intel_dsi_soc_power_off(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PCONF0, 0x2000CC00);
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PAD, 0x00000004);
	udelay(500);
}

static void intel_dsi_pmic_power_off(struct intel_dsi_device *dsi)
{
	intel_soc_pmic_writeb(PMIC_PANEL_EN, 0x00);
}

static void intel_dsi_post_disable(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	u32 val;

	DRM_DEBUG_KMS("\n");

	intel_dsi_disable(encoder);

	intel_dsi_clear_device_ready(encoder);

	val = I915_READ(DSPCLK_GATE_D);
	val &= ~DPOUNIT_CLOCK_GATE_DISABLE;
	I915_WRITE(DSPCLK_GATE_D, val);

	if (intel_dsi->dev.dev_ops->disable_panel_power)
		intel_dsi->dev.dev_ops->disable_panel_power(&intel_dsi->dev);

	/* Disable Panel */
	if (intel_dsi->dev.dev_ops->power_off)
		intel_dsi->dev.dev_ops->power_off(&intel_dsi->dev);

	msleep(intel_dsi->panel_off_delay);
	msleep(intel_dsi->panel_pwr_cycle_delay);

	if (intel_dsi->gem_obj != NULL) {
		kunmap(intel_dsi->cmd_buff);
		i915_gem_object_ggtt_unpin(intel_dsi->gem_obj);
		drm_gem_object_unreference(&intel_dsi->gem_obj->base);
	}
}

static bool intel_dsi_get_hw_state(struct intel_encoder *encoder,
				   enum pipe *pipe)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;
	enum intel_display_power_domain power_domain;
	u32 dsi_status, func;
	enum pipe p;

	DRM_DEBUG_KMS("\n");

	power_domain = intel_display_port_power_domain(encoder);
	if (!intel_display_power_enabled(dev_priv, power_domain)) {
		DRM_ERROR("Domain power not enabled\n");
		return false;
	}

	/* XXX: this only works for one DSI output */
	for (p = PIPE_A; p <= PIPE_B; p++) {
		if (IS_CHERRYVIEW(dev) &&
				STEP_BETWEEN_INCLUSIVE(STEP_A0, STEP_B3) &&
				(MIPI_PORT_CTRL(p) == _MIPIB_PORT_CTRL))
			dsi_status = I915_READ(PIPECONF(PIPE_B)) &
							PIPECONF_ENABLE;
		else
			dsi_status = I915_READ(MIPI_PORT_CTRL(p)) & DPI_ENABLE;
		func = I915_READ(MIPI_DSI_FUNC_PRG(p));

		if (dsi_status || (func & CMD_MODE_DATA_WIDTH_MASK)) {
			if (I915_READ(MIPI_DEVICE_READY(p)) & DEVICE_READY) {
				*pipe = p;
				return true;
			}
		}
	}
	return false;
}

static void intel_dsi_get_config(struct intel_encoder *encoder,
				 struct intel_crtc_config *pipe_config)
{
	struct drm_i915_private *dev_priv = encoder->base.dev->dev_private;

	DRM_DEBUG_KMS("\n");

	/* XXX: read flags, set to adjusted_mode */
	pipe_config->quirks = 1;

	/* for MIPI we use DSI PLL, not DPLL */
	memset(&pipe_config->dpll_hw_state, 0,
		sizeof(pipe_config->dpll_hw_state));

	/* for now statically initialize dot clock */
	pipe_config->adjusted_mode.crtc_clock =
		dev_priv->vbt.lfp_lvds_vbt_mode->clock;
	pipe_config->port_clock = dev_priv->vbt.lfp_lvds_vbt_mode->clock;
}

void intel_dsi_update_panel_fb(struct intel_encoder *encoder)
{
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(&encoder->base);
	unsigned char uc_data[] = {MIPI_DCS_WRITE_MEMORY_START};

	dsi_send_dcs_cmd(intel_dsi, 0, uc_data, sizeof(uc_data), true);
}

static enum drm_mode_status
intel_dsi_mode_valid(struct drm_connector *connector,
		     struct drm_display_mode *mode)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);
	struct drm_display_mode *fixed_mode = intel_connector->panel.fixed_mode;
	struct intel_dsi *intel_dsi = intel_attached_dsi(connector);

	DRM_DEBUG_KMS("\n");

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		DRM_DEBUG_KMS("MODE_NO_DBLESCAN\n");
		return MODE_NO_DBLESCAN;
	}

	if (fixed_mode) {
		if (mode->hdisplay > fixed_mode->hdisplay)
			return MODE_PANEL;
		if (mode->vdisplay > fixed_mode->vdisplay)
			return MODE_PANEL;
	}

	return intel_dsi->dev.dev_ops->mode_valid(&intel_dsi->dev, mode);
}

/* return txclkesc cycles in terms of divider and duration in us */
static u16 txclkesc(u32 divider, unsigned int us)
{
	switch (divider) {
	case ESCAPE_CLOCK_DIVIDER_1:
	default:
		return 20 * us;
	case ESCAPE_CLOCK_DIVIDER_2:
		return 10 * us;
	case ESCAPE_CLOCK_DIVIDER_4:
		return 5 * us;
	}
}

/* return pixels in terms of txbyteclkhs */
static u32 txbyteclkhs(u32 pixels, int bpp, int lane_count,
					u16 burst_mode_ratio)
{
	return DIV_ROUND_UP_ULL((u64)pixels * bpp * burst_mode_ratio,
						8 * lane_count * 100);
}

static void set_dsi_timings(struct drm_encoder *encoder,
			    const struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	int pipe = intel_crtc->pipe;
	unsigned int bpp = intel_dsi->dsi_bpp;
	unsigned int lane_count = intel_dsi->lane_count;
	int count = 1;
	u16 mode_hactive;

	u16 hactive, hfp, hsync, hbp, vfp, vsync, vbp;

	hactive = mode->hdisplay;
	hfp = mode->hsync_start - mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;

	if (intel_dsi->dual_link) {
		hactive /= 2;
		if ((IS_CHERRYVIEW(dev_priv->dev) &&
			STEP_FROM(STEP_B0)) &&
			(intel_dsi->dual_link & MIPI_DUAL_LINK_FRONT_BACK))
			hactive += intel_dsi->pixel_overlap;
		hfp /= 2;
		hsync /= 2;
		hbp /= 2;

		count = 2;
		pipe = PIPE_A;
	}

	mode_hactive = hactive;
	vfp = mode->vsync_start - mode->vdisplay;
	vsync = mode->vsync_end - mode->vsync_start;
	vbp = mode->vtotal - mode->vsync_end;

	/* horizontal values are in terms of high speed byte clock */
	hactive = txbyteclkhs(hactive, bpp, lane_count,
					intel_dsi->burst_mode_ratio);
	hfp = txbyteclkhs(hfp, bpp, lane_count, intel_dsi->burst_mode_ratio);
	hsync = txbyteclkhs(hsync, bpp, lane_count,
					intel_dsi->burst_mode_ratio);
	hbp = txbyteclkhs(hbp, bpp, lane_count, intel_dsi->burst_mode_ratio);

	do {
		I915_WRITE(MIPI_HACTIVE_AREA_COUNT(pipe), hactive);
		I915_WRITE(MIPI_HFP_COUNT(pipe), hfp);

		/*
		 * meaningful for video mode non-burst sync pulse mode only,
		 * can be zero for non-burst sync events and burst modes
		 */
		I915_WRITE(MIPI_HSYNC_PADDING_COUNT(pipe), hsync);
		I915_WRITE(MIPI_HBP_COUNT(pipe), hbp);

		/* vertical values are in terms of lines */
		I915_WRITE(MIPI_VFP_COUNT(pipe), vfp);
		I915_WRITE(MIPI_VSYNC_PADDING_COUNT(pipe), vsync);
		I915_WRITE(MIPI_VBP_COUNT(pipe), vbp);

		I915_WRITE(MIPI_DPI_RESOLUTION(pipe),
			   mode->vdisplay << VERTICAL_ADDRESS_SHIFT |
			   mode_hactive << HORIZONTAL_ADDRESS_SHIFT);

		/* For Port C for dual link */
		if (intel_dsi->dual_link)
			pipe = PIPE_B;
	} while (--count > 0);
}

static void intel_dsi_mode_set(struct intel_encoder *intel_encoder)
{
	struct drm_encoder *encoder = &intel_encoder->base;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	struct drm_display_mode *adjusted_mode =
		&intel_crtc->config.adjusted_mode;
	int pipe = intel_crtc->pipe, count = 1;
	unsigned int bpp = intel_dsi->dsi_bpp;
	u32 val, tmp;

	DRM_DEBUG_KMS("pipe %c\n", pipe_name(pipe));

	if (intel_dsi->dual_link) {
		count = 2;
		pipe = PIPE_A;
	}

	do {
		/*
		 * escape clock divider, 20MHz, shared for A and C.
		 * device ready must be off when doing this! txclkesc?
		 */
		tmp = I915_READ(MIPI_CTRL(0));
		tmp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
		I915_WRITE(MIPI_CTRL(0), tmp | ESCAPE_CLOCK_DIVIDER_1);

		/* read request priority is per pipe */
		tmp = I915_READ(MIPI_CTRL(pipe));
		tmp &= ~READ_REQUEST_PRIORITY_MASK;
		I915_WRITE(MIPI_CTRL(pipe), tmp | READ_REQUEST_PRIORITY_HIGH);

		/* XXX: why here, why like this? handling in irq handler?! */
		I915_WRITE(MIPI_INTR_STAT(pipe), 0xffffffff);
		I915_WRITE(MIPI_INTR_EN(pipe), 0xffffffff);

		I915_WRITE(MIPI_DPHY_PARAM(pipe), intel_dsi->dphy_reg);

		/* For Port C for dual link */
		if (intel_dsi->dual_link)
			pipe = PIPE_B;
	} while (--count > 0);

	I915_WRITE(MIPI_DPI_RESOLUTION(pipe),
		   adjusted_mode->vdisplay << VERTICAL_ADDRESS_SHIFT |
		   adjusted_mode->hdisplay << HORIZONTAL_ADDRESS_SHIFT);

	if (is_vid_mode(intel_dsi))
		set_dsi_timings(encoder, adjusted_mode);

	if (intel_dsi->dual_link) {
		pipe = PIPE_A;
		count = 2;
	} else
		count = 1;

	do {
		val = intel_dsi->lane_count << DATA_LANES_PRG_REG_SHIFT;
		if (is_cmd_mode(intel_dsi)) {
			val |= intel_dsi->channel <<
					CMD_MODE_CHANNEL_NUMBER_SHIFT;
			val |= CMD_MODE_DATA_WIDTH_OPTION2;

			I915_WRITE(MIPI_DBI_FIFO_THROTTLE(pipe),
					DBI_FIFO_EMPTY_QUARTER);
			I915_WRITE(MIPI_HS_LP_DBI_ENABLE(pipe), 0);
		} else {
			val |= intel_dsi->channel <<
					VID_MODE_CHANNEL_NUMBER_SHIFT;

			/* XXX: cross-check bpp vs. pixel format? */
			val |= intel_dsi->pixel_format;
		}
		I915_WRITE(MIPI_DSI_FUNC_PRG(pipe), val);

		/*
		 * timeouts for recovery. one frame IIUC. if counter expires,
		 * EOT and stop state.
		 */

		/*
		 * In burst mode, value greater than one DPI line Time in byte
		 * clock (txbyteclkhs) To timeout this timer 1+ of the above
		 * said value is recommended.
		 *
		 * In non-burst mode, Value greater than one DPI frame time in
		 * byte clock(txbyteclkhs) To timeout this timer 1+ of the above
		 * said value is recommended.
		 *
		 * In DBI only mode, value greater than one DBI frame time in
		 * byte clock(txbyteclkhs) To timeout this timer 1+ of the above
		 * said value is recommended.
		 */

		if (is_vid_mode(intel_dsi) &&
			intel_dsi->video_mode_format == VIDEO_MODE_BURST) {
			I915_WRITE(MIPI_HS_TX_TIMEOUT(pipe),
					txbyteclkhs(adjusted_mode->htotal, bpp,
					intel_dsi->lane_count,
					intel_dsi->burst_mode_ratio) + 1);
		} else {
			I915_WRITE(MIPI_HS_TX_TIMEOUT(pipe),
					txbyteclkhs(adjusted_mode->vtotal *
					adjusted_mode->htotal,
					bpp, intel_dsi->lane_count,
					intel_dsi->burst_mode_ratio) + 1);
		}

		I915_WRITE(MIPI_LP_RX_TIMEOUT(pipe), intel_dsi->lp_rx_timeout);
		I915_WRITE(MIPI_TURN_AROUND_TIMEOUT(pipe),
					intel_dsi->turn_arnd_val);
		I915_WRITE(MIPI_DEVICE_RESET_TIMER(pipe),
						intel_dsi->rst_timer_val);

		/* dphy stuff */

		/* in terms of low power clock */
		I915_WRITE(MIPI_INIT_COUNT(pipe),
				txclkesc(intel_dsi->escape_clk_div, 100));

		val = 0;
		if (intel_dsi->eotp_pkt == 0)
			val |= EOT_DISABLE;

		if (intel_dsi->clock_stop)
			val |= CLOCKSTOP;

		/* recovery disables */
		I915_WRITE(MIPI_EOT_DISABLE(pipe), val);

		/* in terms of low power clock */
		I915_WRITE(MIPI_INIT_COUNT(pipe), intel_dsi->init_count);

		/*
		 * in terms of txbyteclkhs. actual high to low switch +
		 * MIPI_STOP_STATE_STALL * MIPI_LP_BYTECLK.
		 *
		 * XXX: write MIPI_STOP_STATE_STALL?
		 */
		I915_WRITE(MIPI_HIGH_LOW_SWITCH_COUNT(pipe),
				intel_dsi->hs_to_lp_count);

		/*
		 * XXX: low power clock equivalence in terms of byte clock.
		 * the number of byte clocks occupied in one low power clock.
		 * based on txbyteclkhs and txclkesc. txclkesc time / txbyteclk
		 * time * (105 + MIPI_STOP_STATE_STALL) / 105.???
		 */
		I915_WRITE(MIPI_LP_BYTECLK(pipe), intel_dsi->lp_byte_clk);

		/*
		 * the bw essential for transmitting 16 long packets containing
		 * 252 bytes meant for dcs write memory command is programmed
		 * in this register in terms of byte clocks. based on dsi
		 * transfer rate and the number of lanes configured the time
		 * taken to transmit 16 long packets in a dsi stream varies.
		 */
		I915_WRITE(MIPI_DBI_BW_CTRL(pipe), intel_dsi->bw_timer);

		I915_WRITE(MIPI_CLK_LANE_SWITCH_TIME_CNT(pipe),
		   intel_dsi->clk_lp_to_hs_count << LP_HS_SSW_CNT_SHIFT |
		   intel_dsi->clk_hs_to_lp_count << HS_LP_PWR_SW_CNT_SHIFT);

		if (is_vid_mode(intel_dsi)) {
			/*
			 * Some panels might have resolution which is not a
			 * multiple of 64 like 1366 x 768. Enable RANDOM
			 * resolution support for such panels by default
			  */
			I915_WRITE(MIPI_VIDEO_MODE_FORMAT(pipe),
				intel_dsi->video_frmt_cfg_bits |
				intel_dsi->video_mode_format |
				IP_TG_CONFIG |
				RANDOM_DPI_DISPLAY_RESOLUTION);
		} else
			I915_WRITE(MIPI_VIDEO_MODE_FORMAT(pipe),
					intel_dsi->video_frmt_cfg_bits |
					IP_TG_CONFIG |
					RANDOM_DPI_DISPLAY_RESOLUTION);


		/* For Port C for dual link */
		if (intel_dsi->dual_link)
			pipe = PIPE_B;

	} while (--count > 0);
}

static enum drm_connector_status
intel_dsi_detect(struct drm_connector *connector, bool force)
{
	struct intel_dsi *intel_dsi = intel_attached_dsi(connector);
	struct intel_encoder *intel_encoder = &intel_dsi->base;
	enum intel_display_power_domain power_domain;
	enum drm_connector_status connector_status;
	struct drm_i915_private *dev_priv =
		intel_encoder->base.dev->dev_private;

	DRM_DEBUG_KMS("\n");
	power_domain = intel_display_port_power_domain(intel_encoder);

	intel_display_power_get(dev_priv, power_domain);
	connector_status = intel_dsi->dev.dev_ops->detect(&intel_dsi->dev);
	intel_display_power_put(dev_priv, power_domain);

	return connector_status;
}

static int intel_dsi_get_modes(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);
	struct intel_dsi *intel_dsi = intel_attached_dsi(connector);
	struct drm_display_mode *mode;

	DRM_DEBUG_KMS("\n");

	if (!intel_connector->panel.fixed_mode) {
		DRM_DEBUG_KMS("no fixed mode\n");
		return 0;
	}

	mode = drm_mode_duplicate(connector->dev,
				  intel_connector->panel.fixed_mode);
	if (!mode) {
		DRM_DEBUG_KMS("drm_mode_duplicate failed\n");
		return 0;
	}

	drm_mode_probed_add(connector, mode);

	/*Fill the panel info here*/
	intel_dsi->dev.dev_ops->get_info(0, connector);
	return 1;
}

static void intel_dsi_destroy(struct drm_connector *connector)
{
	struct intel_connector *intel_connector = to_intel_connector(connector);

	DRM_DEBUG_KMS("\n");
	intel_panel_fini(&intel_connector->panel);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int intel_dsi_set_property(struct drm_connector *connector,
		struct drm_property *property,
		uint64_t val)
{
	struct intel_dsi *intel_dsi = intel_attached_dsi(connector);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;
	struct intel_connector *intel_connector = to_intel_connector(connector);
	struct intel_encoder *encoder = intel_connector->encoder;
	struct intel_crtc *intel_crtc = encoder->new_crtc;
	int ret;

	ret = drm_object_property_set_value(&connector->base, property, val);
	if (ret)
		return ret;

	if (property == dev_priv->force_pfit_property) {
		if (intel_connector->panel.fitting_mode == val)
			return 0;

		intel_connector->panel.fitting_mode = val;
		if (IS_VALLEYVIEW(dev_priv->dev)) {
			if (dev_priv->scaling_reqd || i915.limitbw) {
				if (intel_connector->panel.fitting_mode
					== PFIT_OFF)
					return 0;
			}
			intel_gmch_panel_fitting(intel_crtc,
				&intel_crtc->config,
				intel_connector->panel.fitting_mode);
			DRM_DEBUG_DRIVER("panel fitting mode = %u\n",
				intel_connector->panel.fitting_mode);
			return 0;
		} else
			goto done;
	}

	if (property == dev_priv->scaling_src_size_property) {
		intel_crtc->scaling_src_size = val;
		DRM_DEBUG_DRIVER("src size = %u", intel_crtc->scaling_src_size);
		return 0;
	}
done:
	if (intel_dsi->base.base.crtc)
		intel_crtc_restore_mode(intel_dsi->base.base.crtc);
	return 0;
}

static const struct drm_encoder_funcs intel_dsi_funcs = {
	.destroy = intel_encoder_destroy,
};

static const struct drm_connector_helper_funcs
	intel_dsi_connector_helper_funcs = {
	.get_modes = intel_dsi_get_modes,
	.mode_valid = intel_dsi_mode_valid,
	.best_encoder = intel_best_encoder,
};

static const struct drm_connector_funcs intel_dsi_connector_funcs = {
	.dpms = intel_connector_dpms,
	.detect = intel_dsi_detect,
	.destroy = intel_dsi_destroy,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = intel_dsi_set_property,
};

void intel_dsi_pmic_backlight_on(struct intel_dsi_device *dsi)
{
	intel_soc_pmic_writeb(PMIC_BKL_EN, 0xFF);
	intel_soc_pmic_writeb(PMIC_PWM_EN, 0x01);

	generic_enable_bklt(dsi);
}

void intel_dsi_soc_backlight_on(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PCONF0, 0x2000CC00);
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PAD, 0x00000005);
	udelay(500);
}

void intel_dsi_pmic_backlight_off(struct intel_dsi_device *dsi)
{
	generic_disable_bklt(dsi);

	intel_soc_pmic_writeb(PMIC_PWM_EN, 0x00);
	intel_soc_pmic_writeb(PMIC_BKL_EN, 0x7F);
}

void intel_dsi_soc_backlight_off(struct intel_dsi_device *dsi)
{
	struct intel_dsi *intel_dsi = container_of(dsi, struct intel_dsi, dev);
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PCONF0, 0x2000CC00);
	vlv_gpio_write(dev_priv, IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PAD, 0x00000004);
	udelay(500);
}

static void
intel_dsi_add_properties(struct intel_dsi *intel_dsi,
				struct drm_connector *connector)
{
	intel_attach_force_pfit_property(connector);
	intel_attach_scaling_src_size_property(connector);
}

bool intel_dsi_init(struct drm_device *dev)
{
	struct intel_dsi *intel_dsi;
	struct intel_encoder *intel_encoder;
	struct drm_encoder *encoder;
	struct intel_connector *intel_connector;
	struct drm_connector *connector;
	struct drm_display_mode *fixed_mode = NULL;
	struct drm_i915_private *dev_priv = dev->dev_private;
	const struct intel_dsi_device *dsi;
	unsigned int i;

	DRM_DEBUG_KMS("\n");

	/* There is no detection method for MIPI so rely on VBT */
	if (!dev_priv->vbt.has_mipi)
		return false;

	intel_dsi = kzalloc(sizeof(*intel_dsi), GFP_KERNEL);
	if (!intel_dsi)
		return false;

	intel_connector = kzalloc(sizeof(*intel_connector), GFP_KERNEL);
	if (!intel_connector) {
		kfree(intel_dsi);
		return false;
	}

	intel_encoder = &intel_dsi->base;
	encoder = &intel_encoder->base;
	intel_dsi->attached_connector = intel_connector;

	if (IS_VALLEYVIEW(dev)) {
		dev_priv->mipi_mmio_base = VLV_MIPI_BASE;
	} else {
		DRM_ERROR("Unsupported Mipi device to reg base");
		return false;
	}

	connector = &intel_connector->base;

	drm_encoder_init(dev, encoder, &intel_dsi_funcs, DRM_MODE_ENCODER_DSI);

	/* XXX: very likely not all of these are needed */
	intel_encoder->hot_plug = intel_dsi_hot_plug;
	intel_encoder->compute_config = intel_dsi_compute_config;
	intel_encoder->pre_pll_enable = intel_dsi_pre_pll_enable;
	intel_encoder->pre_enable = intel_dsi_pre_enable;
	intel_encoder->mode_set = intel_dsi_mode_set;
	intel_encoder->enable = intel_dsi_enable_nop;
	intel_encoder->disable = intel_dsi_pre_disable;
	intel_encoder->post_disable = intel_dsi_post_disable;
	intel_encoder->get_hw_state = intel_dsi_get_hw_state;
	intel_encoder->get_config = intel_dsi_get_config;

	intel_connector->get_hw_state = intel_connector_get_hw_state;
	intel_connector->unregister = intel_connector_unregister;

	for (i = 0; i < ARRAY_SIZE(intel_dsi_devices); i++) {
		dsi = &intel_dsi_devices[i];
		intel_dsi->dev = *dsi;

		if (dsi->dev_ops->init(&intel_dsi->dev))
			break;
	}

	if (i == ARRAY_SIZE(intel_dsi_devices)) {
		DRM_DEBUG_KMS("no device found\n");
		goto err;
	}

	intel_encoder->type = INTEL_OUTPUT_DSI;

	if (dev_priv->vbt.dsi.port == DVO_PORT_MIPIA)
		intel_encoder->crtc_mask = (1 << 0);
	else if (dev_priv->vbt.dsi.port == DVO_PORT_MIPIC)
		intel_encoder->crtc_mask = (1 << 1);

	intel_dsi->cmd_buff = NULL;
	intel_dsi->cmd_buff_phy_addr = 0;
	intel_dsi->gem_obj = NULL;

	intel_encoder->cloneable = 0;
	drm_connector_init(dev, connector, &intel_dsi_connector_funcs,
			   DRM_MODE_CONNECTOR_DSI);

	drm_connector_helper_add(connector, &intel_dsi_connector_helper_funcs);

	connector->display_info.subpixel_order = SubPixelHorizontalRGB; /*XXX*/
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	intel_dsi_add_properties(intel_dsi, connector);
	intel_connector_attach_encoder(intel_connector, intel_encoder);

	drm_connector_register(connector);

	fixed_mode = dsi->dev_ops->get_modes(&intel_dsi->dev);
	if (!fixed_mode) {
		DRM_DEBUG_KMS("no fixed mode\n");
		goto err;
	}

	if (dev_priv->vbt.dsi.seq_version < 3) {
		if (dev_priv->vbt.dsi.config->pmic_soc_blc) {
			intel_dsi->dev.dev_ops->power_on =
						intel_dsi_soc_power_on;
			intel_dsi->dev.dev_ops->power_off =
						intel_dsi_soc_power_off;
			intel_dsi->dev.dev_ops->enable_backlight =
						intel_dsi_soc_backlight_on;
			intel_dsi->dev.dev_ops->disable_backlight =
						intel_dsi_soc_backlight_off;

		} else {
			intel_dsi->dev.dev_ops->power_on =
						intel_dsi_pmic_power_on;
			intel_dsi->dev.dev_ops->power_off =
						intel_dsi_pmic_power_off;
			intel_dsi->dev.dev_ops->enable_backlight =
						intel_dsi_pmic_backlight_on;
			intel_dsi->dev.dev_ops->disable_backlight =
						intel_dsi_pmic_backlight_off;
		}
	}

	fixed_mode->type |= DRM_MODE_TYPE_PREFERRED;
	intel_panel_init(&intel_connector->panel, fixed_mode, NULL);
	intel_connector->panel.fitting_mode = 0;

	intel_panel_setup_backlight(connector);

	if (dev_priv->scaling_reqd || i915.limitbw)  {
		intel_connector->panel.fitting_mode = AUTOSCALE;
		DRM_DEBUG_DRIVER
			("Enabling pfit as scaling reqd flag set\n");
	}
	return true;

err:
	drm_encoder_cleanup(&intel_encoder->base);
	kfree(intel_dsi);
	kfree(intel_connector);

	return false;
}
