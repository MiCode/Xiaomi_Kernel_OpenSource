/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/uaccess.h>
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"


/* MDP v1.7 specific macros */

/* PCC_EN for PCC opmode*/
#define PCC_ENABLE	BIT(0)
#define PCC_OP_MODE_OFF 0
#define PCC_CONST_COEFF_OFF 4
#define PCC_R_COEFF_OFF 0x10
#define PCC_G_COEFF_OFF 0x1C
#define PCC_B_COEFF_OFF 0x28
#define PCC_RG_COEFF_OFF 0x34
#define PCC_RB_COEFF_OFF 0x40
#define PCC_GB_COEFF_OFF 0x4C
#define PCC_RGB_COEFF_OFF 0x58
#define PCC_CONST_COEFF_MASK 0xFFFF
#define PCC_COEFF_MASK 0x3FFFF


#define GAMUT_OP_MODE_OFF 0
#define GAMUT_TABLE_INDEX 4
#define GAMUT_TABLE_UPPER_R 8
#define GAMUT_TABLE_LOWER_GB 0xC
#define GAMUT_C0_SCALE_OFF 0x10
#define GAMUT_CLK_CTRL 0xD0
#define GAMUT_CLK_STATUS 0xD4
#define GAMUT_READ_TABLE_EN  BIT(16)
#define GAMUT_TABLE_SELECT(x) ((BIT(x)) << 12)
#define GAMUT_COARSE_EN (BIT(2))
#define GAMUT_COARSE_INDEX 1248
#define GAMUT_FINE_INDEX 0
#define GAMUT_MAP_EN BIT(1)
#define GAMUT_ENABLE BIT(0)
#define GAMUT_CLK_GATING_ACTIVE 0x0
#define GAMUT_CLK_GATING_PARTIAL_ACTIVE 0x11
#define GAMUT_CLK_GATING_INACTIVE 0x33

#define IGC_MASK_MAX 3
#define IGC_C0_LUT 0
#define IGC_RGB_C0_LUT 0xC
#define IGC_DMA_C0_LUT 0x18
#define IGC_CONFIG_MASK(n) \
	((((1 << (IGC_MASK_MAX + 1)) - 1) & ~(1 << n)) << 28)
#define IGC_INDEX_UPDATE BIT(25)
#define IGC_INDEX_VALUE_UPDATE (BIT(24) | IGC_INDEX_UPDATE)
#define IGC_DATA_MASK (BIT(12) - 1)
#define IGC_DSPP_OP_MODE_EN BIT(0)

#define HIST_LUT_VIG_OP_FIRST_EN	BIT(21)
#define HIST_LUT_DSPP_OP_FIRST_EN	BIT(21)
#define HIST_LUT_VIG_OP_ENABLE		BIT(10)
#define HIST_LUT_DSPP_OP_ENABLE		BIT(19)
#define REG_SSPP_VIG_HIST_LUT_BASE	0x1400
#define REG_DSPP_HIST_LUT_BASE		0x1400
#define REG_SSPP_VIG_HIST_SWAP_BASE	0x300
#define REG_DSPP_HIST_SWAP_BASE		0x234
#define ENHIST_LOWER_VALUE_MASK		0x3FF
#define ENHIST_UPPER_VALUE_MASK		0x3FF0000
#define ENHIST_BIT_SHIFT		16

#define PGC_OPMODE_OFF 0
#define PGC_C0_LUT_INDEX 4
#define PGC_INDEX_OFF 4
#define PGC_C1C2_LUT_OFF 8
#define PGC_LUT_SWAP 0x1C
#define PGC_LUT_SEL 0x20
#define PGC_DATA_MASK (BIT(10) - 1)
#define PGC_ODD_SHIFT 16
#define PGC_SWAP 1
#define PGC_8B_ROUND BIT(1)
#define PGC_ENABLE BIT(0)

#define HIST_V3_INTR_BIT_MASK 0xF33333
#define HIST_CTL_OFF_DSPP_V1_7 0x210
#define HIST_CTL_OFF_SSPP_V1_7 0x2C4
#define HIST_DATA_OFF_DSPP_V1_7 0x1000
#define HIST_DATA_OFF_SSPP_V1_7 0xA00
#define HIST_DATA_MASK 0xFFFFFF
#define DITHER_MATRIX_OFF 0x14
#define DITHER_MATRIX_INDEX 16
#define DITHER_DEPTH_MAP_INDEX 9
static u32 dither_matrix[DITHER_MATRIX_INDEX] = {
	15, 7, 13, 5, 3, 11, 1, 9, 12, 4, 14, 6, 0, 8, 2, 10};
static u32 dither_depth_map[DITHER_DEPTH_MAP_INDEX] = {
	0, 0, 0, 0, 0, 1, 2, 3, 3};

#define PA_DSPP_GLOBAL_OFF 0x238
#define PA_DSPP_MEM_COL_SKIN_P0_OFF 0x254
#define PA_DSPP_MEM_COL_SKIN_P2_OFF 0x318
#define PA_DSPP_MEM_COL_SKY_P0_OFF 0x268
#define PA_DSPP_MEM_COL_SKY_P2_OFF 0x320
#define PA_DSPP_MEM_COL_FOL_P0_OFF 0x27C
#define PA_DSPP_MEM_COL_FOL_P2_OFF 0x328
#define PA_SIX_ZONE_LUT_OFF 0x248
#define PA_SIX_ZONE_REGION_OFF 0x250
#define PA_SIX_ZONE_ADJ_OFF 0x330
#define PA_VIG_GLOBAL_OFF 0x310
#define PA_VIG_MEM_COL_SKIN_P0_OFF 0x288
#define PA_VIG_MEM_COL_SKIN_P2_OFF 0x418
#define PA_VIG_MEM_COL_SKY_P0_OFF 0x29C
#define PA_VIG_MEM_COL_SKY_P2_OFF 0x420
#define PA_VIG_MEM_COL_FOL_P0_OFF 0x2B0
#define PA_VIG_MEM_COL_FOL_P2_OFF 0x428
#define PA_DSPP_HOLD_OFF 0x314
#define PA_VIG_HOLD_OFF 0x414
#define PA_GLOBAL_HUE_MASK 0xFFF
#define PA_GLOBAL_SAT_MASK 0xFFFF
#define PA_GLOBAL_VAL_MASK 0xFF
#define PA_GLOBAL_CONT_MASK 0xFF
#define PA_MEM_COL_ADJ_P0_MASK 0xFFFF07FF
#define PA_MEM_COL_HUE_REGION_MASK 0x7FF07FF
#define PA_MEM_COL_SAT_REGION_MASK 0xFFFFFF
#define PA_MEM_COL_VAL_REGION_MASK 0xFFFFFF
#define PA_SIX_ZONE_INDEX_UPDATE BIT(26)
#define PA_SIX_ZONE_VALUE_UPDATE BIT(25)
#define PA_SIX_ZONE_CURVE_P0_MASK 0xFFF
#define PA_SIX_ZONE_CURVE_P1_MASK 0xFFF0FFF
#define PA_SIX_ZONE_ADJ_P0_MASK 0xFFFF
#define PA_HOLD_MASK 0x3
#define PA_HOLD_SAT_SHIFT 0
#define PA_HOLD_VAL_SHIFT 2
#define PA_HOLD_SKIN_SHIFT 0
#define PA_HOLD_SKY_SHIFT 4
#define PA_HOLD_FOL_SHIFT 8
#define PA_HOLD_SIX_ZONE_SHIFT 12
#define PA_HOLD_SKIN_MASK 0xF
#define PA_HOLD_SKY_MASK 0xF0
#define PA_HOLD_FOL_MASK 0xF00
#define PA_HOLD_SIX_ZONE_MASK 0xF000
#define PA_DSPP_OP_ENABLE BIT(20)
#define PA_DSPP_OP_HUE_MASK BIT(25)
#define PA_DSPP_OP_SAT_MASK BIT(26)
#define PA_DSPP_OP_VAL_MASK BIT(27)
#define PA_DSPP_OP_CONT_MASK BIT(28)
#define PA_DSPP_OP_SAT_ZERO_EXP_EN BIT(1)
#define PA_DSPP_OP_SIX_ZONE_HUE_MASK BIT(29)
#define PA_DSPP_OP_SIX_ZONE_SAT_MASK BIT(30)
#define PA_DSPP_OP_SIX_ZONE_VAL_MASK BIT(31)
#define PA_DSPP_OP_MEM_COL_SKIN_MASK BIT(5)
#define PA_DSPP_OP_MEM_COL_FOL_MASK BIT(6)
#define PA_DSPP_OP_MEM_COL_SKY_MASK BIT(7)
#define PA_DSPP_OP_MEM_PROT_HUE_EN BIT(22)
#define PA_DSPP_OP_MEM_PROT_SAT_EN BIT(23)
#define PA_DSPP_OP_MEM_PROT_VAL_EN BIT(24)
#define PA_DSPP_OP_MEM_PROT_CONT_EN BIT(18)
#define PA_DSPP_OP_MEM_PROT_BLEND_EN BIT(3)
#define PA_DSPP_OP_MEM_PROT_SIX_EN BIT(17)
#define PA_VIG_OP_HUE_MASK BIT(25)
#define PA_VIG_OP_SAT_MASK BIT(26)
#define PA_VIG_OP_VAL_MASK BIT(27)
#define PA_VIG_OP_CONT_MASK BIT(28)
#define PA_VIG_OP_MEM_PROT_HUE_EN BIT(12)
#define PA_VIG_OP_MEM_PROT_SAT_EN BIT(13)
#define PA_VIG_OP_MEM_PROT_VAL_EN BIT(14)
#define PA_VIG_OP_MEM_PROT_CONT_EN BIT(15)
#define PA_VIG_OP_MEM_COL_SKIN_MASK BIT(5)
#define PA_VIG_OP_MEM_COL_FOL_MASK BIT(6)
#define PA_VIG_OP_MEM_COL_SKY_MASK BIT(7)
#define PA_VIG_OP_MEM_PROT_BLEND_EN BIT(1)
#define PA_VIG_OP_ENABLE BIT(4)
#define PA_VIG_OP_SAT_ZERO_EXP_EN BIT(2)

static struct mdss_pp_res_type_v1_7 config_data;

static int pp_hist_lut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_hist_lut_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_dither_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_dither_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
/* histogram prototypes */
static int pp_hist_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_get_hist_offset(u32 block, u32 *ctl_off);
static int pp_get_hist_isr(u32 *isr_mask);
static bool pp_is_sspp_hist_supp(void);

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side);

/* Gamut prototypes */
static int pp_gamut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_gamut_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
/* PCC prototypes */
static int pp_pcc_set_config(char __iomem *base_addr,
			struct pp_sts_type *pp_sts, void *cfg_data,
			u32 block_type);
static int pp_pcc_get_config(char __iomem *base_addr, void *cfg_data,
				u32 block_type, u32 disp_num);
/* PA prototypes */
static int pp_pa_set_config(char __iomem *base_addr,
			struct pp_sts_type *pp_sts, void *cfg_data,
			u32 block_type);
static int pp_pa_get_config(char __iomem *base_addr, void *cfg_data,
				u32 block_type, u32 disp_num);
