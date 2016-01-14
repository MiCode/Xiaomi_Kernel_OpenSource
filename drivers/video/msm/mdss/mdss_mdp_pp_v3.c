/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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


#define IGC_DSPP_OP_MODE_EN BIT(0)

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

void *pp_get_driver_ops_v3(struct mdp_pp_driver_ops *ops)
{
	void *pp_cfg = NULL;

	if (!ops) {
		pr_err("PP driver ops invalid %p\n", ops);
		return ERR_PTR(-EINVAL);
	}

	pp_cfg = pp_get_driver_ops_v1_7(ops);
	if (IS_ERR_OR_NULL(pp_cfg))
		return NULL;
	/* PA ops */
	ops->pp_ops[PA].pp_set_config = pp_pa_set_config;
	ops->pp_ops[PA].pp_get_config = pp_pa_get_config;
	ops->pp_ops[PA].pp_get_version = pp_pa_get_version;

	/* Dither ops */
	ops->pp_ops[DITHER].pp_set_config = pp_dither_set_config;
	ops->pp_ops[DITHER].pp_get_config = pp_dither_get_config;
	ops->pp_ops[DITHER].pp_get_version = pp_dither_get_version;

	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;

	ops->gamut_clk_gate_en = NULL;
	return pp_cfg;
}

static int pp_pa_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	return -EINVAL;
}

static int pp_pa_get_config(char __iomem *base_addr, void *cfg_data,
		u32 block_type, u32 disp_num)
{
	return -EINVAL;
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
	return -EINVAL;
}

static int pp_dither_set_config(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type)
{
	return -EINVAL;
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
		pr_err("Invalid pp_sts %p or opmode %p\n", pp_sts, opmode);
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
