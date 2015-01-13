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

#include <drm/drmP.h>
#include <intel_adf_device.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_pipe.h>

bool vlv_pipe_vblank_on(struct vlv_pipe *pipe)
{
	u32 val = REG_READ(pipe->status_offset);
	if (val & PIPE_VBLANK_INTERRUPT_ENABLE) {
		pr_info("ADF: %s: vblank already on for pipe = %x\n",
			__func__, pipe->offset);
	} else {
		REG_WRITE(pipe->status_offset,
			 (val | PIPE_VBLANK_INTERRUPT_ENABLE));
		REG_POSTING_READ(pipe->status_offset);
	}

	return true;
}


bool vlv_pipe_vblank_off(struct vlv_pipe *pipe)
{
	u32 val = REG_READ(pipe->status_offset);
	if (val & PIPE_VBLANK_INTERRUPT_ENABLE) {
		REG_WRITE(pipe->status_offset,
				val & ~PIPE_VBLANK_INTERRUPT_ENABLE);
		REG_POSTING_READ(pipe->status_offset);
	} else
		pr_info("ADF: %s: vblank already off for pipe = %x\n",
			__func__, pipe->offset);

	return true;
}

bool vlv_pipe_wait_for_vblank(struct vlv_pipe *pipe)
{
	u32 frame;
	bool ret = true;

	frame = REG_READ(pipe->frame_count_offset);

	if (wait_for(REG_POSTING_READ(pipe->frame_count_offset) != frame, 50)) {
		pr_info("ADF: %s: vblank wait timed out\n", __func__);
		ret = false;
	}

	return ret;
}

static unsigned int usecs_to_scanlines(struct drm_mode_modeinfo *mode,
				unsigned int usecs)
{
	/* paranoia */
	if (!mode->htotal)
		return 1;

	return DIV_ROUND_UP(usecs * mode->clock,
			    1000 * mode->htotal);
}

void vlv_pipe_evade_vblank(struct vlv_pipe *pipe,
	struct drm_mode_modeinfo *mode, bool *wait_for_vblank)
{
	u32 val, min, max;
	long timeout = msecs_to_jiffies(3);

	/* FIXME needs to be calibrated sensibly */
	min = mode->vdisplay - usecs_to_scanlines(mode, 50);
	max = mode->vdisplay - 1;

	/* FIXME: why is irq_disable required here ? */
	local_irq_disable();
	val = REG_READ(pipe->scan_line_offset);
	local_irq_enable();

	while (val >= min && val <= max && timeout > 0) {
		vlv_pipe_wait_for_vblank(pipe);
		local_irq_disable();
		val = REG_READ(pipe->scan_line_offset);
		local_irq_enable();
		*wait_for_vblank = false;
	}

	if (val >= min && val <= max)
		pr_warn("ADF: Page flipping close to vblank start\n"
			"(DSL=%u, VBL=%u)\n", val, mode->vdisplay);
}

u32 vlv_pipe_get_event(struct vlv_pipe *pipe, u32 *event)
{
	u32 pipestat = 0, value = 0;

	pipestat = REG_READ(pipe->status_offset);

	*event = 0;

	pr_debug("%s: PIPESTAT = 0x%x\n", __func__, pipestat);

	/* FIFO under run */
	if (pipestat & FIFO_UNDERRUN_STAT) {
		*event |= INTEL_PIPE_EVENT_UNDERRUN;
		value |= FIFO_UNDERRUN_STAT;
	}

	/* Sprite B Flip done interrupt */
	if (pipestat & SPRITE2_FLIP_DONE_STAT) {
		*event |= INTEL_PIPE_EVENT_SPRITE2_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;
	}

	/* Sprite A Flip done interrupt */
	if (pipestat & SPRITE1_FLIP_DONE_STAT) {
		*event |= INTEL_PIPE_EVENT_SPRITE1_FLIP;
		value |= SPRITE2_FLIP_DONE_STAT;
	}

	/* Plane A Flip done interrupt */
	if (pipestat & PLANE_FLIP_DONE_STAT) {
		*event |= INTEL_PIPE_EVENT_PRIMARY_FLIP;
		value |= PLANE_FLIP_DONE_STAT;
	}

	/* Vsync interrupt */
	if (pipestat & VSYNC_STAT) {
		*event |= INTEL_PIPE_EVENT_VSYNC;
		value |= VSYNC_STAT;
	}

	/* DPST event */
	if (pipestat & DPST_EVENT_STAT) {
		*event |= INTEL_PIPE_EVENT_DPST;
		value |= DPST_EVENT_STAT;
	}

	/* Clear the 1st level interrupt. */
	REG_WRITE(pipe->status_offset, pipestat | value);

	return 0;
}

