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
 */

#include <intel_adf_device.h>
#include <drm/drmP.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dsi_port.h>


u32 vlv_dsi_port_is_vid_mode(struct vlv_dsi_port *port)
{
	return port->operation_mode == DSI_DPI;
}

u32 vlv_dsi_port_is_cmd_mode(struct vlv_dsi_port *port)
{
	return port->operation_mode == DSI_DBI;
}

u32  vlv_dsi_port_wait_for_fifo_empty(struct vlv_dsi_port *port)
{
	u32 mask;

	mask = LP_CTRL_FIFO_EMPTY | HS_CTRL_FIFO_EMPTY |
			LP_DATA_FIFO_EMPTY | HS_DATA_FIFO_EMPTY;

	/* Wait for FIFOs to become empty */
	if (wait_for((REG_READ(port->fifo_stat_offset) & mask) == mask, 100))
		pr_err("ADF: %s DSI FIFO not empty\n", __func__);
	return 0;
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
	if (lane_count)
		return DIV_ROUND_UP(DIV_ROUND_UP(pixels * bpp *
				burst_mode_ratio, 8 * 100), lane_count);
	else
		return -EINVAL;
}

/* to be called before pll disable */
bool vlv_dsi_port_clear_device_ready(struct vlv_dsi_port *port)
{
	u32 val;

	REG_WRITE(port->dr_offset, DEVICE_READY | ULPS_STATE_ENTER);
	usleep_range(2000, 2500);

	REG_WRITE(port->dr_offset, DEVICE_READY | ULPS_STATE_EXIT);
	usleep_range(2000, 2500);

	REG_WRITE(port->dr_offset, DEVICE_READY | ULPS_STATE_ENTER);
	usleep_range(2000, 2500);

	val = REG_READ(port->offset);
	REG_WRITE(port->offset, val & ~LP_OUTPUT_HOLD);
	usleep_range(1000, 1500);

	if (wait_for(((REG_READ(port->offset) & AFE_LATCHOUT)
			== 0x00000), 30))
		pr_err("DSI LP not going Low\n");
	REG_WRITE(port->dr_offset, 0x00);
	usleep_range(2000, 2500);

	return true;
}

bool vlv_dsi_port_set_device_ready(struct vlv_dsi_port *port)
{
	u32 val;

	/* ULPS */
	REG_WRITE(port->dr_offset, DEVICE_READY | ULPS_STATE_ENTER);
	usleep_range(1000, 1500);

	val = REG_READ(port->offset);
	REG_WRITE(port->offset, val | LP_OUTPUT_HOLD);
	usleep_range(1000, 1500);

	if (wait_for(((REG_READ(port->offset) & AFE_LATCHOUT)
				== 0x00000), 30))
		pr_err("ADF: %s DSI LP not going Low\n", __func__);

	REG_WRITE(port->dr_offset, DEVICE_READY | ULPS_STATE_EXIT);
	usleep_range(2000, 2500);
	REG_WRITE(port->dr_offset, DEVICE_READY);
	usleep_range(2000, 2500);

	return true;
}


static void set_dsi_timings(struct vlv_dsi_port *port,
		struct drm_mode_modeinfo *mode,
		struct dsi_config *config)
{
	struct dsi_context *intel_dsi = &config->ctx;
	unsigned int bpp = config->bpp;
	unsigned int lane_count = intel_dsi->lane_count;
	u16 hactive, hfp, hsync, hbp, vfp, vsync, vbp;

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

	REG_WRITE(port->hactive_count_offset, hactive);
	REG_WRITE(port->hfp_count_offset, hfp);

	/*
	 * meaningful for video mode non-burst sync
	 * pulse mode only, can be zero
	 * for non-burst sync events and burst modes
	 */
	REG_WRITE(port->hsync_count_offset, hsync);
	REG_WRITE(port->hbp_count_offset, hbp);

	/* vertical values are in terms of lines */
	REG_WRITE(port->vfp_count_offset, vfp);
	REG_WRITE(port->vsync_count_offset, vsync);
	REG_WRITE(port->vbp_count_offset, vbp);

}

