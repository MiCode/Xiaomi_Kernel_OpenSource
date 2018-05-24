/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include "mdss_mdp_pp_common.h"

#define IGC_DSPP_OP_MODE_EN BIT(0)
#define ENHIST_BIT_SHIFT 16
/* PA related define */

/* Offsets from DSPP/VIG base to PA block */
#define PA_DSPP_BLOCK_REG_OFF 0x800
#define PA_VIG_BLOCK_REG_OFF 0x1200

/* Offsets to various subblocks from PA block
 * in VIG/DSPP.
 */
#define PA_OP_MODE_REG_OFF 0x0
#define PA_HIST_REG_OFF 0x4
#define PA_LUTV_SWAP_REG_OFF 0x18
#define PA_HSIC_REG_OFF 0x1C
#define PA_DITHER_CTL_REG_OFF 0x2C
#define PA_PWL_HOLD_REG_OFF 0x40

/* Memory Color offsets */
#define PA_MEM_COL_REG_OFF 0x80
#define PA_MEM_SKIN_REG_OFF (PA_MEM_COL_REG_OFF)
#define PA_MEM_SKY_REG_OFF  (PA_MEM_SKIN_REG_OFF + \
				JUMP_REGISTERS_OFF(5))
#define PA_MEM_FOL_REG_OFF  (PA_MEM_SKY_REG_OFF + \
				JUMP_REGISTERS_OFF(5))
#define PA_MEM_SKIN_ADJUST_P2_REG_OFF (PA_MEM_FOL_REG_OFF + \
					JUMP_REGISTERS_OFF(5))
#define PA_MEM_SKY_ADJUST_P2_REG_OFF (PA_MEM_SKIN_ADJUST_P2_REG_OFF + \
					JUMP_REGISTERS_OFF(2))
#define PA_MEM_FOL_ADJUST_P2_REG_OFF (PA_MEM_SKY_ADJUST_P2_REG_OFF + \
					JUMP_REGISTERS_OFF(2))

#define PA_SZONE_REG_OFF 0x100
#define PA_LUTV_REG_OFF 0x200
#define PA_HIST_RAM_REG_OFF 0x400

#define PPB_GLOBAL_DITHER_REG_OFF 0x30E0
#define DITHER_MATRIX_LEN 16
#define DITHER_DEPTH_MAP_INDEX 9
static u32 dither_matrix[DITHER_MATRIX_LEN] = {
	15, 7, 13, 5, 3, 11, 1, 9, 12, 4, 14, 6, 0, 8, 2, 10};
static u32 dither_depth_map[DITHER_DEPTH_MAP_INDEX] = {
	0, 0, 0, 0, 0, 1, 2, 3, 3};

/* histogram prototypes */
static int pp_get_hist_offset(u32 block, u32 *ctl_off);
static int pp_hist_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_hist_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num);

/* PA LUT prototypes */
static int pp_hist_lut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num);
static int pp_hist_lut_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_hist_lut_get_version(u32 *version);
static void pp_hist_lut_opmode_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts);

static int pp_pa_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_pa_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num);
static int pp_pa_get_version(u32 *version);

static int pp_dither_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num);
static int pp_dither_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
static int pp_dither_get_version(u32 *version);

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side);

static void pp_pa_set_global_adj_regs(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data, u32 flag);

static void pp_pa_set_mem_col(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data, u32 flags);

static void pp_pa_set_six_zone(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data,
		u32 flags);

static void pp_pa_opmode_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts);

