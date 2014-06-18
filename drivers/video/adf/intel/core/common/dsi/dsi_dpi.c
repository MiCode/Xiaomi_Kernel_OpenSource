/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#include "intel_adf_device.h"
#include "core/common/dsi/dsi_pipe.h"
#include "core/common/dsi/dsi_panel.h"
#include "core/common/dsi/dsi_config.h"
#include "core/common/dsi/dsi_pkg_sender.h"
#include "dsi_dpi.h"
#include "pwr_mgmt.h"

/* Try to enter MAXFIFO mode immediately. */
#define REPEATED_FRM_CNT_THRESHHOLD 1

static int __dpi_enter_ulps_locked(struct dsi_pipe *pipe, int offset)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;

	if (!sender) {
		pr_debug("pkg sender is NULL\n");
		return -EINVAL;
	}

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	if (ctx->device_ready & ULPS_STATE_MASK) {
		pr_debug("Broken ULPS states\n");
		return -EINVAL;
	}

	if (offset != 0)
		sender->work_for_slave_panel = true;

	/*wait for all FIFOs empty*/
	dsi_wait_for_fifos_empty(sender);
	sender->work_for_slave_panel = false;

	/*inform DSI host is to be put on ULPS*/
	ctx->device_ready |= ULPS_ENTER;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	pr_debug("entered ULPS state\n");
	return 0;
}

static int __dpi_exit_ulps_locked(struct dsi_pipe *pipe, int offset)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	/*enter ULPS EXIT state*/
	ctx->device_ready &= ~ULPS_STATE_MASK;
	ctx->device_ready |= ULPS_EXIT;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	/*wait for 1ms as spec suggests*/
	mdelay(1);

	/*clear ULPS state*/
	ctx->device_ready &= ~ULPS_STATE_MASK;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	pr_debug("exited ULPS state\n");
	return 0;
}

static void __dpi_set_properties(struct dsi_pipe *pipe,
		enum enum_ports port)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;
	int offset = 0;
	u32 mipi_eot_disable_mask = 0;

	if (port == PORT_C)
		offset = MIPI_C_REG_OFFSET;

	/*D-PHY parameter*/
	REG_WRITE(regs->dphy_param_reg + offset, ctx->dphy_param);

	/*Configure DSI controller*/
	REG_WRITE(regs->mipi_control_reg + offset, ctx->mipi_control);
	REG_WRITE(regs->intr_en_reg + offset, ctx->intr_en);
	REG_WRITE(regs->hs_tx_timeout_reg + offset, ctx->hs_tx_timeout);
	REG_WRITE(regs->lp_rx_timeout_reg + offset, ctx->lp_rx_timeout);
	REG_WRITE(regs->turn_around_timeout_reg + offset,
			ctx->turn_around_timeout);
	REG_WRITE(regs->device_reset_timer_reg + offset,
			ctx->device_reset_timer);
	REG_WRITE(regs->high_low_switch_count_reg + offset,
			ctx->high_low_switch_count);
	REG_WRITE(regs->init_count_reg + offset, ctx->init_count);
	mipi_eot_disable_mask = LP_RX_TIMEOUT_REC_DISABLE |
		HS_TX_TIMEOUT_REC_DISABLE | LOW_CONTENTION_REC_DISABLE |
		HIGH_CONTENTION_REC_DISABLE | TXDSI_TYPE_NOT_RECOG_REC_DISABLE |
		TXECC_MULTIBIT_REC_DISABLE | CLOCK_STOP | EOT_DIS;
	REG_WRITE(regs->eot_disable_reg + offset,
			(REG_READ(regs->eot_disable_reg) &
			 ~mipi_eot_disable_mask) |
			(ctx->eot_disable & mipi_eot_disable_mask));
	REG_WRITE(regs->lp_byteclk_reg + offset, ctx->lp_byteclk);
	REG_WRITE(regs->clk_lane_switch_time_cnt_reg + offset,
			ctx->clk_lane_switch_time_cnt);
	REG_WRITE(regs->video_mode_format_reg + offset, ctx->video_mode_format);
	REG_WRITE(regs->dsi_func_prg_reg + offset, ctx->dsi_func_prg);

	/*DSI timing*/
	REG_WRITE(regs->dpi_resolution_reg + offset, ctx->dpi_resolution);
	REG_WRITE(regs->hsync_count_reg + offset, ctx->hsync_count);
	REG_WRITE(regs->hbp_count_reg + offset, ctx->hbp_count);
	REG_WRITE(regs->hfp_count_reg + offset, ctx->hfp_count);
	REG_WRITE(regs->hactive_count_reg + offset, ctx->hactive_count);
	REG_WRITE(regs->vsync_count_reg + offset, ctx->vsync_count);
	REG_WRITE(regs->vbp_count_reg + offset, ctx->vbp_count);
	REG_WRITE(regs->vfp_count_reg + offset, ctx->vfp_count);
}

