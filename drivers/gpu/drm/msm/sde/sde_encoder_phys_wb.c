/*
 * Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/debugfs.h>

#include "sde_encoder_phys.h"
#include "sde_formats.h"
#include "sde_hw_top.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_wb.h"
#include "sde_vbif.h"

#define to_sde_encoder_phys_wb(x) \
	container_of(x, struct sde_encoder_phys_wb, base)

#define WBID(wb_enc) ((wb_enc) ? wb_enc->wb_dev->wb_idx : -1)

/**
 * sde_encoder_phys_wb_is_master - report wb always as master encoder
 */
static bool sde_encoder_phys_wb_is_master(struct sde_encoder_phys *phys_enc)
{
	return true;
}

/**
 * sde_encoder_phys_wb_get_intr_type - get interrupt type based on block mode
 * @hw_wb:	Pointer to h/w writeback driver
 */
static enum sde_intr_type sde_encoder_phys_wb_get_intr_type(
		struct sde_hw_wb *hw_wb)
{
	return (hw_wb->caps->features & BIT(SDE_WB_BLOCK_MODE)) ?
			SDE_IRQ_TYPE_WB_ROT_COMP : SDE_IRQ_TYPE_WB_WFD_COMP;
}

/**
 * sde_encoder_phys_wb_set_ot_limit - set OT limit for writeback interface
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_set_ot_limit(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_vbif_set_ot_params ot_params;

	memset(&ot_params, 0, sizeof(ot_params));
	ot_params.xin_id = hw_wb->caps->xin_id;
	ot_params.num = hw_wb->idx - WB_0;
	ot_params.width = wb_enc->wb_roi.w;
	ot_params.height = wb_enc->wb_roi.h;
	ot_params.is_wfd = true;
	ot_params.frame_rate = phys_enc->cached_mode.vrefresh;
	ot_params.vbif_idx = hw_wb->caps->vbif_idx;
	ot_params.clk_ctrl = hw_wb->caps->clk_ctrl;
	ot_params.rd = false;

	sde_vbif_set_ot_limit(phys_enc->sde_kms, &ot_params);
}

/**
 * sde_encoder_phys_wb_set_traffic_shaper - set traffic shaper for writeback
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_set_traffic_shaper(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb_cfg *wb_cfg = &wb_enc->wb_cfg;

	/* traffic shaper is only enabled for rotator */
	wb_cfg->ts_cfg.en = false;
}

/**
 * sde_encoder_phys_setup_cdm - setup chroma down block
 * @phys_enc:	Pointer to physical encoder
 * @fb:		Pointer to output framebuffer
 * @format:	Output format
 */
void sde_encoder_phys_setup_cdm(struct sde_encoder_phys *phys_enc,
		struct drm_framebuffer *fb, const struct sde_format *format,
		struct sde_rect *wb_roi)
{
	struct sde_hw_cdm *hw_cdm = phys_enc->hw_cdm;
	struct sde_hw_cdm_cfg *cdm_cfg = &phys_enc->cdm_cfg;
	int ret;

	if (!SDE_FORMAT_IS_YUV(format)) {
		SDE_DEBUG("[cdm_disable fmt:%x]\n",
				format->base.pixel_format);

		if (hw_cdm && hw_cdm->ops.disable)
			hw_cdm->ops.disable(hw_cdm);

		return;
	}

	memset(cdm_cfg, 0, sizeof(struct sde_hw_cdm_cfg));

	cdm_cfg->output_width = wb_roi->w;
	cdm_cfg->output_height = wb_roi->h;
	cdm_cfg->output_fmt = format;
	cdm_cfg->output_type = CDM_CDWN_OUTPUT_WB;
	cdm_cfg->output_bit_depth = SDE_FORMAT_IS_DX(format) ?
		CDM_CDWN_OUTPUT_10BIT : CDM_CDWN_OUTPUT_8BIT;

	/* enable 10 bit logic */
	switch (cdm_cfg->output_fmt->chroma_sample) {
	case SDE_CHROMA_RGB:
		cdm_cfg->h_cdwn_type = CDM_CDWN_DISABLE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	case SDE_CHROMA_H2V1:
		cdm_cfg->h_cdwn_type = CDM_CDWN_COSITE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	case SDE_CHROMA_420:
		cdm_cfg->h_cdwn_type = CDM_CDWN_COSITE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_OFFSITE;
		break;
	case SDE_CHROMA_H1V2:
	default:
		SDE_ERROR("unsupported chroma sampling type\n");
		cdm_cfg->h_cdwn_type = CDM_CDWN_DISABLE;
		cdm_cfg->v_cdwn_type = CDM_CDWN_DISABLE;
		break;
	}

