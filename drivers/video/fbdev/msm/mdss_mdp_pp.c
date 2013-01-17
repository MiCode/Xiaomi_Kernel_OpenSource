/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/delay.h>

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
#define ENHIST_LUT_ENTRIES 256
#define HIST_V_SIZE	256

#define HIST_WAIT_TIMEOUT(frame) ((60 * HZ * (frame)) / 1000)
/* hist collect state */
enum {
	HIST_UNKNOWN,
	HIST_IDLE,
	HIST_RESET,
	HIST_START,
	HIST_READY,
};

struct pp_hist_col_info {
	u32 col_state;
	u32 col_en;
	u32 read_request;
	u32 hist_cnt_read;
	u32 hist_cnt_sent;
	u32 frame_cnt;
	u32 is_kick_ready;
	struct completion comp;
	u32 data[HIST_V_SIZE];
};

static u32 dither_matrix[16] = {
	15, 7, 13, 5, 3, 11, 1, 9, 12, 4, 14, 6, 0, 8, 2, 10};
static u32 dither_depth_map[9] = {
	0, 0, 0, 0, 0, 1, 2, 3, 3};

#define GAMUT_T0_SIZE	125
#define GAMUT_T1_SIZE	100
#define GAMUT_T2_SIZE	80
#define GAMUT_T3_SIZE	100
#define GAMUT_T4_SIZE	100
#define GAMUT_T5_SIZE	80
#define GAMUT_T6_SIZE	64
#define GAMUT_T7_SIZE	80
#define GAMUT_TOTAL_TABLE_SIZE (GAMUT_T0_SIZE + GAMUT_T1_SIZE + \
	GAMUT_T2_SIZE + GAMUT_T3_SIZE + GAMUT_T4_SIZE + \
	GAMUT_T5_SIZE + GAMUT_T6_SIZE + GAMUT_T7_SIZE)

struct pp_sts_type {
	u32 pa_sts;
	u32 pcc_sts;
	u32 igc_sts;
	u32 igc_tbl_idx;
	u32 argc_sts;
	u32 enhist_sts;
	u32 dither_sts;
	u32 gamut_sts;
	u32 pgc_sts;
};

#define PP_FLAGS_DIRTY_PA	0x1
#define PP_FLAGS_DIRTY_PCC	0x2
#define PP_FLAGS_DIRTY_IGC	0x4
#define PP_FLAGS_DIRTY_ARGC	0x8
#define PP_FLAGS_DIRTY_ENHIST	0x10
#define PP_FLAGS_DIRTY_DITHER	0x20
#define PP_FLAGS_DIRTY_GAMUT	0x40
#define PP_FLAGS_DIRTY_HIST_COL	0x80
#define PP_FLAGS_DIRTY_PGC	0x100

#define PP_STS_ENABLE	0x1
#define PP_STS_GAMUT_FIRST	0x2

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
	u32 enhist_lut[MDSS_BLOCK_DISP_NUM][ENHIST_LUT_ENTRIES];
	struct mdp_pa_cfg pa_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pcc_cfg_data pcc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_igc_lut_data igc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data argc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data pgc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_hist_lut_data enhist_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_dither_cfg_data dither_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_gamut_cfg_data gamut_disp_cfg[MDSS_BLOCK_DISP_NUM];
	uint16_t gamut_tbl[MDSS_BLOCK_DISP_NUM][GAMUT_TOTAL_TABLE_SIZE];
	struct pp_hist_col_info
		*hist_col[MDSS_BLOCK_DISP_NUM][MDSS_MDP_MAX_DSPP];
	u32 hist_data[MDSS_BLOCK_DISP_NUM][HIST_V_SIZE];
	/* physical info */
	struct pp_sts_type pp_dspp_sts[MDSS_MDP_MAX_DSPP];
	struct pp_hist_col_info dspp_hist[MDSS_MDP_MAX_DSPP];
};

static DEFINE_MUTEX(mdss_pp_mutex);
static DEFINE_SPINLOCK(mdss_hist_lock);
static DEFINE_MUTEX(mdss_mdp_hist_mutex);
static struct mdss_pp_res_type *mdss_pp_res;

static void pp_hist_read(u32 v_base, struct pp_hist_col_info *hist_info);
static void pp_update_pcc_regs(u32 offset,
				struct mdp_pcc_cfg_data *cfg_ptr);
static void pp_update_igc_lut(struct mdp_igc_lut_data *cfg,
				u32 offset, u32 blk_idx);
static void pp_update_gc_one_lut(u32 offset,
				struct mdp_ar_gc_lut_data *lut_data);
static void pp_update_argc_lut(u32 offset,
				struct mdp_pgc_lut_data *config);
static void pp_update_hist_lut(u32 offset, struct mdp_hist_lut_data *cfg);
static void pp_pa_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_pa_cfg *pa_config);
static void pp_pcc_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_pcc_cfg_data *pcc_config);
static void pp_igc_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_igc_lut_data *igc_config,
				u32 pipe_num);
