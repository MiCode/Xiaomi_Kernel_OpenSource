// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

static const u32 cwb_irq_tbl[PINGPONG_MAX] = {SDE_NONE, INTR_IDX_PP1_OVFL,
	INTR_IDX_PP2_OVFL, INTR_IDX_PP3_OVFL, INTR_IDX_PP4_OVFL,
	INTR_IDX_PP5_OVFL, SDE_NONE, SDE_NONE};

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
	qos_params.client_type = phys_enc->in_clone_mode ?
					VBIF_CWB_CLIENT : VBIF_NRT_CLIENT;

	SDE_DEBUG("[qos_remap] wb:%d vbif:%d xin:%d clone:%d\n",
			qos_params.num,
			qos_params.vbif_idx,
			qos_params.xin_id, qos_params.client_type);

	sde_vbif_set_qos_remap(phys_enc->sde_kms, &qos_params);
}

/**
 * sde_encoder_phys_wb_set_qos - set QoS/danger/safe LUTs for writeback
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_set_qos(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_wb *hw_wb;
	struct sde_hw_wb_qos_cfg qos_cfg = {0};
	struct sde_perf_cfg *perf;
	u32 fps_index = 0, lut_index, index, frame_rate, qos_count;

	if (!phys_enc || !phys_enc->sde_kms || !phys_enc->sde_kms->catalog) {
		SDE_ERROR("invalid parameter(s)\n");
		return;
	}

	wb_enc = to_sde_encoder_phys_wb(phys_enc);
	if (!wb_enc->hw_wb) {
		SDE_ERROR("invalid writeback hardware\n");
		return;
	}

	perf = &phys_enc->sde_kms->catalog->perf;
	frame_rate = phys_enc->cached_mode.vrefresh;

	hw_wb = wb_enc->hw_wb;
	qos_count = perf->qos_refresh_count;
	while (qos_count && perf->qos_refresh_rate) {
		if (frame_rate >= perf->qos_refresh_rate[qos_count - 1]) {
			fps_index = qos_count - 1;
			break;
		}
		qos_count--;
	}

	qos_cfg.danger_safe_en = true;

	if (phys_enc->in_clone_mode)
		lut_index = SDE_QOS_LUT_USAGE_CWB;
	else
		lut_index = SDE_QOS_LUT_USAGE_NRT;
	index = (fps_index * SDE_QOS_LUT_USAGE_MAX) + lut_index;

	qos_cfg.danger_lut = perf->danger_lut[index];
	qos_cfg.safe_lut = (u32) perf->safe_lut[index];
	qos_cfg.creq_lut = perf->creq_lut[index];

	SDE_DEBUG("wb_enc:%d hw idx:%d fps:%d mode:%d luts[0x%x,0x%x 0x%llx]\n",
		DRMID(phys_enc->parent), hw_wb->idx - WB_0,
		frame_rate, phys_enc->in_clone_mode,
		qos_cfg.danger_lut, qos_cfg.safe_lut, qos_cfg.creq_lut);

	if (hw_wb->ops.setup_qos_lut)
		hw_wb->ops.setup_qos_lut(hw_wb, &qos_cfg);
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
	struct sde_hw_pingpong *hw_pp;
	int ret;

	if (!phys_enc || !format)
		return;

	cdm_cfg = &phys_enc->cdm_cfg;
	hw_pp = phys_enc->hw_pp;
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

	if (hw_cdm && hw_pp && hw_cdm->ops.enable) {
		cdm_cfg->pp_id = hw_pp->idx;
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
	drm_framebuffer_get(fb);

	format = msm_framebuffer_format(fb);
	if (!format) {
		SDE_DEBUG("invalid format for fb\n");
		return;
	}

	wb_cfg->dest.format = sde_get_sde_format_ext(
			format->pixel_format,
			fb->modifier);
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

static void _sde_encoder_phys_wb_setup_cwb(struct sde_encoder_phys *phys_enc,
					bool enable)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	struct sde_hw_ctl *hw_ctl = phys_enc->hw_ctl;
	struct sde_crtc *crtc = to_sde_crtc(wb_enc->crtc);
	struct sde_hw_pingpong *hw_pp = phys_enc->hw_pp;
	bool need_merge = (crtc->num_mixers > 1);
	int i = 0;

	if (!phys_enc->in_clone_mode) {
		SDE_DEBUG("not in CWB mode. early return\n");
		return;
	}

	if (!hw_pp || !hw_ctl || !hw_wb || hw_pp->idx >= PINGPONG_MAX) {
		SDE_ERROR("invalid hw resources - return\n");
		return;
	}

	hw_ctl = crtc->mixers[0].hw_ctl;
	if (hw_ctl && hw_ctl->ops.setup_intf_cfg_v1 &&
			test_bit(SDE_WB_CWB_CTRL, &hw_wb->caps->features)) {
		struct sde_hw_intf_cfg_v1 intf_cfg = { 0, };

		for (i = 0; i < crtc->num_mixers; i++)
			intf_cfg.cwb[intf_cfg.cwb_count++] =
				(enum sde_cwb)(hw_pp->idx + i);

		if (enable && hw_pp->merge_3d && (intf_cfg.merge_3d_count <
				MAX_MERGE_3D_PER_CTL_V1) && need_merge)
			intf_cfg.merge_3d[intf_cfg.merge_3d_count++] =
				hw_pp->merge_3d->idx;

		if (hw_pp->ops.setup_3d_mode)
			hw_pp->ops.setup_3d_mode(hw_pp, (enable && need_merge) ?
					BLEND_3D_H_ROW_INT : 0);

		if (hw_wb->ops.bind_pingpong_blk)
			hw_wb->ops.bind_pingpong_blk(hw_wb, enable, hw_pp->idx);

		if (hw_ctl->ops.update_cwb_cfg) {
			hw_ctl->ops.update_cwb_cfg(hw_ctl, &intf_cfg, enable);
			SDE_DEBUG("in CWB mode on CTL_%d PP-%d merge3d:%d\n",
					hw_ctl->idx - CTL_0,
					hw_pp->idx - PINGPONG_0,
					hw_pp->merge_3d ?
					hw_pp->merge_3d->idx - MERGE_3D_0 : -1);
		}
	} else {
		struct sde_hw_intf_cfg *intf_cfg = &phys_enc->intf_cfg;

		memset(intf_cfg, 0, sizeof(struct sde_hw_intf_cfg));
		intf_cfg->intf = SDE_NONE;
		intf_cfg->wb = hw_wb->idx;

		if (hw_ctl && hw_ctl->ops.update_wb_cfg) {
			hw_ctl->ops.update_wb_cfg(hw_ctl, intf_cfg, enable);
			SDE_DEBUG("in CWB mode adding WB for CTL_%d\n",
					hw_ctl->idx - CTL_0);
		}
	}
}

/**
 * sde_encoder_phys_wb_setup_cdp - setup chroma down prefetch block
 * @phys_enc:	Pointer to physical encoder
 */
