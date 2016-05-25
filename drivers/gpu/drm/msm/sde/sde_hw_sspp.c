/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_lm.h"
#include "sde_hw_sspp.h"

#define SDE_FETCH_CONFIG_RESET_VALUE   0x00000087

/* SDE_SSPP_SRC */
#define SSPP_SRC_SIZE                      0x00
#define SSPP_SRC_XY                        0x08
#define SSPP_OUT_SIZE                      0x0c
#define SSPP_OUT_XY                        0x10
#define SSPP_SRC0_ADDR                     0x14
#define SSPP_SRC1_ADDR                     0x18
#define SSPP_SRC2_ADDR                     0x1C
#define SSPP_SRC3_ADDR                     0x20
#define SSPP_SRC_YSTRIDE0                  0x24
#define SSPP_SRC_YSTRIDE1                  0x28
#define SSPP_SRC_FORMAT                    0x30
#define SSPP_SRC_UNPACK_PATTERN            0x34
#define SSPP_SRC_OP_MODE                   0x38
#define MDSS_MDP_OP_DEINTERLACE            BIT(22)

#define MDSS_MDP_OP_DEINTERLACE_ODD        BIT(23)
#define MDSS_MDP_OP_IGC_ROM_1              BIT(18)
#define MDSS_MDP_OP_IGC_ROM_0              BIT(17)
#define MDSS_MDP_OP_IGC_EN                 BIT(16)
#define MDSS_MDP_OP_FLIP_UD                BIT(14)
#define MDSS_MDP_OP_FLIP_LR                BIT(13)
#define MDSS_MDP_OP_BWC_EN                 BIT(0)
#define MDSS_MDP_OP_PE_OVERRIDE            BIT(31)
#define MDSS_MDP_OP_BWC_LOSSLESS           (0 << 1)
#define MDSS_MDP_OP_BWC_Q_HIGH             (1 << 1)
#define MDSS_MDP_OP_BWC_Q_MED              (2 << 1)

#define SSPP_SRC_CONSTANT_COLOR            0x3c
#define SSPP_FETCH_CONFIG                  0x048
#define SSPP_DANGER_LUT                    0x60
#define SSPP_SAFE_LUT                      0x64
#define SSPP_CREQ_LUT                      0x68
#define SSPP_DECIMATION_CONFIG             0xB4
#define SSPP_SRC_ADDR_SW_STATUS            0x70
#define SSPP_SW_PIX_EXT_C0_LR              0x100
#define SSPP_SW_PIX_EXT_C0_TB              0x104
#define SSPP_SW_PIX_EXT_C0_REQ_PIXELS      0x108
#define SSPP_SW_PIX_EXT_C1C2_LR            0x110
#define SSPP_SW_PIX_EXT_C1C2_TB            0x114
#define SSPP_SW_PIX_EXT_C1C2_REQ_PIXELS    0x118
#define SSPP_SW_PIX_EXT_C3_LR              0x120
#define SSPP_SW_PIX_EXT_C3_TB              0x124
#define SSPP_SW_PIX_EXT_C3_REQ_PIXELS      0x128
#define SSPP_UBWC_ERROR_STATUS             0x138
#define SSPP_VIG_OP_MODE                   0x0

/* SDE_SSPP_SCALER_QSEED2 */
#define SCALE_CONFIG                       0x04
#define COMP0_3_PHASE_STEP_X               0x10
#define COMP0_3_PHASE_STEP_Y               0x14
#define COMP1_2_PHASE_STEP_X               0x18
#define COMP1_2_PHASE_STEP_Y               0x1c
#define COMP0_3_INIT_PHASE_X               0x20
#define COMP0_3_INIT_PHASE_Y               0x24
#define COMP1_2_INIT_PHASE_X               0x28
#define COMP1_2_INIT_PHASE_Y               0x2C
#define VIG_0_QSEED2_SHARP                 0x30

/*
 * Definitions for ViG op modes
 */
