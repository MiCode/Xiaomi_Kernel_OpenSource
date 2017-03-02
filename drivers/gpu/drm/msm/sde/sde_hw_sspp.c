/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include "sde_hw_color_processing.h"
#include "sde_dbg.h"

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

/* SSPP_MULTIRECT*/
#define SSPP_SRC_SIZE_REC1                 0x16C
#define SSPP_SRC_XY_REC1                   0x168
#define SSPP_OUT_SIZE_REC1                 0x160
#define SSPP_OUT_XY_REC1                   0x164
#define SSPP_SRC_FORMAT_REC1               0x174
#define SSPP_SRC_UNPACK_PATTERN_REC1       0x178
#define SSPP_SRC_OP_MODE_REC1              0x17C
#define SSPP_MULTIRECT_OPMODE              0x170
#define SSPP_SRC_CONSTANT_COLOR_REC1       0x180
#define SSPP_EXCL_REC_SIZE_REC1            0x184
#define SSPP_EXCL_REC_XY_REC1              0x188

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
#define SSPP_EXCL_REC_CTL                  0x40
#define SSPP_FETCH_CONFIG                  0x048
#define SSPP_DANGER_LUT                    0x60
#define SSPP_SAFE_LUT                      0x64
#define SSPP_CREQ_LUT                      0x68
#define SSPP_QOS_CTRL                      0x6C
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
#define SSPP_EXCL_REC_SIZE                 0x1B4
#define SSPP_EXCL_REC_XY                   0x1B8
#define SSPP_VIG_OP_MODE                   0x0
#define SSPP_VIG_CSC_10_OP_MODE            0x0

/* SSPP_QOS_CTRL */
#define SSPP_QOS_CTRL_VBLANK_EN            BIT(16)
#define SSPP_QOS_CTRL_DANGER_SAFE_EN       BIT(0)
#define SSPP_QOS_CTRL_DANGER_VBLANK_MASK   0x3
#define SSPP_QOS_CTRL_DANGER_VBLANK_OFF    4
#define SSPP_QOS_CTRL_CREQ_VBLANK_MASK     0x3
#define SSPP_QOS_CTRL_CREQ_VBLANK_OFF      20

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

/* SDE_SSPP_SCALER_QSEED3 */
#define QSEED3_HW_VERSION                  0x00
#define QSEED3_OP_MODE                     0x04
#define QSEED3_RGB2Y_COEFF                 0x08
#define QSEED3_PHASE_INIT                  0x0C
#define QSEED3_PHASE_STEP_Y_H              0x10
#define QSEED3_PHASE_STEP_Y_V              0x14
#define QSEED3_PHASE_STEP_UV_H             0x18
#define QSEED3_PHASE_STEP_UV_V             0x1C
#define QSEED3_PRELOAD                     0x20
#define QSEED3_DE_SHARPEN                  0x24
#define QSEED3_DE_SHARPEN_CTL              0x28
#define QSEED3_DE_SHAPE_CTL                0x2C
#define QSEED3_DE_THRESHOLD                0x30
#define QSEED3_DE_ADJUST_DATA_0            0x34
#define QSEED3_DE_ADJUST_DATA_1            0x38
#define QSEED3_DE_ADJUST_DATA_2            0x3C
#define QSEED3_SRC_SIZE_Y_RGB_A            0x40
#define QSEED3_SRC_SIZE_UV                 0x44
#define QSEED3_DST_SIZE                    0x48
#define QSEED3_COEF_LUT_CTRL               0x4C
#define QSEED3_COEF_LUT_SWAP_BIT           0
#define QSEED3_COEF_LUT_DIR_BIT            1
#define QSEED3_COEF_LUT_Y_CIR_BIT          2
#define QSEED3_COEF_LUT_UV_CIR_BIT         3
#define QSEED3_COEF_LUT_Y_SEP_BIT          4
#define QSEED3_COEF_LUT_UV_SEP_BIT         5
#define QSEED3_BUFFER_CTRL                 0x50
#define QSEED3_CLK_CTRL0                   0x54
#define QSEED3_CLK_CTRL1                   0x58
#define QSEED3_CLK_STATUS                  0x5C
#define QSEED3_MISR_CTRL                   0x70
#define QSEED3_MISR_SIGNATURE_0            0x74
#define QSEED3_MISR_SIGNATURE_1            0x78
#define QSEED3_PHASE_INIT_Y_H              0x90
#define QSEED3_PHASE_INIT_Y_V              0x94
#define QSEED3_PHASE_INIT_UV_H             0x98
#define QSEED3_PHASE_INIT_UV_V             0x9C
#define QSEED3_COEF_LUT                    0x100
#define QSEED3_FILTERS                     5
#define QSEED3_LUT_REGIONS                 4
#define QSEED3_CIRCULAR_LUTS               9
#define QSEED3_SEPARABLE_LUTS              10
#define QSEED3_LUT_SIZE                    60
#define QSEED3_ENABLE                      2
#define QSEED3_DIR_LUT_SIZE                (200 * sizeof(u32))
#define QSEED3_CIR_LUT_SIZE \
	(QSEED3_LUT_SIZE * QSEED3_CIRCULAR_LUTS * sizeof(u32))
