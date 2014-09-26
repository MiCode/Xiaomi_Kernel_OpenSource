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
 */

#include <drm/i915_drm.h>
#include <drm/i915_adf_wrapper.h>
#include <intel_adf_device.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_config.h>
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"

static void vlv_flisdsi_write(u32 reg, u32 val)
{
	intel_dpio_sideband_rw(INTEL_SIDEBAND_REG_WRITE, IOSF_PORT_FLISDSI,
			      reg, &val);
}

static void band_gap_reset(void)
{
	pr_err("ADF: %s\n", __func__);

	vlv_flisdsi_write(0x0F, 0x0001);
	vlv_flisdsi_write(0x0F, 0x0005);
	vlv_flisdsi_write(0x0F, 0x0025);
	udelay(150);
	vlv_flisdsi_write(0x0F, 0x0000);
	vlv_flisdsi_write(0x08, 0x0000);
}

static inline bool is_vid_mode(struct dsi_config *config)
{
	return config->ctx.operation_mode == DSI_DPI;
}

static inline bool is_cmd_mode(struct dsi_config *config)
{
	return config->ctx.operation_mode == DSI_DBI;
}

static void intel_dsi_device_ready(struct dsi_pipe *dsi_pipe)
{
	int pipe = dsi_pipe->config.pipe;
	u32 val;

	pr_err("ADF: %s\n", __func__);

	REG_WRITE(MIPI_DEVICE_READY(pipe), ULPS_STATE_ENTER);
	usleep_range(2500, 3000);

	val = REG_READ(MIPI_PORT_CTRL(pipe));
	REG_WRITE(MIPI_PORT_CTRL(pipe), val | LP_OUTPUT_HOLD);
	usleep_range(1000, 1500);

	REG_WRITE(MIPI_DEVICE_READY(pipe), ULPS_STATE_EXIT);
	usleep_range(2500, 3000);

	REG_WRITE(MIPI_DEVICE_READY(pipe), DEVICE_READY);
	usleep_range(2500, 3000);
}

