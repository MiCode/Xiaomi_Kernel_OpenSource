// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#define WB_CSC_BASE			0x260
#define WB_DST_ADDR_SW_STATUS		0x2B0
#define WB_CDP_CNTL			0x2B4
#define WB_OUT_IMAGE_SIZE		0x2C0
#define WB_OUT_XY			0x2C4

#define CWB_CTRL_SRC_SEL		0x0
#define CWB_CTRL_MODE			0x4
#define CWB_CTRL_BLK_SIZE		0x100
#define CWB_CTRL_BASE_OFFSET		0x83000

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
	if (b) {
		b->base_off = addr;
		b->blk_off = CWB_CTRL_BASE_OFFSET;
		b->length = CWB_CTRL_BLK_SIZE * m->pingpong_count;
		b->hwversion = m->hwversion;
		b->log_mask = SDE_DBG_MASK_WB;

		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, "cwb", b->blk_off,
			b->blk_off + b->length, 0xff);
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

static void sde_hw_wb_program_cwb_ctrl(struct sde_hw_wb *ctx,
	const enum sde_cwb cur_idx, const enum sde_cwb data_src,
	bool dspp_out, bool enable)
{
	struct sde_hw_blk_reg_map *c;
	u32 blk_base;

	if (!ctx)
		return;

	c = &ctx->cwb_hw;
	blk_base = CWB_CTRL_BLK_SIZE * (cur_idx - CWB_0);

	if (enable) {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, data_src - CWB_0);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, dspp_out);
	} else {
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_SRC_SEL, 0xf);
		SDE_REG_WRITE(c, blk_base + CWB_CTRL_MODE, 0x0);
	}
}

static void _setup_wb_ops(struct sde_hw_wb_ops *ops,
	unsigned long features)
{
	ops->setup_outaddress = sde_hw_wb_setup_outaddress;
	ops->setup_outformat = sde_hw_wb_setup_format;

	if (test_bit(SDE_WB_XY_ROI_OFFSET, &features))
		ops->setup_roi = sde_hw_wb_roi;

	if (test_bit(SDE_WB_QOS, &features))
		ops->setup_qos_lut = sde_hw_wb_setup_qos_lut;

	if (test_bit(SDE_WB_CDP, &features))
		ops->setup_cdp = sde_hw_wb_setup_cdp;

	if (test_bit(SDE_WB_INPUT_CTRL, &features))
		ops->bind_pingpong_blk = sde_hw_wb_bind_pingpong_blk;

	if (test_bit(SDE_WB_CWB_CTRL, &features))
		ops->program_cwb_ctrl = sde_hw_wb_program_cwb_ctrl;
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

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_wb_destroy(struct sde_hw_wb *hw_wb)
{
	if (hw_wb)
		sde_hw_blk_destroy(&hw_wb->base);
	kfree(hw_wb);
}