static int __dpi_config_port(struct dsi_pipe *pipe, enum enum_ports port)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;
	int offset = 0;

	if (port == PORT_C)
		offset = MIPI_C_REG_OFFSET;

	/*exit ULPS state*/
	__dpi_exit_ulps_locked(pipe, offset);

	/*Enable DSI Controller*/
	REG_WRITE(regs->device_ready_reg + offset,
		ctx->device_ready | DEVICE_READY);

	/*set low power output hold*/
	if (port == PORT_C)
		offset = 0x1000;

	REG_WRITE(regs->mipi_reg + offset, (ctx->mipi | LP_OUTPUT_HOLD_ENABLE));

	return 0;
}

static int __set_vblank(struct dsi_pipe *pipe, bool enabled)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	u8 idx = pipe->base.base.idx;
	u32 event_bit = 0;

	switch (idx) {
	case 0:
		event_bit = IIR_PIPEA_EVENT;
		break;
	case 2:
		event_bit = IIR_PIPEC_EVENT;
		break;
	default:
		pr_err("%s: invalid pipe index %d\n", __func__, idx);
		return -EINVAL;
	}

	/* FIXME: add lock to avoid race condition for the 1st level IRQ. */

	/* Enable/disable Vblank interrupt for DPI panel. */
	if (enabled) {
		REG_WRITE(IMR, REG_READ(IMR) & ~event_bit);
		REG_WRITE(IER, REG_READ(IER) | event_bit);
		REG_WRITE(regs->pipestat_reg, REG_READ(regs->pipestat_reg) |
				(VBLANK_ENABLE | VBLANK_STATUS));
		REG_READ(regs->pipestat_reg);
	} else {
		/*
		 * FIXME: keep 1st level interrupt on. To use get/put mechanism
		 * to manage it if it's required to turn on/off the 1st level
		 * interrupt dynamically.
		 */
/*
		REG_WRITE(IMR, REG_READ(IMR) | event_bit);
		REG_WRITE(IER, REG_READ(IER) & ~event_bit);
*/
		REG_WRITE(regs->pipestat_reg,
			REG_READ(regs->pipestat_reg) & ~VBLANK_ENABLE);
		REG_READ(regs->pipestat_reg);
	}

	return 0;
}

static int __set_repeated_frame(struct dsi_pipe *pipe, bool enabled)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	u8 idx = pipe->base.base.idx;
	u32 event_bit = 0;
	int repeated_frm_cnt_threshold = REPEATED_FRM_CNT_THRESHHOLD;

	switch (idx) {
	case 0:
		event_bit = IIR_PIPEA_EVENT;
		break;
	case 2:
		event_bit = IIR_PIPEC_EVENT;
		break;
	default:
		pr_err("%s: invalid pipe index %d\n", __func__, idx);
		return -EINVAL;
	}

	/* FIXME: add lock to avoid race condition for the 1st level IRQ. */

	/* Enable/disable Repeated Frame interrupt for DPI panel. */
	if (enabled) {
		REG_WRITE(PIPE_A_CRC_CTRL_RED, ENBALE_CRC);
		REG_WRITE(PIPEA_RPT_FRM_CNT_THRESHOLD,
				RPT_FRAME_CNT_LOGIC_ENABLE |
				repeated_frm_cnt_threshold);

		REG_WRITE(IMR, REG_READ(IMR) & ~event_bit);
		REG_WRITE(IER, REG_READ(IER) | event_bit);
		REG_WRITE(regs->pipestat_reg,
				REG_READ(regs->pipestat_reg) |
				(REPEATED_FRAME_CNT_ENABLE |
				 REPEATED_FRAME_CNT_STATUS));
		REG_READ(regs->pipestat_reg);
	} else {
		/*
		 * FIXME: keep 1st level interrupt on. To use get/put mechanism
		 * to manage it if it's required to turn on/off the 1st level
		 * interrupt dynamically.
		 */
/*
		REG_WRITE(IMR, REG_READ(IMR) | event_bit);
		REG_WRITE(IER, REG_READ(IER) & ~event_bit);
*/
		REG_WRITE(regs->pipestat_reg,
				REG_READ(regs->pipestat_reg) &
				~(REPEATED_FRAME_CNT_ENABLE));
		REG_READ(regs->pipestat_reg);

		REG_WRITE(PIPEA_RPT_FRM_CNT_THRESHOLD,
				REG_READ(PIPEA_RPT_FRM_CNT_THRESHOLD) &
				~RPT_FRAME_CNT_LOGIC_ENABLE);
		REG_WRITE(PIPE_A_CRC_CTRL_RED,
				REG_READ(PIPE_A_CRC_CTRL_RED) & ~ENBALE_CRC);
	}

	return 0;
}

