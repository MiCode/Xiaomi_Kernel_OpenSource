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

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"

#define MAX_TAB_BUFFER_SIZE	12
#define MAX_LINE_BUFFER_SIZE 256

static inline void tab_prefix(char *tab_str, int n)
{
	while ((n)--)
		strlcat((tab_str), "\t", MAX_TAB_BUFFER_SIZE);
}

enum {
	UINT32,
	UINT16,
};

void pp_print_lut(void *data, int size, char *tab, uint32_t type)
{
	char buf[MAX_LINE_BUFFER_SIZE];
	int lines = size / 16;
	int last_start = lines * 16;
	int i, j;
	uint32_t read = 0;

	if (!data || !tab)
		return;

	buf[0] = '\0';
	for (i = 0; i < lines; i++) {
		buf[0] = '\0';
		read += snprintf(buf, MAX_LINE_BUFFER_SIZE - read,
			"%s", tab);
		for (j = 0; j < 16; j++) {
			if (type == UINT32)
				read += snprintf(buf + read,
					MAX_LINE_BUFFER_SIZE - read, "%04x ",
					((uint32_t *)data)[i*16+j]);
			else if (type == UINT16)
				read += snprintf(buf + read,
					MAX_LINE_BUFFER_SIZE - read, "%02x ",
					((uint16_t *)data)[i*16+j]);
		}
		snprintf(buf + read, MAX_LINE_BUFFER_SIZE - read, "\n");

		pr_debug("%s", buf);
		memset(buf, 0, sizeof(char) * MAX_LINE_BUFFER_SIZE);
		read = 0;
	}

	lines = size % 16;
	read += snprintf(buf, MAX_LINE_BUFFER_SIZE - read, "%s", tab);
	for (i = 0; i < lines; i++) {
		if (type == UINT32)
			read += snprintf(buf + read,
					MAX_LINE_BUFFER_SIZE - read, "%04x ",
					((uint32_t *)data)[last_start+i]);
		else if (type == UINT16)
			read += snprintf(buf + read,
					MAX_LINE_BUFFER_SIZE - read, "%02x ",
					((uint16_t *)data)[last_start+i]);
	}
	snprintf(buf + read, MAX_LINE_BUFFER_SIZE - read, "\n");
	pr_debug("%s", buf);
}

void pp_print_pcc_coeff(struct mdp_pcc_coeff *pcc_coeff, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!pcc_coeff || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pcc_coeff:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sc: %x\n"
		"%sr: %x\n%sg: %x\n%sb: %x\n"
		"%srr: %x\n%sgg: %x\n%sbb: %x\n"
		"%srg: %x\n%sgb: %x\n%srb: %x\n"
		"%srgb_0: %x\n%srgb_1: %x\n",
		tab, pcc_coeff->c,
		tab, pcc_coeff->r,
		tab, pcc_coeff->g,
		tab, pcc_coeff->b,
		tab, pcc_coeff->rr,
		tab, pcc_coeff->gg,
		tab, pcc_coeff->bb,
		tab, pcc_coeff->rg,
		tab, pcc_coeff->gb,
		tab, pcc_coeff->rb,
		tab, pcc_coeff->rgb_0,
		tab, pcc_coeff->rgb_1);
}

void pp_print_pcc_cfg_data(struct mdp_pcc_cfg_data *pcc_data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!pcc_data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pcc_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n%sops: %x\n",
		tab, pcc_data->block,
		tab, pcc_data->ops);

	pp_print_pcc_coeff(&pcc_data->r, tab_depth + 1);
	pp_print_pcc_coeff(&pcc_data->g, tab_depth + 1);
	pp_print_pcc_coeff(&pcc_data->b, tab_depth + 1);
}

void pp_print_csc_cfg(struct mdp_csc_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_csc_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sflags: %x\n",
		tab, data->flags);

	pr_debug("%scsc_mv[]:\n", tab);
	pp_print_lut(&data->csc_mv[0], 9, tab, UINT32);
	pr_debug("%scsc_pre_bv[]:\n", tab);
	pp_print_lut(&data->csc_pre_bv[0], 3, tab, UINT32);
	pr_debug("%scsc_post_bv[]:\n", tab);
	pp_print_lut(&data->csc_post_bv[0], 3, tab, UINT32);
	pr_debug("%scsc_pre_lv[]:\n", tab);
	pp_print_lut(&data->csc_pre_lv[0], 6, tab, UINT32);
	pr_debug("%scsc_post_lv[]:\n", tab);
	pp_print_lut(&data->csc_post_lv[0], 6, tab, UINT32);
}

void pp_print_csc_cfg_data(struct mdp_csc_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_csc_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n",
		tab, data->block);

	pp_print_csc_cfg(&data->csc_data, tab_depth + 1);
}