static void pp_pa_update_dspp_opmode(int pa_sts, u32 *opmode);
static void pp_pa_update_vig_opmode(int pa_sts, u32 *opmode);

static int pp_igc_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_igc_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_pgc_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_pgc_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_pcc_get_version(u32 *version);
static int pp_igc_get_version(u32 *version);
static int pp_pgc_get_version(u32 *version);
static int pp_pa_get_version(u32 *version);
static int pp_gamut_get_version(u32 *version);
static int pp_dither_get_version(u32 *version);
static int pp_hist_lut_get_version(u32 *version);
static void pp_gamut_clock_gating_en(char __iomem *base_addr);

void *pp_get_driver_ops(struct mdp_pp_driver_ops *ops)
{
	if (!ops) {
		pr_err("PP driver ops invalid %p\n", ops);
		return ERR_PTR(-EINVAL);
	}

	/* IGC ops */
	ops->pp_ops[IGC].pp_set_config = pp_igc_set_config;
	ops->pp_ops[IGC].pp_get_config = pp_igc_get_config;
	ops->pp_ops[IGC].pp_get_version = pp_igc_get_version;

	/* PCC ops */
	ops->pp_ops[PCC].pp_set_config = pp_pcc_set_config;
	ops->pp_ops[PCC].pp_get_config = pp_pcc_get_config;
	ops->pp_ops[PCC].pp_get_version = pp_pcc_get_version;
	/* GC ops */
	ops->pp_ops[GC].pp_set_config = pp_pgc_set_config;
	ops->pp_ops[GC].pp_get_config = pp_pgc_get_config;
	ops->pp_ops[GC].pp_get_version = pp_pgc_get_version;

	/* PA ops */
	ops->pp_ops[PA].pp_set_config = pp_pa_set_config;
	ops->pp_ops[PA].pp_get_config = pp_pa_get_config;
	ops->pp_ops[PA].pp_get_version = pp_pa_get_version;

	/* Gamut ops */
	ops->pp_ops[GAMUT].pp_set_config = pp_gamut_set_config;
	ops->pp_ops[GAMUT].pp_get_config = pp_gamut_get_config;
	ops->pp_ops[GAMUT].pp_get_version = pp_gamut_get_version;

	/* Dither ops */
	ops->pp_ops[DITHER].pp_set_config = pp_dither_set_config;
	ops->pp_ops[DITHER].pp_get_config = pp_dither_get_config;
	ops->pp_ops[DITHER].pp_get_version = pp_dither_get_version;

	/* QSEED ops */
	ops->pp_ops[QSEED].pp_set_config = NULL;
	ops->pp_ops[QSEED].pp_get_config = NULL;
	ops->pp_ops[QSEED].pp_get_version = NULL;

	/* HIST_LUT ops */
	ops->pp_ops[HIST_LUT].pp_set_config = pp_hist_lut_set_config;
	ops->pp_ops[HIST_LUT].pp_get_config = pp_hist_lut_get_config;
	ops->pp_ops[HIST_LUT].pp_get_version = pp_hist_lut_get_version;

	/* HIST ops */
	ops->pp_ops[HIST].pp_set_config = NULL;
	ops->pp_ops[HIST].pp_get_config = pp_hist_get_config;
	ops->pp_ops[HIST].pp_get_version = NULL;

	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;

	ops->get_hist_offset = pp_get_hist_offset;
	ops->get_hist_isr_info = pp_get_hist_isr;
	ops->is_sspp_hist_supp = pp_is_sspp_hist_supp;
	ops->gamut_clk_gate_en = pp_gamut_clock_gating_en;
	return &config_data;
}

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side)
{
	if (!pp_sts || !opmode) {
		pr_err("Invalid pp_sts %p or opmode %p\n", pp_sts, opmode);
		return;
	}
	switch (location) {
	case SSPP_RGB:
		break;
	case SSPP_DMA:
		break;
	case SSPP_VIG:
		if (pp_sts->pa_sts & PP_STS_ENABLE)
			pp_pa_update_vig_opmode(pp_sts->pa_sts, opmode);
		if (pp_sts->enhist_sts & PP_STS_ENABLE) {
			*opmode |= HIST_LUT_VIG_OP_ENABLE |
				  PA_VIG_OP_ENABLE;
			if (pp_sts->enhist_sts & PP_STS_PA_LUT_FIRST)
				*opmode |= HIST_LUT_VIG_OP_FIRST_EN;
		}
		break;
	case DSPP:
		if (pp_sts_is_enabled(pp_sts->pa_sts, side))
			pp_pa_update_dspp_opmode(pp_sts->pa_sts, opmode);
		if (pp_sts_is_enabled(pp_sts->igc_sts, side))
			*opmode |= IGC_DSPP_OP_MODE_EN;
		if (pp_sts->enhist_sts & PP_STS_ENABLE) {
			*opmode |= HIST_LUT_DSPP_OP_ENABLE |
				  PA_DSPP_OP_ENABLE;
			if (pp_sts->enhist_sts & PP_STS_PA_LUT_FIRST)
				*opmode |= HIST_LUT_DSPP_OP_FIRST_EN;
		}
		if (pp_sts_is_enabled(pp_sts->dither_sts, side))
			*opmode |= MDSS_MDP_DSPP_OP_DST_DITHER_EN;
		break;
	case LM:
		if (pp_sts->argc_sts & PP_STS_ENABLE)
			pr_debug("pgc in LM enabled\n");
		break;
	default:
		pr_err("Invalid block type %d\n", location);
		break;
	}
	return;
}

static int pp_hist_lut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{

	int ret = 0, i = 0;
	char __iomem *hist_addr;
	u32 sz = 0, temp = 0, *data = NULL;
	struct mdp_hist_lut_data_v1_7 *lut_data = NULL;
	struct mdp_hist_lut_data *lut_cfg_data = NULL;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}

	lut_cfg_data = (struct mdp_hist_lut_data *) cfg_data;
	if (!(lut_cfg_data->ops & MDP_PP_OPS_READ)) {
		pr_err("read ops not set for hist_lut %d\n", lut_cfg_data->ops);
		return 0;
	}
	if (lut_cfg_data->version != mdp_hist_lut_v1_7 ||
		!lut_cfg_data->cfg_payload) {
		pr_err("invalid hist_lut version %d payload %p\n",
		       lut_cfg_data->version, lut_cfg_data->cfg_payload);
		return -EINVAL;
	}
	lut_data = lut_cfg_data->cfg_payload;
	if (lut_data->len != ENHIST_LUT_ENTRIES) {
		pr_err("invalid hist_lut len %d", lut_data->len);
		return -EINVAL;
	}
	sz = ENHIST_LUT_ENTRIES * sizeof(u32);
	if (!access_ok(VERIFY_WRITE, lut_data->data, sz)) {
		pr_err("invalid lut address for hist_lut sz %d\n", sz);
		return -EFAULT;
	}

	switch (block_type) {
	case SSPP_VIG:
		hist_addr = base_addr + REG_SSPP_VIG_HIST_LUT_BASE;
		break;
	case DSPP:
		hist_addr = base_addr + REG_DSPP_HIST_LUT_BASE;
		break;
	default:
		pr_err("Invalid block type %d\n", block_type);
		ret = -EINVAL;
		break;
	}

	if (ret) {
		pr_err("Failed to read hist_lut table ret %d", ret);
		return ret;
	}

	data = kzalloc(sz, GFP_KERNEL);
	if (!data) {
		pr_err("allocation failed for hist_lut size %d\n", sz);
		return -ENOMEM;
	}

	for (i = 0; i < ENHIST_LUT_ENTRIES; i += 2) {
		temp = readl_relaxed(hist_addr);
		data[i] = temp & ENHIST_LOWER_VALUE_MASK;
		data[i + 1] =
			(temp & ENHIST_UPPER_VALUE_MASK) >> ENHIST_BIT_SHIFT;
		hist_addr += 4;
	}
	if (copy_to_user(lut_data->data, data, sz)) {
		pr_err("faild to copy the hist_lut back to user\n");
		ret = -EFAULT;
	}
	kfree(data);
	return ret;
}

static int pp_hist_lut_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	int ret = 0, i = 0;
	u32 temp = 0;
	struct mdp_hist_lut_data *lut_cfg_data = NULL;
	struct mdp_hist_lut_data_v1_7 *lut_data = NULL;
	char __iomem *hist_addr = NULL, *swap_addr = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}

	lut_cfg_data = (struct mdp_hist_lut_data *) cfg_data;
	if (lut_cfg_data->version != mdp_hist_lut_v1_7) {
		pr_err("invalid hist_lut version %d\n", lut_cfg_data->version);
		return -EINVAL;
	}

	if (lut_cfg_data->ops & MDP_PP_OPS_DISABLE) {
		pr_debug("Disable Hist LUT\n");
		goto bail_out;
	}

	if (!(lut_cfg_data->ops & ~(MDP_PP_OPS_READ))) {
		pr_err("only read ops set for lut\n");
		return ret;
	}
	if (!(lut_cfg_data->ops & MDP_PP_OPS_WRITE)) {
		pr_debug("non write ops set %d\n", lut_cfg_data->ops);
		goto bail_out;
	}
	lut_data = lut_cfg_data->cfg_payload;
	if (!lut_data) {
		pr_err("invalid hist_lut cfg_payload %p\n", lut_data);
		return -EINVAL;
	}

	if (lut_data->len != ENHIST_LUT_ENTRIES || !lut_data->data) {
		pr_err("invalid hist_lut len %d data %p\n",
		       lut_data->len, lut_data->data);
		return -EINVAL;
	}
	switch (block_type) {
	case SSPP_VIG:
		hist_addr = base_addr + REG_SSPP_VIG_HIST_LUT_BASE;
		swap_addr = base_addr +
			REG_SSPP_VIG_HIST_SWAP_BASE;
		break;
	case DSPP:
		hist_addr = base_addr + REG_DSPP_HIST_LUT_BASE;
		swap_addr = base_addr + REG_DSPP_HIST_SWAP_BASE;
		break;
	default:
		pr_err("Invalid block type %d\n", block_type);
		ret = -EINVAL;
		break;
	}
	if (ret) {
		pr_err("hist_lut table not updated ret %d", ret);
		return ret;
	}
	for (i = 0; i < ENHIST_LUT_ENTRIES; i += 2) {
		temp = (lut_data->data[i] & ENHIST_LOWER_VALUE_MASK) |
			((lut_data->data[i + 1] & ENHIST_LOWER_VALUE_MASK)
			 << ENHIST_BIT_SHIFT);

		writel_relaxed(temp, hist_addr);
		hist_addr += 4;
	}
	if (lut_cfg_data->hist_lut_first)
		pp_sts->enhist_sts |= PP_STS_PA_LUT_FIRST;


	writel_relaxed(1, swap_addr);

bail_out:
	if (lut_cfg_data->ops & MDP_PP_OPS_DISABLE)
		pp_sts->enhist_sts &= ~PP_STS_ENABLE;
	else if (lut_cfg_data->ops & MDP_PP_OPS_ENABLE)
		pp_sts->enhist_sts |= PP_STS_ENABLE;

	return ret;
}

static int pp_dither_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	pr_err("Operation not supported\n");
	return -ENOTSUPP;
}

