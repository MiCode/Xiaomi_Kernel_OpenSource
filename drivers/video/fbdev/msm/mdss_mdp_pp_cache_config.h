/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2016, 2018, 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef MDSS_MDP_CACHE_CONFIG_H
#define MDSS_MDP_CACHE_CONFIG_H
#include "mdss_mdp_pp.h"

struct mdp_pp_cache_res {
	enum pp_config_block block;
	struct mdss_pp_res_type *mdss_pp_res;
	struct mdss_mdp_pipe *pipe_res;
};

int pp_hist_lut_cache_params(struct mdp_hist_lut_data *config,
			  struct mdp_pp_cache_res *res_cache);

int pp_dither_cache_params(struct mdp_dither_cfg_data *config,
			  struct mdss_pp_res_type *mdss_pp_res,
			  int copy_from_kernel);

int pp_gamut_cache_params(struct mdp_gamut_cfg_data *config,
			  struct mdss_pp_res_type *mdss_pp_res);
int pp_pcc_cache_params(struct mdp_pcc_cfg_data *config,
			  struct mdp_pp_cache_res *res_cache);
int pp_pa_cache_params(struct mdp_pa_v2_cfg_data *config,
			  struct mdp_pp_cache_res *res_cache);

int pp_igc_lut_cache_params(struct mdp_igc_lut_data *config,
			    struct mdp_pp_cache_res *res_cache,
			    u32 copy_from_kernel);

int pp_pgc_lut_cache_params(struct mdp_pgc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    int location);

int pp_copy_layer_igc_payload(struct mdp_overlay_pp_params *pp_info);
int pp_copy_layer_hist_lut_payload(struct mdp_overlay_pp_params *pp_info);
int pp_copy_layer_pa_payload(struct mdp_overlay_pp_params *pp_info);
int pp_copy_layer_pcc_payload(struct mdp_overlay_pp_params *pp_info);
int pp_pa_dither_cache_params(struct mdp_dither_cfg_data *config,
			 struct mdp_pp_cache_res *res_cache);

#endif