void *pp_get_driver_ops_v3(struct mdp_pp_driver_ops *ops)
{
	void *pp_cfg = NULL;

	if (!ops) {
		pr_err("PP driver ops invalid %pK\n", ops);
		return ERR_PTR(-EINVAL);
	}

	pp_cfg = pp_get_driver_ops_v1_7(ops);
	if (IS_ERR_OR_NULL(pp_cfg))
		return NULL;
	/* PA ops */
	ops->pp_ops[PA].pp_set_config = pp_pa_set_config;
	ops->pp_ops[PA].pp_get_config = pp_pa_get_config;
	ops->pp_ops[PA].pp_get_version = pp_pa_get_version;

	/* HIST_LUT ops */
	ops->pp_ops[HIST_LUT].pp_set_config = pp_hist_lut_set_config;
	ops->pp_ops[HIST_LUT].pp_get_config = pp_hist_lut_get_config;
	ops->pp_ops[HIST_LUT].pp_get_version = pp_hist_lut_get_version;

	/* HIST ops */
	ops->pp_ops[HIST].pp_set_config = pp_hist_set_config;
	ops->pp_ops[HIST].pp_get_config = pp_hist_get_config;
	ops->pp_ops[HIST].pp_get_version = NULL;

	/* Dither ops */
	ops->pp_ops[DITHER].pp_set_config = pp_dither_set_config;
	ops->pp_ops[DITHER].pp_get_config = pp_dither_get_config;
	ops->pp_ops[DITHER].pp_get_version = pp_dither_get_version;

	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;

	ops->get_hist_offset = pp_get_hist_offset;
	ops->gamut_clk_gate_en = NULL;

	return pp_cfg;
}

static int pp_get_hist_offset(u32 block, u32 *ctl_off)
{
	int ret = 0;

	if (!ctl_off) {
		pr_err("invalid params ctl_off %pK\n", ctl_off);
		return -EINVAL;
	}

	switch (block) {
	case SSPP_VIG:
		*ctl_off = PA_VIG_BLOCK_REG_OFF + PA_HIST_REG_OFF;
		break;
	case DSPP:
		*ctl_off = PA_DSPP_BLOCK_REG_OFF + PA_HIST_REG_OFF;
		break;
	default:
		pr_err("Invalid block type %d\n", block);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_hist_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data, u32 block_type)
{
	u32 opmode = 0;
	struct pp_hist_col_info *hist_info = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %pK cfg_data %pK pp_sts_type %pK\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}

	if (block_type != DSPP) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	hist_info = (struct pp_hist_col_info *)cfg_data;
	opmode = readl_relaxed(base_addr + PA_DSPP_BLOCK_REG_OFF +
				PA_OP_MODE_REG_OFF);
	/* set the hist_en bit */
	if (hist_info->col_en) {
		pp_sts->hist_sts |= PP_STS_ENABLE;
		opmode |= BIT(16);
	} else {
		pp_sts->hist_sts &= ~PP_STS_ENABLE;
		opmode &= ~BIT(16);
	}

	writel_relaxed(opmode, base_addr + PA_DSPP_BLOCK_REG_OFF +
			PA_OP_MODE_REG_OFF);
	return 0;
}

static int pp_hist_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{
	int i = 0;
	u32 sum = 0;
	struct pp_hist_col_info *hist_info = NULL;
	char __iomem *hist_addr;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %pK cfg_data %pK\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}

	if (block_type != DSPP) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	hist_info = (struct pp_hist_col_info *) cfg_data;
	hist_addr = base_addr + PA_DSPP_BLOCK_REG_OFF + PA_HIST_RAM_REG_OFF;

	for (i = 0; i < HIST_V_SIZE; i++) {
		hist_info->data[i] = readl_relaxed(hist_addr) & REG_MASK(24);
		hist_addr += 0x4;
		sum += hist_info->data[i];
	}
	hist_info->hist_cnt_read++;
	return sum;
}

