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
#include "dsi_dbi.h"
#include "pwr_mgmt.h"

static void dbi_update_fb(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_config *config = &pipe->config;

	if (!IS_ANN()) {
		/* refresh plane changes */
		REG_WRITE(dsplinoff_reg, REG_READ(dsplinoff_reg));
		REG_WRITE(dspsurf_reg, REG_READ(dspsurf_reg));
		REG_READ(dspsurf_reg);
	}

	if (is_dual_link(config))
		dsi_send_dual_dcs(sender, write_mem_start, NULL, 0,
				  CMD_DATA_SRC_PIPE, DSI_SEND_PACKAGE, true);
	else
		dsi_send_dcs(sender, write_mem_start, NULL, 0,
			     CMD_DATA_SRC_PIPE, DSI_SEND_PACKAGE);
}

static int __dbi_enter_ulps_locked(struct dsi_pipe *pipe, int offset)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	if ((offset == 0) && (ctx->device_ready & ULPS_STATE_MASK)) {
		pr_err("%s:Broken ULPS states\n", __func__);
		return -EINVAL;
	}

	if (offset != 0)
		sender->work_for_slave_panel = true;

	/*wait for all FIFOs empty*/
	dsi_wait_for_fifos_empty(sender);
	sender->work_for_slave_panel = false;

	/*inform DSI host is to be put on ULPS*/
	ctx->device_ready |= (ULPS_ENTER | DEVICE_READY);
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);
	mdelay(1);

	/* set AFE hold value*/
	REG_WRITE(regs->mipi_reg + offset,
	     REG_READ(regs->mipi_reg + offset) & ~LP_OUTPUT_HOLD_ENABLE);

	pr_debug("%s: entered ULPS state\n", __func__);

	return 0;
}

static int __dbi_exit_ulps_locked(struct dsi_pipe *pipe, int offset)
{
	int tem = 0;
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_context *ctx = &config->ctx;

	ctx->device_ready = REG_READ(regs->device_ready_reg + offset);

	/*inform DSI host is to be put on ULPS*/
	ctx->device_ready |= (ULPS_ENTER | DEVICE_READY);
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	mdelay(1);
	/* clear AFE hold value*/
	if (offset != 0)
		tem = 0x1000;
	REG_WRITE(regs->mipi_reg + tem,
		REG_READ(regs->mipi_reg + tem) | LP_OUTPUT_HOLD_ENABLE);

	/*enter ULPS EXIT state*/
	ctx->device_ready &= ~ULPS_STATE_MASK;
	ctx->device_ready |= (ULPS_EXIT | DEVICE_READY);
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	/*wait for 1ms as spec suggests*/
	mdelay(1);

	/*clear ULPS state*/
	ctx->device_ready &= ~ULPS_STATE_MASK;
	ctx->device_ready |= DEVICE_READY;
	REG_WRITE(regs->device_ready_reg + offset, ctx->device_ready);

	mdelay(1);

	pr_debug("%s: exited ULPS state\n", __func__);
	return 0;
}

static void __dbi_set_properties(struct dsi_pipe *pipe,
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
	REG_WRITE(regs->dsi_func_prg_reg + offset, ctx->dsi_func_prg);

	/*DBI bw ctrl*/
	REG_WRITE(regs->dbi_bw_ctrl_reg + offset, ctx->dbi_bw_ctrl);
}

static int __set_te(struct dsi_pipe *pipe, bool enabled)
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

	/* Enable/disable TE interrupt for DBI panel. */
	if (enabled) {
		REG_WRITE(IMR, REG_READ(IMR) & ~event_bit);
		REG_WRITE(IER, REG_READ(IER) | event_bit);
		REG_WRITE(regs->pipestat_reg,
			REG_READ(regs->pipestat_reg) |
			(TE_ENABLE | TE_STATUS));
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
			REG_READ(regs->pipestat_reg) & ~TE_ENABLE);
		REG_READ(regs->pipestat_reg);
	}

	return 0;
}