u32  vlv_dsi_port_prepare(struct vlv_dsi_port *port,
		struct drm_mode_modeinfo *mode,
		struct dsi_config *config)
{
	struct dsi_context *intel_dsi = &config->ctx;
	unsigned int bpp = config->bpp;
	u32 val, tmp;

	REG_WRITE(port->dr_offset, 0x0);
	usleep_range(2000, 2500);

	/*
	 * escape clock divider, 20MHz, shared for A and C. device ready must be
	 * off when doing this! txclkesc?
	 */
	/* FIXEME: check if this is applicable only for pipe A or not  */
	tmp = REG_READ(port->ctrl_offset);
	tmp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
	REG_WRITE(port->ctrl_offset, tmp | ESCAPE_CLOCK_DIVIDER_1);

	/* read request priority is per pipe */
	tmp = REG_READ(port->ctrl_offset);
	tmp &= ~READ_REQUEST_PRIORITY_MASK;
	REG_WRITE(port->ctrl_offset, tmp | READ_REQUEST_PRIORITY_HIGH);

	/* XXX: why here, why like this? handling in irq handler?! */
	/* FIXME: move these to common */
	REG_WRITE(MIPI_INTR_STAT(0), 0xffffffff);
	REG_WRITE(MIPI_INTR_EN(0), 0xffffffff);

	REG_WRITE(port->dphy_param_offset, intel_dsi->dphy_reg);

	REG_WRITE(port->dpi_res_offset,
		mode->vdisplay << VERTICAL_ADDRESS_SHIFT |
		mode->hdisplay << HORIZONTAL_ADDRESS_SHIFT);

	set_dsi_timings(port, mode, config);

	val = intel_dsi->lane_count << DATA_LANES_PRG_REG_SHIFT;
	if (vlv_dsi_port_is_cmd_mode(port)) {
		val |= intel_dsi->channel << CMD_MODE_CHANNEL_NUMBER_SHIFT;
		val |= CMD_MODE_DATA_WIDTH_8_BIT; /* XXX */
	} else {
		val |= intel_dsi->channel << VID_MODE_CHANNEL_NUMBER_SHIFT;

		/* XXX: cross-check bpp vs. pixel format? */
		val |= intel_dsi->pixel_format;
	}

	val |= VID_MODE_NOT_SUPPORTED;
	REG_WRITE(port->func_prg_offset, val);

	/*
	 * timeouts for recovery. one frame IIUC. if counter expires, EOT and
	 * stop state.
	 */

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

	if (vlv_dsi_port_is_vid_mode(port) &&
	intel_dsi->video_mode_format == VIDEO_MODE_BURST) {
		REG_WRITE(port->hs_tx_timeout_offset,
			txbyteclkhs(mode->htotal, bpp,
				intel_dsi->lane_count,
				intel_dsi->burst_mode_ratio) + 1);
	} else {
		REG_WRITE(port->hs_tx_timeout_offset,
			txbyteclkhs(mode->vtotal *
				mode->htotal,
				bpp, intel_dsi->lane_count,
				intel_dsi->burst_mode_ratio) + 1);
	}

	REG_WRITE(port->lp_rx_timeout_offset, intel_dsi->lp_rx_timeout);
	REG_WRITE(port->ta_timeout_offset, intel_dsi->turn_arnd_val);
	REG_WRITE(port->device_reset_timer_offset, intel_dsi->rst_timer_val);

	/* dphy stuff */

	/* in terms of low power clock */
	REG_WRITE(port->init_count_offset, txclkesc(intel_dsi->escape_clk_div,
						100));

	val = 0;
	if (intel_dsi->eotp_pkt == 0)
		val |= EOT_DISABLE;

	if (intel_dsi->clock_stop)
		val |= CLOCKSTOP;

	/* recovery disables */
	REG_WRITE(port->eot_offset, val);

	/* in terms of low power clock */
	REG_WRITE(port->init_count_offset, intel_dsi->init_count);

	/*
	 * in terms of txbyteclkhs. actual high to low switch +
	 * MIPI_STOP_STATE_STALL * MIPI_LP_BYTECLK.
	 *
	 * XXX: write MIPI_STOP_STATE_STALL?
	 */
	REG_WRITE(port->hl_switch_count_offset, intel_dsi->hs_to_lp_count);

