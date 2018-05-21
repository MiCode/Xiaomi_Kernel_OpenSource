/*
 * Copyright (c) 2015-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/debugfs.h>
#include <uapi/drm/sde_drm.h>

#include "sde_encoder_phys.h"
#include "sde_formats.h"
#include "sde_hw_top.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_wb.h"
#include "sde_vbif.h"
#include "sde_crtc.h"

#define to_sde_encoder_phys_wb(x) \
	container_of(x, struct sde_encoder_phys_wb, base)

#define WBID(wb_enc) \
	((wb_enc && wb_enc->wb_dev) ? wb_enc->wb_dev->wb_idx - WB_0 : -1)

#define TO_S15D16(_x_)	((_x_) << 7)

/**
 * sde_rgb2yuv_601l - rgb to yuv color space conversion matrix
 *
 */
static struct sde_csc_cfg sde_encoder_phys_wb_rgb2yuv_601l = {
	{
		TO_S15D16(0x0083), TO_S15D16(0x0102), TO_S15D16(0x0032),
		TO_S15D16(0x1fb5), TO_S15D16(0x1f6c), TO_S15D16(0x00e1),
		TO_S15D16(0x00e1), TO_S15D16(0x1f45), TO_S15D16(0x1fdc)
	},
	{ 0x00, 0x00, 0x00 },
	{ 0x0040, 0x0200, 0x0200 },
	{ 0x000, 0x3ff, 0x000, 0x3ff, 0x000, 0x3ff },
	{ 0x040, 0x3ac, 0x040, 0x3c0, 0x040, 0x3c0 },
};

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
 * sde_encoder_phys_wb_set_qos_remap - set QoS remapper for writeback
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_set_qos_remap(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_wb *hw_wb;
	struct drm_crtc *crtc;
	struct sde_vbif_set_qos_params qos_params;

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->crtc) {
		SDE_ERROR("invalid arguments\n");
		return;
	}

	wb_enc = to_sde_encoder_phys_wb(phys_enc);
	if (!wb_enc->crtc) {
		SDE_ERROR("invalid crtc");
		return;
	}

	crtc = wb_enc->crtc;

	if (!wb_enc->hw_wb || !wb_enc->hw_wb->caps) {
		SDE_ERROR("invalid writeback hardware\n");
		return;
	}

	hw_wb = wb_enc->hw_wb;

	memset(&qos_params, 0, sizeof(qos_params));
	qos_params.vbif_idx = hw_wb->caps->vbif_idx;
	qos_params.xin_id = hw_wb->caps->xin_id;
	qos_params.clk_ctrl = hw_wb->caps->clk_ctrl;
	qos_params.num = hw_wb->idx - WB_0;
	qos_params.is_rt = sde_crtc_get_client_type(crtc) != NRT_CLIENT;

	SDE_DEBUG("[qos_remap] wb:%d vbif:%d xin:%d rt:%d\n",
			qos_params.num,
			qos_params.vbif_idx,
			qos_params.xin_id, qos_params.is_rt);

	sde_vbif_set_qos_remap(phys_enc->sde_kms, &qos_params);
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
	struct sde_hw_cdm *hw_cdm;
	struct sde_hw_cdm_cfg *cdm_cfg;
	int ret;

	if (!phys_enc || !format)
		return;

	cdm_cfg = &phys_enc->cdm_cfg;
	hw_cdm = phys_enc->hw_cdm;
	if (!hw_cdm)
		return;

	if (!SDE_FORMAT_IS_YUV(format)) {
		SDE_DEBUG("[cdm_disable fmt:%x]\n",
				format->base.pixel_format);

		if (hw_cdm && hw_cdm->ops.disable)
			hw_cdm->ops.disable(hw_cdm);

		return;
	}

	memset(cdm_cfg, 0, sizeof(struct sde_hw_cdm_cfg));

	if (!wb_roi)
		return;

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

	if (hw_cdm && hw_cdm->ops.setup_csc_data) {
		ret = hw_cdm->ops.setup_csc_data(hw_cdm,
				&sde_encoder_phys_wb_rgb2yuv_601l);
		if (ret < 0) {
			SDE_ERROR("failed to setup CSC %d\n", ret);
			return;
		}
	}

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
	struct sde_hw_wb_cdp_cfg *cdp_cfg;
	const struct msm_format *format;
	int ret;
	struct msm_gem_address_space *aspace;
	u32 fb_mode;

	if (!phys_enc || !phys_enc->sde_kms || !phys_enc->sde_kms->catalog ||
			!phys_enc->connector) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	hw_wb = wb_enc->hw_wb;
	wb_cfg = &wb_enc->wb_cfg;
	cdp_cfg = &wb_enc->cdp_cfg;
	memset(wb_cfg, 0, sizeof(struct sde_hw_wb_cfg));

	wb_cfg->intf_mode = phys_enc->intf_mode;

	fb_mode = sde_connector_get_property(phys_enc->connector->state,
			CONNECTOR_PROP_FB_TRANSLATION_MODE);
	if (phys_enc->enable_state == SDE_ENC_DISABLING)
		wb_cfg->is_secure = false;
	else if (fb_mode == SDE_DRM_FB_SEC)
		wb_cfg->is_secure = true;
	else
		wb_cfg->is_secure = false;

	aspace = (wb_cfg->is_secure) ?
			wb_enc->aspace[SDE_IOMMU_DOMAIN_SECURE] :
			wb_enc->aspace[SDE_IOMMU_DOMAIN_UNSECURE];

	SDE_DEBUG("[fb_secure:%d]\n", wb_cfg->is_secure);

	ret = msm_framebuffer_prepare(fb, aspace);
	if (ret) {
		SDE_ERROR("prep fb failed, %d\n", ret);
		return;
	}

	/* cache framebuffer for cleanup in writeback done */
	wb_enc->wb_fb = fb;
	wb_enc->wb_aspace = aspace;

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
		ret = sde_format_populate_layout(aspace, fb, &wb_cfg->dest);
		if (ret) {
			SDE_DEBUG("failed to populate layout %d\n", ret);
			return;
		}
		wb_cfg->dest.width = fb->width;
		wb_cfg->dest.height = fb->height;
		wb_cfg->dest.num_planes = wb_cfg->dest.format->num_planes;
	} else {
		ret = sde_format_populate_layout_with_roi(aspace, fb, wb_roi,
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

	if (hw_wb->ops.setup_cdp) {
		memset(cdp_cfg, 0, sizeof(struct sde_hw_wb_cdp_cfg));

		cdp_cfg->enable = phys_enc->sde_kms->catalog->perf.cdp_cfg
				[SDE_PERF_CDP_USAGE_NRT].wr_enable;
		cdp_cfg->ubwc_meta_enable =
				SDE_FORMAT_IS_UBWC(wb_cfg->dest.format);
		cdp_cfg->tile_amortize_enable =
				SDE_FORMAT_IS_UBWC(wb_cfg->dest.format) ||
				SDE_FORMAT_IS_TILE(wb_cfg->dest.format);
		cdp_cfg->preload_ahead = SDE_WB_CDP_PRELOAD_AHEAD_64;

		hw_wb->ops.setup_cdp(hw_wb, cdp_cfg);
	}

	if (hw_wb->ops.setup_outaddress) {
		SDE_EVT32(hw_wb->idx,
				wb_cfg->dest.width,
				wb_cfg->dest.height,
				wb_cfg->dest.plane_addr[0],
				wb_cfg->dest.plane_size[0],
				wb_cfg->dest.plane_addr[1],
				wb_cfg->dest.plane_size[1],
				wb_cfg->dest.plane_addr[2],
				wb_cfg->dest.plane_size[2],
				wb_cfg->dest.plane_addr[3],
				wb_cfg->dest.plane_size[3]);
		hw_wb->ops.setup_outaddress(hw_wb, wb_cfg);
	}
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

	/* bypass check if commit with no framebuffer */
	fb = sde_wb_connector_state_get_output_fb(conn_state);
	if (!fb) {
		SDE_DEBUG("no output framebuffer\n");
		return 0;
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
			!(wb_cfg->features & BIT(SDE_WB_UBWC))) {
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
 * _sde_encoder_phys_wb_update_flush - flush hardware update
 * @phys_enc:	Pointer to physical encoder
 */
static void _sde_encoder_phys_wb_update_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_cdm *hw_cdm;
	u32 flush_mask = 0;

	if (!phys_enc)
		return;

	hw_wb = wb_enc->hw_wb;
	hw_ctl = phys_enc->hw_ctl;
	hw_cdm = phys_enc->hw_cdm;

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

	if (hw_ctl->ops.get_pending_flush)
		flush_mask = hw_ctl->ops.get_pending_flush(hw_ctl);

	SDE_DEBUG("Pending flush mask for CTL_%d is 0x%x, WB %d\n",
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

	/* clear writeback framebuffer - will be updated in setup_fb */
	wb_enc->wb_fb = NULL;
	wb_enc->wb_aspace = NULL;

	if (phys_enc->enable_state == SDE_ENC_DISABLING) {
		fb = wb_enc->fb_disable;
		wb_roi->w = 0;
		wb_roi->h = 0;
	} else {
		fb = sde_wb_get_output_fb(wb_enc->wb_dev);
		sde_wb_get_output_roi(wb_enc->wb_dev, wb_roi);
	}

	if (!fb) {
		SDE_DEBUG("no output framebuffer\n");
		return;
	}

	SDE_DEBUG("[fb_id:%u][fb:%u,%u]\n", fb->base.id,
			fb->width, fb->height);

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

	sde_encoder_phys_wb_set_qos_remap(phys_enc);

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
	u32 event = 0;

	SDE_DEBUG("[wb:%d,%u]\n", hw_wb->idx - WB_0,
			wb_enc->frame_count);

	/* don't notify upper layer for internal commit */
	if (phys_enc->enable_state == SDE_ENC_DISABLING)
		goto complete;

	event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE
			| SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE
			| SDE_ENCODER_FRAME_EVENT_DONE;

	atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0);
	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, event);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), hw_wb->idx - WB_0, event);