#define QSEED3_SEP_LUT_SIZE \
	(QSEED3_LUT_SIZE * QSEED3_SEPARABLE_LUTS * sizeof(u32))

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

/*
 * Definitions for CSC 10 op modes
 */
#define VIG_CSC_10_SRC_DATAFMT BIT(1)
#define VIG_CSC_10_EN          BIT(0)
#define CSC_10BIT_OFFSET       4

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
	case SDE_SSPP_CSC_10BIT:
		*idx = sblk->csc_blk.base;
		break;
	case SDE_SSPP_HSIC:
		*idx = sblk->hsic_blk.base;
		break;
	case SDE_SSPP_PCC:
		*idx = sblk->pcc_blk.base;
		break;
	case SDE_SSPP_MEMCOLOR:
		*idx = sblk->memcolor_blk.base;
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static void sde_hw_sspp_setup_multirect(struct sde_hw_pipe *ctx,
		enum sde_sspp_multirect_index index,
		enum sde_sspp_multirect_mode mode)
{
	u32 mode_mask;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	if (index == SDE_SSPP_RECT_SOLO) {
		/**
		 * if rect index is RECT_SOLO, we cannot expect a
		 * virtual plane sharing the same SSPP id. So we go
		 * and disable multirect
		 */
		mode_mask = 0;
	} else {
		mode_mask = SDE_REG_READ(&ctx->hw, SSPP_MULTIRECT_OPMODE + idx);
		mode_mask |= index;
		mode_mask |= (mode == SDE_SSPP_MULTIRECT_TIME_MX) ? 0x4 : 0x0;
	}

	SDE_REG_WRITE(&ctx->hw, SSPP_MULTIRECT_OPMODE + idx, mode_mask);
}

static void _sspp_setup_opmode(struct sde_hw_pipe *ctx,
		u32 mask, u8 en)
{
	u32 idx;
	u32 opmode;

	if (!test_bit(SDE_SSPP_SCALER_QSEED2, &ctx->cap->features) ||
		_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED2, &idx) ||
		!test_bit(SDE_SSPP_CSC, &ctx->cap->features))
		return;

	opmode = SDE_REG_READ(&ctx->hw, SSPP_VIG_OP_MODE + idx);

	if (en)
		opmode |= mask;
	else
		opmode &= ~mask;

	SDE_REG_WRITE(&ctx->hw, SSPP_VIG_OP_MODE + idx, opmode);
}

static void _sspp_setup_csc10_opmode(struct sde_hw_pipe *ctx,
		u32 mask, u8 en)
{
	u32 idx;
	u32 opmode;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_CSC_10BIT, &idx))
		return;

	opmode = SDE_REG_READ(&ctx->hw, SSPP_VIG_CSC_10_OP_MODE + idx);
	if (en)
		opmode |= mask;
	else
		opmode &= ~mask;

	SDE_REG_WRITE(&ctx->hw, SSPP_VIG_CSC_10_OP_MODE + idx, opmode);
}

/**
 * Setup source pixel format, flip,
 */
