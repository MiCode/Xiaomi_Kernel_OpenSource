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
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

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

#define HIST_WAIT_TIMEOUT(frame) ((75 * HZ * (frame)) / 1000)
/* hist collect state */
enum {
	HIST_UNKNOWN,
	HIST_IDLE,
	HIST_RESET,
	HIST_START,
	HIST_READY,
};

static u32 dither_matrix[16] = {
	15, 7, 13, 5, 3, 11, 1, 9, 12, 4, 14, 6, 0, 8, 2, 10};
static u32 dither_depth_map[9] = {
	0, 0, 0, 0, 0, 1, 2, 3, 3};

static u32 igc_limited[IGC_LUT_ENTRIES] = {
	16777472, 17826064, 18874656, 19923248,
	19923248, 20971840, 22020432, 23069024,
	24117616, 25166208, 26214800, 26214800,
	27263392, 28311984, 29360576, 30409168,
	31457760, 32506352, 32506352, 33554944,
	34603536, 35652128, 36700720, 37749312,
	38797904, 38797904, 39846496, 40895088,
	41943680, 42992272, 44040864, 45089456,
	45089456, 46138048, 47186640, 48235232,
	49283824, 50332416, 51381008, 51381008,
	52429600, 53478192, 54526784, 55575376,
	56623968, 57672560, 58721152, 58721152,
	59769744, 60818336, 61866928, 62915520,
	63964112, 65012704, 65012704, 66061296,
	67109888, 68158480, 69207072, 70255664,
	71304256, 71304256, 72352848, 73401440,
	74450032, 75498624, 76547216, 77595808,
	77595808, 78644400, 79692992, 80741584,
	81790176, 82838768, 83887360, 83887360,
	84935952, 85984544, 87033136, 88081728,
	89130320, 90178912, 90178912, 91227504,
	92276096, 93324688, 94373280, 95421872,
	96470464, 96470464, 97519056, 98567648,
	99616240, 100664832, 101713424, 102762016,
	102762016, 103810608, 104859200, 105907792,
	106956384, 108004976, 109053568, 109053568,
	110102160, 111150752, 112199344, 113247936,
	114296528, 115345120, 115345120, 116393712,
	117442304, 118490896, 119539488, 120588080,
	121636672, 121636672, 122685264, 123733856,
	124782448, 125831040, 126879632, 127928224,
	127928224, 128976816, 130025408, 131074000,
	132122592, 133171184, 134219776, 135268368,
	135268368, 136316960, 137365552, 138414144,
	139462736, 140511328, 141559920, 141559920,
	142608512, 143657104, 144705696, 145754288,
	146802880, 147851472, 147851472, 148900064,
	149948656, 150997248, 152045840, 153094432,
	154143024, 154143024, 155191616, 156240208,
	157288800, 158337392, 159385984, 160434576,
	160434576, 161483168, 162531760, 163580352,
	164628944, 165677536, 166726128, 166726128,
	167774720, 168823312, 169871904, 170920496,
	171969088, 173017680, 173017680, 174066272,
	175114864, 176163456, 177212048, 178260640,
	179309232, 179309232, 180357824, 181406416,
	182455008, 183503600, 184552192, 185600784,
	185600784, 186649376, 187697968, 188746560,
	189795152, 190843744, 191892336, 191892336,
	192940928, 193989520, 195038112, 196086704,
	197135296, 198183888, 198183888, 199232480,
	200281072, 201329664, 202378256, 203426848,
	204475440, 204475440, 205524032, 206572624,
	207621216, 208669808, 209718400, 210766992,
	211815584, 211815584, 212864176, 213912768,
	214961360, 216009952, 217058544, 218107136,
	218107136, 219155728, 220204320, 221252912,
	222301504, 223350096, 224398688, 224398688,
	225447280, 226495872, 227544464, 228593056,
	229641648, 230690240, 230690240, 231738832,
	232787424, 233836016, 234884608, 235933200,
	236981792, 236981792, 238030384, 239078976,
	240127568, 241176160, 242224752, 243273344,
	243273344, 244321936, 245370528, 246419120};

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

#define PP_FLAGS_DIRTY_PA	0x1
#define PP_FLAGS_DIRTY_PCC	0x2
#define PP_FLAGS_DIRTY_IGC	0x4
#define PP_FLAGS_DIRTY_ARGC	0x8
#define PP_FLAGS_DIRTY_ENHIST	0x10
#define PP_FLAGS_DIRTY_DITHER	0x20
#define PP_FLAGS_DIRTY_GAMUT	0x40
#define PP_FLAGS_DIRTY_HIST_COL	0x80
#define PP_FLAGS_DIRTY_PGC	0x100
#define PP_FLAGS_DIRTY_SHARP	0x200

#define PP_STS_ENABLE	0x1
#define PP_STS_GAMUT_FIRST	0x2

#define PP_AD_STATE_INIT	0x2
#define PP_AD_STATE_CFG		0x4
#define PP_AD_STATE_DATA	0x8
#define PP_AD_STATE_RUN		0x10
#define PP_AD_STATE_VSYNC	0x20
#define PP_AD_STATE_BL_LIN	0x40

#define PP_AD_STATE_IS_INITCFG(st)	(((st) & PP_AD_STATE_INIT) &&\
						((st) & PP_AD_STATE_CFG))

#define PP_AD_STATE_IS_READY(st)	(((st) & PP_AD_STATE_INIT) &&\
						((st) & PP_AD_STATE_CFG) &&\
						((st) & PP_AD_STATE_DATA))

#define PP_AD_STS_DIRTY_INIT	0x2
#define PP_AD_STS_DIRTY_CFG	0x4
#define PP_AD_STS_DIRTY_DATA	0x8
#define PP_AD_STS_DIRTY_VSYNC	0x10

#define PP_AD_STS_IS_DIRTY(sts) (((sts) & PP_AD_STS_DIRTY_INIT) ||\
					((sts) & PP_AD_STS_DIRTY_CFG))

/* Bits 0 and 1 */
#define MDSS_AD_INPUT_AMBIENT	(0x03)
/* Bits 3 and 7 */
#define MDSS_AD_INPUT_STRENGTH	(0x88)
/*
 * Check data by shifting by mode to see if it matches to the
 * MDSS_AD_INPUT_* bitfields
 */
#define MDSS_AD_MODE_DATA_MATCH(mode, data) ((1 << (mode)) & (data))
#define MDSS_AD_RUNNING_AUTO_BL(ad) (((ad)->state & PP_AD_STATE_RUN) &&\
				((ad)->cfg.mode == MDSS_AD_MODE_AUTO_BL))
#define MDSS_AD_RUNNING_AUTO_STR(ad) (((ad)->state & PP_AD_STATE_RUN) &&\
				((ad)->cfg.mode == MDSS_AD_MODE_AUTO_STR))

#define SHARP_STRENGTH_DEFAULT	32
#define SHARP_EDGE_THR_DEFAULT	112
#define SHARP_SMOOTH_THR_DEFAULT	8
#define SHARP_NOISE_THR_DEFAULT	2

#define MDP_PP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_SPDM,		\
		.dst = MSM_BUS_SLAVE_IMEM_CFG,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

#define SZ_37_5M (37500000 * 8)

static struct msm_bus_vectors mdp_pp_bus_vectors[] = {
	MDP_PP_BUS_VECTOR_ENTRY(0, 0),
	MDP_PP_BUS_VECTOR_ENTRY(0, SZ_37_5M),
};
static struct msm_bus_paths mdp_pp_bus_usecases[ARRAY_SIZE(mdp_pp_bus_vectors)];
static struct msm_bus_scale_pdata mdp_pp_bus_scale_table = {
	.usecase = mdp_pp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_pp_bus_usecases),
	.name = "mdss_pp",
};

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
	u32 hist_data[MDSS_BLOCK_DISP_NUM][HIST_V_SIZE];
	/* physical info */
	struct pp_sts_type pp_disp_sts[MDSS_BLOCK_DISP_NUM];
	struct pp_hist_col_info dspp_hist[MDSS_MDP_MAX_DSPP];
};

static DEFINE_MUTEX(mdss_pp_mutex);
static struct mdss_pp_res_type *mdss_pp_res;

static void pp_hist_read(char __iomem *v_addr,
				struct pp_hist_col_info *hist_info);
static int pp_histogram_setup(u32 *op, u32 block, struct mdss_mdp_mixer *mix);
static int pp_histogram_disable(struct pp_hist_col_info *hist_info,
					u32 done_bit, char __iomem *ctl_base);
static void pp_update_pcc_regs(char __iomem *addr,
				struct mdp_pcc_cfg_data *cfg_ptr);
static void pp_update_igc_lut(struct mdp_igc_lut_data *cfg,
				char __iomem *addr, u32 blk_idx);
static void pp_update_gc_one_lut(char __iomem *addr,
				struct mdp_ar_gc_lut_data *lut_data,
				uint8_t num_stages);
static void pp_update_argc_lut(char __iomem *addr,
				struct mdp_pgc_lut_data *config);
static void pp_update_hist_lut(char __iomem *base,
				struct mdp_hist_lut_data *cfg);
static void pp_pa_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_pa_cfg *pa_config);
static void pp_pcc_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_pcc_cfg_data *pcc_config);
static void pp_igc_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_igc_lut_data *igc_config,
				u32 pipe_num);
static void pp_enhist_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_hist_lut_data *enhist_cfg);
static void pp_sharp_config(char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_sharp_cfg *sharp_config);
static int mdss_ad_init_checks(struct msm_fb_data_type *mfd);
static int mdss_mdp_get_ad(struct msm_fb_data_type *mfd,
					struct mdss_ad_info **ad);
static int pp_update_ad_input(struct msm_fb_data_type *mfd);
static void pp_ad_vsync_handler(struct mdss_mdp_ctl *ctl, ktime_t t);
static void pp_ad_cfg_write(struct mdss_ad_info *ad);
static void pp_ad_init_write(struct mdss_ad_info *ad);
static void pp_ad_input_write(struct mdss_ad_info *ad, u32 bl_lvl);
static int mdss_mdp_ad_setup(struct msm_fb_data_type *mfd);
static void pp_ad_cfg_lut(char __iomem *addr, u32 *data);

static u32 last_sts, last_state;

int mdss_mdp_csc_setup_data(u32 block, u32 blk_idx, u32 tbl_idx,
				   struct mdp_csc_cfg *data)
{
	int i, ret = 0;
	char __iomem *base, *addr;
	u32 val = 0;
	struct mdss_data_type *mdata;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl;


	if (data == NULL) {
		pr_err("no csc matrix specified\n");
		return -EINVAL;
	}