complete:
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
	u32 irq_status, event = 0;
	u64 wb_time = 0;
	int rc = 0;
	u32 timeout = max_t(u32, wb_enc->wbdone_timeout, KICKOFF_TIMEOUT_MS);

	/* Return EWOULDBLOCK since we know the wait isn't necessary */
	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("encoder already disabled\n");
		return -EWOULDBLOCK;
	}

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count,
			!!wb_enc->wb_fb);

	/* signal completion if commit with no framebuffer */
	if (!wb_enc->wb_fb) {
		SDE_DEBUG("no output framebuffer\n");
		sde_encoder_phys_wb_done_irq(wb_enc, wb_enc->irq_idx);
	}

	ret = wait_for_completion_timeout(&wb_enc->wbdone_complete,
			msecs_to_jiffies(timeout));

	if (!ret) {
		SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
				wb_enc->frame_count);
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				wb_enc->irq_idx, true);
		if (irq_status) {
			SDE_DEBUG("wb:%d done but irq not triggered\n",
					WBID(wb_enc));
			sde_encoder_phys_wb_done_irq(wb_enc, wb_enc->irq_idx);
		} else {
			SDE_ERROR("wb:%d kickoff timed out\n",
					WBID(wb_enc));
			atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0);

			event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE
				| SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE
				| SDE_ENCODER_FRAME_EVENT_ERROR;
			if (phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc, event);
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
		SDE_DEBUG("wb:%d took %llu us\n", WBID(wb_enc), wb_time);
	}

	/* cleanup writeback framebuffer */
	if (wb_enc->wb_fb && wb_enc->wb_aspace) {
		msm_framebuffer_cleanup(wb_enc->wb_fb, wb_enc->wb_aspace);
		wb_enc->wb_fb = NULL;
		wb_enc->wb_aspace = NULL;
	}

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count,
			wb_time, event, rc);

	return rc;
}

