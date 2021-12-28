// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dsc.h"
#include "sde_hw_pingpong.h"
#include "sde_dbg.h"
#include "sde_kms.h"


#define DSC_CMN_MAIN_CNF           0x00

/* SDE_DSC_ENC regsiter offsets */
#define ENC_DF_CTRL                0x00
#define ENC_GENERAL_STATUS         0x04
#define ENC_HSLICE_STATUS          0x08
#define ENC_OUT_STATUS             0x0C
#define ENC_INT_STAT               0x10
#define ENC_INT_CLR                0x14
#define ENC_INT_MASK               0x18
#define DSC_MAIN_CONF              0x30
#define DSC_PICTURE_SIZE           0x34
#define DSC_SLICE_SIZE             0x38
#define DSC_MISC_SIZE              0x3C
#define DSC_HRD_DELAYS             0x40
#define DSC_RC_SCALE               0x44
#define DSC_RC_SCALE_INC_DEC       0x48
#define DSC_RC_OFFSETS_1           0x4C
#define DSC_RC_OFFSETS_2           0x50
#define DSC_RC_OFFSETS_3           0x54
#define DSC_RC_OFFSETS_4           0x58
#define DSC_FLATNESS_QP            0x5C
#define DSC_RC_MODEL_SIZE          0x60
#define DSC_RC_CONFIG              0x64
#define DSC_RC_BUF_THRESH_0        0x68
#define DSC_RC_BUF_THRESH_1        0x6C
#define DSC_RC_BUF_THRESH_2        0x70
#define DSC_RC_BUF_THRESH_3        0x74
#define DSC_RC_MIN_QP_0            0x78
#define DSC_RC_MIN_QP_1            0x7C
#define DSC_RC_MIN_QP_2            0x80
#define DSC_RC_MAX_QP_0            0x84
#define DSC_RC_MAX_QP_1            0x88
#define DSC_RC_MAX_QP_2             0x8C
#define DSC_RC_RANGE_BPG_OFFSETS_0  0x90
#define DSC_RC_RANGE_BPG_OFFSETS_1  0x94
#define DSC_RC_RANGE_BPG_OFFSETS_2  0x98

/* SDE_DSC_CTL regsiter offsets */
#define DSC_CTL                    0x00
#define DSC_CFG                    0x04
#define DSC_DATA_IN_SWAP           0x08
#define DSC_CLK_CTRL               0x0C


static int _dsc_calc_ob_max_addr(struct sde_hw_dsc *hw_dsc, int num_ss)
{
	enum sde_dsc idx;

	idx = hw_dsc->idx;

	if (!(hw_dsc->caps->features & BIT(SDE_DSC_NATIVE_422_EN))) {
		if (num_ss == 1)
			return 2399;
		else if (num_ss == 2)
			return 1199;
	} else {
		if (num_ss == 1)
			return 1199;
		else if (num_ss == 2)
			return 599;
	}
	return 0;
}

static inline int _dsc_subblk_offset(struct sde_hw_dsc *hw_dsc, int s_id,
		u32 *idx)
{
	const struct sde_dsc_sub_blks *sblk;

	if (!hw_dsc)
		return -EINVAL;

	sblk = hw_dsc->caps->sblk;

	switch (s_id) {
	case SDE_DSC_ENC:
		*idx = sblk->enc.base;
		break;
	case SDE_DSC_CTL:
		*idx = sblk->ctl.base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void sde_hw_dsc_disable(struct sde_hw_dsc *hw_dsc)
{
	struct sde_hw_blk_reg_map *dsc_c;
	u32 idx;

	if (!hw_dsc)
		return;

	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_CTL, &idx))
		return;

	dsc_c = &hw_dsc->hw;
	SDE_REG_WRITE(dsc_c, DSC_CFG + idx, 0);

	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_ENC, &idx))
		return;

	SDE_REG_WRITE(dsc_c, ENC_DF_CTRL + idx, 0);
	SDE_REG_WRITE(dsc_c, DSC_MAIN_CONF + idx, 0);
}

