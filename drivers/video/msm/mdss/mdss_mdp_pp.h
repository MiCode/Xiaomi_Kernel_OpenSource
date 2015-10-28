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

#ifndef MDSS_MDP_PP_DEBUG_H
#define MDSS_MDP_PP_DEBUG_H

#include <linux/msm_mdp.h>

#define MDSS_BLOCK_DISP_NUM (MDP_BLOCK_MAX - MDP_LOGICAL_BLOCK_DISP_0)

/* PP STS related flags */
#define PP_STS_ENABLE	0x1
#define PP_STS_GAMUT_FIRST	0x2
#define PP_STS_PA_LUT_FIRST	0x4

#define PP_STS_PA_HUE_MASK		0x2
#define PP_STS_PA_SAT_MASK		0x4
#define PP_STS_PA_VAL_MASK		0x8
#define PP_STS_PA_CONT_MASK		0x10
#define PP_STS_PA_MEM_PROTECT_EN	0x20
#define PP_STS_PA_MEM_COL_SKIN_MASK	0x40
#define PP_STS_PA_MEM_COL_FOL_MASK	0x80
#define PP_STS_PA_MEM_COL_SKY_MASK	0x100
#define PP_STS_PA_SIX_ZONE_HUE_MASK	0x200
#define PP_STS_PA_SIX_ZONE_SAT_MASK	0x400
#define PP_STS_PA_SIX_ZONE_VAL_MASK	0x800
#define PP_STS_PA_SAT_ZERO_EXP_EN	0x1000
#define PP_STS_PA_MEM_PROT_HUE_EN	0x2000
#define PP_STS_PA_MEM_PROT_SAT_EN	0x4000
#define PP_STS_PA_MEM_PROT_VAL_EN	0x8000
#define PP_STS_PA_MEM_PROT_CONT_EN	0x10000
#define PP_STS_PA_MEM_PROT_BLEND_EN	0x20000
#define PP_STS_PA_MEM_PROT_SIX_EN	0x40000

/* Demo mode macros */
#define MDSS_SIDE_NONE	0
#define MDSS_SIDE_LEFT	1
#define MDSS_SIDE_RIGHT	2
/* size calculated for c0,c1_c2 for 4 tables */
#define GAMUT_COLOR_COEFF_SIZE_V1_7 (2 * MDP_GAMUT_TABLE_V1_7_SZ * 4)
/* 16 entries for c0,c1,c2 */
#define GAMUT_SCALE_OFFSET_SIZE_V1_7 (3 * MDP_GAMUT_SCALE_OFF_SZ)
#define GAMUT_TOTAL_TABLE_SIZE_V1_7 (GAMUT_COLOR_COEFF_SIZE_V1_7 + \
				  GAMUT_SCALE_OFFSET_SIZE_V1_7)

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


enum pp_block_opmodes {
	PP_OPMODE_VIG = 1,
	PP_OPMODE_DSPP,
	PP_OPMODE_MAX
};

enum pp_config_block {
	SSPP_RGB = 1,
	SSPP_DMA,
	SSPP_VIG,
	DSPP,
	LM
};

struct mdp_pp_feature_ops {
	u32 feature;
	int (*pp_get_config)(char __iomem *base_addr, void *cfg_data,
			u32 block_type, u32 disp_num);
	int (*pp_set_config)(char __iomem *base_addr,
		struct pp_sts_type *pp_sts, void *cfg_data,
		u32 block_type);
	int (*pp_get_version)(u32 *version);
};

struct mdp_pp_driver_ops {
	struct mdp_pp_feature_ops pp_ops[PP_FEATURE_MAX];
	void (*pp_opmode_config)(int location, struct pp_sts_type *pp_sts,
			u32 *opmode, int side);
	int (*get_hist_offset)(u32 block, u32 *ctl_off);
	int (*get_hist_isr_info)(u32 *isr_mask);
	bool (*is_sspp_hist_supp)(void);
	void (*gamut_clk_gate_en)(char __iomem *base_addr);
};