static int pp_dither_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	int i = 0;
	u32 data;
	struct mdp_dither_cfg_data *dither_cfg_data = NULL;
	struct mdp_dither_data_v1_7 *dither_data = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}

	dither_cfg_data = (struct mdp_dither_cfg_data *) cfg_data;

	if (dither_cfg_data->version != mdp_dither_v1_7) {
		pr_err("invalid dither version %d\n", dither_cfg_data->version);
		return -EINVAL;
	}
	if (dither_cfg_data->flags & MDP_PP_OPS_READ) {
		pr_err("Invalid context for read operation\n");
		return -EINVAL;
	}
	if (dither_cfg_data->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("set disable dither\n");
		goto bail_out;
	}

	if (!(dither_cfg_data->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("non write ops set %d\n", dither_cfg_data->flags);
		goto bail_out;
	}

	dither_data = dither_cfg_data->cfg_payload;
	if (!dither_data) {
		pr_err("invalid payload for dither %p\n", dither_data);
		return -EINVAL;
	}

	if ((dither_data->g_y_depth >= DITHER_DEPTH_MAP_INDEX) ||
		(dither_data->b_cb_depth >= DITHER_DEPTH_MAP_INDEX) ||
		(dither_data->r_cr_depth >= DITHER_DEPTH_MAP_INDEX)) {
		pr_err("invalid data for dither, g_y_depth %d y_cb_depth %d r_cr_depth %d\n",
			dither_data->g_y_depth, dither_data->b_cb_depth,
			dither_data->r_cr_depth);
		return -EINVAL;
	}
	data = dither_depth_map[dither_data->g_y_depth];
	data |= dither_depth_map[dither_data->b_cb_depth] << 2;
	data |= dither_depth_map[dither_data->r_cr_depth] << 4;
	data |= dither_cfg_data->mode << 8;
	writel_relaxed(data, base_addr);
	base_addr += DITHER_MATRIX_OFF;
	for (i = 0; i < DITHER_MATRIX_INDEX; i += 4) {
		data = dither_matrix[i] |
			(dither_matrix[i + 1] << 4) |
			(dither_matrix[i + 2] << 8) |
			(dither_matrix[i + 3] << 12);
		writel_relaxed(data, base_addr);
		base_addr += 4;
	}
bail_out:
	if (dither_cfg_data->flags & MDP_PP_OPS_DISABLE)
		pp_sts->dither_sts &= ~PP_STS_ENABLE;
	else if (dither_cfg_data->flags & MDP_PP_OPS_ENABLE)
		pp_sts->dither_sts |= PP_STS_ENABLE;
	pp_sts_set_split_bits(&pp_sts->dither_sts, dither_cfg_data->flags);

	return 0;
}

static int pp_hist_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	int ret = 0, i = 0;
	u32 sum = 0;
	struct pp_hist_col_info *hist_info = NULL;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}

	hist_info = (struct pp_hist_col_info *) cfg_data;

	switch (block_type) {
	case SSPP_VIG:
		base_addr += HIST_DATA_OFF_SSPP_V1_7;
		break;
	case DSPP:
		base_addr += HIST_DATA_OFF_DSPP_V1_7;
		break;
	default:
		pr_err("Invalid block type %d\n", block_type);
		ret = -EINVAL;
		break;
	}
	if (ret) {
		pr_err("Failed to read hist data ret %d\n", ret);
		return ret;
	}

	for (i = 0; i < HIST_V_SIZE; i++) {
		hist_info->data[i] = readl_relaxed(base_addr) & HIST_DATA_MASK;
		base_addr += 0x4;
		sum += hist_info->data[i];
	}
	hist_info->hist_cnt_read++;
	return sum;
}

static int pp_get_hist_offset(u32 block, u32 *ctl_off)
{
	int ret = 0;

	if (!ctl_off) {
		pr_err("invalid params ctl_off %p\n", ctl_off);
		return -EINVAL;
	}
	switch (block) {
	case SSPP_VIG:
		*ctl_off = HIST_CTL_OFF_SSPP_V1_7;
		break;
	case DSPP:
		*ctl_off = HIST_CTL_OFF_DSPP_V1_7;
		break;
	default:
		pr_err("Invalid block type %d\n", block);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_get_hist_isr(u32 *isr_mask)
{
	if (!isr_mask) {
		pr_err("invalid params isr_mask %p\n", isr_mask);
		return -EINVAL;
	}

	*isr_mask = HIST_V3_INTR_BIT_MASK;
	return 0;
}

static bool pp_is_sspp_hist_supp(void)
{
	return false;
}

static int pp_gamut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	u32 val = 0, sz = 0, sz_scale = 0, mode = 0, tbl_sz = 0;
	u32 index_start = 0;
	int i = 0, j = 0, ret = 0;
	u32 *gamut_tbl = NULL, *gamut_c0 = NULL, *gamut_c1c2 = NULL;
	struct mdp_gamut_cfg_data *gamut_cfg = (struct mdp_gamut_cfg_data *)
						cfg_data;
	struct mdp_gamut_data_v1_7 gamut_data;
	u32 clk_gate_disable = 0;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}
	if (gamut_cfg->version != mdp_gamut_v1_7) {
		pr_err("unsupported version of gamut %d\n",
		       gamut_cfg->version);
		return -EINVAL;
	}
	if (copy_from_user(&gamut_data, gamut_cfg->cfg_payload,
			   sizeof(gamut_data))) {
		pr_err("copy from user failed for gamut_data\n");
		return -EFAULT;
	}
	mode = readl_relaxed(base_addr + GAMUT_OP_MODE_OFF);
	if (mode & GAMUT_COARSE_EN) {
		tbl_sz = MDP_GAMUT_TABLE_V1_7_COARSE_SZ;
		sz = tbl_sz * sizeof(u32);
		index_start = GAMUT_COARSE_INDEX;
		gamut_data.mode = mdp_gamut_coarse_mode;
	} else {
		tbl_sz = MDP_GAMUT_TABLE_V1_7_SZ;
		sz = tbl_sz * sizeof(u32);
		index_start = GAMUT_FINE_INDEX;
		gamut_data.mode = mdp_gamut_fine_mode;
	}
	gamut_data.map_en = mode & GAMUT_MAP_EN;
	sz_scale = MDP_GAMUT_SCALE_OFF_SZ * sizeof(u32);
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		if (!access_ok(VERIFY_WRITE, gamut_data.c0_data[i], sz)) {
			pr_err("invalid c0 address for sz %d table index %d\n",
				sz, (i+1));
			return -EFAULT;
		}
		if (!access_ok(VERIFY_WRITE, gamut_data.c1_c2_data[i], sz)) {
			pr_err("invalid c1c2 address for sz %d table index %d\n",
				sz, (i+1));
			return -EFAULT;
		}
		gamut_data.tbl_size[i] = tbl_sz;
		if (i < MDP_GAMUT_SCALE_OFF_TABLE_NUM) {
			if (!access_ok(VERIFY_WRITE,
			     gamut_data.scale_off_data[i], sz_scale)) {
				pr_err("invalid scale address for sz %d color c%d\n",
					sz_scale, i);
				return -EFAULT;
			}
			gamut_data.tbl_scale_off_sz[i] =
			MDP_GAMUT_SCALE_OFF_SZ;
		}
	}
	/* allocate for c0 and c1c2 tables */
	gamut_tbl = kzalloc((sz * 2) , GFP_KERNEL);
	if (!gamut_tbl) {
		pr_err("failed to alloc table of sz %d\n", sz);
		return -ENOMEM;
	}
	gamut_c0 = gamut_tbl;
	gamut_c1c2 = gamut_c0 + tbl_sz;
	writel_relaxed(GAMUT_CLK_GATING_INACTIVE, base_addr + GAMUT_CLK_CTRL);
	clk_gate_disable = 1;
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		val = index_start;
		val |= GAMUT_READ_TABLE_EN;
		val |= GAMUT_TABLE_SELECT(i);
		writel_relaxed(val, (base_addr + GAMUT_TABLE_INDEX));
		for (j = 0; j < tbl_sz; j++) {
			gamut_c1c2[j] = readl_relaxed(base_addr +
					GAMUT_TABLE_LOWER_GB);
			gamut_c0[j] = readl_relaxed(base_addr +
				      GAMUT_TABLE_UPPER_R);
		}
		if (copy_to_user(gamut_data.c0_data[i], gamut_c0, sz)) {
			pr_err("copy to user failed for table %d c0 sz %d\n",
			       i, sz);
			ret = -EFAULT;
			goto bail_out;
		}
		if (copy_to_user(gamut_data.c1_c2_data[i], gamut_c1c2, sz)) {
			pr_err("copy to user failed for table %d c1c2 sz %d\n",
			       i, sz);
			ret = -EFAULT;
			goto bail_out;
		}
	}
	sz_scale = MDP_GAMUT_SCALE_OFF_TABLE_NUM * MDP_GAMUT_SCALE_OFF_SZ
		   * sizeof(u32);
	if (sz < sz_scale) {
		kfree(gamut_tbl);
		gamut_tbl = kzalloc(sz_scale , GFP_KERNEL);
		if (!gamut_tbl) {
			pr_err("failed to alloc scale tbl size %d\n",
			       sz_scale);
			ret = -ENOMEM;
			goto bail_out;
		}
	}
	gamut_c0 = gamut_tbl;
	base_addr += GAMUT_C0_SCALE_OFF;
	for (i = 0;
	     i < (MDP_GAMUT_SCALE_OFF_TABLE_NUM * MDP_GAMUT_SCALE_OFF_SZ);
	     i++) {
		gamut_c0[i] = readl_relaxed(base_addr);
		base_addr += 4;
	}
	for (i = 0; i < MDP_GAMUT_SCALE_OFF_TABLE_NUM; i++) {
		if (copy_to_user(gamut_data.scale_off_data[i],
				 &gamut_c0[i * MDP_GAMUT_SCALE_OFF_SZ],
				 (MDP_GAMUT_SCALE_OFF_SZ * sizeof(u32)))) {
			pr_err("copy to user failed for scale color c%d\n",
			       i);
			ret = -EFAULT;
			goto bail_out;
		}
	}
	if (copy_to_user(gamut_cfg->cfg_payload, &gamut_data,
			 sizeof(gamut_data))) {
		pr_err("failed to copy the gamut info into payload\n");
		ret = -EFAULT;
	}
bail_out:
	if (clk_gate_disable)
		writel_relaxed(GAMUT_CLK_GATING_PARTIAL_ACTIVE,
					   base_addr + GAMUT_CLK_CTRL);
	kfree(gamut_tbl);
	return ret;
}