	SDE_DEBUG("[cdm_enable:%d,%d,%X,%d,%d,%d,%d]\n",
			cdm_cfg->output_width,
			cdm_cfg->output_height,
			cdm_cfg->output_fmt->base.pixel_format,
			cdm_cfg->output_type,
			cdm_cfg->output_bit_depth,
			cdm_cfg->h_cdwn_type,
			cdm_cfg->v_cdwn_type);

	if (hw_cdm && hw_cdm->ops.setup_cdwn) {
		ret = hw_cdm->ops.setup_cdwn(hw_cdm, cdm_cfg);
		if (ret < 0) {
			SDE_ERROR("failed to setup CDM %d\n", ret);
			return;
		}
	}

	if (hw_cdm && hw_cdm->ops.enable) {
		ret = hw_cdm->ops.enable(hw_cdm, cdm_cfg);
		if (ret < 0) {
			SDE_ERROR("failed to enable CDM %d\n", ret);
			return;
		}
	}
}

/**
 * sde_encoder_phys_wb_setup_fb - setup output framebuffer
 * @phys_enc:	Pointer to physical encoder
 * @fb:		Pointer to output framebuffer
 * @wb_roi:	Pointer to output region of interest
 */
static void sde_encoder_phys_wb_setup_fb(struct sde_encoder_phys *phys_enc,
		struct drm_framebuffer *fb, struct sde_rect *wb_roi)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb;
	struct sde_hw_wb_cfg *wb_cfg;
	const struct msm_format *format;
	int ret, mmu_id;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	hw_wb = wb_enc->hw_wb;
	wb_cfg = &wb_enc->wb_cfg;
	memset(wb_cfg, 0, sizeof(struct sde_hw_wb_cfg));

	wb_cfg->intf_mode = phys_enc->intf_mode;
	wb_cfg->is_secure = (fb->flags & DRM_MODE_FB_SECURE) ? true : false;
	mmu_id = (wb_cfg->is_secure) ?
			wb_enc->mmu_id[SDE_IOMMU_DOMAIN_SECURE] :
			wb_enc->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE];

	SDE_DEBUG("[fb_secure:%d]\n", wb_cfg->is_secure);

	format = msm_framebuffer_format(fb);
	if (!format) {
		SDE_DEBUG("invalid format for fb\n");
		return;
	}

	wb_cfg->dest.format = sde_get_sde_format_ext(
			format->pixel_format,
			fb->modifier,
			drm_format_num_planes(fb->pixel_format));
	if (!wb_cfg->dest.format) {
		/* this error should be detected during atomic_check */
		SDE_ERROR("failed to get format %x\n", format->pixel_format);
		return;
	}
	wb_cfg->roi = *wb_roi;

	if (hw_wb->caps->features & BIT(SDE_WB_XY_ROI_OFFSET)) {
		ret = sde_format_populate_layout(mmu_id, fb, &wb_cfg->dest);
		if (ret) {
			SDE_DEBUG("failed to populate layout %d\n", ret);
			return;
		}
		wb_cfg->dest.width = fb->width;
		wb_cfg->dest.height = fb->height;
		wb_cfg->dest.num_planes = wb_cfg->dest.format->num_planes;
	} else {
		ret = sde_format_populate_layout_with_roi(mmu_id, fb, wb_roi,
			&wb_cfg->dest);
		if (ret) {
			/* this error should be detected during atomic_check */
			SDE_DEBUG("failed to populate layout %d\n", ret);
			return;
		}
	}

	if ((wb_cfg->dest.format->fetch_planes == SDE_PLANE_PLANAR) &&
			(wb_cfg->dest.format->element[0] == C1_B_Cb))
		swap(wb_cfg->dest.plane_addr[1], wb_cfg->dest.plane_addr[2]);

	SDE_DEBUG("[fb_offset:%8.8x,%8.8x,%8.8x,%8.8x]\n",
			wb_cfg->dest.plane_addr[0],
			wb_cfg->dest.plane_addr[1],
			wb_cfg->dest.plane_addr[2],
			wb_cfg->dest.plane_addr[3]);
	SDE_DEBUG("[fb_stride:%8.8x,%8.8x,%8.8x,%8.8x]\n",
			wb_cfg->dest.plane_pitch[0],
			wb_cfg->dest.plane_pitch[1],
			wb_cfg->dest.plane_pitch[2],
			wb_cfg->dest.plane_pitch[3]);

	if (hw_wb->ops.setup_roi)
		hw_wb->ops.setup_roi(hw_wb, wb_cfg);

	if (hw_wb->ops.setup_outformat)
		hw_wb->ops.setup_outformat(hw_wb, wb_cfg);

	if (hw_wb->ops.setup_outaddress)
		hw_wb->ops.setup_outaddress(hw_wb, wb_cfg);
}

