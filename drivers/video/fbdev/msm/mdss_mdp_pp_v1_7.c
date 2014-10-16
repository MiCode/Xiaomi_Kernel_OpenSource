/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#define GAMUT_ENABLE BIT(0)

static struct mdss_pp_res_type_v1_7 config_data;

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

void *pp_get_driver_ops(struct mdp_pp_driver_ops *ops)
{
	if (!ops) {
		pr_err("PP driver ops invalid %p\n", ops);
		return ERR_PTR(-EINVAL);
	}

	/* IGC ops */
	ops->pp_ops[IGC].pp_set_config = NULL;
	ops->pp_ops[IGC].pp_get_config = NULL;

	/* PCC ops */
	ops->pp_ops[PCC].pp_set_config = pp_pcc_set_config;
	ops->pp_ops[PCC].pp_get_config = pp_pcc_get_config;

	/* GC ops */
	ops->pp_ops[GC].pp_set_config = NULL;
	ops->pp_ops[GC].pp_get_config = NULL;

	/* PA ops */
	ops->pp_ops[PA].pp_set_config = NULL;
	ops->pp_ops[PA].pp_get_config = NULL;

	/* Gamut ops */
	ops->pp_ops[GAMUT].pp_set_config = pp_gamut_set_config;
	ops->pp_ops[GAMUT].pp_get_config = pp_gamut_get_config;

	/* CSC ops */
	ops->pp_ops[CSC].pp_set_config = NULL;
	ops->pp_ops[CSC].pp_get_config = NULL;

	/* Dither ops */
	ops->pp_ops[DITHER].pp_set_config = NULL;
	ops->pp_ops[DITHER].pp_get_config = NULL;

	/* QSEED ops */
	ops->pp_ops[QSEED].pp_set_config = NULL;
	ops->pp_ops[QSEED].pp_get_config = NULL;

	/* PA_LUT ops */
	ops->pp_ops[HIST_LUT].pp_set_config = NULL;
	ops->pp_ops[HIST_LUT].pp_get_config = NULL;

	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;

	return &config_data;
}

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side)
{
	if (!pp_sts || !opmode) {
		pr_err("Invalid pp_sts %p or opmode %p\n", pp_sts, opmode);
		return;
	}

	return;
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
	} else {
		mode = mdp_gamut_fine_mode;
		tbl_sz = MDP_GAMUT_TABLE_V1_7_SZ;
		sz = tbl_sz * sizeof(u32);
		index_start = GAMUT_FINE_INDEX;
	}
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
		pr_info("only read ops is set %d", gamut_cfg_data->flags);
		return 0;
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
		if (i >= MDP_GAMUT_SCALE_OFF_TABLE_NUM)
			continue;
		for (j = 0; j < MDP_GAMUT_SCALE_OFF_SZ; j++) {
			writel_relaxed((gamut_data->scale_off_data[i][j]),
				       base_addr_scale);
			base_addr_scale += 4;
		}
	}
bail_out:
	if (!ret) {
		val = 0;
		if (gamut_cfg_data->flags & MDP_PP_OPS_DISABLE) {
			pp_sts->gamut_sts &= ~PP_STS_ENABLE;
			writel_relaxed(val, base_addr + GAMUT_OP_MODE_OFF);
		} else if (gamut_cfg_data->flags & MDP_PP_OPS_ENABLE) {
			if (gamut_data->mode == mdp_gamut_coarse_mode)
				val |= GAMUT_COARSE_EN;
			val |= GAMUT_ENABLE;
			writel_relaxed(val, base_addr + GAMUT_OP_MODE_OFF);
			pp_sts->gamut_sts |= PP_STS_ENABLE;
		}
		pp_sts_set_split_bits(&pp_sts->gamut_sts,
				      gamut_cfg_data->flags);
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
	if (pcc_cfg_data->ops & MDP_PP_OPS_DISABLE) {
		writel_relaxed(opmode, base_addr + PCC_OP_MODE_OFF);
		pp_sts->pcc_sts &= ~PP_STS_ENABLE;
	} else if (pcc_cfg_data->ops & MDP_PP_OPS_ENABLE) {
		opmode |= PCC_ENABLE;
		writel_relaxed(opmode, base_addr + PCC_OP_MODE_OFF);
		pp_sts->pcc_sts |= PP_STS_ENABLE;
	}
	pp_sts_set_split_bits(&pp_sts->pcc_sts, pcc_cfg_data->ops);

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
