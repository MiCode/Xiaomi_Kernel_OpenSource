// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <drm/msm_drm_pp.h>
#include "sde_kms.h"
#include "sde_reg_dma.h"
#include "sde_hw_rc.h"
#include "sde_hw_catalog.h"
#include "sde_hw_util.h"
#include "sde_hw_dspp.h"
#include "sde_hw_reg_dma_v1_color_proc.h"

/**
 * Hardware register set
 */
#define SDE_HW_RC_REG0 0x00
#define SDE_HW_RC_REG1 0x04
#define SDE_HW_RC_REG2 0x08
#define SDE_HW_RC_REG3 0x0C
#define SDE_HW_RC_REG4 0x10
#define SDE_HW_RC_REG5 0x14
#define SDE_HW_RC_REG6 0x18
#define SDE_HW_RC_REG7 0x1C
#define SDE_HW_RC_REG8 0x20
#define SDE_HW_RC_REG9 0x24
#define SDE_HW_RC_REG10 0x28
#define SDE_HW_RC_REG11 0x2C
#define SDE_HW_RC_REG12 0x30
#define SDE_HW_RC_REG13 0x34

#define SDE_HW_RC_DATA_REG_SIZE  18
#define SDE_HW_RC_SKIP_DATA_PROG 0x1

#define SDE_HW_RC_DISABLE_R1 0x01E
#define SDE_HW_RC_DISABLE_R2 0x1E0

#define SDE_HW_RC_PU_SKIP_OP 0x1

/**
 * struct sde_hw_rc_state - rounded corner cached state per RC instance
 *
 * @last_rc_mask_cfg: cached value of most recent programmed mask.
 * @mask_programmed: true if mask was programmed at least once to RC hardware.
 * @last_roi_list: cached value of most recent processed list of ROIs.
 * @roi_programmed: true if list of ROIs were processed at least once.
 */
struct sde_hw_rc_state {
	struct drm_msm_rc_mask_cfg *last_rc_mask_cfg;
	bool mask_programmed;
	struct msm_roi_list *last_roi_list;
	bool roi_programmed;
};

static struct sde_hw_rc_state rc_state[RC_MAX - RC_0] = {
	{
		.last_rc_mask_cfg = NULL,
		.last_roi_list = NULL,
		.mask_programmed = false,
		.roi_programmed = false,
	},
	{
		.last_rc_mask_cfg = NULL,
		.last_roi_list = NULL,
		.mask_programmed = false,
		.roi_programmed = false,
	},
};
#define RC_STATE(hw_dspp) rc_state[hw_dspp->cap->sblk->rc.idx]

enum rc_param_r {
	RC_PARAM_R0     = 0x0,
	RC_PARAM_R1     = 0x1,
	RC_PARAM_R2     = 0x2,
	RC_PARAM_R1R2   = (RC_PARAM_R1 | RC_PARAM_R2),
};

enum rc_param_a {
	RC_PARAM_A0     = 0x2,
	RC_PARAM_A1     = 0x4,
};

enum rc_param_b {
	RC_PARAM_B0     = 0x0,
	RC_PARAM_B1     = 0x1,
	RC_PARAM_B2     = 0x2,
	RC_PARAM_B1B2   = (RC_PARAM_B1 | RC_PARAM_B2),
};

enum rc_param_c {
	RC_PARAM_C0     = (BIT(8)),
	RC_PARAM_C1     = (BIT(10)),
	RC_PARAM_C2     = (BIT(10) | BIT(11)),
	RC_PARAM_C3     = (BIT(8) | BIT(10)),
	RC_PARAM_C4     = (BIT(8) | BIT(9)),
	RC_PARAM_C5     = (BIT(8) | BIT(9) | BIT(10) | BIT(11)),
};

enum rc_merge_mode {
	RC_MERGE_SINGLE_PIPE = 0x0,
	RC_MERGE_DUAL_PIPE   = 0x1
};

struct rc_config_table {
	enum rc_param_a param_a;
	enum rc_param_b param_b;
	enum rc_param_c param_c;
	enum rc_merge_mode merge_mode;
	enum rc_merge_mode merge_mode_en;
};