/**
 * sde_encoder_phys_wb_setup_cdp - setup chroma down prefetch block
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_setup_cdp(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_hw_intf_cfg *intf_cfg = &wb_enc->intf_cfg;

	memset(intf_cfg, 0, sizeof(struct sde_hw_intf_cfg));

	intf_cfg->intf = SDE_NONE;
	intf_cfg->wb = hw_wb->idx;
	intf_cfg->mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);

	if (phys_enc->hw_ctl && phys_enc->hw_ctl->ops.setup_intf_cfg)
		phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl,
				intf_cfg);
}

/**
 * sde_encoder_phys_wb_atomic_check - verify and fixup given atomic states
 * @phys_enc:	Pointer to physical encoder
 * @crtc_state:	Pointer to CRTC atomic state
 * @conn_state:	Pointer to connector atomic state
 */
static int sde_encoder_phys_wb_atomic_check(
		struct sde_encoder_phys *phys_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	const struct sde_wb_cfg *wb_cfg = hw_wb->caps;
	struct drm_framebuffer *fb;
	const struct sde_format *fmt;
	struct sde_rect wb_roi;
	const struct drm_display_mode *mode = &crtc_state->mode;
	int rc;

	SDE_DEBUG("[atomic_check:%d,%d,\"%s\",%d,%d]\n",
			hw_wb->idx - WB_0, mode->base.id, mode->name,
			mode->hdisplay, mode->vdisplay);

	if (!conn_state || !conn_state->connector) {
		SDE_ERROR("invalid connector state\n");
		return -EINVAL;
	} else if (conn_state->connector->status !=
			connector_status_connected) {
		SDE_ERROR("connector not connected %d\n",
				conn_state->connector->status);
		return -EINVAL;
	}

	memset(&wb_roi, 0, sizeof(struct sde_rect));

	rc = sde_wb_connector_state_get_output_roi(conn_state, &wb_roi);
	if (rc) {
		SDE_ERROR("failed to get roi %d\n", rc);
		return rc;
	}

	SDE_DEBUG("[roi:%u,%u,%u,%u]\n", wb_roi.x, wb_roi.y,
			wb_roi.w, wb_roi.h);

	fb = sde_wb_connector_state_get_output_fb(conn_state);
	if (!fb) {
		SDE_ERROR("no output framebuffer\n");
		return -EINVAL;
	}

	SDE_DEBUG("[fb_id:%u][fb:%u,%u]\n", fb->base.id,
			fb->width, fb->height);

	fmt = sde_get_sde_format_ext(fb->pixel_format, fb->modifier,
			drm_format_num_planes(fb->pixel_format));
	if (!fmt) {
		SDE_ERROR("unsupported output pixel format:%x\n",
				fb->pixel_format);
		return -EINVAL;
	}

	SDE_DEBUG("[fb_fmt:%x,%llx]\n", fb->pixel_format,
			fb->modifier[0]);

	if (SDE_FORMAT_IS_YUV(fmt) &&
			!(wb_cfg->features & BIT(SDE_WB_YUV_CONFIG))) {
		SDE_ERROR("invalid output format %x\n", fmt->base.pixel_format);
		return -EINVAL;
	}

	if (SDE_FORMAT_IS_UBWC(fmt) &&
			!(wb_cfg->features & BIT(SDE_WB_UBWC_1_0))) {
		SDE_ERROR("invalid output format %x\n", fmt->base.pixel_format);
		return -EINVAL;
	}

	if (SDE_FORMAT_IS_YUV(fmt) != !!phys_enc->hw_cdm)
		crtc_state->mode_changed = true;

	if (wb_roi.w && wb_roi.h) {
		if (wb_roi.w != mode->hdisplay) {
			SDE_ERROR("invalid roi w=%d, mode w=%d\n", wb_roi.w,
					mode->hdisplay);
			return -EINVAL;
		} else if (wb_roi.h != mode->vdisplay) {
			SDE_ERROR("invalid roi h=%d, mode h=%d\n", wb_roi.h,
					mode->vdisplay);
			return -EINVAL;
		} else if (wb_roi.x + wb_roi.w > fb->width) {
			SDE_ERROR("invalid roi x=%d, w=%d, fb w=%d\n",
					wb_roi.x, wb_roi.w, fb->width);
			return -EINVAL;
		} else if (wb_roi.y + wb_roi.h > fb->height) {
			SDE_ERROR("invalid roi y=%d, h=%d, fb h=%d\n",
					wb_roi.y, wb_roi.h, fb->height);
			return -EINVAL;
		} else if (wb_roi.w > wb_cfg->sblk->maxlinewidth) {
			SDE_ERROR("invalid roi w=%d, maxlinewidth=%u\n",
					wb_roi.w, wb_cfg->sblk->maxlinewidth);
			return -EINVAL;
		}
	} else {
		if (wb_roi.x || wb_roi.y) {
			SDE_ERROR("invalid roi x=%d, y=%d\n",
					wb_roi.x, wb_roi.y);
			return -EINVAL;
		} else if (fb->width != mode->hdisplay) {
			SDE_ERROR("invalid fb w=%d, mode w=%d\n", fb->width,
					mode->hdisplay);
			return -EINVAL;
		} else if (fb->height != mode->vdisplay) {
			SDE_ERROR("invalid fb h=%d, mode h=%d\n", fb->height,
					mode->vdisplay);
			return -EINVAL;
		} else if (fb->width > wb_cfg->sblk->maxlinewidth) {
			SDE_ERROR("invalid fb w=%d, maxlinewidth=%u\n",
					fb->width, wb_cfg->sblk->maxlinewidth);
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * sde_encoder_phys_wb_flush - flush hardware update
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_hw_ctl *hw_ctl = phys_enc->hw_ctl;
	struct sde_hw_cdm *hw_cdm = phys_enc->hw_cdm;
	u32 flush_mask = 0;

	SDE_DEBUG("[wb:%d]\n", hw_wb->idx - WB_0);

	if (!hw_ctl) {
		SDE_DEBUG("[wb:%d] no ctl assigned\n", hw_wb->idx - WB_0);
		return;
	}

	if (hw_ctl->ops.get_bitmask_wb)
		hw_ctl->ops.get_bitmask_wb(hw_ctl, &flush_mask, hw_wb->idx);

	if (hw_ctl->ops.get_bitmask_cdm && hw_cdm)
		hw_ctl->ops.get_bitmask_cdm(hw_ctl, &flush_mask, hw_cdm->idx);

	if (hw_ctl->ops.update_pending_flush)
		hw_ctl->ops.update_pending_flush(hw_ctl, flush_mask);

	SDE_DEBUG("Flushing CTL_ID %d, flush_mask %x, WB %d\n",
			hw_ctl->idx - CTL_0, flush_mask, hw_wb->idx - WB_0);
}

/**
 * sde_encoder_phys_wb_setup - setup writeback encoder
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_setup(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct drm_display_mode mode = phys_enc->cached_mode;
	struct drm_framebuffer *fb;
	struct sde_rect *wb_roi = &wb_enc->wb_roi;

	SDE_DEBUG("[mode_set:%d,%d,\"%s\",%d,%d]\n",
			hw_wb->idx - WB_0, mode.base.id, mode.name,
			mode.hdisplay, mode.vdisplay);

	memset(wb_roi, 0, sizeof(struct sde_rect));

	fb = sde_wb_get_output_fb(wb_enc->wb_dev);
	if (!fb) {
		SDE_DEBUG("no output framebuffer\n");
		return;
	}

	SDE_DEBUG("[fb_id:%u][fb:%u,%u]\n", fb->base.id,
			fb->width, fb->height);

	sde_wb_get_output_roi(wb_enc->wb_dev, wb_roi);
	if (wb_roi->w == 0 || wb_roi->h == 0) {
		wb_roi->x = 0;
		wb_roi->y = 0;
		wb_roi->w = fb->width;
		wb_roi->h = fb->height;
	}

	SDE_DEBUG("[roi:%u,%u,%u,%u]\n", wb_roi->x, wb_roi->y,
			wb_roi->w, wb_roi->h);

	wb_enc->wb_fmt = sde_get_sde_format_ext(fb->pixel_format, fb->modifier,
			drm_format_num_planes(fb->pixel_format));
	if (!wb_enc->wb_fmt) {
		SDE_ERROR("unsupported output pixel format: %d\n",
				fb->pixel_format);
		return;
	}

	SDE_DEBUG("[fb_fmt:%x,%llx]\n", fb->pixel_format,
			fb->modifier[0]);

	sde_encoder_phys_wb_set_ot_limit(phys_enc);

	sde_encoder_phys_wb_set_traffic_shaper(phys_enc);

	sde_encoder_phys_setup_cdm(phys_enc, fb, wb_enc->wb_fmt, wb_roi);

	sde_encoder_phys_wb_setup_fb(phys_enc, fb, wb_roi);

	sde_encoder_phys_wb_setup_cdp(phys_enc);
}

/**
 * sde_encoder_phys_wb_unregister_irq - unregister writeback interrupt handler
 * @phys_enc:	Pointer to physical encoder
 */
static int sde_encoder_phys_wb_unregister_irq(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;

	if (wb_enc->bypass_irqreg)
		return 0;

	sde_core_irq_disable(phys_enc->sde_kms, &wb_enc->irq_idx, 1);
	sde_core_irq_unregister_callback(phys_enc->sde_kms, wb_enc->irq_idx,
			&wb_enc->irq_cb);

	SDE_DEBUG("un-register IRQ for wb %d, irq_idx=%d\n",
			hw_wb->idx - WB_0,
			wb_enc->irq_idx);

	return 0;
}

/**
 * sde_encoder_phys_wb_done_irq - writeback interrupt handler
 * @arg:	Pointer to writeback encoder
 * @irq_idx:	interrupt index
 */
static void sde_encoder_phys_wb_done_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_wb *wb_enc = arg;
	struct sde_encoder_phys *phys_enc = &wb_enc->base;
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;

	SDE_DEBUG("[wb:%d,%u]\n", hw_wb->idx - WB_0,
			wb_enc->frame_count);

	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, SDE_ENCODER_FRAME_EVENT_DONE);

	phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
			phys_enc);

	complete_all(&wb_enc->wbdone_complete);
}