static void sde_hw_sspp_setup_format(struct sde_hw_pipe *ctx,
		const struct sde_format *fmt, u32 flags,
		enum sde_sspp_multirect_index rect_mode)
{
	struct sde_hw_blk_reg_map *c;
	u32 chroma_samp, unpack, src_format;
	u32 secure = 0;
	u32 opmode = 0;
	u32 op_mode_off, unpack_pat_off, format_off;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !fmt)
		return;

	if (rect_mode == SDE_SSPP_RECT_SOLO || rect_mode == SDE_SSPP_RECT_0) {
		op_mode_off = SSPP_SRC_OP_MODE;
		unpack_pat_off = SSPP_SRC_UNPACK_PATTERN;
		format_off = SSPP_SRC_FORMAT;
	} else {
		op_mode_off = SSPP_SRC_OP_MODE_REC1;
		unpack_pat_off = SSPP_SRC_UNPACK_PATTERN_REC1;
		format_off = SSPP_SRC_FORMAT_REC1;
	}

	c = &ctx->hw;
	opmode = SDE_REG_READ(c, op_mode_off + idx);
	opmode &= ~(MDSS_MDP_OP_FLIP_LR | MDSS_MDP_OP_FLIP_UD |
			MDSS_MDP_OP_BWC_EN | MDSS_MDP_OP_PE_OVERRIDE);

	if (flags & SDE_SSPP_SECURE_OVERLAY_SESSION) {
		secure = SDE_REG_READ(c, SSPP_SRC_ADDR_SW_STATUS + idx);

		if (rect_mode == SDE_SSPP_RECT_SOLO)
			secure |= 0xF;
		else if (rect_mode == SDE_SSPP_RECT_0)
			secure |= 0x5;
		else if (rect_mode == SDE_SSPP_RECT_1)
			secure |= 0xA;
	}

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

	if (SDE_FORMAT_IS_DX(fmt))
		src_format |= BIT(14);

	/* update scaler opmode, if appropriate */
	if (test_bit(SDE_SSPP_CSC, &ctx->cap->features))
		_sspp_setup_opmode(ctx, VIG_OP_CSC_EN | VIG_OP_CSC_SRC_DATAFMT,
			SDE_FORMAT_IS_YUV(fmt));
	else if (test_bit(SDE_SSPP_CSC_10BIT, &ctx->cap->features))
		_sspp_setup_csc10_opmode(ctx,
			VIG_CSC_10_EN | VIG_CSC_10_SRC_DATAFMT,
			SDE_FORMAT_IS_YUV(fmt));

	SDE_REG_WRITE(c, format_off + idx, src_format);
	SDE_REG_WRITE(c, unpack_pat_off + idx, unpack);
	SDE_REG_WRITE(c, op_mode_off + idx, opmode);
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
		struct sde_hw_pipe_cfg *sspp,
		struct sde_hw_pixel_ext *pe,
		void *scaler_cfg)
{
	struct sde_hw_blk_reg_map *c;
	int config_h = 0x0;
	int config_v = 0x0;
	u32 idx;

	(void)sspp;
	(void)scaler_cfg;
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

static void _sde_hw_sspp_setup_scaler3_lut(struct sde_hw_pipe *ctx,
		struct sde_hw_scaler3_cfg *scaler3_cfg)
{
	u32 idx;
	int i, j, filter;
	int config_lut = 0x0;
	unsigned long lut_flags;
	u32 lut_addr, lut_offset, lut_len;
	u32 *lut[QSEED3_FILTERS] = {NULL, NULL, NULL, NULL, NULL};
	static const uint32_t offset[QSEED3_FILTERS][QSEED3_LUT_REGIONS][2] = {
		{{18, 0x000}, {12, 0x120}, {12, 0x1E0}, {8, 0x2A0} },
		{{6, 0x320}, {3, 0x3E0}, {3, 0x440}, {3, 0x4A0} },
		{{6, 0x500}, {3, 0x5c0}, {3, 0x620}, {3, 0x680} },
		{{6, 0x380}, {3, 0x410}, {3, 0x470}, {3, 0x4d0} },
		{{6, 0x560}, {3, 0x5f0}, {3, 0x650}, {3, 0x6b0} },
	};

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED3, &idx) ||
		!scaler3_cfg)
		return;

	lut_flags = (unsigned long) scaler3_cfg->lut_flag;
	if (test_bit(QSEED3_COEF_LUT_DIR_BIT, &lut_flags) &&
		(scaler3_cfg->dir_len == QSEED3_DIR_LUT_SIZE)) {
		lut[0] = scaler3_cfg->dir_lut;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_Y_CIR_BIT, &lut_flags) &&
		(scaler3_cfg->y_rgb_cir_lut_idx < QSEED3_CIRCULAR_LUTS) &&
		(scaler3_cfg->cir_len == QSEED3_CIR_LUT_SIZE)) {
		lut[1] = scaler3_cfg->cir_lut +
			scaler3_cfg->y_rgb_cir_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_UV_CIR_BIT, &lut_flags) &&
		(scaler3_cfg->uv_cir_lut_idx < QSEED3_CIRCULAR_LUTS) &&
		(scaler3_cfg->cir_len == QSEED3_CIR_LUT_SIZE)) {
		lut[2] = scaler3_cfg->cir_lut +
			scaler3_cfg->uv_cir_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_Y_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->y_rgb_sep_lut_idx < QSEED3_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3_SEP_LUT_SIZE)) {
		lut[3] = scaler3_cfg->sep_lut +
			scaler3_cfg->y_rgb_sep_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_UV_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->uv_sep_lut_idx < QSEED3_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3_SEP_LUT_SIZE)) {
		lut[4] = scaler3_cfg->sep_lut +
			scaler3_cfg->uv_sep_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}

	if (config_lut) {
		for (filter = 0; filter < QSEED3_FILTERS; filter++) {
			if (!lut[filter])
				continue;
			lut_offset = 0;
			for (i = 0; i < QSEED3_LUT_REGIONS; i++) {
				lut_addr = QSEED3_COEF_LUT + idx
					+ offset[filter][i][1];
				lut_len = offset[filter][i][0] << 2;
				for (j = 0; j < lut_len; j++) {
					SDE_REG_WRITE(&ctx->hw,
						lut_addr,
						(lut[filter])[lut_offset++]);
					lut_addr += 4;
				}
			}
		}
	}

	if (test_bit(QSEED3_COEF_LUT_SWAP_BIT, &lut_flags))
		SDE_REG_WRITE(&ctx->hw, QSEED3_COEF_LUT_CTRL + idx, BIT(0));

}

