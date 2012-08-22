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
#include <linux/uaccess.h>

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
#define IGC_LUT_ENTRIES	256
#define GC_LUT_SEGMENTS	16

struct pp_sts_type {
	u32 pa_sts;
	u32 pcc_sts;
	u32 igc_sts;
	u32 igc_tbl_idx;
	u32 argc_sts;
};

#define PP_FLAGS_DIRTY_PA	0x1
#define PP_FLAGS_DIRTY_PCC	0x2
#define PP_FLAGS_DIRTY_IGC	0x4
#define PP_FLAGS_DIRTY_ARGC	0x8

#define PP_STS_ENABLE	0x1

struct mdss_pp_res_type {
	/* logical info */
	u32 pp_disp_flags[MDSS_BLOCK_DISP_NUM];
	u32 igc_lut_c0c1[MDSS_BLOCK_DISP_NUM][IGC_LUT_ENTRIES];
	u32 igc_lut_c2[MDSS_BLOCK_DISP_NUM][IGC_LUT_ENTRIES];
	struct mdp_ar_gc_lut_data
		gc_lut_r[MDSS_BLOCK_DISP_NUM][GC_LUT_SEGMENTS];
	struct mdp_ar_gc_lut_data
		gc_lut_g[MDSS_BLOCK_DISP_NUM][GC_LUT_SEGMENTS];
	struct mdp_ar_gc_lut_data
		gc_lut_b[MDSS_BLOCK_DISP_NUM][GC_LUT_SEGMENTS];

	struct mdp_pa_cfg_data pa_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pcc_cfg_data pcc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_igc_lut_data igc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data pgc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	/* physical info */
	struct pp_sts_type pp_dspp_sts[MDSS_MDP_MAX_DSPP];
};

static DEFINE_MUTEX(mdss_pp_mutex);
static struct mdss_pp_res_type *mdss_pp_res;

static void pp_update_pcc_regs(u32 offset,
				struct mdp_pcc_cfg_data *cfg_ptr);
static void pp_update_igc_lut(struct mdp_igc_lut_data *cfg,
				u32 offset, u32 blk_idx);
static void pp_update_gc_one_lut(u32 offset,
		struct mdp_ar_gc_lut_data *lut_data);
static void pp_update_argc_lut(u32 offset,
		struct mdp_pgc_lut_data *config);