/**
 * sde_encoder_phys_wb_register_irq - register writeback interrupt handler
 * @phys_enc:	Pointer to physical encoder
 */
static int sde_encoder_phys_wb_register_irq(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_irq_callback *irq_cb = &wb_enc->irq_cb;
	enum sde_intr_type intr_type;
	int ret = 0;

	if (wb_enc->bypass_irqreg)
		return 0;

	intr_type = sde_encoder_phys_wb_get_intr_type(hw_wb);
	wb_enc->irq_idx = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			intr_type, hw_wb->idx);
	if (wb_enc->irq_idx < 0) {
		SDE_ERROR(
			"failed to lookup IRQ index for WB_DONE with wb=%d\n",
			hw_wb->idx - WB_0);
		return -EINVAL;
	}

	irq_cb->func = sde_encoder_phys_wb_done_irq;
	irq_cb->arg = wb_enc;
	ret = sde_core_irq_register_callback(phys_enc->sde_kms,
			wb_enc->irq_idx, irq_cb);
	if (ret) {
		SDE_ERROR("failed to register IRQ callback WB_DONE\n");
		return ret;
	}

	ret = sde_core_irq_enable(phys_enc->sde_kms, &wb_enc->irq_idx, 1);
	if (ret) {
		SDE_ERROR(
			"failed to enable IRQ for WB_DONE, wb %d, irq_idx=%d\n",
				hw_wb->idx - WB_0,
				wb_enc->irq_idx);
		wb_enc->irq_idx = -EINVAL;

		/* Unregister callback on IRQ enable failure */
		sde_core_irq_unregister_callback(phys_enc->sde_kms,
				wb_enc->irq_idx, irq_cb);
		return ret;
	}

	SDE_DEBUG("registered IRQ for wb %d, irq_idx=%d\n",
			hw_wb->idx - WB_0,
			wb_enc->irq_idx);

	return ret;
}