static void sde_encoder_phys_wb_setup_cdp(struct sde_encoder_phys *phys_enc,
		const struct sde_format *format)
{
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_wb *hw_wb;
	struct sde_hw_cdm *hw_cdm;
	struct sde_hw_ctl *ctl;
	const int num_wb = 1;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (phys_enc->in_clone_mode) {
		SDE_DEBUG("in CWB mode. early return\n");
		return;
	}

	wb_enc = to_sde_encoder_phys_wb(phys_enc);
	hw_wb = wb_enc->hw_wb;
	hw_cdm = phys_enc->hw_cdm;
	ctl = phys_enc->hw_ctl;

	if (test_bit(SDE_CTL_ACTIVE_CFG, &ctl->caps->features) &&
		(phys_enc->hw_ctl &&
		 phys_enc->hw_ctl->ops.setup_intf_cfg_v1)) {
		struct sde_hw_intf_cfg_v1 *intf_cfg_v1 = &phys_enc->intf_cfg_v1;
		struct sde_hw_pingpong *hw_pp = phys_enc->hw_pp;
		enum sde_3d_blend_mode mode_3d;

		memset(intf_cfg_v1, 0, sizeof(struct sde_hw_intf_cfg_v1));

		mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);
		intf_cfg_v1->intf_count = SDE_NONE;
		intf_cfg_v1->wb_count = num_wb;
		intf_cfg_v1->wb[0] = hw_wb->idx;
		if (SDE_FORMAT_IS_YUV(format)) {
			if (!phys_enc->hw_cdm) {
				SDE_ERROR("Format:YUV but no cdm allocated\n");
				SDE_EVT32(DRMID(phys_enc->parent),
							 SDE_EVTLOG_ERROR);
				return;
			}

			intf_cfg_v1->cdm_count = num_wb;
			intf_cfg_v1->cdm[0] = hw_cdm->idx;
		}

		if (mode_3d && hw_pp && hw_pp->merge_3d &&
			intf_cfg_v1->merge_3d_count < MAX_MERGE_3D_PER_CTL_V1)
			intf_cfg_v1->merge_3d[intf_cfg_v1->merge_3d_count++] =
					hw_pp->merge_3d->idx;

		if (hw_pp && hw_pp->ops.setup_3d_mode)
			hw_pp->ops.setup_3d_mode(hw_pp, mode_3d);

		/* setup which pp blk will connect to this wb */
		if (hw_pp && hw_wb->ops.bind_pingpong_blk)
			hw_wb->ops.bind_pingpong_blk(hw_wb, true,
					hw_pp->idx);

		phys_enc->hw_ctl->ops.setup_intf_cfg_v1(phys_enc->hw_ctl,
				intf_cfg_v1);
	} else if (phys_enc->hw_ctl && phys_enc->hw_ctl->ops.setup_intf_cfg) {
		struct sde_hw_intf_cfg *intf_cfg = &phys_enc->intf_cfg;

		memset(intf_cfg, 0, sizeof(struct sde_hw_intf_cfg));

		intf_cfg->intf = SDE_NONE;
		intf_cfg->wb = hw_wb->idx;
		intf_cfg->mode_3d =
			sde_encoder_helper_get_3d_blend_mode(phys_enc);
		phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl,
				intf_cfg);
	}

}