	/*
	 * XXX: low power clock equivalence in terms of byte clock. the number
	 * of byte clocks occupied in one low power clock. based on txbyteclkhs
	 * and txclkesc. txclkesc time / txbyteclk time * (105 +
	 * MIPI_STOP_STATE_STALL) / 105.???
	 */
	REG_WRITE(port->lp_byteclk_offset, intel_dsi->lp_byte_clk);

	/*
	 * the bw essential for transmitting 16 long packets containing 252
	 * bytes meant for dcs write memory command is programmed in this
	 * register in terms of byte clocks. based on dsi transfer rate and the
	 * number of lanes configured the time taken to transmit 16 long packets
	 * in a dsi stream varies.
	 */
	REG_WRITE(port->dbi_bw_offset, intel_dsi->bw_timer);

	REG_WRITE(port->lane_switch_time_offset,
		intel_dsi->clk_lp_to_hs_count << LP_HS_SSW_CNT_SHIFT |
		intel_dsi->clk_hs_to_lp_count << HS_LP_PWR_SW_CNT_SHIFT);

	if (vlv_dsi_port_is_vid_mode(port))
		/*
		 * Some panels might have resolution which is not a multiple of
		 * 64 like 1366 x 768. Enable RANDOM resolution support for such
		 * panels by default
		 */
		REG_WRITE(port->video_mode_offset,
				intel_dsi->video_frmt_cfg_bits |
				intel_dsi->video_mode_format |
				IP_TG_CONFIG |
				RANDOM_DPI_DISPLAY_RESOLUTION);

	REG_WRITE(port->dr_offset, DEVICE_READY);
	usleep_range(2000, 2500);

	return 0;
}

u32 vlv_dsi_port_pre_enable(struct vlv_dsi_port *port,
			struct drm_mode_modeinfo *mode,
			struct dsi_config *config)
{
	u32 val;
	struct dsi_context *intel_dsi = &config->ctx;
	vlv_dsi_port_wait_for_fifo_empty(port);

	REG_WRITE(port->dr_offset, 0);
	usleep_range(2000, 2500);

	val = REG_READ(port->func_prg_offset);
	val |= intel_dsi->pixel_format;
	REG_WRITE(port->func_prg_offset, val);

	val = REG_READ(port->eot_offset);
	if (intel_dsi->eotp_pkt == 0)
		val |= EOT_DISABLE;

	REG_WRITE(port->eot_offset, val);

	REG_WRITE(port->dr_offset, DEVICE_READY);
	usleep_range(2000, 2500);

	if (vlv_dsi_port_is_cmd_mode(port))
		REG_WRITE(port->max_ret_pkt_offset, 8 * 4);

	return 0;
}

u32 vlv_dsi_port_enable(struct vlv_dsi_port *port, u32 port_bits)
{
	u32 val;
	if (vlv_dsi_port_is_vid_mode(port)) {
		vlv_dsi_port_wait_for_fifo_empty(port);

		/* assert ip_tg_enable signal */
		val = REG_READ(port->offset) &
				~LANE_CONFIGURATION_MASK;
		val |= port_bits;
		REG_WRITE(port->offset, val | DPI_ENABLE);
		REG_POSTING_READ(port->offset);
	}
	return 0;
}

u32 vlv_dsi_port_disable(struct vlv_dsi_port *port,
		struct dsi_config *config)
{
	u32 val, clk_div = 0;
	vlv_dsi_port_wait_for_fifo_empty(port);

	if (config != NULL)
		clk_div = config->ctx.escape_clk_div;

	if (vlv_dsi_port_is_vid_mode(port)) {
		/* de-assert ip_tg_enable signal */
		val = REG_READ(port->offset);
		REG_WRITE(port->offset, val & ~DPI_ENABLE);
		REG_POSTING_READ(port->offset);

		usleep_range(2000, 2500);
	}

	/* Panel commands can be sent when clock is in LP11 */
	REG_WRITE(port->dr_offset, 0x0);