static int pp_hist_lut_get_config(char __iomem *base_addr, void *cfg_data,
			   u32 block_type, u32 disp_num)
{

	int ret = 0, i = 0;
	char __iomem *hist_lut_addr;
	u32 sz = 0, temp = 0, *data = NULL;
	struct mdp_hist_lut_data_v1_7 lut_data_v1_7;
	struct mdp_hist_lut_data_v1_7 *lut_data = &lut_data_v1_7;
	struct mdp_hist_lut_data *lut_cfg_data = NULL;

	if (!base_addr || !cfg_data) {
		pr_err("invalid params base_addr %pK cfg_data %pK\n",
		       base_addr, cfg_data);
		return -EINVAL;
	}

	if (block_type != DSPP) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	lut_cfg_data = (struct mdp_hist_lut_data *) cfg_data;
	if (!(lut_cfg_data->ops & MDP_PP_OPS_READ)) {
		pr_err("read ops not set for hist_lut %d\n", lut_cfg_data->ops);
		return 0;
	}
	if (lut_cfg_data->version != mdp_hist_lut_v1_7 ||
		!lut_cfg_data->cfg_payload) {
		pr_err("invalid hist_lut version %d payload %pK\n",
		       lut_cfg_data->version, lut_cfg_data->cfg_payload);
		return -EINVAL;
	}
	if (copy_from_user(lut_data, (void __user *) lut_cfg_data->cfg_payload,
		sizeof(*lut_data))) {
		pr_err("copy from user failed for lut_data\n");
		return -EFAULT;
	}
	if (lut_data->len != ENHIST_LUT_ENTRIES) {
		pr_err("invalid hist_lut len %d", lut_data->len);
		return -EINVAL;
	}
	sz = ENHIST_LUT_ENTRIES * sizeof(u32);
	if (!access_ok(VERIFY_WRITE, lut_data->data, sz)) {
		pr_err("invalid lut address for hist_lut sz %d\n", sz);
		return -EFAULT;
	}

	hist_lut_addr = base_addr + PA_DSPP_BLOCK_REG_OFF + PA_LUTV_REG_OFF;

	data = kzalloc(sz, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (i = 0; i < ENHIST_LUT_ENTRIES; i += 2) {
		temp = readl_relaxed(hist_lut_addr);
		data[i] = temp & REG_MASK(10);
		data[i + 1] =
			(temp & REG_MASK_SHIFT(10, 16)) >> ENHIST_BIT_SHIFT;
		hist_lut_addr += 4;
	}
	if (copy_to_user(lut_data->data, data, sz)) {
		pr_err("failed to copy the hist_lut back to user\n");
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
	char __iomem *hist_lut_addr = NULL, *swap_addr = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %pK cfg_data %pK pp_sts_type %pK\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}

	if (block_type != DSPP) {
		pr_err("Invalid block type %d\n", block_type);
		return -EINVAL;
	}

	lut_cfg_data = (struct mdp_hist_lut_data *) cfg_data;
	if (lut_cfg_data->version != mdp_hist_lut_v1_7) {
		pr_err("invalid hist_lut version %d\n", lut_cfg_data->version);
		return -EINVAL;
	}

	if (!(lut_cfg_data->ops & ~(MDP_PP_OPS_READ))) {
		pr_err("only read ops set for lut\n");
		return ret;
	}
	if (lut_cfg_data->ops & MDP_PP_OPS_DISABLE ||
		!(lut_cfg_data->ops & MDP_PP_OPS_WRITE)) {
		pr_debug("non write ops set %d\n", lut_cfg_data->ops);
		goto hist_lut_set_sts;
	}
	lut_data = lut_cfg_data->cfg_payload;
	if (!lut_data) {
		pr_err("invalid hist_lut cfg_payload %pK\n", lut_data);
		return -EINVAL;
	}

	if (lut_data->len != ENHIST_LUT_ENTRIES || !lut_data->data) {
		pr_err("invalid hist_lut len %d data %pK\n",
		       lut_data->len, lut_data->data);
		return -EINVAL;
	}

	hist_lut_addr = base_addr + PA_DSPP_BLOCK_REG_OFF + PA_LUTV_REG_OFF;
	swap_addr = base_addr + PA_DSPP_BLOCK_REG_OFF + PA_LUTV_SWAP_REG_OFF;

	for (i = 0; i < ENHIST_LUT_ENTRIES; i += 2) {
		temp = (lut_data->data[i] & REG_MASK(10)) |
			((lut_data->data[i + 1] & REG_MASK(10))
			 << ENHIST_BIT_SHIFT);

		writel_relaxed(temp, hist_lut_addr);
		hist_lut_addr += 4;
	}

	writel_relaxed(1, swap_addr);

hist_lut_set_sts:
	if (lut_cfg_data->ops & MDP_PP_OPS_DISABLE) {
		pp_sts->enhist_sts &= ~(PP_STS_ENABLE | PP_STS_PA_LUT_FIRST);
	} else if (lut_cfg_data->ops & MDP_PP_OPS_ENABLE) {
		pp_sts->enhist_sts |= PP_STS_ENABLE;
		if (lut_cfg_data->hist_lut_first)
			pp_sts->enhist_sts |= PP_STS_PA_LUT_FIRST;
		else
			pp_sts->enhist_sts &= ~PP_STS_PA_LUT_FIRST;
	}

	pp_hist_lut_opmode_config(base_addr + PA_DSPP_BLOCK_REG_OFF, pp_sts);
	return ret;
}

static int pp_hist_lut_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version %pK\n", version);
		return -EINVAL;
	}
	*version = mdp_hist_lut_v1_7;
	return 0;
}

