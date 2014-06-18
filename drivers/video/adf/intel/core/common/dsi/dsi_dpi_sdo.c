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

#include "core/common/dsi/dsi_dpi_sdo.h"

#define SDO_COUNT 0

static bool sdo_unsupported_plane(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_registers *regs;
	u32 plane_val = 0;
	u32 plane_d_offset = SP_D_REG_OFFSET;
	u32 plane_offset = SP_REG_OFFSET;
	/* FIXME: Anniedale has another 3 Sprite Planes attached to Pipe A. */
	int num_sprite = 3;
	/*
	 * FIXME: Anniedale has 2 Overlay Planes
	 * which can be attached to Pipe A.
	 */
	int num_ov = 2;
	u32 ov_a_offset = 0;
	u32 ov_plane_offset = OV_C_REG_OFFSET;
	int i = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return false;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return false;
	}

	regs = &config->regs;
	if (!regs) {
		pr_err("%s: invalid DC registers", __func__);
		return false;
	}

	/* Check whether there is any Sprite Plane attached to Pipe A. */
	for (i = plane_d_offset;
			i < (plane_d_offset + num_sprite * plane_offset);
			i += plane_offset) {
		plane_val = REG_READ(regs->dspcntr_reg + i);
		if ((plane_val & PRIMARY_PLANE_ENABLE) &&
				((plane_val & PIPE_SELECT_MASK) ==
				 PIPE_SELECT_FIXED))
			return true;
	}

	/* Check whether there is any Sprite Plane attached to Pipe A. */
	for (i = ov_a_offset; i < (ov_a_offset + num_ov * ov_plane_offset);
			i += ov_plane_offset) {
		/* FIXME: Anniedale supports SDO mode with Overlay Plane A. */
		if (i == ov_a_offset)
			continue;

		plane_val = REG_READ(regs->ovaadd_reg + i);
		if ((plane_val & OV_ENABLE) &&
				((plane_val & OV_PIPE_SELECT_MASK) ==
				 OV_ASSIGN_TO_PIPE_A))
			return true;
	}

	return false;
}

static void sdo_event_notify(struct dsi_pipe *pipe)
{
	pr_debug("Generating invalidate event to HWC/SF\n");

	/*
	 * FIXME: to invoke adf_event_notify(),
	 * or wake up the event polling thread.
	 */
}

static int exit_sdo(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	u8 event = INTEL_PIPE_EVENT_UNKNOWN;
	u32 pwr_cur = 0;
	int retry = 0;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	/* Note: ideally to invoke adf_event_get(). */
	/* Enable the Repeated Frame interrupt. */
	event = INTEL_PIPE_EVENT_REPEATED_FRAME;
	if (pipe->ops.set_event)
		pipe->ops.set_event(pipe, event, true);

	/* Exit S0i1-Display On state. */
	/* Notify P-Unit to disable S0i1-Display On mode. */
	pwr_cur = intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM);
	pwr_cur &= ~PUNIT_SDO_ENABLE;
	intel_mid_msgbus_write32(PUNIT_PORT, DSP_SS_PM, pwr_cur);

	/* Disable the MAXFIFO mode. */
	REG_WRITE(DSPSRCTRL, REG_READ(DSPSRCTRL) & ~MAXFIFO_ENABLE);

	retry = 32;
	while (--retry && (REG_READ(DSPSRCTRL) & MAXFIFO_STATUS))
		usleep_range(1000, 1500);

	if (!retry)
		pr_err("Failed to exit from MAXFIFO mode.\n");

	if (!(REG_READ(DSPSRCTRL) & MAXFIFO_STATUS))
		pr_debug("Exited from MAXFIFO mode.\n");

	return err;
}

static int enter_sdo(struct dsi_pipe *pipe, int level)
{
	struct dsi_config *config;
	u32 pwr_cur = 0;
	int retry = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	pr_debug("Enter SDO\n");

	if (level < SDO_EXITED) {
		pr_err("%s: incorrect sdo level %d", __func__, level);
		return -EINVAL;
	}

	if (sdo_unsupported_plane(pipe)) {
		/*
		 * Notify HWC/SF to composite layers into frame buffer
		 * (YUV layer can be attached with Overlay A for ANN).
		 */
		pr_debug("%s: Issuing invalidate UEvent\n", __func__);
		sdo_event_notify(pipe);
		return 0;
	}

	/* Enter S0i1-Display On state. */
	/* Notify P-Unit to enable S0i1-Display On mode. */
	pwr_cur = intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM);
	pwr_cur |= PUNIT_SDO_ENABLE;
	intel_mid_msgbus_write32(PUNIT_PORT, DSP_SS_PM, pwr_cur);

	/* Enable the MAXFIFO mode, with specified watermarks. */
	REG_WRITE(DSPSRCTRL, MAXFIFO_ENABLE |
			(SPRITE_PLANE_A_WM1 << MAXFIFO_WM1_SHIFT) |
			(SPRITE_PLANE_A_WM0 << MAXFIFO_WM_SHIFT));

	retry = 32;
	while (--retry && (!(REG_READ(DSPSRCTRL) & MAXFIFO_STATUS)))
		usleep_range(1000, 1500);

	if (!retry)
		pr_err("Failed to enter MAXFIFO mode.\n");

	if (REG_READ(DSPSRCTRL) & MAXFIFO_STATUS)
		pr_debug("Entered MAXFIFO mode.\n");

	return 0;
}

