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
 *
 *
 * Authors:
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>

#include <core/vlv/vlv_dc_config.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/intel_dc_config.h>
#include <intel_adf.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/common/dsi/dsi_pipe.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>

void vlv_vblank_on(int pipe)
{
	u32 val = REG_READ(PIPESTAT(pipe));
	if (val & PIPE_VBLANK_INTERRUPT_ENABLE) {
		pr_info("ADF: %s: vblank already on for pipe = %d\n",
			__func__, pipe);
	} else {
		REG_WRITE(PIPESTAT(pipe),
			 (val | PIPE_VBLANK_INTERRUPT_ENABLE));
		REG_POSTING_READ(PIPESTAT(pipe));
	}
}

void vlv_vblank_off(int pipe)
{
	u32 val = REG_READ(PIPESTAT(pipe));
	if (val & PIPE_VBLANK_INTERRUPT_ENABLE) {
		REG_WRITE(PIPESTAT(pipe),
				val & ~PIPE_VBLANK_INTERRUPT_ENABLE);
		REG_POSTING_READ(PIPESTAT(pipe));
	} else
		pr_info("ADF: %s: vblank already off for pipe = %d\n",
			__func__, pipe);
}

void vlv_wait_for_vblank(int pipe)
{
	u32 frame, frame_reg = PIPE_FRMCOUNT_GM45(pipe);

	frame = REG_READ(frame_reg);

	if (wait_for(REG_POSTING_READ(frame_reg) != frame, 50))
		pr_info("ADF: %s: vblank wait timed out\n", __func__);
}

void vlv_wait_for_pipe_off(int pipe)
{
	int reg = PIPECONF(pipe);

	/* Wait for the Pipe State to go off */
	if (wait_for((REG_READ(reg) & I965_PIPECONF_ACTIVE) == 0, 100))
		pr_err("ADF: %s: pipe_off wait timed out\n", __func__);
}

int vlv_display_on(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = NULL;
	struct drm_mode_modeinfo mode;
	int reg, i;
	u32 val = 0;
	u8 index;
	bool is_dsi = pipe->type == INTEL_PIPE_DSI ? true : false;

	if (!pipe)
		return -EINVAL;

	index = pipe->base.idx;

	if (is_dsi) {
		dsi_pipe = to_dsi_pipe(pipe);

		/* encoder enable */
		dsi_pipe->ops.power_on(dsi_pipe);

		/* get the configured mode */
		dsi_pipe->panel->ops->get_config_mode(&dsi_pipe->config, &mode);
		dsi_pipe->dpms_state = DRM_MODE_DPMS_ON;
	}

	pipe_mode_set(pipe, &mode);

	/* FIXME: Enable PF here if needed */

	/* Load default gamma LUT */
	reg = PALETTE(0);
	for (i = 0; i < 256; i++) {
		REG_WRITE(reg + 4 * i,
			(dsi_pipe->config.lut_r[i] << 16) |
			(dsi_pipe->config.lut_g[i] << 8) |
			(dsi_pipe->config.lut_b[i]));
	}

	/* Enable pipe */
	reg = PIPECONF(index);
	val = REG_READ(reg);
	if (val & PIPECONF_ENABLE)
		pr_err("ADF: %s: Pipe already on !!\n", __func__);
	else {
		REG_WRITE(reg, val | PIPECONF_ENABLE);
		REG_POSTING_READ(reg);
	}

	/* program Gamma enable */
	val = REG_READ(DSPCNTR(index)) | DISPPLANE_GAMMA_ENABLE;

	/* disable rotation for now */
	val &= ~(1 << 15);
	REG_WRITE(DSPCNTR(index), val);
	REG_POSTING_READ(DSPCNTR(index));

	vlv_vblank_on(index);

	/* enable vsyncs */
	pipe->ops->set_event(pipe, INTEL_PIPE_EVENT_VSYNC, true);

	/* Enable DPST */
	vlv_dpst_display_on();

	return 0;
}