/**
 * sde_encoder_phys_wb_prepare_for_kickoff - pre-kickoff processing
 * @phys_enc:	Pointer to physical encoder
 * @params:	kickoff parameters
 * Returns:	Zero on success
 */
static int sde_encoder_phys_wb_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	int ret;

	SDE_DEBUG("[wb:%d,%u]\n", wb_enc->hw_wb->idx - WB_0,
			wb_enc->kickoff_count);

	reinit_completion(&wb_enc->wbdone_complete);

	ret = sde_encoder_phys_wb_register_irq(phys_enc);
	if (ret) {
		SDE_ERROR("failed to register irq %d\n", ret);
		return ret;
	}

	wb_enc->kickoff_count++;

	/* set OT limit & enable traffic shaper */
	sde_encoder_phys_wb_setup(phys_enc);

	_sde_encoder_phys_wb_update_flush(phys_enc);

	/* vote for iommu/clk/bus */
	wb_enc->start_time = ktime_get();

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->kickoff_count);
	return 0;
}

/**
 * sde_encoder_phys_wb_trigger_flush - trigger flush processing
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_trigger_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	if (!phys_enc || !wb_enc->hw_wb) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	SDE_DEBUG("[wb:%d]\n", wb_enc->hw_wb->idx - WB_0);

	/* clear pending flush if commit with no framebuffer */
	if (!wb_enc->wb_fb) {
		SDE_DEBUG("no output framebuffer\n");
		return;
	}

	sde_encoder_helper_trigger_flush(phys_enc);
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
 * _sde_encoder_phys_wb_init_internal_fb - create fb for internal commit
 * @wb_enc:		Pointer to writeback encoder
 * @pixel_format:	DRM pixel format
 * @width:		Desired fb width
 * @height:		Desired fb height
 * @pitch:		Desired fb pitch
 */