static struct rc_config_table config_table[] =  {
	/* RC_PARAM_A0 configurations */
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B0,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B1B2,
		.param_c = RC_PARAM_C3,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B1,
		.param_c = RC_PARAM_C0,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B2,
		.param_c = RC_PARAM_C1,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B0,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_DUAL_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B1B2,
		.param_c = RC_PARAM_C3,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_DUAL_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B1,
		.param_c = RC_PARAM_C0,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A0,
		.param_b = RC_PARAM_B2,
		.param_c = RC_PARAM_C1,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,
	},

	/* RC_PARAM_A1 configurations */
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B0,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B1B2,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B1,
		.param_c = RC_PARAM_C4,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B2,
		.param_c = RC_PARAM_C2,
		.merge_mode = RC_MERGE_SINGLE_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B0,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_DUAL_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B1B2,
		.param_c = RC_PARAM_C5,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_DUAL_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B1,
		.param_c = RC_PARAM_C4,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
	{
		.param_a = RC_PARAM_A1,
		.param_b = RC_PARAM_B2,
		.param_c = RC_PARAM_C2,
		.merge_mode = RC_MERGE_DUAL_PIPE,
		.merge_mode_en = RC_MERGE_SINGLE_PIPE,

	},
};

static inline void _sde_hw_rc_reg_write(
		struct sde_hw_dspp *hw_dspp,
		int offset,
		u32 value)
{
	u32 address = hw_dspp->cap->sblk->rc.base + offset;

	SDE_DEBUG("rc:%u, address:0x%08X, value:0x%08X\n",
			hw_dspp->cap->sblk->rc.idx,
			hw_dspp->hw.blk_off + address, value);
	SDE_REG_WRITE(&hw_dspp->hw, address, value);
}

static int _sde_hw_rc_get_enable_bits(
		enum rc_param_a param_a,
		enum rc_param_b param_b,
		enum rc_param_c *param_c,
		u32 merge_mode,
		u32 *merge_mode_en)
{
	int i = 0;

	if (!param_c || !merge_mode_en) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(config_table); i++) {
		if (merge_mode == config_table[i].merge_mode &&
				param_a == config_table[i].param_a &&
				param_b == config_table[i].param_b) {
			*param_c = config_table[i].param_c;
			*merge_mode_en = config_table[i].merge_mode_en;
			SDE_DEBUG("found param_c:0x%08X, merge_mode_en:%d\n",
					*param_c, *merge_mode_en);
			return 0;
		}
	}
	SDE_ERROR("configuration not supported");

	return -EINVAL;
}

static int _sde_hw_rc_get_merge_mode(
		const struct sde_hw_cp_cfg *hw_cfg,
		u32 *merge_mode)
{
	int rc = 0;

	if (!hw_cfg || !merge_mode) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (hw_cfg->num_of_mixers == 1)
		*merge_mode = RC_MERGE_SINGLE_PIPE;
	else if (hw_cfg->num_of_mixers == 2)
		*merge_mode = RC_MERGE_DUAL_PIPE;
	else {
		SDE_ERROR("invalid number of mixers:%d\n",
				hw_cfg->num_of_mixers);
		return -EINVAL;
	}

	SDE_DEBUG("number mixers:%u, merge mode:%u\n",
			hw_cfg->num_of_mixers, *merge_mode);

	return rc;
}

static int _sde_hw_rc_get_ajusted_roi(
		const struct sde_hw_cp_cfg *hw_cfg,
		const struct sde_rect *pu_roi,
		struct sde_rect *rc_roi)
{
	int rc = 0;

	if (!hw_cfg || !pu_roi || !rc_roi) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	/*when partial update is disabled, use full screen ROI*/
	if (pu_roi->w == 0 && pu_roi->h == 0) {
		rc_roi->x = pu_roi->x;
		rc_roi->y = pu_roi->y;
		rc_roi->w = hw_cfg->displayh;
		rc_roi->h = hw_cfg->displayv;
	} else {
		memcpy(rc_roi, pu_roi, sizeof(struct sde_rect));
	}

	SDE_DEBUG("displayh:%u, displayv:%u\n", hw_cfg->displayh,
			hw_cfg->displayv);
	SDE_DEBUG("pu_roi x:%u, y:%u, w:%u, h:%u\n", pu_roi->x, pu_roi->y,
			pu_roi->w, pu_roi->h);
	SDE_DEBUG("rc_roi x:%u, y:%u, w:%u, h:%u\n", rc_roi->x, rc_roi->y,
			rc_roi->w, rc_roi->h);

	return rc;
}