static void pp_hist_lut_opmode_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts)
{
	u32 opmode = 0;

	if (!base_addr || !pp_sts) {
		pr_err("invalid params base_addr %pK pp_sts_type %pK\n",
			base_addr, pp_sts);
		return;
	}
	opmode = readl_relaxed(base_addr + PA_OP_MODE_REG_OFF);

	/* set the hist_lutv_en and hist_lutv_first_en bits */
	if (pp_sts->enhist_sts & PP_STS_ENABLE) {
		opmode |= BIT(19) | BIT(20);
		opmode |= (pp_sts->enhist_sts & PP_STS_PA_LUT_FIRST) ?
				BIT(21) : 0;
	} else {
		opmode &= ~(BIT(19) | BIT(21));
		if (!(pp_sts->pa_sts & PP_STS_ENABLE))
			opmode &= ~BIT(20);
	}

	writel_relaxed(opmode, base_addr + PA_OP_MODE_REG_OFF);
}

static int pp_pa_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	struct mdp_pa_v2_cfg_data *pa_cfg_data = NULL;
	struct mdp_pa_data_v1_7 *pa_data = NULL;
	char __iomem *block_addr = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %pK cfg_data %pK pp_sts_type %pK\n",
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

	block_addr = base_addr +
		((block_type == DSPP) ? PA_DSPP_BLOCK_REG_OFF :
		 PA_VIG_BLOCK_REG_OFF);

	if (pa_cfg_data->flags & MDP_PP_OPS_DISABLE ||
		!(pa_cfg_data->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("pa_cfg_data->flags = %d\n", pa_cfg_data->flags);
		goto pa_set_sts;
	}

	pa_data = pa_cfg_data->cfg_payload;
	if (!pa_data) {
		pr_err("invalid payload for pa %pK\n", pa_data);
		return -EINVAL;
	}

	pp_pa_set_global_adj_regs(block_addr, pa_data, pa_cfg_data->flags);
	pp_pa_set_mem_col(block_addr, pa_data, pa_cfg_data->flags);
	if (block_type == DSPP)
		pp_pa_set_six_zone(block_addr, pa_data, pa_cfg_data->flags);

pa_set_sts:
	pp_pa_set_sts(pp_sts, pa_data, pa_cfg_data->flags, block_type);
	pp_pa_opmode_config(block_addr, pp_sts);

	return 0;
}

static int pp_pa_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num)
{
	return -ENOTSUPP;
}

static int pp_pa_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version");
		return -EINVAL;
	}
	*version = mdp_pa_v1_7;
	return 0;
}