static bool _sde_enc_phys_wb_detect_cwb(struct sde_encoder_phys *phys_enc,
		struct drm_crtc_state *crtc_state)
{
	struct drm_encoder *encoder;
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	const struct sde_wb_cfg *wb_cfg = wb_enc->hw_wb->caps;

	/* Check if WB has CWB support */
	if (!(wb_cfg->features & BIT(SDE_WB_HAS_CWB)))
		return false;

	/* if any other encoder is connected to same crtc enable clone mode*/
	drm_for_each_encoder(encoder, crtc_state->crtc->dev) {
		if (encoder->crtc != crtc_state->crtc)
			continue;

		if (phys_enc->parent != encoder) {
			return true;
		}
	}

	return false;
}

static int _sde_enc_phys_wb_validate_cwb(struct sde_encoder_phys *phys_enc,
			struct drm_crtc_state *crtc_state,
			 struct drm_connector_state *conn_state)
{
	struct sde_crtc_state *cstate = to_sde_crtc_state(crtc_state);
	struct sde_rect wb_roi = {0,};
	struct sde_rect pu_roi = {0,};
	int data_pt;
	int ds_outw = 0;
	int ds_outh = 0;
	int ds_in_use = false;
	int i = 0;
	int ret = 0;

	if (!phys_enc->in_clone_mode) {
		SDE_DEBUG("not in CWB mode. early return\n");
		goto exit;
	}

	ret = sde_wb_connector_state_get_output_roi(conn_state, &wb_roi);
	if (ret) {
		SDE_ERROR("failed to get roi %d\n", ret);
		goto exit;
	}

	data_pt = sde_crtc_get_property(cstate, CRTC_PROP_CAPTURE_OUTPUT);

	/* compute cumulative ds output dimensions if in use */
	for (i = 0; i < cstate->num_ds; i++)
		if (cstate->ds_cfg[i].scl3_cfg.enable) {
			ds_in_use = true;
			ds_outw += cstate->ds_cfg[i].scl3_cfg.dst_width;
			ds_outh = cstate->ds_cfg[i].scl3_cfg.dst_height;
		}

	/* if ds in use check wb roi against ds output dimensions */
	if ((data_pt == CAPTURE_DSPP_OUT) &&  ds_in_use &&
			((wb_roi.w != ds_outw) || (wb_roi.h != ds_outh))) {
		SDE_ERROR("invalid wb roi with dest scalar [%dx%d vs %dx%d]\n",
				wb_roi.w, wb_roi.h, ds_outw, ds_outh);
		ret = -EINVAL;
		goto exit;
	}