/**
 * sde_encoder_phys_wb_mode_set - set display mode
 * @phys_enc:	Pointer to physical encoder
 * @mode:	Pointer to requested display mode
 * @adj_mode:	Pointer to adjusted display mode
 */
static void sde_encoder_phys_wb_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_rm *rm = &phys_enc->sde_kms->rm;
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_rm_hw_iter iter;
	int i, instance;

	phys_enc->cached_mode = *adj_mode;
	instance = phys_enc->split_role == ENC_ROLE_SLAVE ? 1 : 0;

	SDE_DEBUG("[mode_set_cache:%d,%d,\"%s\",%d,%d]\n",
			hw_wb->idx - WB_0, mode->base.id,
			mode->name, mode->hdisplay, mode->vdisplay);

	phys_enc->hw_ctl = NULL;
	phys_enc->hw_cdm = NULL;

	/* Retrieve previously allocated HW Resources. CTL shouldn't fail */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CTL);
	for (i = 0; i <= instance; i++) {
		sde_rm_get_hw(rm, &iter);
		if (i == instance)
			phys_enc->hw_ctl = (struct sde_hw_ctl *) iter.hw;
	}

	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_ERROR("failed init ctl: %ld\n", PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}

	/* CDM is optional */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CDM);
	for (i = 0; i <= instance; i++) {
		sde_rm_get_hw(rm, &iter);
		if (i == instance)
			phys_enc->hw_cdm = (struct sde_hw_cdm *) iter.hw;
	}

	if (IS_ERR(phys_enc->hw_cdm)) {
		SDE_ERROR("CDM required but not allocated: %ld\n",
				PTR_ERR(phys_enc->hw_cdm));
		phys_enc->hw_ctl = NULL;
	}
}