static int pp_dither_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num)
{
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
	char __iomem *dither_opmode = NULL;

	if (!base_addr || !cfg_data || !pp_sts) {
		pr_err("invalid params base_addr %pK cfg_data %pK pp_sts_type %pK\n",
		      base_addr, cfg_data, pp_sts);
		return -EINVAL;
	}
	if (block_type != PPB)
		return -ENOTSUPP;
	dither_opmode = base_addr + PPB_GLOBAL_DITHER_REG_OFF;
	base_addr = dither_opmode + 4;

	dither_cfg_data = (struct mdp_dither_cfg_data *) cfg_data;

	if (dither_cfg_data->version != mdp_dither_v1_7) {
		pr_err("invalid dither version %d\n", dither_cfg_data->version);
		return -EINVAL;
	}

	if (dither_cfg_data->flags & MDP_PP_OPS_READ) {
		pr_err("Invalid context for read operation\n");
		return -EINVAL;
	}

	if (dither_cfg_data->flags & MDP_PP_OPS_DISABLE ||
		!(dither_cfg_data->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("non write ops set %d\n", dither_cfg_data->flags);
		goto dither_set_sts;
	}

	dither_data = dither_cfg_data->cfg_payload;
	if (!dither_data) {
		pr_err("invalid payload for dither %pK\n", dither_data);
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
	data |= (dither_data->temporal_en) ? (1  << 8) : 0;
	writel_relaxed(data, base_addr);
	base_addr += 4;
	for (i = 0; i < DITHER_MATRIX_LEN; i += 4) {
		data = (dither_matrix[i] & REG_MASK(4)) |
			((dither_matrix[i + 1] & REG_MASK(4)) << 4) |
			((dither_matrix[i + 2] & REG_MASK(4)) << 8) |
			((dither_matrix[i + 3] & REG_MASK(4)) << 12);
		writel_relaxed(data, base_addr);
		base_addr += 4;
	}

dither_set_sts:
	pp_sts_set_split_bits(&pp_sts->dither_sts,
			dither_cfg_data->flags);
	if (dither_cfg_data->flags & MDP_PP_OPS_DISABLE) {
		pp_sts->dither_sts &= ~PP_STS_ENABLE;
		writel_relaxed(0, dither_opmode);
	} else if (dither_cfg_data->flags & MDP_PP_OPS_ENABLE) {
		pp_sts->dither_sts |= PP_STS_ENABLE;
		if (pp_sts_is_enabled(pp_sts->dither_sts, pp_sts->side_sts))
			writel_relaxed(BIT(0), dither_opmode);
	}
	return 0;
}

static int pp_dither_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid param version");
		return -EINVAL;
	}
	*version = mdp_dither_v1_7;
	return 0;
}

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side)
{
	if (!pp_sts || !opmode) {
		pr_err("Invalid pp_sts %pK or opmode %pK\n", pp_sts, opmode);
		return;
	}
	switch (location) {
	case SSPP_DMA:
		break;
	case SSPP_VIG:
		break;
	case DSPP:
		if (pp_sts_is_enabled(pp_sts->igc_sts, side))
			*opmode |= IGC_DSPP_OP_MODE_EN;
		break;
	case LM:
		if (pp_sts->argc_sts & PP_STS_ENABLE)
			pr_debug("pgc in LM enabled\n");
		break;
	default:
		pr_err("Invalid block type %d\n", location);
		break;
	}
}

static void pp_pa_set_global_adj_regs(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data, u32 flags)
{
	char __iomem *addr = NULL;

	addr = base_addr + PA_HSIC_REG_OFF;
	if (flags & MDP_PP_PA_HUE_ENABLE)
		writel_relaxed((pa_data->global_hue_adj &
					REG_MASK(12)), addr);
	addr += 4;
	if (flags & MDP_PP_PA_SAT_ENABLE)
		writel_relaxed((pa_data->global_sat_adj &
					REG_MASK(16)), addr);
	addr += 4;
	if (flags & MDP_PP_PA_VAL_ENABLE)
		writel_relaxed((pa_data->global_val_adj &
					REG_MASK(8)), addr);
	addr += 4;
	if (flags & MDP_PP_PA_CONT_ENABLE)
		writel_relaxed((pa_data->global_cont_adj &
					REG_MASK(8)), addr);
}