	/* validate conn roi against pu rect */
	if (cstate->user_roi_list.num_rects) {
		sde_kms_rect_merge_rectangles(&cstate->user_roi_list, &pu_roi);
		if (wb_roi.w != pu_roi.w || wb_roi.h != pu_roi.h) {
			SDE_ERROR("invalid wb roi with pu [%dx%d vs %dx%d]\n",
					wb_roi.w, wb_roi.h, pu_roi.w, pu_roi.h);
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	return ret;
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
	bool clone_mode_curr = false;

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

	clone_mode_curr = _sde_enc_phys_wb_detect_cwb(phys_enc, crtc_state);

	/**
	 * Fail the WB commit when there is a CWB session enabled in HW.
	 * CWB session needs to be disabled since WB and CWB share the same
	 * writeback hardware block.
	 */
	if (phys_enc->in_clone_mode && !clone_mode_curr) {
		SDE_ERROR("WB commit before CWB disable\n");
		return -EINVAL;
	}

	SDE_DEBUG("detect CWB - status:%d\n", clone_mode_curr);
	phys_enc->in_clone_mode = clone_mode_curr;
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

	fmt = sde_get_sde_format_ext(fb->format->format, fb->modifier);
	if (!fmt) {
		SDE_ERROR("unsupported output pixel format:%x\n",
				fb->format->format);
		return -EINVAL;
	}

	SDE_DEBUG("[fb_fmt:%x,%llx]\n", fb->format->format,
			fb->modifier);

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

	rc = _sde_enc_phys_wb_validate_cwb(phys_enc, crtc_state, conn_state);
	if (rc) {
		SDE_ERROR("failed in cwb validation %d\n", rc);
		return rc;
	}

	return rc;
}

static void _sde_encoder_phys_wb_update_cwb_flush(
		struct sde_encoder_phys *phys_enc, bool enable)
{
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_wb *hw_wb;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_cdm *hw_cdm;
	struct sde_hw_pingpong *hw_pp;
	struct sde_crtc *crtc;
	struct sde_crtc_state *crtc_state;
	int i = 0;
	int cwb_capture_mode = 0;
	enum sde_cwb cwb_idx = 0;
	enum sde_cwb src_pp_idx = 0;
	bool dspp_out = false;
	bool need_merge = false;

	if (!phys_enc->in_clone_mode) {
		SDE_DEBUG("not in CWB mode. early return\n");
		return;
	}

	wb_enc = to_sde_encoder_phys_wb(phys_enc);
	crtc = to_sde_crtc(wb_enc->crtc);
	crtc_state = to_sde_crtc_state(wb_enc->crtc->state);
	cwb_capture_mode = sde_crtc_get_property(crtc_state,
			CRTC_PROP_CAPTURE_OUTPUT);

	hw_pp = phys_enc->hw_pp;
	hw_wb = wb_enc->hw_wb;
	hw_cdm = phys_enc->hw_cdm;

	/* In CWB mode, program actual source master sde_hw_ctl from crtc */
	hw_ctl = crtc->mixers[0].hw_ctl;
	if (!hw_ctl || !hw_wb || !hw_pp) {
		SDE_ERROR("[wb] HW resource not available for CWB\n");
		return;
	}

	/* treating LM idx of primary display ctl path as source ping-pong idx*/
	src_pp_idx = (enum sde_cwb)crtc->mixers[0].hw_lm->idx;
	cwb_idx = (enum sde_cwb)hw_pp->idx;
	dspp_out = (cwb_capture_mode == CAPTURE_DSPP_OUT);
	need_merge = (crtc->num_mixers > 1) ? true : false;

	if (src_pp_idx > CWB_0 ||  ((cwb_idx + crtc->num_mixers) > CWB_MAX)) {
		SDE_ERROR("invalid hw config for CWB\n");
		return;
	}

	if (hw_ctl->ops.update_bitmask_wb)
		hw_ctl->ops.update_bitmask_wb(hw_ctl, hw_wb->idx, 1);

	if (hw_ctl->ops.update_bitmask_cdm && hw_cdm)
		hw_ctl->ops.update_bitmask_cdm(hw_ctl, hw_cdm->idx, 1);

	if (test_bit(SDE_WB_CWB_CTRL, &hw_wb->caps->features)) {
		for (i = 0; i < crtc->num_mixers; i++) {
			cwb_idx = (enum sde_cwb) (hw_pp->idx + i);
			src_pp_idx = (enum sde_cwb) (src_pp_idx + i);

			if (hw_wb->ops.program_cwb_ctrl)
				hw_wb->ops.program_cwb_ctrl(hw_wb, cwb_idx,
						src_pp_idx, dspp_out, enable);

			if (hw_ctl->ops.update_bitmask_cwb)
				hw_ctl->ops.update_bitmask_cwb(hw_ctl,
						cwb_idx, 1);
		}

		if (need_merge && hw_ctl->ops.update_bitmask_merge3d
				&& hw_pp && hw_pp->merge_3d)
			hw_ctl->ops.update_bitmask_merge3d(hw_ctl,
					hw_pp->merge_3d->idx, 1);
	} else {
		phys_enc->hw_mdptop->ops.set_cwb_ppb_cntl(phys_enc->hw_mdptop,
				need_merge, dspp_out);
	}
}

/**
 * _sde_encoder_phys_wb_update_flush - flush hardware update
 * @phys_enc:	Pointer to physical encoder
 */
static void _sde_encoder_phys_wb_update_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc;
	struct sde_hw_wb *hw_wb;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_cdm *hw_cdm;
	struct sde_hw_pingpong *hw_pp;
	struct sde_ctl_flush_cfg pending_flush = {0,};

	if (!phys_enc)
		return;

	wb_enc = to_sde_encoder_phys_wb(phys_enc);
	hw_wb = wb_enc->hw_wb;
	hw_cdm = phys_enc->hw_cdm;
	hw_pp = phys_enc->hw_pp;
	hw_ctl = phys_enc->hw_ctl;

	SDE_DEBUG("[wb:%d]\n", hw_wb->idx - WB_0);

	if (phys_enc->in_clone_mode) {
		SDE_DEBUG("in CWB mode. early return\n");
		return;
	}

	if (!hw_ctl) {
		SDE_DEBUG("[wb:%d] no ctl assigned\n", hw_wb->idx - WB_0);
		return;
	}

	if (hw_ctl->ops.update_bitmask_wb)
		hw_ctl->ops.update_bitmask_wb(hw_ctl, hw_wb->idx, 1);

	if (hw_ctl->ops.update_bitmask_cdm && hw_cdm)
		hw_ctl->ops.update_bitmask_cdm(hw_ctl, hw_cdm->idx, 1);

	if (hw_ctl->ops.update_bitmask_merge3d && hw_pp && hw_pp->merge_3d)
		hw_ctl->ops.update_bitmask_merge3d(hw_ctl,
				hw_pp->merge_3d->idx, 1);

	if (hw_ctl->ops.get_pending_flush)
		hw_ctl->ops.get_pending_flush(hw_ctl,
				&pending_flush);

	SDE_DEBUG("Pending flush mask for CTL_%d is 0x%x, WB %d\n",
			hw_ctl->idx - CTL_0, pending_flush.pending_flush_mask,
			hw_wb->idx - WB_0);
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

	wb_enc->wb_fmt = sde_get_sde_format_ext(fb->format->format,
							fb->modifier);
	if (!wb_enc->wb_fmt) {
		SDE_ERROR("unsupported output pixel format: %d\n",
				fb->format->format);
		return;
	}

	SDE_DEBUG("[fb_fmt:%x,%llx]\n", fb->format->format,
			fb->modifier);

	sde_encoder_phys_wb_set_ot_limit(phys_enc);

	sde_encoder_phys_wb_set_qos_remap(phys_enc);

	sde_encoder_phys_wb_set_qos(phys_enc);

	sde_encoder_phys_setup_cdm(phys_enc, fb, wb_enc->wb_fmt, wb_roi);

	sde_encoder_phys_wb_setup_fb(phys_enc, fb, wb_roi);

	sde_encoder_phys_wb_setup_cdp(phys_enc, wb_enc->wb_fmt);

	_sde_encoder_phys_wb_setup_cwb(phys_enc, true);
}

static void _sde_encoder_phys_wb_frame_done_helper(void *arg, bool frame_error)
{
	struct sde_encoder_phys_wb *wb_enc = arg;
	struct sde_encoder_phys *phys_enc = &wb_enc->base;
	struct sde_hw_wb *hw_wb = wb_enc->hw_wb;
	u32 event = frame_error ? SDE_ENCODER_FRAME_EVENT_ERROR : 0;

	SDE_DEBUG("[wb:%d,%u]\n", hw_wb->idx - WB_0, wb_enc->frame_count);

	/* don't notify upper layer for internal commit */
	if (phys_enc->enable_state == SDE_ENC_DISABLING &&
			!phys_enc->in_clone_mode)
		goto complete;

	if (phys_enc->parent_ops.handle_frame_done &&
	    atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0)) {
		event |= SDE_ENCODER_FRAME_EVENT_DONE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;

		if (phys_enc->in_clone_mode)
			event |= SDE_ENCODER_FRAME_EVENT_CWB_DONE;
		else
			event |= SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;

		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, event);
	}

	if (!phys_enc->in_clone_mode && phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), hw_wb->idx - WB_0, event,
		frame_error);