static void ann_dc_setup(struct dsi_pipe *pipe)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;

	pr_debug("restore some registers to default value\n");

	power_island_get(OSPM_DISPLAY_B | OSPM_DISPLAY_C);

	REG_WRITE(DSPCLK_GATE_D, 0x0);
	REG_WRITE(RAMCLK_GATE_D, DISABLE_CLKGATING_RDB | DISABLE_CLKGATING_OVR);
	REG_WRITE(PFIT_CONTROL, PFIT_PIPE_SEL_B);
	REG_WRITE(DSPIEDCFGSHDW, 0x0);
	/* TODO: to figure out the watermark values. */
	REG_WRITE(DSPARB2, 0x000A0200);
	REG_WRITE(DSPARB, 0x18040080);
	REG_WRITE(DSPFW1, 0x0F0F3F3F);
	REG_WRITE(DSPFW2, 0x5F2F0F3F);
	REG_WRITE(DSPFW3, 0x0);
	REG_WRITE(DSPFW4, 0x07071F1F);
	REG_WRITE(DSPFW5, 0x2F17071F);
	REG_WRITE(DSPFW6, 0x00001F3F);
	REG_WRITE(DSPFW7, 0x1F3F1F3F);
	REG_WRITE(DSPSRCTRL, 0x00080100);
	REG_WRITE(CHICKEN_BIT, PIPEA_PALETTE);
	REG_WRITE(FBDC_CHICKEN, 0x0C0C0C0C);
	REG_WRITE(CURACNTR, 0x0);
	REG_WRITE(CURBCNTR, 0x0);
	REG_WRITE(CURCCNTR, 0x0);
	REG_WRITE(IEP_OVA_CTRL, 0x0);
	REG_WRITE(IEP_OVA_CTRL, 0x0);
	REG_WRITE(DSPACNTR, 0x0);
	REG_WRITE(DSPBCNTR, 0x0);
	REG_WRITE(DSPCCNTR, 0x0);
	REG_WRITE(DSPDCNTR, 0x0);
	REG_WRITE(DSPECNTR, 0x0);
	REG_WRITE(DSPFCNTR, 0x0);

	power_island_put(OSPM_DISPLAY_B | OSPM_DISPLAY_C);

	pr_debug("setup drain latency\n");

	REG_WRITE(regs->ddl1_reg, ctx->ddl1);
	REG_WRITE(regs->ddl2_reg, ctx->ddl2);
	REG_WRITE(regs->ddl3_reg, ctx->ddl3);
	REG_WRITE(regs->ddl4_reg, ctx->ddl4);
}