	mdata = mdss_mdp_get_mdata();
	switch (block) {
	case MDSS_MDP_BLOCK_SSPP:
		if (blk_idx < mdata->nvig_pipes) {
			pipe = mdata->vig_pipes + blk_idx;
			base = pipe->base;
			if (tbl_idx == 1)
				base += MDSS_MDP_REG_VIG_CSC_1_BASE;
			else
				base += MDSS_MDP_REG_VIG_CSC_0_BASE;
		} else {
			ret = -EINVAL;
		}
		break;
	case MDSS_MDP_BLOCK_WB:
		if (blk_idx < mdata->nctl) {
			ctl = mdata->ctl_off + blk_idx;
			base = ctl->wb_base + MDSS_MDP_REG_WB_CSC_BASE;
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

	addr = base + CSC_MV_OFF;
	for (i = 0; i < 9; i++) {
		if (i & 0x1) {
			val |= data->csc_mv[i] << 16;
			writel_relaxed(val, addr);
			addr += sizeof(u32 *);
		} else {
			val = data->csc_mv[i];
		}
	}
	writel_relaxed(val, addr); /* COEFF_33 */

	addr = base + CSC_BV_OFF;
	for (i = 0; i < 3; i++) {
		writel_relaxed(data->csc_pre_bv[i], addr);
		writel_relaxed(data->csc_post_bv[i], addr + CSC_POST_OFF);
		addr += sizeof(u32 *);
	}

	addr = base + CSC_LV_OFF;
	for (i = 0; i < 6; i += 2) {
		val = (data->csc_pre_lv[i] << 8) | data->csc_pre_lv[i+1];
		writel_relaxed(val, addr);

		val = (data->csc_post_lv[i] << 8) | data->csc_post_lv[i+1];
		writel_relaxed(val, addr + CSC_POST_OFF);
		addr += sizeof(u32 *);
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
				char __iomem *base, struct pp_sts_type *pp_sts)
{
	char __iomem *addr;
	int i, j;
	if (gamut_cfg->flags & MDP_PP_OPS_WRITE) {
		addr = base + MDSS_MDP_REG_DSPP_GAMUT_BASE;
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				writel_relaxed((u32)gamut_cfg->r_tbl[i][j],
						addr);
			addr += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				writel_relaxed((u32)gamut_cfg->g_tbl[i][j],
						addr);
			addr += 4;
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			for (j = 0; j < gamut_cfg->tbl_size[i]; j++)
				writel_relaxed((u32)gamut_cfg->b_tbl[i][j],
						addr);
			addr += 4;
		}
		if (gamut_cfg->gamut_first)
			pp_sts->gamut_sts |= PP_STS_GAMUT_FIRST;
	}

	if (gamut_cfg->flags & MDP_PP_OPS_DISABLE)
		pp_sts->gamut_sts &= ~PP_STS_ENABLE;
	else if (gamut_cfg->flags & MDP_PP_OPS_ENABLE)
		pp_sts->gamut_sts |= PP_STS_ENABLE;
}

static void pp_pa_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_pa_cfg *pa_config)
{
	if (flags & PP_FLAGS_DIRTY_PA) {
		if (pa_config->flags & MDP_PP_OPS_WRITE) {
			writel_relaxed(pa_config->hue_adj, addr);
			addr += 4;
			writel_relaxed(pa_config->sat_adj, addr);
			addr += 4;
			writel_relaxed(pa_config->val_adj, addr);
			addr += 4;
			writel_relaxed(pa_config->cont_adj, addr);
		}
		if (pa_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->pa_sts &= ~PP_STS_ENABLE;
		else if (pa_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->pa_sts |= PP_STS_ENABLE;
	}
}

static void pp_pcc_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_pcc_cfg_data *pcc_config)
{
	if (flags & PP_FLAGS_DIRTY_PCC) {
		if (pcc_config->ops & MDP_PP_OPS_WRITE)
			pp_update_pcc_regs(addr, pcc_config);

		if (pcc_config->ops & MDP_PP_OPS_DISABLE)
			pp_sts->pcc_sts &= ~PP_STS_ENABLE;
		else if (pcc_config->ops & MDP_PP_OPS_ENABLE)
			pp_sts->pcc_sts |= PP_STS_ENABLE;
	}
}

static void pp_igc_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_igc_lut_data *igc_config,
				u32 pipe_num)
{
	u32 tbl_idx;
	if (flags & PP_FLAGS_DIRTY_IGC) {
		if (igc_config->ops & MDP_PP_OPS_WRITE)
			pp_update_igc_lut(igc_config, addr, pipe_num);

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

static void pp_enhist_config(unsigned long flags, char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_hist_lut_data *enhist_cfg)
{
	if (flags & PP_FLAGS_DIRTY_ENHIST) {
		if (enhist_cfg->ops & MDP_PP_OPS_WRITE)
			pp_update_hist_lut(addr, enhist_cfg);

		if (enhist_cfg->ops & MDP_PP_OPS_DISABLE)
			pp_sts->enhist_sts &= ~PP_STS_ENABLE;
		else if (enhist_cfg->ops & MDP_PP_OPS_ENABLE)
			pp_sts->enhist_sts |= PP_STS_ENABLE;
	}
}

/*the below function doesn't do error checking on the input params*/
static void pp_sharp_config(char __iomem *addr,
				struct pp_sts_type *pp_sts,
				struct mdp_sharp_cfg *sharp_config)
{
	if (sharp_config->flags & MDP_PP_OPS_WRITE) {
		writel_relaxed(sharp_config->strength, addr);
		addr += 4;
		writel_relaxed(sharp_config->edge_thr, addr);
		addr += 4;
		writel_relaxed(sharp_config->smooth_thr, addr);
		addr += 4;
		writel_relaxed(sharp_config->noise_thr, addr);
	}
	if (sharp_config->flags & MDP_PP_OPS_DISABLE)
		pp_sts->sharp_sts &= ~PP_STS_ENABLE;
	else if (sharp_config->flags & MDP_PP_OPS_ENABLE)
		pp_sts->sharp_sts |= PP_STS_ENABLE;

}

static int pp_vig_pipe_setup(struct mdss_mdp_pipe *pipe, u32 *op)
{
	u32 opmode = 0;
	unsigned long flags = 0;
	char __iomem *offset;

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

	pp_histogram_setup(&opmode, MDSS_PP_SSPP_CFG | pipe->num, pipe->mixer);

	if (pipe->flags & MDP_OVERLAY_PP_CFG_EN) {
		if (pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_PA_CFG) {
			flags = PP_FLAGS_DIRTY_PA;
			pp_pa_config(flags,
				pipe->base + MDSS_MDP_REG_VIG_PA_BASE,
				&pipe->pp_res.pp_sts,
				&pipe->pp_cfg.pa_cfg);

			if (pipe->pp_res.pp_sts.pa_sts & PP_STS_ENABLE)
				opmode |= (1 << 4); /* PA_EN */
		}

		if (pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_HIST_LUT_CFG) {
			pp_enhist_config(PP_FLAGS_DIRTY_ENHIST,
				pipe->base + MDSS_MDP_REG_VIG_HIST_LUT_BASE,
				&pipe->pp_res.pp_sts,
				&pipe->pp_cfg.hist_lut_cfg);
		}
	}

	if (pipe->pp_res.pp_sts.enhist_sts & PP_STS_ENABLE) {
		/* Enable HistLUT and PA */
		opmode |= BIT(10) | BIT(4);
		if (!(pipe->pp_res.pp_sts.pa_sts & PP_STS_ENABLE)) {
			/* Program default value */
			offset = pipe->base + MDSS_MDP_REG_VIG_PA_BASE;
			writel_relaxed(0, offset);
			writel_relaxed(0, offset + 4);
			writel_relaxed(0, offset + 8);
			writel_relaxed(0, offset + 12);
		}
	}

	*op = opmode;

	return 0;
}

static int mdss_mdp_scale_setup(struct mdss_mdp_pipe *pipe)
{
	u32 scale_config = 0;
	u32 phasex_step = 0, phasey_step = 0;
	u32 chroma_sample;
	u32 filter_mode;
	struct mdss_data_type *mdata;
	u32 src_w, src_h;

	mdata = mdss_mdp_get_mdata();
	if (mdata->mdp_rev >= MDSS_MDP_HW_REV_102 && pipe->src_fmt->is_yuv)
		filter_mode = MDSS_MDP_SCALE_FILTER_CA;
	else
		filter_mode = MDSS_MDP_SCALE_FILTER_BIL;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA) {
		if (pipe->dst.h != pipe->src.h || pipe->dst.w != pipe->src.w) {
			pr_err("no scaling supported on dma pipe\n");
			return -EINVAL;
		} else {
			return 0;
		}
	}

	src_w = pipe->src.w >> pipe->horz_deci;
	src_h = pipe->src.h >> pipe->vert_deci;

	chroma_sample = pipe->src_fmt->chroma_sample;
	if (pipe->flags & MDP_SOURCE_ROTATED_90) {
		if (chroma_sample == MDSS_MDP_CHROMA_H1V2)
			chroma_sample = MDSS_MDP_CHROMA_H2V1;
		else if (chroma_sample == MDSS_MDP_CHROMA_H2V1)
			chroma_sample = MDSS_MDP_CHROMA_H1V2;
	}

	if (!(pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_SHARP_CFG)) {
		pipe->pp_cfg.sharp_cfg.flags = MDP_PP_OPS_ENABLE |
			MDP_PP_OPS_WRITE;
		pipe->pp_cfg.sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
		pipe->pp_cfg.sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
		pipe->pp_cfg.sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
		pipe->pp_cfg.sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;
	}

	if ((pipe->src_fmt->is_yuv) &&
		!((pipe->dst.w < src_w) || (pipe->dst.h < src_h))) {
		pp_sharp_config(pipe->base +
		   MDSS_MDP_REG_VIG_QSEED2_SHARP,
		   &pipe->pp_res.pp_sts,
		   &pipe->pp_cfg.sharp_cfg);
	}

	if ((src_h != pipe->dst.h) ||
	    (pipe->pp_res.pp_sts.sharp_sts & PP_STS_ENABLE) ||
	    (chroma_sample == MDSS_MDP_CHROMA_420) ||
	    (chroma_sample == MDSS_MDP_CHROMA_H1V2)) {
		pr_debug("scale y - src_h=%d dst_h=%d\n", src_h, pipe->dst.h);

		if ((src_h / MAX_DOWNSCALE_RATIO) > pipe->dst.h) {
			pr_err("too much downscaling height=%d->%d",
			       src_h, pipe->dst.h);
			return -EINVAL;
		}

		scale_config |= MDSS_MDP_SCALEY_EN;
		phasey_step = pipe->phase_step_y;

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG) {
			u32 chroma_shift = 0;
			if (!pipe->vert_deci &&
			    ((chroma_sample == MDSS_MDP_CHROMA_420) ||
			    (chroma_sample == MDSS_MDP_CHROMA_H1V2)))
				chroma_shift = 1; /* 2x upsample chroma */

			if (src_h <= pipe->dst.h) {
				scale_config |= /* G/Y, A */
					(filter_mode << 10) |
					(MDSS_MDP_SCALE_FILTER_BIL << 18);
			} else
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 10) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 18);

			if ((src_h >> chroma_shift) <= pipe->dst.h)
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_BIL << 14);
			else
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_PCMN << 14);

			writel_relaxed(phasey_step >> chroma_shift, pipe->base +
				MDSS_MDP_REG_VIG_QSEED2_C12_PHASESTEPY);
		} else {
			if (src_h <= pipe->dst.h)
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 10) |
					(MDSS_MDP_SCALE_FILTER_BIL << 18);
			else
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 10) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 18);
		}
	}

	if ((src_w != pipe->dst.w) ||
	    (pipe->pp_res.pp_sts.sharp_sts & PP_STS_ENABLE) ||
	    (chroma_sample == MDSS_MDP_CHROMA_420) ||
	    (chroma_sample == MDSS_MDP_CHROMA_H2V1)) {
		pr_debug("scale x - src_w=%d dst_w=%d\n", src_w, pipe->dst.w);

		if ((src_w / MAX_DOWNSCALE_RATIO) > pipe->dst.w) {
			pr_err("too much downscaling width=%d->%d",
			       src_w, pipe->dst.w);
			return -EINVAL;
		}

		scale_config |= MDSS_MDP_SCALEX_EN;
		phasex_step = pipe->phase_step_x;

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG) {
			u32 chroma_shift = 0;

			if (!pipe->horz_deci &&
			    ((chroma_sample == MDSS_MDP_CHROMA_420) ||
			    (chroma_sample == MDSS_MDP_CHROMA_H2V1)))
				chroma_shift = 1; /* 2x upsample chroma */

			if (src_w <= pipe->dst.w) {
				scale_config |= /* G/Y, A */
					(filter_mode << 8) |
					(MDSS_MDP_SCALE_FILTER_BIL << 16);
			} else
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 8) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 16);

			if ((src_w >> chroma_shift) <= pipe->dst.w)
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_BIL << 12);
			else
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_PCMN << 12);

			writel_relaxed(phasex_step >> chroma_shift, pipe->base +
				MDSS_MDP_REG_VIG_QSEED2_C12_PHASESTEPX);
		} else {
			if (src_w <= pipe->dst.w)
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 8) |
					(MDSS_MDP_SCALE_FILTER_BIL << 16);
			else
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 8) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 16);
		}
	}

	writel_relaxed(scale_config, pipe->base +
	   MDSS_MDP_REG_SCALE_CONFIG);
	writel_relaxed(phasex_step, pipe->base +
	   MDSS_MDP_REG_SCALE_PHASE_STEP_X);
	writel_relaxed(phasey_step, pipe->base +
	   MDSS_MDP_REG_SCALE_PHASE_STEP_Y);
	return 0;
}

int mdss_mdp_pipe_pp_setup(struct mdss_mdp_pipe *pipe, u32 *op)
{
	int ret = 0;
	if (!pipe)
		return -ENODEV;

	ret = mdss_mdp_scale_setup(pipe);
	if (ret)
		return -EINVAL;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)
		ret = pp_vig_pipe_setup(pipe, op);

	return ret;
}

void mdss_mdp_pipe_sspp_term(struct mdss_mdp_pipe *pipe)
{
	u32 done_bit;
	struct pp_hist_col_info *hist_info;
	char __iomem *ctl_base;

	if (!pipe && pipe->pp_res.hist.col_en) {
		done_bit = 3 << (pipe->num * 4);
		hist_info = &pipe->pp_res.hist;
		ctl_base = pipe->base +
			MDSS_MDP_REG_VIG_HIST_CTL_BASE;
		pp_histogram_disable(hist_info, done_bit, ctl_base);
	}
	memset(&pipe->pp_cfg, 0, sizeof(struct mdp_overlay_pp_params));
	memset(&pipe->pp_res, 0, sizeof(struct mdss_pipe_pp_res));
}

int mdss_mdp_pipe_sspp_setup(struct mdss_mdp_pipe *pipe, u32 *op)
{
	int ret = 0;
	unsigned long flags = 0;
	char __iomem *pipe_base;
	u32 pipe_num;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (pipe == NULL)
		return -EINVAL;

	/*
	 * TODO: should this function be responsible for masking multiple
	 * pipes to be written in dual pipe case?
	 * if so, requires rework of update_igc_lut
	 */
	switch (pipe->type) {
	case MDSS_MDP_PIPE_TYPE_VIG:
		pipe_base = mdata->mdp_base + MDSS_MDP_REG_IGC_VIG_BASE;
		pipe_num = pipe->num - MDSS_MDP_SSPP_VIG0;
		break;
	case MDSS_MDP_PIPE_TYPE_RGB:
		pipe_base = mdata->mdp_base + MDSS_MDP_REG_IGC_RGB_BASE;
		pipe_num = pipe->num - MDSS_MDP_SSPP_RGB0;
		break;
	case MDSS_MDP_PIPE_TYPE_DMA:
		pipe_base = mdata->mdp_base + MDSS_MDP_REG_IGC_DMA_BASE;
		pipe_num = pipe->num - MDSS_MDP_SSPP_DMA0;
		break;
	default:
		return -EINVAL;
	}

	if (pipe->pp_cfg.config_ops & MDP_OVERLAY_PP_IGC_CFG) {
		flags |= PP_FLAGS_DIRTY_IGC;
		pp_igc_config(flags, pipe_base, &pipe->pp_res.pp_sts,
					&pipe->pp_cfg.igc_cfg, pipe_num);
	}

	if (pipe->pp_res.pp_sts.igc_sts & PP_STS_ENABLE)
		*op |= (1 << 16); /* IGC_LUT_EN */

	return ret;
}

static int pp_mixer_setup(u32 disp_num,
		struct mdss_mdp_mixer *mixer)
{
	u32 flags, dspp_num, opmode = 0;
	struct mdp_pgc_lut_data *pgc_config;
	struct pp_sts_type *pp_sts;
	struct mdss_mdp_ctl *ctl;
	char __iomem *addr;
	dspp_num = mixer->num;

	if (!mixer || !mixer->ctl)
		return -EINVAL;
	ctl = mixer->ctl;

	/* no corresponding dspp */
	if ((mixer->type != MDSS_MDP_MIXER_TYPE_INTF) ||
		(dspp_num >= MDSS_MDP_MAX_DSPP))
		return 0;
	if (disp_num < MDSS_BLOCK_DISP_NUM)
		flags = mdss_pp_res->pp_disp_flags[disp_num];
	else
		flags = 0;

