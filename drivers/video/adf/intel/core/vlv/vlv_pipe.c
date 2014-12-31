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

#define PIPE_B_SIZE 0x1E1A0C

void vlv_pipe_program_m_n(struct vlv_pipe *pipe, struct intel_link_m_n *m_n)
{
	REG_WRITE(pipe->datam1_offset, TU_SIZE(m_n->tu) | m_n->gmch_m);
	REG_WRITE(pipe->datan1_offset, m_n->gmch_n);
	REG_WRITE(pipe->linkm1_offset, m_n->link_m);
	REG_WRITE(pipe->linkn1_offset, m_n->link_n);
}

void vlv_pipe_program_m2_n2(struct vlv_pipe *pipe, struct intel_link_m_n *m_n)
{
	REG_WRITE(pipe->datam2_offset, TU_SIZE(m_n->tu) | m_n->gmch_m);
	REG_WRITE(pipe->datan2_offset, m_n->gmch_n);
	REG_WRITE(pipe->linkm2_offset, m_n->link_m);
	REG_WRITE(pipe->linkn2_offset, m_n->link_n);
}

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

	val = REG_READ(pipe->status_offset);
	val |= PIPE_VSYNC_INTERRUPT_ENABLE;
	REG_WRITE(pipe->status_offset, val);

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

	val = REG_READ(pipe->status_offset);
	val &= ~PIPE_VSYNC_INTERRUPT_ENABLE;
	REG_WRITE(pipe->status_offset, val);

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

static u32 usecs_to_scanlines(struct drm_mode_modeinfo *mode,
				u32 usecs)
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
	struct vlv_pipeline *vlv_pipeline = container_of(pipe,
			struct vlv_pipeline, pipe);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	u32 val = 0;
	u32 err = 0;

	val = REG_READ(pipe->offset);
	val |= PIPECONF_ENABLE;
	REG_WRITE(pipe->offset, val);
	vlv_update_pipe_status(intel_config, pipe->pipe_id, true);

	/* temp to avoid unused variable error */
	pr_info("ADF: %s:%d\n", __func__, mode->vdisplay);

	return err;

}

int vlv_pipe_disable(struct vlv_pipe *pipe)
{
	struct vlv_pipeline *vlv_pipeline = container_of(pipe,
			struct vlv_pipeline, pipe);
	struct intel_dc_config *intel_config = &vlv_pipeline->config->base;

	u32 val = 0;
	u32 err = 0;

	val = REG_READ(pipe->offset);
	val |= PIPECONF_PLANE_DISABLE;
	REG_WRITE(pipe->offset, val);
	vlv_pipe_wait_for_vblank(pipe);

	val = REG_READ(pipe->offset);
	val &= ~(PIPECONF_ENABLE);
	val &= ~(DITHERING_TYPE_MASK | DDA_RESET | BPC_MASK |
					PIPECONF_DITHERING);
	REG_WRITE(pipe->offset, val);

	vlv_update_pipe_status(intel_config, pipe->pipe_id, false);

	/* Wait for the Pipe State to go off */
	if (wait_for(!(REG_READ(pipe->offset) & I965_PIPECONF_ACTIVE), 100)) {
		pr_err("ADF: %s: pipe_off wait timed out\n", __func__);
		err = -ETIMEDOUT;
	}

	return err;
}

bool vlv_pipe_wait_for_pll_lock(struct vlv_pipe *pipe)
{
	u32 err = 0;
	if (wait_for(vlv_cck_read(CCK_REG_DSI_PLL_CONTROL) &
			DSI_PLL_LOCK, 20)) {
		pr_err("DSI PLL lock failed\n");
		err = -EINVAL; /* FIXME: assign correct error */
	}

	return err;
}

u32 vlv_pipe_program_timings(struct vlv_pipe *pipe,
		struct drm_mode_modeinfo *mode,
		enum intel_pipe_type type, u8 bpp)
{
	u32 vblank_start;
	u32 vblank_end;
	u32 hblank_start;
	u32 hblank_end;
	u32 pipeconf = 0;
	u32 i = 0;

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
	 * FIXME: DRM has special case here check if it is required
	 * pipesrc controls the size that is scaled from, which should
	 * always be the user's requested size.
	 */
	REG_WRITE(pipe->src_size_offset,
			((mode->hdisplay - 1) << 16) | (mode->vdisplay - 1));

	if (pipe->pipe_id == PIPE_B)
		REG_WRITE(PIPE_B_SIZE, (mode->hdisplay - 1) |
				((mode->vdisplay - 1) << 16));

	/*
	 * FIXME: check if dithering needs checing here or later
	 * if (intel_crtc->config.dither && intel_crtc->config.pipe_bpp != 30)
	 *      pipeconf |= PIPECONF_DITHER_EN |
	 *              PIPECONF_DITHER_TYPE_SP;
	 * pipeconf |= PIPECONF_COLOR_RANGE_SELECT;
	 */

