/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 XiaoMi, Inc.
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
void mdss_pp_input_key_event(struct msm_fb_data_type *mfd, u32 key_code);
#endif