/*
 * Power on sequence for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
int dpi_pipe_power_on(struct dsi_pipe *pipe)
{
	u32 val = 0;
	struct dsi_config *config;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	struct dsi_panel *panel;
	int retry, reset_count = 10;
	int i;
	int err = 0;
	u32 power_island = 0;
	int offset = 0;

	pr_debug("\n");

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	regs = &config->regs;
	ctx = &config->ctx;

	panel = pipe->panel;
	if (!panel || !panel->ops) {
		pr_err("%s: invalid panel\n", __func__);
		return -ENODEV;
	}

	power_island = pipe_to_island(pipe->base.base.idx);

	if (power_island & (OSPM_DISPLAY_A | OSPM_DISPLAY_C))
		power_island |= OSPM_DISPLAY_MIO;

	if (is_dual_link(config))
		power_island |= OSPM_DISPLAY_C;

	if (!power_island_get(power_island))
		return -EAGAIN;

	if (!dsi_pipe_enable_clocking(pipe)) {
		pr_err("%s: Failed to enable clocking for DSI.\n", __func__);
		return -EAGAIN;
	}

reset_recovery:
	--reset_count;
	/*HW-Reset*/
	if (panel->ops->reset)
		panel->ops->reset(pipe);

	/*
	 * Wait for DSI PLL locked on pipe, and only need to poll status of pipe
	 * A as both MIPI pipes share the same DSI PLL.
	 */
	if (pipe->base.base.idx == 0) {
		retry = 20000;
		while (!(REG_READ(regs->pipeconf_reg) & PIPECONF_DSIPLL_LOCK) &&
				--retry)
			udelay(150);
		if (!retry) {
			pr_err("PLL failed to lock on pipe\n");
			err = -EAGAIN;
			goto power_on_err;
		}
	}

	if (IS_ANN()) {
		/* FIXME: reset the DC registers for ANN A0 */
		ann_dc_setup(pipe);
	}

	__dpi_set_properties(pipe, PORT_A);

	/*Setup pipe timing*/
	REG_WRITE(regs->htotal_reg, ctx->htotal);
	REG_WRITE(regs->hblank_reg, ctx->hblank);
	REG_WRITE(regs->hsync_reg, ctx->hsync);
	REG_WRITE(regs->vtotal_reg, ctx->vtotal);
	REG_WRITE(regs->vblank_reg, ctx->vblank);
	REG_WRITE(regs->vsync_reg, ctx->vsync);
	REG_WRITE(regs->pipesrc_reg, ctx->pipesrc);

	REG_WRITE(regs->dsppos_reg, ctx->dsppos);
	REG_WRITE(regs->dspstride_reg, ctx->dspstride);

	/*Setup plane*/
	REG_WRITE(regs->dspsize_reg, ctx->dspsize);
	REG_WRITE(regs->dspsurf_reg, ctx->dspsurf);
	REG_WRITE(regs->dsplinoff_reg, ctx->dsplinoff);
	REG_WRITE(regs->vgacntr_reg, ctx->vgacntr);

	/*restore color_coef (chrome) */
	for (i = 0; i < 6; i++)
		REG_WRITE(regs->color_coef_reg + (i << 2), ctx->color_coef[i]);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		REG_WRITE(regs->palette_reg + (i << 2), ctx->palette[i]);

	/* restore dpst setting */
	/*
	   if (dev_priv->psb_dpst_state) {
	   dpstmgr_reg_restore_locked(dev, dsi_config);
	   psb_enable_pipestat(dev_priv, 0, PIPE_DPST_EVENT_ENABLE);
	   }
	 */

	if (__dpi_config_port(pipe, PORT_A) != 0) {
		if (!reset_count) {
			err = -EAGAIN;
			goto power_on_err;
		}
		pr_err("Failed to init dsi controller, reset it!\n");
		goto reset_recovery;
	}

	if (is_dual_link(config)) {
		__dpi_set_properties(pipe, PORT_C);
		__dpi_config_port(pipe, PORT_C);
	}

	/**
	 * Different panel may have different ways to have
	 * drvIC initialized. Support it!
	 */
	if (panel->ops->drv_ic_init) {
		if (panel->ops->drv_ic_init(pipe)) {
			if (!reset_count) {
				err = -EAGAIN;
				goto power_on_err;
			}

			pr_err("Failed to init dsi controller, reset it!\n");
			goto reset_recovery;
		}
	}

	/*Enable MIPI Port A*/
	offset = 0x0;
	REG_WRITE(regs->mipi_reg + offset, (ctx->mipi | MIPI_PORT_ENABLE));
	REG_WRITE(regs->dpi_control_reg + offset, TURN_ON);
	if (is_dual_link(config)) {
		/*Enable MIPI Port C*/
		REG_WRITE(MIPIC_PORT_CTRL, (ctx->mipi | MIPI_PORT_ENABLE));
		offset = MIPI_C_REG_OFFSET;
		REG_WRITE(regs->dpi_control_reg + offset, TURN_ON);
	}

	/**
	 * Different panel may have different ways to have
	 * panel turned on. Support it!
	 */
	if (panel->ops->power_on)
		if (panel->ops->power_on(pipe)) {
			pr_err("Failed to power on panel\n");
			err = -EAGAIN;
			goto power_on_err;
		}

	if (IS_ANN()) {
		REG_WRITE(regs->ddl1_reg, ctx->ddl1);
		REG_WRITE(regs->ddl2_reg, ctx->ddl2);
		REG_WRITE(regs->ddl3_reg, ctx->ddl3);
		REG_WRITE(regs->ddl4_reg, ctx->ddl4);
	}

	/*Enable pipe*/
	val = ctx->pipeconf;
	val &= ~(SPRITE_OVERLAY_PLANES_OFF | CURSOR_PLANES_OFF);

	/**
	 * Frame Start occurs on third HBLANK
	 * after the start of VBLANK
	 */
	val |= PIPEACONF_ENABLE | FRAME_START_DELAY_THIRD;
	REG_WRITE(regs->pipeconf_reg, val);

	/*Wait for pipe enabling,when timing generator
	  is wroking */
	if (REG_READ(regs->mipi_reg) & MIPI_PORT_ENABLE) {
		retry = 10000;
		while (--retry && !(REG_READ(regs->pipeconf_reg) &
					     PIPE_STATE_ENABLED))
			udelay(3);

		if (!retry) {
			pr_err("Failed to enable pipe\n");
			err = -EAGAIN;
			goto power_on_err;
		}
	}

	/*enable plane*/
	val = ctx->dspcntr | PRIMARY_PLANE_ENABLE;
	REG_WRITE(regs->dspcntr_reg, val);

	if (panel->ops->set_brightness) {
		if (ctx->backlight_level <= 0)
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
			ctx->backlight_level = BRIGHTNESS_MAX_LEVEL;
#else
			ctx->backlight_level = 100;
#endif

		if (panel->ops->set_brightness(pipe, ctx->backlight_level))
			pr_err("Failed to set panel brightness\n");
	} else {
		pr_err("Failed to set panel brightness\n");
	}

	if (panel->ops->drv_set_panel_mode)
		panel->ops->drv_set_panel_mode(pipe);

	/* FIXME: To remove it after HWC enables vsync event setting. */
	__set_vblank(pipe, true);

	return err;

