// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_wb.h"
#include "sde_formats.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define WB_DST_FORMAT			0x000
#define WB_DST_OP_MODE			0x004
#define WB_DST_PACK_PATTERN		0x008
#define WB_DST0_ADDR			0x00C
#define WB_DST1_ADDR			0x010
#define WB_DST2_ADDR			0x014
#define WB_DST3_ADDR			0x018
#define WB_DST_YSTRIDE0			0x01C
#define WB_DST_YSTRIDE1			0x020
#define WB_DST_YSTRIDE1			0x020
#define WB_DST_DITHER_BITDEPTH		0x024
#define WB_DST_MATRIX_ROW0		0x030
#define WB_DST_MATRIX_ROW1		0x034
#define WB_DST_MATRIX_ROW2		0x038
#define WB_DST_MATRIX_ROW3		0x03C
#define WB_DST_WRITE_CONFIG		0x048
#define WB_ROTATION_DNSCALER		0x050
#define WB_ROTATOR_PIPE_DOWNSCALER	0x054
#define WB_N16_INIT_PHASE_X_C03		0x060
#define WB_N16_INIT_PHASE_X_C12		0x064
#define WB_N16_INIT_PHASE_Y_C03		0x068
#define WB_N16_INIT_PHASE_Y_C12		0x06C
#define WB_OUT_SIZE			0x074
#define WB_ALPHA_X_VALUE		0x078
#define WB_DANGER_LUT			0x084
#define WB_SAFE_LUT			0x088
#define WB_QOS_CTRL			0x090
#define WB_CREQ_LUT_0			0x098
#define WB_CREQ_LUT_1			0x09C
#define WB_UBWC_STATIC_CTRL		0x144
#define WB_MUX				0x150
#define WB_CROP_CTRL			0x154
#define WB_CROP_OFFSET			0x158
#define WB_CSC_BASE			0x260
#define WB_DST_ADDR_SW_STATUS		0x2B0
#define WB_CDP_CNTL			0x2B4
#define WB_OUT_IMAGE_SIZE		0x2C0
#define WB_OUT_XY			0x2C4

#define CWB_CTRL_SRC_SEL		0x0
#define CWB_CTRL_MODE			0x4

/* WB_QOS_CTRL */
#define WB_QOS_CTRL_DANGER_SAFE_EN	BIT(0)

static struct sde_wb_cfg *_wb_offset(enum sde_wb wb,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->wb_count; i++) {
		if (wb == m->wb[i].id) {
			b->base_off = addr;
			b->blk_off = m->wb[i].base;
			b->length = m->wb[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_WB;
			return &m->wb[i];
		}
	}
	return ERR_PTR(-EINVAL);
}

static void _sde_hw_cwb_ctrl_init(struct sde_mdss_cfg *m,
		void __iomem *addr, struct sde_hw_blk_reg_map *b)
{
	int i;
	u32 blk_off;
	char name[64] = {0};

	if (!b)
		return;

	b->base_off = addr;
	b->blk_off = m->cwb_blk_off;
	b->length = 0x20;
	b->hwversion = m->hwversion;
	b->log_mask = SDE_DBG_MASK_WB;

	for (i = 0; i < m->pingpong_count; i++) {
		snprintf(name, sizeof(name), "cwb%d", i);
		blk_off = b->blk_off + (m->cwb_blk_stride * i);

		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, name,
				blk_off, blk_off + b->length, 0xff);
	}
}

static void _sde_hw_dcwb_ctrl_init(struct sde_mdss_cfg *m,
		void __iomem *addr, struct sde_hw_blk_reg_map *b)
{
	int i;
	u32 blk_off;
	char name[64] = {0};

	if (!b)
		return;

	b->base_off = addr;
	b->blk_off = m->cwb_blk_off;
	b->length = 0x20;
	b->hwversion = m->hwversion;
	b->log_mask = SDE_DBG_MASK_WB;

	for (i = 0; i < m->dcwb_count; i++) {
		snprintf(name, sizeof(name), "dcwb%d", i);
		blk_off = b->blk_off + (m->cwb_blk_stride * i);

		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, name,
				blk_off, blk_off + b->length, 0xff);
	}
}