/**
 * sde_encoder_phys_wb_wait_for_commit_done - wait until request is committed
 * @phys_enc:	Pointer to physical encoder
 */
static int sde_encoder_phys_wb_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	unsigned long ret;
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	u32 irq_status;
	u64 wb_time = 0;
	int rc = 0;

	/* Return EWOULDBLOCK since we know the wait isn't necessary */
	if (WARN_ON(phys_enc->enable_state != SDE_ENC_ENABLED))
		return -EWOULDBLOCK;

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count);

	ret = wait_for_completion_timeout(&wb_enc->wbdone_complete,
			KICKOFF_TIMEOUT_JIFFIES);

	if (!ret) {
		SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
				wb_enc->frame_count);

		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				wb_enc->irq_idx, true);
		if (irq_status) {
			SDE_DEBUG("wb:%d done but irq not triggered\n",
					wb_enc->wb_dev->wb_idx - WB_0);
			sde_encoder_phys_wb_done_irq(wb_enc, wb_enc->irq_idx);
		} else {
			SDE_ERROR("wb:%d kickoff timed out\n",
					wb_enc->wb_dev->wb_idx - WB_0);
			if (phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
						phys_enc->parent, phys_enc,
						SDE_ENCODER_FRAME_EVENT_ERROR);
			rc = -ETIMEDOUT;
		}
	}

	sde_encoder_phys_wb_unregister_irq(phys_enc);

	if (!rc)
		wb_enc->end_time = ktime_get();

	/* once operation is done, disable traffic shaper */
	if (wb_enc->wb_cfg.ts_cfg.en && wb_enc->hw_wb &&
			wb_enc->hw_wb->ops.setup_trafficshaper) {
		wb_enc->wb_cfg.ts_cfg.en = false;
		wb_enc->hw_wb->ops.setup_trafficshaper(
				wb_enc->hw_wb, &wb_enc->wb_cfg);
	}

	/* remove vote for iommu/clk/bus */
	wb_enc->frame_count++;

	if (!rc) {
		wb_time = (u64)ktime_to_us(wb_enc->end_time) -
				(u64)ktime_to_us(wb_enc->start_time);
		SDE_DEBUG("wb:%d took %llu us\n",
			wb_enc->wb_dev->wb_idx - WB_0, wb_time);
	}

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count,
			wb_time);

	return rc;
}

/**
 * sde_encoder_phys_wb_prepare_for_kickoff - pre-kickoff processing
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	int ret;

	SDE_DEBUG("[wb:%d,%u]\n", wb_enc->hw_wb->idx - WB_0,
			wb_enc->kickoff_count);

	reinit_completion(&wb_enc->wbdone_complete);

	ret = sde_encoder_phys_wb_register_irq(phys_enc);
	if (ret) {
		SDE_ERROR("failed to register irq %d\n", ret);
		return;
	}

	wb_enc->kickoff_count++;

	/* set OT limit & enable traffic shaper */
	sde_encoder_phys_wb_setup(phys_enc);

	sde_encoder_phys_wb_flush(phys_enc);

	/* vote for iommu/clk/bus */
	wb_enc->start_time = ktime_get();

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->kickoff_count);
}

/**
 * sde_encoder_phys_wb_handle_post_kickoff - post-kickoff processing
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_handle_post_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	SDE_DEBUG("[wb:%d]\n", wb_enc->hw_wb->idx - WB_0);

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc));
}

/**
 * sde_encoder_phys_wb_enable - enable writeback encoder
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct drm_device *dev;
	struct drm_connector *connector;

	SDE_DEBUG("[wb:%d]\n", hw_wb->idx - WB_0);

	if (!wb_enc->base.parent || !wb_enc->base.parent->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	}
	dev = wb_enc->base.parent->dev;

	/* find associated writeback connector */
	mutex_lock(&dev->mode_config.mutex);
	drm_for_each_connector(connector, phys_enc->parent->dev) {
		if (connector->encoder == phys_enc->parent)
			break;
	}
	mutex_unlock(&dev->mode_config.mutex);

	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find writeback connector\n");
		return;
	}
	wb_enc->wb_dev = sde_wb_connector_get_wb(connector);

	phys_enc->enable_state = SDE_ENC_ENABLED;
}