static int pp_gamut_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	int val = 0, ret = 0, i = 0, j = 0;
	u32 index_start = 0, tbl_sz = 0;
	struct mdp_gamut_cfg_data *gamut_cfg_data = NULL;
	struct mdp_gamut_data_v1_7 *gamut_data = NULL;
	char __iomem *base_addr_scale = base_addr;
	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}
	gamut_cfg_data = (struct mdp_gamut_cfg_data *) cfg_data;
	if (gamut_cfg_data->version != mdp_gamut_v1_7) {
		pr_err("invalid gamut version %d\n", gamut_cfg_data->version);
		return -EINVAL;
	}
	if (!(gamut_cfg_data->flags & ~(MDP_PP_OPS_READ))) {
		pr_debug("only read ops is set %d", gamut_cfg_data->flags);
		return 0;
	}

	if (gamut_cfg_data->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("disabling gamut\n");
		goto bail_out;
	}

	gamut_data = (struct mdp_gamut_data_v1_7 *)
		      gamut_cfg_data->cfg_payload;
	if (!gamut_data) {
		pr_err("invalid payload for gamut %p\n", gamut_data);
		return -EINVAL;
	}

	if (gamut_data->mode != mdp_gamut_fine_mode &&
	    gamut_data->mode != mdp_gamut_coarse_mode) {
		pr_err("invalid gamut mode %d", gamut_data->mode);
		return -EINVAL;
	}
	index_start = (gamut_data->mode == mdp_gamut_fine_mode) ?
		       GAMUT_FINE_INDEX : GAMUT_COARSE_INDEX;
	tbl_sz = (gamut_data->mode == mdp_gamut_fine_mode) ?
		  MDP_GAMUT_TABLE_V1_7_SZ : MDP_GAMUT_TABLE_V1_7_COARSE_SZ;
	if (!(gamut_cfg_data->flags & MDP_PP_OPS_WRITE))
		goto bail_out;
	/* Sanity check for all tables */
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		if (!gamut_data->c0_data[i] || !gamut_data->c1_c2_data[i]
		    || (gamut_data->tbl_size[i] != tbl_sz)) {
			pr_err("invalid param for c0 %p c1c2 %p table %d size %d expected sz %d\n",
			       gamut_data->c0_data[i],
			       gamut_data->c1_c2_data[i], i,
			       gamut_data->tbl_size[i], tbl_sz);
			ret = -EINVAL;
			goto bail_out;
		}
		if (i < MDP_GAMUT_SCALE_OFF_TABLE_NUM &&
		    (!gamut_data->scale_off_data[i] ||
		    (gamut_data->tbl_scale_off_sz[i] !=
		    MDP_GAMUT_SCALE_OFF_SZ))) {
			pr_err("invalid param for scale table %p for c%d size %d expected size%d\n",
				gamut_data->scale_off_data[i], i,
				gamut_data->tbl_scale_off_sz[i],
				MDP_GAMUT_SCALE_OFF_SZ);
			ret = -EINVAL;
			goto bail_out;
		}
	}
	base_addr_scale += GAMUT_C0_SCALE_OFF;
	writel_relaxed(GAMUT_CLK_GATING_INACTIVE, base_addr + GAMUT_CLK_CTRL);
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		val = index_start;
		val |= GAMUT_TABLE_SELECT(i);
		writel_relaxed(val, (base_addr + GAMUT_TABLE_INDEX));
		for (j = 0; j < gamut_data->tbl_size[i]; j++) {
			writel_relaxed(gamut_data->c1_c2_data[i][j],
				       base_addr + GAMUT_TABLE_LOWER_GB);
			writel_relaxed(gamut_data->c0_data[i][j],
				      base_addr + GAMUT_TABLE_UPPER_R);
		}
		if ((i >= MDP_GAMUT_SCALE_OFF_TABLE_NUM) ||
				(!gamut_data->map_en))
			continue;
		for (j = 0; j < MDP_GAMUT_SCALE_OFF_SZ; j++) {
			writel_relaxed((gamut_data->scale_off_data[i][j]),
				       base_addr_scale);
			base_addr_scale += 4;
		}
	}
	writel_relaxed(GAMUT_CLK_GATING_PARTIAL_ACTIVE,
				   base_addr + GAMUT_CLK_CTRL);
bail_out:
	if (!ret) {
		val = 0;
		pp_sts_set_split_bits(&pp_sts->gamut_sts,
				gamut_cfg_data->flags);
		if (gamut_cfg_data->flags & MDP_PP_OPS_DISABLE) {
			pp_sts->gamut_sts &= ~PP_STS_ENABLE;
			writel_relaxed(val, base_addr + GAMUT_OP_MODE_OFF);
		} else if (gamut_cfg_data->flags & MDP_PP_OPS_ENABLE) {
			pp_sts->gamut_sts |= PP_STS_ENABLE;
			if (pp_sts_is_enabled(pp_sts->gamut_sts,
					      pp_sts->side_sts)) {
				if (gamut_data->mode == mdp_gamut_coarse_mode)
					val |= GAMUT_COARSE_EN;
				if (gamut_data->map_en)
					val |= GAMUT_MAP_EN;
				val |= GAMUT_ENABLE;
			}
			writel_relaxed(val, base_addr + GAMUT_OP_MODE_OFF);
		}
	}
	return ret;
}