static void _sde_hw_dcwb_pp_ctrl_init(struct sde_mdss_cfg *m,
		void __iomem *addr, struct sde_hw_wb *hw_wb)
{
	int i = 0, dcwb_pp_count = 0;
	struct sde_pingpong_cfg *pp_blk = NULL;

	if (!hw_wb) {
		DRM_ERROR("hw_wb is null\n");
		return;
	}

	for (i = 0; i < m->pingpong_count; i++) {
		pp_blk = &m->pingpong[i];
		if (test_bit(SDE_PINGPONG_CWB_DITHER, &pp_blk->features)) {
			if (dcwb_pp_count < DCWB_MAX - DCWB_0) {
				hw_wb->dcwb_pp_hw[dcwb_pp_count].caps = pp_blk;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].idx = pp_blk->id;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].hw.base_off = addr;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].hw.blk_off = pp_blk->base;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].hw.length = pp_blk->len;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].hw.hwversion = m->hwversion;
				hw_wb->dcwb_pp_hw[dcwb_pp_count].hw.log_mask = SDE_DBG_MASK_WB;
			} else {
				DRM_ERROR("Invalid dcwb pp count %d more than %d",
					dcwb_pp_count, DCWB_MAX - DCWB_0);
				return;
			}
			++dcwb_pp_count;
		}
	}
}

static void sde_hw_wb_setup_outaddress(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;

	SDE_REG_WRITE(c, WB_DST0_ADDR, data->dest.plane_addr[0]);
	SDE_REG_WRITE(c, WB_DST1_ADDR, data->dest.plane_addr[1]);
	SDE_REG_WRITE(c, WB_DST2_ADDR, data->dest.plane_addr[2]);
	SDE_REG_WRITE(c, WB_DST3_ADDR, data->dest.plane_addr[3]);
}

static void sde_hw_wb_setup_format(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	const struct sde_format *fmt = data->dest.format;
	u32 dst_format, pattern, ystride0, ystride1, outsize, chroma_samp;
	u32 write_config = 0;
	u32 opmode = 0;
	u32 dst_addr_sw = 0;

	chroma_samp = fmt->chroma_sample;

	dst_format = (chroma_samp << 23) |
			(fmt->fetch_planes << 19) |
			(fmt->bits[C3_ALPHA] << 6) |
			(fmt->bits[C2_R_Cr] << 4) |
			(fmt->bits[C1_B_Cb] << 2) |
			(fmt->bits[C0_G_Y] << 0);

	if (fmt->bits[C3_ALPHA] || fmt->alpha_enable) {
		dst_format |= BIT(8); /* DSTC3_EN */
		if (!fmt->alpha_enable ||
				!(ctx->caps->features & BIT(SDE_WB_PIPE_ALPHA)))
			dst_format |= BIT(14); /* DST_ALPHA_X */
	}

	if (SDE_FORMAT_IS_YUV(fmt) &&
			(ctx->caps->features & BIT(SDE_WB_YUV_CONFIG)))
		dst_format |= BIT(15);

	if (SDE_FORMAT_IS_DX(fmt))
		dst_format |= BIT(21);

	pattern = (fmt->element[3] << 24) |
			(fmt->element[2] << 16) |
			(fmt->element[1] << 8)  |
			(fmt->element[0] << 0);

	dst_format |= (fmt->unpack_align_msb << 18) |
			(fmt->unpack_tight << 17) |
			((fmt->unpack_count - 1) << 12) |
			((fmt->bpp - 1) << 9);

	ystride0 = data->dest.plane_pitch[0] |
			(data->dest.plane_pitch[1] << 16);
	ystride1 = data->dest.plane_pitch[2] |
			(data->dest.plane_pitch[3] << 16);

	if (data->roi.h && data->roi.w)
		outsize = (data->roi.h << 16) | data->roi.w;
	else
		outsize = (data->dest.height << 16) | data->dest.width;

	if (SDE_FORMAT_IS_UBWC(fmt)) {
		opmode |= BIT(0);
		dst_format |= BIT(31);
		write_config |= (ctx->mdp->highest_bank_bit << 8);
		if (fmt->base.pixel_format == DRM_FORMAT_RGB565)
			write_config |= 0x8;
		if (IS_UBWC_20_SUPPORTED(ctx->catalog->ubwc_version))
			SDE_REG_WRITE(c, WB_UBWC_STATIC_CTRL,
					(ctx->mdp->ubwc_swizzle << 0) |
					(ctx->mdp->highest_bank_bit << 4));
		if (IS_UBWC_10_SUPPORTED(ctx->catalog->ubwc_version))
			SDE_REG_WRITE(c, WB_UBWC_STATIC_CTRL,
					(ctx->mdp->ubwc_swizzle << 0) |
					BIT(8) |
					(ctx->mdp->highest_bank_bit << 4));
	}

	if (data->is_secure)
		dst_addr_sw |= BIT(0);

	SDE_REG_WRITE(c, WB_ALPHA_X_VALUE, 0xFF);
	SDE_REG_WRITE(c, WB_DST_FORMAT, dst_format);
	SDE_REG_WRITE(c, WB_DST_OP_MODE, opmode);
	SDE_REG_WRITE(c, WB_DST_PACK_PATTERN, pattern);
	SDE_REG_WRITE(c, WB_DST_YSTRIDE0, ystride0);
	SDE_REG_WRITE(c, WB_DST_YSTRIDE1, ystride1);
	SDE_REG_WRITE(c, WB_OUT_SIZE, outsize);
	SDE_REG_WRITE(c, WB_DST_WRITE_CONFIG, write_config);
	SDE_REG_WRITE(c, WB_DST_ADDR_SW_STATUS, dst_addr_sw);
}