static void pp_pa_set_mem_col(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data, u32 flags)
{
	char __iomem *mem_col_base = NULL, *mem_col_p2 = NULL;
	struct mdp_pa_mem_col_data_v1_7 *mem_col_data = NULL;
	uint32_t mask = 0, hold = 0, hold_mask = 0;
	uint32_t hold_curr = 0;

	flags &= (MDP_PP_PA_SKIN_ENABLE | MDP_PP_PA_SKY_ENABLE |
			MDP_PP_PA_FOL_ENABLE);
	if (!flags)
		return;
	while (flags) {
		if (flags & MDP_PP_PA_SKIN_ENABLE) {
			flags &= ~MDP_PP_PA_SKIN_ENABLE;
			mem_col_base = base_addr + PA_MEM_SKIN_REG_OFF;
			mem_col_p2 = base_addr + PA_MEM_SKIN_ADJUST_P2_REG_OFF;
			mem_col_data = &pa_data->skin_cfg;
			hold |= pa_data->skin_cfg.sat_hold & REG_MASK(2);
			hold |= (pa_data->skin_cfg.val_hold & REG_MASK(2))
				<< 2;
			hold_mask |= REG_MASK(4);
		} else if (flags & MDP_PP_PA_SKY_ENABLE) {
			flags &= ~MDP_PP_PA_SKY_ENABLE;
			mem_col_base = base_addr + PA_MEM_SKY_REG_OFF;
			mem_col_p2 = base_addr + PA_MEM_SKY_ADJUST_P2_REG_OFF;
			mem_col_data = &pa_data->sky_cfg;
			hold |= (pa_data->sky_cfg.sat_hold & REG_MASK(2)) << 4;
			hold |= (pa_data->sky_cfg.val_hold & REG_MASK(2)) << 6;
			hold_mask |= REG_MASK_SHIFT(4, 4);
		} else if (flags & MDP_PP_PA_FOL_ENABLE) {
			flags &= ~MDP_PP_PA_FOL_ENABLE;
			mem_col_base = base_addr + PA_MEM_FOL_REG_OFF;
			mem_col_p2 = base_addr + PA_MEM_FOL_ADJUST_P2_REG_OFF;
			mem_col_data = &pa_data->fol_cfg;
			hold |= (pa_data->fol_cfg.sat_hold & REG_MASK(2)) << 8;
			hold |= (pa_data->fol_cfg.val_hold & REG_MASK(2)) << 10;
			hold_mask |= REG_MASK_SHIFT(4, 8);
		} else {
			break;
		}
		mask = REG_MASK_SHIFT(16, 16) | REG_MASK(11);
		writel_relaxed((mem_col_data->color_adjust_p0 & mask),
				mem_col_base);
		mem_col_base += 4;
		mask = U32_MAX;
		writel_relaxed((mem_col_data->color_adjust_p1 & mask),
				mem_col_base);
		mem_col_base += 4;
		mask = REG_MASK_SHIFT(11, 16) | REG_MASK(11);
		writel_relaxed((mem_col_data->hue_region & mask),
				mem_col_base);
		mem_col_base += 4;
		mask = REG_MASK(24);
		writel_relaxed((mem_col_data->sat_region & mask),
				mem_col_base);
		mem_col_base += 4;
		/* mask is same for val and sat */
		writel_relaxed((mem_col_data->val_region & mask),
				mem_col_base);
		mask = U32_MAX;
		writel_relaxed((mem_col_data->color_adjust_p2 & mask),
				mem_col_p2);
		mem_col_p2 += 4;
		writel_relaxed((mem_col_data->blend_gain & mask),
				mem_col_p2);
	}
	hold_curr = readl_relaxed(base_addr + PA_PWL_HOLD_REG_OFF) &
			REG_MASK(16);
	hold_curr &= ~hold_mask;
	hold = hold_curr | (hold & hold_mask);
	writel_relaxed(hold, (base_addr + PA_PWL_HOLD_REG_OFF));
}