	pp_sts = &mdss_pp_res->pp_disp_sts[disp_num];
	/* GC_LUT is in layer mixer */
	if (flags & PP_FLAGS_DIRTY_ARGC) {
		pgc_config = &mdss_pp_res->argc_disp_cfg[disp_num];
		if (pgc_config->flags & MDP_PP_OPS_WRITE) {
			addr = mixer->base +
				MDSS_MDP_REG_LM_GC_LUT_BASE;
			pp_update_argc_lut(addr, pgc_config);
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
		addr = mixer->base + MDSS_MDP_REG_LM_OP_MODE;
		opmode = readl_relaxed(addr);
		opmode |= (1 << 0); /* GC_LUT_EN */
		writel_relaxed(opmode, addr);
	}
	return 0;
}

static char __iomem *mdss_mdp_get_mixer_addr_off(u32 dspp_num)
{
	struct mdss_data_type *mdata;
	struct mdss_mdp_mixer *mixer;

	mdata = mdss_mdp_get_mdata();
	if (mdata->nmixers_intf <= dspp_num) {
		pr_err("Invalid dspp_num=%d", dspp_num);
		return ERR_PTR(-EINVAL);
	}
	mixer = mdata->mixer_intf + dspp_num;
	return mixer->base;
}

static char __iomem *mdss_mdp_get_dspp_addr_off(u32 dspp_num)
{
	struct mdss_data_type *mdata;
	struct mdss_mdp_mixer *mixer;

	mdata = mdss_mdp_get_mdata();
	if (mdata->nmixers_intf <= dspp_num) {
		pr_err("Invalid dspp_num=%d", dspp_num);
		return ERR_PTR(-EINVAL);
	}
	mixer = mdata->mixer_intf + dspp_num;
	return mixer->dspp_base;
}

/* Assumes that function will be called from within clock enabled space*/
static int pp_histogram_setup(u32 *op, u32 block, struct mdss_mdp_mixer *mix)
{
	int ret = -EINVAL;
	char __iomem *base;
	u32 op_flags, kick_base, col_state;
	struct mdss_data_type *mdata;
	struct mdss_mdp_pipe *pipe;
	struct pp_hist_col_info *hist_info;
	unsigned long flag;

	if (mix && (PP_LOCAT(block) == MDSS_PP_DSPP_CFG)) {
		/* HIST_EN & AUTO_CLEAR */
		op_flags = BIT(16) | BIT(17);
		hist_info = &mdss_pp_res->dspp_hist[mix->num];
		base = mdss_mdp_get_dspp_addr_off(PP_BLOCK(block));
		kick_base = MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
	} else if (PP_LOCAT(block) == MDSS_PP_SSPP_CFG) {
		mdata = mdss_mdp_get_mdata();
		pipe = mdss_mdp_pipe_get(mdata, BIT(PP_BLOCK(block)));
		if (IS_ERR_OR_NULL(pipe)) {
			pr_debug("pipe DNE (%d)", (u32) BIT(PP_BLOCK(block)));
			ret = -ENODEV;
			goto error;
		}
		/* HIST_EN & AUTO_CLEAR */
		op_flags = BIT(8) + BIT(9);
		hist_info = &pipe->pp_res.hist;
		base = pipe->base;
		kick_base = MDSS_MDP_REG_VIG_HIST_CTL_BASE;
		mdss_mdp_pipe_unmap(pipe);
	} else {
		pr_warn("invalid histogram location (%d)", block);
		goto error;
	}

	if (hist_info->col_en) {
		*op |= op_flags;
		mutex_lock(&hist_info->hist_mutex);
		spin_lock_irqsave(&hist_info->hist_lock, flag);
		col_state = hist_info->col_state;
		if (hist_info->is_kick_ready &&
			((col_state == HIST_IDLE) ||
			((false == hist_info->read_request) &&
				col_state == HIST_READY))) {
			/* Kick off collection */
			writel_relaxed(1, base + kick_base);
			hist_info->col_state = HIST_START;
		}
		spin_unlock_irqrestore(&hist_info->hist_lock, flag);
		mutex_unlock(&hist_info->hist_mutex);
	}
	ret = 0;
error:
	return ret;
}

static int pp_dspp_setup(u32 disp_num, struct mdss_mdp_mixer *mixer)
{
	u32 flags, dspp_num, opmode = 0;
	struct mdp_dither_cfg_data *dither_cfg;
	struct mdp_pgc_lut_data *pgc_config;
	struct pp_sts_type *pp_sts;
	u32 data;
	char __iomem *base, *addr;
	int i, ret = 0;
	struct mdss_data_type *mdata;
	struct mdss_mdp_ctl *ctl;
	u32 mixer_cnt;
	u32 mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];

	if (!mixer || !mixer->ctl || !mixer->ctl->mdata)
		return -EINVAL;
	ctl = mixer->ctl;
	mdata = ctl->mdata;
	dspp_num = mixer->num;
	/* no corresponding dspp */
	if ((mixer->type != MDSS_MDP_MIXER_TYPE_INTF) ||
		(dspp_num >= MDSS_MDP_MAX_DSPP))
		return -EINVAL;
	base = mdss_mdp_get_dspp_addr_off(dspp_num);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	ret = pp_histogram_setup(&opmode, MDSS_PP_DSPP_CFG | dspp_num, mixer);
	if (ret)
		goto dspp_exit;

	if (disp_num < MDSS_BLOCK_DISP_NUM)
		flags = mdss_pp_res->pp_disp_flags[disp_num];
	else
		flags = 0;

	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);
	if (dspp_num < mdata->nad_cfgs && (mixer_cnt != 2)) {
		ret = mdss_mdp_ad_setup(ctl->mfd);
		if (ret < 0)
			pr_warn("ad_setup(dspp%d) returns %d", dspp_num, ret);
	}
	/* call calibration specific processing here */
	if (ctl->mfd->calib_mode)
		goto flush_exit;

	/* nothing to update */
	if ((!flags) && (!(opmode)) && (ret <= 0))
		goto dspp_exit;
	ret = 0;

	pp_sts = &mdss_pp_res->pp_disp_sts[disp_num];

	pp_pa_config(flags, base + MDSS_MDP_REG_DSPP_PA_BASE, pp_sts,
					&mdss_pp_res->pa_disp_cfg[disp_num]);
	pp_pcc_config(flags, base + MDSS_MDP_REG_DSPP_PCC_BASE, pp_sts,
					&mdss_pp_res->pcc_disp_cfg[disp_num]);

	pp_igc_config(flags, mdata->mdp_base + MDSS_MDP_REG_IGC_DSPP_BASE,
				pp_sts, &mdss_pp_res->igc_disp_cfg[disp_num],
				dspp_num);

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
			addr = base + MDSS_MDP_REG_DSPP_PA_BASE;
			writel_relaxed(0, addr);
			writel_relaxed(0, addr + 4);
			writel_relaxed(0, addr + 8);
			writel_relaxed(0, addr + 12);
		}
	}
	if (flags & PP_FLAGS_DIRTY_DITHER) {
		dither_cfg = &mdss_pp_res->dither_disp_cfg[disp_num];
		if (dither_cfg->flags & MDP_PP_OPS_WRITE) {
			addr = base + MDSS_MDP_REG_DSPP_DITHER_DEPTH;
			data = dither_depth_map[dither_cfg->g_y_depth];
			data |= dither_depth_map[dither_cfg->b_cb_depth] << 2;
			data |= dither_depth_map[dither_cfg->r_cr_depth] << 4;
			writel_relaxed(data, addr);
			addr += 0x14;
			for (i = 0; i << 16; i += 4) {
				data = dither_matrix[i] |
					(dither_matrix[i + 1] << 4) |
					(dither_matrix[i + 2] << 8) |
					(dither_matrix[i + 3] << 12);
				writel_relaxed(data, addr);
				addr += 4;
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
			addr = base + MDSS_MDP_REG_DSPP_GC_BASE;
			pp_update_argc_lut(addr, pgc_config);
		}
		if (pgc_config->flags & MDP_PP_OPS_DISABLE)
			pp_sts->pgc_sts &= ~PP_STS_ENABLE;
		else if (pgc_config->flags & MDP_PP_OPS_ENABLE)
			pp_sts->pgc_sts |= PP_STS_ENABLE;
	}
	if (pp_sts->pgc_sts & PP_STS_ENABLE)
		opmode |= (1 << 22);

flush_exit:
	writel_relaxed(opmode, base + MDSS_MDP_REG_DSPP_OP_MODE);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, BIT(13 + dspp_num));
	wmb();
dspp_exit:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	return ret;
}

int mdss_mdp_pp_setup(struct mdss_mdp_ctl *ctl)
{
	int ret = 0;

	if ((!ctl->mfd) || (!mdss_pp_res))
		return -EINVAL;

	/* TODO: have some sort of reader/writer lock to prevent unclocked
	 * access while display power is toggled */
	if (!ctl->mfd->panel_power_on) {
		ret = -EPERM;
		goto error;
	}
	mutex_lock(&ctl->mfd->lock);
	ret = mdss_mdp_pp_setup_locked(ctl);
	mutex_unlock(&ctl->mfd->lock);
error:
	return ret;
}