static int _sde_hw_rc_get_param_rb(
		const struct drm_msm_rc_mask_cfg *rc_mask_cfg,
		const struct sde_rect *rc_roi,
		enum rc_param_r *param_r,
		enum rc_param_b *param_b)
{
	int rc = 0;
	int half_panel_x = 0, half_panel_w = 0;
	int cfg_param_01 = 0, cfg_param_02 = 0;
	int x1 = 0, x2 = 0, y1 = 0, y2 = 0;

	if (!rc_mask_cfg || !rc_roi || !param_r || !param_b) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (rc_mask_cfg->cfg_param_03 == RC_PARAM_A1)
		half_panel_w = rc_mask_cfg->cfg_param_04[0] +
				rc_mask_cfg->cfg_param_04[1];
	else if (rc_mask_cfg->cfg_param_03 == RC_PARAM_A0)
		half_panel_w = rc_mask_cfg->cfg_param_04[0];
	else {
		SDE_ERROR("invalid cfg_param_03:%u\n",
				rc_mask_cfg->cfg_param_03);
		return -EINVAL;
	}

	cfg_param_01 = rc_mask_cfg->cfg_param_01;
	cfg_param_02 = rc_mask_cfg->cfg_param_02;
	x1 = rc_roi->x;
	x2 = rc_roi->x + rc_roi->w - 1;
	y1 = rc_roi->y;
	y2 = rc_roi->y + rc_roi->h - 1;
	half_panel_x = half_panel_w - 1;

	SDE_DEBUG("x1:%u y1:%u x2:%u y2:%u\n", x1, y1, x2, y2);
	SDE_DEBUG("cfg_param_01:%u cfg_param_02:%u half_panel_x:%u",
			cfg_param_01, cfg_param_02, half_panel_x);

	if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0 || half_panel_x < 0 ||
			x1 >= x2 || y1 >= y2) {
		SDE_ERROR("invalid coordinates\n");
		return -EINVAL;
	}

	if (y1 <= cfg_param_01) {
		*param_r |= RC_PARAM_R1;
		if (x1 <= half_panel_x && x2 <= half_panel_x)
			*param_b |= RC_PARAM_B1;
		else if (x1 > half_panel_x && x2 > half_panel_x)
			*param_b |= RC_PARAM_B2;
		else
			*param_b |= RC_PARAM_B1B2;
	}

	if (y2 >= cfg_param_02) {
		*param_r |= RC_PARAM_R2;
		if (x1 <= half_panel_x && x2 <= half_panel_x)
			*param_b |= RC_PARAM_B1;
		else if (x1 > half_panel_x && x2 > half_panel_x)
			*param_b |= RC_PARAM_B2;
		else
			*param_b |= RC_PARAM_B1B2;
	}

	SDE_DEBUG("param_r:0x%08X param_b:0x%08X\n", *param_r, *param_b);
	SDE_EVT32(rc_roi->x, rc_roi->y, rc_roi->w, rc_roi->h);
	SDE_EVT32(x1, y1, x2, y2, cfg_param_01, cfg_param_02, half_panel_x);

	return rc;
}

static int _sde_hw_rc_program_enable_bits(
		struct sde_hw_dspp *hw_dspp,
		struct drm_msm_rc_mask_cfg *rc_mask_cfg,
		enum rc_param_a param_a,
		enum rc_param_b param_b,
		enum rc_param_r param_r,
		int merge_mode,
		struct sde_rect *rc_roi)
{
	int rc = 0;
	u32 val = 0, param_c = 0, rc_merge_mode = 0, ystart = 0;
	u64 flags = 0;
	bool r1_valid = false, r2_valid = false;
	bool pu_in_r1 = false, pu_in_r2 = false;
	bool r1_enable = false, r2_enable = false;

	if (!hw_dspp || !rc_mask_cfg || !rc_roi) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	rc = _sde_hw_rc_get_enable_bits(param_a, param_b, &param_c,
			merge_mode, &rc_merge_mode);
	if (rc) {
		SDE_ERROR("invalid enable bits, rc:%d\n", rc);
		return rc;
	}

	flags = rc_mask_cfg->flags;
	r1_valid = ((flags & SDE_HW_RC_DISABLE_R1) != SDE_HW_RC_DISABLE_R1);
	r2_valid = ((flags & SDE_HW_RC_DISABLE_R2) != SDE_HW_RC_DISABLE_R2);
	pu_in_r1 = (param_r == RC_PARAM_R1 || param_r == RC_PARAM_R1R2);
	pu_in_r2 = (param_r == RC_PARAM_R2 || param_r == RC_PARAM_R1R2);
	r1_enable = (r1_valid && pu_in_r1);
	r2_enable = (r2_valid && pu_in_r2);