static void _sde_hw_sspp_setup_scaler3_de(struct sde_hw_pipe *ctx,
		struct sde_hw_scaler3_de_cfg *de_cfg)
{
	u32 idx;
	u32 sharp_lvl, sharp_ctl, shape_ctl, de_thr;
	u32 adjust_a, adjust_b, adjust_c;
	struct sde_hw_blk_reg_map *hw;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED3, &idx) || !de_cfg)
		return;

	if (!de_cfg->enable)
		return;

	hw = &ctx->hw;
	sharp_lvl = (de_cfg->sharpen_level1 & 0x1FF) |
		((de_cfg->sharpen_level2 & 0x1FF) << 16);

	sharp_ctl = ((de_cfg->limit & 0xF) << 9) |
		((de_cfg->prec_shift & 0x7) << 13) |
		((de_cfg->clip & 0x7) << 16);

	shape_ctl = (de_cfg->thr_quiet & 0xFF) |
		((de_cfg->thr_dieout & 0x3FF) << 16);

	de_thr = (de_cfg->thr_low & 0x3FF) |
		((de_cfg->thr_high & 0x3FF) << 16);

	adjust_a = (de_cfg->adjust_a[0] & 0x3FF) |
		((de_cfg->adjust_a[1] & 0x3FF) << 10) |
		((de_cfg->adjust_a[2] & 0x3FF) << 20);

	adjust_b = (de_cfg->adjust_b[0] & 0x3FF) |
		((de_cfg->adjust_b[1] & 0x3FF) << 10) |
		((de_cfg->adjust_b[2] & 0x3FF) << 20);

	adjust_c = (de_cfg->adjust_c[0] & 0x3FF) |
		((de_cfg->adjust_c[1] & 0x3FF) << 10) |
		((de_cfg->adjust_c[2] & 0x3FF) << 20);

	SDE_REG_WRITE(hw, QSEED3_DE_SHARPEN + idx, sharp_lvl);
	SDE_REG_WRITE(hw, QSEED3_DE_SHARPEN_CTL + idx, sharp_ctl);
	SDE_REG_WRITE(hw, QSEED3_DE_SHAPE_CTL + idx, shape_ctl);
	SDE_REG_WRITE(hw, QSEED3_DE_THRESHOLD + idx, de_thr);
	SDE_REG_WRITE(hw, QSEED3_DE_ADJUST_DATA_0 + idx, adjust_a);
	SDE_REG_WRITE(hw, QSEED3_DE_ADJUST_DATA_1 + idx, adjust_b);
	SDE_REG_WRITE(hw, QSEED3_DE_ADJUST_DATA_2 + idx, adjust_c);

}