static void intel_dsi_enable(struct dsi_pipe *dsi_pipe)
{
	struct dsi_config *config = &dsi_pipe->config;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	int pipe = config->pipe;
	struct dsi_panel *panel = dsi_pipe->panel;
	u32 temp;

	pr_debug("ADF: %s\n", __func__);
	if (is_cmd_mode(config))
		REG_WRITE(MIPI_MAX_RETURN_PKT_SIZE(pipe), 8 * 4);
	else {
		msleep(20); /* XXX */
		dpi_send_cmd(dsi_pipe, TURN_ON, DPI_LP_MODE_EN);
		msleep(100);

		if (panel->ops->power_on)
			panel->ops->power_on(dsi_pipe);

		wait_for_dsi_fifo_empty(dsi_pipe);

		/* assert ip_tg_enable signal */
		temp = REG_READ(MIPI_PORT_CTRL(pipe)) &
				~LANE_CONFIGURATION_MASK;
		temp = temp | intel_dsi->port_bits;
		REG_WRITE(MIPI_PORT_CTRL(pipe), temp | DPI_ENABLE);
		REG_POSTING_READ(MIPI_PORT_CTRL(pipe));
	}
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
static u16 txbyteclkhs(u16 pixels, int bpp, int lane_count,
		       u16 burst_mode_ratio)
{
	return DIV_ROUND_UP(DIV_ROUND_UP(pixels * bpp * burst_mode_ratio,
					 8 * 100), lane_count);
}

static void set_dsi_timings(struct dsi_pipe *dsi_pipe,
			    const struct drm_mode_modeinfo *mode)
{
	struct dsi_config *config = &dsi_pipe->config;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	int pipe = config->pipe;
	unsigned int bpp = config->bpp;
	unsigned int lane_count = intel_dsi->lane_count;
	u32 val;

	u16 hactive, hfp, hsync, hbp, vfp, vsync, vbp;

	pr_err("ADF: %s\n", __func__);

	/* DEVICE_READY[0] = 0 */
	val = REG_READ(MIPI_DEVICE_READY(pipe));
	val &= ~0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), val);

	hactive = mode->hdisplay;
	hfp = mode->hsync_start - mode->hdisplay;
	hsync = mode->hsync_end - mode->hsync_start;
	hbp = mode->htotal - mode->hsync_end;

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

	REG_WRITE(MIPI_HACTIVE_AREA_COUNT(pipe), hactive);
	REG_WRITE(MIPI_HFP_COUNT(pipe), hfp);

	/* meaningful for video mode non-burst sync pulse mode only, can be zero
	 * for non-burst sync events and burst modes */
	REG_WRITE(MIPI_HSYNC_PADDING_COUNT(pipe), hsync);
	REG_WRITE(MIPI_HBP_COUNT(pipe), hbp);

	/* vertical values are in terms of lines */
	REG_WRITE(MIPI_VFP_COUNT(pipe), vfp);
	REG_WRITE(MIPI_VSYNC_PADDING_COUNT(pipe), vsync);
	REG_WRITE(MIPI_VBP_COUNT(pipe), vbp);

	val = intel_dsi->lane_count << DATA_LANES_PRG_REG_SHIFT;
	if (is_cmd_mode(config)) {
		val |= intel_dsi->channel << CMD_MODE_CHANNEL_NUMBER_SHIFT;
		val |= CMD_MODE_DATA_WIDTH_8_BIT; /* XXX */
	} else {
		val |= intel_dsi->channel << VID_MODE_CHANNEL_NUMBER_SHIFT;

		/* XXX: cross-check bpp vs. pixel format? */
		val |= intel_dsi->pixel_format;
	}
	REG_WRITE(MIPI_DSI_FUNC_PRG(pipe), val);

	val = 0;
	if (intel_dsi->eotp_pkt == 0)
		val |= EOT_DISABLE;

	if (intel_dsi->clock_stop)
		val |= CLOCKSTOP;

	/* recovery disables */
	REG_WRITE(MIPI_EOT_DISABLE(pipe), val);

	/* DEVICE_READY[0] = 1 */
	val = REG_READ(MIPI_DEVICE_READY(pipe));
	val |= 0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), val);

	/* XXX: why here, why like this? handling in irq handler?! */
	REG_WRITE(MIPI_INTR_STAT(pipe), 0xffffffff);
	REG_WRITE(MIPI_INTR_EN(pipe), 0xffffffff);
}

int intel_dsi_modeset(struct dsi_pipe *dsi_pipe, struct drm_mode_modeinfo *mode)
{
	pr_err("ADF: %s\n", __func__);

	vlv_display_off(&dsi_pipe->base);

	return 0;
}