	val = REG_READ(port->offset);
	val &= ~ESCAPE_CLOCK_DIVIDER_MASK;
	REG_WRITE(port->offset, val |
		clk_div <<
		ESCAPE_CLOCK_DIVIDER_SHIFT);

	val = REG_READ(port->func_prg_offset);
	val &= ~VID_MODE_FORMAT_MASK;
	REG_WRITE(port->func_prg_offset, val);

	REG_WRITE(port->eot_offset, CLOCKSTOP);

	REG_WRITE(port->dr_offset, 0x1);

	return 0;
}

bool vlv_dsi_port_can_be_disabled(struct vlv_dsi_port *port)
{
	u32 val = REG_READ(port->offset);
	u32 func = REG_READ(port->func_prg_offset);
	bool ret = false;

	if ((val & DPI_ENABLE) || (func & CMD_MODE_DATA_WIDTH_MASK)) {
		if (REG_READ(port->dr_offset) & DEVICE_READY)
			ret = true;
	}

	return ret;
}

bool vlv_dsi_port_init(struct vlv_dsi_port *port, enum port enum_port,
		enum pipe pipe)
{
	port->offset = MIPI_PORT_CTRL(pipe);
	port->dr_offset = MIPI_DEVICE_READY(pipe);
	port->func_prg_offset = MIPI_DSI_FUNC_PRG(pipe);
	port->eot_offset = MIPI_EOT_DISABLE(pipe);
	port->fifo_stat_offset = MIPI_GEN_FIFO_STAT(pipe);
	port->ctrl_offset = MIPI_CTRL(pipe);
	port->dphy_param_offset = MIPI_DPHY_PARAM(pipe);
	port->dpi_res_offset = MIPI_DPI_RESOLUTION(pipe);
	port->hfp_count_offset = MIPI_HFP_COUNT(pipe);
	port->hsync_count_offset = MIPI_HSYNC_PADDING_COUNT(pipe);
	port->hbp_count_offset = MIPI_HBP_COUNT(pipe);
	port->vfp_count_offset = MIPI_VFP_COUNT(pipe);
	port->vsync_count_offset = MIPI_VSYNC_PADDING_COUNT(pipe);
	port->vbp_count_offset = MIPI_VBP_COUNT(pipe);
	port->max_ret_pkt_offset	= MIPI_MAX_RETURN_PKT_SIZE(pipe);
	port->hs_tx_timeout_offset	= MIPI_HS_TX_TIMEOUT(pipe);
	port->lp_rx_timeout_offset	= MIPI_LP_RX_TIMEOUT(pipe);
	port->ta_timeout_offset		= MIPI_TURN_AROUND_TIMEOUT(pipe);
	port->device_reset_timer_offset	= MIPI_DEVICE_RESET_TIMER(pipe);
	port->init_count_offset		= MIPI_INIT_COUNT(pipe);
	port->hl_switch_count_offset	= MIPI_HIGH_LOW_SWITCH_COUNT(pipe);
	port->lp_byteclk_offset		= MIPI_LP_BYTECLK(pipe);
	port->dbi_bw_offset		= MIPI_DBI_BW_CTRL(pipe);
	port->lane_switch_time_offset	= MIPI_CLK_LANE_SWITCH_TIME_CNT(pipe);
	port->video_mode_offset		= MIPI_VIDEO_MODE_FORMAT(pipe);

	/* cmd mode offsets */
	port->hs_ls_dbi_enable_offset = MIPI_HS_LP_DBI_ENABLE(pipe);
	port->hs_gen_ctrl_offset = MIPI_HS_GEN_CTRL(pipe);
	port->lp_gen_ctrl_offset = MIPI_LP_GEN_CTRL(pipe);
	port->hs_gen_data_offset = MIPI_HS_GEN_DATA(pipe);
	port->lp_gen_data_offset = MIPI_LP_GEN_DATA(pipe);
	port->dpi_ctrl_offset    = MIPI_DPI_CONTROL(pipe);

	port->intr_stat_offset = MIPI_INTR_STAT(pipe);

	port->port_id = enum_port;

	/* FIXME: take appropriate value based on detection */
	port->operation_mode = 0;

	return true;
}

bool vlv_dsi_port_destroy(struct vlv_dsi_port *port)
{
	return true;
}