	if (r1_enable)
		val |= BIT(0);

	if (r2_enable)
		val |= BIT(4);

	/*corner case for partial update in R2 region*/
	if (!r1_enable && r2_enable)
		ystart = rc_roi->y;

	SDE_DEBUG("flags:%x, R1 valid:%d, R2 valid:%d, PU in R1:%d, PU in R2:%d, Y_START:%d\n",
			flags, r1_valid, r2_valid, pu_in_r1, pu_in_r2, ystart);
	SDE_EVT32(flags, r1_valid, r2_valid, pu_in_r1, pu_in_r2, ystart);

	val |= param_c;
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG1, val);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG13, ystart);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG9, rc_merge_mode);

	return rc;
}

static int _sde_hw_rc_program_roi(
		struct sde_hw_dspp *hw_dspp,
		struct drm_msm_rc_mask_cfg *rc_mask_cfg,
		int merge_mode,
		struct sde_rect *rc_roi)
{
	int rc = 0;
	u32 val2 = 0, val3 = 0, val4 = 0;
	enum rc_param_r param_r = RC_PARAM_R0;
	enum rc_param_a param_a = RC_PARAM_A0;
	enum rc_param_b param_b = RC_PARAM_B0;

	if (!hw_dspp || !rc_mask_cfg || !rc_roi) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	rc = _sde_hw_rc_get_param_rb(rc_mask_cfg, rc_roi, &param_r,
			&param_b);
	if (rc) {
		SDE_ERROR("invalid rc roi, rc:%d\n", rc);
		return rc;
	}

	param_a = rc_mask_cfg->cfg_param_03;
	rc = _sde_hw_rc_program_enable_bits(hw_dspp, rc_mask_cfg,
			param_a, param_b, param_r, merge_mode, rc_roi);
	if (rc) {
		SDE_ERROR("failed to program enable bits, rc:%d\n", rc);
		return rc;
	}

	val2 = ((rc_mask_cfg->cfg_param_01 & 0x0000FFFF) |
			((rc_mask_cfg->cfg_param_02 << 16) & 0xFFFF0000));
	if (param_a == RC_PARAM_A1) {
		val3 = (rc_mask_cfg->cfg_param_04[0] |
				(rc_mask_cfg->cfg_param_04[1] << 16));
		val4 = (rc_mask_cfg->cfg_param_04[2] |
				(rc_mask_cfg->cfg_param_04[3] << 16));
	} else if (param_a == RC_PARAM_A0) {
		val3 = (rc_mask_cfg->cfg_param_04[0]);
		val4 = (rc_mask_cfg->cfg_param_04[1]);
	}

	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG2, val2);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG3, val3);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG4, val4);

	return 0;
}

static int _sde_hw_rc_program_data_offset(
		struct sde_hw_dspp *hw_dspp,
		struct drm_msm_rc_mask_cfg *rc_mask_cfg)
{
	int rc = 0;
	u32 val5 = 0, val6 = 0, val7 = 0, val8 = 0;
	u32 cfg_param_07;

	if (!hw_dspp || !rc_mask_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	cfg_param_07 = rc_mask_cfg->cfg_param_07;
	if (rc_mask_cfg->cfg_param_03 == RC_PARAM_A1) {
		val5 = ((rc_mask_cfg->cfg_param_05[0] + cfg_param_07) |
				((rc_mask_cfg->cfg_param_05[1] + cfg_param_07)
				<< 16));
		val6 = ((rc_mask_cfg->cfg_param_05[2] + cfg_param_07)|
				((rc_mask_cfg->cfg_param_05[3] + cfg_param_07)
				<< 16));
		val7 = ((rc_mask_cfg->cfg_param_06[0] + cfg_param_07) |
				((rc_mask_cfg->cfg_param_06[1] + cfg_param_07)
				<< 16));
		val8 = ((rc_mask_cfg->cfg_param_06[2] + cfg_param_07) |
				((rc_mask_cfg->cfg_param_06[3] + cfg_param_07)
				<< 16));
	} else if (rc_mask_cfg->cfg_param_03 == RC_PARAM_A0) {
		val5 = (rc_mask_cfg->cfg_param_05[0] + cfg_param_07);
		val6 = (rc_mask_cfg->cfg_param_05[1] + cfg_param_07);
		val7 = (rc_mask_cfg->cfg_param_06[0] + cfg_param_07);
		val8 = (rc_mask_cfg->cfg_param_06[1] + cfg_param_07);
	}

	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG5, val5);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG6, val6);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG7, val7);
	_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG8, val8);

	return rc;
}

