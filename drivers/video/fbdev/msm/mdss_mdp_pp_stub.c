// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016, 2020, The Linux Foundation. All rights reserved.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"
#include "mdss_mdp_pp_common.h"

static int pp_stub_set_config(char __iomem *base_addr,
		 struct pp_sts_type *pp_sts, void *cfg_data,
		 u32 block_type);
static int pp_stub_get_config(char __iomem *base_addr, void *cfg_data,
			    u32 block_type, u32 disp_num);
static int pp_stub_get_version(u32 *version);

static int pp_get_hist_offset(u32 block, u32 *ctl_off);
static int pp_get_hist_isr(u32 *isr_mask);
static bool pp_is_sspp_hist_supp(void);

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side);

void *pp_get_driver_ops_stub(struct mdp_pp_driver_ops *ops)
{
	int i = 0;

	if (!ops) {
		pr_err("PP driver ops invalid %pK\n", ops);
		return ERR_PTR(-EINVAL);
	}
	for (i = 0; i < PP_MAX_FEATURES; i++) {
		ops->pp_ops[i].feature = i;
		ops->pp_ops[i].pp_get_config = pp_stub_get_config;
		ops->pp_ops[i].pp_get_version = pp_stub_get_version;
		ops->pp_ops[i].pp_set_config = pp_stub_set_config;
	}
	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;
	ops->get_hist_offset = pp_get_hist_offset;
	ops->get_hist_isr_info = pp_get_hist_isr;
	ops->is_sspp_hist_supp = pp_is_sspp_hist_supp;
	ops->gamut_clk_gate_en = NULL;
	return NULL;
}

static int pp_stub_get_version(u32 *version)
{
	if (!version) {
		pr_err("invalid version param\n");
		return -EINVAL;
	}
	*version = mdp_pp_unknown;
	return 0;
}

static int pp_stub_set_config(char __iomem *base_addr,
		 struct pp_sts_type *pp_sts, void *cfg_data,
		 u32 block_type)
{
	return -ENOTSUPP;
}

static int pp_stub_get_config(char __iomem *base_addr, void *cfg_data,
			    u32 block_type, u32 disp_num)
{
	return -ENOTSUPP;
}

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side)
{
}

static int pp_get_hist_isr(u32 *isr_mask)
{
	if (!isr_mask) {
		pr_err("invalid params isr_mask %pK\n", isr_mask);
		return -EINVAL;
	}

	*isr_mask = 0;
	return 0;
}

static int pp_get_hist_offset(u32 block, u32 *ctl_off)
{
	int ret = 0;

	if (!ctl_off) {
		pr_err("invalid params ctl_off %pK\n", ctl_off);
		return -EINVAL;
	}
	*ctl_off = U32_MAX;
	return ret;
}

static bool pp_is_sspp_hist_supp(void)
{
	return false;
}