u32 vlv_pipe_set_event(struct vlv_pipe *pipe, u32 event, bool enabled)
{
	u32 pipestat = 0, value = 0;
	u32 err = 0;

	if ((enabled == false) && (event == INTEL_PIPE_EVENT_VSYNC)) {
		pr_debug("ADF: %s: Not allowing VSYNC OFF\n", __func__);
		return 0;
	}

	switch (event) {
	case INTEL_PIPE_EVENT_SPRITE2_FLIP:
		pipestat = SPRITE2_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_SPRITE1_FLIP:
		pipestat = SPRITE1_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_PRIMARY_FLIP:
		pipestat = PLANE_FLIP_DONE_EN;
		break;
	case INTEL_PIPE_EVENT_VSYNC:
		pipestat = VSYNC_EN;
		break;
	case INTEL_PIPE_EVENT_DPST:
		pipestat = DPST_EVENT_EN;
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0)
		goto out;

	value = REG_READ(pipe->status_offset);

	if (enabled)
		/* Enable interrupts */
		REG_WRITE(pipe->status_offset, value | pipestat);
	else
		/* Disable interrupts */
		REG_WRITE(pipe->status_offset, value & (~pipestat));
out:
	return err;
}


u32 vlv_pipe_enable(struct vlv_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	u32 val = 0;
	u32 err = 0;

	val = REG_READ(pipe->offset);
	val |= PIPECONF_ENABLE;
	REG_WRITE(pipe->offset, val);

	/* temp to avoid unused variable error */
	pr_info("ADF: %s:%d\n", __func__, mode->vdisplay);

	return err;

}

u32 vlv_pipe_disable(struct vlv_pipe *pipe)
{
	u32 val = 0;
	u32 err = 0;

	val = REG_READ(pipe->offset);
	val &= ~PIPECONF_ENABLE;
	REG_WRITE(pipe->offset, val);

	/* Wait for the Pipe State to go off */
	if (wait_for(!(REG_READ(pipe->offset) & I965_PIPECONF_ACTIVE), 100)) {
		pr_err("ADF: %s: pipe_off wait timed out\n", __func__);
		err = -EINVAL; /* FIXME: replace with timeout error :) */
	}

	return err;
}

bool vlv_pipe_wait_for_pll_lock(struct vlv_pipe *pipe)
{
	u32 err = 0;
	if (wait_for(REG_READ(pipe->offset) & PIPECONF_DSI_PLL_LOCKED, 20)) {
		pr_err("DSI PLL lock failed\n");
		err = -EINVAL; /* FIXME: assign correct error */
	}

	return err;
}

u32 vlv_pipe_program_timings(struct vlv_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	int vblank_start;
	int vblank_end;
	int hblank_start;
	int hblank_end;
	u32 pipeconf = 0;
	int i = 0;

	vblank_start = min(mode->vsync_start, mode->vdisplay);
	vblank_end = max(mode->vsync_end, mode->vtotal);
	hblank_start = min(mode->hsync_start, mode->hdisplay);
	hblank_end = max(mode->hsync_end, mode->htotal);

	REG_WRITE(pipe->htotal_offset,
		(mode->hdisplay - 1) |
		((mode->htotal - 1) << 16));

	REG_WRITE(pipe->hblank_offset,
		(hblank_start - 1) |
		((hblank_end - 1) << 16));

	REG_WRITE(pipe->hsync_offset,
		(mode->hsync_start - 1) |
		((mode->hsync_end - 1) << 16));

	REG_WRITE(pipe->vtotal_offset,
		(mode->vdisplay - 1) |
		((mode->vtotal - 1) << 16));

	REG_WRITE(pipe->vblank_offset,
		(vblank_start - 1) |
		((vblank_end - 1) << 16));

	REG_WRITE(pipe->vsync_offset,
		(mode->vsync_start - 1) |
		((mode->vsync_end - 1) << 16));

	/*
	 * pipesrc controls the size that is scaled from, which should
	 * always be the user's requested size.
	 */
	REG_WRITE(pipe->src_size_offset,
		((mode->hdisplay - 1) << 16) | (mode->vdisplay - 1));

	/*
	 * pipesrc and dspsize control the size that is scaled from,
	 * which should always be the user's requested size.
	 */

	pipeconf |= PIPECONF_PROGRESSIVE;
	REG_WRITE(pipe->offset, pipeconf);
	REG_POSTING_READ(pipe->offset);

	/* FIXME: make this separate func and use passed values */
	/* Load default gamma LUT */
	for (i = 0; i < 256; i++) {
		REG_WRITE(pipe->gamma_offset + 4 * i,
			(i << 16) |
			(i << 8) |
			(i));
	}

	/* TODO primary plane fb update */

	return 0;
}

bool vlv_pipe_init(struct vlv_pipe *pipe, enum pipe pipeid)
{
	pipe->offset = PIPECONF(pipeid);
	pipe->status_offset = PIPESTAT(pipeid);
	pipe->scan_line_offset = PIPEDSL(pipeid);
	pipe->frame_count_offset = PIPE_FRMCOUNT_GM45(pipeid);

	pipe->htotal_offset = HTOTAL(pipeid);
	pipe->hblank_offset = HBLANK(pipeid);
	pipe->hsync_offset = HSYNC(pipeid);
	pipe->vtotal_offset = VTOTAL(pipeid);
	pipe->vblank_offset = VBLANK(pipeid);
	pipe->vsync_offset = VSYNC(pipeid);
	pipe->gamma_offset = PALETTE(pipeid);

	pipe->src_size_offset = PIPESRC(pipeid);

	pipe->pipe_id = pipeid;

	return true;
}

bool vlv_pipe_destroy(struct vlv_pipe *pipe)
{

	return true;
}