int intel_dsi_prepare(struct dsi_pipe *dsi_pipe, struct drm_mode_modeinfo *mode)
{
	struct dsi_config *config = &dsi_pipe->config;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	int pipe = config->pipe;
	unsigned int bpp = config->bpp;
	u32 val, tmp;

	pr_err("ADF: %s\n", __func__);

	val = REG_READ(MIPI_DEVICE_READY(pipe));
	val &= ~0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), val);

	/* escape clock divider, 20MHz, shared for A and C. device ready must be
	 * off when doing this! txclkesc? */
	tmp = REG_READ(MIPI_CTRL(0));
	tmp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
	REG_WRITE(MIPI_CTRL(0), tmp | ESCAPE_CLOCK_DIVIDER_1);

	/* read request priority is per pipe */
	tmp = REG_READ(MIPI_CTRL(pipe));
	tmp &= ~READ_REQUEST_PRIORITY_MASK;
	REG_WRITE(MIPI_CTRL(pipe), tmp | READ_REQUEST_PRIORITY_HIGH);

	REG_WRITE(MIPI_DPHY_PARAM(pipe), intel_dsi->dphy_reg);

	REG_WRITE(MIPI_DPI_RESOLUTION(pipe),
		   mode->vdisplay << VERTICAL_ADDRESS_SHIFT |
		   mode->hdisplay << HORIZONTAL_ADDRESS_SHIFT);

	/* timeouts for recovery. one frame IIUC. if counter expires, EOT and
	 * stop state. */

	/*
	 * In burst mode, value greater than one DPI line Time in byte clock
	 * (txbyteclkhs) To timeout this timer 1+ of the above said value is
	 * recommended.
	 *
	 * In non-burst mode, Value greater than one DPI frame time in byte
	 * clock(txbyteclkhs) To timeout this timer 1+ of the above said value
	 * is recommended.
	 *
	 * In DBI only mode, value greater than one DBI frame time in byte
	 * clock(txbyteclkhs) To timeout this timer 1+ of the above said value
	 * is recommended.
	 */

	if (is_vid_mode(config) &&
	    intel_dsi->video_mode_format == VIDEO_MODE_BURST) {
		REG_WRITE(MIPI_HS_TX_TIMEOUT(pipe),
			   txbyteclkhs(mode->htotal, bpp,
				       intel_dsi->lane_count,
				       intel_dsi->burst_mode_ratio) + 1);
	} else {
		REG_WRITE(MIPI_HS_TX_TIMEOUT(pipe),
			   txbyteclkhs(mode->vtotal *
				       mode->htotal,
				       bpp, intel_dsi->lane_count,
				       intel_dsi->burst_mode_ratio) + 1);
	}

	REG_WRITE(MIPI_HS_TX_TIMEOUT(pipe), 0x3FFFFF);
	REG_WRITE(MIPI_LP_RX_TIMEOUT(pipe), intel_dsi->lp_rx_timeout);
	REG_WRITE(MIPI_TURN_AROUND_TIMEOUT(pipe), intel_dsi->turn_arnd_val);
	REG_WRITE(MIPI_DEVICE_RESET_TIMER(pipe), intel_dsi->rst_timer_val);

	/* dphy stuff */

	/* in terms of low power clock */
	REG_WRITE(MIPI_INIT_COUNT(pipe),
				 txclkesc(intel_dsi->escape_clk_div, 100));

	/* in terms of low power clock */
	REG_WRITE(MIPI_INIT_COUNT(pipe), intel_dsi->init_count);

	/* in terms of txbyteclkhs. actual high to low switch +
	 * MIPI_STOP_STATE_STALL * MIPI_LP_BYTECLK.
	 *
	 * XXX: write MIPI_STOP_STATE_STALL?
	 */
	REG_WRITE(MIPI_HIGH_LOW_SWITCH_COUNT(pipe),
		   intel_dsi->hs_to_lp_count);

	/* XXX: low power clock equivalence in terms of byte clock. the number
	 * of byte clocks occupied in one low power clock. based on txbyteclkhs
	 * and txclkesc. txclkesc time / txbyteclk time * (105 +
	 * MIPI_STOP_STATE_STALL) / 105.???
	 */
	REG_WRITE(MIPI_LP_BYTECLK(pipe), intel_dsi->lp_byte_clk);

	/* the bw essential for transmitting 16 long packets containing 252
	 * bytes meant for dcs write memory command is programmed in this
	 * register in terms of byte clocks. based on dsi transfer rate and the
	 * number of lanes configured the time taken to transmit 16 long packets
	 * in a dsi stream varies. */
	REG_WRITE(MIPI_DBI_BW_CTRL(pipe), intel_dsi->bw_timer);

	REG_WRITE(MIPI_CLK_LANE_SWITCH_TIME_CNT(pipe),
		   intel_dsi->clk_lp_to_hs_count << LP_HS_SSW_CNT_SHIFT |
		   intel_dsi->clk_hs_to_lp_count << HS_LP_PWR_SW_CNT_SHIFT);

	if (is_vid_mode(config))
		/* Some panels might have resolution which is not a multiple of
		 * 64 like 1366 x 768. Enable RANDOM resolution support for such
		 * panels by default */
		REG_WRITE(MIPI_VIDEO_MODE_FORMAT(pipe),
			   intel_dsi->video_frmt_cfg_bits |
			   intel_dsi->video_mode_format |
			   IP_TG_CONFIG |
			   RANDOM_DPI_DISPLAY_RESOLUTION);

	REG_WRITE(MIPI_DSI_FUNC_PRG(pipe), 0);
	REG_WRITE(MIPI_EOT_DISABLE(pipe), CLOCKSTOP);

	val = REG_READ(MIPI_DEVICE_READY(pipe));
	val |= 0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), val);

	return 0;
}

