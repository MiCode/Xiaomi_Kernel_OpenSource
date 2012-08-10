/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "mdss_fb.h"
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

#define MDSS_BLOCK_DISP_NUM	(MDP_BLOCK_MAX - MDP_LOGICAL_BLOCK_DISP_0)

struct pp_sts_type {
	u32 pa_sts;
};

#define PP_FLAGS_DIRTY_PA	0x1

#define PP_STS_ENABLE	0x1

struct mdss_pp_res_type {
	/* logical info */
	u32 pp_disp_flags[MDSS_BLOCK_DISP_NUM];
	struct mdp_pa_cfg_data pa_disp_cfg[MDSS_BLOCK_DISP_NUM];
	/* physical info */
	struct pp_sts_type pp_dspp_sts[MDSS_MDP_MAX_DSPP];
};

static DEFINE_MUTEX(mdss_pp_mutex);
static struct mdss_pp_res_type *mdss_pp_res;

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

static int pp_dspp_setup(u32 disp_num, struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_mixer *mixer)
{
	u32 flags, base, offset, dspp_num, opmode = 0;
	struct mdp_pa_cfg_data *pa_config;
	struct pp_sts_type *pp_sts;

	dspp_num = mixer->num;
	/* no corresponding dspp */
	if ((mixer->type != MDSS_MDP_MIXER_TYPE_INTF) ||
		(dspp_num >= MDSS_MDP_MAX_DSPP))
		return 0;

	if (disp_num < MDSS_BLOCK_DISP_NUM)
		flags = mdss_pp_res->pp_disp_flags[disp_num];
	else
		flags = 0;

	/* nothing to update */
	if (!flags)
		return 0;
	pp_sts = &mdss_pp_res->pp_dspp_sts[dspp_num];
	base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num);
	if (flags & PP_FLAGS_DIRTY_PA) {
		pa_config = &mdss_pp_res->pa_disp_cfg[disp_num];
		if (pa_config->flags & MDP_PP_OPS_WRITE) {
			offset = base + MDSS_MDP_REG_DSPP_PA_BASE;
			MDSS_MDP_REG_WRITE(offset, pa_config->hue_adj);
			offset += 4;
			MDSS_MDP_REG_WRITE(offset, pa_config->sat_adj);
			offset += 4;
			MDSS_MDP_REG_WRITE(offset, pa_config->val_adj);
			offset += 4;
			MDSS_MDP_REG_WRITE(offset, pa_config->cont_adj);
		}
		if (pa_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->pa_sts &= ~PP_STS_ENABLE;
		else if (pa_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->pa_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->pa_sts & PP_STS_ENABLE)
		opmode |= (1 << 20); /* PA_EN */
	MDSS_MDP_REG_WRITE(base + MDSS_MDP_REG_DSPP_OP_MODE, opmode);
	ctl->flush_bits |= BIT(13 + dspp_num); /* DSPP */
	return 0;

}
int mdss_mdp_pp_setup(struct mdss_mdp_ctl *ctl)
{
	u32 disp_num;
	if ((!ctl->mfd) || (!mdss_pp_res))
		return -EINVAL;

	/* treat fb_num the same as block logical id*/
	disp_num = ctl->mfd->index;

	mutex_lock(&mdss_pp_mutex);
	if (ctl->mixer_left)
		pp_dspp_setup(disp_num, ctl, ctl->mixer_left);
	if (ctl->mixer_right)
		pp_dspp_setup(disp_num, ctl, ctl->mixer_right);
	/* clear dirty flag */
	if (disp_num < MDSS_BLOCK_DISP_NUM)
		mdss_pp_res->pp_disp_flags[disp_num] = 0;
	mutex_unlock(&mdss_pp_mutex);

	return 0;
}

int mdss_mdp_pp_init(struct device *dev)
{
	int ret = 0;
	mutex_lock(&mdss_pp_mutex);
	if (!mdss_pp_res) {
		mdss_pp_res = devm_kzalloc(dev, sizeof(*mdss_pp_res),
				GFP_KERNEL);
		if (mdss_pp_res == NULL) {
			pr_err("%s mdss_pp_res allocation failed!", __func__);
			ret = -ENOMEM;
		}
	}
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
void mdss_mdp_pp_term(struct device *dev)
{
	if (!mdss_pp_res) {
		mutex_lock(&mdss_pp_mutex);
		devm_kfree(dev, mdss_pp_res);
		mdss_pp_res = NULL;
		mutex_unlock(&mdss_pp_mutex);
	}
}

int mdss_mdp_pa_config(struct mdp_pa_cfg_data *config, u32 *copyback)
{
	int i, ret = 0;
	u32 pa_offset, disp_num, mixer_cnt;
	u32 mixer_id[MDSS_MDP_MAX_LAYERMIXER];

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->flags & MDP_PP_OPS_READ) {
		mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);
		if (!mixer_cnt) {
			ret = -EPERM;
			goto pa_config_exit;
		}
		/* only read the first mixer */
		for (i = 0; i < mixer_cnt; i++) {
			if (mixer_id[i] < MDSS_MDP_MAX_DSPP)
				break;
		}
		if (i >= mixer_cnt) {
			ret = -EPERM;
			goto pa_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		pa_offset = MDSS_MDP_REG_DSPP_OFFSET(mixer_id[i]) +
			  MDSS_MDP_REG_DSPP_PA_BASE;

		config->hue_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->sat_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->val_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->cont_adj = MDSS_MDP_REG_READ(pa_offset);
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_pp_res->pa_disp_cfg[disp_num] = *config;
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_PA;
	}

pa_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