static int sde_hw_rc_check_mask_cfg(
		struct sde_hw_dspp *hw_dspp,
		struct sde_hw_cp_cfg *hw_cfg,
		struct drm_msm_rc_mask_cfg *rc_mask_cfg)
{
	int rc = 0;
	u32 i = 0;
	u32 half_panel_width;
	u64 flags;
	u32 cfg_param_01, cfg_param_02, cfg_param_03;
	u32 cfg_param_07, cfg_param_08;
	u32 *cfg_param_04, *cfg_param_05, *cfg_param_06;
	bool r1_enable, r2_enable;

	if (!hw_dspp || !hw_cfg || !rc_mask_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (hw_cfg->panel_height != rc_mask_cfg->height ||
		rc_mask_cfg->width != hw_cfg->panel_width) {
		SDE_ERROR("RC mask Layer: h %d w %d panel: h %d w %d mismatch\n",
				rc_mask_cfg->height, rc_mask_cfg->width,
				hw_cfg->panel_height, hw_cfg->panel_width);
		return -EINVAL;
	}
	flags = rc_mask_cfg->flags;
	cfg_param_01 = rc_mask_cfg->cfg_param_01;
	cfg_param_02 = rc_mask_cfg->cfg_param_02;
	cfg_param_03 = rc_mask_cfg->cfg_param_03;
	cfg_param_04 = rc_mask_cfg->cfg_param_04;
	cfg_param_05 = rc_mask_cfg->cfg_param_05;
	cfg_param_06 = rc_mask_cfg->cfg_param_06;
	cfg_param_07 = rc_mask_cfg->cfg_param_07;
	cfg_param_08 = rc_mask_cfg->cfg_param_08;
	r1_enable = ((flags & SDE_HW_RC_DISABLE_R1) != SDE_HW_RC_DISABLE_R1);
	r2_enable = ((flags & SDE_HW_RC_DISABLE_R2) != SDE_HW_RC_DISABLE_R2);

	if (cfg_param_07 > hw_dspp->cap->sblk->rc.mem_total_size) {
		SDE_ERROR("invalid cfg_param_07:%d\n", cfg_param_07);
		return -EINVAL;
	}

	if (cfg_param_08 > RC_DATA_SIZE_MAX) {
		SDE_ERROR("invalid cfg_param_08:%d\n", cfg_param_08);
		return -EINVAL;
	}

	if ((cfg_param_07 + cfg_param_08) >
			hw_dspp->cap->sblk->rc.mem_total_size) {
		SDE_ERROR("invalid cfg_param_08:%d, cfg_param_07:%d, max:%u\n",
				cfg_param_08, cfg_param_07,
				hw_dspp->cap->sblk->rc.mem_total_size);
		return -EINVAL;
	}

	if (!(cfg_param_03 == RC_PARAM_A1 || cfg_param_03 == RC_PARAM_A0)) {
		SDE_ERROR("invalid cfg_param_03:%d\n", cfg_param_03);
		return -EINVAL;
	}

	for (i = 0; i < cfg_param_03; i++) {
		if (cfg_param_04[i] < 4) {
			SDE_ERROR("invalid cfg_param_04[%d]:%d\n", i,
					cfg_param_04[i]);
			return -EINVAL;
		}
	}

	half_panel_width = hw_cfg->panel_width / cfg_param_03 * 2;
	for (i = 0; i < cfg_param_03; i += 2) {
		if (cfg_param_04[i] + cfg_param_04[i+1] != half_panel_width) {
			SDE_ERROR("invalid ratio [%d]:%d, [%d]:%d, %d\n",
					i, cfg_param_04[i], i+1,
					cfg_param_04[i+1], half_panel_width);
			return -EINVAL;
		}
	}

	if (r1_enable && r2_enable) {
		if (cfg_param_01 > cfg_param_02) {
			SDE_ERROR("invalid cfg_param_01:%d, cfg_param_02:%d\n",
					cfg_param_01, cfg_param_02);
			return -EINVAL;
		}
	} else {
		SDE_DEBUG("R1 or R2 disabled, skip overlap check");
	}

	if (r1_enable) {
		if (cfg_param_01 < 1) {
			SDE_ERROR("invalid min cfg_param_01:%d\n",
					cfg_param_01);
			return -EINVAL;
		}

		for (i = 0; i < cfg_param_03 - 1; i++) {
			if (cfg_param_05[i] >= cfg_param_05[i+1]) {
				SDE_ERROR("invalid cfg_param_05 %d, %d\n",
						cfg_param_05[i],
						cfg_param_05[i+1]);
				return -EINVAL;
			}
		}

		for (i = 0; i < cfg_param_03; i++) {
			if (cfg_param_05[i] > RC_DATA_SIZE_MAX) {
				SDE_ERROR("invalid cfg_param_05[%d]:%d\n", i,
						cfg_param_05[i]);
				return -EINVAL;
			}

		}
	} else {
		SDE_DEBUG("R1 is disabled, skip parameter checks\n");
	}

	if (r2_enable) {
		if ((hw_cfg->panel_height - cfg_param_02) < 1) {
			SDE_ERROR("invalid max cfg_param_02:%d, panel_height:%d\n",
					cfg_param_02, hw_cfg->panel_height);
			return -EINVAL;
		}

		for (i = 0; i < cfg_param_03 - 1; i++) {
			if (cfg_param_06[i] >= cfg_param_06[i+1]) {
				SDE_ERROR("invalid cfg_param_06 %d, %d\n",
						cfg_param_06[i],
						cfg_param_06[i+1]);
				return -EINVAL;
			}
		}

		for (i = 0; i < cfg_param_03; i++) {
			if (cfg_param_06[i] > RC_DATA_SIZE_MAX) {
				SDE_ERROR("invalid cfg_param_06[%d]:%d\n", i,
						cfg_param_06[i]);
				return -EINVAL;
			}

		}
	} else {
		SDE_DEBUG("R2 is disabled, skip parameter checks\n");
	}

	return rc;
}

int sde_hw_rc_check_mask(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if ((hw_cfg->len == 0 && hw_cfg->payload == NULL)) {
		SDE_DEBUG("RC feature disabled, skip mask checks\n");
		return 0;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_rc_mask_cfg) ||
			!hw_cfg->payload) {
		SDE_ERROR("invalid payload len %d exp %zd\n", hw_cfg->len,
				sizeof(struct drm_msm_rc_mask_cfg));
		return -EINVAL;
	}