complete:
	wake_up_all(&phys_enc->pending_kickoff_wq);
}

/**
 * sde_encoder_phys_wb_done_irq - Pingpong overflow interrupt handler for CWB
 * @arg:	Pointer to writeback encoder
 * @irq_idx:	interrupt index
 */
static void sde_encoder_phys_cwb_ovflow(void *arg, int irq_idx)
{
	_sde_encoder_phys_wb_frame_done_helper(arg, true);
}

/**
 * sde_encoder_phys_wb_done_irq - writeback interrupt handler
 * @arg:	Pointer to writeback encoder
 * @irq_idx:	interrupt index
 */
static void sde_encoder_phys_wb_done_irq(void *arg, int irq_idx)
{
	_sde_encoder_phys_wb_frame_done_helper(arg, false);
}

/**
 * sde_encoder_phys_wb_irq_ctrl - irq control of WB
 * @phys:	Pointer to physical encoder
 * @enable:	indicates enable or disable interrupts
 */
static void sde_encoder_phys_wb_irq_ctrl(
		struct sde_encoder_phys *phys, bool enable)
{

	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys);
	int index = 0, refcount;
	int ret = 0, pp = 0;

	if (!wb_enc)
		return;

	if (wb_enc->bypass_irqreg)
		return;

	pp = phys->hw_pp->idx - PINGPONG_0;
	if ((pp + CRTC_DUAL_MIXERS) >= PINGPONG_MAX) {
		SDE_ERROR("invalid pingpong index for WB or CWB\n");
		return;
	}

	refcount = atomic_read(&phys->wbirq_refcount);

	if (enable && atomic_inc_return(&phys->wbirq_refcount) == 1) {
		sde_encoder_helper_register_irq(phys, INTR_IDX_WB_DONE);
		if (ret)
			atomic_dec_return(&phys->wbirq_refcount);

		for (index = 0; index < CRTC_DUAL_MIXERS; index++)
			if (cwb_irq_tbl[index + pp] != SDE_NONE)
				sde_encoder_helper_register_irq(phys,
					cwb_irq_tbl[index + pp]);
	} else if (!enable &&
			atomic_dec_return(&phys->wbirq_refcount) == 0) {
		sde_encoder_helper_unregister_irq(phys, INTR_IDX_WB_DONE);
		if (ret)
			atomic_inc_return(&phys->wbirq_refcount);

		for (index = 0; index < CRTC_DUAL_MIXERS; index++)
			if (cwb_irq_tbl[index + pp] != SDE_NONE)
				sde_encoder_helper_unregister_irq(phys,
					cwb_irq_tbl[index + pp]);
	}
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
		SDE_ERROR("failed init ctl: %ld\n",
			(!phys_enc->hw_ctl) ?
			-EINVAL : PTR_ERR(phys_enc->hw_ctl));
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
		phys_enc->hw_cdm = NULL;
	}
}