static void _sde_hw_sspp_setup_scaler3(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *sspp,
		struct sde_hw_pixel_ext *pe,
		void *scaler_cfg)
{
	u32 idx;
	u32 op_mode = 0;
	u32 phase_init, preload, src_y_rgb, src_uv, dst;
	struct sde_hw_scaler3_cfg *scaler3_cfg = scaler_cfg;

	(void)pe;
	if (_sspp_subblk_offset(ctx, SDE_SSPP_SCALER_QSEED3, &idx) || !sspp
		|| !scaler3_cfg || !ctx || !ctx->cap || !ctx->cap->sblk)
		return;

	if (!scaler3_cfg->enable)
		goto end;

	op_mode |= BIT(0);
	op_mode |= (scaler3_cfg->y_rgb_filter_cfg & 0x3) << 16;

	if (SDE_FORMAT_IS_YUV(sspp->layout.format)) {
		op_mode |= BIT(12);
		op_mode |= (scaler3_cfg->uv_filter_cfg & 0x3) << 24;
	}

	op_mode |= (scaler3_cfg->blend_cfg & 1) << 31;
	op_mode |= (scaler3_cfg->dir_en) ? BIT(4) : 0;

	preload =
		((scaler3_cfg->preload_x[0] & 0x7F) << 0) |
		((scaler3_cfg->preload_y[0] & 0x7F) << 8) |
		((scaler3_cfg->preload_x[1] & 0x7F) << 16) |
		((scaler3_cfg->preload_y[1] & 0x7F) << 24);

	src_y_rgb = (scaler3_cfg->src_width[0] & 0x1FFFF) |
		((scaler3_cfg->src_height[0] & 0x1FFFF) << 16);

	src_uv = (scaler3_cfg->src_width[1] & 0x1FFFF) |
		((scaler3_cfg->src_height[1] & 0x1FFFF) << 16);

	dst = (scaler3_cfg->dst_width & 0x1FFFF) |
		((scaler3_cfg->dst_height & 0x1FFFF) << 16);

	if (scaler3_cfg->de.enable) {
		_sde_hw_sspp_setup_scaler3_de(ctx, &scaler3_cfg->de);
		op_mode |= BIT(8);
	}

	if (scaler3_cfg->lut_flag)
		_sde_hw_sspp_setup_scaler3_lut(ctx, scaler3_cfg);

	if (ctx->cap->sblk->scaler_blk.version == 0x1002) {
		phase_init =
			((scaler3_cfg->init_phase_x[0] & 0x3F) << 0) |
			((scaler3_cfg->init_phase_y[0] & 0x3F) << 8) |
			((scaler3_cfg->init_phase_x[1] & 0x3F) << 16) |
			((scaler3_cfg->init_phase_y[1] & 0x3F) << 24);
		SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_INIT + idx, phase_init);
	} else {
		SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_INIT_Y_H + idx,
			scaler3_cfg->init_phase_x[0] & 0x1FFFFF);
		SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_INIT_Y_V + idx,
			scaler3_cfg->init_phase_y[0] & 0x1FFFFF);
		SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_INIT_UV_H + idx,
			scaler3_cfg->init_phase_x[1] & 0x1FFFFF);
		SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_INIT_UV_V + idx,
			scaler3_cfg->init_phase_y[1] & 0x1FFFFF);
	}

	SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_STEP_Y_H + idx,
		scaler3_cfg->phase_step_x[0] & 0xFFFFFF);

	SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_STEP_Y_V + idx,
		scaler3_cfg->phase_step_y[0] & 0xFFFFFF);

	SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_STEP_UV_H + idx,
		scaler3_cfg->phase_step_x[1] & 0xFFFFFF);

	SDE_REG_WRITE(&ctx->hw, QSEED3_PHASE_STEP_UV_V + idx,
		scaler3_cfg->phase_step_y[1] & 0xFFFFFF);

	SDE_REG_WRITE(&ctx->hw, QSEED3_PRELOAD + idx, preload);

	SDE_REG_WRITE(&ctx->hw, QSEED3_SRC_SIZE_Y_RGB_A + idx, src_y_rgb);

	SDE_REG_WRITE(&ctx->hw, QSEED3_SRC_SIZE_UV + idx, src_uv);

	SDE_REG_WRITE(&ctx->hw, QSEED3_DST_SIZE + idx, dst);