int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, u32 tbl_idx,
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
static int pp_mixer_setup(u32 disp_num, struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_mixer *mixer)
{
	u32 flags, offset, dspp_num, opmode = 0;
	struct mdp_pgc_lut_data *pgc_config;
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

	pp_sts = &mdss_pp_res->pp_dspp_sts[dspp_num];
	/* GC_LUT is in layer mixer */
	if (flags & PP_FLAGS_DIRTY_ARGC) {
		pgc_config = &mdss_pp_res->pgc_disp_cfg[disp_num];
		if (pgc_config->flags & MDP_PP_OPS_WRITE) {
			offset = MDSS_MDP_REG_LM_OFFSET(disp_num) +
				MDSS_MDP_REG_LM_GC_LUT_BASE;
			pp_update_argc_lut(offset, pgc_config);
		}
		if (pgc_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->argc_sts &= ~PP_STS_ENABLE;
		else if (pgc_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->argc_sts |= PP_STS_ENABLE;
		ctl->flush_bits |= BIT(6) << dspp_num; /* LAYER_MIXER */
	}
	/* update LM opmode if LM needs flush */
	if ((pp_sts->argc_sts & PP_STS_ENABLE) &&
		(ctl->flush_bits & (BIT(6) << dspp_num))) {
		offset = MDSS_MDP_REG_LM_OFFSET(dspp_num) +
			MDSS_MDP_REG_LM_OP_MODE;
		opmode = MDSS_MDP_REG_READ(offset);
		opmode |= (1 << 0); /* GC_LUT_EN */
		MDSS_MDP_REG_WRITE(offset, opmode);
	}
	return 0;
}
static int pp_dspp_setup(u32 disp_num, struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_mixer *mixer)
{
	u32 flags, base, offset, dspp_num, opmode = 0;
	struct mdp_pa_cfg_data *pa_config;
	struct mdp_pcc_cfg_data *pcc_config;
	struct mdp_igc_lut_data *igc_config;
	struct pp_sts_type *pp_sts;
	u32 tbl_idx;
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
	if (flags & PP_FLAGS_DIRTY_PCC) {
		pcc_config = &mdss_pp_res->pcc_disp_cfg[disp_num];
		if (pcc_config->ops & MDP_PP_OPS_WRITE) {
			offset = base + MDSS_MDP_REG_DSPP_PCC_BASE;
			pp_update_pcc_regs(offset, pcc_config);
		}
		if (pcc_config->ops & MDP_PP_OPS_DISABLE)
			pp_sts->pcc_sts &= ~PP_STS_ENABLE;
		else if (pcc_config->ops & MDP_PP_OPS_ENABLE)
			pp_sts->pcc_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->pcc_sts & PP_STS_ENABLE)
		opmode |= (1 << 4); /* PCC_EN */

	if (flags & PP_FLAGS_DIRTY_IGC) {
		igc_config = &mdss_pp_res->igc_disp_cfg[disp_num];
		if (igc_config->ops & MDP_PP_OPS_WRITE) {
			offset = MDSS_MDP_REG_IGC_DSPP_BASE;
			pp_update_igc_lut(igc_config, offset, dspp_num);
		}
		if (igc_config->ops & MDP_PP_IGC_FLAG_ROM0) {
			pp_sts->pcc_sts |= PP_STS_ENABLE;
			tbl_idx = 1;
		} else if (igc_config->ops & MDP_PP_IGC_FLAG_ROM1) {
			pp_sts->pcc_sts |= PP_STS_ENABLE;
			tbl_idx = 2;
		} else {
			tbl_idx = 0;
		}
		pp_sts->igc_tbl_idx = tbl_idx;
		if (igc_config->ops & MDP_PP_OPS_DISABLE)
			pp_sts->igc_sts &= ~PP_STS_ENABLE;
		else if (igc_config->ops & MDP_PP_OPS_ENABLE)
			pp_sts->igc_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->igc_sts & PP_STS_ENABLE) {
		opmode |= (1 << 0) | /* IGC_LUT_EN */
			      (pp_sts->igc_tbl_idx << 1);
	}

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
	if (ctl->mixer_left) {
		pp_mixer_setup(disp_num, ctl, ctl->mixer_left);
		pp_dspp_setup(disp_num, ctl, ctl->mixer_left);
	}
	if (ctl->mixer_right) {
		pp_mixer_setup(disp_num, ctl, ctl->mixer_right);
		pp_dspp_setup(disp_num, ctl, ctl->mixer_right);
	}
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
static int pp_get_dspp_num(u32 disp_num, u32 *dspp_num)
{
	int i;
	u32 mixer_cnt;
	u32 mixer_id[MDSS_MDP_MAX_LAYERMIXER];
	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!mixer_cnt)
		return -EPERM;

	/* only read the first mixer */
	for (i = 0; i < mixer_cnt; i++) {
		if (mixer_id[i] < MDSS_MDP_MAX_DSPP)
			break;
	}
	if (i >= mixer_cnt)
		return -EPERM;
	*dspp_num = mixer_id[i];
	return 0;
}

int mdss_mdp_pa_config(struct mdp_pa_cfg_data *config, u32 *copyback)
{
	int ret = 0;
	u32 pa_offset, disp_num, dspp_num = 0;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->flags & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto pa_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		pa_offset = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
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

static void pp_read_pcc_regs(u32 offset,
				struct mdp_pcc_cfg_data *cfg_ptr)
{
	cfg_ptr->r.c = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.c = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.c = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.r = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.r = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.r = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.g = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.g = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.g = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.b = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.b = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.b = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.rr = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.rr = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.rr = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.rg = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.rg = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.rg = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.rb = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.rb = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.rb = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.gg = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.gg = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.gg = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.gb = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.gb = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.gb = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.bb = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.bb = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.bb = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.rgb_0 = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.rgb_0 = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.rgb_0 = MDSS_MDP_REG_READ(offset + 8);
	offset += 0x10;

	cfg_ptr->r.rgb_1 = MDSS_MDP_REG_READ(offset);
	cfg_ptr->g.rgb_1 = MDSS_MDP_REG_READ(offset + 4);
	cfg_ptr->b.rgb_1 = MDSS_MDP_REG_READ(offset + 8);
}

static void pp_update_pcc_regs(u32 offset,
				struct mdp_pcc_cfg_data *cfg_ptr)
{
	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.c);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.c);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.c);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.r);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.r);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.r);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.g);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.g);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.g);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.b);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.b);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.b);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.rr);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.rr);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.rr);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.rg);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.rg);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.rg);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.rb);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.rb);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.rb);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.gg);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.gg);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.gg);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.gb);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.gb);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.gb);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.bb);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.bb);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.bb);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.rgb_0);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.rgb_0);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.rgb_0);
	offset += 0x10;

	MDSS_MDP_REG_WRITE(offset, cfg_ptr->r.rgb_1);
	MDSS_MDP_REG_WRITE(offset + 4, cfg_ptr->g.rgb_1);
	MDSS_MDP_REG_WRITE(offset + 8, cfg_ptr->b.rgb_1);
}