static void pp_enhist_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_hist_lut_data *enhist_cfg);

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

static void pp_gamut_config(struct mdp_gamut_cfg_data *gamut_cfg,
				u32 base, struct pp_sts_type *pp_sts)
{
	u32 offset;
	int i, j;
	if (gamut_cfg->flags & MDP_PP_OPS_WRITE) {
		offset = base + MDSS_MDP_REG_DSPP_GAMUT_BASE;
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				MDSS_MDP_REG_WRITE(offset,
					(u32)gamut_cfg->r_tbl[i][j]);
			offset += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				MDSS_MDP_REG_WRITE(offset,
					(u32)gamut_cfg->g_tbl[i][j]);
			offset += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				MDSS_MDP_REG_WRITE(offset,
					(u32)gamut_cfg->b_tbl[i][j]);
			offset += 4;
		}
		if (gamut_cfg->gamut_first)
			pp_sts->gamut_sts |= PP_STS_GAMUT_FIRST;
	}

	if (gamut_cfg->flags & MDP_PP_OPS_DISABLE)
		pp_sts->gamut_sts &= ~PP_STS_ENABLE;
	else if (gamut_cfg->flags & MDP_PP_OPS_ENABLE)
		pp_sts->gamut_sts |= PP_STS_ENABLE;
}