static int pp_pcc_set_config(char __iomem *base_addr,
			struct pp_sts_type *pp_sts, void *cfg_data,
			u32 block_type)
{
	struct mdp_pcc_cfg_data *pcc_cfg_data = NULL;
	struct mdp_pcc_data_v1_7 *pcc_data = NULL;
	char __iomem *addr = NULL;
	u32 opmode = 0;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts %p\n",
			base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}
	pcc_cfg_data = (struct mdp_pcc_cfg_data *) cfg_data;
	if (pcc_cfg_data->version != mdp_pcc_v1_7) {
		pr_err("invalid pcc version %d\n", pcc_cfg_data->version);
		return -EINVAL;
	}
	if (!(pcc_cfg_data->ops & ~(MDP_PP_OPS_READ))) {
		pr_info("only read ops is set %d", pcc_cfg_data->ops);
		return 0;
	}
	pcc_data = pcc_cfg_data->cfg_payload;
	if (!pcc_data) {
		pr_err("invalid payload for pcc %p\n", pcc_data);
		return -EINVAL;
	}

	if (!(pcc_cfg_data->ops & MDP_PP_OPS_WRITE))
		goto bail_out;

	addr = base_addr + PCC_CONST_COEFF_OFF;
	writel_relaxed(pcc_data->r.c & PCC_CONST_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.c & PCC_CONST_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.c & PCC_CONST_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_R_COEFF_OFF;
	writel_relaxed(pcc_data->r.r & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.r & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.r & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_G_COEFF_OFF;
	writel_relaxed(pcc_data->r.g & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.g & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.g & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_B_COEFF_OFF;
	writel_relaxed(pcc_data->r.b & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.b & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.b & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_RG_COEFF_OFF;
	writel_relaxed(pcc_data->r.rg & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.rg & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.rg & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_RB_COEFF_OFF;
	writel_relaxed(pcc_data->r.rb & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.rb & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.rb & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_GB_COEFF_OFF;
	writel_relaxed(pcc_data->r.gb & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.gb & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.gb & PCC_COEFF_MASK, addr + 8);

	addr = base_addr + PCC_RGB_COEFF_OFF;
	writel_relaxed(pcc_data->r.rgb & PCC_COEFF_MASK, addr);
	writel_relaxed(pcc_data->g.rgb & PCC_COEFF_MASK, addr + 4);
	writel_relaxed(pcc_data->b.rgb & PCC_COEFF_MASK, addr + 8);

bail_out:
	pp_sts_set_split_bits(&pp_sts->pcc_sts, pcc_cfg_data->ops);
	if (pcc_cfg_data->ops & MDP_PP_OPS_DISABLE) {
		writel_relaxed(opmode, base_addr + PCC_OP_MODE_OFF);
		pp_sts->pcc_sts &= ~PP_STS_ENABLE;
	} else if (pcc_cfg_data->ops & MDP_PP_OPS_ENABLE) {
		pp_sts->pcc_sts |= PP_STS_ENABLE;
		if (pp_sts_is_enabled(pp_sts->pcc_sts, pp_sts->side_sts))
			opmode |= PCC_ENABLE;
		writel_relaxed(opmode, base_addr + PCC_OP_MODE_OFF);
	}

	return 0;
}

static int pp_pcc_get_config(char __iomem *base_addr, void *cfg_data,
				u32 block_type, u32 disp_num)
{
	char __iomem *addr;
	struct mdp_pcc_cfg_data *pcc_cfg = NULL;
	struct mdp_pcc_data_v1_7 pcc_data;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}

	pcc_cfg = (struct mdp_pcc_cfg_data *) cfg_data;
	if (pcc_cfg->version != mdp_pcc_v1_7) {
		pr_err("unsupported version of pcc %d\n",
		       pcc_cfg->version);
		return -EINVAL;
	}

	addr = base_addr + PCC_CONST_COEFF_OFF;
	pcc_data.r.c = readl_relaxed(addr) & PCC_CONST_COEFF_MASK;
	pcc_data.g.c = readl_relaxed(addr + 4) & PCC_CONST_COEFF_MASK;
	pcc_data.b.c = readl_relaxed(addr + 8) & PCC_CONST_COEFF_MASK;

	addr = base_addr + PCC_R_COEFF_OFF;
	pcc_data.r.r = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.r = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.r = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_G_COEFF_OFF;
	pcc_data.r.g = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.g = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.g = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_B_COEFF_OFF;
	pcc_data.r.b = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.b = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.b = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_RG_COEFF_OFF;
	pcc_data.r.rg = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.rg = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.rg = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_RB_COEFF_OFF;
	pcc_data.r.rb = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.rb = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.rb = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_GB_COEFF_OFF;
	pcc_data.r.gb = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.gb = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.gb = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	addr = base_addr + PCC_RGB_COEFF_OFF;
	pcc_data.r.rgb = readl_relaxed(addr) & PCC_COEFF_MASK;
	pcc_data.g.rgb = readl_relaxed(addr + 4) & PCC_COEFF_MASK;
	pcc_data.b.rgb = readl_relaxed(addr + 8) & PCC_COEFF_MASK;

	if (copy_to_user(pcc_cfg->cfg_payload, &pcc_data,
			 sizeof(pcc_data))) {
		pr_err("failed to copy the pcc info into payload\n");
		return -EFAULT;
	}

	return 0;
}

static void pp_pa_set_global_adj_regs(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data, u32 flags,
				int block_type)
{
	char __iomem *addr = NULL;

	if (block_type == DSPP)
		addr = base_addr + PA_DSPP_GLOBAL_OFF;
	else
		addr = base_addr + PA_VIG_GLOBAL_OFF;

	if (flags & MDP_PP_PA_HUE_ENABLE)
		writel_relaxed((pa_data->global_hue_adj &
				PA_GLOBAL_HUE_MASK), addr);
	addr += 4;
	if (flags & MDP_PP_PA_SAT_ENABLE)
		writel_relaxed((pa_data->global_sat_adj &
				PA_GLOBAL_SAT_MASK), addr);
	addr += 4;
	if (flags & MDP_PP_PA_VAL_ENABLE)
		writel_relaxed((pa_data->global_val_adj &
				PA_GLOBAL_VAL_MASK), addr);
	addr += 4;
	if (flags & MDP_PP_PA_CONT_ENABLE)
		writel_relaxed((pa_data->global_cont_adj &
				PA_GLOBAL_CONT_MASK), addr);
}

static void pp_pa_set_mem_col_regs(char __iomem *mem_col_p0_addr,
				char __iomem *mem_col_p2_addr,
				struct mdp_pa_mem_col_data_v1_7 *mem_col_data)
{
	writel_relaxed((mem_col_data->color_adjust_p0 &
			PA_MEM_COL_ADJ_P0_MASK), mem_col_p0_addr);
	mem_col_p0_addr += 4;
	writel_relaxed(mem_col_data->color_adjust_p1, mem_col_p0_addr);
	mem_col_p0_addr += 4;
	writel_relaxed((mem_col_data->hue_region &
			PA_MEM_COL_HUE_REGION_MASK), mem_col_p0_addr);
	mem_col_p0_addr += 4;
	writel_relaxed((mem_col_data->sat_region &
			PA_MEM_COL_SAT_REGION_MASK), mem_col_p0_addr);
	mem_col_p0_addr += 4;
	writel_relaxed((mem_col_data->val_region &
			PA_MEM_COL_VAL_REGION_MASK), mem_col_p0_addr);

	writel_relaxed(mem_col_data->color_adjust_p2, mem_col_p2_addr);
	mem_col_p2_addr += 4;
	writel_relaxed(mem_col_data->blend_gain, mem_col_p2_addr);
}

static void pp_pa_set_mem_col(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data, u32 flags,
				int block_type, uint32_t *pa_hold,
				uint32_t *pa_hold_mask)
{
	uint32_t sat_hold = 0, val_hold = 0, mem_col_hold = 0;
	u32 skin_p0_off = 0, skin_p2_off = 0;
	u32 sky_p0_off = 0, sky_p2_off = 0;
	u32 fol_p0_off = 0, fol_p2_off = 0;
	char __iomem *mem_col_p0_addr = NULL;
	char __iomem *mem_col_p2_addr = NULL;
	if (block_type == DSPP) {
		skin_p0_off = PA_DSPP_MEM_COL_SKIN_P0_OFF;
		skin_p2_off = PA_DSPP_MEM_COL_SKIN_P2_OFF;
		sky_p0_off = PA_DSPP_MEM_COL_SKY_P0_OFF;
		sky_p2_off = PA_DSPP_MEM_COL_SKY_P2_OFF;
		fol_p0_off = PA_DSPP_MEM_COL_FOL_P0_OFF;
		fol_p2_off = PA_DSPP_MEM_COL_FOL_P2_OFF;
	} else {
		skin_p0_off = PA_VIG_MEM_COL_SKIN_P0_OFF;
		skin_p2_off = PA_VIG_MEM_COL_SKIN_P2_OFF;
		sky_p0_off = PA_VIG_MEM_COL_SKY_P0_OFF;
		sky_p2_off = PA_VIG_MEM_COL_SKY_P2_OFF;
		fol_p0_off = PA_VIG_MEM_COL_FOL_P0_OFF;
		fol_p2_off = PA_VIG_MEM_COL_FOL_P2_OFF;
	}
	/* Update skin zone memory color registers */
	if (flags & MDP_PP_PA_SKIN_ENABLE) {
		mem_col_p0_addr = base_addr + skin_p0_off;
		mem_col_p2_addr = base_addr + skin_p2_off;
		pp_pa_set_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->skin_cfg);
		sat_hold = (pa_data->skin_cfg.sat_hold & PA_HOLD_MASK) <<
			    PA_HOLD_SAT_SHIFT;
		val_hold = (pa_data->skin_cfg.val_hold & PA_HOLD_MASK) <<
			    PA_HOLD_VAL_SHIFT;
		mem_col_hold = (sat_hold | val_hold) << PA_HOLD_SKIN_SHIFT;
		*pa_hold |= mem_col_hold;
		*pa_hold_mask |= PA_HOLD_SKIN_MASK;
	}
	/* Update sky zone memory color registers */
	if (flags & MDP_PP_PA_SKY_ENABLE) {
		mem_col_p0_addr = base_addr + sky_p0_off;
		mem_col_p2_addr = base_addr + sky_p2_off;
		pp_pa_set_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->sky_cfg);
		sat_hold = (pa_data->sky_cfg.sat_hold & PA_HOLD_MASK) <<
			    PA_HOLD_SAT_SHIFT;
		val_hold = (pa_data->sky_cfg.val_hold & PA_HOLD_MASK) <<
			    PA_HOLD_VAL_SHIFT;
		mem_col_hold = (sat_hold | val_hold) << PA_HOLD_SKY_SHIFT;
		*pa_hold |= mem_col_hold;
		*pa_hold_mask |= PA_HOLD_SKY_MASK;
	}
	/* Update foliage zone memory color registers */
	if (flags & MDP_PP_PA_FOL_ENABLE) {
		mem_col_p0_addr = base_addr + fol_p0_off;
		mem_col_p2_addr = base_addr + fol_p2_off;
		pp_pa_set_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->fol_cfg);
		sat_hold = (pa_data->fol_cfg.sat_hold & PA_HOLD_MASK) <<
			    PA_HOLD_SAT_SHIFT;
		val_hold = (pa_data->fol_cfg.val_hold & PA_HOLD_MASK) <<
			    PA_HOLD_VAL_SHIFT;
		mem_col_hold = (sat_hold | val_hold) << PA_HOLD_FOL_SHIFT;
		*pa_hold |= mem_col_hold;
		*pa_hold_mask |= PA_HOLD_FOL_MASK;
	}
}

static void pp_pa_set_six_zone(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data,
				u32 flags,
				uint32_t *pa_hold,
				uint32_t *pa_hold_mask)
{
	u32 data, i;
	char __iomem *addr = base_addr + PA_SIX_ZONE_LUT_OFF;
	uint32_t sat_hold = 0, val_hold = 0, mem_col_hold = 0;
	/* Update six zone memory color registers */
	if (!(flags & MDP_PP_PA_SIX_ZONE_ENABLE))
		return;

	if (!pa_data->six_zone_len || !pa_data->six_zone_curve_p0 ||
	    !pa_data->six_zone_curve_p1) {
		pr_err("Invalid six zone data: len %d curve_p0 %p curve_p1 %p\n",
		       pa_data->six_zone_len,
		       pa_data->six_zone_curve_p0,
		       pa_data->six_zone_curve_p1);
		return;
	}

	writel_relaxed((pa_data->six_zone_curve_p1[0] &
			PA_SIX_ZONE_CURVE_P1_MASK), addr + 4);
	/* Index Update to trigger auto-incrementing LUT accesses */
	data = PA_SIX_ZONE_INDEX_UPDATE;
	writel_relaxed((pa_data->six_zone_curve_p0[0] &
			PA_SIX_ZONE_CURVE_P0_MASK) | data, addr);

	/* Remove Index Update */
	for (i = 1; i < MDP_SIX_ZONE_LUT_SIZE; i++) {
		writel_relaxed((pa_data->six_zone_curve_p1[i] &
				PA_SIX_ZONE_CURVE_P1_MASK), addr + 4);
		writel_relaxed((pa_data->six_zone_curve_p0[i] &
				PA_SIX_ZONE_CURVE_P0_MASK), addr);
	}
	addr = base_addr + PA_SIX_ZONE_REGION_OFF;
	writel_relaxed(pa_data->six_zone_thresh, addr);

	addr = base_addr + PA_SIX_ZONE_ADJ_OFF;
	writel_relaxed((pa_data->six_zone_adj_p0 &
			PA_SIX_ZONE_ADJ_P0_MASK), addr);
	addr += 4;
	writel_relaxed(pa_data->six_zone_adj_p1, addr);

	sat_hold = (pa_data->six_zone_sat_hold & PA_HOLD_MASK) <<
		    PA_HOLD_SAT_SHIFT;
	val_hold = (pa_data->six_zone_val_hold & PA_HOLD_MASK) <<
		    PA_HOLD_VAL_SHIFT;
	mem_col_hold = (sat_hold | val_hold) << PA_HOLD_SIX_ZONE_SHIFT;
	*pa_hold |= mem_col_hold;
	*pa_hold_mask |= PA_HOLD_SIX_ZONE_MASK;
}

static void pp_pa_set_sts(struct pp_sts_type *pp_sts,
				struct mdp_pa_data_v1_7 *pa_data,
				int enable_flag,
				int block_type)
{
	pp_sts->pa_sts = 0;

	if (enable_flag & MDP_PP_OPS_ENABLE)
		pp_sts->pa_sts |= PP_STS_ENABLE;
	/* Disable takes priority over all flags */
	if (enable_flag & MDP_PP_OPS_DISABLE) {
		pp_sts->pa_sts &= ~PP_STS_ENABLE;
		return;
	}

	if (!pa_data) {
		pr_err("PA cfg payload is null, enable flag %d\n", enable_flag);
		return;
	}

	/* Global HSV STS update */
	if (pa_data->mode & MDP_PP_PA_HUE_MASK)
		pp_sts->pa_sts |= PP_STS_PA_HUE_MASK;
	if (pa_data->mode & MDP_PP_PA_SAT_MASK)
		pp_sts->pa_sts |= PP_STS_PA_SAT_MASK;
	if (pa_data->mode & MDP_PP_PA_VAL_MASK)
		pp_sts->pa_sts |= PP_STS_PA_VAL_MASK;
	if (pa_data->mode & MDP_PP_PA_CONT_MASK)
		pp_sts->pa_sts |= PP_STS_PA_CONT_MASK;
	if (pa_data->mode & MDP_PP_PA_SAT_ZERO_EXP_EN)
		pp_sts->pa_sts |= PP_STS_PA_SAT_ZERO_EXP_EN;

	/* Memory Protect STS update */
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_HUE_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_HUE_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_SAT_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_SAT_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_VAL_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_VAL_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_CONT_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_CONT_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_BLEND_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_BLEND_EN;
	if ((block_type == DSPP) &&
			(pa_data->mode & MDP_PP_PA_MEM_PROT_SIX_EN))
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_SIX_EN;

	/* Memory Color STS update */
	if (pa_data->mode & MDP_PP_PA_MEM_COL_SKIN_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_SKIN_MASK;
	if (pa_data->mode & MDP_PP_PA_MEM_COL_SKY_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_SKY_MASK;
	if (pa_data->mode & MDP_PP_PA_MEM_COL_FOL_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_FOL_MASK;

	/* Six Zone STS update */
	if (block_type == DSPP) {
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_HUE_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_HUE_MASK;
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_SAT_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_SAT_MASK;
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_VAL_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_VAL_MASK;

		pp_sts_set_split_bits(&pp_sts->pa_sts, enable_flag);
	}
}

static int pp_pa_set_config(char __iomem *base_addr,
			struct pp_sts_type *pp_sts, void *cfg_data,
			u32 block_type)
{
	struct mdp_pa_v2_cfg_data *pa_cfg_data = NULL;
	struct mdp_pa_data_v1_7 *pa_data = NULL;
	uint32_t pa_hold = 0, pa_hold_mask = 0, pa_hold_tmp;
	char __iomem *pa_hold_addr = NULL;
	int ret = 0;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}
	if ((block_type != DSPP) && (block_type != SSPP_VIG)) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	pa_cfg_data = (struct mdp_pa_v2_cfg_data *) cfg_data;
	if (pa_cfg_data->version != mdp_pa_v1_7) {
		pr_err("invalid pa version %d\n", pa_cfg_data->version);
		return -EINVAL;
	}
	if (!(pa_cfg_data->flags & ~(MDP_PP_OPS_READ))) {
		pr_info("only read ops is set %d", pa_cfg_data->flags);
		return 0;
	}
	if (pa_cfg_data->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("Disable PA");
		goto pa_set_sts;
	}

	pa_data = pa_cfg_data->cfg_payload;
	if (!pa_data) {
		pr_err("invalid payload for pa %p\n", pa_data);
		return -EINVAL;
	}

	if (!(pa_cfg_data->flags & MDP_PP_OPS_WRITE)) {
		pr_warn("No write flag enabled for PA flags %d\n",
			pa_cfg_data->flags);
		return 0;
	}

	pp_pa_set_global_adj_regs(base_addr, pa_data, pa_cfg_data->flags,
			block_type);
	pp_pa_set_mem_col(base_addr, pa_data, pa_cfg_data->flags,
			block_type, &pa_hold, &pa_hold_mask);
	if (block_type == DSPP)
		pp_pa_set_six_zone(base_addr, pa_data, pa_cfg_data->flags,
				   &pa_hold, &pa_hold_mask);

	/*
	 * Only modify the PA hold bits for PA features that have
	 * been updated.
	 */
	if (block_type == DSPP)
		pa_hold_addr = base_addr + PA_DSPP_HOLD_OFF;
	else
		pa_hold_addr = base_addr + PA_VIG_HOLD_OFF;
	pa_hold_tmp = readl_relaxed(pa_hold_addr);
	pa_hold_tmp &= ~pa_hold_mask;
	pa_hold |= pa_hold_tmp;
	writel_relaxed(pa_hold, pa_hold_addr);

pa_set_sts:
	pp_pa_set_sts(pp_sts, pa_data, pa_cfg_data->flags, block_type);

	return ret;
}

static void pp_pa_get_global_adj_regs(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data, u32 flags,
				int block_type)
{
	char __iomem *addr = NULL;

	if (block_type == DSPP)
		addr = base_addr + PA_DSPP_GLOBAL_OFF;
	else
		addr = base_addr + PA_VIG_GLOBAL_OFF;

	if (flags & MDP_PP_PA_HUE_ENABLE)
		pa_data->global_hue_adj = readl_relaxed(addr) &
					  PA_GLOBAL_HUE_MASK;
	addr += 4;
	if (flags & MDP_PP_PA_SAT_ENABLE)
		pa_data->global_sat_adj = readl_relaxed(addr) &
					  PA_GLOBAL_SAT_MASK;
	addr += 4;
	if (flags & MDP_PP_PA_VAL_ENABLE)
		pa_data->global_val_adj = readl_relaxed(addr) &
					  PA_GLOBAL_VAL_MASK;
	addr += 4;
	if (flags & MDP_PP_PA_CONT_ENABLE)
		pa_data->global_cont_adj = readl_relaxed(addr) &
					  PA_GLOBAL_CONT_MASK;
}

static void pp_pa_get_mem_col_regs(char __iomem *mem_col_p0_addr,
				char __iomem *mem_col_p2_addr,
				struct mdp_pa_mem_col_data_v1_7 *mem_col_data)
{
	mem_col_data->color_adjust_p0 = readl_relaxed(mem_col_p0_addr) &
					PA_MEM_COL_ADJ_P0_MASK;
	mem_col_p0_addr += 4;
	mem_col_data->color_adjust_p1 = readl_relaxed(mem_col_p0_addr);
	mem_col_p0_addr += 4;
	mem_col_data->hue_region = readl_relaxed(mem_col_p0_addr) &
				   PA_MEM_COL_HUE_REGION_MASK;
	mem_col_p0_addr += 4;
	mem_col_data->sat_region = readl_relaxed(mem_col_p0_addr) &
				   PA_MEM_COL_SAT_REGION_MASK;
	mem_col_p0_addr += 4;
	mem_col_data->val_region = readl_relaxed(mem_col_p0_addr) &
				   PA_MEM_COL_VAL_REGION_MASK;

	mem_col_data->color_adjust_p2 = readl_relaxed(mem_col_p2_addr);
	mem_col_p2_addr += 4;
	mem_col_data->blend_gain = readl_relaxed(mem_col_p2_addr);
}

static void pp_pa_get_mem_col(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data, u32 flags,
				int block_type,
				uint32_t pa_hold)
{
	uint32_t mem_col_hold = 0;
	u32 skin_p0_off = 0, skin_p2_off = 0;
	u32 sky_p0_off = 0, sky_p2_off = 0;
	u32 fol_p0_off = 0, fol_p2_off = 0;
	char __iomem *mem_col_p0_addr = NULL;
	char __iomem *mem_col_p2_addr = NULL;
	if (block_type == DSPP) {
		skin_p0_off = PA_DSPP_MEM_COL_SKIN_P0_OFF;
		skin_p2_off = PA_DSPP_MEM_COL_SKIN_P2_OFF;
		sky_p0_off = PA_DSPP_MEM_COL_SKY_P0_OFF;
		sky_p2_off = PA_DSPP_MEM_COL_SKY_P2_OFF;
		fol_p0_off = PA_DSPP_MEM_COL_FOL_P0_OFF;
		fol_p2_off = PA_DSPP_MEM_COL_FOL_P2_OFF;
	} else {
		skin_p0_off = PA_VIG_MEM_COL_SKIN_P0_OFF;
		skin_p2_off = PA_VIG_MEM_COL_SKIN_P2_OFF;
		sky_p0_off = PA_VIG_MEM_COL_SKY_P0_OFF;
		sky_p2_off = PA_VIG_MEM_COL_SKY_P2_OFF;
		fol_p0_off = PA_VIG_MEM_COL_FOL_P0_OFF;
		fol_p2_off = PA_VIG_MEM_COL_FOL_P2_OFF;
	}
	/* Update skin zone memory color registers */
	if (flags & MDP_PP_PA_SKIN_ENABLE) {
		mem_col_p0_addr = base_addr + skin_p0_off;
		mem_col_p2_addr = base_addr + skin_p2_off;
		pp_pa_get_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->skin_cfg);
		mem_col_hold = pa_hold >> PA_HOLD_SKIN_SHIFT;
		pa_data->skin_cfg.sat_hold = (mem_col_hold >>
				PA_HOLD_SAT_SHIFT) & PA_HOLD_MASK;
		pa_data->skin_cfg.val_hold = (mem_col_hold >>
				PA_HOLD_VAL_SHIFT) & PA_HOLD_MASK;
	}
	/* Update sky zone memory color registers */
	if (flags & MDP_PP_PA_SKY_ENABLE) {
		mem_col_p0_addr = base_addr + sky_p0_off;
		mem_col_p2_addr = base_addr + sky_p2_off;
		pp_pa_get_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->sky_cfg);
		mem_col_hold = pa_hold >> PA_HOLD_SKY_SHIFT;
		pa_data->sky_cfg.sat_hold = (mem_col_hold >>
				PA_HOLD_SAT_SHIFT) & PA_HOLD_MASK;
		pa_data->sky_cfg.val_hold = (mem_col_hold >>
				PA_HOLD_VAL_SHIFT) & PA_HOLD_MASK;
	}
	/* Update foliage zone memory color registers */
	if (flags & MDP_PP_PA_FOL_ENABLE) {
		mem_col_p0_addr = base_addr + fol_p0_off;
		mem_col_p2_addr = base_addr + fol_p2_off;
		pp_pa_get_mem_col_regs(mem_col_p0_addr, mem_col_p2_addr,
				       &pa_data->fol_cfg);
		mem_col_hold = pa_hold >> PA_HOLD_FOL_SHIFT;
		pa_data->sky_cfg.sat_hold = (mem_col_hold >>
				PA_HOLD_SAT_SHIFT) & PA_HOLD_MASK;
		pa_data->sky_cfg.val_hold = (mem_col_hold >>
				PA_HOLD_VAL_SHIFT) & PA_HOLD_MASK;
	}
}

static int pp_pa_get_six_zone(char __iomem *base_addr,
				struct mdp_pa_data_v1_7 *pa_data, u32 flags,
				u32 pa_hold)
{
	uint32_t six_zone_sz = 0, six_zone_buf_sz = 0;
	u32 data = 0;
	char __iomem *addr = base_addr + PA_SIX_ZONE_LUT_OFF;
	uint32_t *six_zone_read_buf = NULL;
	uint32_t *six_zone_p0 = NULL, *six_zone_p1 = NULL;
	uint32_t six_zone_hold = 0;
	int ret = 0, i;

	if (pa_data->six_zone_len != MDP_SIX_ZONE_LUT_SIZE) {
		pr_err("Invalid six zone length %d\n",
			pa_data->six_zone_len);
		return -EINVAL;
	}
	six_zone_sz = pa_data->six_zone_len * sizeof(uint32_t);

	if (!access_ok(VERIFY_WRITE, pa_data->six_zone_curve_p0,
			six_zone_sz)) {
		pr_err("invalid six_zone_curve_p0 addr for sz %d\n",
			six_zone_sz);
		return -EFAULT;
	}
	if (!access_ok(VERIFY_WRITE, pa_data->six_zone_curve_p1,
			six_zone_sz)) {
		pr_err("invalid six_zone_curve_p1 addr for sz %d\n",
			six_zone_sz);
		return -EFAULT;
	}

	six_zone_buf_sz = 2 * six_zone_sz;
	six_zone_read_buf = kzalloc(six_zone_buf_sz, GFP_KERNEL);
	if (!six_zone_read_buf) {
		pr_err("allocation failed for six zone lut size %d\n",
			six_zone_buf_sz);
		ret = -ENOMEM;
		goto six_zone_exit;
	}
	six_zone_p0 = six_zone_read_buf;
	six_zone_p1 = &six_zone_read_buf[MDP_SIX_ZONE_LUT_SIZE];

	data = PA_SIX_ZONE_VALUE_UPDATE | PA_SIX_ZONE_INDEX_UPDATE;
	writel_relaxed(data, addr);

	for (i = 0; i < MDP_SIX_ZONE_LUT_SIZE; i++) {
		six_zone_p1[i] = readl_relaxed(addr + 4) &
				 PA_SIX_ZONE_CURVE_P1_MASK;
		six_zone_p0[i] = readl_relaxed(addr) &
				 PA_SIX_ZONE_CURVE_P0_MASK;
	}

	addr = base_addr + PA_SIX_ZONE_REGION_OFF;
	pa_data->six_zone_thresh = readl_relaxed(addr);

	addr = base_addr + PA_SIX_ZONE_ADJ_OFF;
	pa_data->six_zone_adj_p0 = readl_relaxed(addr) &
				   PA_SIX_ZONE_ADJ_P0_MASK;
	addr += 4;
	pa_data->six_zone_adj_p1 = readl_relaxed(addr);

	if (copy_to_user(pa_data->six_zone_curve_p0, six_zone_p0,
			 six_zone_sz)) {
		pr_err("Failed to copy six zone p0 data\n");
		ret = -EFAULT;
		goto six_zone_memory_exit;
	}
	if (copy_to_user(pa_data->six_zone_curve_p1, six_zone_p1,
			 six_zone_sz)) {
		pr_err("Failed to copy six zone p1 data\n");
		ret = -EFAULT;
		goto six_zone_memory_exit;
	}

	six_zone_hold = pa_hold >> PA_HOLD_SIX_ZONE_SHIFT;
	pa_data->six_zone_sat_hold = (six_zone_hold >> PA_HOLD_SAT_SHIFT) &
				     PA_HOLD_MASK;
	pa_data->six_zone_val_hold = (six_zone_hold >> PA_HOLD_VAL_SHIFT) &
				     PA_HOLD_MASK;

six_zone_memory_exit:
	kfree(six_zone_read_buf);
six_zone_exit:
	return ret;
}

static int pp_pa_get_config(char __iomem *base_addr, void *cfg_data,
				u32 block_type, u32 disp_num)
{
	struct mdp_pa_v2_cfg_data *pa_cfg_data = NULL;
	struct mdp_pa_data_v1_7 pa_data;
	int ret = 0;
	uint32_t pa_hold = 0;
	char __iomem *pa_hold_addr = NULL;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p\n",
		      base_addr, cfg_data);
		return -EINVAL;
	}
	if ((block_type != DSPP) && (block_type != SSPP_VIG)) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	pa_cfg_data = (struct mdp_pa_v2_cfg_data *) cfg_data;
	if (pa_cfg_data->version != mdp_pa_v1_7) {
		pr_err("invalid pa version %d\n", pa_cfg_data->version);
		return -EINVAL;
	}

	if (copy_from_user(&pa_data, pa_cfg_data->cfg_payload,
			sizeof(pa_data))) {
		pr_err("copy from user failed for pa data\n");
		return -EFAULT;
	}

	if (block_type == DSPP)
		pa_hold_addr = base_addr + PA_DSPP_HOLD_OFF;
	else
		pa_hold_addr = base_addr + PA_VIG_HOLD_OFF;
	pa_hold = readl_relaxed(pa_hold_addr);

	if ((block_type == DSPP) &&
	    (pa_cfg_data->flags & MDP_PP_PA_SIX_ZONE_ENABLE)) {
		ret = pp_pa_get_six_zone(base_addr,
				   &pa_data,
				   pa_cfg_data->flags,
				   pa_hold);
		if (ret) {
			pr_err("six zone read failed ret %d\n", ret);
			return ret;
		}
	}
	pp_pa_get_global_adj_regs(base_addr, &pa_data, pa_cfg_data->flags,
			block_type);
	pp_pa_get_mem_col(base_addr, &pa_data, pa_cfg_data->flags,
			block_type, pa_hold);

	ret = copy_to_user(pa_cfg_data->cfg_payload, &pa_data, sizeof(pa_data));
	if (ret) {
		pr_err("Failed to copy PA data to user\n");
		return -EFAULT;
	}

	return 0;
}