#define VIG_OP_CSC_DST_DATAFMT BIT(19)
#define VIG_OP_CSC_SRC_DATAFMT BIT(18)
#define VIG_OP_CSC_EN          BIT(17)
#define VIG_OP_MEM_PROT_CONT   BIT(15)
#define VIG_OP_MEM_PROT_VAL    BIT(14)
#define VIG_OP_MEM_PROT_SAT    BIT(13)
#define VIG_OP_MEM_PROT_HUE    BIT(12)
#define VIG_OP_HIST            BIT(8)
#define VIG_OP_SKY_COL         BIT(7)
#define VIG_OP_FOIL            BIT(6)
#define VIG_OP_SKIN_COL        BIT(5)
#define VIG_OP_PA_EN           BIT(4)
#define VIG_OP_PA_SAT_ZERO_EXP BIT(2)
#define VIG_OP_MEM_PROT_BLEND  BIT(1)

static inline int _sspp_subblk_offset(struct sde_hw_pipe *ctx,
		int s_id,
		u32 *idx)
{
	int rc = 0;
	const struct sde_sspp_sub_blks *sblk = ctx->cap->sblk;

	if (!ctx)
		return -EINVAL;

	switch (s_id) {
	case SDE_SSPP_SRC:
		*idx = sblk->src_blk.base;
		break;
	case SDE_SSPP_SCALER_QSEED2:
	case SDE_SSPP_SCALER_QSEED3:
	case SDE_SSPP_SCALER_RGB:
		*idx = sblk->scaler_blk.base;
		break;
	case SDE_SSPP_CSC:
		*idx = sblk->csc_blk.base;
		break;
	case SDE_SSPP_PA_V1:
		*idx = sblk->pa_blk.base;
		break;
	case SDE_SSPP_HIST_V1:
		*idx = sblk->hist_lut.base;
		break;
	case SDE_SSPP_PCC:
		*idx = sblk->pcc_blk.base;
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static void _sspp_setup_opmode(struct sde_hw_pipe *ctx,
		u32 mask, u8 en)
{
	u32 idx;
	u32 opmode;

	if (!_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED2, &idx) &&
			(test_bit(SDE_SSPP_CSC, &ctx->cap->features) ||
			 test_bit(SDE_SSPP_PA_V1, &ctx->cap->features))) {
		opmode = SDE_REG_READ(&ctx->hw, SSPP_VIG_OP_MODE + idx);

		if (en)
			opmode |= mask;
		else
			opmode &= ~mask;

		SDE_REG_WRITE(&ctx->hw, SSPP_VIG_OP_MODE + idx, opmode);
	}
}
/**
 * Setup source pixel format, flip,
 */
static void sde_hw_sspp_setup_format(struct sde_hw_pipe *ctx,
		const struct sde_format *fmt, u32 flags)
{
	struct sde_hw_blk_reg_map *c;
	u32 chroma_samp, unpack, src_format;
	u32 secure = 0;
	u32 opmode = 0;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !fmt)
		return;

	c = &ctx->hw;
	opmode = SDE_REG_READ(c, SSPP_SRC_OP_MODE + idx);
	opmode &= ~(MDSS_MDP_OP_FLIP_LR | MDSS_MDP_OP_FLIP_UD |
			MDSS_MDP_OP_BWC_EN | MDSS_MDP_OP_PE_OVERRIDE);

	if (flags & SDE_SSPP_SECURE_OVERLAY_SESSION)
		secure = 0xF;

	if (flags & SDE_SSPP_FLIP_LR)
		opmode |= MDSS_MDP_OP_FLIP_LR;
	if (flags & SDE_SSPP_FLIP_UD)
		opmode |= MDSS_MDP_OP_FLIP_UD;

	chroma_samp = fmt->chroma_sample;
	if (flags & SDE_SSPP_SOURCE_ROTATED_90) {
		if (chroma_samp == SDE_CHROMA_H2V1)
			chroma_samp = SDE_CHROMA_H1V2;
		else if (chroma_samp == SDE_CHROMA_H1V2)
			chroma_samp = SDE_CHROMA_H2V1;
	}

	src_format = (chroma_samp << 23) | (fmt->fetch_planes << 19) |
		(fmt->bits[C3_ALPHA] << 6) | (fmt->bits[C2_R_Cr] << 4) |
		(fmt->bits[C1_B_Cb] << 2) | (fmt->bits[C0_G_Y] << 0);

	if (flags & SDE_SSPP_ROT_90)
		src_format |= BIT(11); /* ROT90 */

	if (fmt->alpha_enable && fmt->fetch_planes == SDE_PLANE_INTERLEAVED)
		src_format |= BIT(8); /* SRCC3_EN */

	if (flags & SDE_SSPP_SOLID_FILL)
		src_format |= BIT(22);

	unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
		(fmt->element[1] << 8) | (fmt->element[0] << 0);
	src_format |= ((fmt->unpack_count - 1) << 12) |
		(fmt->unpack_tight << 17) |
		(fmt->unpack_align_msb << 18) |
		((fmt->bpp - 1) << 9);

	if (fmt->fetch_mode != SDE_FETCH_LINEAR) {
		if (SDE_FORMAT_IS_UBWC(fmt))
			opmode |= MDSS_MDP_OP_BWC_EN;
		src_format |= (fmt->fetch_mode & 3) << 30; /*FRAME_FORMAT */
		SDE_REG_WRITE(c, SSPP_FETCH_CONFIG,
			SDE_FETCH_CONFIG_RESET_VALUE |
			ctx->highest_bank_bit << 18);
	}

	opmode |= MDSS_MDP_OP_PE_OVERRIDE;

	/* if this is YUV pixel format, enable CSC */
	if (SDE_FORMAT_IS_YUV(fmt))
		src_format |= BIT(15);

	/* update scaler opmode, if appropriate */
	_sspp_setup_opmode(ctx,
		VIG_OP_CSC_EN | VIG_OP_CSC_SRC_DATAFMT, SDE_FORMAT_IS_YUV(fmt));

	SDE_REG_WRITE(c, SSPP_SRC_FORMAT + idx, src_format);
	SDE_REG_WRITE(c, SSPP_SRC_UNPACK_PATTERN + idx, unpack);
	SDE_REG_WRITE(c, SSPP_SRC_OP_MODE + idx, opmode);
	SDE_REG_WRITE(c, SSPP_SRC_ADDR_SW_STATUS + idx, secure);

	/* clear previous UBWC error */
	SDE_REG_WRITE(c, SSPP_UBWC_ERROR_STATUS + idx, BIT(31));
}