	switch (bpp) {
	case 18:
		pipeconf |= PIPECONF_6BPC;
		break;
	case 24:
		pipeconf |= PIPECONF_8BPC;
		break;
	case 30:
		pipeconf |= PIPECONF_10BPC;
		break;
	default:
		/* Case prevented by intel_choose_pipe_bpp_dither. */
		pr_err("%s: invalid bpp passed\n", __func__);
	}

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		pipeconf |= PIPECONF_INTERLACE_W_SYNC_SHIFT;
	else
		pipeconf |= PIPECONF_PROGRESSIVE;

	/* clear the plane disable bits */
	pipeconf &= ~PIPECONF_PLANE_DISABLE;

	/*
	 * FIXME: enable when color ranges are supported
	 * pipeconf |= PIPECONF_COLOR_RANGE_SELECT;
	 */

	REG_WRITE(pipe->offset, pipeconf);
	REG_POSTING_READ(pipe->offset);

	/* clear flags by writing back 1 */
	pipeconf = REG_READ(pipe->status_offset);
	REG_WRITE(pipe->status_offset, pipeconf);

	/* FIXME: make this separate func and use passed values */
	/* Load default gamma LUT */
	for (i = 0; i < 256; i++) {
		REG_WRITE(pipe->gamma_offset + 4 * i,
			(i << 16) |
			(i << 8) |
			(i));
	}

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

	pipe->datam1_offset = PIPE_DATA_M1(pipeid);
	pipe->datan1_offset = PIPE_DATA_N1(pipeid);
	pipe->linkm1_offset = PIPE_LINK_M1(pipeid);
	pipe->linkn1_offset = PIPE_LINK_N1(pipeid);

	pipe->datam2_offset = PIPE_DATA_M2(pipeid);
	pipe->datan2_offset = PIPE_DATA_N2(pipeid);
	pipe->linkm2_offset = PIPE_LINK_M2(pipeid);
	pipe->linkn2_offset = PIPE_LINK_N2(pipeid);

	pipe->psr_ctrl_offset = VLV_EDP_PSR_CONTROL(pipeid);
	pipe->psr_sts_offset = VLV_EDP_PSR_STATUS(pipeid);
	pipe->psr_crc1_offset = VLV_EDP_PSR_CRC1(pipeid);
	pipe->psr_crc2_offset = VLV_EDP_PSR_CRC2(pipeid);
	pipe->psr_vsc_sdp_offset = VLV_EDP_PSR_VSC_SDP(pipeid);

	pipe->src_size_offset = PIPESRC(pipeid);
	pipe->pipe_id = pipeid;

#ifdef INTEL_ADF_DUMP_INIT_REGS
	pr_info("ADF: Pipe regs are:\n");
	pr_info("=====================================\n");
	pr_info("conf=0x%x status=0x%x scan_line=0x%x frame_count=0x%x\n",
		pipe->offset, pipe->status_offset,
		pipe->scan_line_offset, pipe->frame_count_offset);
	pr_info("htotal=0x%x hblank=0x%x hsync=0x%x\n",
		pipe->htotal_offset, pipe->hblank_offset, pipe->hsync_offset);
	pr_info("vtotal=0x%x vblank=0x%x vsync=0x%x\n",
		pipe->vtotal_offset, pipe->vblank_offset, pipe->vsync_offset);
	pr_info("gamma=0x%x src_sz=0x%x id=%d\n",
		pipe->gamma_offset, pipe->src_size_offset, pipe->pipe_id);
	pr_info("=====================================\n");
#endif
	return true;
}

bool vlv_pipe_destroy(struct vlv_pipe *pipe)
{

	return true;
}

/*
 * Take interface level configurations to pipe varibles
 * to use them in flip time.
 */
void vlv_pipe_pre_validate(struct intel_pipe *pipe,
		struct intel_adf_post_custom_data *custom)
{
	struct intel_pipeline *pipeline = pipe->pipeline;
	struct vlv_pipeline *vlv_pipeline = to_vlv_pipeline(pipeline);
	struct intel_adf_config *custom_cfg;
	int i;

	for (i = 0; i < custom->n_configs; i++) {
		custom_cfg = &custom->configs[i];

		if ((custom_cfg->type == INTEL_ADF_CONFIG_COLOR) &&
		    (pipe->base.idx == PIPE_B) &&
		    (custom_cfg->color.flags & INTEL_ADF_COLOR_CANVAS)) {

			vlv_pipeline->pplane.canvas_updated = true;
			vlv_pipeline->pplane.canvas_col =
				custom_cfg->color.color;
		}
	}
}