static void pp_pa_set_six_zone(char __iomem *base_addr,
		struct mdp_pa_data_v1_7 *pa_data,
		u32 flags)
{
	char __iomem *addr = base_addr + PA_SZONE_REG_OFF;
	uint32_t mask_p0 = 0, mask_p1 = 0, hold = 0, hold_mask = 0;
	uint32_t hold_curr = 0;
	int i = 0;

	if (!(flags & MDP_PP_PA_SIX_ZONE_ENABLE))
		return;

	if (pa_data->six_zone_len != MDP_SIX_ZONE_LUT_SIZE ||
			!pa_data->six_zone_curve_p0 ||
			!pa_data->six_zone_curve_p1) {
		pr_err("Invalid six zone data: len %d curve_p0 %pK curve_p1 %pK\n",
				pa_data->six_zone_len,
				pa_data->six_zone_curve_p0,
				pa_data->six_zone_curve_p1);
		return;
	}
	mask_p0 = REG_MASK(12);
	mask_p1 = REG_MASK(12) | REG_MASK_SHIFT(12, 16);
	writel_relaxed((pa_data->six_zone_curve_p1[0] & mask_p1), addr + 4);
	/* Update the index to 0 and write value */
	writel_relaxed((pa_data->six_zone_curve_p0[0] & mask_p0) | BIT(26),
			addr);
	for (i = 1; i < MDP_SIX_ZONE_LUT_SIZE; i++) {
		writel_relaxed((pa_data->six_zone_curve_p1[i] & mask_p1),
				addr + 4);
		writel_relaxed((pa_data->six_zone_curve_p0[i] & mask_p0), addr);
	}
	addr += 8;
	writel_relaxed(pa_data->six_zone_thresh, addr);
	addr += 4;
	writel_relaxed(pa_data->six_zone_adj_p0 & REG_MASK(16), addr);
	addr += 4;
	writel_relaxed(pa_data->six_zone_adj_p1, addr);

	hold = (pa_data->six_zone_sat_hold & REG_MASK(2)) << 12;
	hold |= (pa_data->six_zone_val_hold & REG_MASK(2)) << 14;
	hold_mask = REG_MASK_SHIFT(4, 12);
	hold_curr = readl_relaxed(base_addr + PA_PWL_HOLD_REG_OFF) &
					REG_MASK(16);
	hold_curr &= ~hold_mask;
	hold = hold_curr | (hold & hold_mask);
	writel_relaxed(hold, (base_addr + PA_PWL_HOLD_REG_OFF));
}

static void pp_pa_opmode_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts)
{
	uint32_t opmode = 0;

	/* set the PA bits */
	if (pp_sts->pa_sts & PP_STS_ENABLE) {
		opmode |= BIT(20);

		if (pp_sts->pa_sts & PP_STS_PA_HUE_MASK)
			opmode |= BIT(25);
		if (pp_sts->pa_sts & PP_STS_PA_SAT_MASK)
			opmode |= BIT(26);
		if (pp_sts->pa_sts & PP_STS_PA_VAL_MASK)
			opmode |= BIT(27);
		if (pp_sts->pa_sts & PP_STS_PA_CONT_MASK)
			opmode |= BIT(28);
		if (pp_sts->pa_sts & PP_STS_PA_SAT_ZERO_EXP_EN)
			opmode |= BIT(1);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_COL_SKIN_MASK)
			opmode |= BIT(5);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_COL_FOL_MASK)
			opmode |= BIT(6);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_COL_SKY_MASK)
			opmode |= BIT(7);
		if (pp_sts->pa_sts & PP_STS_PA_SIX_ZONE_HUE_MASK)
			opmode |= BIT(29);
		if (pp_sts->pa_sts & PP_STS_PA_SIX_ZONE_SAT_MASK)
			opmode |= BIT(30);
		if (pp_sts->pa_sts & PP_STS_PA_SIX_ZONE_VAL_MASK)
			opmode |= BIT(31);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_HUE_EN)
			opmode |= BIT(22);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_SAT_EN)
			opmode |= BIT(23);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_VAL_EN)
			opmode |= BIT(24);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_CONT_EN)
			opmode |= BIT(18);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_BLEND_EN)
			opmode |= BIT(3);
		if (pp_sts->pa_sts & PP_STS_PA_MEM_PROT_SIX_EN)
			opmode |= BIT(17);
	}

	/* reset hist_en, hist_lutv_en and hist_lutv_first_en
	 *  bits based on the pp_sts
	 */
	if (pp_sts->hist_sts & PP_STS_ENABLE)
		opmode |= BIT(16);
	if (pp_sts->enhist_sts & PP_STS_ENABLE)
		opmode |= BIT(19) | BIT(20);
	if (pp_sts->enhist_sts & PP_STS_PA_LUT_FIRST)
		opmode |= BIT(21);

	writel_relaxed(opmode, base_addr + PA_OP_MODE_REG_OFF);
}