end:
	if (!SDE_FORMAT_IS_DX(sspp->layout.format))
		op_mode |= BIT(14);

	if (sspp->layout.format->alpha_enable) {
		op_mode |= BIT(10);
		if (ctx->cap->sblk->scaler_blk.version == 0x1002)
			op_mode |= (scaler3_cfg->alpha_filter_cfg & 0x1) << 30;
		else
			op_mode |= (scaler3_cfg->alpha_filter_cfg & 0x3) << 29;
	}
	SDE_REG_WRITE(&ctx->hw, QSEED3_OP_MODE + idx, op_mode);
}

/**
 * sde_hw_sspp_setup_rects()
 */
static void sde_hw_sspp_setup_rects(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *cfg,
		enum sde_sspp_multirect_index rect_index)
{
	struct sde_hw_blk_reg_map *c;
	u32 src_size, src_xy, dst_size, dst_xy, ystride0, ystride1;
	u32 src_size_off, src_xy_off, out_size_off, out_xy_off;
	u32 decimation = 0;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !cfg)
		return;

	c = &ctx->hw;

	if (rect_index == SDE_SSPP_RECT_SOLO || rect_index == SDE_SSPP_RECT_0) {
		src_size_off = SSPP_SRC_SIZE;
		src_xy_off = SSPP_SRC_XY;
		out_size_off = SSPP_OUT_SIZE;
		out_xy_off = SSPP_OUT_XY;
	} else {
		src_size_off = SSPP_SRC_SIZE_REC1;
		src_xy_off = SSPP_SRC_XY_REC1;
		out_size_off = SSPP_OUT_SIZE_REC1;
		out_xy_off = SSPP_OUT_XY_REC1;
	}


	/* src and dest rect programming */
	src_xy = (cfg->src_rect.y << 16) | (cfg->src_rect.x);
	src_size = (cfg->src_rect.h << 16) | (cfg->src_rect.w);
	dst_xy = (cfg->dst_rect.y << 16) | (cfg->dst_rect.x);
	dst_size = (cfg->dst_rect.h << 16) | (cfg->dst_rect.w);

	if (rect_index == SDE_SSPP_RECT_SOLO) {
		ystride0 = (cfg->layout.plane_pitch[0]) |
			(cfg->layout.plane_pitch[1] << 16);
		ystride1 = (cfg->layout.plane_pitch[2]) |
			(cfg->layout.plane_pitch[3] << 16);
	} else {
		ystride0 = SDE_REG_READ(c, SSPP_SRC_YSTRIDE0 + idx);
		ystride1 = SDE_REG_READ(c, SSPP_SRC_YSTRIDE1 + idx);

		if (rect_index == SDE_SSPP_RECT_0) {
			ystride0 |= cfg->layout.plane_pitch[0];
			ystride1 |= cfg->layout.plane_pitch[2];
		}  else {
			ystride0 |= cfg->layout.plane_pitch[0] << 16;
			ystride1 |= cfg->layout.plane_pitch[2] << 16;
		}
	}

	/* program scaler, phase registers, if pipes supporting scaling */
	if (ctx->cap->features & SDE_SSPP_SCALER) {
		/* program decimation */
		decimation = ((1 << cfg->horz_decimation) - 1) << 8;
		decimation |= ((1 << cfg->vert_decimation) - 1);
	}

	/* rectangle register programming */
	SDE_REG_WRITE(c, src_size_off + idx, src_size);
	SDE_REG_WRITE(c, src_xy_off + idx, src_xy);
	SDE_REG_WRITE(c, out_size_off + idx, dst_size);
	SDE_REG_WRITE(c, out_xy_off + idx, dst_xy);

	SDE_REG_WRITE(c, SSPP_SRC_YSTRIDE0 + idx, ystride0);
	SDE_REG_WRITE(c, SSPP_SRC_YSTRIDE1 + idx, ystride1);
	SDE_REG_WRITE(c, SSPP_DECIMATION_CONFIG + idx, decimation);
}

/**
 * _sde_hw_sspp_setup_excl_rect() - set exclusion rect configs
 * @ctx: Pointer to pipe context
 * @excl_rect: Exclusion rect configs
 */