/* call only when holding and mfd->lock */
int mdss_mdp_pp_setup_locked(struct mdss_mdp_ctl *ctl)
{
	u32 disp_num;
	if ((!ctl->mfd) || (!mdss_pp_res))
		return -EINVAL;

	/* treat fb_num the same as block logical id*/
	disp_num = ctl->mfd->index;

	mutex_lock(&mdss_pp_mutex);
	if (ctl->mixer_left) {
		pp_mixer_setup(disp_num, ctl->mixer_left);
		pp_dspp_setup(disp_num, ctl->mixer_left);
	}
	if (ctl->mixer_right) {
		pp_mixer_setup(disp_num, ctl->mixer_right);
		pp_dspp_setup(disp_num, ctl->mixer_right);
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
int mdss_mdp_pp_resume(struct mdss_mdp_ctl *ctl, u32 dspp_num)
{
	u32 flags = 0, disp_num, bl;
	struct pp_sts_type pp_sts;
	struct mdss_ad_info *ad;
	struct mdss_data_type *mdata = ctl->mdata;
	if (dspp_num >= MDSS_MDP_MAX_DSPP) {
		pr_warn("invalid dspp_num");
		return -EINVAL;
	}
	disp_num = ctl->mfd->index;

	if (dspp_num < mdata->nad_cfgs) {
		ad = &mdata->ad_cfgs[dspp_num];

		if (PP_AD_STATE_CFG & ad->state)
			pp_ad_cfg_write(ad);
		if (PP_AD_STATE_INIT & ad->state)
			pp_ad_init_write(ad);
		if (PP_AD_STATE_DATA & ad->state) {
			bl = ad->bl_mfd->bl_level;
			ad->last_bl = bl;
			if (ad->state & PP_AD_STATE_BL_LIN) {
				bl = ad->bl_lin[bl >> ad->bl_bright_shift];
				bl = bl << ad->bl_bright_shift;
			}
			pp_ad_input_write(ad, bl);
		}
		if ((PP_AD_STATE_VSYNC & ad->state) && ad->calc_itr)
			ctl->add_vsync_handler(ctl, &ad->handle);
	}

	pp_sts = mdss_pp_res->pp_disp_sts[disp_num];

	if (pp_sts.pa_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PA;
		if (!(mdss_pp_res->pa_disp_cfg[disp_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pa_disp_cfg[disp_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.pcc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PCC;
		if (!(mdss_pp_res->pcc_disp_cfg[disp_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pcc_disp_cfg[disp_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.igc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_IGC;
		if (!(mdss_pp_res->igc_disp_cfg[disp_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->igc_disp_cfg[disp_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.argc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_ARGC;
		if (!(mdss_pp_res->argc_disp_cfg[disp_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->argc_disp_cfg[disp_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.enhist_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_ENHIST;
		if (!(mdss_pp_res->enhist_disp_cfg[disp_num].ops
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->enhist_disp_cfg[disp_num].ops |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.dither_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_DITHER;
		if (!(mdss_pp_res->dither_disp_cfg[disp_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->dither_disp_cfg[disp_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.gamut_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_GAMUT;
		if (!(mdss_pp_res->gamut_disp_cfg[disp_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->gamut_disp_cfg[disp_num].flags |=
				MDP_PP_OPS_WRITE;
	}
	if (pp_sts.pgc_sts & PP_STS_ENABLE) {
		flags |= PP_FLAGS_DIRTY_PGC;
		if (!(mdss_pp_res->pgc_disp_cfg[disp_num].flags
					& MDP_PP_OPS_DISABLE))
			mdss_pp_res->pgc_disp_cfg[disp_num].flags |=
				MDP_PP_OPS_WRITE;
	}

	mdss_pp_res->pp_disp_flags[disp_num] |= flags;
	return 0;
}

int mdss_mdp_pp_init(struct device *dev)
{
	int i, ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_pipe *vig;
	struct msm_bus_scale_pdata *pp_bus_pdata;

	mutex_lock(&mdss_pp_mutex);
	if (!mdss_pp_res) {
		mdss_pp_res = devm_kzalloc(dev, sizeof(*mdss_pp_res),
				GFP_KERNEL);
		if (mdss_pp_res == NULL) {
			pr_err("%s mdss_pp_res allocation failed!", __func__);
			ret = -ENOMEM;
		}

		for (i = 0; i < MDSS_MDP_MAX_DSPP; i++) {
			mutex_init(&mdss_pp_res->dspp_hist[i].hist_mutex);
			spin_lock_init(&mdss_pp_res->dspp_hist[i].hist_lock);
		}
	}
	if (mdata) {
		vig = mdata->vig_pipes;
		for (i = 0; i < mdata->nvig_pipes; i++) {
			mutex_init(&vig[i].pp_res.hist.hist_mutex);
			spin_lock_init(&vig[i].pp_res.hist.hist_lock);
		}
		if (!mdata->pp_bus_hdl) {
			pp_bus_pdata = &mdp_pp_bus_scale_table;
			for (i = 0; i < pp_bus_pdata->num_usecases; i++) {
				mdp_pp_bus_usecases[i].num_paths = 1;
				mdp_pp_bus_usecases[i].vectors =
					&mdp_pp_bus_vectors[i];
			}

			mdata->pp_bus_hdl =
				msm_bus_scale_register_client(pp_bus_pdata);
			if (!mdata->pp_bus_hdl) {
				pr_err("not able to register pp_bus_scale\n");
				ret = -ENOMEM;
			}
			pr_debug("register pp_bus_hdl=%x\n", mdata->pp_bus_hdl);
		}

	}
	mutex_unlock(&mdss_pp_mutex);
	return ret;
}
void mdss_mdp_pp_term(struct device *dev)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata->pp_bus_hdl) {
		msm_bus_scale_unregister_client(mdata->pp_bus_hdl);
		mdata->pp_bus_hdl = 0;
	}
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
	u32 mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];
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

int mdss_mdp_pa_config(struct mdss_mdp_ctl *ctl, struct mdp_pa_cfg_data *config,
			u32 *copyback)
{
	int ret = 0;
	u32 disp_num, dspp_num = 0;
	char __iomem *pa_addr;

	if (!ctl)
		return -EINVAL;

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
		pa_addr = mdss_mdp_get_dspp_addr_off(dspp_num) +
			  MDSS_MDP_REG_DSPP_PA_BASE;
		config->pa_data.hue_adj = readl_relaxed(pa_addr);
		pa_addr += 4;
		config->pa_data.sat_adj = readl_relaxed(pa_addr);
		pa_addr += 4;
		config->pa_data.val_adj = readl_relaxed(pa_addr);
		pa_addr += 4;
		config->pa_data.cont_adj = readl_relaxed(pa_addr);
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_pp_res->pa_disp_cfg[disp_num] = config->pa_data;
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_PA;
	}

pa_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}

static void pp_read_pcc_regs(char __iomem *addr,
				struct mdp_pcc_cfg_data *cfg_ptr)
{
	cfg_ptr->r.c = readl_relaxed(addr);
	cfg_ptr->g.c = readl_relaxed(addr + 4);
	cfg_ptr->b.c = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.r = readl_relaxed(addr);
	cfg_ptr->g.r = readl_relaxed(addr + 4);
	cfg_ptr->b.r = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.g = readl_relaxed(addr);
	cfg_ptr->g.g = readl_relaxed(addr + 4);
	cfg_ptr->b.g = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.b = readl_relaxed(addr);
	cfg_ptr->g.b = readl_relaxed(addr + 4);
	cfg_ptr->b.b = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.rr = readl_relaxed(addr);
	cfg_ptr->g.rr = readl_relaxed(addr + 4);
	cfg_ptr->b.rr = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.rg = readl_relaxed(addr);
	cfg_ptr->g.rg = readl_relaxed(addr + 4);
	cfg_ptr->b.rg = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.rb = readl_relaxed(addr);
	cfg_ptr->g.rb = readl_relaxed(addr + 4);
	cfg_ptr->b.rb = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.gg = readl_relaxed(addr);
	cfg_ptr->g.gg = readl_relaxed(addr + 4);
	cfg_ptr->b.gg = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.gb = readl_relaxed(addr);
	cfg_ptr->g.gb = readl_relaxed(addr + 4);
	cfg_ptr->b.gb = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.bb = readl_relaxed(addr);
	cfg_ptr->g.bb = readl_relaxed(addr + 4);
	cfg_ptr->b.bb = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.rgb_0 = readl_relaxed(addr);
	cfg_ptr->g.rgb_0 = readl_relaxed(addr + 4);
	cfg_ptr->b.rgb_0 = readl_relaxed(addr + 8);
	addr += 0x10;

	cfg_ptr->r.rgb_1 = readl_relaxed(addr);
	cfg_ptr->g.rgb_1 = readl_relaxed(addr + 4);
	cfg_ptr->b.rgb_1 = readl_relaxed(addr + 8);
}

static void pp_update_pcc_regs(char __iomem *addr,
				struct mdp_pcc_cfg_data *cfg_ptr)
{
	writel_relaxed(cfg_ptr->r.c, addr);
	writel_relaxed(cfg_ptr->g.c, addr + 4);
	writel_relaxed(cfg_ptr->b.c, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.r, addr);
	writel_relaxed(cfg_ptr->g.r, addr + 4);
	writel_relaxed(cfg_ptr->b.r, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.g, addr);
	writel_relaxed(cfg_ptr->g.g, addr + 4);
	writel_relaxed(cfg_ptr->b.g, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.b, addr);
	writel_relaxed(cfg_ptr->g.b, addr + 4);
	writel_relaxed(cfg_ptr->b.b, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.rr, addr);
	writel_relaxed(cfg_ptr->g.rr, addr + 4);
	writel_relaxed(cfg_ptr->b.rr, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.rg, addr);
	writel_relaxed(cfg_ptr->g.rg, addr + 4);
	writel_relaxed(cfg_ptr->b.rg, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.rb, addr);
	writel_relaxed(cfg_ptr->g.rb, addr + 4);
	writel_relaxed(cfg_ptr->b.rb, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.gg, addr);
	writel_relaxed(cfg_ptr->g.gg, addr + 4);
	writel_relaxed(cfg_ptr->b.gg, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.gb, addr);
	writel_relaxed(cfg_ptr->g.gb, addr + 4);
	writel_relaxed(cfg_ptr->b.gb, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.bb, addr);
	writel_relaxed(cfg_ptr->g.bb, addr + 4);
	writel_relaxed(cfg_ptr->b.bb, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.rgb_0, addr);
	writel_relaxed(cfg_ptr->g.rgb_0, addr + 4);
	writel_relaxed(cfg_ptr->b.rgb_0, addr + 8);
	addr += 0x10;

	writel_relaxed(cfg_ptr->r.rgb_1, addr);
	writel_relaxed(cfg_ptr->g.rgb_1, addr + 4);
	writel_relaxed(cfg_ptr->b.rgb_1, addr + 8);
}

int mdss_mdp_pcc_config(struct mdss_mdp_ctl *ctl,
					struct mdp_pcc_cfg_data *config,
					u32 *copyback)
{
	int ret = 0;
	u32 disp_num, dspp_num = 0;
	char __iomem *addr;

	if (!ctl)
		return -EINVAL;

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

		addr = mdss_mdp_get_dspp_addr_off(dspp_num) +
			  MDSS_MDP_REG_DSPP_PCC_BASE;
		pp_read_pcc_regs(addr, config);
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_pp_res->pcc_disp_cfg[disp_num] = *config;
		mdss_pp_res->pp_disp_flags[disp_num] |= PP_FLAGS_DIRTY_PCC;
	}

pcc_config_exit:
	mutex_unlock(&mdss_pp_mutex);
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}

static void pp_read_igc_lut(struct mdp_igc_lut_data *cfg,
				char __iomem *addr, u32 blk_idx)
{
	int i;
	u32 data;

	/* INDEX_UPDATE & VALUE_UPDATEN */
	data = (3 << 24) | (((~(1 << blk_idx)) & 0x7) << 28);
	writel_relaxed(data, addr);

	for (i = 0; i < cfg->len; i++)
		cfg->c0_c1_data[i] = readl_relaxed(addr) & 0xFFF;

	addr += 0x4;
	writel_relaxed(data, addr);
	for (i = 0; i < cfg->len; i++)
		cfg->c0_c1_data[i] |= (readl_relaxed(addr) & 0xFFF) << 16;

	addr += 0x4;
	writel_relaxed(data, addr);
	for (i = 0; i < cfg->len; i++)
		cfg->c2_data[i] = readl_relaxed(addr) & 0xFFF;
}

static void pp_update_igc_lut(struct mdp_igc_lut_data *cfg,
				char __iomem *addr, u32 blk_idx)
{
	int i;
	u32 data;
	/* INDEX_UPDATE */
	data = (1 << 25) | (((~(1 << blk_idx)) & 0x7) << 28);
	writel_relaxed((cfg->c0_c1_data[0] & 0xFFF) | data, addr);

	/* disable index update */
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		writel_relaxed((cfg->c0_c1_data[i] & 0xFFF) | data, addr);

	addr += 0x4;
	data |= (1 << 25);
	writel_relaxed(((cfg->c0_c1_data[0] >> 16) & 0xFFF) | data, addr);
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		writel_relaxed(((cfg->c0_c1_data[i] >> 16) & 0xFFF) | data,
				addr);

	addr += 0x4;
	data |= (1 << 25);
	writel_relaxed((cfg->c2_data[0] & 0xFFF) | data, addr);
	data &= ~(1 << 25);
	for (i = 1; i < cfg->len; i++)
		writel_relaxed((cfg->c2_data[i] & 0xFFF) | data, addr);
}

int mdss_mdp_limited_lut_igc_config(struct mdss_mdp_ctl *ctl)
{
	int ret = 0;
	u32 copyback = 0;
	u32 copy_from_kernel = 1;
	struct mdp_igc_lut_data config;

	if (!ctl)
		return -EINVAL;

	config.len = IGC_LUT_ENTRIES;
	config.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE;
	config.block = (ctl->mfd->index) + MDP_LOGICAL_BLOCK_DISP_0;
	config.c0_c1_data = igc_limited;
	config.c2_data = igc_limited;

	ret = mdss_mdp_igc_lut_config(ctl, &config, &copyback,
					copy_from_kernel);
	return ret;
}

int mdss_mdp_igc_lut_config(struct mdss_mdp_ctl *ctl,
					struct mdp_igc_lut_data *config,
					u32 *copyback, u32 copy_from_kernel)
{
	int ret = 0;
	u32 tbl_idx, disp_num, dspp_num = 0;
	struct mdp_igc_lut_data local_cfg;
	char __iomem *igc_addr;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl)
		return -EINVAL;

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX))
		return -EINVAL;

	if (config->len != IGC_LUT_ENTRIES)
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
		igc_addr = mdata->mdp_base + MDSS_MDP_REG_IGC_DSPP_BASE +
			(0x10 * tbl_idx);
		local_cfg = *config;
		local_cfg.c0_c1_data =
			&mdss_pp_res->igc_lut_c0c1[disp_num][0];
		local_cfg.c2_data =
			&mdss_pp_res->igc_lut_c2[disp_num][0];
		pp_read_igc_lut(&local_cfg, igc_addr, dspp_num);
		if (copy_to_user(config->c0_c1_data, local_cfg.c2_data,
			config->len * sizeof(u32))) {
			ret = -EFAULT;
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			goto igc_config_exit;
		}
		if (copy_to_user(config->c2_data, local_cfg.c0_c1_data,
			config->len * sizeof(u32))) {
			ret = -EFAULT;
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			goto igc_config_exit;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		if (copy_from_kernel) {
			memcpy(&mdss_pp_res->igc_lut_c0c1[disp_num][0],
			config->c0_c1_data, config->len * sizeof(u32));
			memcpy(&mdss_pp_res->igc_lut_c2[disp_num][0],
			config->c2_data, config->len * sizeof(u32));
		} else {
			if (copy_from_user(
				&mdss_pp_res->igc_lut_c0c1[disp_num][0],
				config->c0_c1_data,
				config->len * sizeof(u32))) {
				ret = -EFAULT;
				goto igc_config_exit;
			}
			if (copy_from_user(
				&mdss_pp_res->igc_lut_c2[disp_num][0],
				config->c2_data, config->len * sizeof(u32))) {
				ret = -EFAULT;
				goto igc_config_exit;
			}
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
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}
static void pp_update_gc_one_lut(char __iomem *addr,
		struct mdp_ar_gc_lut_data *lut_data,
		uint8_t num_stages)
{
	int i, start_idx, idx;

	start_idx = (readl_relaxed(addr) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].x_start, addr);
	}
	for (i = 0; i < start_idx; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].x_start, addr);
	}
	addr += 4;
	start_idx = (readl_relaxed(addr) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].slope, addr);
	}
	for (i = 0; i < start_idx; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].slope, addr);
	}
	addr += 4;
	start_idx = (readl_relaxed(addr) >> 16) & 0xF;
	for (i = start_idx; i < GC_LUT_SEGMENTS; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].offset, addr);
	}
	for (i = 0; i < start_idx; i++) {
		idx = min((uint8_t)i, (uint8_t)(num_stages-1));
		writel_relaxed(lut_data[idx].offset, addr);
	}
}
static void pp_update_argc_lut(char __iomem *addr,
				struct mdp_pgc_lut_data *config)
{
	pp_update_gc_one_lut(addr, config->r_data, config->num_r_stages);
	addr += 0x10;
	pp_update_gc_one_lut(addr, config->g_data, config->num_g_stages);
	addr += 0x10;
	pp_update_gc_one_lut(addr, config->b_data, config->num_b_stages);
}
static void pp_read_gc_one_lut(char __iomem *addr,
		struct mdp_ar_gc_lut_data *gc_data)
{
	int i, start_idx, data;
	data = readl_relaxed(addr);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].x_start = data & 0xFFF;

	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = readl_relaxed(addr);
		gc_data[i].x_start = data & 0xFFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = readl_relaxed(addr);
		gc_data[i].x_start = data & 0xFFF;
	}

	addr += 4;
	data = readl_relaxed(addr);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].slope = data & 0x7FFF;
	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = readl_relaxed(addr);
		gc_data[i].slope = data & 0x7FFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = readl_relaxed(addr);
		gc_data[i].slope = data & 0x7FFF;
	}
	addr += 4;
	data = readl_relaxed(addr);
	start_idx = (data >> 16) & 0xF;
	gc_data[start_idx].offset = data & 0x7FFF;
	for (i = start_idx + 1; i < GC_LUT_SEGMENTS; i++) {
		data = readl_relaxed(addr);
		gc_data[i].offset = data & 0x7FFF;
	}
	for (i = 0; i < start_idx; i++) {
		data = readl_relaxed(addr);
		gc_data[i].offset = data & 0x7FFF;
	}
}

static int pp_read_argc_lut(struct mdp_pgc_lut_data *config, char __iomem *addr)
{
	int ret = 0;
	pp_read_gc_one_lut(addr, config->r_data);
	addr += 0x10;
	pp_read_gc_one_lut(addr, config->g_data);
	addr += 0x10;
	pp_read_gc_one_lut(addr, config->b_data);
	return ret;
}

/* Note: Assumes that its inputs have been checked by calling function */
static void pp_update_hist_lut(char __iomem *addr,
				struct mdp_hist_lut_data *cfg)
{
	int i;
	for (i = 0; i < ENHIST_LUT_ENTRIES; i++)
		writel_relaxed(cfg->data[i], addr);
	/* swap */
	if (PP_LOCAT(cfg->block) == MDSS_PP_DSPP_CFG)
		writel_relaxed(1, addr + 4);
	else
		writel_relaxed(1, addr + 16);
}

int mdss_mdp_argc_config(struct mdss_mdp_ctl *ctl,
				struct mdp_pgc_lut_data *config,
				u32 *copyback)
{
	int ret = 0;
	u32 disp_num, dspp_num = 0;
	struct mdp_pgc_lut_data local_cfg;
	struct mdp_pgc_lut_data *pgc_ptr;
	u32 tbl_size, r_size, g_size, b_size;
	char __iomem *argc_addr = 0;

	if (!ctl)
		return -EINVAL;

	if ((PP_BLOCK(config->block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(config->block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);

	disp_num = PP_BLOCK(config->block) - MDP_LOGICAL_BLOCK_DISP_0;
	switch (PP_LOCAT(config->block)) {
	case MDSS_PP_LM_CFG:
		argc_addr = mdss_mdp_get_mixer_addr_off(dspp_num) +
			MDSS_MDP_REG_LM_GC_LUT_BASE;
		pgc_ptr = &mdss_pp_res->argc_disp_cfg[disp_num];
		mdss_pp_res->pp_disp_flags[disp_num] |=
			PP_FLAGS_DIRTY_ARGC;
		break;
	case MDSS_PP_DSPP_CFG:
		argc_addr = mdss_mdp_get_dspp_addr_off(dspp_num) +
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
		pp_read_argc_lut(&local_cfg, argc_addr);
		if (copy_to_user(config->r_data,
			&mdss_pp_res->gc_lut_r[disp_num][0], tbl_size)) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_to_user(config->g_data,
			&mdss_pp_res->gc_lut_g[disp_num][0], tbl_size)) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_to_user(config->b_data,
			&mdss_pp_res->gc_lut_b[disp_num][0], tbl_size)) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			ret = -EFAULT;
			goto argc_config_exit;
		}
		*copyback = 1;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		r_size = config->num_r_stages *
			sizeof(struct mdp_ar_gc_lut_data);
		g_size = config->num_g_stages *
			sizeof(struct mdp_ar_gc_lut_data);
		b_size = config->num_b_stages *
			sizeof(struct mdp_ar_gc_lut_data);
		if (r_size > tbl_size ||
			g_size > tbl_size ||
			b_size > tbl_size ||
			r_size == 0 ||
			g_size == 0 ||
			b_size == 0) {
			ret = -EINVAL;
			pr_warn("%s, number of rgb stages invalid",
				__func__);
			goto argc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->gc_lut_r[disp_num][0],
			config->r_data, r_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->gc_lut_g[disp_num][0],
			config->g_data, g_size)) {
			ret = -EFAULT;
			goto argc_config_exit;
		}
		if (copy_from_user(&mdss_pp_res->gc_lut_b[disp_num][0],
			config->b_data, b_size)) {
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
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}
int mdss_mdp_hist_lut_config(struct mdss_mdp_ctl *ctl,
					struct mdp_hist_lut_data *config,
					u32 *copyback)
{
	int i, ret = 0;
	u32 disp_num, dspp_num = 0;
	char __iomem *hist_addr;

	if (!ctl)
		return -EINVAL;

	if ((PP_BLOCK(config->block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(config->block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	mutex_lock(&mdss_pp_mutex);
	disp_num = PP_BLOCK(config->block) - MDP_LOGICAL_BLOCK_DISP_0;

	if (config->ops & MDP_PP_OPS_READ) {
		ret = pp_get_dspp_num(disp_num, &dspp_num);
		if (ret) {
			pr_err("%s, no dspp connects to disp %d",
				__func__, disp_num);
			goto enhist_config_exit;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		hist_addr = mdss_mdp_get_dspp_addr_off(dspp_num) +
			  MDSS_MDP_REG_DSPP_HIST_LUT_BASE;
		for (i = 0; i < ENHIST_LUT_ENTRIES; i++)
			mdss_pp_res->enhist_lut[disp_num][i] =
				readl_relaxed(hist_addr);
		if (copy_to_user(config->data,
			&mdss_pp_res->enhist_lut[disp_num][0],
			ENHIST_LUT_ENTRIES * sizeof(u32))) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
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
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}

int mdss_mdp_dither_config(struct mdss_mdp_ctl *ctl,
					struct mdp_dither_cfg_data *config,
					u32 *copyback)
{
	u32 disp_num;
	if (!ctl)
		return -EINVAL;

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
	mdss_mdp_pp_setup(ctl);
	return 0;
}

int mdss_mdp_gamut_config(struct mdss_mdp_ctl *ctl,
					struct mdp_gamut_cfg_data *config,
					u32 *copyback)
{
	int i, j, size_total = 0, ret = 0;

	u32 disp_num, dspp_num = 0;
	uint16_t *tbl_off;
	struct mdp_gamut_cfg_data local_cfg;
	uint16_t *r_tbl[MDP_GAMUT_TABLE_NUM];
	uint16_t *g_tbl[MDP_GAMUT_TABLE_NUM];
	uint16_t *b_tbl[MDP_GAMUT_TABLE_NUM];
	char __iomem *addr;

	if (!ctl)
		return -EINVAL;

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

		addr = mdss_mdp_get_dspp_addr_off(dspp_num) +
			  MDSS_MDP_REG_DSPP_GAMUT_BASE;
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			r_tbl[i] = kzalloc(
				sizeof(uint16_t) * config->tbl_size[i],
				GFP_KERNEL);
			if (!r_tbl[i]) {
				pr_err("%s: alloc failed\n", __func__);
				goto gamut_config_exit;
			}
			for (j = 0; j < config->tbl_size[i]; j++)
				r_tbl[i][j] =
					(u16)readl_relaxed(addr);
			addr += 4;
			ret = copy_to_user(config->r_tbl[i], r_tbl[i],
				     sizeof(uint16_t) * config->tbl_size[i]);
			kfree(r_tbl[i]);
			if (ret) {
				pr_err("%s: copy tbl to usr failed\n",
					__func__);
				goto gamut_config_exit;
			}
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			g_tbl[i] = kzalloc(
				sizeof(uint16_t) * config->tbl_size[i],
				GFP_KERNEL);
			if (!g_tbl[i]) {
				pr_err("%s: alloc failed\n", __func__);
				goto gamut_config_exit;
			}
			for (j = 0; j < config->tbl_size[i]; j++)
				g_tbl[i][j] =
					(u16)readl_relaxed(addr);
			addr += 4;
			ret = copy_to_user(config->g_tbl[i], g_tbl[i],
				     sizeof(uint16_t) * config->tbl_size[i]);
			kfree(g_tbl[i]);
			if (ret) {
				pr_err("%s: copy tbl to usr failed\n",
					__func__);
				goto gamut_config_exit;
			}
		}
		for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
			b_tbl[i] = kzalloc(
				sizeof(uint16_t) * config->tbl_size[i],
				GFP_KERNEL);
			if (!b_tbl[i]) {
				pr_err("%s: alloc failed\n", __func__);
				goto gamut_config_exit;
			}
			for (j = 0; j < config->tbl_size[i]; j++)
				b_tbl[i][j] =
					(u16)readl_relaxed(addr);
			addr += 4;
			ret = copy_to_user(config->b_tbl[i], b_tbl[i],
				     sizeof(uint16_t) * config->tbl_size[i]);
			kfree(b_tbl[i]);
			if (ret) {
				pr_err("%s: copy tbl to usr failed\n",
					__func__);
				goto gamut_config_exit;
			}
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
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}
static void pp_hist_read(char __iomem *v_addr,
				struct pp_hist_col_info *hist_info)
{
	int i, i_start;
	u32 data;
	data = readl_relaxed(v_addr);
	i_start = data >> 24;
	hist_info->data[i_start] = data & 0xFFFFFF;
	for (i = i_start + 1; i < HIST_V_SIZE; i++)
		hist_info->data[i] = readl_relaxed(v_addr) & 0xFFFFFF;
	for (i = 0; i < i_start - 1; i++)
		hist_info->data[i] = readl_relaxed(v_addr) & 0xFFFFFF;
	hist_info->hist_cnt_read++;
}

/* Assumes that relevant clocks are enabled */
static int pp_histogram_enable(struct pp_hist_col_info *hist_info,
				struct mdp_histogram_start_req *req,
				u32 shift_bit, char __iomem *ctl_base)
{
	unsigned long flag;
	int ret = 0;
	mutex_lock(&hist_info->hist_mutex);
	/* check if it is idle */
	if (hist_info->col_en) {
		pr_info("%s Hist collection has already been enabled %d",
			__func__, (u32) ctl_base);
		ret = -EINVAL;
		goto exit;
	}
	hist_info->frame_cnt = req->frame_cnt;
	init_completion(&hist_info->comp);
	hist_info->hist_cnt_read = 0;
	hist_info->hist_cnt_sent = 0;
	hist_info->hist_cnt_time = 0;
	spin_lock_irqsave(&hist_info->hist_lock, flag);
	hist_info->read_request = false;
	hist_info->col_state = HIST_RESET;
	hist_info->col_en = true;
	spin_unlock_irqrestore(&hist_info->hist_lock, flag);
	hist_info->is_kick_ready = false;
	mdss_mdp_hist_irq_enable(3 << shift_bit);
	writel_relaxed(req->frame_cnt, ctl_base + 8);
	/* Kick out reset start */
	writel_relaxed(1, ctl_base + 4);
exit:
	mutex_unlock(&hist_info->hist_mutex);
	return ret;
}

int mdss_mdp_histogram_start(struct mdss_mdp_ctl *ctl,
				struct mdp_histogram_start_req *req)
{
	u32 done_shift_bit;
	char __iomem *ctl_base;
	struct pp_hist_col_info *hist_info;
	int i, ret = 0;
	u32 disp_num, dspp_num = 0;
	u32 mixer_cnt, mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (!ctl)
		return -EINVAL;

	if ((PP_BLOCK(req->block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(req->block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	disp_num = PP_BLOCK(req->block) - MDP_LOGICAL_BLOCK_DISP_0;
	mixer_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!mixer_cnt) {
		pr_err("%s, no dspp connects to disp %d",
			__func__, disp_num);
		ret = -EPERM;
		goto hist_exit;
	}
	if (mixer_cnt >= MDSS_MDP_MAX_DSPP) {
		pr_err("%s, Too many dspp connects to disp %d",
			__func__, mixer_cnt);
		ret = -EPERM;
		goto hist_exit;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (PP_LOCAT(req->block) == MDSS_PP_SSPP_CFG) {
		i = MDSS_PP_ARG_MASK & req->block;
		if (!i) {
			ret = -EINVAL;
			pr_warn("Must pass pipe arguments, %d", i);
			goto hist_exit;
		}

		for (i = 0; i < MDSS_PP_ARG_NUM; i++) {
			if (!PP_ARG(i, req->block))
				continue;
			pipe = mdss_mdp_pipe_get(mdata, BIT(i));
			if (IS_ERR_OR_NULL(pipe))
				continue;
			if (!pipe || pipe->num > MDSS_MDP_SSPP_VIG2) {
				mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
				ret = -EINVAL;
				pr_warn("Invalid Hist pipe (%d)", i);
				goto hist_exit;
			}
			done_shift_bit = (pipe->num * 4);
			hist_info = &pipe->pp_res.hist;
			ctl_base = pipe->base +
				MDSS_MDP_REG_VIG_HIST_CTL_BASE;
			ret = pp_histogram_enable(hist_info, req,
						done_shift_bit,	ctl_base);
			mdss_mdp_pipe_unmap(pipe);
		}
	} else if (PP_LOCAT(req->block) == MDSS_PP_DSPP_CFG) {
		for (i = 0; i < mixer_cnt; i++) {
			dspp_num = mixer_id[i];
			done_shift_bit = (dspp_num * 4) + 12;
			hist_info = &mdss_pp_res->dspp_hist[dspp_num];
			ctl_base = mdss_mdp_get_dspp_addr_off(dspp_num) +
				MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
			ret = pp_histogram_enable(hist_info, req,
						done_shift_bit,	ctl_base);
			mdss_pp_res->pp_disp_flags[disp_num] |=
							PP_FLAGS_DIRTY_HIST_COL;
		}
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

hist_exit:
	if (!ret && (PP_LOCAT(req->block) == MDSS_PP_DSPP_CFG)) {
		mdss_mdp_pp_setup(ctl);
		/* wait for a frame to let histrogram enable itself */
		/* TODO add hysteresis value to be able to remove this sleep */
		usleep(41666);
		for (i = 0; i < mixer_cnt; i++) {
			dspp_num = mixer_id[i];
			hist_info = &mdss_pp_res->dspp_hist[dspp_num];
			mutex_lock(&hist_info->hist_mutex);
			hist_info->is_kick_ready = true;
			mutex_unlock(&hist_info->hist_mutex);
		}
	} else if (!ret) {
		for (i = 0; i < MDSS_PP_ARG_NUM; i++) {
			if (!PP_ARG(i, req->block))
				continue;
			pr_info("PP_ARG(%d) = %d", i, PP_ARG(i, req->block));
			pipe = mdss_mdp_pipe_get(mdata, BIT(i));
			if (IS_ERR_OR_NULL(pipe))
				continue;
			hist_info = &pipe->pp_res.hist;
			hist_info->is_kick_ready = true;
			mdss_mdp_pipe_unmap(pipe);
		}
	}
	return ret;
}

static int pp_histogram_disable(struct pp_hist_col_info *hist_info,
					u32 done_bit, char __iomem *ctl_base)
{
	int ret = 0;
	unsigned long flag;
	mutex_lock(&hist_info->hist_mutex);
	if (hist_info->col_en == false) {
		pr_debug("Histogram already disabled (%d)", (u32) ctl_base);
		ret = -EINVAL;
		goto exit;
	}
	complete_all(&hist_info->comp);
	spin_lock_irqsave(&hist_info->hist_lock, flag);
	hist_info->col_en = false;
	hist_info->col_state = HIST_UNKNOWN;
	spin_unlock_irqrestore(&hist_info->hist_lock, flag);
	hist_info->is_kick_ready = false;
	mdss_mdp_hist_irq_disable(done_bit);
	writel_relaxed(BIT(1), ctl_base);/* cancel */
	ret = 0;
exit:
	mutex_unlock(&hist_info->hist_mutex);
	return ret;
}

int mdss_mdp_histogram_stop(struct mdss_mdp_ctl *ctl, u32 block)
{
	int i, ret = 0;
	char __iomem *ctl_base;
	u32 dspp_num, disp_num, done_bit;
	struct pp_hist_col_info *hist_info;
	u32 mixer_cnt, mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl)
		return -EINVAL;

	if ((PP_BLOCK(block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	disp_num = PP_BLOCK(block) - MDP_LOGICAL_BLOCK_DISP_0;
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
	if (PP_LOCAT(block) == MDSS_PP_SSPP_CFG) {
		i = MDSS_PP_ARG_MASK & block;
		if (!i) {
			pr_warn("Must pass pipe arguments, %d", i);
			goto hist_stop_clk;
		}

		for (i = 0; i < MDSS_PP_ARG_NUM; i++) {
			if (!PP_ARG(i, block))
				continue;
			pipe = mdss_mdp_pipe_get(mdata, BIT(i));
			if (IS_ERR_OR_NULL(pipe) ||
					pipe->num > MDSS_MDP_SSPP_VIG2) {
				pr_warn("Invalid Hist pipe (%d)", i);
				continue;
			}
			done_bit = 3 << (pipe->num * 4);
			hist_info = &pipe->pp_res.hist;
			ctl_base = pipe->base +
				MDSS_MDP_REG_VIG_HIST_CTL_BASE;
			ret = pp_histogram_disable(hist_info, done_bit,
								ctl_base);
			mdss_mdp_pipe_unmap(pipe);
			if (ret)
				goto hist_stop_clk;
		}
	} else if (PP_LOCAT(block) == MDSS_PP_DSPP_CFG) {
		for (i = 0; i < mixer_cnt; i++) {
			dspp_num = mixer_id[i];
			done_bit = 3 << ((dspp_num * 4) + 12);
			hist_info = &mdss_pp_res->dspp_hist[dspp_num];
			ctl_base = mdss_mdp_get_dspp_addr_off(dspp_num) +
				  MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
			ret = pp_histogram_disable(hist_info, done_bit,
								ctl_base);
			if (ret)
				goto hist_stop_clk;
			mdss_pp_res->pp_disp_flags[disp_num] |=
							PP_FLAGS_DIRTY_HIST_COL;
		}
	}
hist_stop_clk:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
hist_stop_exit:
	if (!ret && (PP_LOCAT(block) == MDSS_PP_DSPP_CFG))
		mdss_mdp_pp_setup(ctl);
	return ret;
}

static int pp_hist_collect(struct mdss_mdp_ctl *ctl,
				struct mdp_histogram_data *hist,
				struct pp_hist_col_info *hist_info,
				char __iomem *ctl_base)
{
	int wait_ret, ret = 0;
	u32 timeout;
	char __iomem *v_base;
	unsigned long flag;
	struct mdss_pipe_pp_res *res;
	struct mdss_mdp_pipe *pipe;

	mutex_lock(&hist_info->hist_mutex);
	if ((hist_info->col_en == 0) ||
			(hist_info->col_state == HIST_UNKNOWN)) {
		ret = -EINVAL;
		goto hist_collect_exit;
	}
	spin_lock_irqsave(&hist_info->hist_lock, flag);
	/* wait for hist done if cache has no data */
	if (hist_info->col_state != HIST_READY) {
		hist_info->read_request = true;
		spin_unlock_irqrestore(&hist_info->hist_lock, flag);
		timeout = HIST_WAIT_TIMEOUT(hist_info->frame_cnt);
		mutex_unlock(&hist_info->hist_mutex);
		/* flush updates before wait*/
		if (PP_LOCAT(hist->block) == MDSS_PP_DSPP_CFG)
			mdss_mdp_pp_setup(ctl);
		if (PP_LOCAT(hist->block) == MDSS_PP_SSPP_CFG) {
			res = container_of(hist_info, struct mdss_pipe_pp_res,
						hist);
			pipe = container_of(res, struct mdss_mdp_pipe, pp_res);
			pipe->params_changed++;
		}
		wait_ret = wait_for_completion_killable_timeout(
				&(hist_info->comp), timeout);

		mutex_lock(&hist_info->hist_mutex);
		if (wait_ret == 0) {
			ret = -ETIMEDOUT;
			spin_lock_irqsave(&hist_info->hist_lock, flag);
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
			hist_info->hist_cnt_time++;
			hist_info->col_state = HIST_READY;
			spin_unlock_irqrestore(&hist_info->hist_lock, flag);
		} else if (wait_ret < 0) {
			ret = -EINTR;
			pr_debug("%s: bin collection interrupted",
					__func__);
			goto hist_collect_exit;
		}
		if (hist_info->col_state != HIST_READY) {
			ret = -ENODATA;
			pr_debug("%s: state is not ready: %d",
					__func__, hist_info->col_state);
			goto hist_collect_exit;
		}
	} else {
		spin_unlock_irqrestore(&hist_info->hist_lock, flag);
	}
	spin_lock_irqsave(&hist_info->hist_lock, flag);
	if (hist_info->col_state == HIST_READY) {
		spin_unlock_irqrestore(&hist_info->hist_lock, flag);
		v_base = ctl_base + 0x1C;
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		pp_hist_read(v_base, hist_info);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		spin_lock_irqsave(&hist_info->hist_lock, flag);
		hist_info->read_request = false;
		hist_info->col_state = HIST_IDLE;
	}
	spin_unlock_irqrestore(&hist_info->hist_lock, flag);
hist_collect_exit:
	mutex_unlock(&hist_info->hist_mutex);
	return ret;
}

int mdss_mdp_hist_collect(struct mdss_mdp_ctl *ctl,
					struct mdp_histogram_data *hist)
{
	int i, j, off, ret = 0;
	struct pp_hist_col_info *hist_info;
	u32 dspp_num, disp_num;
	char __iomem *ctl_base;
	u32 hist_cnt, mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];
	u32 *hist_concat = NULL;
	u32 *hist_data_addr;
	u32 pipe_cnt = 0;
	u32 pipe_num = MDSS_MDP_SSPP_VIG0;
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl)
		return -EINVAL;

	if ((PP_BLOCK(hist->block) < MDP_LOGICAL_BLOCK_DISP_0) ||
		(PP_BLOCK(hist->block) >= MDP_BLOCK_MAX))
		return -EINVAL;

	disp_num = PP_BLOCK(hist->block) - MDP_LOGICAL_BLOCK_DISP_0;
	hist_cnt = mdss_mdp_get_ctl_mixers(disp_num, mixer_id);

	if (!hist_cnt) {
		pr_err("%s, no dspp connects to disp %d",
			__func__, disp_num);
		ret = -EPERM;
		goto hist_collect_exit;
	}
	if (hist_cnt >= MDSS_MDP_MAX_DSPP) {
		pr_err("%s, Too many dspp connects to disp %d",
			__func__, hist_cnt);
		ret = -EPERM;
		goto hist_collect_exit;
	}
	if (PP_LOCAT(hist->block) == MDSS_PP_DSPP_CFG) {
		hist_info = &mdss_pp_res->dspp_hist[disp_num];
		for (i = 0; i < hist_cnt; i++) {
			dspp_num = mixer_id[i];
			hist_info = &mdss_pp_res->dspp_hist[dspp_num];
			ctl_base = mdss_mdp_get_dspp_addr_off(dspp_num) +
				MDSS_MDP_REG_DSPP_HIST_CTL_BASE;
			ret = pp_hist_collect(ctl, hist, hist_info, ctl_base);
			if (ret)
				goto hist_collect_exit;
		}
		if (hist->bin_cnt != HIST_V_SIZE) {
			pr_err("User not expecting size %d output",
							HIST_V_SIZE);
			ret = -EINVAL;
			goto hist_collect_exit;
		}
		if (hist_cnt > 1) {
			hist_concat = kmalloc(HIST_V_SIZE * sizeof(u32),
								GFP_KERNEL);
			if (!hist_concat) {
				ret = -ENOMEM;
				goto hist_collect_exit;
			}
			memset(hist_concat, 0, HIST_V_SIZE * sizeof(u32));
			for (i = 0; i < hist_cnt; i++) {
				dspp_num = mixer_id[i];
				hist_info = &mdss_pp_res->dspp_hist[dspp_num];
				mutex_lock(&hist_info->hist_mutex);
				for (j = 0; j < HIST_V_SIZE; j++)
					hist_concat[j] += hist_info->data[j];
				mutex_unlock(&hist_info->hist_mutex);
			}
			hist_data_addr = hist_concat;
		} else {
			hist_data_addr = hist_info->data;
		}
		hist_info = &mdss_pp_res->dspp_hist[disp_num];
		hist_info->hist_cnt_sent++;
	} else if (PP_LOCAT(hist->block) == MDSS_PP_SSPP_CFG) {

		hist_cnt = MDSS_PP_ARG_MASK & hist->block;
		if (!hist_cnt) {
			pr_warn("Must pass pipe arguments, %d", hist_cnt);
			goto hist_collect_exit;
		}

		/* Find the first pipe requested */
		for (i = 0; i < MDSS_PP_ARG_NUM; i++) {
			if (PP_ARG(i, hist_cnt)) {
				pipe_num = i;
				break;
			}
		}

		pipe = mdss_mdp_pipe_get(mdata, BIT(pipe_num));
		if (IS_ERR_OR_NULL(pipe)) {
			pr_warn("Invalid starting hist pipe, %d", pipe_num);
			ret = -ENODEV;
			goto hist_collect_exit;
		}
		hist_info  = &pipe->pp_res.hist;
		mdss_mdp_pipe_unmap(pipe);
		for (i = pipe_num; i < MDSS_PP_ARG_NUM; i++) {
			if (!PP_ARG(i, hist->block))
				continue;
			pipe_cnt++;
			pipe = mdss_mdp_pipe_get(mdata, BIT(i));
			if (IS_ERR_OR_NULL(pipe) ||
					pipe->num > MDSS_MDP_SSPP_VIG2) {
				pr_warn("Invalid Hist pipe (%d)", i);
				continue;
			}
			hist_info = &pipe->pp_res.hist;
			ctl_base = pipe->base +
				MDSS_MDP_REG_VIG_HIST_CTL_BASE;
			ret = pp_hist_collect(ctl, hist, hist_info, ctl_base);
			mdss_mdp_pipe_unmap(pipe);
			if (ret)
				goto hist_collect_exit;
		}
		if (pipe_cnt != 0 &&
			(hist->bin_cnt != (HIST_V_SIZE * pipe_cnt))) {
			pr_err("User not expecting size %d output",
						pipe_cnt * HIST_V_SIZE);
			ret = -EINVAL;
			goto hist_collect_exit;
		}
		if (pipe_cnt > 1) {
			hist_concat = kmalloc(HIST_V_SIZE * pipe_cnt *
						sizeof(u32), GFP_KERNEL);
			if (!hist_concat) {
				ret = -ENOMEM;
				goto hist_collect_exit;
			}

			memset(hist_concat, 0, pipe_cnt * HIST_V_SIZE *
								sizeof(u32));
			for (i = pipe_num; i < MDSS_PP_ARG_NUM; i++) {
				if (!PP_ARG(i, hist->block))
					continue;
				pipe = mdss_mdp_pipe_get(mdata, BIT(i));
				hist_info  = &pipe->pp_res.hist;
				off = HIST_V_SIZE * i;
				mutex_lock(&hist_info->hist_mutex);
				for (j = off; j < off + HIST_V_SIZE; j++)
					hist_concat[j] =
						hist_info->data[j - off];
				hist_info->hist_cnt_sent++;
				mutex_unlock(&hist_info->hist_mutex);
				mdss_mdp_pipe_unmap(pipe);
			}

			hist_data_addr = hist_concat;
		} else {
			hist_data_addr = hist_info->data;
		}
	} else {
		pr_info("No Histogram at location %d", PP_LOCAT(hist->block));
		goto hist_collect_exit;
	}
	ret = copy_to_user(hist->c0, hist_data_addr, sizeof(u32) *
								hist->bin_cnt);
hist_collect_exit:
	kfree(hist_concat);

	return ret;
}
void mdss_mdp_hist_intr_done(u32 isr)
{
	u32 isr_blk, blk_idx;
	struct pp_hist_col_info *hist_info = NULL;
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
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
				blk_idx = MDSS_MDP_SSPP_VIG0;
				isr_blk = isr & 0x3;
				isr &= ~0x3;
			} else if (isr & 0x30) {
				blk_idx = MDSS_MDP_SSPP_VIG1;
				isr_blk = (isr >> 4) & 0x3;
				isr &= ~0x30;
			} else {
				blk_idx = MDSS_MDP_SSPP_VIG2;
				isr_blk = (isr >> 8) & 0x3;
				isr &= ~0x300;
			}
			pipe = mdss_mdp_pipe_search(mdata, BIT(blk_idx));
			if (IS_ERR_OR_NULL(pipe)) {
				pr_debug("pipe DNE, %d", blk_idx);
				continue;
			}
			hist_info = &pipe->pp_res.hist;
		}
		/* Histogram Done Interrupt */
		if (hist_info && (isr_blk & 0x1) &&
			(hist_info->col_en)) {
			spin_lock(&hist_info->hist_lock);
			hist_info->col_state = HIST_READY;
			spin_unlock(&hist_info->hist_lock);
			if (hist_info->read_request)
				complete(&hist_info->comp);
		}
		/* Histogram Reset Done Interrupt */
		if ((isr_blk & 0x2) &&
			(hist_info->col_en)) {
				spin_lock(&hist_info->hist_lock);
				hist_info->col_state = HIST_IDLE;
				spin_unlock(&hist_info->hist_lock);
		}
	};
}

static struct msm_fb_data_type *mdss_get_mfd_from_index(int index)
{
	struct msm_fb_data_type *out = NULL;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int i;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if ((ctl->power_on) && (ctl->mfd)
				&& (ctl->mfd->index == index))
			out = ctl->mfd;
	}
	return out;
}

#define MDSS_AD_MAX_MIXERS 1
static int mdss_ad_init_checks(struct msm_fb_data_type *mfd)
{
	u32 mixer_id[MDSS_MDP_INTF_MAX_LAYERMIXER];
	u32 mixer_num;
	u32 ret = -EINVAL;
	int i = 0;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct msm_fb_data_type *ad_mfd = mfd;

	if (ad_mfd->ext_ad_ctrl >= 0)
		ad_mfd = mdss_get_mfd_from_index(ad_mfd->ext_ad_ctrl);

	if (!ad_mfd || !mdata)
		return ret;

	if (mdata->nad_cfgs == 0) {
		pr_debug("Assertive Display not supported by device");
		return -ENODEV;
	}

	mixer_num = mdss_mdp_get_ctl_mixers(ad_mfd->index, mixer_id);
	if (!mixer_num) {
		pr_debug("no mixers connected, %d", mixer_num);
		return -EHOSTDOWN;
	}
	if (mixer_num > MDSS_AD_MAX_MIXERS) {
		pr_debug("too many mixers, not supported, %d", mixer_num);
		return ret;
	}

	do {
		if (mixer_id[i] >= mdata->nad_cfgs) {
			pr_err("invalid mixer input, %d", mixer_id[i]);
			return ret;
		}
		i++;
	} while (i < mixer_num);

	return mixer_id[0];
}

static int mdss_mdp_get_ad(struct msm_fb_data_type *mfd,
					struct mdss_ad_info **ret_ad)
{
	int ad_num, ret = 0;
	struct mdss_data_type *mdata;
	struct mdss_ad_info *ad = NULL;
	mdata = mfd_to_mdata(mfd);

	ad_num = mdss_ad_init_checks(mfd);
	if (ad_num >= 0)
		ad = &mdata->ad_cfgs[ad_num];
	else
		ret = ad_num;
	*ret_ad = ad;
	return ret;
}

static int pp_update_ad_input(struct msm_fb_data_type *mfd)
{
	int ret;
	struct mdss_ad_info *ad;
	struct mdss_ad_input input;
	struct mdss_mdp_ctl *ctl;

	if (!mfd)
		return -EINVAL;
	ctl = mfd_to_ctl(mfd);
	if (!ctl)
		return -EINVAL;

	ret = mdss_mdp_get_ad(mfd, &ad);
	if (ret)
		return ret;
	if (!ad || ad->cfg.mode == MDSS_AD_MODE_AUTO_BL)
		return -EINVAL;

	pr_debug("backlight level changed (%d), trigger update to AD",
						mfd->bl_level);
	input.mode = ad->cfg.mode;
	if (MDSS_AD_MODE_DATA_MATCH(ad->cfg.mode, MDSS_AD_INPUT_AMBIENT))
		input.in.amb_light = ad->ad_data;
	else
		input.in.strength = ad->ad_data;
	/* call to ad_input will trigger backlight read */
	return mdss_mdp_ad_input(mfd, &input, 0);
}

int mdss_mdp_ad_config(struct msm_fb_data_type *mfd,
			struct mdss_ad_init_cfg *init_cfg)
{
	struct mdss_ad_info *ad;
	struct mdss_mdp_ctl *ctl;
	struct msm_fb_data_type *bl_mfd;
	int lin_ret = -1, inv_ret = -1, ret = 0;
	u32 ratio_temp, shift = 0;

	ret = mdss_mdp_get_ad(mfd, &ad);
	if (ret)
		return ret;
	if (mfd->panel_info->type == WRITEBACK_PANEL) {
		bl_mfd = mdss_get_mfd_from_index(0);
		if (!bl_mfd)
			return ret;
	} else {
		bl_mfd = mfd;
	}

	mutex_lock(&ad->lock);
	if (init_cfg->ops & MDP_PP_AD_INIT) {
		memcpy(&ad->init, &init_cfg->params.init,
				sizeof(struct mdss_ad_init));
		if (init_cfg->params.init.bl_lin_len == AD_BL_LIN_LEN) {
			lin_ret = copy_from_user(&ad->bl_lin,
				init_cfg->params.init.bl_lin,
				AD_BL_LIN_LEN * sizeof(uint32_t));
			inv_ret = copy_from_user(&ad->bl_lin_inv,
				init_cfg->params.init.bl_lin_inv,
				AD_BL_LIN_LEN * sizeof(uint32_t));
			if (lin_ret || inv_ret)
				ret = -ENOMEM;
			ratio_temp =  mfd->panel_info->bl_max / AD_BL_LIN_LEN;
			while (ratio_temp > 0) {
				ratio_temp = ratio_temp >> 1;
				shift++;
			}
			ad->bl_bright_shift = shift;
		} else if (init_cfg->params.init.bl_lin_len) {
			ret = -EINVAL;
		}
		if (!lin_ret && !inv_ret)
			ad->state |= PP_AD_STATE_BL_LIN;
		else
			ad->state &= !PP_AD_STATE_BL_LIN;

		ad->sts |= PP_AD_STS_DIRTY_INIT;
	} else if (init_cfg->ops & MDP_PP_AD_CFG) {
		memcpy(&ad->cfg, &init_cfg->params.cfg,
				sizeof(struct mdss_ad_cfg));
		/*
		 * TODO: specify panel independent range of input from cfg,
		 * scale input backlight_scale to panel bl_max's range
		 */
		ad->cfg.backlight_scale = bl_mfd->panel_info->bl_max;
		ad->sts |= PP_AD_STS_DIRTY_CFG;
	}

	if (!ret && (init_cfg->ops & MDP_PP_OPS_DISABLE)) {
		ad->sts &= ~PP_STS_ENABLE;
		mutex_unlock(&ad->lock);
		cancel_work_sync(&ad->calc_work);
		mutex_lock(&ad->lock);
		ad->mfd = NULL;
		ad->bl_mfd = NULL;
	} else if (!ret && (init_cfg->ops & MDP_PP_OPS_ENABLE)) {
		ad->sts |= PP_STS_ENABLE;
		ad->mfd = mfd;
		ad->bl_mfd = bl_mfd;
	}
	mutex_unlock(&ad->lock);
	ctl = mfd_to_ctl(mfd);
	if (!ret)
		mdss_mdp_pp_setup(ctl);
	return ret;
}

int mdss_mdp_ad_input(struct msm_fb_data_type *mfd,
			struct mdss_ad_input *input, int wait) {
	int ret = 0;
	struct mdss_ad_info *ad;
	struct mdss_mdp_ctl *ctl;
	u32 bl;

	ret = mdss_mdp_get_ad(mfd, &ad);
	if (ret)
		return ret;

	mutex_lock(&ad->lock);
	if ((!PP_AD_STATE_IS_INITCFG(ad->state) &&
			!PP_AD_STS_IS_DIRTY(ad->sts)) &&
			!input->mode == MDSS_AD_MODE_CALIB) {
		pr_warn("AD not initialized or configured.");
		ret = -EPERM;
		goto error;
	}
	switch (input->mode) {
	case MDSS_AD_MODE_AUTO_BL:
	case MDSS_AD_MODE_AUTO_STR:
		if (!MDSS_AD_MODE_DATA_MATCH(ad->cfg.mode,
				MDSS_AD_INPUT_AMBIENT)) {
			ret = -EINVAL;
			goto error;
		}
		ad->ad_data_mode = MDSS_AD_INPUT_AMBIENT;
		pr_debug("ambient = %d", input->in.amb_light);
		ad->ad_data = input->in.amb_light;
		ad->calc_itr = ad->cfg.stab_itr;
		ad->sts |= PP_AD_STS_DIRTY_VSYNC;
		ad->sts |= PP_AD_STS_DIRTY_DATA;
		break;
	case MDSS_AD_MODE_TARG_STR:
	case MDSS_AD_MODE_MAN_STR:
		if (!MDSS_AD_MODE_DATA_MATCH(ad->cfg.mode,
				MDSS_AD_INPUT_STRENGTH)) {
			ret = -EINVAL;
			goto error;
		}
		ad->ad_data_mode = MDSS_AD_INPUT_STRENGTH;
		pr_debug("strength = %d", input->in.strength);
		ad->ad_data = input->in.strength;
		ad->calc_itr = ad->cfg.stab_itr;
		ad->sts |= PP_AD_STS_DIRTY_VSYNC;
		ad->sts |= PP_AD_STS_DIRTY_DATA;
		break;
	case MDSS_AD_MODE_CALIB:
		wait = 0;
		if (mfd->calib_mode) {
			bl = input->in.calib_bl;
			if (bl >= AD_BL_LIN_LEN) {
				pr_warn("calib_bl 255 max!");
				break;
			}
			mutex_unlock(&ad->lock);
			mutex_lock(&mfd->bl_lock);
			MDSS_BRIGHT_TO_BL(bl, bl, mfd->panel_info->bl_max,
							MDSS_MAX_BL_BRIGHTNESS);
			mdss_fb_set_backlight(mfd, bl);
			mutex_unlock(&mfd->bl_lock);
			mutex_lock(&ad->lock);
		} else {
			pr_warn("should be in calib mode");
		}
		break;
	default:
		pr_warn("invalid default %d", input->mode);
		ret = -EINVAL;
		goto error;
	}
error:
	mutex_unlock(&ad->lock);
	if (!ret) {
		if (wait) {
			mutex_lock(&ad->lock);
			init_completion(&ad->comp);
			mutex_unlock(&ad->lock);
		}
		ctl = mfd_to_ctl(mfd);
		mdss_mdp_pp_setup(ctl);
		if (wait) {
			ret = wait_for_completion_interruptible_timeout(
					&ad->comp, HIST_WAIT_TIMEOUT(1));
			if (ret == 0)
				ret = -ETIMEDOUT;
			else if (ret > 0)
				input->output = ad->last_str;
		}
	}
	return ret;
}

static void pp_ad_input_write(struct mdss_ad_info *ad, u32 bl_lvl)
{
	char __iomem *base = ad->base;
	switch (ad->cfg.mode) {
	case MDSS_AD_MODE_AUTO_BL:
		writel_relaxed(ad->ad_data, base + MDSS_MDP_REG_AD_AL);
		break;
	case MDSS_AD_MODE_AUTO_STR:
		writel_relaxed(bl_lvl, base + MDSS_MDP_REG_AD_BL);
		writel_relaxed(ad->ad_data, base + MDSS_MDP_REG_AD_AL);
		break;
	case MDSS_AD_MODE_TARG_STR:
		writel_relaxed(bl_lvl, base + MDSS_MDP_REG_AD_BL);
		writel_relaxed(ad->ad_data, base + MDSS_MDP_REG_AD_TARG_STR);
		break;
	case MDSS_AD_MODE_MAN_STR:
		writel_relaxed(bl_lvl, base + MDSS_MDP_REG_AD_BL);
		writel_relaxed(ad->ad_data, base + MDSS_MDP_REG_AD_STR_MAN);
		break;
	default:
		pr_warn("Invalid mode! %d", ad->cfg.mode);
		break;
	}
}

static void pp_ad_init_write(struct mdss_ad_info *ad)
{
	u32 temp;
	char __iomem *base = ad->base;
	writel_relaxed(ad->init.i_control[0] & 0x1F,
				base + MDSS_MDP_REG_AD_CON_CTRL_0);
	writel_relaxed(ad->init.i_control[1] << 8,
				base + MDSS_MDP_REG_AD_CON_CTRL_1);

	temp = ad->init.white_lvl << 16;
	temp |= ad->init.black_lvl & 0xFFFF;
	writel_relaxed(temp, base + MDSS_MDP_REG_AD_BW_LVL);

	writel_relaxed(ad->init.var, base + MDSS_MDP_REG_AD_VAR);

	writel_relaxed(ad->init.limit_ampl, base + MDSS_MDP_REG_AD_AMP_LIM);

	writel_relaxed(ad->init.i_dither, base + MDSS_MDP_REG_AD_DITH);

	temp = ad->init.slope_max << 8;
	temp |= ad->init.slope_min & 0xFF;
	writel_relaxed(temp, base + MDSS_MDP_REG_AD_SLOPE);

	writel_relaxed(ad->init.dither_ctl, base + MDSS_MDP_REG_AD_DITH_CTRL);

	writel_relaxed(ad->init.format, base + MDSS_MDP_REG_AD_CTRL_0);
	writel_relaxed(ad->init.auto_size, base + MDSS_MDP_REG_AD_CTRL_1);

	temp = ad->init.frame_w << 16;
	temp |= ad->init.frame_h & 0xFFFF;
	writel_relaxed(temp, base + MDSS_MDP_REG_AD_FRAME_SIZE);

	temp = ad->init.logo_v << 8;
	temp |= ad->init.logo_h & 0xFF;
	writel_relaxed(temp, base + MDSS_MDP_REG_AD_LOGO_POS);

	pp_ad_cfg_lut(base + MDSS_MDP_REG_AD_LUT_FI, ad->init.asym_lut);
	pp_ad_cfg_lut(base + MDSS_MDP_REG_AD_LUT_CC, ad->init.color_corr_lut);
}

#define MDSS_PP_AD_DEF_CALIB 0x6E
static void pp_ad_cfg_write(struct mdss_ad_info *ad)
{
	char __iomem *base = ad->base;
	u32 temp, temp_calib = MDSS_PP_AD_DEF_CALIB;
	switch (ad->cfg.mode) {
	case MDSS_AD_MODE_AUTO_BL:
		temp = ad->cfg.backlight_max << 16;
		temp |= ad->cfg.backlight_min & 0xFFFF;
		writel_relaxed(temp, base + MDSS_MDP_REG_AD_BL_MINMAX);
		writel_relaxed(ad->cfg.amb_light_min,
				base + MDSS_MDP_REG_AD_AL_MIN);
		temp = ad->cfg.filter[1] << 16;
		temp |= ad->cfg.filter[0] & 0xFFFF;
		writel_relaxed(temp, base + MDSS_MDP_REG_AD_AL_FILT);
	case MDSS_AD_MODE_AUTO_STR:
		pp_ad_cfg_lut(base + MDSS_MDP_REG_AD_LUT_AL,
				ad->cfg.al_calib_lut);
		writel_relaxed(ad->cfg.strength_limit,
				base + MDSS_MDP_REG_AD_STR_LIM);
		temp = ad->cfg.calib[3] << 16;
		temp |= ad->cfg.calib[2] & 0xFFFF;
		writel_relaxed(temp, base + MDSS_MDP_REG_AD_CALIB_CD);
		writel_relaxed(ad->cfg.t_filter_recursion,
				base + MDSS_MDP_REG_AD_TFILT_CTRL);
		temp_calib = ad->cfg.calib[0] & 0xFFFF;
	case MDSS_AD_MODE_TARG_STR:
		temp = ad->cfg.calib[1] << 16;
		temp |= temp_calib;
		writel_relaxed(temp, base + MDSS_MDP_REG_AD_CALIB_AB);
	case MDSS_AD_MODE_MAN_STR:
		writel_relaxed(ad->cfg.backlight_scale,
				base + MDSS_MDP_REG_AD_BL_MAX);
		writel_relaxed(ad->cfg.mode, base + MDSS_MDP_REG_AD_MODE_SEL);
		pr_debug("stab_itr = %d", ad->cfg.stab_itr);
		break;
	default:
		break;
	}
}

static void pp_ad_vsync_handler(struct mdss_mdp_ctl *ctl, ktime_t t)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_ad_info *ad;

	if (ctl->mixer_left && ctl->mixer_left->num < mdata->nad_cfgs) {
		ad = &mdata->ad_cfgs[ctl->mixer_left->num];
		queue_work(mdata->ad_calc_wq, &ad->calc_work);
	}
}

#define MDSS_PP_AD_BYPASS_DEF 0x101
static int mdss_mdp_ad_setup(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct mdss_ad_info *ad;
	struct mdss_mdp_ctl *ctl = mfd_to_ctl(mfd);
	struct msm_fb_data_type *bl_mfd;
	char __iomem *base;
	u32 temp;
	u32 bypass = MDSS_PP_AD_BYPASS_DEF, bl;

	ret = mdss_mdp_get_ad(mfd, &ad);
	if (ret)
		return ret;
	if (mfd->panel_info->type == WRITEBACK_PANEL) {
		bl_mfd = mdss_get_mfd_from_index(0);
		if (!bl_mfd)
			return ret;
	} else {
		bl_mfd = mfd;
	}


	base = ad->base;

	mutex_lock(&ad->lock);
	if (ad->sts != last_sts || ad->state != last_state) {
		last_sts = ad->sts;
		last_state = ad->state;
		pr_debug("begining: ad->sts = 0x%08x, state = 0x%08x", ad->sts,
								ad->state);
	}
	if (!PP_AD_STS_IS_DIRTY(ad->sts) &&
		(ad->sts & PP_AD_STS_DIRTY_DATA)) {
		/*
		 * Write inputs to regs when the data has been updated or
		 * Assertive Display is up and running as long as there are
		 * no updates to AD init or cfg
		 */
		ad->sts &= ~PP_AD_STS_DIRTY_DATA;
		ad->state |= PP_AD_STATE_DATA;
		mutex_lock(&bl_mfd->bl_lock);
		bl = bl_mfd->bl_level;
		pr_debug("dirty data, last_bl = %d ", ad->last_bl);
		if ((ad->cfg.mode == MDSS_AD_MODE_AUTO_STR) &&
							(ad->last_bl != bl)) {
			ad->last_bl = bl;
			ad->calc_itr = ad->cfg.stab_itr;
			ad->sts |= PP_AD_STS_DIRTY_VSYNC;
			if (ad->state & PP_AD_STATE_BL_LIN) {
				bl = ad->bl_lin[bl >> ad->bl_bright_shift];
				bl = bl << ad->bl_bright_shift;
			}
			mutex_unlock(&bl_mfd->bl_lock);
		}
		mutex_unlock(&mfd->bl_lock);
		pp_ad_input_write(ad, bl);
	}

	if (ad->sts & PP_AD_STS_DIRTY_CFG) {
		ad->sts &= ~PP_AD_STS_DIRTY_CFG;
		ad->state |= PP_AD_STATE_CFG;

		pp_ad_cfg_write(ad);

		if (!MDSS_AD_MODE_DATA_MATCH(ad->cfg.mode, ad->ad_data_mode)) {
			ad->sts &= ~PP_AD_STS_DIRTY_DATA;
			ad->state &= ~PP_AD_STATE_DATA;
			pr_debug("Mode switched, data invalidated!");
		}
	}
	if (ad->sts & PP_AD_STS_DIRTY_INIT) {
		ad->sts &= ~PP_AD_STS_DIRTY_INIT;
		ad->state |= PP_AD_STATE_INIT;
		pp_ad_init_write(ad);
	}

	/* update ad screen size if it has changed since last configuration */
	if (mfd->panel_info->type == WRITEBACK_PANEL &&
		(ad->init.frame_w != ctl->width ||
			ad->init.frame_h != ctl->height)) {
		pr_debug("changing from %dx%d to %dx%d", ad->init.frame_w,
							ad->init.frame_h,
							ctl->width,
							ctl->height);
		ad->init.frame_w = ctl->width;
		ad->init.frame_h = ctl->height;
		temp = ad->init.frame_w << 16;
		temp |= ad->init.frame_h & 0xFFFF;
		writel_relaxed(temp, base + MDSS_MDP_REG_AD_FRAME_SIZE);
	}

	if ((ad->sts & PP_STS_ENABLE) && PP_AD_STATE_IS_READY(ad->state)) {
		bypass = 0;
		ret = 1;
		ad->state |= PP_AD_STATE_RUN;
		mutex_lock(&bl_mfd->bl_lock);
		if (bl_mfd != mfd)
			bl_mfd->ext_ad_ctrl = mfd->index;
		bl_mfd->mdp.update_ad_input = pp_update_ad_input;
		bl_mfd->ext_bl_ctrl = ad->cfg.bl_ctrl_mode;
		mutex_unlock(&bl_mfd->bl_lock);

	} else {
		if (ad->state & PP_AD_STATE_RUN) {
			ret = 1;
			/* Clear state and regs when going to off state*/
			ad->sts = 0;
			ad->sts |= PP_AD_STS_DIRTY_VSYNC;
			ad->state &= !PP_AD_STATE_INIT;
			ad->state &= !PP_AD_STATE_CFG;
			ad->state &= !PP_AD_STATE_DATA;
			ad->state &= !PP_AD_STATE_BL_LIN;
			ad->bl_bright_shift = 0;
			ad->ad_data = 0;
			ad->ad_data_mode = 0;
			ad->last_bl = 0;
			ad->calc_itr = 0;
			memset(&ad->bl_lin, 0, sizeof(uint32_t) *
								AD_BL_LIN_LEN);
			memset(&ad->bl_lin_inv, 0, sizeof(uint32_t) *
								AD_BL_LIN_LEN);
			memset(&ad->init, 0, sizeof(struct mdss_ad_init));
			memset(&ad->cfg, 0, sizeof(struct mdss_ad_cfg));
			mutex_lock(&bl_mfd->bl_lock);
			bl_mfd->mdp.update_ad_input = NULL;
			bl_mfd->ext_bl_ctrl = 0;
			bl_mfd->ext_ad_ctrl = -1;
			mutex_unlock(&bl_mfd->bl_lock);
		}
		ad->state &= ~PP_AD_STATE_RUN;
	}
	writel_relaxed(bypass, base);

	if (PP_AD_STS_DIRTY_VSYNC & ad->sts) {
		pr_debug("dirty vsync, calc_itr = %d", ad->calc_itr);
		ad->sts &= ~PP_AD_STS_DIRTY_VSYNC;
		if (!(PP_AD_STATE_VSYNC & ad->state) && ad->calc_itr &&
					(ad->state & PP_AD_STATE_RUN)) {
			ctl->add_vsync_handler(ctl, &ad->handle);
			ad->state |= PP_AD_STATE_VSYNC;
		} else if ((PP_AD_STATE_VSYNC & ad->state) &&
			(!ad->calc_itr || !(PP_AD_STATE_RUN & ad->state))) {
			ctl->remove_vsync_handler(ctl, &ad->handle);
			ad->state &= ~PP_AD_STATE_VSYNC;
		}
	}

	if (ad->sts != last_sts || ad->state != last_state) {
		last_sts = ad->sts;
		last_state = ad->state;
		pr_debug("end: ad->sts = 0x%08x, state = 0x%08x", ad->sts,
								ad->state);
	}
	mutex_unlock(&ad->lock);
	return ret;
}

#define MDSS_PP_AD_SLEEP 10
static void pp_ad_calc_worker(struct work_struct *work)
{
	struct mdss_ad_info *ad;
	struct mdss_mdp_ctl *ctl;
	struct msm_fb_data_type *mfd, *bl_mfd;
	u32 bl, calc_done = 0;
	ad = container_of(work, struct mdss_ad_info, calc_work);

	mutex_lock(&ad->lock);
	if (!ad->mfd  || !ad->bl_mfd || !(ad->sts & PP_STS_ENABLE)) {
		mutex_unlock(&ad->lock);
		return;
	}
	mfd = ad->mfd;
	bl_mfd = ad->bl_mfd;
	ctl = mfd_to_ctl(ad->mfd);

	if ((ad->cfg.mode == MDSS_AD_MODE_AUTO_STR) && (ad->last_bl == 0)) {
		mutex_unlock(&ad->lock);
		return;
	}

	if (PP_AD_STATE_RUN & ad->state) {
		/* Kick off calculation */
		ad->calc_itr--;
		writel_relaxed(1, ad->base + MDSS_MDP_REG_AD_START_CALC);
	}
	if (ad->state & PP_AD_STATE_RUN) {
		do {
			calc_done = readl_relaxed(ad->base +
				MDSS_MDP_REG_AD_CALC_DONE);
			if (!calc_done)
				usleep(MDSS_PP_AD_SLEEP);
		} while (!calc_done && (ad->state & PP_AD_STATE_RUN));
		if (calc_done) {
			ad->last_str = 0xFF & readl_relaxed(ad->base +
						MDSS_MDP_REG_AD_STR_OUT);
			if (MDSS_AD_RUNNING_AUTO_BL(ad)) {
				bl = 0xFFFF & readl_relaxed(ad->base +
						MDSS_MDP_REG_AD_BL_OUT);
				if (ad->state & PP_AD_STATE_BL_LIN) {
					bl = bl >> ad->bl_bright_shift;
					bl = min_t(u32, bl,
						MDSS_MAX_BL_BRIGHTNESS);
					bl = ad->bl_lin_inv[bl];
					bl = bl << ad->bl_bright_shift;
				}
				pr_debug("calc bl = %d", bl);
				ad->last_str |= bl << 16;
				mutex_lock(&ad->bl_mfd->bl_lock);
				if (ad->bl_mfd->bl_level)
					mdss_fb_set_backlight(ad->bl_mfd, bl);
				mutex_unlock(&ad->bl_mfd->bl_lock);
			}
			pr_debug("calc_str = %d, calc_itr %d",
							ad->last_str & 0xFF,
							ad->calc_itr);
		} else {
			ad->last_str = 0xFFFFFFFF;
		}
	}
	complete(&ad->comp);

	if (!ad->calc_itr) {
		ad->state &= ~PP_AD_STATE_VSYNC;
		ctl->remove_vsync_handler(ctl, &ad->handle);
	}
	mutex_unlock(&ad->lock);
	mutex_lock(&mfd->lock);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, BIT(13 + ad->num));
	mutex_unlock(&mfd->lock);

	/* Trigger update notify to wake up those waiting for display updates */
	mdss_fb_update_notify_update(bl_mfd);
}

#define PP_AD_LUT_LEN 33
static void pp_ad_cfg_lut(char __iomem *addr, u32 *data)
{
	int i;
	u32 temp;

	for (i = 0; i < PP_AD_LUT_LEN - 1; i += 2) {
		temp = data[i+1] << 16;
		temp |= (data[i] & 0xFFFF);
		writel_relaxed(temp, addr + (i*2));
	}
	writel_relaxed(data[PP_AD_LUT_LEN - 1] << 16,
			addr + ((PP_AD_LUT_LEN - 1) * 2));
}

int mdss_mdp_ad_addr_setup(struct mdss_data_type *mdata, u32 *ad_off)
{
	u32 i;
	int rc = 0;

	mdata->ad_cfgs = devm_kzalloc(&mdata->pdev->dev,
				sizeof(struct mdss_ad_info) * mdata->nad_cfgs,
				GFP_KERNEL);

	if (!mdata->ad_cfgs) {
		pr_err("unable to setup assertive display:devm_kzalloc fail\n");
		return -ENOMEM;
	}

	mdata->ad_calc_wq = create_singlethread_workqueue("ad_calc_wq");
	for (i = 0; i < mdata->nad_cfgs; i++) {
		mdata->ad_cfgs[i].base = mdata->mdp_base + ad_off[i];
		mdata->ad_cfgs[i].num = i;
		mdata->ad_cfgs[i].calc_itr = 0;
		mdata->ad_cfgs[i].last_str = 0xFFFFFFFF;
		mdata->ad_cfgs[i].last_bl = 0;
		mutex_init(&mdata->ad_cfgs[i].lock);
		mdata->ad_cfgs[i].handle.vsync_handler = pp_ad_vsync_handler;
		mdata->ad_cfgs[i].handle.cmd_post_flush = true;
		INIT_WORK(&mdata->ad_cfgs[i].calc_work, pp_ad_calc_worker);
	}
	return rc;
}

static int is_valid_calib_addr(void *addr)
{
	int ret = 0;
	unsigned int ptr;
	ptr = (unsigned int) addr;
	/* if request is outside the MDP reg-map or is not aligned 4 */
	if (ptr > 0x5138 || ptr % 0x4)
		goto end;
	if (ptr >= 0x100 && ptr <= 0x5138) {
		/* if ptr is in dspp range */
		if (ptr >= 0x4600 && ptr <= 0x5138) {
			/* if ptr is in dspp0 range*/
			if (ptr >= 0x4600 && ptr <= 0x4938)
				ptr -= 0x4600;
			/* if ptr is in dspp1 range */
			else if (ptr >= 0x4a00 && ptr <= 0x4d38)
				ptr -= 0x4a00;
			/* if ptr is in dspp2 range */
			else if (ptr >= 0x4e00 && ptr <= 0x5138)
				ptr -= 0x4e00;
			/* if ptr is in pcc plane rgb coeff.range */
			if (ptr >= 0x30 && ptr <= 0xe8)
				ret = 1;
			/* if ptr is in ARLUT red range */
			else if (ptr >= 0x2b0 && ptr <= 0x2b8)
				ret = 1;
			/* if ptr is in PA range */
			else if (ptr >= 0x238 && ptr <= 0x244)
				ret = 1;
			 /* if ptr is in ARLUT green range */
			else if (ptr >= 0x2c0 && ptr <= 0x2c8)
				ret = 1;
			/* if ptr is in ARLUT blue range or
			    gamut map table range */
			else if (ptr >= 0x2d0 && ptr <= 0x338)
				ret = 1;
			/* if ptr is dspp0,dspp1,dspp2 op mode
						register */
			else if (ptr == 0)
				ret = 1;
		} else if (ptr >= 0x600 && ptr <= 0x608)
				ret = 1;
		else if (ptr >= 0x400 && ptr <= 0x408)
				ret = 1;
		else if ((ptr == 0x1830) || (ptr == 0x1c30) ||
				(ptr == 0x1430) || (ptr == 0x1e38))
				ret = 1;
		else if ((ptr == 0x1e3c) || (ptr == 0x1e30))
				ret = 1;
		else if (ptr >= 0x3220 && ptr <= 0x3228)
				ret = 1;
		else if (ptr == 0x3200 || ptr == 0x100)
				ret = 1;
		else if (ptr == 0x104 || ptr == 0x614 || ptr == 0x714 ||
			ptr == 0x814 || ptr == 0x914 || ptr == 0xa14)
				ret = 1;
		else if (ptr == 0x618 || ptr == 0x718 || ptr == 0x818 ||
				 ptr == 0x918 || ptr == 0xa18)
				ret = 1;
		else if (ptr == 0x2234 || ptr == 0x1e34 || ptr == 0x2634)
				ret = 1;
	} else if (ptr == 0x0)
		ret = 1;
end:
	return ret;
}

int mdss_mdp_calib_config(struct mdp_calib_config_data *cfg, u32 *copyback)
{
	int ret = -1;
	void *ptr = (void *) cfg->addr;

	if (is_valid_calib_addr(ptr))
		ret = 0;
	else
		return ret;
	ptr = (void *)(((unsigned int) ptr) + (mdss_res->mdp_base));
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (cfg->ops & MDP_PP_OPS_READ) {
		cfg->data = readl_relaxed(ptr);
		*copyback = 1;
		ret = 0;
	} else if (cfg->ops & MDP_PP_OPS_WRITE) {
		writel_relaxed(cfg->data, ptr);
		ret = 0;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	return ret;
}

int mdss_mdp_calib_mode(struct msm_fb_data_type *mfd,
				struct mdss_calib_cfg *cfg)
{
	if (!mdss_pp_res || !mfd)
		return -EINVAL;
	mutex_lock(&mdss_pp_mutex);
	mfd->calib_mode = cfg->calib_mask;
	mutex_unlock(&mdss_pp_mutex);
	return 0;
}

int mdss_mdp_calib_config_buffer(struct mdp_calib_config_buffer *cfg,
						u32 *copyback)
{
	int ret = -1, counter;
	uint32_t *buff = NULL, *buff_org = NULL;
	void *ptr;
	int i = 0;

	if (!cfg) {
		pr_err("Invalid buffer pointer");
		return ret;
	}

	if (cfg->size == 0) {
		pr_err("Invalid buffer size");
		return ret;
	}

	counter = cfg->size / (sizeof(uint32_t) * 2);
	buff_org = buff = kzalloc(cfg->size, GFP_KERNEL);
	if (buff == NULL) {
		pr_err("Allocation failed");
		return ret;
	}

	if (copy_from_user(buff, cfg->buffer, cfg->size)) {
		kfree(buff);
		pr_err("Copy failed");
		return ret;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (cfg->ops & MDP_PP_OPS_READ) {
		for (i = 0 ; i < counter ; i++) {
			if (is_valid_calib_addr((void *) *buff)) {
				ret = 0;
			} else {
				ret = -1;
				pr_err("Address validation failed");
				break;
			}

			ptr = (void *)(((unsigned int) *buff) +
					 (mdss_res->mdp_base));
			buff++;
			*buff = readl_relaxed(ptr);
			buff++;
		}
		if (!ret)
			ret = copy_to_user(cfg->buffer, buff_org, cfg->size);
		*copyback = 1;
	} else if (cfg->ops & MDP_PP_OPS_WRITE) {
		for (i = 0 ; i < counter ; i++) {
			if (is_valid_calib_addr((void *) *buff)) {
				ret = 0;
			} else {
				ret = -1;
				pr_err("Address validation failed");
				break;
			}

			ptr = (void *)(((unsigned int) *buff) +
					 (mdss_res->mdp_base));
			buff++;
			writel_relaxed(*buff, ptr);
			buff++;
		}
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	kfree(buff_org);
	return ret;
}