struct mdss_pp_res_type_v1_7 {
	u32 pgc_lm_table_c0[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 pgc_lm_table_c1[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 pgc_lm_table_c2[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 pgc_table_c0[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 pgc_table_c1[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 pgc_table_c2[MDSS_BLOCK_DISP_NUM][PGC_LUT_ENTRIES];
	u32 igc_table_c0_c1[MDSS_BLOCK_DISP_NUM][IGC_LUT_ENTRIES];
	u32 igc_table_c2[MDSS_BLOCK_DISP_NUM][IGC_LUT_ENTRIES];
	u32 hist_lut[MDSS_BLOCK_DISP_NUM][ENHIST_LUT_ENTRIES];
	u32 six_zone_lut_p0[MDSS_BLOCK_DISP_NUM][MDP_SIX_ZONE_LUT_SIZE];
	u32 six_zone_lut_p1[MDSS_BLOCK_DISP_NUM][MDP_SIX_ZONE_LUT_SIZE];
	struct mdp_pgc_lut_data_v1_7 pgc_dspp_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data_v1_7 pgc_lm_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_igc_lut_data_v1_7 igc_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_hist_lut_data_v1_7 hist_lut_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_dither_data_v1_7 dither_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_gamut_data_v1_7 gamut_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_pcc_data_v1_7 pcc_v17_data[MDSS_BLOCK_DISP_NUM];
	struct mdp_pa_data_v1_7 pa_v17_data[MDSS_BLOCK_DISP_NUM];
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
	struct mdp_pa_v2_cfg_data pa_v2_disp_cfg[MDSS_BLOCK_DISP_NUM];
	u32 six_zone_lut_curve_p0[MDSS_BLOCK_DISP_NUM][MDP_SIX_ZONE_LUT_SIZE];
	u32 six_zone_lut_curve_p1[MDSS_BLOCK_DISP_NUM][MDP_SIX_ZONE_LUT_SIZE];
	struct mdp_pcc_cfg_data pcc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_igc_lut_data igc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data argc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_pgc_lut_data pgc_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_hist_lut_data enhist_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_dither_cfg_data dither_disp_cfg[MDSS_BLOCK_DISP_NUM];
	struct mdp_gamut_cfg_data gamut_disp_cfg[MDSS_BLOCK_DISP_NUM];
	uint16_t gamut_tbl[MDSS_BLOCK_DISP_NUM][GAMUT_TOTAL_TABLE_SIZE];
	u32 hist_data[MDSS_BLOCK_DISP_NUM][HIST_V_SIZE];
	struct pp_sts_type pp_disp_sts[MDSS_BLOCK_DISP_NUM];
	/* physical info */
	struct pp_hist_col_info *dspp_hist;
	/*
	 * The pp_data_res will be a pointer to newer MDP revisions of the
	 * pp_res, which will hold the cfg_payloads of each feature in a single
	 * struct.
	 */
	void *pp_data_res;
};

#if defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_ARCH_MSM8937)
void *pp_get_driver_ops(struct mdp_pp_driver_ops *ops);
#else
static inline void *pp_get_driver_ops(struct mdp_pp_driver_ops *ops)
{
	memset(ops, 0, sizeof(struct mdp_pp_driver_ops));
	return NULL;
}
#endif

static inline void pp_sts_set_split_bits(u32 *sts, u32 bits)
{
	u32 tmp = *sts;
	tmp &= ~MDSS_PP_SPLIT_MASK;
	tmp |= bits & MDSS_PP_SPLIT_MASK;
	*sts = tmp;
}

static inline bool pp_sts_is_enabled(u32 sts, int side)
{
	bool ret = false;
	/*
	 * If there are no sides, or if there are no split mode bits set, the
	 * side can't be disabled via split mode.
	 *
	 * Otherwise, if the side being checked opposes the split mode
	 * configuration, the side is disabled.
	 */
	if ((side == MDSS_SIDE_NONE) || !(sts & MDSS_PP_SPLIT_MASK))
		ret = true;
	else if ((sts & MDSS_PP_SPLIT_RIGHT_ONLY) && (side == MDSS_SIDE_RIGHT))
		ret = true;
	else if ((sts & MDSS_PP_SPLIT_LEFT_ONLY) && (side == MDSS_SIDE_LEFT))
		ret = true;

	return ret && (sts & PP_STS_ENABLE);
}

/* Debug related functions */
void pp_print_lut(void *data, int size, char *tab, uint32_t type);
void pp_print_uint16_lut(uint16_t *data, int size, char *tab);
void pp_print_pcc_coeff(struct mdp_pcc_coeff *pcc_coeff, int tab_depth);
void pp_print_pcc_cfg_data(struct mdp_pcc_cfg_data *pcc_data, int tab_depth);
void pp_print_csc_cfg(struct mdp_csc_cfg *data, int tab_depth);
void pp_print_csc_cfg_data(struct mdp_csc_cfg_data *data, int tab_depth);
void pp_print_igc_lut_data(struct mdp_igc_lut_data *data, int tab_depth);
void pp_print_ar_gc_lut_data(struct mdp_ar_gc_lut_data *data, int tab_depth);
void pp_print_pgc_lut_data(struct mdp_pgc_lut_data *data, int tab_depth);
void pp_print_hist_lut_data(struct mdp_hist_lut_data *data, int tab_depth);
void pp_print_lut_cfg_data(struct mdp_lut_cfg_data *data, int tab_depth);
void pp_print_qseed_cfg(struct mdp_qseed_cfg *data, int tab_depth);
void pp_print_qseed_cfg_data(struct mdp_qseed_cfg_data *data, int tab_depth);
void pp_print_pa_cfg(struct mdp_pa_cfg *data, int tab_depth);
void pp_print_pa_cfg_data(struct mdp_pa_cfg_data *data, int tab_depth);
void pp_print_mem_col_cfg(struct mdp_pa_mem_col_cfg *data, int tab_depth);
void pp_print_pa_v2_data(struct mdp_pa_v2_data *data, int tab_depth);
void pp_print_pa_v2_cfg_data(struct mdp_pa_v2_cfg_data *data, int tab_depth);
void pp_print_dither_cfg_data(struct mdp_dither_cfg_data *data, int tab_depth);
void pp_print_gamut_cfg_data(struct mdp_gamut_cfg_data *data, int tab_depth);
void pp_print_ad_init(struct mdss_ad_init *data, int tab_depth);
void pp_print_ad_cfg(struct mdss_ad_cfg *data, int tab_depth);
void pp_print_ad_init_cfg(struct mdss_ad_init_cfg *data, int tab_depth);
void pp_print_ad_input(struct mdss_ad_input *data, int tab_depth);
void pp_print_histogram_cfg(struct mdp_histogram_cfg *data, int tab_depth);
void pp_print_sharp_cfg(struct mdp_sharp_cfg *data, int tab_depth);
void pp_print_calib_config_data(struct mdp_calib_config_data *data,
				int tab_depth);
void pp_print_calib_config_buffer(struct mdp_calib_config_buffer *data,
				int tab_depth);
void pp_print_calib_dcm_state(struct mdp_calib_dcm_state *data, int tab_depth);
void pp_print_mdss_calib_cfg(struct mdss_calib_cfg *data, int tab_depth);

#endif