static int _sde_encoder_phys_wb_init_internal_fb(
		struct sde_encoder_phys_wb *wb_enc,
		uint32_t pixel_format, uint32_t width,
		uint32_t height, uint32_t pitch)
{
	struct drm_device *dev;
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd;
	uint32_t size;
	int nplanes, i, ret;
	struct msm_gem_address_space *aspace;

	if (!wb_enc || !wb_enc->base.parent || !wb_enc->base.sde_kms) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	aspace = wb_enc->base.sde_kms->aspace[SDE_IOMMU_DOMAIN_UNSECURE];
	if (!aspace) {
		SDE_ERROR("invalid address space\n");
		return -EINVAL;
	}

	dev = wb_enc->base.sde_kms->dev;
	if (!dev) {
		SDE_ERROR("invalid dev\n");
		return -EINVAL;
	}

	memset(&mode_cmd, 0, sizeof(mode_cmd));
	mode_cmd.pixel_format = pixel_format;
	mode_cmd.width = width;
	mode_cmd.height = height;
	mode_cmd.pitches[0] = pitch;

	size = sde_format_get_framebuffer_size(pixel_format,
			mode_cmd.width, mode_cmd.height,
			mode_cmd.pitches, NULL, 0);
	if (!size) {
		SDE_DEBUG("not creating zero size buffer\n");
		return -EINVAL;
	}

	/* allocate gem tracking object */
	nplanes = drm_format_num_planes(pixel_format);
	if (nplanes > SDE_MAX_PLANES) {
		SDE_ERROR("requested format has too many planes\n");
		return -EINVAL;
	}
	mutex_lock(&dev->struct_mutex);
	wb_enc->bo_disable[0] = msm_gem_new(dev, size,
			MSM_BO_SCANOUT | MSM_BO_WC);
	mutex_unlock(&dev->struct_mutex);

	if (IS_ERR_OR_NULL(wb_enc->bo_disable[0])) {
		ret = PTR_ERR(wb_enc->bo_disable[0]);
		wb_enc->bo_disable[0] = NULL;

		SDE_ERROR("failed to create bo, %d\n", ret);
		return ret;
	}

	for (i = 0; i < nplanes; ++i) {
		wb_enc->bo_disable[i] = wb_enc->bo_disable[0];
		mode_cmd.pitches[i] = width *
			drm_format_plane_cpp(pixel_format, i);
	}

	fb = msm_framebuffer_init(dev, &mode_cmd, wb_enc->bo_disable);
	if (IS_ERR_OR_NULL(fb)) {
		ret = PTR_ERR(fb);
		drm_gem_object_unreference(wb_enc->bo_disable[0]);
		wb_enc->bo_disable[0] = NULL;

		SDE_ERROR("failed to init fb, %d\n", ret);
		return ret;
	}

	/* prepare the backing buffer now so that it's available later */
	ret = msm_framebuffer_prepare(fb, aspace);
	if (!ret)
		wb_enc->fb_disable = fb;
	return ret;
}