void pp_print_igc_lut_data(struct mdp_igc_lut_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_igc_lut_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n"
		"%slen: %x\n"
		"%sops: %x\n",
		tab, data->block,
		tab, data->len,
		tab, data->ops);

	pr_debug("%sc0_c1_data[]:\n", tab);
	pp_print_lut(&data->c0_c1_data[0], data->len, tab, UINT32);
	pr_debug("%sc2_data[]:\n", tab);
	pp_print_lut(&data->c2_data[0], data->len, tab, UINT32);
}

void pp_print_ar_gc_lut_data(struct mdp_ar_gc_lut_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_ar_gc_lut_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sx_start: %x\n"
		"%sslope: %x\n"
		"%soffset: %x\n",
		tab, data->x_start,
		tab, data->slope,
		tab, data->offset);
}

void pp_print_pgc_lut_data(struct mdp_pgc_lut_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;
	int i;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pgc_lut_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n"
		"%sflags: %x\n"
		"%snum_r_stages: %x\n"
		"%snum_g_stages: %x\n"
		"%snum_b_stages: %x\n",
		tab, data->block,
		tab, data->flags,
		tab, data->num_r_stages,
		tab, data->num_g_stages,
		tab, data->num_b_stages);

	for (i = 0; i < data->num_r_stages; i++) {
		pr_debug("%sr_data[%d]\n", tab, i);
		pp_print_ar_gc_lut_data(&data->r_data[i], tab_depth + 1);
	}
	for (i = 0; i < data->num_g_stages; i++) {
		pr_debug("%sg_data[%d]\n", tab, i);
		pp_print_ar_gc_lut_data(&data->g_data[i], tab_depth + 1);
	}
	for (i = 0; i < data->num_b_stages; i++) {
		pr_debug("%sb_data[%d]\n", tab, i);
		pp_print_ar_gc_lut_data(&data->b_data[i], tab_depth + 1);
	}
}

void pp_print_hist_lut_data(struct mdp_hist_lut_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_hist_lut_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n"
		"%sops: %x\n"
		"%slen: %x\n",
		tab, data->block,
		tab, data->ops,
		tab, data->len);

	pr_debug("%sdata[]:\n", tab);
	pp_print_lut(&data->data[0], data->len, tab, UINT32);
}

void pp_print_lut_cfg_data(struct mdp_lut_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_lut_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%slut_type: %x\n",
		tab, data->lut_type);

	switch (data->lut_type) {
	case mdp_lut_igc:
		pp_print_igc_lut_data(&data->data.igc_lut_data, tab_depth + 1);
		break;
	case mdp_lut_pgc:
		pp_print_pgc_lut_data(&data->data.pgc_lut_data, tab_depth + 1);
		break;
	case mdp_lut_hist:
		pp_print_hist_lut_data(&data->data.hist_lut_data,
				tab_depth + 1);
		break;
	default:
		break;
	}
}

void pp_print_qseed_cfg(struct mdp_qseed_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_qseed_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%stable_num: %x\n"
		"%sops: %x\n"
		"%slen: %x\n",
		tab, data->table_num,
		tab, data->ops,
		tab, data->len);

	pr_debug("%sdata[]:\n", tab);
	pp_print_lut(&data->data[0], data->len, tab, UINT32);
}

void pp_print_qseed_cfg_data(struct mdp_qseed_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[tab_depth] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_qseed_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n",
		tab, data->block);

	pp_print_qseed_cfg(&data->qseed_data, tab_depth + 1);
}

void pp_print_pa_cfg(struct mdp_pa_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pa_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sflags: %x\n"
		"%shue_adj: %x\n"
		"%ssat_adj: %x\n"
		"%sval_adj: %x\n"
		"%scont_adj: %x\n",
		tab, data->flags,
		tab, data->hue_adj,
		tab, data->sat_adj,
		tab, data->val_adj,
		tab, data->cont_adj);
}

void pp_print_pa_cfg_data(struct mdp_pa_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pa_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n",
		tab, data->block);

	pp_print_pa_cfg(&data->pa_data, tab_depth + 1);
}

void pp_print_mem_col_cfg(struct mdp_pa_mem_col_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pa_mem_col_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%scolor_adjust_p0: %x\n"
		"%scolor_adjust_p1: %x\n"
		"%shue_region: %x\n"
		"%ssat_region: %x\n"
		"%sval_region: %x\n",
		tab, data->color_adjust_p0,
		tab, data->color_adjust_p1,
		tab, data->hue_region,
		tab, data->sat_region,
		tab, data->val_region);
}