static void pp_pa_update_vig_opmode(int pa_sts, u32 *opmode)
{
	*opmode |= PA_VIG_OP_ENABLE;
	if (pa_sts & PP_STS_PA_HUE_MASK)
		*opmode |= PA_VIG_OP_HUE_MASK;
	if (pa_sts & PP_STS_PA_SAT_MASK)
		*opmode |= PA_VIG_OP_SAT_MASK;
	if (pa_sts & PP_STS_PA_VAL_MASK)
		*opmode |= PA_VIG_OP_VAL_MASK;
	if (pa_sts & PP_STS_PA_CONT_MASK)
		*opmode |= PA_VIG_OP_CONT_MASK;
	if (pa_sts & PP_STS_PA_SAT_ZERO_EXP_EN)
		*opmode |= PA_VIG_OP_SAT_ZERO_EXP_EN;
	if (pa_sts & PP_STS_PA_MEM_COL_SKIN_MASK)
		*opmode |= PA_VIG_OP_MEM_COL_SKIN_MASK;
	if (pa_sts & PP_STS_PA_MEM_COL_FOL_MASK)
		*opmode |= PA_VIG_OP_MEM_COL_FOL_MASK;
	if (pa_sts & PP_STS_PA_MEM_COL_SKY_MASK)
		*opmode |= PA_VIG_OP_MEM_COL_SKY_MASK;
	if (pa_sts & PP_STS_PA_MEM_PROT_HUE_EN)
		*opmode |= PA_VIG_OP_MEM_PROT_HUE_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_SAT_EN)
		*opmode |= PA_VIG_OP_MEM_PROT_SAT_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_VAL_EN)
		*opmode |= PA_VIG_OP_MEM_PROT_VAL_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_CONT_EN)
		*opmode |= PA_VIG_OP_MEM_PROT_CONT_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_BLEND_EN)
		*opmode |= PA_VIG_OP_MEM_PROT_BLEND_EN;
}