static void sde_hw_dsc_config(struct sde_hw_dsc *hw_dsc,
		struct msm_display_dsc_info *dsc, u32 mode,
		bool ich_reset_override)
{
	struct sde_hw_blk_reg_map *dsc_c;
	u32 idx;
	u32 data = 0;
	u32 bpp;

	if (!hw_dsc || !dsc)
		return;

	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_ENC, &idx))
		return;

	dsc_c = &hw_dsc->hw;

	if (mode & DSC_MODE_SPLIT_PANEL)
		data |= BIT(0);

	if (mode & DSC_MODE_MULTIPLEX)
		data |= BIT(1);

	data |= (dsc->num_active_ss_per_enc & 0x3) << 7;
	SDE_REG_WRITE(dsc_c, DSC_CMN_MAIN_CNF, data);

	data = (dsc->initial_lines & 0xff);
	data |= ((mode & DSC_MODE_VIDEO) ? 1 : 0) << 9;
	if (ich_reset_override)
		data |= 0xC00; // set bit 10 and 11
	data |= (_dsc_calc_ob_max_addr(hw_dsc, dsc->num_active_ss_per_enc) << 18);

	SDE_REG_WRITE(dsc_c, ENC_DF_CTRL + idx, data);

	data = (dsc->config.dsc_version_minor & 0xf) << 28;
	if (dsc->config.dsc_version_minor == 0x2) {
		if (dsc->config.native_422)
			data |= BIT(22);
		if (dsc->config.native_420)
			data |= BIT(21);
	}

	bpp = dsc->config.bits_per_pixel;
	/* as per hw requirement bpp should be programmed
	 * twice the actual value in case of 420 or 422 encoding
	 */
	if (dsc->config.native_422 || dsc->config.native_420)
		bpp = 2 * bpp;
	data |= (dsc->config.block_pred_enable ? 1 : 0) << 20;
	data |= (bpp << 10);
	data |= (dsc->config.line_buf_depth & 0xf) << 6;
	data |= dsc->config.convert_rgb << 4;
	data |= dsc->config.bits_per_component & 0xf;

	SDE_REG_WRITE(dsc_c, DSC_MAIN_CONF + idx, data);

	data = (dsc->config.pic_width & 0xffff) |
		((dsc->config.pic_height & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_PICTURE_SIZE + idx, data);

	data = (dsc->config.slice_width & 0xffff) |
		((dsc->config.slice_height & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_SLICE_SIZE + idx, data);

	SDE_REG_WRITE(dsc_c, DSC_MISC_SIZE + idx,
			(dsc->config.slice_chunk_size) & 0xffff);

	data = (dsc->config.initial_xmit_delay & 0xffff) |
		((dsc->config.initial_dec_delay & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_HRD_DELAYS + idx, data);

	SDE_REG_WRITE(dsc_c, DSC_RC_SCALE + idx,
			dsc->config.initial_scale_value & 0x3f);

	data = (dsc->config.scale_increment_interval & 0xffff) |
		((dsc->config.scale_decrement_interval & 0x7ff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_RC_SCALE_INC_DEC + idx, data);

	data = (dsc->config.first_line_bpg_offset & 0x1f) |
		((dsc->config.second_line_bpg_offset & 0x1f) << 5);

	SDE_REG_WRITE(dsc_c, DSC_RC_OFFSETS_1 + idx, data);

	data = (dsc->config.nfl_bpg_offset & 0xffff) |
		((dsc->config.slice_bpg_offset & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_RC_OFFSETS_2 + idx, data);

	data = (dsc->config.initial_offset & 0xffff) |
		((dsc->config.final_offset & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_RC_OFFSETS_3 + idx, data);

	data = (dsc->config.nsl_bpg_offset & 0xffff) |
		((dsc->config.second_line_offset_adj & 0xffff) << 16);

	SDE_REG_WRITE(dsc_c, DSC_RC_OFFSETS_4 + idx, data);

	data = (dsc->config.flatness_min_qp & 0x1f);
	data |= (dsc->config.flatness_max_qp & 0x1f) << 5;
	data |= (dsc->det_thresh_flatness & 0xff) << 10;

	SDE_REG_WRITE(dsc_c, DSC_FLATNESS_QP + idx, data);

	SDE_REG_WRITE(dsc_c, DSC_RC_MODEL_SIZE + idx,
			(dsc->config.rc_model_size) & 0xffff);

	data = dsc->config.rc_edge_factor & 0xf;
	data |= (dsc->config.rc_quant_incr_limit0 & 0x1f) << 8;
	data |= (dsc->config.rc_quant_incr_limit1 & 0x1f) << 13;
	data |= (dsc->config.rc_tgt_offset_high & 0xf) << 20;
	data |= (dsc->config.rc_tgt_offset_low & 0xf) << 24;

	SDE_REG_WRITE(dsc_c, DSC_RC_CONFIG + idx, data);

	/* program the dsc wrapper */
	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_CTL, &idx))
		return;

	data = BIT(0); /* encoder enable */
	if (dsc->config.native_422)
		data |= BIT(8);
	else if (dsc->config.native_420)
		data |= BIT(9);
	if (!dsc->config.convert_rgb)
		data |= BIT(10);
	if (dsc->config.bits_per_component == 8)
		data |= BIT(11);
	if (mode & DSC_MODE_SPLIT_PANEL)
		data |= BIT(12);
	if (mode & DSC_MODE_MULTIPLEX)
		data |= BIT(13);
	if (!(mode & DSC_MODE_VIDEO))
		data |= BIT(17);
	if (dsc->dsc_4hsmerge_en) {
		data |= dsc->dsc_4hsmerge_padding << 18;
		data |= dsc->dsc_4hsmerge_alignment << 22;
		data |= BIT(16);
	}

	SDE_REG_WRITE(dsc_c, DSC_CFG + idx, data);
}

static void sde_hw_dsc_config_thresh(struct sde_hw_dsc *hw_dsc,
		struct msm_display_dsc_info *dsc)
{
	struct sde_hw_blk_reg_map *dsc_c;
	u32 idx, off;
	int i, j = 0;
	struct drm_dsc_rc_range_parameters *rc;
	u32 data = 0, min_qp = 0, max_qp = 0, bpg_off = 0;

	if (!hw_dsc || !dsc)
		return;

	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_ENC, &idx))
		return;

	dsc_c = &hw_dsc->hw;
	rc = dsc->config.rc_range_params;

	off = 0;
	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; i++) {
		data |= dsc->config.rc_buf_thresh[i] << (8*j);
		j++;
		if ((j == 4) || (i == DSC_NUM_BUF_RANGES - 2)) {
			SDE_REG_WRITE(dsc_c, DSC_RC_BUF_THRESH_0 + idx + off,
					data);
			off += 4;
			j = 0;
			data = 0;
		}
	}

	off = 0;
	for (i = 0; i < DSC_NUM_BUF_RANGES; i++) {
		min_qp |= (rc[i].range_min_qp & 0x1f) << 5*j;
		max_qp |= (rc[i].range_max_qp & 0x1f) << 5*j;
		bpg_off |= (rc[i].range_bpg_offset & 0x3f) << 6*j;
		j++;
		if (j == 5) {
			SDE_REG_WRITE(dsc_c, DSC_RC_MIN_QP_0 + idx + off,
					min_qp);
			SDE_REG_WRITE(dsc_c, DSC_RC_MAX_QP_0 + idx + off,
					max_qp);
			SDE_REG_WRITE(dsc_c,
					DSC_RC_RANGE_BPG_OFFSETS_0 + idx + off,
					bpg_off);
			off += 4;
			j = 0;
			min_qp = 0;
			max_qp = 0;
			bpg_off = 0;
		}
	}
}

static void sde_hw_dsc_bind_pingpong_blk(
		struct sde_hw_dsc *hw_dsc,
		bool enable,
		const enum sde_pingpong pp)
{
	struct sde_hw_blk_reg_map *dsc_c;
	int idx;
	int mux_cfg = 0xF; /* Disabled */

	if (!hw_dsc)
		return;

	if (_dsc_subblk_offset(hw_dsc, SDE_DSC_CTL, &idx))
		return;

	dsc_c = &hw_dsc->hw;
	if (enable)
		mux_cfg = (pp - PINGPONG_0) & 0x7;

	SDE_REG_WRITE(dsc_c, DSC_CTL + idx, mux_cfg);
}

void sde_dsc1_2_setup_ops(struct sde_hw_dsc_ops *ops,
		const unsigned long features)
{
	ops->dsc_disable = sde_hw_dsc_disable;
	ops->dsc_config = sde_hw_dsc_config;
	ops->dsc_config_thresh = sde_hw_dsc_config_thresh;
	ops->bind_pingpong_blk = sde_hw_dsc_bind_pingpong_blk;
}