static void _sde_hw_sspp_setup_excl_rect(struct sde_hw_pipe *ctx,
		struct sde_rect *excl_rect,
		enum sde_sspp_multirect_index rect_index)
{
	struct sde_hw_blk_reg_map *c;
	u32 size, xy;
	u32 idx;
	u32 reg_xy, reg_size;
	u32 excl_ctrl, enable_bit;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx) || !excl_rect)
		return;

	if (rect_index == SDE_SSPP_RECT_0 || rect_index == SDE_SSPP_RECT_SOLO) {
		reg_xy = SSPP_EXCL_REC_XY;
		reg_size = SSPP_EXCL_REC_SIZE;
		enable_bit = BIT(0);
	} else {
		reg_xy = SSPP_EXCL_REC_XY_REC1;
		reg_size = SSPP_EXCL_REC_SIZE_REC1;
		enable_bit = BIT(1);
	}

	c = &ctx->hw;

	xy = (excl_rect->y << 16) | (excl_rect->x);
	size = (excl_rect->h << 16) | (excl_rect->w);

	excl_ctrl = SDE_REG_READ(c, SSPP_EXCL_REC_CTL + idx);
	if (!size) {
		SDE_REG_WRITE(c, SSPP_EXCL_REC_CTL + idx,
				excl_ctrl & ~enable_bit);
	} else {
		SDE_REG_WRITE(c, SSPP_EXCL_REC_CTL + idx,
				excl_ctrl | enable_bit);
		SDE_REG_WRITE(c, reg_size + idx, size);
		SDE_REG_WRITE(c, reg_xy + idx, xy);
	}
}

static void sde_hw_sspp_setup_sourceaddress(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_cfg *cfg,
		enum sde_sspp_multirect_index rect_mode)
{
	int i;
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	if (rect_mode == SDE_SSPP_RECT_SOLO) {
		for (i = 0; i < ARRAY_SIZE(cfg->layout.plane_addr); i++)
			SDE_REG_WRITE(&ctx->hw, SSPP_SRC0_ADDR + idx + i * 0x4,
					cfg->layout.plane_addr[i]);
	} else if (rect_mode == SDE_SSPP_RECT_0) {
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC0_ADDR + idx,
				cfg->layout.plane_addr[0]);
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC2_ADDR + idx,
				cfg->layout.plane_addr[2]);
	} else {
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC1_ADDR + idx,
				cfg->layout.plane_addr[0]);
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC3_ADDR + idx,
				cfg->layout.plane_addr[2]);
	}
}

static void sde_hw_sspp_setup_csc(struct sde_hw_pipe *ctx,
		struct sde_csc_cfg *data)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_CSC, &idx) || !data)
		return;

	if (test_bit(SDE_SSPP_CSC_10BIT, &ctx->cap->features))
		idx += CSC_10BIT_OFFSET;

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

static void sde_hw_sspp_setup_solidfill(struct sde_hw_pipe *ctx, u32 color, enum
		sde_sspp_multirect_index rect_index)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	if (rect_index == SDE_SSPP_RECT_SOLO || rect_index == SDE_SSPP_RECT_0)
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC_CONSTANT_COLOR + idx, color);
	else
		SDE_REG_WRITE(&ctx->hw, SSPP_SRC_CONSTANT_COLOR_REC1 + idx,
				color);
}

static void sde_hw_sspp_setup_danger_safe_lut(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_qos_cfg *cfg)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	SDE_REG_WRITE(&ctx->hw, SSPP_DANGER_LUT + idx, cfg->danger_lut);
	SDE_REG_WRITE(&ctx->hw, SSPP_SAFE_LUT + idx, cfg->safe_lut);
}

static void sde_hw_sspp_setup_creq_lut(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_qos_cfg *cfg)
{
	u32 idx;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	SDE_REG_WRITE(&ctx->hw, SSPP_CREQ_LUT + idx, cfg->creq_lut);
}

static void sde_hw_sspp_setup_qos_ctrl(struct sde_hw_pipe *ctx,
		struct sde_hw_pipe_qos_cfg *cfg)
{
	u32 idx;
	u32 qos_ctrl = 0;

	if (_sspp_subblk_offset(ctx, SDE_SSPP_SRC, &idx))
		return;

	if (cfg->vblank_en) {
		qos_ctrl |= ((cfg->creq_vblank &
				SSPP_QOS_CTRL_CREQ_VBLANK_MASK) <<
				SSPP_QOS_CTRL_CREQ_VBLANK_OFF);
		qos_ctrl |= ((cfg->danger_vblank &
				SSPP_QOS_CTRL_DANGER_VBLANK_MASK) <<
				SSPP_QOS_CTRL_DANGER_VBLANK_OFF);
		qos_ctrl |= SSPP_QOS_CTRL_VBLANK_EN;
	}