int mdss_mdp_pcc_config(struct mdp_pcc_cfg_data *config, u32 *copyback)
{
	int ret = 0;
	u32 base, disp_num, dspp_num = 0;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->ops & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto pcc_config_exit;
		}

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_PCC_BASE;
		pp_read_pcc_regs(base, config);
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_pp_res->pcc_disp_cfg[disp_num] = *config;
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_PCC;
	}

pcc_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;

}

static void pp_read_igc_lut(struct mdp_igc_lut_data *cfg,
				u32 offset, u32 blk_idx)
{
	int i;
	u32 data;

	/* INDEX_UPDATE & VALUE_UPDATEN */
	data = (3 << 24) | (((~(1 << blk_idx)) & 0x7) << 28);
	MDSS_MDP_REG_WRITE(offset, data);

	for (i = 0; i < cfg->len; i++)
		cfg->c0_c1_data[i] = MDSS_MDP_REG_READ(offset) & 0xFFF;

	offset += 0x4;
	MDSS_MDP_REG_WRITE(offset, data);
	for (i = 0; i < cfg->len; i++)
		cfg->c0_c1_data[i] |= (MDSS_MDP_REG_READ(offset) & 0xFFF) << 16;

	offset += 0x4;
	MDSS_MDP_REG_WRITE(offset, data);
	for (i = 0; i < cfg->len; i++)
		cfg->c2_data[i] = MDSS_MDP_REG_READ(offset) & 0xFFF;
}

static void pp_update_igc_lut(struct mdp_igc_lut_data *cfg,
				u32 offset, u32 blk_idx)
{
	int i;
	u32 data;
	/* INDEX_UPDATE */
	data = (1 << 25) | (((~(1 << blk_idx)) & 0x7) << 28);
	MDSS_MDP_REG_WRITE(offset, (cfg->c0_c1_data[0] & 0xFFF) | data);

	/* disable index update */
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		MDSS_MDP_REG_WRITE(offset, (cfg->c0_c1_data[i] & 0xFFF) | data);

	offset += 0x4;
	data |= (1 << 25);
	MDSS_MDP_REG_WRITE(offset, ((cfg->c0_c1_data[0] >> 16) & 0xFFF) | data);
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		MDSS_MDP_REG_WRITE(offset,
		((cfg->c0_c1_data[i] >> 16) & 0xFFF) | data);

	offset += 0x4;
	data |= (1 << 25);
	MDSS_MDP_REG_WRITE(offset, (cfg->c2_data[0] & 0xFFF) | data);
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		MDSS_MDP_REG_WRITE(offset, (cfg->c2_data[i] & 0xFFF) | data);
}

int mdss_mdp_igc_lut_config(struct mdp_igc_lut_data *config, u32 *copyback)
{
	int ret = 0;
	u32 tbl_idx, igc_offset, disp_num, dspp_num = 0;
	struct mdp_igc_lut_data local_cfg;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->ops & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto igc_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		if (config->ops & MDP_PP_IGC_FLAG_ROM0)
			tbl_idx = 1;
		else if (config->ops & MDP_PP_IGC_FLAG_ROM1)
			tbl_idx = 2;
		else
			tbl_idx = 0;
		igc_offset = MDSS_MDP_REG_IGC_DSPP_BASE + (0x10 * tbl_idx);
		local_cfg = *config;
		local_cfg.c0_c1_data =
			&mdss_pp_res->igc_lut_c0c1[disp_num][0];
		local_cfg.c2_data =
			&mdss_pp_res->igc_lut_c2[disp_num][0];
		pp_read_igc_lut(&local_cfg, igc_offset, dspp_num);
		if (copy_to_user(config->c0_c1_data, local_cfg.c2_data,
			config->len * sizeof(u32))) {
			ret = -EFAULT;
			goto igc_config_exit;
		}
		if (copy_to_user(config->c2_data, local_cfg.c0_c1_data,
			config->len * sizeof(u32))) {
			ret = -EFAULT;
			goto igc_config_exit;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		if (copy_from_user(&mdss_pp_res->igc_lut_c0c1[disp_num][0],
			config->c0_c1_data, config->len * sizeof(u32))) {
			ret = -EFAULT;
			goto igc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->igc_lut_c2[disp_num][0],
			config->c2_data, config->len * sizeof(u32))) {
			ret = -EFAULT;
			goto igc_config_exit;
		}
		mdss_pp_res->igc_disp_cfg[disp_num] = *config;
		mdss_pp_res->igc_disp_cfg[disp_num].c0_c1_data =
			&mdss_pp_res->igc_lut_c0c1[disp_num][0];
		mdss_pp_res->igc_disp_cfg[disp_num].c2_data =
			&mdss_pp_res->igc_lut_c2[disp_num][0];
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_IGC;
	}