static void sde_hw_sspp_setup_pe_config(struct sde_hw_pipe *ctx,
		struct sde_hw_pixel_ext *pe_ext)
{
	struct sde_hw_blk_reg_map *c;
	u8 color;
	u32 lr_pe[4], tb_pe[4], tot_req_pixels[4];
	const u32 bytemask = 0xff;
	const u32 shortmask = 0xffff;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !pe_ext)
		return;

	c = &ctx->hw;

	/* program SW pixel extension override for all pipes*/
	for (color = 0; color < SDE_MAX_PLANES; color++) {
		/* color 2 has the same set of registers as color 1 */
		if (color == 2)
			continue;

		lr_pe[color] = ((pe_ext->right_ftch[color] & bytemask) << 24)|
			((pe_ext->right_rpt[color] & bytemask) << 16)|
			((pe_ext->left_ftch[color] & bytemask) << 8)|
			(pe_ext->left_rpt[color] & bytemask);

		tb_pe[color] = ((pe_ext->btm_ftch[color] & bytemask) << 24)|
			((pe_ext->btm_rpt[color] & bytemask) << 16)|
			((pe_ext->top_ftch[color] & bytemask) << 8)|
			(pe_ext->top_rpt[color] & bytemask);

		tot_req_pixels[color] = (((pe_ext->roi_h[color] +
			pe_ext->num_ext_pxls_top[color] +
			pe_ext->num_ext_pxls_btm[color]) & shortmask) << 16) |
			((pe_ext->roi_w[color] +
			pe_ext->num_ext_pxls_left[color] +
			pe_ext->num_ext_pxls_right[color]) & shortmask);
	}

	/* color 0 */
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C0_LR + idx, lr_pe[0]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C0_TB + idx, tb_pe[0]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C0_REQ_PIXELS + idx,
			tot_req_pixels[0]);

	/* color 1 and color 2 */
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C1C2_LR + idx, lr_pe[1]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C1C2_TB + idx, tb_pe[1]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C1C2_REQ_PIXELS + idx,
			tot_req_pixels[1]);

	/* color 3 */
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C3_LR + idx, lr_pe[3]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C3_TB + idx, lr_pe[3]);
	SDE_REG_WRITE(c, SSPP_SW_PIX_EXT_C3_REQ_PIXELS + idx,
			tot_req_pixels[3]);
}