power_on_err:
	power_island_put(power_island);
	return err;
}

/*
 * Power off sequence for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
int dpi_pipe_power_off(struct dsi_pipe *pipe)
{
	u32 val = 0;
	u32 tmp = 0;
	struct dsi_config *config;
	struct dsi_panel *panel;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	int retry;
	int i;
	int err = 0;
	u32 power_island = 0;
	int offset = 0;

	pr_debug("\n");

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	regs = &config->regs;
	ctx = &config->ctx;

	panel = pipe->panel;
	if (!panel || !panel->ops) {
		pr_err("%s: invalid panel\n", __func__);
		return -ENODEV;
	}

	tmp = REG_READ(regs->pipeconf_reg);
	ctx->dspcntr = REG_READ(regs->dspcntr_reg);

	/*save color_coef (chrome) */
	for (i = 0; i < 6; i++)
		ctx->color_coef[i] = REG_READ(regs->color_coef_reg + (i << 2));

	/* save palette (gamma) */
	for (i = 0; i < 256; i++)
		ctx->palette[i] = REG_READ(regs->palette_reg + (i << 2));

	/*
	 * Couldn't disable the pipe until DRM_WAIT_ON signaled by last
	 * vblank event when playing video, otherwise the last vblank event
	 * will lost when pipe disabled before vblank interrupt coming
	 * sometimes.
	 */

	/*Disable panel*/
	val = ctx->dspcntr;
	REG_WRITE(regs->dspcntr_reg, (val & ~PRIMARY_PLANE_ENABLE));
	/*Disable overlay & cursor panel assigned to this pipe*/
	REG_WRITE(regs->pipeconf_reg,
		(tmp | SPRITE_OVERLAY_PLANES_OFF | CURSOR_PLANES_OFF));

	/*Disable pipe*/
	val = REG_READ(regs->pipeconf_reg);
	ctx->pipeconf = val;
	REG_WRITE(regs->pipeconf_reg, (val & ~PIPEACONF_ENABLE));

	/*wait for pipe disabling,
	  pipe synchronization plus , only avaiable when
	  timer generator is working*/
	if (REG_READ(regs->mipi_reg) & MIPI_PORT_ENABLE) {
		retry = 100000;
		while (--retry && (REG_READ(regs->pipeconf_reg) &
					    PIPE_STATE_ENABLED))
			udelay(5);

		if (!retry) {
			pr_err("Failed to disable pipe\n");
			/*
			 * FIXME: turn off the power island directly
			 * although failed to disable pipe.
			 */
			err = 0;
			goto power_off_err;
		}
	}

	/**
	 * Different panel may have different ways to have
	 * panel turned off. Support it!
	 */
	if (panel->ops->power_off) {
		if (panel->ops->power_off(pipe)) {
			pr_err("Failed to power off panel\n");
			err = -EAGAIN;
			goto power_off_err;
		}
	}

	/*Disable MIPI port*/
	REG_WRITE(regs->mipi_reg, (REG_READ(regs->mipi_reg) &
				   ~MIPI_PORT_ENABLE));

	/*clear Low power output hold*/
	REG_WRITE(regs->mipi_reg,
		(REG_READ(regs->mipi_reg) & ~LP_OUTPUT_HOLD_ENABLE));

	/*Disable DSI controller*/
	REG_WRITE(regs->device_ready_reg, (ctx->device_ready & ~DEVICE_READY));

	/*enter ULPS*/
	__dpi_enter_ulps_locked(pipe, offset);

	if (is_dual_link(config)) {
		offset = 0x1000;
		/*Disable MIPI port*/
		REG_WRITE(MIPIC_PORT_CTRL,
			(REG_READ(MIPIC_PORT_CTRL) & ~MIPI_PORT_ENABLE));
		/*clear Low power output hold*/
		REG_WRITE(MIPIC_PORT_CTRL,
			(REG_READ(MIPIC_PORT_CTRL) & ~LP_OUTPUT_HOLD_ENABLE));

		offset = MIPI_C_REG_OFFSET;
		/*Disable DSI controller*/
		REG_WRITE(regs->device_ready_reg + offset,
				(ctx->device_ready & ~DEVICE_READY));

		/*enter ULPS*/
		__dpi_enter_ulps_locked(pipe, offset);
		offset = 0x0;
	}

	if (!dsi_pipe_disable_clocking(pipe)) {
		pr_err("%s: Failed to disable clocking for DSI.\n", __func__);
		err = -EAGAIN;
		goto power_off_err;
	}