static void intel_dsi_pre_pll_enable(struct dsi_pipe *dsi_pipe)
{
	pr_debug("ADF: %s\n", __func__);
	vlv_enable_dsi_pll(&dsi_pipe->config);
}

int intel_dsi_pre_enable(struct dsi_pipe *dsi_pipe)
{
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	struct dsi_panel *panel = dsi_pipe->panel;
	int pipe = dsi_pipe->config.pipe;
	u32 tmp;
	struct drm_mode_modeinfo mode;

	pr_err("ADF: %s\n", __func__);

	/* program rcomp for compliance, reduce from 50 ohms to 45 ohms
	 * needed everytime after power gate */
	vlv_flisdsi_write(0x04, 0x0004);

	/* bandgap reset is needed after everytime we do power gate */
	band_gap_reset();

	/*
	 * FIXME:
	 * Add panel power on using PMIC/I2C
	 */
	msleep(intel_dsi->panel_on_delay);

	if (panel->ops->reset)
		panel->ops->reset(dsi_pipe);

	/* Disable DPOunit clock gating, can stall pipe
	 * and we need DPLL REFA always enabled */
	tmp = REG_READ(DPLL(pipe));
	tmp |= DPLL_REFA_CLK_ENABLE_VLV;
	REG_WRITE(DPLL(pipe), tmp);

	tmp = REG_READ(DSPCLK_GATE_D);
	tmp |= DPOUNIT_CLOCK_GATE_DISABLE;
	REG_WRITE(DSPCLK_GATE_D, tmp);

	intel_dsi_pre_pll_enable(dsi_pipe);

	/* put device in ready state */
	intel_dsi_device_ready(dsi_pipe);

	/*
	 * FIXME:
	 * get the mode; works for DSI as only one mode
	 */
	panel->ops->get_config_mode(&dsi_pipe->config, &mode);

	intel_dsi_prepare(dsi_pipe, &mode);

	if (panel->ops->drv_ic_init)
		panel->ops->drv_ic_init(dsi_pipe);

	wait_for_dsi_fifo_empty(dsi_pipe);

	set_dsi_timings(dsi_pipe, &mode);

	/* Enable port in pre-enable phase itself because as per hw team
	 * recommendation, port should be enabled befor plane & pipe */
	intel_dsi_enable(dsi_pipe);

	return 0;
}

void intel_dsi_pre_disable(struct dsi_pipe *dsi_pipe)
{
	struct dsi_config *config = &dsi_pipe->config;

	pr_debug("ADF: %s\n", __func__);

	if (is_vid_mode(config)) {
		/* Send Shutdown command to the panel in LP mode */
		dpi_send_cmd(dsi_pipe, SHUTDOWN, DPI_LP_MODE_EN);
		usleep_range(10000, 10500);
		pr_err("ADF: %s: Sent DPI_SHUTDOWN\n", __func__);
	}
}