int dsi_report_repeated_frame(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_sdo *sdo;
	int sdo_level = SDO_INIT;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	sdo = config->sdo;
	if (!sdo)
		return 0;

	sdo_level = SDO_ENTERED_LEVEL0;

	mutex_lock(&config->ctx_lock);

	if (!sdo->sdo_enabled)
		goto repeated_frame_out;

	/* FIXME: if panel is off, then ignore it */

	/*
	 * To enter MAXFIFO mode ASAP, because the Repeated Frame interrupt has
	 * been disabled at top half of IRQ handler, and the work couldn't be
	 * scheduled any more.
	 */
	if (sdo_level <= sdo->sdo_state)
		goto repeated_frame_out;
	else if (++sdo->free_count > SDO_COUNT && sdo->ref_count > 0) {
		/* reset free count */
		sdo->free_count = 0;
		/* enter SDO */
		err = enter_sdo(pipe, sdo_level);
		if (err) {
			pr_debug("Failed to enter SDO\n");
			goto repeated_frame_out;
		}
		sdo->sdo_state = sdo_level;
		sdo->ref_count = 0;
	}
repeated_frame_out:
	mutex_unlock(&config->ctx_lock);
	return err;
}

int dsi_sdo_forbid(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_sdo *sdo;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	sdo = config->sdo;
	if (!sdo)
		return 0;

	mutex_lock(&config->ctx_lock);

	/*exit SDO if necessary*/
	if (!sdo->sdo_enabled)
		goto forbid_out;

	pr_debug("\n");

	/*if reference count is not 0, it means SDO was forbidden*/
	if (sdo->ref_count) {
		sdo->ref_count++;
		goto forbid_out;
	}

	/*exited SDO if current SDO state is SDO_ENTERED*/
	if ((sdo->sdo_state > SDO_EXITED) || (sdo->sdo_state == SDO_INIT)) {
		err = exit_sdo(pipe);
		if (err) {
			pr_err("Failed to exit SDO\n");
			goto forbid_out;
		}
		sdo->sdo_state = SDO_EXITED;
	}

	sdo->ref_count++;
forbid_out:
	mutex_unlock(&config->ctx_lock);
	return err;
}

int dsi_sdo_allow(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_sdo *sdo;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	sdo = config->sdo;
	if (!sdo)
		return 0;

	mutex_lock(&config->ctx_lock);

	if (!sdo->sdo_enabled)
		goto allow_out;

	if (!sdo->ref_count)
		goto allow_out;

	pr_debug("\n");

	sdo->ref_count--;
allow_out:
	mutex_unlock(&config->ctx_lock);
	return 0;
}

void dsi_sdo_enable(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_sdo *sdo;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return;
	}

	pr_debug("\n");

	sdo = config->sdo;
	if (!sdo)
		return;

	mutex_lock(&config->ctx_lock);
	sdo->sdo_enabled = 1;
	mutex_unlock(&config->ctx_lock);
}

void dsi_sdo_disable(struct dsi_pipe *pipe)
{
	struct dsi_config *config;
	struct dsi_sdo *sdo;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return;
	}

	pr_debug("\n");

	sdo = config->sdo;
	if (!sdo)
		return;

	mutex_lock(&config->ctx_lock);
	sdo->sdo_enabled = 0;
	sdo->ref_count = 0;
	sdo->free_count = 0;
	sdo->sdo_state = SDO_INIT;
	mutex_unlock(&config->ctx_lock);
}

static void __dpi_sdo_repeated_frame_work(struct work_struct *work)
{
	struct dsi_sdo *sdo =
		container_of(work, struct dsi_sdo, repeated_frm_work);
	struct dsi_config *config = (struct dsi_config *)sdo->config;
	struct dsi_pipe *pipe = container_of(config, struct dsi_pipe, config);

	dsi_report_repeated_frame(pipe);
}

/**
 * Init SDO structure
 */
int dsi_sdo_init(struct dsi_pipe *pipe)
{
	struct dsi_sdo *sdo;

	pr_debug("\n");

	if (!pipe) {
		pr_err("%s: invalid arguments", __func__);
		return -EINVAL;
	}

	sdo = kzalloc(sizeof(struct dsi_sdo), GFP_KERNEL);
	if (!sdo) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	/*set SDO state*/
	sdo->sdo_state = SDO_INIT;

	/*init DSI config*/
	sdo->config = &pipe->config;

	pipe->config.sdo = sdo;

	INIT_WORK(&sdo->repeated_frm_work, __dpi_sdo_repeated_frame_work);

	return 0;
}

/**
 * destroy SDO structure
 */
void dsi_sdo_destroy(struct dsi_config *config)
{
	struct dsi_sdo *sdo;

	pr_debug("\n");

	if (config) {
		sdo = config->sdo;
		kfree(sdo);
		config->sdo = NULL;
	}
}