static void _sde_hw_sspp_setup_scaler(struct sde_hw_pipe *ctx,
		struct sde_hw_pixel_ext *pe)
{
	struct sde_hw_blk_reg_map *c;
	int config_h = 0x0;
	int config_v = 0x0;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED2, &idx) || !pe)
		return;

	c = &ctx->hw;

	/* enable scaler(s) if valid filter set */
	if (pe->horz_filter[SDE_SSPP_COMP_0] < SDE_SCALE_FILTER_MAX)
		config_h |= pe->horz_filter[SDE_SSPP_COMP_0] << 8;
	if (pe->horz_filter[SDE_SSPP_COMP_1_2] < SDE_SCALE_FILTER_MAX)
		config_h |= pe->horz_filter[SDE_SSPP_COMP_1_2] << 12;
	if (pe->horz_filter[SDE_SSPP_COMP_3] < SDE_SCALE_FILTER_MAX)
		config_h |= pe->horz_filter[SDE_SSPP_COMP_3] << 16;

	if (config_h)
		config_h |= BIT(0);

	if (pe->vert_filter[SDE_SSPP_COMP_0] < SDE_SCALE_FILTER_MAX)
		config_v |= pe->vert_filter[SDE_SSPP_COMP_0] << 10;
	if (pe->vert_filter[SDE_SSPP_COMP_1_2] < SDE_SCALE_FILTER_MAX)
		config_v |= pe->vert_filter[SDE_SSPP_COMP_1_2] << 14;
	if (pe->vert_filter[SDE_SSPP_COMP_3] < SDE_SCALE_FILTER_MAX)
		config_v |= pe->vert_filter[SDE_SSPP_COMP_3] << 18;

	if (config_v)
		config_v |= BIT(1);

	SDE_REG_WRITE(c, SCALE_CONFIG + idx,  config_h | config_v);
	SDE_REG_WRITE(c, COMP0_3_INIT_PHASE_X + idx,
		pe->init_phase_x[SDE_SSPP_COMP_0]);
	SDE_REG_WRITE(c, COMP0_3_INIT_PHASE_Y + idx,
		pe->init_phase_y[SDE_SSPP_COMP_0]);
	SDE_REG_WRITE(c, COMP0_3_PHASE_STEP_X + idx,
		pe->phase_step_x[SDE_SSPP_COMP_0]);
	SDE_REG_WRITE(c, COMP0_3_PHASE_STEP_Y + idx,
		pe->phase_step_y[SDE_SSPP_COMP_0]);

	SDE_REG_WRITE(c, COMP1_2_INIT_PHASE_X + idx,
		pe->init_phase_x[SDE_SSPP_COMP_1_2]);
	SDE_REG_WRITE(c, COMP1_2_INIT_PHASE_Y + idx,
		pe->init_phase_y[SDE_SSPP_COMP_1_2]);
	SDE_REG_WRITE(c, COMP1_2_PHASE_STEP_X + idx,
		pe->phase_step_x[SDE_SSPP_COMP_1_2]);
	SDE_REG_WRITE(c, COMP1_2_PHASE_STEP_Y + idx,
		pe->phase_step_y[SDE_SSPP_COMP_1_2]);
}