/**
 * sde_encoder_phys_wb_disable - disable writeback encoder
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;

	SDE_DEBUG("[wb:%d]\n", hw_wb->idx - WB_0);

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("encoder is already disabled\n");
		return;
	}

	if (wb_enc->frame_count != wb_enc->kickoff_count) {
		SDE_DEBUG("[wait_for_done: wb:%d, frame:%u, kickoff:%u]\n",
				hw_wb->idx - WB_0, wb_enc->frame_count,
				wb_enc->kickoff_count);
		sde_encoder_phys_wb_wait_for_commit_done(phys_enc);
	}

	if (phys_enc->hw_cdm && phys_enc->hw_cdm->ops.disable) {
		SDE_DEBUG_DRIVER("[cdm_disable]\n");
		phys_enc->hw_cdm->ops.disable(phys_enc->hw_cdm);
	}

	phys_enc->enable_state = SDE_ENC_DISABLED;
}

/**
 * sde_encoder_phys_wb_get_hw_resources - get hardware resources
 * @phys_enc:	Pointer to physical encoder
 * @hw_res:	Pointer to encoder resources
 */
static void sde_encoder_phys_wb_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb;
	struct drm_framebuffer *fb;
	const struct sde_format *fmt;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	fb = sde_wb_connector_state_get_output_fb(conn_state);
	if (!fb) {
		SDE_ERROR("no output framebuffer\n");
		return;
	}

	fmt = sde_get_sde_format_ext(fb->pixel_format, fb->modifier,
			drm_format_num_planes(fb->pixel_format));
	if (!fmt) {
		SDE_ERROR("unsupported output pixel format:%d\n",
				fb->pixel_format);
		return;
	}

	hw_wb = wb_enc->hw_wb;
	hw_res->wbs[hw_wb->idx - WB_0] = phys_enc->intf_mode;
	hw_res->needs_cdm = SDE_FORMAT_IS_YUV(fmt);
	SDE_DEBUG("[wb:%d] intf_mode=%d needs_cdm=%d\n", hw_wb->idx - WB_0,
			hw_res->wbs[hw_wb->idx - WB_0],
			hw_res->needs_cdm);
}

#ifdef CONFIG_DEBUG_FS
/**
 * sde_encoder_phys_wb_init_debugfs - initialize writeback encoder debugfs
 * @phys_enc:	Pointer to physical encoder
 * @sde_kms:	Pointer to SDE KMS object
 */