/* dbi interface power on*/
int dbi_power_on(struct dsi_pipe *pipe)
{
	u32 val = 0;
	struct dsi_config *config;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	int retry;
	int err = 0;
	u32 power_island = 0;
	u32 sprite_reg_offset = 0;
	int i = 0;
	int offset = 0;

	if (!pipe)
		return -EINVAL;

	config = &pipe->config;
	regs = &config->regs;
	ctx = &config->ctx;

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
			pr_err("%s: PLL failed to lock on pipe\n", __func__);
			err = -EAGAIN;
			goto power_on_err;
		}
	}

	if (IS_ANN()) {
		/* FIXME: reset the DC registers for ANN A0 */
		power_island_get(OSPM_DISPLAY_B | OSPM_DISPLAY_C);

		REG_WRITE(DSPCLK_GATE_D, 0x0); /* 0x10000000 */
		REG_WRITE(RAMCLK_GATE_D, 0xc0000); /* 0x0 */
		REG_WRITE(PFIT_CONTROL, 0x20000000);
		REG_WRITE(DSPIEDCFGSHDW, 0x0);
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
		REG_WRITE(DSPCHICKENBIT, 0x20);
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
	}

	/*exit ULPS*/
	if (__dbi_exit_ulps_locked(pipe, 0)) {
		pr_err("%s: Failed to exit ULPS\n", __func__);
		goto power_on_err;
	}
	/*update MIPI port config*/
	REG_WRITE(regs->mipi_reg, ctx->mipi |
			 REG_READ(regs->mipi_reg));

	/*unready dsi adapter for re-programming*/
	REG_WRITE(regs->device_ready_reg,
		REG_READ(regs->device_ready_reg) & ~DEVICE_READY);

	if (is_dual_link(config)) {
		if (__dbi_exit_ulps_locked(pipe, MIPI_C_REG_OFFSET)) {
			pr_err("%s: Failed to exit ULPS\n", __func__);
			goto power_on_err;
		}
		REG_WRITE(MIPIC_PORT_CTRL, ctx->mipi |
			  REG_READ(MIPIC_PORT_CTRL));
		/*unready dsi adapter for re-programming*/
		offset = MIPI_C_REG_OFFSET;
		REG_WRITE(regs->device_ready_reg + offset,
			REG_READ(regs->device_ready_reg + offset) &
			~DEVICE_READY);
	}

	/*
	 * According to MIPI D-PHY spec, if clock stop feature is enabled (EOT
	 * Disable), un-ready MIPI adapter needs to wait for 20 cycles from HS
	 * to LP mode. Per calculation 1us is enough.
	 */
	if (ctx->eot_disable & CLOCK_STOP)
		udelay(1);

	__dbi_set_properties(pipe, PORT_A);

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

	/*restore color_coef (chrome) */
	for (i = 0; i < 6; i++)
		REG_WRITE(regs->color_coef_reg + (i << 2), ctx->color_coef[i]);

	/* restore palette (gamma) */
	for (i = 0; i < 256; i++)
		REG_WRITE(regs->palette_reg + (i << 2), ctx->palette[i]);

	/*Setup plane*/
	REG_WRITE(regs->dspsize_reg, ctx->dspsize);
	REG_WRITE(regs->dspsurf_reg, ctx->dspsurf);
	REG_WRITE(regs->dsplinoff_reg, ctx->dsplinoff);
	REG_WRITE(regs->vgacntr_reg, ctx->vgacntr);

	if (is_dual_link(config))
		__dbi_set_properties(pipe, PORT_C);

	/*enable plane*/
	val = ctx->dspcntr | PRIMARY_PLANE_ENABLE;
	REG_WRITE(regs->dspcntr_reg, val);

	if (ctx->sprite_dspcntr & PRIMARY_PLANE_ENABLE) {
		if (pipe->base.base.idx == 0)
			sprite_reg_offset = SP_D_REG_OFFSET;
		else if (pipe->base.base.idx == 2)
			sprite_reg_offset = SP_B_REG_OFFSET;

		/* Set up Sprite Plane */
		REG_WRITE(regs->dspsize_reg + sprite_reg_offset,
				ctx->sprite_dspsize);
		REG_WRITE(regs->dspsurf_reg + sprite_reg_offset,
				ctx->sprite_dspsurf);
		REG_WRITE(regs->dsplinoff_reg + sprite_reg_offset,
				ctx->sprite_dsplinoff);
		REG_WRITE(regs->dsppos_reg + sprite_reg_offset,
				ctx->sprite_dsppos);
		REG_WRITE(regs->dspstride_reg + sprite_reg_offset,
				ctx->sprite_dspstride);

		/* enable plane */
		REG_WRITE(regs->dspcntr_reg + sprite_reg_offset,
				ctx->sprite_dspcntr);
	}

	/* Set up Overlay Plane */
	if (ctx->ovaadd)
		REG_WRITE(OVAADD, ctx->ovaadd);

	if (ctx->ovcadd)
		REG_WRITE(OVCADD, ctx->ovcadd);

	/*ready dsi adapter*/
	REG_WRITE(regs->device_ready_reg,
		REG_READ(regs->device_ready_reg) | DEVICE_READY);
	mdelay(1);

	if (is_dual_link(config)) {
		REG_WRITE(regs->device_ready_reg + offset,
			REG_READ(regs->device_ready_reg + offset) |
			DEVICE_READY);
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
	val |= PIPEACONF_ENABLE | PIPEACONF_DSR;
	REG_WRITE(regs->pipeconf_reg, val);

	/*Wait for pipe enabling,when timing generator is working */
	retry = 10000;
	while (--retry && !(REG_READ(regs->pipeconf_reg) & PIPE_STATE_ENABLED))
		udelay(3);

	if (!retry) {
		pr_err("Failed to enable pipe\n");
		err = -EAGAIN;
		goto power_on_err;
	}

	/*
	 * FIXME: Enable TE to trigger "write_mem_start" issuing
	 * in non-normal boot modes, and need to move to dbi_pipe_power_on
	 * as TE enabling hasn't been done in HWC yet.
	 */
	__set_te(pipe, true);

	return err;

power_on_err:
	power_island_put(power_island);
	return err;
}