/**
 * _sde_encoder_phys_wb_destroy_internal_fb - deconstruct internal fb
 * @wb_enc:		Pointer to writeback encoder
 */
static void _sde_encoder_phys_wb_destroy_internal_fb(
		struct sde_encoder_phys_wb *wb_enc)
{
	if (!wb_enc)
		return;

	if (wb_enc->fb_disable) {
		drm_framebuffer_unregister_private(wb_enc->fb_disable);
		drm_framebuffer_remove(wb_enc->fb_disable);
		wb_enc->fb_disable = NULL;
	}

	if (wb_enc->bo_disable[0]) {
		drm_gem_object_unreference(wb_enc->bo_disable[0]);
		wb_enc->bo_disable[0] = NULL;
	}
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
	connector = phys_enc->connector;

	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find writeback connector\n");
		return;
	}
	wb_enc->wb_dev = sde_wb_connector_get_wb(connector);

	phys_enc->enable_state = SDE_ENC_ENABLED;

	/*
	 * cache the crtc in wb_enc on enable for duration of use case
	 * for correctly servicing asynchronous irq events and timers
	 */
	wb_enc->crtc = phys_enc->parent->crtc;
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

	if (!phys_enc->hw_ctl || !phys_enc->parent ||
			!phys_enc->sde_kms || !wb_enc->fb_disable) {
		SDE_DEBUG("invalid enc, skipping extra commit\n");
		goto exit;
	}

	/* reset h/w before final flush */
	if (phys_enc->hw_ctl->ops.clear_pending_flush)
		phys_enc->hw_ctl->ops.clear_pending_flush(phys_enc->hw_ctl);
	if (sde_encoder_helper_reset_mixers(phys_enc, wb_enc->fb_disable))
		goto exit;

	phys_enc->enable_state = SDE_ENC_DISABLING;
	sde_encoder_phys_wb_prepare_for_kickoff(phys_enc, NULL);
	if (phys_enc->hw_ctl->ops.trigger_flush)
		phys_enc->hw_ctl->ops.trigger_flush(phys_enc->hw_ctl);
	sde_encoder_helper_trigger_start(phys_enc);
	sde_encoder_phys_wb_wait_for_commit_done(phys_enc);
exit:
	phys_enc->enable_state = SDE_ENC_DISABLED;
	wb_enc->crtc = NULL;
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
	const struct sde_format *fmt = NULL;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	fb = sde_wb_connector_state_get_output_fb(conn_state);
	if (fb) {
		fmt = sde_get_sde_format_ext(fb->pixel_format, fb->modifier,
				drm_format_num_planes(fb->pixel_format));
		if (!fmt) {
			SDE_ERROR("unsupported output pixel format:%d\n",
					fb->pixel_format);
			return;
		}
	}

	hw_wb = wb_enc->hw_wb;
	hw_res->wbs[hw_wb->idx - WB_0] = phys_enc->intf_mode;
	hw_res->needs_cdm = fmt ? SDE_FORMAT_IS_YUV(fmt) : false;
	SDE_DEBUG("[wb:%d] intf_mode=%d needs_cdm=%d\n", hw_wb->idx - WB_0,
			hw_res->wbs[hw_wb->idx - WB_0],
			hw_res->needs_cdm);
}

#ifdef CONFIG_DEBUG_FS
/**
 * sde_encoder_phys_wb_init_debugfs - initialize writeback encoder debugfs
 * @phys_enc:		Pointer to physical encoder
 * @debugfs_root:	Pointer to virtual encoder's debugfs_root dir
 */