void pp_print_pa_v2_data(struct mdp_pa_v2_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pa_v2_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sflags: %x\n"
		"%sglobal_hue_adj: %x\n"
		"%sglobal_sat_adj: %x\n"
		"%sglobal_val_adj: %x\n"
		"%sglobal_cont_adj: %x\n",
		tab, data->flags,
		tab, data->global_hue_adj,
		tab, data->global_sat_adj,
		tab, data->global_val_adj,
		tab, data->global_cont_adj);

	pp_print_mem_col_cfg(&data->skin_cfg, tab_depth + 1);
	pp_print_mem_col_cfg(&data->sky_cfg, tab_depth + 1);
	pp_print_mem_col_cfg(&data->fol_cfg, tab_depth + 1);

	pr_debug("%ssix_zone_len: %x\n"
		"%ssix_zone_thresh: %x\n",
		tab, data->six_zone_len,
		tab, data->six_zone_thresh);

	pr_debug("%ssix_zone_curve_p0[]:\n", tab);
	pp_print_lut(&data->six_zone_curve_p0[0], data->six_zone_len, tab,
			UINT32);
	pr_debug("%ssix_zone_curve_p1[]:\n", tab);
	pp_print_lut(&data->six_zone_curve_p1[0], data->six_zone_len, tab,
			UINT32);
}

void pp_print_pa_v2_cfg_data(struct mdp_pa_v2_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_pa_v2_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n",
		tab, data->block);

	pp_print_pa_v2_data(&data->pa_v2_data, tab_depth + 1);
}

void pp_print_dither_cfg_data(struct mdp_dither_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_dither_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n"
		"%sflags: %x\n"
		"%sg_y_depth: %x\n"
		"%sr_cr_depth: %x\n"
		"%sb_cb_depth: %x\n",
		tab, data->block,
		tab, data->flags,
		tab, data->g_y_depth,
		tab, data->r_cr_depth,
		tab, data->b_cb_depth);
}

void pp_print_gamut_cfg_data(struct mdp_gamut_cfg_data *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;
	int i;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_gamut_cfg_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sblock: %x\n"
		"%sflags: %x\n"
		"%sgamut_first: %x\n",
		tab, data->block,
		tab, data->flags,
		tab, data->gamut_first);

	pr_debug("%stbl_size[]:\n", tab);
	pp_print_lut(&data->tbl_size[0], MDP_GAMUT_TABLE_NUM, tab, UINT32);

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		pr_debug("%sr_tbl[%d]:\n", tab, i);
		pp_print_lut(&data->r_tbl[i][0], data->tbl_size[i], tab,
				UINT16);
	}

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		pr_debug("%sg_tbl[%d]:\n", tab, i);
		pp_print_lut(&data->g_tbl[i][0], data->tbl_size[i], tab,
				UINT16);
	}

	for (i = 0; i < MDP_GAMUT_TABLE_NUM; i++) {
		pr_debug("%sb_tbl[%d]:\n", tab, i);
		pp_print_lut(&data->b_tbl[i][0], data->tbl_size[i], tab,
				UINT16);
	}
}

void pp_print_ad_init(struct mdss_ad_init *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdss_ad_init:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sasym_lut[]:\n", tab);
	pp_print_lut(&data->asym_lut[0], 33, tab, UINT32);

	pr_debug("%scolor_corr_lut[]:\n", tab);
	pp_print_lut(&data->color_corr_lut[0], 33, tab, UINT32);

	pr_debug("%si_control[]:\n%s%x %x\n"
		"%sblack_lvl: %x\n"
		"%swhite_lvl: %x\n"
		"%svar: %x\n"
		"%slimit_ampl: %x\n"
		"%si_dither: %x\n"
		"%sslope_max: %x\n"
		"%sslope_min: %x\n"
		"%sdither_ctl: %x\n"
		"%sformat: %x\n"
		"%sauto_size: %x\n"
		"%sframe_w: %x\n"
		"%sframe_h: %x\n"
		"%slogo_v: %x\n"
		"%slogo_h: %x\n"
		"%sbl_lin_len: %x\n",
		tab, tab, data->i_control[0], data->i_control[1],
		tab, data->black_lvl,
		tab, data->white_lvl,
		tab, data->var,
		tab, data->limit_ampl,
		tab, data->i_dither,
		tab, data->slope_max,
		tab, data->slope_min,
		tab, data->dither_ctl,
		tab, data->format,
		tab, data->auto_size,
		tab, data->frame_w,
		tab, data->frame_h,
		tab, data->logo_v,
		tab, data->logo_h,
		tab, data->bl_lin_len);

	pr_debug("%sbl_lin[]:\n", tab);
	pp_print_lut(&data->bl_lin[0], data->bl_lin_len, tab, UINT32);

	pr_debug("%sbl_lin_inv[]:\n", tab);
	pp_print_lut(&data->bl_lin_inv[0], data->bl_lin_len, tab, UINT32);
}