static void sde_hw_wb_roi(struct sde_hw_wb *ctx, struct sde_hw_wb_cfg *wb)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 image_size, out_size, out_xy;

	image_size = (wb->dest.height << 16) | wb->dest.width;
	out_xy = (wb->roi.y << 16) | wb->roi.x;
	out_size = (wb->roi.h << 16) | wb->roi.w;

	SDE_REG_WRITE(c, WB_OUT_IMAGE_SIZE, image_size);
	SDE_REG_WRITE(c, WB_OUT_XY, out_xy);
	SDE_REG_WRITE(c, WB_OUT_SIZE, out_size);
}

static void sde_hw_wb_crop(struct sde_hw_wb *ctx, struct sde_hw_wb_cfg *wb, bool crop)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 crop_xy;

	crop_xy = (wb->crop.y << 16) | wb->crop.x;

	if (crop) {
		SDE_REG_WRITE(c, WB_CROP_CTRL, 0x1);
		SDE_REG_WRITE(c, WB_CROP_OFFSET, crop_xy);
	} else {
		SDE_REG_WRITE(c, WB_CROP_CTRL, 0x0);
	}
}

static void sde_hw_wb_setup_qos_lut(struct sde_hw_wb *ctx,
		struct sde_hw_wb_qos_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 qos_ctrl = 0;

	if (!ctx || !cfg)
		return;

	SDE_REG_WRITE(c, WB_DANGER_LUT, cfg->danger_lut);
	SDE_REG_WRITE(c, WB_SAFE_LUT, cfg->safe_lut);

	if (ctx->caps && test_bit(SDE_WB_QOS_8LVL, &ctx->caps->features)) {
		SDE_REG_WRITE(c, WB_CREQ_LUT_0, cfg->creq_lut);
		SDE_REG_WRITE(c, WB_CREQ_LUT_1, cfg->creq_lut >> 32);
	}

	if (cfg->danger_safe_en)
		qos_ctrl |= WB_QOS_CTRL_DANGER_SAFE_EN;

	SDE_REG_WRITE(c, WB_QOS_CTRL, qos_ctrl);
}

static void sde_hw_wb_setup_cdp(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cdp_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 cdp_cntl = 0;

	if (!ctx || !cfg)
		return;

	c = &ctx->hw;

	if (cfg->enable)
		cdp_cntl |= BIT(0);
	if (cfg->ubwc_meta_enable)
		cdp_cntl |= BIT(1);
	if (cfg->preload_ahead == SDE_WB_CDP_PRELOAD_AHEAD_64)
		cdp_cntl |= BIT(3);

	SDE_REG_WRITE(c, WB_CDP_CNTL, cdp_cntl);
}

static void sde_hw_wb_bind_pingpong_blk(
		struct sde_hw_wb *ctx,
		bool enable,
		const enum sde_pingpong pp)
{
	struct sde_hw_blk_reg_map *c;
	int mux_cfg = 0xF;

	if (!ctx)
		return;

	c = &ctx->hw;
	if (enable)
		mux_cfg = (pp - PINGPONG_0) & 0x7;

	SDE_REG_WRITE(c, WB_MUX, mux_cfg);
}