	if (cfg->danger_safe_en)
		qos_ctrl |= SSPP_QOS_CTRL_DANGER_SAFE_EN;

	SDE_REG_WRITE(&ctx->hw, SSPP_QOS_CTRL + idx, qos_ctrl);
}

static void _setup_layer_ops(struct sde_hw_pipe *c,
		unsigned long features)
{
	if (test_bit(SDE_SSPP_SRC, &features)) {
		c->ops.setup_format = sde_hw_sspp_setup_format;
		c->ops.setup_rects = sde_hw_sspp_setup_rects;
		c->ops.setup_sourceaddress = sde_hw_sspp_setup_sourceaddress;
		c->ops.setup_solidfill = sde_hw_sspp_setup_solidfill;
		c->ops.setup_pe = sde_hw_sspp_setup_pe_config;
	}

	if (test_bit(SDE_SSPP_EXCL_RECT, &features))
		c->ops.setup_excl_rect = _sde_hw_sspp_setup_excl_rect;

	if (test_bit(SDE_SSPP_QOS, &features)) {
		c->ops.setup_danger_safe_lut =
			sde_hw_sspp_setup_danger_safe_lut;
		c->ops.setup_creq_lut = sde_hw_sspp_setup_creq_lut;
		c->ops.setup_qos_ctrl = sde_hw_sspp_setup_qos_ctrl;
	}

	if (test_bit(SDE_SSPP_CSC, &features) ||
		test_bit(SDE_SSPP_CSC_10BIT, &features))
		c->ops.setup_csc = sde_hw_sspp_setup_csc;

	if (test_bit(SDE_SSPP_SCALER_QSEED2, &features)) {
		c->ops.setup_sharpening = sde_hw_sspp_setup_sharpening;
		c->ops.setup_scaler = _sde_hw_sspp_setup_scaler;
	}

	if (sde_hw_sspp_multirect_enabled(c->cap))
		c->ops.setup_multirect = sde_hw_sspp_setup_multirect;

	if (test_bit(SDE_SSPP_SCALER_QSEED3, &features))
		c->ops.setup_scaler = _sde_hw_sspp_setup_scaler3;

	if (test_bit(SDE_SSPP_HSIC, &features)) {
		/* TODO: add version based assignment here as inline or macro */
		if (c->cap->sblk->hsic_blk.version ==
			(SDE_COLOR_PROCESS_VER(0x1, 0x7))) {
			c->ops.setup_pa_hue = sde_setup_pipe_pa_hue_v1_7;
			c->ops.setup_pa_sat = sde_setup_pipe_pa_sat_v1_7;
			c->ops.setup_pa_val = sde_setup_pipe_pa_val_v1_7;
			c->ops.setup_pa_cont = sde_setup_pipe_pa_cont_v1_7;
		}
	}

	if (test_bit(SDE_SSPP_MEMCOLOR, &features)) {
		if (c->cap->sblk->memcolor_blk.version ==
			(SDE_COLOR_PROCESS_VER(0x1, 0x7)))
			c->ops.setup_pa_memcolor =
				sde_setup_pipe_pa_memcol_v1_7;
	}
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
				b->length = catalog->sspp[i].len;
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
	struct sde_hw_pipe *hw_pipe;
	struct sde_sspp_cfg *cfg;

	hw_pipe = kzalloc(sizeof(*hw_pipe), GFP_KERNEL);
	if (!hw_pipe)
		return ERR_PTR(-ENOMEM);

	cfg = _sspp_offset(idx, addr, catalog, &hw_pipe->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(hw_pipe);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	hw_pipe->idx = idx;
	hw_pipe->cap = cfg;
	_setup_layer_ops(hw_pipe, hw_pipe->cap->features);
	hw_pipe->highest_bank_bit = catalog->mdp[0].highest_bank_bit;

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name,
			hw_pipe->hw.blk_off,
			hw_pipe->hw.blk_off + hw_pipe->hw.length,
			hw_pipe->hw.xin_id);

	if (cfg->sblk->scaler_blk.len)
		sde_dbg_reg_register_dump_range(SDE_DBG_NAME,
			cfg->sblk->scaler_blk.name,
			cfg->sblk->scaler_blk.base,
			cfg->sblk->scaler_blk.base + cfg->sblk->scaler_blk.len,
			hw_pipe->hw.xin_id);

	return hw_pipe;
}

void sde_hw_sspp_destroy(struct sde_hw_pipe *ctx)
{
	kfree(ctx);
}