/**
 * sde_hw_sspp_setup_rects()
 */
static void sde_hw_sspp_setup_rects(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *cfg,
		struct sde_hw_pixel_ext *pe_ext)
{
	struct sde_hw_blk_reg_map *c;
	u32 src_size, src_xy, dst_size, dst_xy, ystride0, ystride1;
	u32 decimation = 0;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !cfg)
		return;

	c = &ctx->hw;

	/* program pixel extension override */
	if (pe_ext)
		sde_hw_sspp_setup_pe_config(ctx, pe_ext);

	/* src and dest rect programming */
	src_xy = (cfg->src_rect.y << 16) | (cfg->src_rect.x);
	src_size = (cfg->src_rect.h << 16) | (cfg->src_rect.w);
	dst_xy = (cfg->dst_rect.y << 16) | (cfg->dst_rect.x);
	dst_size = (cfg->dst_rect.h << 16) | (cfg->dst_rect.w);

	ystride0 = (cfg->layout.plane_pitch[0]) |
			(cfg->layout.plane_pitch[1] << 16);
	ystride1 = (cfg->layout.plane_pitch[2]) |
			(cfg->layout.plane_pitch[3] << 16);

	/* program scaler, phase registers, if pipes supporting scaling */
	if (ctx->cap->features & SDE_SSPP_SCALER) {
		/* program decimation */
		decimation = ((1 << cfg->horz_decimation) - 1) << 8;
		decimation |= ((1 << cfg->vert_decimation) - 1);

		_sde_hw_sspp_setup_scaler(ctx, pe_ext);
	}

	/* rectangle register programming */
	SDE_REG_WRITE(c, SSPP_SRC_SIZE + idx, src_size);
	SDE_REG_WRITE(c, SSPP_SRC_XY + idx, src_xy);
	SDE_REG_WRITE(c, SSPP_OUT_SIZE + idx, dst_size);
	SDE_REG_WRITE(c, SSPP_OUT_XY + idx, dst_xy);

	SDE_REG_WRITE(c, SSPP_SRC_YSTRIDE0 + idx, ystride0);
	SDE_REG_WRITE(c, SSPP_SRC_YSTRIDE1 + idx, ystride1);
	SDE_REG_WRITE(c, SSPP_DECIMATION_CONFIG + idx, decimation);
}

static void sde_hw_sspp_setup_sourceaddress(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *cfg)
{
	int i;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	for (i = 0; i < ARRAY_SIZE(cfg->layout.plane_addr); i++)
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC0_ADDR + idx + i * 0x4,
			cfg->layout.plane_addr[i]);
}

static void sde_hw_sspp_setup_csc(struct sde_hw_pipe *ctx,
		struct sde_csc_cfg *data)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_CSC, &idx) || !data)
		return;

	sde_hw_csc_setup(&ctx->hw, idx, data);
}

static void sde_hw_sspp_setup_sharpening(struct sde_hw_pipe *ctx,
		struct sde_hw_sharp_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED2, &idx) || !cfg ||
			!test_bit(SDE_SSPP_SCALER_QSEED2, &ctx->cap->features))
		return;

	c = &ctx->hw;

	SDE_REG_WRITE(c, VIG_0_QSEED2_SHARP + idx, cfg->strength);
	SDE_REG_WRITE(c, VIG_0_QSEED2_SHARP + idx + 0x4, cfg->edge_thr);
	SDE_REG_WRITE(c, VIG_0_QSEED2_SHARP + idx + 0x8, cfg->smooth_thr);
	SDE_REG_WRITE(c, VIG_0_QSEED2_SHARP + idx + 0xC, cfg->noise_thr);
}

static void sde_hw_sspp_setup_solidfill(struct sde_hw_pipe *ctx, u32 color)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	SDE_REG_WRITE(&ctx->hw, SSPP_SRC_CONSTANT_COLOR + idx, color);
}