igc_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
static void pp_update_gc_one_lut(u32 offset,
		struct mdp_ar_gc_lut_data *lut_data)
{
	int i, start_idx;

	start_idx = (MDSS_MDP_REG_READ(offset) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].x_start);
	for (i = 0; i < start_idx; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].x_start);
	offset += 4;
	start_idx = (MDSS_MDP_REG_READ(offset) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].slope);
	for (i = 0; i < start_idx; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].slope);
	offset += 4;
	start_idx = (MDSS_MDP_REG_READ(offset) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].offset);
	for (i = 0; i < start_idx; i++)
		MDSS_MDP_REG_WRITE(offset, lut_data[i].offset);
}
static void pp_update_argc_lut(u32 offset, struct mdp_pgc_lut_data *config)
{
	pp_update_gc_one_lut(offset, config->r_data);
	offset += 0x10;
	pp_update_gc_one_lut(offset, config->g_data);
	offset += 0x10;
	pp_update_gc_one_lut(offset, config->b_data);
}
static void pp_read_gc_one_lut(u32 offset,
		struct mdp_ar_gc_lut_data *gc_data)
{
	int i, start_idx, data;
	data = MDSS_MDP_REG_READ(offset);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].x_start = data & 0xFFF;

	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].x_start = data & 0xFFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].x_start = data & 0xFFF;
	}

	offset += 4;
	data = MDSS_MDP_REG_READ(offset);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].slope = data & 0x7FFF;
	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].slope = data & 0x7FFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].slope = data & 0x7FFF;
	}
	offset += 4;
	data = MDSS_MDP_REG_READ(offset);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].offset = data & 0x7FFF;
	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].offset = data & 0x7FFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = MDSS_MDP_REG_READ(offset);
		gc_data[i].offset = data & 0x7FFF;
	}
}

static int pp_read_argc_lut(struct mdp_pgc_lut_data *config, u32 offset)
{
	int ret = 0;
	pp_read_gc_one_lut(offset, config->r_data);
	offset += 0x10;
	pp_read_gc_one_lut(offset, config->g_data);
	offset += 0x10;
	pp_read_gc_one_lut(offset, config->b_data);
	return ret;
}
int mdss_mdp_argc_config(struct mdp_pgc_lut_data *config, u32 *copyback)
{
	int ret = 0;
	u32 argc_offset, disp_num, dspp_num = 0;
	struct mdp_pgc_lut_data local_cfg;
	u32 tbl_size;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	tbl_size = GC_LUT_SEGMENTS * sizeof(struct mdp_ar_gc_lut_data);
	if (config->flags & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto argc_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		argc_offset = MDSS_MDP_REG_LM_OFFSET(dspp_num) +
				MDSS_MDP_REG_LM_GC_LUT_BASE;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		local_cfg = *config;
		local_cfg.r_data =
			&mdss_pp_res->gc_lut_r[disp_num][0];
		local_cfg.g_data =
			&mdss_pp_res->gc_lut_g[disp_num][0];
		local_cfg.b_data =
			&mdss_pp_res->gc_lut_b[disp_num][0];
		pp_read_argc_lut(&local_cfg, argc_offset);
		if (copy_to_user(config->r_data,
			&mdss_pp_res->gc_lut_r[disp_num][0], tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_to_user(config->g_data,
			&mdss_pp_res->gc_lut_g[disp_num][0], tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_to_user(config->b_data,
			&mdss_pp_res->gc_lut_b[disp_num][0], tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		if (copy_from_user(&mdss_pp_res->gc_lut_r[disp_num][0],
			config->r_data, tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->gc_lut_g[disp_num][0],
			config->g_data, tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->gc_lut_b[disp_num][0],
			config->b_data, tbl_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		mdss_pp_res->pgc_disp_cfg[disp_num] = *config;
		mdss_pp_res->pgc_disp_cfg[disp_num].r_data =
			&mdss_pp_res->gc_lut_r[disp_num][0];
		mdss_pp_res->pgc_disp_cfg[disp_num].g_data =
			&mdss_pp_res->gc_lut_g[disp_num][0];
		mdss_pp_res->pgc_disp_cfg[disp_num].b_data =
			&mdss_pp_res->gc_lut_b[disp_num][0];
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_ARGC;
	}
argc_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