int intel_dsi_disable(struct dsi_pipe *dsi_pipe)
{
	struct dsi_config *config = &dsi_pipe->config;
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	struct dsi_panel *panel = dsi_pipe->panel;
	int pipe = dsi_pipe->config.pipe;
	u32 temp;

	pr_err("ADF: %s\n", __func__);

	if (is_vid_mode(config)) {
		wait_for_dsi_fifo_empty(dsi_pipe);

		/* de-assert ip_tg_enable signal */
		temp = REG_READ(MIPI_PORT_CTRL(pipe));
		REG_WRITE(MIPI_PORT_CTRL(pipe), temp & ~DPI_ENABLE);
		REG_POSTING_READ(MIPI_PORT_CTRL(pipe));

		usleep_range(2000, 2500);
	}

	/* Panel commands can be sent when clock is in LP11 */
	temp = REG_READ(MIPI_DEVICE_READY(pipe));
	temp &= ~0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), temp);

	temp = REG_READ(MIPI_CTRL(pipe));
	temp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
	REG_WRITE(MIPI_CTRL(pipe), temp |
		   intel_dsi->escape_clk_div <<
		   ESCAPE_CLOCK_DIVIDER_SHIFT);

	/*
	 * FIXME:
	 * Writing 0 here works but we get LP not going low error
	temp = REG_READ(MIPI_DSI_FUNC_PRG(pipe));
	temp &= ~VID_MODE_FORMAT_MASK;
	REG_WRITE(MIPI_DSI_FUNC_PRG(pipe), temp);
	*/
	REG_WRITE(MIPI_DSI_FUNC_PRG(pipe), 0);

	REG_WRITE(MIPI_EOT_DISABLE(pipe), CLOCKSTOP);

	temp = REG_READ(MIPI_DEVICE_READY(pipe));
	temp |= 0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), temp);

	/* if disable packets are sent before sending shutdown packet then in
	 * some next enable sequence send turn on packet error is observed */
	if (panel->ops->power_off)
		panel->ops->power_off(dsi_pipe);

	wait_for_dsi_fifo_empty(dsi_pipe);

	return 0;
}

static void intel_dsi_clear_device_ready(struct dsi_pipe *dsi_pipe)
{
	int pipe = dsi_pipe->config.pipe;
	u32 val;
	pr_debug("ADF: %s\n", __func__);

	REG_WRITE(MIPI_DEVICE_READY(pipe), DEVICE_READY | ULPS_STATE_ENTER);
	usleep_range(2000, 2500);

	REG_WRITE(MIPI_DEVICE_READY(pipe), DEVICE_READY | ULPS_STATE_EXIT);
	usleep_range(2000, 2500);

	REG_WRITE(MIPI_DEVICE_READY(pipe), DEVICE_READY | ULPS_STATE_ENTER);
	usleep_range(2000, 2500);

	if (wait_for(((REG_READ(MIPI_PORT_CTRL(pipe)) & AFE_LATCHOUT)
		      == 0x00000), 30))
		DRM_ERROR("DSI LP not going Low\n");

	val = REG_READ(MIPI_PORT_CTRL(pipe));
	REG_WRITE(MIPI_PORT_CTRL(pipe), val & ~LP_OUTPUT_HOLD);
	usleep_range(1000, 1500);

	val = REG_READ(MIPI_DEVICE_READY(pipe));
	val &= ~0x1;
	REG_WRITE(MIPI_DEVICE_READY(pipe), val);
	usleep_range(2000, 2500);

	vlv_disable_dsi_pll(&dsi_pipe->config);
}

int intel_dsi_post_disable(struct dsi_pipe *dsi_pipe)
{
	struct dsi_context *intel_dsi = &dsi_pipe->config.ctx;
	struct dsi_panel *panel = dsi_pipe->panel;
	u32 val;

	pr_debug("ADF: %s\n", __func__);
	intel_dsi_disable(dsi_pipe);

	intel_dsi_clear_device_ready(dsi_pipe);

	val = REG_READ(DSPCLK_GATE_D);
	val &= ~DPOUNIT_CLOCK_GATE_DISABLE;
	REG_WRITE(DSPCLK_GATE_D, val);

	if (panel->ops->disable_panel_power)
		panel->ops->disable_panel_power(dsi_pipe);

	msleep(intel_dsi->panel_off_delay);
	msleep(intel_dsi->panel_pwr_cycle_delay);

	return 0;
}

int intel_dsi_set_events(struct dsi_pipe *dsi_pipe, u8 event, bool enabled)
{
	return 0;
}

void intel_dsi_get_events(struct dsi_pipe *dsi_pipe, u32 *events)
{
	return;
}

void intel_dsi_handle_events(struct dsi_pipe *dsi_pipe, u32 events)
{
	return;
}

void intel_dsi_pre_post(struct dsi_pipe *dsi_pipe)
{
	return;
}