static void pp_pa_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_pa_cfg *pa_config)
{
	if (flags & PP_FLAGS_DIRTY_PA) {
		if (pa_config->flags & MDP_PP_OPS_WRITE) {
			MDSS_MDP_REG_WRITE(base, pa_config->hue_adj);
			base += 4;
			MDSS_MDP_REG_WRITE(base, pa_config->sat_adj);
			base += 4;
			MDSS_MDP_REG_WRITE(base, pa_config->val_adj);
			base += 4;
			MDSS_MDP_REG_WRITE(base, pa_config->cont_adj);
		}
		if (pa_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->pa_sts &= ~PP_STS_ENABLE;
		else if (pa_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->pa_sts |= PP_STS_ENABLE;
	}
}

static void pp_pcc_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_pcc_cfg_data *pcc_config)
{
	if (flags & PP_FLAGS_DIRTY_PCC) {
		if (pcc_config->ops & MDP_PP_OPS_WRITE)
			pp_update_pcc_regs(base, pcc_config);

		if (pcc_config->ops & MDP_PP_OPS_DISABLE)
			pp_sts->pcc_sts &= ~PP_STS_ENABLE;
		else if (pcc_config->ops & MDP_PP_OPS_ENABLE)
			pp_sts->pcc_sts |= PP_STS_ENABLE;
	}
}

static void pp_igc_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_igc_lut_data *igc_config,
				u32 pipe_num)
{
	u32 tbl_idx;
	if (flags & PP_FLAGS_DIRTY_IGC) {
		if (igc_config->ops & MDP_PP_OPS_WRITE)
			pp_update_igc_lut(igc_config, base, pipe_num);

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
}

static void pp_enhist_config(unsigned long flags, u32 base,
				struct pp_sts_type *pp_sts,
				struct mdp_hist_lut_data *enhist_cfg)
{
	if (flags & PP_FLAGS_DIRTY_ENHIST) {
		if (enhist_cfg->ops & MDP_PP_OPS_WRITE)
			pp_update_hist_lut(base, enhist_cfg);

		if (enhist_cfg->ops & MDP_PP_OPS_DISABLE)
			pp_sts->enhist_sts &= ~PP_STS_ENABLE;
		else if (enhist_cfg->ops & MDP_PP_OPS_ENABLE)
			pp_sts->enhist_sts |= PP_STS_ENABLE;
	}
}

static int pp_vig_pipe_setup(struct mdss_mdp_pipe *pipe, u32 *op)
{
	struct pp_sts_type pp_sts;
	u32 opmode = 0, base = 0;
	unsigned long flags = 0;

	pr_debug("pnum=%x\n", pipe->num);

	if ((pipe->flags & MDP_OVERLAY_PP_CFG_EN) &&
		(pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_CSC_CFG)) {
			opmode |= !!(pipe->pp_cfg.csc_cfg.flags &
						MDP_CSC_FLAG_ENABLE) << 17;
			opmode |= !!(pipe->pp_cfg.csc_cfg.flags &
						MDP_CSC_FLAG_YUV_IN) << 18;
			opmode |= !!(pipe->pp_cfg.csc_cfg.flags &
						MDP_CSC_FLAG_YUV_OUT) << 19;
			/*
			 * TODO: Allow pipe to be programmed whenever new CSC is
			 * applied (i.e. dirty bit)
			 */
			if (pipe->play_cnt == 0)
				mdss_mdp_csc_setup_data(MDSS_MDP_BLOCK_SSPP,
				  pipe->num, 1, &pipe->pp_cfg.csc_cfg);
	} else {
		if (pipe->src_fmt->is_yuv)
			opmode |= (0 << 19) |	/* DST_DATA=RGB */
				  (1 << 18) |	/* SRC_DATA=YCBCR */
				  (1 << 17);	/* CSC_1_EN */
		/*
		 * TODO: Needs to be part of dirty bit logic: if there is a
		 * previously configured pipe need to re-configure CSC matrix
		 */
		if (pipe->play_cnt == 0) {
			mdss_mdp_csc_setup(MDSS_MDP_BLOCK_SSPP, pipe->num, 1,
					   MDSS_MDP_CSC_YUV2RGB);
		}
	}

	if (pipe->flags & MDP_OVERLAY_PP_CFG_EN) {
		if (pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_PA_CFG) {
			flags = PP_FLAGS_DIRTY_PA;
			base = MDSS_MDP_REG_SSPP_OFFSET(pipe->num) +
				MDSS_MDP_REG_VIG_PA_BASE;
			pp_sts.pa_sts = 0;
			pp_pa_config(flags, base, &pp_sts,
					&pipe->pp_cfg.pa_cfg);
			if (pp_sts.pa_sts & PP_STS_ENABLE)
				opmode |= (1 << 4); /* PA_EN */
		}
	}

	*op = opmode;

	return 0;
}

int mdss_mdp_pipe_pp_setup(struct mdss_mdp_pipe *pipe, u32 *op)
{
	int ret = 0;
	if (!pipe)
		return -ENODEV;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)
		ret = pp_vig_pipe_setup(pipe, op);
	else if (pipe->type == MDSS_MDP_PIPE_TYPE_RGB)
		ret = -EINVAL;
	else if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		ret = -EINVAL;

	return ret;
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
		pgc_config = &mdss_pp_res->argc_disp_cfg[disp_num];
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
	struct mdp_dither_cfg_data *dither_cfg;
	struct pp_hist_col_info *hist_info;
	struct mdp_pgc_lut_data *pgc_config;
	struct pp_sts_type *pp_sts;
	u32 data, col_state;
	unsigned long flag;
	int i;

	dspp_num = mixer->num;
	/* no corresponding dspp */
	if ((mixer->type != MDSS_MDP_MIXER_TYPE_INTF) ||
		(dspp_num >= MDSS_MDP_MAX_DSPP))
		return 0;
	base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num);
	hist_info = &mdss_pp_res->dspp_hist[dspp_num];
	if (hist_info->col_en) {
		/* HIST_EN & AUTO_CLEAR */
		opmode |= (1 << 16) | (1 << 17);
		mutex_lock(&mdss_mdp_hist_mutex);
		spin_lock_irqsave(&mdss_hist_lock, flag);
		col_state = hist_info->col_state;
		if (hist_info->is_kick_ready &&
				((col_state == HIST_IDLE) ||
				((false == hist_info->read_request) &&
						col_state == HIST_READY))) {
			/* Kick off collection */
			MDSS_MDP_REG_WRITE(base +
				MDSS_MDP_REG_DSPP_HIST_CTL_BASE, 1);
			hist_info->col_state = HIST_START;
		}
		hist_info->is_kick_ready = true;
		spin_unlock_irqrestore(&mdss_hist_lock, flag);
		mutex_unlock(&mdss_mdp_hist_mutex);
	}

	if (disp_num < MDSS_BLOCK_DISP_NUM)
		flags = mdss_pp_res->pp_disp_flags[disp_num];
	else
		flags = 0;

	/* nothing to update */
	if ((!flags) && (!(hist_info->col_en)))
		return 0;

	pp_sts = &mdss_pp_res->pp_dspp_sts[dspp_num];

	pp_pa_config(flags, base + MDSS_MDP_REG_DSPP_PA_BASE, pp_sts,
					&mdss_pp_res->pa_disp_cfg[disp_num]);

	pp_pcc_config(flags, base + MDSS_MDP_REG_DSPP_PCC_BASE, pp_sts,
					&mdss_pp_res->pcc_disp_cfg[disp_num]);

	pp_igc_config(flags, MDSS_MDP_REG_IGC_DSPP_BASE, pp_sts,
				&mdss_pp_res->igc_disp_cfg[disp_num], dspp_num);

	pp_enhist_config(flags, base + MDSS_MDP_REG_DSPP_HIST_LUT_BASE,
			pp_sts, &mdss_pp_res->enhist_disp_cfg[disp_num]);

	if (pp_sts->pa_sts & PP_STS_ENABLE)
		opmode |= (1 << 20); /* PA_EN */

	if (pp_sts->pcc_sts & PP_STS_ENABLE)
		opmode |= (1 << 4); /* PCC_EN */

	if (pp_sts->igc_sts & PP_STS_ENABLE) {
		opmode |= (1 << 0) | /* IGC_LUT_EN */
			      (pp_sts->igc_tbl_idx << 1);
	}

	if (pp_sts->enhist_sts & PP_STS_ENABLE) {
		opmode |= (1 << 19) | /* HIST_LUT_EN */
				  (1 << 20); /* PA_EN */
		if (!(pp_sts->pa_sts & PP_STS_ENABLE)) {
			/* Program default value */
			offset = base + MDSS_MDP_REG_DSPP_PA_BASE;
			MDSS_MDP_REG_WRITE(offset, 0);
			MDSS_MDP_REG_WRITE(offset + 4, 0);
			MDSS_MDP_REG_WRITE(offset + 8, 0);
			MDSS_MDP_REG_WRITE(offset + 12, 0);
		}
	}
	if (flags & PP_FLAGS_DIRTY_DITHER) {
		dither_cfg = &mdss_pp_res->dither_disp_cfg[disp_num];
		if (dither_cfg->flags & MDP_PP_OPS_WRITE) {
			offset = base + MDSS_MDP_REG_DSPP_DITHER_DEPTH;
			MDSS_MDP_REG_WRITE(offset,
			  dither_depth_map[dither_cfg->g_y_depth] |
			  (dither_depth_map[dither_cfg->b_cb_depth] << 2) |
			  (dither_depth_map[dither_cfg->r_cr_depth] << 4));
			offset += 0x14;
			for (i = 0; i << 16; i += 4) {
				data = dither_matrix[i] |
					(dither_matrix[i + 1] << 4) |
					(dither_matrix[i + 2] << 8) |
					(dither_matrix[i + 3] << 12);
				MDSS_MDP_REG_WRITE(offset, data);
				offset += 4;
			}
		}
		if (dither_cfg->flags & MDP_PP_OPS_DISABLE)
			pp_sts->dither_sts &= ~PP_STS_ENABLE;
		else if (dither_cfg->flags & MDP_PP_OPS_ENABLE)
			pp_sts->dither_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->dither_sts & PP_STS_ENABLE)
		opmode |= (1 << 8); /* DITHER_EN */
	if (flags & PP_FLAGS_DIRTY_GAMUT)
		pp_gamut_config(&mdss_pp_res->gamut_disp_cfg[disp_num], base,
				pp_sts);
	if (pp_sts->gamut_sts & PP_STS_ENABLE) {
		opmode |= (1 << 23); /* GAMUT_EN */
		if (pp_sts->gamut_sts & PP_STS_GAMUT_FIRST)
			opmode |= (1 << 24); /* GAMUT_ORDER */
	}

	if (flags & PP_FLAGS_DIRTY_PGC) {
		pgc_config = &mdss_pp_res->pgc_disp_cfg[disp_num];
		if (pgc_config->flags & MDP_PP_OPS_WRITE) {
			offset = base + MDSS_MDP_REG_DSPP_GC_BASE;
			pp_update_argc_lut(offset, pgc_config);
		}
		if (pgc_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->pgc_sts &= ~PP_STS_ENABLE;
		else if (pgc_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->pgc_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->pgc_sts & PP_STS_ENABLE)
		opmode |= (1 << 22);

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

/*
 * Set dirty and write bits on features that were enabled so they will be
 * reconfigured
 */
int mdss_mdp_pp_resume(u32 mixer_num)
{
	u32 flags = 0;
	struct pp_sts_type pp_sts;

	if (mixer_num >= MDSS_MDP_MAX_DSPP) {
		pr_warn("invalid mixer_num");
		return -EINVAL;
	}

	pp_sts = mdss_pp_res->pp_dspp_sts[mixer_num];

	if (pp_sts.pa_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PA;
		if (!(mdss_pp_res->pa_disp_cfg[mixer_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pa_disp_cfg[mixer_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.pcc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PCC;
		if (!(mdss_pp_res->pcc_disp_cfg[mixer_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pcc_disp_cfg[mixer_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.igc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_IGC;
		if (!(mdss_pp_res->igc_disp_cfg[mixer_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->igc_disp_cfg[mixer_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.argc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_ARGC;
		if (!(mdss_pp_res->argc_disp_cfg[mixer_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->argc_disp_cfg[mixer_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.enhist_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_ENHIST;
		if (!(mdss_pp_res->enhist_disp_cfg[mixer_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->enhist_disp_cfg[mixer_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.dither_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_DITHER;
		if (!(mdss_pp_res->dither_disp_cfg[mixer_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->dither_disp_cfg[mixer_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.gamut_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_GAMUT;
		if (!(mdss_pp_res->gamut_disp_cfg[mixer_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->gamut_disp_cfg[mixer_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.pgc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PGC;
		if (!(mdss_pp_res->pgc_disp_cfg[mixer_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pgc_disp_cfg[mixer_num].flags |=
				MDP_PP_OPS_WRITE;
	}

	mdss_pp_res->pp_disp_flags[mixer_num] = flags;
	return 0;
}

int mdss_mdp_pp_init(struct device *dev)
{
	int ret = 0;
	int i;
	u32 offset;
	uint32_t data[ENHIST_LUT_ENTRIES];

	mutex_lock(&mdss_pp_mutex);
	if (!mdss_pp_res) {
		mdss_pp_res = devm_kzalloc(dev, sizeof(*mdss_pp_res),
				GFP_KERNEL);
		if (mdss_pp_res == NULL) {
			pr_err("%s mdss_pp_res allocation failed!", __func__);
			ret = -ENOMEM;
		}

		for (i = 0; i < ENHIST_LUT_ENTRIES; i++)
			data[i] = i;

		/* Initialize Histogram LUT for all DSPPs */
		for (i = 0; i < MDSS_MDP_MAX_DSPP; i++) {
			offset = MDSS_MDP_REG_DSPP_OFFSET(i) +
						MDSS_MDP_REG_DSPP_HIST_LUT_BASE;
			mdss_pp_res->enhist_disp_cfg[i].data = data;
			pp_update_hist_lut(offset,
					&mdss_pp_res->enhist_disp_cfg[i]);
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

	if (config->pa_data.flags & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto pa_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		pa_offset = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_PA_BASE;
		config->pa_data.hue_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->pa_data.sat_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->pa_data.val_adj = MDSS_MDP_REG_READ(pa_offset);
		pa_offset += 4;
		config->pa_data.cont_adj = MDSS_MDP_REG_READ(pa_offset);
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_pp_res->pa_disp_cfg[disp_num] = config->pa_data;
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

/* Note: Assumes that its inputs have been checked by calling function */
static void pp_update_hist_lut(u32 offset, struct mdp_hist_lut_data *cfg)
{
	int i;
	for (i = 0; i < ENHIST_LUT_ENTRIES; i++)
		MDSS_MDP_REG_WRITE(offset, cfg->data[i]);
	/* swap */
	MDSS_MDP_REG_WRITE(offset + 4, 1);
}

int mdss_mdp_argc_config(struct mdp_pgc_lut_data *config, u32 *copyback)
{
	int ret = 0;
	u32 argc_offset = 0, disp_num, dspp_num = 0;
	struct mdp_pgc_lut_data local_cfg;
	struct mdp_pgc_lut_data *pgc_ptr;
	u32 tbl_size;

	if ((PP_BLOCK(config->block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(config->block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);

	disp_num = PP_BLOCK(config->block) - MDP_LOGICAL_BLOCK_DISP_0;
	switch (config->block & MDSS_PP_LOCATION_MASK) {
	case MDSS_PP_LM_CFG:
		argc_offset = MDSS_MDP_REG_LM_OFFSET(dspp_num) +
			MDSS_MDP_REG_LM_GC_LUT_BASE;
		pgc_ptr = &mdss_pp_res->argc_disp_cfg[disp_num];
		mdss_pp_res->pp_disp_flags[disp_num] |=
			PP_FLAGS_DIRTY_ARGC;
		break;
	case MDSS_PP_DSPP_CFG:
		argc_offset = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
					MDSS_MDP_REG_DSPP_GC_BASE;
		pgc_ptr = &mdss_pp_res->pgc_disp_cfg[disp_num];
		mdss_pp_res->pp_disp_flags[disp_num] |=
			PP_FLAGS_DIRTY_PGC;
		break;
	default:
		goto argc_config_exit;
		break;
	}

	tbl_size = GC_LUT_SEGMENTS * sizeof(struct mdp_ar_gc_lut_data);

	if (config->flags & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto argc_config_exit;
		}
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

		*pgc_ptr = *config;
		pgc_ptr->r_data =
			&mdss_pp_res->gc_lut_r[disp_num][0];
		pgc_ptr->g_data =
			&mdss_pp_res->gc_lut_g[disp_num][0];
		pgc_ptr->b_data =
			&mdss_pp_res->gc_lut_b[disp_num][0];
	}
argc_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
int mdss_mdp_hist_lut_config(struct mdp_hist_lut_data *config, u32 *copyback)
{
	int i, ret = 0;
	u32 hist_offset, disp_num, dspp_num = 0;

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
			goto enhist_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		hist_offset = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_HIST_LUT_BASE;
		for (i = 0; i < ENHIST_LUT_ENTRIES; i++)
			mdss_pp_res->enhist_lut[disp_num][i] =
				MDSS_MDP_REG_READ(hist_offset);
		if (copy_to_user(config->data,
			&mdss_pp_res->enhist_lut[disp_num][0],
			ENHIST_LUT_ENTRIES * sizeof(u32))) {
			ret = -EFAULT;
			goto enhist_config_exit;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		if (copy_from_user(&mdss_pp_res->enhist_lut[disp_num][0],
			config->data, ENHIST_LUT_ENTRIES * sizeof(u32))) {
			ret = -EFAULT;
			goto enhist_config_exit;
		}
		mdss_pp_res->enhist_disp_cfg[disp_num] = *config;
		mdss_pp_res->enhist_disp_cfg[disp_num].data =
			&mdss_pp_res->enhist_lut[disp_num][0];
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_ENHIST;
	}
enhist_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}

int mdss_mdp_dither_config(struct mdp_dither_cfg_data *config, u32 *copyback)
{
	u32 disp_num;
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;
	if (config->flags & MDP_PP_OPS_READ)
		return -ENOTSUPP;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
	mdss_pp_res->dither_disp_cfg[disp_num] = *config;
	mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_DITHER;
	mutex_unlock(&mdss_pp_mutex);
	return 0;
}

int mdss_mdp_gamut_config(struct mdp_gamut_cfg_data *config, u32 *copyback)
{
	int i, j, size_total = 0, ret = 0;
	u32 offset, disp_num, dspp_num = 0;
	uint16_t *tbl_off;
	struct mdp_gamut_cfg_data local_cfg;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;
	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++)
		size_total += config->tbl_size[i];
	if (size_total != GAMUT_TOTAL_TABLE_SIZE)
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->flags & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto gamut_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		offset = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_GAMUT_BASE;
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < config->tbl_size[i]; j++)
				config->r_tbl[i][j] =
					(u16)MDSS_MDP_REG_READ(offset);
			offset += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < config->tbl_size[i]; j++)
				config->g_tbl[i][j] =
					(u16)MDSS_MDP_REG_READ(offset);
			offset += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < config->tbl_size[i]; j++)
				config->b_tbl[i][j] =
					(u16)MDSS_MDP_REG_READ(offset);
			offset += 4;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		local_cfg = *config;
		tbl_off = mdss_pp_res->gamut_tbl[disp_num];
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			local_cfg.r_tbl[i] = tbl_off;
			if (copy_from_user(tbl_off, config->r_tbl[i],
				config->tbl_size[i] * sizeof(uint16_t))) {
				ret = -EFAULT;
				goto gamut_config_exit;
			}
			tbl_off += local_cfg.tbl_size[i];
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			local_cfg.g_tbl[i] = tbl_off;
			if (copy_from_user(tbl_off, config->g_tbl[i],
				config->tbl_size[i] * sizeof(uint16_t))) {
				ret = -EFAULT;
				goto gamut_config_exit;
			}
			tbl_off += local_cfg.tbl_size[i];
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			local_cfg.b_tbl[i] = tbl_off;
			if (copy_from_user(tbl_off, config->b_tbl[i],
				config->tbl_size[i] * sizeof(uint16_t))) {
				ret = -EFAULT;
				goto gamut_config_exit;
			}
			tbl_off += local_cfg.tbl_size[i];
		}
		mdss_pp_res->gamut_disp_cfg[disp_num] = local_cfg;
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_GAMUT;
	}
gamut_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
static void pp_hist_read(u32 v_base, struct pp_hist_col_info *hist_info)
{
	int i, i_start;
	u32 data;
	data = MDSS_MDP_REG_READ(v_base);
	i_start = data >> 24;
	hist_info->data[i_start] = data & 0xFFFFFF;
	for (i = i_start + 1; i < HIST_V_SIZE; i++)
		hist_info->data[i] = MDSS_MDP_REG_READ(v_base) & 0xFFFFFF;
	for (i = 0; i < i_start - 1; i++)
		hist_info->data[i] = MDSS_MDP_REG_READ(v_base) & 0xFFFFFF;
	hist_info->hist_cnt_read++;
}

int mdss_mdp_histogram_start(struct mdp_histogram_start_req *req)
{
	u32 ctl_base, done_shift_bit;
	struct pp_hist_col_info *hist_info;
	int i, ret = 0;
	u32 disp_num, dspp_num = 0;
	u32 mixer_cnt, mixer_id[MDSS_MDP_MAX_LAYERMIXER];
	unsigned long flag;

	if ((req->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(req->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_mdp_hist_mutex);
	disp_num = req->block - MDP_LOGICAL_BLOCK_DISP_0;
	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!mixer_cnt) {
		pr_err("%s, no dspp connects to disp %d",
			__func__, disp_num);
		ret = -EPERM;
		goto hist_start_exit;
	}
	if (mixer_cnt >= MDSS_MDP_MAX_DSPP) {
		pr_err("%s, Too many dspp connects to disp %d",
			__func__, mixer_cnt);
		ret = -EPERM;
		goto hist_start_exit;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	for (i = 0; i < mixer_cnt; i++) {
		dspp_num = mixer_id[i];
		hist_info = &mdss_pp_res->dspp_hist[dspp_num];
		done_shift_bit = (dspp_num * 4) + 12;
		/* check if it is idle */
		if (hist_info->col_en) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			pr_info("%s Hist collection has already been enabled %d",
				__func__, dspp_num);
			goto hist_start_exit;
		}
		spin_lock_irqsave(&mdss_hist_lock, flag);
		hist_info->frame_cnt = req->frame_cnt;
		init_completion(&hist_info->comp);
		hist_info->hist_cnt_read = 0;
		hist_info->hist_cnt_sent = 0;
		hist_info->read_request = false;
		hist_info->col_state = HIST_RESET;
		hist_info->col_en = true;
		hist_info->is_kick_ready = false;
		spin_unlock_irqrestore(&mdss_hist_lock, flag);
		mdss_pp_res->hist_col[disp_num][i] =
			&mdss_pp_res->dspp_hist[dspp_num];
		mdss_mdp_hist_irq_enable(3 << done_shift_bit);
		ctl_base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
		MDSS_MDP_REG_WRITE(ctl_base + 8, req->frame_cnt);
		/* Kick out reset start */
		MDSS_MDP_REG_WRITE(ctl_base + 4, 1);
	}
	for (i = mixer_cnt; i < MDSS_MDP_MAX_DSPP; i++)
		mdss_pp_res->hist_col[disp_num][i] = 0;
	mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_HIST_COL;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
hist_start_exit:
	mutex_unlock(&mdss_mdp_hist_mutex);
	return ret;
}

int mdss_mdp_histogram_stop(u32 block)
{
	int i, ret = 0;
	u32 dspp_num, disp_num, ctl_base, done_bit;
	struct pp_hist_col_info *hist_info;
	u32 mixer_cnt, mixer_id[MDSS_MDP_MAX_LAYERMIXER];
	unsigned long flag;

	if ((block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_mdp_hist_mutex);
	disp_num = block - MDP_LOGICAL_BLOCK_DISP_0;
	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!mixer_cnt) {
		pr_err("%s, no dspp connects to disp %d",
			__func__, disp_num);
		ret = -EPERM;
		goto hist_stop_exit;
	}
	if (mixer_cnt >= MDSS_MDP_MAX_DSPP) {
		pr_err("%s, Too many dspp connects to disp %d",
			__func__, mixer_cnt);
		ret = -EPERM;
		goto hist_stop_exit;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	for (i = 0; i < mixer_cnt; i++) {
		dspp_num = mixer_id[i];
		hist_info = &mdss_pp_res->dspp_hist[dspp_num];
		done_bit = 3 << ((dspp_num * 4) + 12);
		ctl_base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
		if (hist_info->col_en == false) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			goto hist_stop_exit;
		}
		complete_all(&hist_info->comp);
		spin_lock_irqsave(&mdss_hist_lock, flag);
		hist_info->col_en = false;
		hist_info->col_state = HIST_UNKNOWN;
		hist_info->is_kick_ready = false;
		spin_unlock_irqrestore(&mdss_hist_lock, flag);
		mdss_mdp_hist_irq_disable(done_bit);
		MDSS_MDP_REG_WRITE(ctl_base, (1 << 1));/* cancel */
	}
	for (i = 0; i < MDSS_MDP_MAX_DSPP; i++)
		mdss_pp_res->hist_col[disp_num][i] = 0;
	mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_HIST_COL;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
hist_stop_exit:
	mutex_unlock(&mdss_mdp_hist_mutex);
	return ret;
}

int mdss_mdp_hist_collect(struct fb_info *info,
		  struct mdp_histogram_data *hist, u32 *hist_data_addr)
{
	int i, j, wait_ret, ret = 0;
	u32 timeout, v_base;
	struct pp_hist_col_info *hist_info;
	u32 dspp_num, disp_num, ctl_base;
	u32 mixer_cnt, mixer_id[MDSS_MDP_MAX_LAYERMIXER];
	unsigned long flag;

	if ((hist->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(hist->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_mdp_hist_mutex);
	disp_num = hist->block - MDP_LOGICAL_BLOCK_DISP_0;
	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!mixer_cnt) {
		pr_err("%s, no dspp connects to disp %d",
			__func__, disp_num);
		ret = -EPERM;
		goto hist_collect_exit;
	}
	if (mixer_cnt >= MDSS_MDP_MAX_DSPP) {
		pr_err("%s, Too many dspp connects to disp %d",
			__func__, mixer_cnt);
		ret = -EPERM;
		goto hist_collect_exit;
	}
	hist_info = &mdss_pp_res->dspp_hist[0];
	for (i = 0; i < mixer_cnt; i++) {
		dspp_num = mixer_id[i];
		hist_info = &mdss_pp_res->dspp_hist[dspp_num];
		ctl_base = MDSS_MDP_REG_DSPP_OFFSET(dspp_num) +
			  MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
		if ((hist_info->col_en == 0) ||
			(hist_info->col_state == HIST_UNKNOWN)) {
			ret = -EINVAL;
			goto hist_collect_exit;
		}
		spin_lock_irqsave(&mdss_hist_lock, flag);
		/* wait for hist done if cache has no data */
		if (hist_info->col_state != HIST_READY) {
			hist_info->read_request = true;
			spin_unlock_irqrestore(&mdss_hist_lock, flag);
			timeout = HIST_WAIT_TIMEOUT(hist_info->frame_cnt);
			mutex_unlock(&mdss_mdp_hist_mutex);
			wait_ret = wait_for_completion_killable_timeout(
					&(hist_info->comp), timeout);

			mutex_lock(&mdss_mdp_hist_mutex);
			if (wait_ret == 0) {
				ret = -ETIMEDOUT;
				spin_lock_irqsave(&mdss_hist_lock, flag);
				pr_debug("bin collection timedout, state %d",
							hist_info->col_state);
				/*
				 * When the histogram has timed out (usually
				 * underrun) change the SW state back to idle
				 * since histogram hardware will have done the
				 * same. Histogram data also needs to be
				 * cleared in this case, which is done by the
				 * histogram being read (triggered by READY
				 * state, which also moves the histogram SW back
				 * to IDLE).
				 */
				hist_info->col_state = HIST_READY;
				spin_unlock_irqrestore(&mdss_hist_lock, flag);
			} else if (wait_ret < 0) {
				ret = -EINTR;
				pr_debug("%s: bin collection interrupted",
						__func__);
				goto hist_collect_exit;
			}
			if (hist_info->col_state != HIST_READY) {
				ret = -ENODATA;
				pr_debug("%s: collection state is not ready: %d",
						__func__, hist_info->col_state);
				goto hist_collect_exit;
			}
		} else {
			spin_unlock_irqrestore(&mdss_hist_lock, flag);
		}
		spin_lock_irqsave(&mdss_hist_lock, flag);
		if (hist_info->col_state == HIST_READY) {
			spin_unlock_irqrestore(&mdss_hist_lock, flag);
			v_base = ctl_base + 0x1C;
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
			pp_hist_read(v_base, hist_info);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			spin_lock_irqsave(&mdss_hist_lock, flag);
			hist_info->read_request = false;
			hist_info->col_state = HIST_IDLE;
		}
		spin_unlock_irqrestore(&mdss_hist_lock, flag);
	}
	if (mixer_cnt > 1) {
		memset(&mdss_pp_res->hist_data[disp_num][0],
			0, HIST_V_SIZE * sizeof(u32));
		for (i = 0; i < mixer_cnt; i++) {
			dspp_num = mixer_id[i];
			hist_info = &mdss_pp_res->dspp_hist[dspp_num];
			for (j = 0; j < HIST_V_SIZE; j++)
				mdss_pp_res->hist_data[disp_num][i] +=
					hist_info->data[i];
		}
		*hist_data_addr = (u32)&mdss_pp_res->hist_data[disp_num][0];
	} else {
		*hist_data_addr = (u32)hist_info->data;
	}
	hist_info->hist_cnt_sent++;
hist_collect_exit:
	mutex_unlock(&mdss_mdp_hist_mutex);
	return ret;
}
void mdss_mdp_hist_intr_done(u32 isr)
{
	u32 isr_blk, blk_idx;
	struct pp_hist_col_info *hist_info;
	isr &= 0x333333;
	while (isr != 0) {
		if (isr & 0xFFF000) {
			if (isr & 0x3000) {
				blk_idx = 0;
				isr_blk = (isr >> 12) & 0x3;
				isr &= ~0x3000;
			} else if (isr & 0x30000) {
				blk_idx = 1;
				isr_blk = (isr >> 16) & 0x3;
				isr &= ~0x30000;
			} else {
				blk_idx = 2;
				isr_blk = (isr >> 20) & 0x3;
				isr &= ~0x300000;
			}
			hist_info = &mdss_pp_res->dspp_hist[blk_idx];
		} else {
			if (isr & 0x3) {
				blk_idx = 0;
				isr_blk = isr & 0x3;
				isr &= ~0x3;
			} else if (isr & 0x30) {
				blk_idx = 1;
				isr_blk = (isr >> 4) & 0x3;
				isr &= ~0x30;
			} else {
				blk_idx = 2;
				isr_blk = (isr >> 8) & 0x3;
				isr &= ~0x300;
			}
			/* SSPP block, not support yet*/
			continue;
		}
		/* Histogram Done Interrupt */
		if ((isr_blk & 0x1) &&
			(hist_info->col_en)) {
			spin_lock(&mdss_hist_lock);
			hist_info->col_state = HIST_READY;
			spin_unlock(&mdss_hist_lock);
			if (hist_info->read_request)
				complete(&hist_info->comp);
		}
		/* Histogram Reset Done Interrupt */
		if ((isr_blk & 0x2) &&
			(hist_info->col_en)) {
				spin_lock(&mdss_hist_lock);
				hist_info->col_state = HIST_IDLE;
				spin_unlock(&mdss_hist_lock);
		}
	};
}