static int sde_encoder_phys_wb_frame_timeout(struct sde_encoder_phys *phys_enc)
{
	u32 event = 0;

	while (atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0) &&
			phys_enc->parent_ops.handle_frame_done) {

		event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE
			| SDE_ENCODER_FRAME_EVENT_ERROR;

		if (phys_enc->in_clone_mode)
			event |= SDE_ENCODER_FRAME_EVENT_CWB_DONE;
		else
			event |= SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;

		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc, event);

		SDE_EVT32(DRMID(phys_enc->parent), event,
			atomic_read(&phys_enc->pending_retire_fence_cnt));
	}

	return event;
}

static void _sde_encoder_phys_wb_reset_state(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);

	/*
	 * frame count and kickoff count are only used for debug purpose. Frame
	 * count can be more than kickoff count at the end of disable call due
	 * to extra frame_done wait. It does not cause any issue because
	 * frame_done wait is based on retire_fence count. Leaving these
	 * counters for debugging purpose.
	 */
	if (wb_enc->frame_count != wb_enc->kickoff_count) {
		SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
			wb_enc->kickoff_count, wb_enc->frame_count,
			phys_enc->in_clone_mode);
		wb_enc->frame_count = wb_enc->kickoff_count;
	}

	phys_enc->enable_state = SDE_ENC_DISABLED;
	wb_enc->crtc = NULL;
	phys_enc->hw_cdm = NULL;
	phys_enc->hw_ctl = NULL;
	phys_enc->in_clone_mode = false;
}

static int _sde_encoder_phys_wb_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc, bool is_disable)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	u32 event = 0;
	u64 wb_time = 0;
	int rc = 0;
	struct sde_encoder_wait_info wait_info = {0};

	/* Return EWOULDBLOCK since we know the wait isn't necessary */
	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("encoder already disabled\n");
		return -EWOULDBLOCK;
	}

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count,
		wb_enc->kickoff_count, !!wb_enc->wb_fb, is_disable,
		phys_enc->in_clone_mode);

	if (!is_disable && phys_enc->in_clone_mode &&
	    (atomic_read(&phys_enc->pending_retire_fence_cnt) <= 1))
		goto skip_wait;

	/* signal completion if commit with no framebuffer */
	if (!wb_enc->wb_fb) {
		SDE_DEBUG("no output framebuffer\n");
		_sde_encoder_phys_wb_frame_done_helper(wb_enc, false);
	}

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_retire_fence_cnt;
	wait_info.timeout_ms = max_t(u32, wb_enc->wbdone_timeout,
		KICKOFF_TIMEOUT_MS);
	rc = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_WB_DONE,
		&wait_info);
	if (rc == -ETIMEDOUT) {
		SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
			wb_enc->frame_count, SDE_EVTLOG_ERROR);
		SDE_ERROR("wb:%d kickoff timed out\n", WBID(wb_enc));

		event = sde_encoder_phys_wb_frame_timeout(phys_enc);
	}

	/* cleanup writeback framebuffer */
	if (wb_enc->wb_fb && wb_enc->wb_aspace) {
		msm_framebuffer_cleanup(wb_enc->wb_fb, wb_enc->wb_aspace);
		drm_framebuffer_put(wb_enc->wb_fb);
		wb_enc->wb_fb = NULL;
		wb_enc->wb_aspace = NULL;
	}