power_off_err:
	power_island = pipe_to_island(pipe->base.base.idx);

	if (power_island & (OSPM_DISPLAY_A | OSPM_DISPLAY_C))
		power_island |= OSPM_DISPLAY_MIO;

	if (is_dual_link(config))
		power_island |= OSPM_DISPLAY_C;

	if (!power_island_put(power_island))
		return -EINVAL;

	return err;
}

static u16 dpi_to_byte_clock_count(int pixel_clock_count, int num_lane, int bpp)
{
	return (u16)((pixel_clock_count * bpp) / (num_lane * 8));
}

/*
 * Calculate the dpi time basing on a given drm mode @mode
 * return 0 on success.
 * FIXME: I was using proposed mode value for calculation, may need to
 * use crtc mode values later
 */
static int dpi_timing_calculation(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode,
		struct dpi_timing *dpi_timing,
		int num_lane, int bpp)
{
	struct dsi_config *config = &pipe->config;
	int pclk_hsync, pclk_hfp, pclk_hbp, pclk_hactive;
	int pclk_vsync, pclk_vfp, pclk_vbp;

	if (!mode || !dpi_timing) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	pr_debug("pclk %d, hdisplay %d, hsync_start %d, hsync_end %d, htotal %d\n",
			mode->clock, mode->hdisplay, mode->hsync_start,
			mode->hsync_end, mode->htotal);
	pr_debug("vdisplay %d, vsync_start %d, vsync_end %d, vtotal %d\n",
			mode->vdisplay, mode->vsync_start,
			mode->vsync_end, mode->vtotal);

	pclk_hactive = mode->hdisplay;
	pclk_hfp = mode->hsync_start - mode->hdisplay;
	pclk_hsync = mode->hsync_end - mode->hsync_start;
	pclk_hbp = mode->htotal - mode->hsync_end;

	pclk_vfp = mode->vsync_start - mode->vdisplay;
	pclk_vsync = mode->vsync_end - mode->vsync_start;
	pclk_vbp = mode->vtotal - mode->vsync_end;
	/*
	 * byte clock counts were calculated by following formula
	 * bclock_count = pclk_count * bpp / num_lane / 8
	 */
	if (is_dual_link(config)) {
		dpi_timing->hsync_count = pclk_hsync;
		dpi_timing->hbp_count = pclk_hbp;
		dpi_timing->hfp_count = pclk_hfp;
		dpi_timing->hactive_count = pclk_hactive / 2;
		dpi_timing->vsync_count = pclk_vsync;
		dpi_timing->vbp_count = pclk_vbp;
		dpi_timing->vfp_count = pclk_vfp;
	} else {
		dpi_timing->hsync_count =
			dpi_to_byte_clock_count(pclk_hsync, num_lane, bpp);
		dpi_timing->hbp_count =
			dpi_to_byte_clock_count(pclk_hbp, num_lane, bpp);
		dpi_timing->hfp_count =
			dpi_to_byte_clock_count(pclk_hfp, num_lane, bpp);
		dpi_timing->hactive_count =
			dpi_to_byte_clock_count(pclk_hactive, num_lane, bpp);

		dpi_timing->vsync_count =
			dpi_to_byte_clock_count(pclk_vsync, num_lane, bpp);
		dpi_timing->vbp_count =
			dpi_to_byte_clock_count(pclk_vbp, num_lane, bpp);
		dpi_timing->vfp_count =
			dpi_to_byte_clock_count(pclk_vfp, num_lane, bpp);
	}

	pr_debug("DPI timings: %d, %d, %d, %d, %d, %d, %d\n",
			dpi_timing->hsync_count, dpi_timing->hbp_count,
			dpi_timing->hfp_count, dpi_timing->hactive_count,
			dpi_timing->vsync_count, dpi_timing->vbp_count,
			dpi_timing->vfp_count);

	return 0;
}