void pp_print_ad_cfg(struct mdss_ad_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdss_ad_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%smode: %x\n",
		tab, data->mode);

	pr_debug("%sal_calib_lut[]:\n", tab);
	pp_print_lut(&data->al_calib_lut[0], 33, tab, UINT32);

	pr_debug("%sbacklight_min: %x\n"
		"%sbacklight_max: %x\n"
		"%sbacklight_scale: %x\n"
		"%samb_light_min: %x\n",
		tab, data->backlight_min,
		tab, data->backlight_max,
		tab, data->backlight_scale,
		tab, data->amb_light_min);

	pp_print_lut(&data->filter[0], 2, tab, UINT16);
	pp_print_lut(&data->calib[0], 4, tab, UINT16);

	pr_debug("%sstrength_limit: %x\n"
		"%st_filter_recursion: %x\n"
		"%sstab_itr: %x\n"
		"%sbl_ctrl_mode: %x\n",
		tab, data->strength_limit,
		tab, data->t_filter_recursion,
		tab, data->stab_itr,
		tab, data->bl_ctrl_mode);
}

void pp_print_ad_init_cfg(struct mdss_ad_init_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdss_ad_init_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n",
		tab, data->ops);

	if (data->ops & MDP_PP_AD_INIT)
		pp_print_ad_init(&data->params.init, tab_depth + 1);
	else if (data->ops & MDP_PP_AD_CFG)
		pp_print_ad_cfg(&data->params.cfg, tab_depth + 1);
}

void pp_print_ad_input(struct mdss_ad_input *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdss_ad_input:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%smode: %x\n",
		tab, data->mode);

	switch (data->mode) {
	case MDSS_AD_MODE_AUTO_BL:
	case MDSS_AD_MODE_AUTO_STR:
		pr_debug("%samb_light: %x\n",
			tab, data->in.amb_light);
		break;
	case MDSS_AD_MODE_TARG_STR:
	case MDSS_AD_MODE_MAN_STR:
		pr_debug("%sstrength: %x\n",
			tab, data->in.strength);
		break;
	case MDSS_AD_MODE_CALIB:
		pr_debug("%scalib_bl: %x\n",
			tab, data->in.calib_bl);
		break;
	default:
		break;
	}

	pr_debug("%soutput: %x\n",
		tab, data->output);
}

void pp_print_histogram_cfg(struct mdp_histogram_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_histogram_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n"
		"%sblock: %x\n"
		"%sframe_cnt: %x\n"
		"%sbit_mask: %x\n"
		"%snum_bins: %x\n",
		tab, data->ops,
		tab, data->block,
		tab, data->frame_cnt,
		tab, data->bit_mask,
		tab, data->num_bins);
}

void pp_print_sharp_cfg(struct mdp_sharp_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_sharp_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sflags: %x\n"
		"%sstrength: %x\n"
		"%sedge_thr: %x\n"
		"%ssmooth_thr: %x\n"
		"%snoise_thr: %x\n",
		tab, data->flags,
		tab, data->strength,
		tab, data->edge_thr,
		tab, data->smooth_thr,
		tab, data->noise_thr);
}

void pp_print_calib_config_data(struct mdp_calib_config_data *data,
				int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_calib_config_data:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n"
		"%saddr: %x\n"
		"%sdata: %x\n",
		tab, data->ops,
		tab, data->addr,
		tab, data->data);
}

void pp_print_calib_config_buffer(struct mdp_calib_config_buffer *data,
				int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_calib_config_buffer:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n"
		"%ssize: %x\n",
		tab, data->ops,
		tab, data->size);

	pr_debug("%sbuffer[]:\n", tab);
	pp_print_lut(&data->buffer[0], data->size, tab, UINT32);
}

void pp_print_calib_dcm_state(struct mdp_calib_dcm_state *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdp_calib_dcm_state:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n"
		"%sdcm_state: %x\n",
		tab, data->ops,
		tab, data->dcm_state);
}

void pp_print_mdss_calib_cfg(struct mdss_calib_cfg *data, int tab_depth)
{
	char tab[MAX_TAB_BUFFER_SIZE];
	int tmp = 1;

	if (!data || tab_depth < 0)
		return;

	tab[0] = '\0';
	tab_prefix(tab, tab_depth);
	pr_debug("%smdss_calib_cfg:\n", tab);
	tab_prefix(tab, tmp);

	pr_debug("%sops: %x\n"
		"%scalib_mask: %x\n",
		tab, data->ops,
		tab, data->calib_mask);
}