/**
 * Power on sequence for command mode MIPI panel.
 * NOTE: do NOT modify this function
 */
int dbi_pipe_power_on(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_pkg_sender *sender;
	struct dsi_panel *panel;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	int reset_count = 10;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI interface", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	sender = &pipe->sender;
	regs = &config->regs;
	ctx = &config->ctx;

	panel = pipe->panel;
	if (!panel || !panel->ops) {
		pr_err("%s: invalid panel\n", __func__);
		return -ENODEV;
	}

	dsi_dsr_forbid_locked(pipe);

reset_recovery:
	--reset_count;
	err = 0;
	/*after entering dstb mode, need reset*/
	if (panel->ops->exit_deep_standby)
		panel->ops->exit_deep_standby(pipe);

	if (dbi_power_on(pipe)) {
		pr_err("%s:Failed to init display controller!\n", __func__);
		err = -EAGAIN;
		goto power_on_err;
	}

	/**
	 * Different panel may have different ways to have
	 * drvIC initialized. Support it!
	 */
	if (panel->ops->drv_ic_init) {
		if (panel->ops->drv_ic_init(pipe)) {
			pr_err("%s: Failed to init drv IC!\n", __func__);
			err = -EAGAIN;
			goto power_on_err;
		}
	}

	if (!IS_ANN())
		dbi_update_fb(pipe);

	/**
	 * Different panel may have different ways to have
	 * panel turned on. Support it!
	 */
	if (panel->ops->power_on)
		if (panel->ops->power_on(pipe)) {
			pr_err("%s: Failed to power on panel\n", __func__);
			err = -EAGAIN;
			goto power_on_err;
		}
	if (panel->ops->set_brightness) {
		if (ctx->backlight_level <= 0)
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
			ctx->backlight_level = BRIGHTNESS_MAX_LEVEL;
#else
			ctx->backlight_level = 100;
#endif

		if (panel->ops->set_brightness(pipe, ctx->backlight_level))
			pr_err("%s:Failed to set panel brightness\n", __func__);
	}

	/*wait for all FIFOs empty*/
	dsi_wait_for_fifos_empty(sender);

	if (is_dual_link(config)) {
		sender->work_for_slave_panel = true;
		dsi_wait_for_fifos_empty(sender);
		sender->work_for_slave_panel = false;
	}

	if (IS_ANN())
		dbi_update_fb(pipe);

power_on_err:
	if (err && reset_count) {
		pr_err("Failed to init panel, try  reset it again!\n");
		goto reset_recovery;
	}
	dsi_dsr_allow(pipe);
	return err;
}
/**
 * Power off sequence for DBI interface
*/
int dbi_power_off(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	int err = 0;
	u32 power_island = 0;
	int retry, i;
	int offset = 0;
	u32 val;

	if (!pipe)
		return -EINVAL;

	config = &pipe->config;
	regs = &config->regs;
	ctx = &config->ctx;

	ctx->dspcntr    = REG_READ(regs->dspcntr_reg);
	ctx->pipeconf   = REG_READ(regs->pipeconf_reg);

	/*save color_coef (chrome) */
	for (i = 0; i < 6; i++)
		ctx->color_coef[i] = REG_READ(regs->color_coef_reg + (i<<2));

	/* save palette (gamma) */
	for (i = 0; i < 256; i++)
		ctx->palette[i] = REG_READ(regs->palette_reg + (i<<2));

	/*Disable plane*/
	val = ctx->dspcntr;
	REG_WRITE(regs->dspcntr_reg, (val & ~PRIMARY_PLANE_ENABLE));

	/* Disable pipe */
	/* Don't disable DSR mode. */
	REG_WRITE(regs->pipeconf_reg,
		(REG_READ(regs->pipeconf_reg) & ~PIPEACONF_ENABLE));
	/*
	 * wait for pipe disabling,
	 * pipe synchronization plus , only avaiable when
	 * timer generator is working
	 */
	if (REG_READ(regs->mipi_reg) & MIPI_PORT_ENABLE) {
		retry = 100000;
		while (--retry && (REG_READ(regs->pipeconf_reg) &
							PIPE_STATE_ENABLED))
			udelay(5);

		if (!retry) {
			pr_err("Failed to disable pipe\n");
			err = -EAGAIN;
			goto power_off_err;
		}
	}
	if (!is_dual_link(config)) {
		/*enter ULPS*/
		__dbi_enter_ulps_locked(pipe, offset);
	} else {
		/*Disable MIPI port*/
		REG_WRITE(regs->mipi_reg,
			(REG_READ(regs->mipi_reg) & ~MIPI_PORT_ENABLE));
		/*clear Low power output hold*/
		REG_WRITE(regs->mipi_reg,
			(REG_READ(regs->mipi_reg) & ~LP_OUTPUT_HOLD_ENABLE));
		/*Disable DSI controller*/
		REG_WRITE(regs->device_ready_reg, (ctx->device_ready &
						   ~DEVICE_READY));
		/*enter ULPS*/
		__dbi_enter_ulps_locked(pipe, offset);

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
		__dbi_enter_ulps_locked(pipe, offset);
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

/**
 * Power off sequence for command mode MIPI panel.
 * NOTE: do NOT modify this function
 */
int dbi_pipe_power_off(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_panel *panel;
	struct dsi_registers *regs;
	struct dsi_context *ctx;
	int err = 0;

	if (!pipe)
		return -EINVAL;

	pr_debug("\n");

	config = &pipe->config;
	regs = &config->regs;
	ctx = &config->ctx;

	dsi_dsr_forbid_locked(pipe);

	panel = pipe->panel;
	if (!panel || !panel->ops) {
		pr_err("%s: invalid panel\n", __func__);
		return -ENODEV;
	}

	if (panel->ops->set_brightness)
		if (panel->ops->set_brightness(pipe, 0))
			pr_err("%s: Failed to set brightness\n", __func__);

	/*wait for two TE, let pending PVR flip complete*/
	/* FIXME: To Remove */
	msleep(32);

	/**
	 * Different panel may have different ways to have
	 * panel turned off. Support it!
	 */
	if (panel->ops->power_off) {
		if (panel->ops->power_off(pipe)) {
			pr_err("%s: Failed to power off panel\n", __func__);
			err = -EAGAIN;
			goto power_off_err;
		}
	}

	/*power off dbi interface*/
	dbi_power_off(pipe);

power_off_err:
	dsi_dsr_allow(pipe);
	return err;
}

static int dbi_fillup_ctx(struct dsi_pipe *pipe,
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
	ctx->hsync = (mode->hsync_start - 1) |
		((mode->hsync_end - 1) << HORZ_SYNC_END_SHIFT);
	ctx->vtotal =
		(mode->vdisplay - 1) | ((mode->vtotal - 1) << VERT_TOTAL_SHIFT);
	ctx->vblank =
		(vblank_start - 1) | ((vblank_end - 1) << VERT_BLANK_END_SHIFT);
	ctx->vsync = (mode->vsync_start - 1) |
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

	return 0;
}

int dbi_pipe_mode_set(struct dsi_pipe *pipe,
			struct drm_mode_modeinfo *mode)
{
	if (!pipe || !mode) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	return dbi_fillup_ctx(pipe, mode);
}

void dbi_pipe_pre_post(struct dsi_pipe *pipe)
{
	dsi_dsr_forbid_locked(pipe);
}

void dbi_pipe_on_post(struct dsi_pipe *pipe)
{
	dbi_update_fb(pipe);
}

int dbi_pipe_set_event(struct dsi_pipe *pipe, u8 event, bool enabled)
{
	if (!pipe) {
		pr_err("%s: invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (event != INTEL_PIPE_EVENT_VSYNC) {
		pr_err("%s: unsupported event %u\n", __func__, event);
		return -EINVAL;
	}

	pr_debug("%s: pipe %u, event = %u, enabled = %d\n", __func__,
			pipe->base.base.idx, event, enabled);

	return __set_te(pipe, enabled);
}

void dbi_pipe_get_events(struct dsi_pipe *pipe, u32 *events)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_registers *regs = &config->regs;
	struct dsi_dsr *dsr = (struct dsi_dsr *)config->dsr;
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
		case TE_STATUS:
			*events |= INTEL_PIPE_EVENT_VSYNC;
			/* To report TE and allow DSR. */
			/* It's better to be moved into handle_vsync_event(). */
			if (dsr)
				schedule_work(&dsr->te_work);
			break;
		/* TODO: to support more device custom events handling. */
		case DPST_STATUS:
		case REPEATED_FRAME_CNT_STATUS:
		case MIPI_CMD_DONE_STATUS:
		default:
			break;
		}
	}
}