static void sde_hw_wb_bind_dcwb_pp_blk(
		struct sde_hw_wb *ctx,
		bool enable,
		const enum sde_pingpong pp)
{
	struct sde_hw_blk_reg_map *c;
	int mux_cfg = 0xF;

	if (!ctx)
		return;

	c = &ctx->hw;
	if (enable)
		mux_cfg = 0xd;

	SDE_REG_WRITE(c, WB_MUX, mux_cfg);
}

static void sde_hw_wb_program_dcwb_ctrl(struct sde_hw_wb *ctx,
	const enum sde_dcwb cur_idx, const enum sde_cwb data_src,
	int tap_location, bool enable)
{
	struct sde_hw_blk_reg_map *c;
	u32 blk_base;

	if (!ctx)
		return;

	c = &ctx->dcwb_hw;
	blk_base  = ctx->catalog->cwb_blk_stride * (cur_idx - DCWB_0);

	if (enable) {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, data_src - CWB_0);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, tap_location);
	} else {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, 0xf);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, 0x0);
	}
}

static void sde_hw_wb_program_cwb_ctrl(struct sde_hw_wb *ctx,
	const enum sde_cwb cur_idx, const enum sde_cwb data_src,
	bool dspp_out, bool enable)
{
	struct sde_hw_blk_reg_map *c;
	u32 blk_base;

	if (!ctx)
		return;

	c = &ctx->cwb_hw;
	blk_base  = ctx->catalog->cwb_blk_stride * (cur_idx - CWB_0);

	if (enable) {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, data_src - CWB_0);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, dspp_out);
	} else {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, 0xf);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, 0x0);
	}
}

static void sde_hw_wb_program_cwb_dither_ctrl(struct sde_hw_wb *ctx,
		const enum sde_dcwb dcwb_idx, void *cfg, size_t len, bool enable)
{
	struct sde_hw_pingpong *pp = NULL;
	struct sde_hw_blk_reg_map *c = NULL;
	struct drm_msm_dither *dither_data = NULL;
	enum sde_pingpong pp_id = PINGPONG_MAX;
	u32 dither_base = 0, offset = 0, data = 0, idx = 0;
	bool found = false;

	if (!ctx) {
		DRM_ERROR("Invalid pointer ctx is null\n");
		return;
	}

	/* map to pp_id from dcwb id */
	if (dcwb_idx == DCWB_0) {
		pp_id = PINGPONG_CWB_0;
	} else if (dcwb_idx == DCWB_1) {
		pp_id = PINGPONG_CWB_1;
	} else {
		DRM_ERROR("Invalid dcwb_idx %d\n", dcwb_idx);
		return;
	}

	/* find pp blk with pp_id */
	for (idx = 0; idx < DCWB_MAX - DCWB_0; ++idx) {
		pp = &ctx->dcwb_pp_hw[idx];
		if (pp && pp->idx == pp_id) {
			found = true;
			break;
		}
	}

	if (!found) {
		DRM_ERROR("Not found pp id %d\n", pp_id);
		return;
	}

	if (!test_bit(SDE_PINGPONG_CWB_DITHER, &pp->caps->features)) {
		DRM_ERROR("Invalid ping-pong cwb config dcwb idx %d pp id %d\n",
			dcwb_idx, pp_id);
		return;
	}

	c = &pp->hw;
	dither_base = pp->caps->sblk->dither.base;
	dither_data = (struct drm_msm_dither *)cfg;
	if (!dither_data || !enable) {
		SDE_REG_WRITE(c, dither_base, 0);
		SDE_DEBUG("cwb dither disabled, dcwb_idx %u pp_id %u\n", dcwb_idx, pp_id);
		return;
	}

	if (len != sizeof(struct drm_msm_dither)) {
		SDE_ERROR("input len %zu, expected len %zu\n", len,
			sizeof(struct drm_msm_dither));
		return;
	}

	if (dither_data->c0_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither_data->c1_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither_data->c2_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither_data->c3_bitdepth >= DITHER_DEPTH_MAP_INDEX) {
		SDE_ERROR("Invalid bitdepth [c0, c1, c2, c3] = [%u, %u, %u, %u]\n",
			dither_data->c0_bitdepth, dither_data->c1_bitdepth,
			dither_data->c2_bitdepth, dither_data->c3_bitdepth);
		return;
	}

	offset += 4;
	data = dither_depth_map[dither_data->c0_bitdepth] & REG_MASK(2);
	data |= (dither_depth_map[dither_data->c1_bitdepth] & REG_MASK(2)) << 2;
	data |= (dither_depth_map[dither_data->c2_bitdepth] & REG_MASK(2)) << 4;
	data |= (dither_depth_map[dither_data->c3_bitdepth] & REG_MASK(2)) << 6;
	data |= (dither_data->temporal_en) ? (1 << 8) : 0;
	SDE_REG_WRITE(c, dither_base + offset, data);

	for (idx = 0; idx < DITHER_MATRIX_SZ - 3; idx += 4) {
		offset += 4;
		data = (dither_data->matrix[idx] & REG_MASK(4)) |
			((dither_data->matrix[idx + 1] & REG_MASK(4)) << 4) |
			((dither_data->matrix[idx + 2] & REG_MASK(4)) << 8) |
			((dither_data->matrix[idx + 3] & REG_MASK(4)) << 12);
		SDE_REG_WRITE(c, dither_base + offset, data);
	}

	/* Enable dither */
	if (test_bit(SDE_PINGPONG_DITHER_LUMA, &pp->caps->features)
			&& (dither_data->flags & DITHER_LUMA_MODE))
		SDE_REG_WRITE(c, dither_base, 0x11);
	else
		SDE_REG_WRITE(c, dither_base, 1);
	SDE_DEBUG("cwb dither enabled, dcwb_idx %u pp_id %u\n", dcwb_idx, pp_id);
}