static void sde_hw_sspp_setup_histogram_v1(struct sde_hw_pipe *ctx,
		void *cfg)
{
}

static void sde_hw_sspp_setup_memcolor(struct sde_hw_pipe *ctx,
		u32 memcolortype, u8 en)
{
}

static void sde_hw_sspp_setup_igc(struct sde_hw_pipe *ctx)
{
}

void sde_sspp_setup_pa(struct sde_hw_pipe *c)
{
}

static void sde_hw_sspp_setup_danger_safe(struct sde_hw_pipe *ctx,
		u32 danger_lut, u32 safe_lut)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	SDE_REG_WRITE(&ctx->hw, SSPP_DANGER_LUT + idx, danger_lut);
	SDE_REG_WRITE(&ctx->hw, SSPP_SAFE_LUT + idx, safe_lut);
}

static void sde_hw_sspp_qseed2_coeff(void *ctx)
{
}

static void _setup_layer_ops(struct sde_hw_sspp_ops *ops,
		unsigned long features)
{
	if (test_bit(SDE_SSPP_SRC, &features)) {
		ops->setup_format = sde_hw_sspp_setup_format;
		ops->setup_rects = sde_hw_sspp_setup_rects;
		ops->setup_sourceaddress = sde_hw_sspp_setup_sourceaddress;
		ops->setup_solidfill = sde_hw_sspp_setup_solidfill;
		ops->setup_danger_safe = sde_hw_sspp_setup_danger_safe;
	}
	if (test_bit(SDE_SSPP_CSC, &features))
		ops->setup_csc = sde_hw_sspp_setup_csc;

	if (test_bit(SDE_SSPP_PA_V1, &features)) {
		ops->setup_sharpening = sde_hw_sspp_setup_sharpening;
		ops->setup_pa_memcolor = sde_hw_sspp_setup_memcolor;
	}
	if (test_bit(SDE_SSPP_HIST_V1, &features))
		ops->setup_histogram = sde_hw_sspp_setup_histogram_v1;

	if (test_bit(SDE_SSPP_IGC, &features))
		ops->setup_igc = sde_hw_sspp_setup_igc;
}

static struct sde_sspp_cfg *_sspp_offset(enum sde_sspp sspp,
		void __iomem *addr,
		struct sde_mdss_cfg *catalog,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	if ((sspp < SSPP_MAX) && catalog && addr && b) {
		for (i = 0; i < catalog->sspp_count; i++) {
			if (sspp == catalog->sspp[i].id) {
				b->base_off = addr;
				b->blk_off = catalog->sspp[i].base;
				b->hwversion = catalog->hwversion;
				b->log_mask = SDE_DBG_MASK_SSPP;
				return &catalog->sspp[i];
			}
		}
	}

	return ERR_PTR(-ENOMEM);
}

struct sde_hw_pipe *sde_hw_sspp_init(enum sde_sspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *catalog)
{
	struct sde_hw_pipe *ctx;
	struct sde_sspp_cfg *cfg;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	cfg = _sspp_offset(idx, addr, catalog, &ctx->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(ctx);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	ctx->idx = idx;
	ctx->cap = cfg;
	_setup_layer_ops(&ctx->ops, ctx->cap->features);
	ctx->highest_bank_bit = catalog->mdp[0].highest_bank_bit;

	/*
	 * Perform any default initialization for the sspp blocks
	 */
	if (test_bit(SDE_SSPP_SCALER_QSEED2, &cfg->features))
		sde_hw_sspp_qseed2_coeff(ctx);

	if (test_bit(SDE_MDP_PANIC_PER_PIPE, &catalog->mdp[0].features))
		sde_hw_sspp_setup_danger_safe(ctx,
				cfg->sblk->danger_lut,
				cfg->sblk->safe_lut);

	return ctx;
}

void sde_hw_sspp_destroy(struct sde_hw_pipe *ctx)
{
	kfree(ctx);
}

