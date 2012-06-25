/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "mdss_mdp.h"

struct mdp_csc_cfg mdp_csc_convert[MDSS_MDP_MAX_CSC] = {
	[MDSS_MDP_CSC_RGB2RGB] = {
		0,
		{
			0x0200, 0x0000, 0x0000,
			0x0000, 0x0200, 0x0000,
			0x0000, 0x0000, 0x0200,
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	},
	[MDSS_MDP_CSC_YUV2RGB] = {
		0,
		{
			0x0254, 0x0000, 0x0331,
			0x0254, 0xff37, 0xfe60,
			0x0254, 0x0409, 0x0000,
		},
		{ 0xfff0, 0xff80, 0xff80,},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	},
	[MDSS_MDP_CSC_RGB2YUV] = {
		0,
		{
			0x0083, 0x0102, 0x0032,
			0x1fb5, 0x1f6c, 0x00e1,
			0x00e1, 0x1f45, 0x1fdc
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0010, 0x0080, 0x0080,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0010, 0x00eb, 0x0010, 0x00f0, 0x0010, 0x00f0,},
	},
	[MDSS_MDP_CSC_YUV2YUV] = {
		0,
		{
			0x0200, 0x0000, 0x0000,
			0x0000, 0x0200, 0x0000,
			0x0000, 0x0000, 0x0200,
		},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	},
};

#define CSC_MV_OFF	0x0
#define CSC_BV_OFF	0x2C
#define CSC_LV_OFF	0x14
#define CSC_POST_OFF	0xC

static int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, u32 tbl_idx,
				   struct mdp_csc_cfg *data)
{
	int i, ret = 0;
	u32 *off, base, val = 0;

	if (data == NULL) {
		pr_err("no csc matrix specified\n");
		return -EINVAL;
	}

	switch (block) {
	case MDSS_MDP_BLOCK_SSPP:
		if (blk_idx < MDSS_MDP_SSPP_RGB0) {
			base = MDSS_MDP_REG_SSPP_OFFSET(blk_idx);
			if (tbl_idx == 1)
				base += MDSS_MDP_REG_VIG_CSC_1_BASE;
			else
				base += MDSS_MDP_REG_VIG_CSC_0_BASE;
		} else {
			ret = -EINVAL;
		}
		break;
	case MDSS_MDP_BLOCK_WB:
		if (blk_idx < MDSS_MDP_MAX_WRITEBACK) {
			base = MDSS_MDP_REG_WB_OFFSET(blk_idx) +
			       MDSS_MDP_REG_WB_CSC_BASE;
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret != 0) {
		pr_err("unsupported block id for csc\n");
		return ret;
	}

	off = (u32 *) (base + CSC_MV_OFF);
	for (i = 0; i < 9; i++) {
		if (i & 0x1) {
			val |= data->csc_mv[i] << 16;
			MDSS_MDP_REG_WRITE(off, val);
			off++;
		} else {
			val = data->csc_mv[i];
		}
	}
	MDSS_MDP_REG_WRITE(off, val); /* COEFF_33 */

	off = (u32 *) (base + CSC_BV_OFF);
	for (i = 0; i < 3; i++) {
		MDSS_MDP_REG_WRITE(off, data->csc_pre_bv[i]);
		MDSS_MDP_REG_WRITE((u32 *)(((u32)off) + CSC_POST_OFF),
				   data->csc_post_bv[i]);
		off++;
	}

	off = (u32 *) (base + CSC_LV_OFF);
	for (i = 0; i < 6; i += 2) {
		val = (data->csc_pre_lv[i] << 8) | data->csc_pre_lv[i+1];
		MDSS_MDP_REG_WRITE(off, val);

		val = (data->csc_post_lv[i] << 8) | data->csc_post_lv[i+1];
		MDSS_MDP_REG_WRITE((u32 *)(((u32)off) + CSC_POST_OFF), val);
		off++;
	}

	return ret;
}

int mdss_mdp_csc_setup(u32 block, u32 blk_idx, u32 tbl_idx, u32 csc_type)
{
	struct mdp_csc_cfg *data;

	if (csc_type >= MDSS_MDP_MAX_CSC) {
		pr_err("invalid csc matrix index %d\n", csc_type);
		return -ERANGE;
	}

	pr_debug("csc type=%d blk=%d idx=%d tbl=%d\n", csc_type,
		 block, blk_idx, tbl_idx);

	data = &mdp_csc_convert[csc_type];
	return mdss_mdp_csc_setup_data(block, blk_idx, tbl_idx, data);
}

int mdss_mdp_dspp_setup(struct mdss_mdp_ctl *ctl, struct mdss_mdp_mixer *mixer)
{
	int dspp_num;

	if (!ctl || !mixer)
		return -EINVAL;

	dspp_num = mixer->num;

	ctl->flush_bits |= BIT(13 + dspp_num);	/* DSPP */

	return 0;
}