int vlv_display_off(struct intel_pipe *pipe)
{
	int i, reg;
	u32 val = 0;
	u8 index;
	struct dsi_pipe *dsi = NULL;
	int is_dsi = pipe->type == INTEL_PIPE_DSI ? true : false;

	pr_debug("ADF: %s\n", __func__);

	if (!pipe)
		return -EINVAL;

	if (is_dsi) {
		dsi = to_dsi_pipe(pipe);

		/* check harwdare state before disabling */
		if (!dsi->ops.get_hw_state(dsi)) {
			pr_err("%s: DSI device already disabled\n", __func__);
			return 0;
		}
	}

	index = pipe->base.idx;

	/* Disable DPST */
	vlv_dpst_display_off();

	/* disable vsyncs */
	pipe->ops->set_event(pipe, INTEL_PIPE_EVENT_VSYNC, false);

	 /* encoder specifific disabling if needed */
	if (is_dsi)
		/* DSI Shutdown command */
		dsi->ops.pre_power_off(dsi);

	/* Also check for pending flip and the vblank off  */

	/* Disable Sprite planes */
	for (i = 0; i < VLV_NUM_SPRITES; i++) {
		REG_WRITE(SPCNTR(index, i), REG_READ(SPCNTR(index, i)) &
				~SP_ENABLE);

		/* Activate double buffered register update */
		I915_MODIFY_DISPBASE(SPSURF(index, i), 0);
		REG_POSTING_READ(SPSURF(index, i));
	}

	vlv_vblank_off(index);

	/* Disable primary plane */
	reg = DSPCNTR(index);
	val = REG_READ(reg);
	if (val & DISPLAY_PLANE_ENABLE) {
		REG_WRITE(reg, val & ~DISPLAY_PLANE_ENABLE);
		REG_WRITE(DSPSURF(index), REG_READ(DSPSURF(index)));
	} else
		pr_info("ADF:%s: primary plane already disabled on pipe = %d\n",
			__func__, index);

	/* Disable pipe */
	reg = PIPECONF(index);
	val = REG_READ(reg);
	if ((val & PIPECONF_ENABLE) == 0)
		pr_info("ADF: %s: pipe already off\n", __func__);
	else {
		REG_WRITE(reg, val & ~PIPECONF_ENABLE);
		vlv_wait_for_pipe_off(index);
		REG_WRITE(PFIT_CONTROL, 0);
	}

	/* TODO
	* Interface specific encoder post disable should be done here */

	/* encoder off interface specific */
	if (is_dsi) {
		dsi->ops.power_off(dsi);
		dsi->dpms_state = DRM_MODE_DPMS_OFF;
	}
	/*
	 * Disable PLL
	 * Needed for interfaces other than DSI
	 */

	/*TODO*/
	/* Power gate DPIO RX Lanes */

	return 0;
}

int pipe_mode_set(struct intel_pipe *pipe, struct drm_mode_modeinfo *mode)
{
	int vblank_start;
	int vblank_end;
	int hblank_start;
	int hblank_end;
	uint32_t pipeconf = 0;
	u8 index = pipe->base.idx;

	vblank_start = min(mode->vsync_start, mode->vdisplay);
	vblank_end = max(mode->vsync_end, mode->vtotal);
	hblank_start = min(mode->hsync_start, mode->hdisplay);
	hblank_end = max(mode->hsync_end, mode->htotal);

	REG_WRITE(HTOTAL(index),
		(mode->hdisplay - 1) |
		((mode->htotal - 1) << 16));

	REG_WRITE(HBLANK(index),
		(hblank_start - 1) |
		((hblank_end - 1) << 16));

	REG_WRITE(HSYNC(index),
		(mode->hsync_start - 1) |
		((mode->hsync_end - 1) << 16));

	REG_WRITE(VTOTAL(index),
		(mode->vdisplay - 1) |
		((mode->vtotal - 1) << 16));

	REG_WRITE(VBLANK(index),
		(vblank_start - 1) |
		((vblank_end - 1) << 16));

	REG_WRITE(VSYNC(index),
		(mode->vsync_start - 1) |
		((mode->vsync_end - 1) << 16));

	/* pipesrc controls the size that is scaled from, which should
	* always be the user's requested size.
	*/
	REG_WRITE(PIPESRC(index),
		((mode->hdisplay - 1) << 16) | (mode->vdisplay - 1));

	/* pipesrc and dspsize control the size that is scaled from,
	* which should always be the user's requested size.
	*/

	REG_WRITE(DSPSIZE(index),
		((mode->vdisplay - 1) << 16) |
		(mode->hdisplay - 1));
	REG_WRITE(DSPPOS(index), 0);

	pipeconf |= PIPECONF_PROGRESSIVE;
	REG_WRITE(PIPECONF(index), pipeconf);
	REG_POSTING_READ(PIPECONF(index));

	/* TODO primary plane fb update */

	return 0;
}