skip_wait:
	/* remove vote for iommu/clk/bus */
	wb_enc->frame_count++;

	if (!rc) {
		wb_enc->end_time = ktime_get();
		wb_time = (u64)ktime_to_us(wb_enc->end_time) -
				(u64)ktime_to_us(wb_enc->start_time);
		SDE_DEBUG("wb:%d took %llu us\n", WBID(wb_enc), wb_time);
	}

	/* cleanup previous buffer if pending */
	if (wb_enc->cwb_old_fb && wb_enc->cwb_old_aspace) {
		msm_framebuffer_cleanup(wb_enc->cwb_old_fb, wb_enc->cwb_old_aspace);
		drm_framebuffer_put(wb_enc->cwb_old_fb);
		wb_enc->cwb_old_fb = NULL;
		wb_enc->cwb_old_aspace = NULL;
	}

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc), wb_enc->frame_count,
			wb_time, event, rc);

	return rc;
}

/**
 * sde_encoder_phys_wb_wait_for_commit_done - wait until request is committed
 * @phys_enc:	Pointer to physical encoder
 */
static int sde_encoder_phys_wb_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	int rc;

	if (phys_enc->enable_state == SDE_ENC_DISABLING &&
			phys_enc->in_clone_mode) {
		rc = _sde_encoder_phys_wb_wait_for_commit_done(phys_enc, true);
		_sde_encoder_phys_wb_reset_state(phys_enc);
		sde_encoder_phys_wb_irq_ctrl(phys_enc, false);
	} else {
		rc = _sde_encoder_phys_wb_wait_for_commit_done(phys_enc, false);
	}

	return rc;
}

static int sde_encoder_phys_wb_wait_for_cwb_done(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_wb *wb_enc = to_sde_encoder_phys_wb(phys_enc);
	struct sde_encoder_wait_info wait_info = {0};
	int rc = 0;

	if (!phys_enc->in_clone_mode)
		return 0;

	SDE_EVT32(atomic_read(&phys_enc->pending_retire_fence_cnt));

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_retire_fence_cnt;
	wait_info.timeout_ms = max_t(u32, wb_enc->wbdone_timeout,
				KICKOFF_TIMEOUT_MS);

	rc = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_WB_DONE,
		&wait_info);

	if (rc == -ETIMEDOUT)
		SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
			wb_enc->frame_count, SDE_EVTLOG_ERROR);

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

	SDE_DEBUG("[wb:%d,%u]\n", wb_enc->hw_wb->idx - WB_0,
			wb_enc->kickoff_count);

	if (phys_enc->in_clone_mode) {
		wb_enc->cwb_old_fb = wb_enc->wb_fb;
		wb_enc->cwb_old_aspace = wb_enc->wb_aspace;
	}

	wb_enc->kickoff_count++;

	/* set OT limit & enable traffic shaper */
	sde_encoder_phys_wb_setup(phys_enc);

	_sde_encoder_phys_wb_update_flush(phys_enc);

	_sde_encoder_phys_wb_update_cwb_flush(phys_enc, true);

	/* vote for iommu/clk/bus */
	wb_enc->start_time = ktime_get();

	SDE_EVT32(DRMID(phys_enc->parent), WBID(wb_enc),
		wb_enc->kickoff_count, wb_enc->frame_count,
		phys_enc->in_clone_mode);
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

	/*
	 * Bail out iff in CWB mode. In case of CWB, primary control-path
	 * which is actually driving would trigger the flush
	 */
	if (phys_enc->in_clone_mode) {
		SDE_DEBUG("in CWB mode. early return\n");
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
			mode_cmd.pitches, 0);
	if (!size) {
		SDE_DEBUG("not creating zero size buffer\n");
		return -EINVAL;
	}

	/* allocate gem tracking object */
	nplanes = drm_format_num_planes(pixel_format);
	if (nplanes >= SDE_MAX_PLANES) {
		SDE_ERROR("requested format has too many planes\n");
		return -EINVAL;
	}

	wb_enc->bo_disable[0] = msm_gem_new(dev, size,
			MSM_BO_SCANOUT | MSM_BO_WC);
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
		drm_gem_object_put(wb_enc->bo_disable[0]);
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
		drm_gem_object_put(wb_enc->bo_disable[0]);
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

	SDE_DEBUG("[wait_for_done: wb:%d, frame:%u, kickoff:%u]\n",
			hw_wb->idx - WB_0, wb_enc->frame_count,
			wb_enc->kickoff_count);

	if (!phys_enc->in_clone_mode || !wb_enc->crtc->state->active)
		_sde_encoder_phys_wb_wait_for_commit_done(phys_enc, true);

	if (!phys_enc->hw_ctl || !phys_enc->parent ||
			!phys_enc->sde_kms || !wb_enc->fb_disable) {
		SDE_DEBUG("invalid enc, skipping extra commit\n");
		goto exit;
	}

	if (phys_enc->in_clone_mode) {
		_sde_encoder_phys_wb_setup_cwb(phys_enc, false);
		_sde_encoder_phys_wb_update_cwb_flush(phys_enc, false);
		phys_enc->enable_state = SDE_ENC_DISABLING;

		if (wb_enc->crtc->state->active) {
			sde_encoder_phys_wb_irq_ctrl(phys_enc, true);
			return;
		}

		goto exit;
	}

	/* reset h/w before final flush */
	if (phys_enc->hw_ctl->ops.clear_pending_flush)
		phys_enc->hw_ctl->ops.clear_pending_flush(phys_enc->hw_ctl);

	/*
	 * New CTL reset sequence from 5.0 MDP onwards.
	 * If has_3d_merge_reset is not set, legacy reset
	 * sequence is executed.
	 */
	if (hw_wb->catalog->has_3d_merge_reset) {
		sde_encoder_helper_phys_disable(phys_enc, wb_enc);
		goto exit;
	}

	if (sde_encoder_helper_reset_mixers(phys_enc, NULL))
		goto exit;

	phys_enc->enable_state = SDE_ENC_DISABLING;

	sde_encoder_phys_wb_prepare_for_kickoff(phys_enc, NULL);
	sde_encoder_phys_wb_irq_ctrl(phys_enc, true);
	if (phys_enc->hw_ctl->ops.trigger_flush)
		phys_enc->hw_ctl->ops.trigger_flush(phys_enc->hw_ctl);

	sde_encoder_helper_trigger_start(phys_enc);
	_sde_encoder_phys_wb_wait_for_commit_done(phys_enc, true);
	sde_encoder_phys_wb_irq_ctrl(phys_enc, false);