/*
 * Setup DPI timing for video mode MIPI panel.
 * NOTE: do NOT modify this function
 */
static void dpi_set_timing(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dpi_timing dpi_timing;
	struct dsi_config *config = &pipe->config;
	struct dsi_context *ctx = &config->ctx;

	/*dpi resolution*/
	if (is_dual_link(config))
		ctx->dpi_resolution =
			(mode->vdisplay << VERT_ADDR_SHIFT |
			(mode->hdisplay / 2));
	else
		ctx->dpi_resolution =
			(mode->vdisplay << VERT_ADDR_SHIFT | mode->hdisplay);

	/*Calculate DPI timing*/
	dpi_timing_calculation(pipe, mode, &dpi_timing,
			config->lane_count, config->bpp);

	/*update HW context with new DPI timings*/
	ctx->hsync_count = dpi_timing.hsync_count;
	ctx->hbp_count = dpi_timing.hbp_count;
	ctx->hfp_count = dpi_timing.hfp_count;
	ctx->hactive_count = dpi_timing.hactive_count;
	ctx->vsync_count = dpi_timing.vsync_count;
	ctx->vbp_count = dpi_timing.vbp_count;
	ctx->vfp_count = dpi_timing.vfp_count;
}

static int dpi_fillup_ctx(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_context *ctx = &config->ctx;
	struct dsi_panel *panel = pipe->panel;
	u16 hblank_start, hblank_end;
	u16 vblank_start, vblank_end;
	int hdelay;

	/* FIXME: need to do same setting with drm_mode_set_crtcinfo(). */
	hblank_start = min(mode->hdisplay, mode->hsync_start);
	hblank_end = max(mode->htotal, mode->hsync_end);
	vblank_start = min(mode->vdisplay, mode->vsync_start);
	vblank_end = max(mode->vtotal, mode->vsync_end);

	ctx->vgacntr = VGA_DISP_DISABLE;

	/*set up pipe timings */

	ctx->htotal =
		(mode->hdisplay - 1) | ((mode->htotal - 1) << HORZ_TOTAL_SHIFT);
	ctx->hblank =
		(hblank_start - 1) | ((hblank_end - 1) << HORZ_BLANK_END_SHIFT);
	ctx->hsync =
		(mode->hsync_start - 1) |
		((mode->hsync_end - 1) << HORZ_SYNC_END_SHIFT);
	ctx->vtotal =
		(mode->vdisplay - 1) | ((mode->vtotal - 1) << VERT_TOTAL_SHIFT);
	ctx->vblank =
		(vblank_start - 1) | ((vblank_end - 1) << VERT_BLANK_END_SHIFT);
	ctx->vsync =
		(mode->vsync_start - 1) |
		((mode->vsync_end - 1) << VERT_SYNC_END_SHIFT);

	/*pipe source */
	ctx->pipesrc =
		((mode->hdisplay - 1) << HORZ_SRC_SIZE_SHIFT) |
		(mode->vdisplay - 1);

	/*setup dsp plane */
	ctx->dsppos = 0;
	/* PR2 panel has 200 pixel dummy clocks,
	 * So the display timing should be 800x1024, and surface
	 * is 608x1024(64 bits align), then the information between android
	 * and Linux frame buffer is not consistent.
	 */
	if (panel->panel_id == TMD_6X10_VID)
		ctx->dspsize = ((mode->vdisplay - 1) << HEIGHT_SHIFT) |
			(mode->hdisplay - 200 - 1);
	else
		ctx->dspsize = ((mode->vdisplay - 1) << HEIGHT_SHIFT) |
			(mode->hdisplay - 1);

	/* FIXME: To make the aligned value DC independant. */
	ctx->dspstride = ALIGN(mode->hdisplay * 4, 64);
	ctx->dspsurf = 0;
	ctx->dsplinoff = 0;
	ctx->dspcntr = SRC_PIX_FMT_BGRX8888;

	if (pipe->base.base.idx == 2)
		ctx->dspcntr |= PIPE_SELECT_PIPEC;

	/*
	 * Setup pipe configuration for different panels
	 * The formula recommended from hw team is as below:
	 * (htotal * 5ns * hdelay) >= 8000ns
	 * hdelay is the count of delayed HBLANK scan lines
	 * And the max hdelay is 4
	 * by programming of PIPE(A/C) CONF bit 28:27:
	 * 00 = 1 scan line, 01 = 2 scan line,
	 * 02 = 3 scan line, 03 = 4 scan line
	 */
	ctx->pipeconf &= ~FRAME_START_DELAY_MASK;

	hdelay = 8000 / mode->htotal / 5;
	if (8000 % (mode->htotal * 5) > 0)
		hdelay += 1;

	/* Use the max hdelay instead */
	if (hdelay > 4)
		hdelay = 4;

	ctx->pipeconf |= ((hdelay - 1) << FRAME_START_DELAY_SHIFT);

	/*setup deadline*/
	ctx->ddl1 = 0x83838383;
	ctx->ddl2 = 0x83838383;
	ctx->ddl3 = 0x83;
	ctx->ddl4 = 0x8383;

	dpi_set_timing(pipe, mode);

	return 0;
}