static void _setup_wb_ops(struct sde_hw_wb_ops *ops,
	unsigned long features)
{
	ops->setup_outaddress = sde_hw_wb_setup_outaddress;
	ops->setup_outformat = sde_hw_wb_setup_format;

	if (test_bit(SDE_WB_XY_ROI_OFFSET, &features))
		ops->setup_roi = sde_hw_wb_roi;

	if (test_bit(SDE_WB_CROP, &features))
		ops->setup_crop = sde_hw_wb_crop;

	if (test_bit(SDE_WB_QOS, &features))
		ops->setup_qos_lut = sde_hw_wb_setup_qos_lut;

	if (test_bit(SDE_WB_CDP, &features))
		ops->setup_cdp = sde_hw_wb_setup_cdp;

	if (test_bit(SDE_WB_INPUT_CTRL, &features))
		ops->bind_pingpong_blk = sde_hw_wb_bind_pingpong_blk;

	if (test_bit(SDE_WB_CWB_CTRL, &features))
		ops->program_cwb_ctrl = sde_hw_wb_program_cwb_ctrl;

	if (test_bit(SDE_WB_DCWB_CTRL, &features)) {
		ops->program_dcwb_ctrl = sde_hw_wb_program_dcwb_ctrl;
		ops->bind_dcwb_pp_blk = sde_hw_wb_bind_dcwb_pp_blk;
	}

	if (test_bit(SDE_WB_CWB_DITHER_CTRL, &features))
		ops->program_cwb_dither_ctrl = sde_hw_wb_program_cwb_dither_ctrl;
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_wb *sde_hw_wb_init(enum sde_wb idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m,
		struct sde_hw_mdp *hw_mdp)
{
	struct sde_hw_wb *c;
	struct sde_wb_cfg *cfg;
	int rc;

	if (!addr || !m || !hw_mdp)
		return ERR_PTR(-EINVAL);

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _wb_offset(idx, m, addr, &c->hw);
	if (IS_ERR(cfg)) {
		WARN(1, "Unable to find wb idx=%d\n", idx);
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->catalog = m;
	c->mdp = &m->mdp[0];
	c->idx = idx;
	c->caps = cfg;
	_setup_wb_ops(&c->ops, c->caps->features);
	c->hw_mdp = hw_mdp;

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_WB, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	if (test_bit(SDE_WB_CWB_CTRL, &cfg->features))
		_sde_hw_cwb_ctrl_init(m, addr, &c->cwb_hw);

	if (test_bit(SDE_WB_DCWB_CTRL, &cfg->features)) {
		_sde_hw_dcwb_ctrl_init(m, addr, &c->dcwb_hw);
		_sde_hw_dcwb_pp_ctrl_init(m, addr, c);
	}

	return c;

blk_init_error:
	kfree(c);

	return ERR_PTR(rc);
}

void sde_hw_wb_destroy(struct sde_hw_wb *hw_wb)
{
	if (hw_wb)
		sde_hw_blk_destroy(&hw_wb->base);
	kfree(hw_wb);
}