	rc_mask_cfg = hw_cfg->payload;
	if (hw_cfg->num_of_mixers != 1 && hw_cfg->num_of_mixers != 2) {
		SDE_ERROR("invalid number of mixers:%d\n",
				hw_cfg->num_of_mixers);
		return -EINVAL;
	}

	rc = sde_hw_rc_check_mask_cfg(hw_dspp, hw_cfg, rc_mask_cfg);
	if (rc) {
		SDE_ERROR("invalid rc mask configuration, rc:%d\n", rc);
		return rc;
	}

	return 0;
}

int sde_hw_rc_check_pu_roi(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct msm_roi_list *roi_list;
	struct msm_roi_list empty_roi_list;
	struct sde_rect rc_roi, merged_roi;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;
	bool mask_programmed = false;
	enum rc_param_r param_r = RC_PARAM_R0;
	enum rc_param_b param_b = RC_PARAM_B0;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (hw_cfg->len != sizeof(struct sde_drm_roi_v1)) {
		SDE_ERROR("invalid payload size\n");
		return -EINVAL;
	}

	roi_list = hw_cfg->payload;
	if (!roi_list) {
		SDE_DEBUG("full frame update\n");
		memset(&empty_roi_list, 0, sizeof(struct msm_roi_list));
		roi_list = &empty_roi_list;
	}

	rc_mask_cfg = RC_STATE(hw_dspp).last_rc_mask_cfg;
	mask_programmed = RC_STATE(hw_dspp).mask_programmed;

	/* early return when there is no mask in memory */
	if (!mask_programmed || !rc_mask_cfg) {
		SDE_DEBUG("no previous rc mask programmed\n");
		return SDE_HW_RC_PU_SKIP_OP;
	}

	rc = sde_hw_rc_check_mask_cfg(hw_dspp, hw_cfg, rc_mask_cfg);
	if (rc) {
		SDE_ERROR("invalid rc mask configuration, rc:%d\n", rc);
		return rc;
	}

	sde_kms_rect_merge_rectangles(roi_list, &merged_roi);
	rc = _sde_hw_rc_get_ajusted_roi(hw_cfg, &merged_roi, &rc_roi);
	if (rc) {
		SDE_ERROR("failed to get adjusted roi, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_get_param_rb(rc_mask_cfg, &rc_roi,
			&param_r, &param_b);
	if (rc) {
		SDE_ERROR("invalid rc roi, rc:%d\n", rc);
		return rc;
	}

	return 0;
}

int sde_hw_rc_setup_pu_roi(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct msm_roi_list *roi_list;
	struct msm_roi_list empty_roi_list;
	struct sde_rect rc_roi, merged_roi;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;
	enum rc_param_r param_r = RC_PARAM_R0;
	enum rc_param_a param_a = RC_PARAM_A0;
	enum rc_param_b param_b = RC_PARAM_B0;
	u32 merge_mode = 0;
	bool mask_programmed = false;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (hw_cfg->len != sizeof(struct sde_drm_roi_v1)) {
		SDE_ERROR("invalid payload size\n");
		return -EINVAL;
	}

	roi_list = hw_cfg->payload;
	if (!roi_list) {
		SDE_DEBUG("full frame update\n");
		memset(&empty_roi_list, 0, sizeof(struct msm_roi_list));
		roi_list = &empty_roi_list;
	}

	rc_mask_cfg = RC_STATE(hw_dspp).last_rc_mask_cfg;
	mask_programmed = RC_STATE(hw_dspp).mask_programmed;

	/* early return when there is no mask in memory */
	if (!mask_programmed || !rc_mask_cfg) {
		SDE_DEBUG("no previous rc mask programmed\n");
		return SDE_HW_RC_PU_SKIP_OP;
	}

	sde_kms_rect_merge_rectangles(roi_list, &merged_roi);
	rc = _sde_hw_rc_get_ajusted_roi(hw_cfg, &merged_roi, &rc_roi);
	if (rc) {
		SDE_ERROR("failed to get adjusted roi, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_get_merge_mode(hw_cfg, &merge_mode);
	if (rc) {
		SDE_ERROR("invalid merge_mode, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_get_param_rb(rc_mask_cfg, &rc_roi, &param_r,
			&param_b);
	if (rc) {
		SDE_ERROR("invalid roi, rc:%d\n", rc);
		return rc;
	}

	param_a = rc_mask_cfg->cfg_param_03;
	rc = _sde_hw_rc_program_enable_bits(hw_dspp, rc_mask_cfg,
			param_a, param_b, param_r, merge_mode, &rc_roi);
	if (rc) {
		SDE_ERROR("failed to program enable bits, rc:%d\n", rc);
		return rc;
	}

	memcpy(RC_STATE(hw_dspp).last_roi_list,
			roi_list, sizeof(struct msm_roi_list));
	RC_STATE(hw_dspp).roi_programmed = true;

	return 0;
}

int sde_hw_rc_setup_mask(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;
	struct sde_rect rc_roi, merged_roi;
	struct msm_roi_list *last_roi_list;
	u32 merge_mode = 0;
	bool roi_programmed = false;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if ((hw_cfg->len == 0 && hw_cfg->payload == NULL)) {
		SDE_DEBUG("RC feature disabled\n");
		_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG1, 0);

		memset(RC_STATE(hw_dspp).last_rc_mask_cfg, 0,
				sizeof(struct drm_msm_rc_mask_cfg));
		RC_STATE(hw_dspp).mask_programmed = false;
		memset(RC_STATE(hw_dspp).last_roi_list, 0,
				sizeof(struct msm_roi_list));
		RC_STATE(hw_dspp).roi_programmed = false;

		return 0;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_rc_mask_cfg) ||
			!hw_cfg->payload) {
		SDE_ERROR("invalid payload\n");
		return -EINVAL;
	}

	rc_mask_cfg = hw_cfg->payload;
	last_roi_list = RC_STATE(hw_dspp).last_roi_list;
	roi_programmed = RC_STATE(hw_dspp).roi_programmed;

	if (!roi_programmed) {
		SDE_DEBUG("full frame update\n");
		memset(&merged_roi, 0, sizeof(struct sde_rect));
	} else {
		SDE_DEBUG("partial frame update\n");
		sde_kms_rect_merge_rectangles(last_roi_list, &merged_roi);
	}

	rc = _sde_hw_rc_get_ajusted_roi(hw_cfg, &merged_roi, &rc_roi);
	if (rc) {
		SDE_ERROR("failed to get adjusted roi, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_get_merge_mode(hw_cfg, &merge_mode);
	if (rc) {
		SDE_ERROR("invalid merge_mode, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_program_roi(hw_dspp, rc_mask_cfg,
			merge_mode, &rc_roi);
	if (rc) {
		SDE_ERROR("unable to program rc roi, rc:%d\n", rc);
		return rc;
	}

	rc = _sde_hw_rc_program_data_offset(hw_dspp, rc_mask_cfg);
	if (rc) {
		SDE_ERROR("unable to program data offsets, rc:%d\n", rc);
		return rc;
	}

	memcpy(RC_STATE(hw_dspp).last_rc_mask_cfg, rc_mask_cfg,
			sizeof(struct drm_msm_rc_mask_cfg));
	RC_STATE(hw_dspp).mask_programmed = true;

	return 0;
}

int sde_hw_rc_setup_data_dma(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if ((hw_cfg->len == 0 && hw_cfg->payload == NULL)) {
		SDE_DEBUG("RC feature disabled, skip data programming\n");
		return 0;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_rc_mask_cfg) ||
			!hw_cfg->payload) {
		SDE_ERROR("invalid payload\n");
		return -EINVAL;
	}

	rc_mask_cfg = hw_cfg->payload;

	if (rc_mask_cfg->flags & SDE_HW_RC_SKIP_DATA_PROG) {
		SDE_DEBUG("skip data programming\n");
		return 0;
	}

	rc = reg_dmav1_setup_rc_datav1(hw_dspp, cfg);
	if (rc) {
		SDE_ERROR("unable to setup rc with dma, rc:%d\n", rc);
		return rc;
	}

	return rc;
}

int sde_hw_rc_setup_data_ahb(struct sde_hw_dspp *hw_dspp, void *cfg)
{
	int rc = 0, i = 0;
	u32 data = 0, cfg_param_07 = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_rc_mask_cfg *rc_mask_cfg;

	if (!hw_dspp || !hw_cfg) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if ((hw_cfg->len == 0 && hw_cfg->payload == NULL)) {
		SDE_DEBUG("rc feature disabled, skip data programming\n");
		return 0;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_rc_mask_cfg) ||
			!hw_cfg->payload) {
		SDE_ERROR("invalid payload\n");
		return -EINVAL;
	}

	rc_mask_cfg = hw_cfg->payload;

	if (rc_mask_cfg->flags & SDE_HW_RC_SKIP_DATA_PROG) {
		SDE_DEBUG("skip data programming\n");
		return 0;
	}

	cfg_param_07 = rc_mask_cfg->cfg_param_07;
	SDE_DEBUG("cfg_param_07:%u\n", cfg_param_07);

	for (i = 0; i < rc_mask_cfg->cfg_param_08; i++) {
		SDE_DEBUG("cfg_param_09[%d] = 0x%016llX at %u\n", i,
				rc_mask_cfg->cfg_param_09[i], i + cfg_param_07);

		data = (i == 0) ? (BIT(30) | (cfg_param_07 << 18)) : 0;
		data |= (rc_mask_cfg->cfg_param_09[i] & 0x3FFFF);
		_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG10, data);
		data = ((rc_mask_cfg->cfg_param_09[i] >>
				SDE_HW_RC_DATA_REG_SIZE) & 0x3FFFF);
		_sde_hw_rc_reg_write(hw_dspp, SDE_HW_RC_REG10, data);
	}

	return rc;
}

int sde_hw_rc_init(struct sde_hw_dspp *hw_dspp)
{
	int rc = 0;

	RC_STATE(hw_dspp).last_roi_list = kzalloc(
			sizeof(struct msm_roi_list), GFP_KERNEL);
	if (!RC_STATE(hw_dspp).last_roi_list)
		return -ENOMEM;

	RC_STATE(hw_dspp).last_rc_mask_cfg = kzalloc(
			sizeof(struct drm_msm_rc_mask_cfg), GFP_KERNEL);
	if (!RC_STATE(hw_dspp).last_rc_mask_cfg)
		return -ENOMEM;

	return rc;
}