exit:
	_sde_encoder_phys_wb_reset_state(phys_enc);
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
		fmt = sde_get_sde_format_ext(fb->format->format, fb->modifier);
		if (!fmt) {
			SDE_ERROR("unsupported output pixel format:%d\n",
					fb->format->format);
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
	ops->irq_control = sde_encoder_phys_wb_irq_ctrl;
	ops->wait_for_tx_complete = sde_encoder_phys_wb_wait_for_cwb_done;
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
	struct sde_encoder_irq *irq;
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
	wb_enc->wbdone_timeout = KICKOFF_TIMEOUT_MS;

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
	atomic_set(&phys_enc->wbirq_refcount, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);

	irq = &phys_enc->irq[INTR_IDX_WB_DONE];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "wb_done";
	irq->hw_idx =  wb_enc->hw_wb->idx;
	irq->irq_idx = -1;
	irq->intr_type = sde_encoder_phys_wb_get_intr_type(wb_enc->hw_wb);
	irq->intr_idx = INTR_IDX_WB_DONE;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_wb_done_irq;

	irq = &phys_enc->irq[INTR_IDX_PP1_OVFL];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "pp1_overflow";
	irq->hw_idx = CWB_1;
	irq->irq_idx = -1;
	irq->intr_type = SDE_IRQ_TYPE_CWB_OVERFLOW;
	irq->intr_idx = INTR_IDX_PP1_OVFL;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_cwb_ovflow;

	irq = &phys_enc->irq[INTR_IDX_PP2_OVFL];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "pp2_overflow";
	irq->hw_idx = CWB_2;
	irq->irq_idx = -1;
	irq->intr_type = SDE_IRQ_TYPE_CWB_OVERFLOW;
	irq->intr_idx = INTR_IDX_PP2_OVFL;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_cwb_ovflow;

	irq = &phys_enc->irq[INTR_IDX_PP3_OVFL];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "pp3_overflow";
	irq->hw_idx = CWB_3;
	irq->irq_idx = -1;
	irq->intr_type = SDE_IRQ_TYPE_CWB_OVERFLOW;
	irq->intr_idx = INTR_IDX_PP3_OVFL;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_cwb_ovflow;

	irq = &phys_enc->irq[INTR_IDX_PP4_OVFL];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "pp4_overflow";
	irq->hw_idx = CWB_4;
	irq->irq_idx = -1;
	irq->intr_type = SDE_IRQ_TYPE_CWB_OVERFLOW;
	irq->intr_idx = INTR_IDX_PP4_OVFL;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_cwb_ovflow;

	irq = &phys_enc->irq[INTR_IDX_PP5_OVFL];
	INIT_LIST_HEAD(&irq->cb.list);
	irq->name = "pp5_overflow";
	irq->hw_idx = CWB_5;
	irq->irq_idx = -1;
	irq->intr_type = SDE_IRQ_TYPE_CWB_OVERFLOW;
	irq->intr_idx = INTR_IDX_PP5_OVFL;
	irq->cb.arg = wb_enc;
	irq->cb.func = sde_encoder_phys_cwb_ovflow;

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