int dpi_pipe_mode_set(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	if (!pipe || !mode) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	return dpi_fillup_ctx(pipe, mode);
}

void dpi_pipe_pre_post(struct dsi_pipe *pipe)
{
	dsi_sdo_forbid(pipe);
}

int dpi_pipe_set_event(struct dsi_pipe *pipe, u8 event, bool enabled)
{
	int ret = 0;

	if (!pipe) {
		pr_err("%s: invalid parameter\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_debug("%s: pipe %u, event = %u, enabled = %d\n", __func__,
			pipe->base.base.idx, event, enabled);

	switch (event) {
	case INTEL_PIPE_EVENT_VSYNC:
		ret = __set_vblank(pipe, enabled);
		break;
	case INTEL_PIPE_EVENT_REPEATED_FRAME:
		ret = __set_repeated_frame(pipe, enabled);
		break;
	default:
		pr_err("%s: unsupported event %u\n", __func__, event);
		ret = -EINVAL;
		goto err;
	}

err:
	return ret;
}

void dpi_pipe_get_events(struct dsi_pipe *pipe, u32 *events)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	u32 pipe_stat = 0;
	const int pipe_stat_cnt = 16;
	int i = 0;

	*events = 0;

	/* Get the interrupt type via PIPESTAT */
	pipe_stat = REG_READ(regs->pipestat_reg);
	/* FIXME: to add lock */
	/* Clear the 2nd level interrupt. */
	REG_WRITE(regs->pipestat_reg, pipe_stat);

	pr_debug("%s: pipestate = %#x\n", __func__, pipe_stat);

	for (i = 0; i < pipe_stat_cnt; i++) {
		switch (pipe_stat & (1 << i)) {
		case VBLANK_STATUS:
			*events |= INTEL_PIPE_EVENT_VSYNC;
			break;
		case REPEATED_FRAME_CNT_STATUS:
			*events |= INTEL_PIPE_EVENT_REPEATED_FRAME;
			break;
		/* TODO: to support more device custom events handling. */
		case DPST_STATUS:
		case MIPI_CMD_DONE_STATUS:
		default:
			break;
		}
	}
}

void dpi_pipe_handle_events(struct dsi_pipe *pipe, u32 events)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_panel *panel = pipe->panel;
	struct dsi_sdo *sdo = NULL;

	if (!config || !panel) {
		pr_err("%s: invalid arguments", __func__);
		return;
	}

	if ((panel->info.dsi_type != DSI_DPI) || (!pipe->base.primary)) {
		pr_debug("%s: Repeated Frame interrupt should be only asserted"
				"for video mode panel via Pipe A.\n", __func__);
		return;
	}

	if (events & INTEL_PIPE_EVENT_REPEATED_FRAME) {
		/*
		 * Note: the Repeated Frame interrupt should have been disabled
		 * when entering MAXFIFO mode, but we disable it at the top half
		 * of IRQ handler because it's asserted too much frequently
		 * (~14ns) so that the work couldn't be scheduled normally
		 * later.
		 */
		dpi_pipe_set_event(pipe, INTEL_PIPE_EVENT_REPEATED_FRAME,
				   false);

		sdo = (struct dsi_sdo *)config->sdo;
		if (sdo)
			schedule_work(&sdo->repeated_frm_work);
	}
}