static void pp_pa_update_dspp_opmode(int pa_sts, u32 *opmode)
{
	*opmode |= PA_DSPP_OP_ENABLE;
	if (pa_sts & PP_STS_PA_HUE_MASK)
		*opmode |= PA_DSPP_OP_HUE_MASK;
	if (pa_sts & PP_STS_PA_SAT_MASK)
		*opmode |= PA_DSPP_OP_SAT_MASK;
	if (pa_sts & PP_STS_PA_VAL_MASK)
		*opmode |= PA_DSPP_OP_VAL_MASK;
	if (pa_sts & PP_STS_PA_CONT_MASK)
		*opmode |= PA_DSPP_OP_CONT_MASK;
	if (pa_sts & PP_STS_PA_SAT_ZERO_EXP_EN)
		*opmode |= PA_DSPP_OP_SAT_ZERO_EXP_EN;
	if (pa_sts & PP_STS_PA_MEM_COL_SKIN_MASK)
		*opmode |= PA_DSPP_OP_MEM_COL_SKIN_MASK;
	if (pa_sts & PP_STS_PA_MEM_COL_FOL_MASK)
		*opmode |= PA_DSPP_OP_MEM_COL_FOL_MASK;
	if (pa_sts & PP_STS_PA_MEM_COL_SKY_MASK)
		*opmode |= PA_DSPP_OP_MEM_COL_SKY_MASK;
	if (pa_sts & PP_STS_PA_SIX_ZONE_HUE_MASK)
		*opmode |= PA_DSPP_OP_SIX_ZONE_HUE_MASK;
	if (pa_sts & PP_STS_PA_SIX_ZONE_SAT_MASK)
		*opmode |= PA_DSPP_OP_SIX_ZONE_SAT_MASK;
	if (pa_sts & PP_STS_PA_SIX_ZONE_VAL_MASK)
		*opmode |= PA_DSPP_OP_SIX_ZONE_VAL_MASK;
	if (pa_sts & PP_STS_PA_MEM_PROT_HUE_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_HUE_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_SAT_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_SAT_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_VAL_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_VAL_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_CONT_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_CONT_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_BLEND_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_BLEND_EN;
	if (pa_sts & PP_STS_PA_MEM_PROT_SIX_EN)
		*opmode |= PA_DSPP_OP_MEM_PROT_SIX_EN;
}

static int pp_igc_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	int ret = 0, i = 0;
	struct mdp_igc_lut_data *lut_cfg_data = NULL;
	struct mdp_igc_lut_data_v1_7 *lut_data = NULL;
	char __iomem *c0 = NULL, *c1 = NULL, *c2 = NULL;
	u32 data;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}

	lut_cfg_data = (struct mdp_igc_lut_data *) cfg_data;
	if (lut_cfg_data->version != mdp_igc_v1_7 ||
	    !lut_cfg_data->cfg_payload) {
		pr_err("invalid igc version %d payload %p\n",
		       lut_cfg_data->version, lut_cfg_data->cfg_payload);
		return -EINVAL;
	}
	if (!(lut_cfg_data->ops & ~(MDP_PP_OPS_READ))) {
		pr_err("only read ops set for lut\n");
		return ret;
	}
	if (lut_cfg_data->block > IGC_MASK_MAX) {
		pr_err("invalid mask value for IGC %d", lut_cfg_data->block);
		return -EINVAL;
	}
	if (!(lut_cfg_data->ops & MDP_PP_OPS_WRITE)) {
		pr_debug("non write ops set %d\n", lut_cfg_data->ops);
		goto bail_out;
	}
	lut_data = lut_cfg_data->cfg_payload;
	if (lut_data->len != IGC_LUT_ENTRIES || !lut_data->c0_c1_data ||
	    !lut_data->c2_data) {
		pr_err("invalid lut len %d c0_c1_data %p  c2_data %p\n",
		       lut_data->len, lut_data->c0_c1_data, lut_data->c2_data);
		return -EINVAL;
	}
	switch (block_type) {
	case SSPP_RGB:
		c0 = base_addr + IGC_RGB_C0_LUT;
		break;
	case SSPP_DMA:
		c0 = base_addr + IGC_DMA_C0_LUT;
		break;
	case SSPP_VIG:
	case DSPP:
		c0 = base_addr + IGC_C0_LUT;
		break;
	default:
		pr_err("Invalid block type %d\n", block_type);
		ret = -EINVAL;
		break;
	}
	if (ret) {
		pr_err("igc table not updated ret %d\n", ret);
		return ret;
	}
	c1 = c0 + 4;
	c2 = c1 + 4;
	data = IGC_INDEX_UPDATE | IGC_CONFIG_MASK(lut_cfg_data->block);
	pr_debug("data %x block type %d mask %x\n",
		  data, lut_cfg_data->block,
		  IGC_CONFIG_MASK(lut_cfg_data->block));
	writel_relaxed((lut_data->c0_c1_data[0] & IGC_DATA_MASK) | data, c0);
	writel_relaxed(((lut_data->c0_c1_data[0] >> 16)
			& IGC_DATA_MASK) | data, c1);
	writel_relaxed((lut_data->c2_data[0] & IGC_DATA_MASK) | data, c2);
	data &= ~IGC_INDEX_UPDATE;
	/* update the index for c0, c1 , c2 */
	for (i = 1; i < IGC_LUT_ENTRIES; i++) {
		writel_relaxed((lut_data->c0_c1_data[i] & IGC_DATA_MASK)
			       | data, c0);
		writel_relaxed(((lut_data->c0_c1_data[i] >> 16)
				& IGC_DATA_MASK) | data, c1);
		writel_relaxed((lut_data->c2_data[i] & IGC_DATA_MASK)
				| data, c2);
	}
bail_out:
	if (!ret) {
		if (lut_cfg_data->ops & MDP_PP_OPS_DISABLE)
			pp_sts->igc_sts &= ~PP_STS_ENABLE;
		else if (lut_cfg_data->ops & MDP_PP_OPS_ENABLE)
			pp_sts->igc_sts |= PP_STS_ENABLE;
		pp_sts_set_split_bits(&pp_sts->igc_sts,
				      lut_cfg_data->ops);
	}
	return ret;
}