static int sde_encoder_phys_wb_init_debugfs(
		struct sde_encoder_phys *phys_enc, struct dentry *debugfs_root)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	if (!phys_enc || !wb_enc->hw_wb || !debugfs_root)
		return -EINVAL;

	if (!debugfs_create_u32("wbdone_timeout", 0600,
			debugfs_root, &wb_enc->wbdone_timeout)) {
		SDE_ERROR("failed to create debugfs/wbdone_timeout\n");
		return -ENOMEM;
	}

	if (!debugfs_create_u32("bypass_irqreg", 0600,
			debugfs_root, &wb_enc->bypass_irqreg)) {
		SDE_ERROR("failed to create debugfs/bypass_irqreg\n");
		return -ENOMEM;
	}

	return 0;
}
#else
static int sde_encoder_phys_wb_init_debugfs(
		struct sde_encoder_phys *phys_enc, struct dentry *debugfs_root)
{
	return 0;
}
#endif

static int sde_encoder_phys_wb_late_register(struct sde_encoder_phys *phys_enc,
		struct dentry *debugfs_root)
{
	return sde_encoder_phys_wb_init_debugfs(phys_enc, debugfs_root);
}

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

	_sde_encoder_phys_wb_destroy_internal_fb(wb_enc);

	kfree(wb_enc);
}

/**
 * sde_encoder_phys_wb_init_ops - initialize writeback operations
 * @ops:	Pointer to encoder operation table
 */
static void sde_encoder_phys_wb_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->late_register = sde_encoder_phys_wb_late_register;
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
	ops->trigger_flush = sde_encoder_phys_wb_trigger_flush;
	ops->trigger_start = sde_encoder_helper_trigger_start;
	ops->hw_reset = sde_encoder_helper_hw_reset;
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

	if (!p || !p->parent) {
		SDE_ERROR("invalid params\n");
		ret = -EINVAL;
		goto fail_alloc;
	}

	wb_enc = kzalloc(sizeof(*wb_enc), GFP_KERNEL);
	if (!wb_enc) {
		SDE_ERROR("failed to allocate wb enc\n");
		ret = -ENOMEM;
		goto fail_alloc;
	}
	wb_enc->irq_idx = -EINVAL;
	wb_enc->wbdone_timeout = KICKOFF_TIMEOUT_MS;
	init_completion(&wb_enc->wbdone_complete);

	phys_enc = &wb_enc->base;

	if (p->sde_kms->vbif[VBIF_NRT]) {
		wb_enc->aspace[SDE_IOMMU_DOMAIN_UNSECURE] =
			p->sde_kms->aspace[MSM_SMMU_DOMAIN_NRT_UNSECURE];
		wb_enc->aspace[SDE_IOMMU_DOMAIN_SECURE] =
			p->sde_kms->aspace[MSM_SMMU_DOMAIN_NRT_SECURE];
	} else {
		wb_enc->aspace[SDE_IOMMU_DOMAIN_UNSECURE] =
			p->sde_kms->aspace[MSM_SMMU_DOMAIN_UNSECURE];
		wb_enc->aspace[SDE_IOMMU_DOMAIN_SECURE] =
			p->sde_kms->aspace[MSM_SMMU_DOMAIN_SECURE];
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
	phys_enc->vblank_ctl_lock = p->vblank_ctl_lock;
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
	INIT_LIST_HEAD(&wb_enc->irq_cb.list);

	/* create internal buffer for disable logic */
	if (_sde_encoder_phys_wb_init_internal_fb(wb_enc,
				DRM_FORMAT_RGB888, 2, 1, 6)) {
		SDE_ERROR("failed to init internal fb\n");
		goto fail_wb_init;
	}

	SDE_DEBUG("Created sde_encoder_phys_wb for wb %d\n",
			wb_enc->hw_wb->idx - WB_0);

	return phys_enc;

fail_wb_init:
fail_wb_check:
fail_mdp_init:
	kfree(wb_enc);
fail_alloc:
	return ERR_PTR(ret);
}