static int sde_encoder_phys_wb_init_debugfs(
		struct sde_encoder_phys *phys_enc, struct sde_kms *kms)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	if (!phys_enc || !kms || !wb_enc->hw_wb)
		return -EINVAL;

	snprintf(wb_enc->wb_name, ARRAY_SIZE(wb_enc->wb_name), "encoder_wb%d",
			wb_enc->hw_wb->idx - WB_0);

	wb_enc->debugfs_root =
		debugfs_create_dir(wb_enc->wb_name,
				sde_debugfs_get_root(kms));
	if (!wb_enc->debugfs_root) {
		SDE_ERROR("failed to create debugfs\n");
		return -ENOMEM;
	}

	if (!debugfs_create_u32("wbdone_timeout", S_IRUGO | S_IWUSR,
			wb_enc->debugfs_root, &wb_enc->wbdone_timeout)) {
		SDE_ERROR("failed to create debugfs/wbdone_timeout\n");
		return -ENOMEM;
	}

	if (!debugfs_create_u32("bypass_irqreg", S_IRUGO | S_IWUSR,
			wb_enc->debugfs_root, &wb_enc->bypass_irqreg)) {
		SDE_ERROR("failed to create debugfs/bypass_irqreg\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * sde_encoder_phys_wb_destroy_debugfs - destroy writeback encoder debugfs
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_destroy_debugfs(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	if (!phys_enc)
		return;

	debugfs_remove_recursive(wb_enc->debugfs_root);
}
#else
static int sde_encoder_phys_wb_init_debugfs(
		struct sde_encoder_phys *phys_enc, struct sde_kms *kms)
{
	return 0;
}
static void sde_encoder_phys_wb_destroy_debugfs(
		struct sde_encoder_phys *phys_enc)
{
}
#endif

/**
 * sde_encoder_phys_wb_destroy - destroy writeback encoder
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;

	SDE_DEBUG("[wb:%d]\n", hw_wb->idx - WB_0);

	if (!phys_enc)
		return;

	sde_encoder_phys_wb_destroy_debugfs(phys_enc);

	kfree(wb_enc);
}

/**
 * sde_encoder_phys_wb_init_ops - initialize writeback operations
 * @ops:	Pointer to encoder operation table
 */
static void sde_encoder_phys_wb_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_wb_is_master;
	ops->mode_set = sde_encoder_phys_wb_mode_set;
	ops->enable = sde_encoder_phys_wb_enable;
	ops->disable = sde_encoder_phys_wb_disable;
	ops->destroy = sde_encoder_phys_wb_destroy;
	ops->atomic_check = sde_encoder_phys_wb_atomic_check;
	ops->get_hw_resources = sde_encoder_phys_wb_get_hw_resources;
	ops->wait_for_commit_done = sde_encoder_phys_wb_wait_for_commit_done;
	ops->prepare_for_kickoff = sde_encoder_phys_wb_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_wb_handle_post_kickoff;
	ops->trigger_start = sde_encoder_helper_trigger_start;
}

/**
 * sde_encoder_phys_wb_init - initialize writeback encoder
 * @init:	Pointer to init info structure with initialization params
 */
struct sde_encoder_phys *sde_encoder_phys_wb_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc;
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_mdp *hw_mdp;
	int ret = 0;

	SDE_DEBUG("\n");

	wb_enc = kzalloc(sizeof(*wb_enc), GFP_KERNEL);
	if (!wb_enc) {
		ret = -ENOMEM;
		goto fail_alloc;
	}
	wb_enc->irq_idx = -EINVAL;
	wb_enc->wbdone_timeout = KICKOFF_TIMEOUT_MS;
	init_completion(&wb_enc->wbdone_complete);

	phys_enc = &wb_enc->base;

	if (p->sde_kms->vbif[VBIF_NRT]) {
		wb_enc->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE] =
			p->sde_kms->mmu_id[MSM_SMMU_DOMAIN_NRT_UNSECURE];
		wb_enc->mmu_id[SDE_IOMMU_DOMAIN_SECURE] =
			p->sde_kms->mmu_id[MSM_SMMU_DOMAIN_NRT_SECURE];
	} else {
		wb_enc->mmu_id[SDE_IOMMU_DOMAIN_UNSECURE] =
			p->sde_kms->mmu_id[MSM_SMMU_DOMAIN_UNSECURE];
		wb_enc->mmu_id[SDE_IOMMU_DOMAIN_SECURE] =
			p->sde_kms->mmu_id[MSM_SMMU_DOMAIN_SECURE];
	}

	hw_mdp = sde_rm_get_mdp(&p->sde_kms->rm);
	if (IS_ERR_OR_NULL(hw_mdp)) {
		ret = PTR_ERR(hw_mdp);
		SDE_ERROR("failed to init hw_top: %d\n", ret);
		goto fail_mdp_init;
	}
	phys_enc->hw_mdptop = hw_mdp;

	/**
	 * hw_wb resource permanently assigned to this encoder
	 * Other resources allocated at atomic commit time by use case
	 */
	if (p->wb_idx != SDE_NONE) {
		struct sde_rm_hw_iter iter;

		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_WB);
		while (sde_rm_get_hw(&p->sde_kms->rm, &iter)) {
			struct sde_hw_wb *hw_wb = (struct sde_hw_wb *)iter.hw;

			if (hw_wb->idx == p->wb_idx) {
				wb_enc->hw_wb = hw_wb;
				break;
			}
		}

		if (!wb_enc->hw_wb) {
			ret = -EINVAL;
			SDE_ERROR("failed to init hw_wb%d\n", p->wb_idx - WB_0);
			goto fail_wb_init;
		}
	} else {
		ret = -EINVAL;
		SDE_ERROR("invalid wb_idx\n");
		goto fail_wb_check;
	}

	sde_encoder_phys_wb_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_WB_LINE;
	phys_enc->intf_idx = p->intf_idx;
	phys_enc->enc_spinlock = p->enc_spinlock;
	INIT_LIST_HEAD(&wb_enc->irq_cb.list);

	ret = sde_encoder_phys_wb_init_debugfs(phys_enc, p->sde_kms);
	if (ret) {
		SDE_ERROR("failed to init debugfs %d\n", ret);
		goto fail_debugfs_init;
	}

	SDE_DEBUG("Created sde_encoder_phys_wb for wb %d\n",
			wb_enc->hw_wb->idx - WB_0);

	return phys_enc;

fail_debugfs_init:
fail_wb_init:
fail_wb_check:
fail_mdp_init:
	kfree(wb_enc);
fail_alloc:
	return ERR_PTR(ret);
}