static int pp_igc_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	int ret = 0, i = 0;
	struct mdp_igc_lut_data *lut_cfg_data = NULL;
	struct mdp_igc_lut_data_v1_7 *lut_data = NULL;
	char __iomem *c1 = NULL, *c2 = NULL;
	u32 *c0c1_data = NULL, *c2_data = NULL;
	u32 data = 0, sz = 0;

	if (!base_addr || !cfg_data || block_type != DSPP) {
		pr_err("invalid params base_addr %p cfg_data %p block_type %d\n",
		      base_addr, cfg_data, block_type);
		return -EINVAL;
	}
	lut_cfg_data = (struct mdp_igc_lut_data *) cfg_data;
	if (!(lut_cfg_data->ops & MDP_PP_OPS_READ)) {
		pr_err("read ops not set for lut ops %d\n", lut_cfg_data->ops);
		return ret;
	}
	if (lut_cfg_data->version != mdp_igc_v1_7 ||
	    !lut_cfg_data->cfg_payload ||
	    lut_cfg_data->block > IGC_MASK_MAX) {
		pr_err("invalid igc version %d payload %p block %d\n",
		       lut_cfg_data->version, lut_cfg_data->cfg_payload,
		       lut_cfg_data->block);
		ret = -EINVAL;
		goto exit;
	}
	lut_data = lut_cfg_data->cfg_payload;
	if (lut_data->len != IGC_LUT_ENTRIES) {
		pr_err("invalid lut len %d\n", lut_data->len);
		ret = -EINVAL;
		goto exit;
	}
	sz = IGC_LUT_ENTRIES * sizeof(u32);
	if (!access_ok(VERIFY_WRITE, lut_data->c0_c1_data, sz) ||
	    (!access_ok(VERIFY_WRITE, lut_data->c2_data, sz))) {
		pr_err("invalid lut address for sz %d\n", sz);
		ret = -EFAULT;
		goto exit;
	}
	/* Allocate for c0c1 and c2 tables */
	c0c1_data = kzalloc(sz * 2, GFP_KERNEL);
	if (!c0c1_data) {
		pr_err("allocation failed for c0c1 size %d\n", sz * 2);
		ret = -ENOMEM;
		goto exit;
	}
	c2_data = &c0c1_data[IGC_LUT_ENTRIES];
	data = IGC_INDEX_VALUE_UPDATE | IGC_CONFIG_MASK(lut_cfg_data->block);
	pr_debug("data %x block type %d mask %x\n",
		  data, lut_cfg_data->block,
		  IGC_CONFIG_MASK(lut_cfg_data->block));
	c1 = base_addr + 4;
	c2 = c1 + 4;
	writel_relaxed(data, base_addr);
	writel_relaxed(data, c1);
	writel_relaxed(data, c2);
	for (i = 0; i < IGC_LUT_ENTRIES; i++) {
		c0c1_data[i] = readl_relaxed(base_addr) & IGC_DATA_MASK;
		c0c1_data[i] |= (readl_relaxed(c1) & IGC_DATA_MASK) << 16;
		c2_data[i] = readl_relaxed(c2) & IGC_DATA_MASK;
	}
	if (copy_to_user(lut_data->c0_c1_data, c0c1_data, sz)) {
		pr_err("failed to copy the c0c1 data");
		ret = -EFAULT;
	}
	if (!ret && copy_to_user(lut_data->c2_data, c2_data, sz)) {
		pr_err("failed to copy the c2 data");
		ret = -EFAULT;
	}
	kfree(c0c1_data);
exit:
	return ret;
}


static int pp_pgc_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	char __iomem *c0 = NULL, *c1 = NULL, *c2 = NULL;
	u32 val = 0, i = 0, *sts = NULL;
	struct mdp_pgc_lut_data *pgc_data = NULL;
	struct mdp_pgc_lut_data_v1_7  *pgc_data_v17 = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %p cfg_data %p pp_sts_type %p\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}
	if (block_type != DSPP && block_type != LM) {
		pr_err("invalid block type %d\n", block_type);
		return -EINVAL;
	}
	sts = (block_type == DSPP) ? &pp_sts->pgc_sts : &pp_sts->argc_sts;
	pgc_data = (struct mdp_pgc_lut_data *) cfg_data;
	if (pgc_data->version != mdp_pgc_v1_7) {
		pr_err("invalid pgc version %d\n",
			pgc_data->version);
		return -EINVAL;
	}
	if (!(pgc_data->flags & ~(MDP_PP_OPS_READ))) {
		pr_debug("only read ops is set %d", pgc_data->flags);
		return 0;
	}
	if (pgc_data->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("disable GC\n");
		goto set_ops;
	}

	pgc_data_v17 = (struct mdp_pgc_lut_data_v1_7 *) pgc_data->cfg_payload;
	if (!pgc_data_v17) {
		pr_err("invalid payload for GC %p\n", pgc_data_v17);
		return -EINVAL;
	}

	if (pgc_data_v17->len != PGC_LUT_ENTRIES || !pgc_data_v17->c0_data ||
	    !pgc_data_v17->c1_data || !pgc_data_v17->c2_data) {
		pr_err("Invalid params entries %d c0_data %p c1_data %p c2_data %p\n",
			pgc_data_v17->len, pgc_data_v17->c0_data,
			pgc_data_v17->c1_data, pgc_data_v17->c2_data);
		return -EINVAL;
	}
	c0 = base_addr + PGC_C0_LUT_INDEX;
	c1 = c0 + PGC_C1C2_LUT_OFF;
	c2 = c1 + PGC_C1C2_LUT_OFF;
	/*  set the indexes to zero */
	writel_relaxed(0, c0 + PGC_INDEX_OFF);
	writel_relaxed(0, c1 + PGC_INDEX_OFF);
	writel_relaxed(0, c2 + PGC_INDEX_OFF);
	for (i = 0; i < PGC_LUT_ENTRIES; i += 2) {
		val = pgc_data_v17->c0_data[i] & PGC_DATA_MASK;
		val |= (pgc_data_v17->c0_data[i + 1] & PGC_DATA_MASK) <<
			PGC_ODD_SHIFT;
		writel_relaxed(val, c0);
		val = pgc_data_v17->c1_data[i] & PGC_DATA_MASK;
		val |= (pgc_data_v17->c1_data[i + 1] & PGC_DATA_MASK) <<
			PGC_ODD_SHIFT;
		writel_relaxed(val, c1);
		val = pgc_data_v17->c2_data[i] & PGC_DATA_MASK;
		val |= (pgc_data_v17->c2_data[i + 1] & PGC_DATA_MASK) <<
			PGC_ODD_SHIFT;
		writel_relaxed(val, c2);
	}
	if (block_type == DSPP) {
		val = PGC_SWAP;
		writel_relaxed(val, base_addr + PGC_LUT_SWAP);
	}

set_ops:
	if (pgc_data->flags & MDP_PP_OPS_DISABLE) {
		*sts &= ~PP_STS_ENABLE;
		writel_relaxed(0, base_addr + PGC_OPMODE_OFF);
	} else if (pgc_data->flags & MDP_PP_OPS_ENABLE) {
		val = PGC_ENABLE;
		writel_relaxed(val, base_addr + PGC_OPMODE_OFF);
		*sts |= PP_STS_ENABLE;
	}
	return 0;
}

static int pp_pgc_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	int ret = 0;
	char __iomem *c0 = NULL, *c1 = NULL, *c2 = NULL;
	u32 *c0_data = NULL, *c1_data = NULL, *c2_data = NULL;
	u32 val = 0, i = 0, sz = 0;
	struct mdp_pgc_lut_data *pgc_data = NULL;
	struct mdp_pgc_lut_data_v1_7  *pgc_data_v17 = NULL;
	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %p cfg_data %p block_type %d\n",
		      base_addr, cfg_data, block_type);
		return -EINVAL;
	}
	pgc_data = (struct mdp_pgc_lut_data *) cfg_data;
	pgc_data_v17 = (struct mdp_pgc_lut_data_v1_7 *)
			pgc_data->cfg_payload;
	if (pgc_data->version != mdp_pgc_v1_7 || !pgc_data_v17) {
		pr_err("invalid pgc version %d payload %p\n",
			pgc_data->version, pgc_data_v17);
		return -EINVAL;
	}
	if (!(pgc_data->flags & MDP_PP_OPS_READ)) {
		pr_info("read ops is not set %d", pgc_data->flags);
		return -EINVAL;
	}
	sz = PGC_LUT_ENTRIES * sizeof(u32);
	if (!access_ok(VERIFY_WRITE, pgc_data_v17->c0_data, sz) ||
	    !access_ok(VERIFY_WRITE, pgc_data_v17->c1_data, sz) ||
	    !access_ok(VERIFY_WRITE, pgc_data_v17->c2_data, sz)) {
		pr_err("incorrect payload for PGC read size %d\n",
			PGC_LUT_ENTRIES);
		return -EFAULT;
	}
	c0_data = kzalloc(sz * 3, GFP_KERNEL);
	if (!c0_data) {
		pr_err("memory allocation failure sz %d", sz * 3);
		return -ENOMEM;
	}
	c1_data = c0_data + PGC_LUT_ENTRIES;
	c2_data = c1_data + PGC_LUT_ENTRIES;
	c0 = base_addr + PGC_C0_LUT_INDEX;
	c1 = c0 + PGC_C1C2_LUT_OFF;
	c2 = c1 + PGC_C1C2_LUT_OFF;
	/*  set the indexes to zero */
	writel_relaxed(0, c0 + 4);
	writel_relaxed(0, c1 + 4);
	writel_relaxed(0, c2 + 4);
	for (i = 0; i < PGC_LUT_ENTRIES; i += 2) {
		val = readl_relaxed(c0);
		c0_data[i] = val & PGC_DATA_MASK;
		c0_data[i + 1] = (val >> PGC_ODD_SHIFT) & PGC_DATA_MASK;
		val = readl_relaxed(c1);
		c1_data[i] = val & PGC_DATA_MASK;
		c1_data[i + 1] = (val >> PGC_ODD_SHIFT) & PGC_DATA_MASK;
		val = readl_relaxed(c2);
		c2_data[i] = val & PGC_DATA_MASK;
		c2_data[i + 1] = (val >> PGC_ODD_SHIFT) & PGC_DATA_MASK;
	}
	if (copy_to_user(pgc_data_v17->c0_data, c0_data, sz)) {
		pr_err("failed to copyuser c0 data of sz %d\n", sz);
		ret = -EFAULT;
	}
	if (!ret && copy_to_user(pgc_data_v17->c1_data, c1_data, sz)) {
		pr_err("failed to copyuser c1 data of sz %d\n", sz);
		ret = -EFAULT;
	}
	if (!ret && copy_to_user(pgc_data_v17->c2_data, c2_data, sz)) {
		pr_err("failed to copyuser c2 data of sz %d\n", sz);
		ret = -EFAULT;
	}
	if (!ret)
		pgc_data_v17->len = PGC_LUT_ENTRIES;
	kfree(c0_data);
	return ret;
}

static int pp_pcc_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_pcc_v1_7;
	return 0;
}

static int pp_igc_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_igc_v1_7;
	return 0;
}

static int pp_pgc_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_pgc_v1_7;
	return 0;
}

static int pp_pa_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_pa_v1_7;
	return 0;
}

static int pp_gamut_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_gamut_v1_7;
	return 0;
}

static int pp_dither_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_dither_v1_7;
	return 0;
}

static int pp_hist_lut_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %p\n", version);
		return -EINVAL;
	}
	*version = mdp_hist_lut_v1_7;
	return 0;
}

static void pp_gamut_clock_gating_en(char __iomem *base_addr)
{
	u32 val;

	if (base_addr) {
		val = readl_relaxed(base_addr + GAMUT_CLK_CTRL);
		if (val == GAMUT_CLK_GATING_PARTIAL_ACTIVE)
			writel_relaxed(GAMUT_CLK_GATING_ACTIVE,
						   base_addr + GAMUT_CLK_CTRL);
	}
}
