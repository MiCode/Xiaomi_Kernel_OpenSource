/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"
#include "mdss_mdp_pp_cache_config.h"

#define IGC_C1_SHIFT 16
static u32 pp_igc_601[IGC_LUT_ENTRIES] = {
	0, 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 18, 20, 21, 23,
	25, 27, 29, 31, 33, 35, 37, 40, 42, 45, 48, 50, 53, 56, 59, 62,
	66, 69, 72, 76, 79, 83, 87, 91, 95, 99, 103, 107, 112, 116, 121,
	126, 131, 136, 141, 146, 151, 156, 162, 168, 173, 179, 185, 191,
	197, 204, 210, 216, 223, 230, 237, 244, 251, 258, 265, 273, 280,
	288, 296, 304, 312, 320, 329, 337, 346, 354, 363, 372, 381, 390,
	400, 409, 419, 428, 438, 448, 458, 469, 479, 490, 500, 511, 522,
	533, 544, 555, 567, 578, 590, 602, 614, 626, 639, 651, 664, 676,
	689, 702, 715, 728, 742, 755, 769, 783, 797, 811, 825, 840, 854,
	869, 884, 899, 914, 929, 945, 960, 976, 992, 1008, 1024, 1041,
	1057, 1074, 1091, 1108, 1125, 1142, 1159, 1177, 1195, 1213, 1231,
	1249, 1267, 1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420, 1440,
	1459, 1480, 1500, 1520, 1541, 1562, 1582, 1603, 1625, 1646, 1668,
	1689, 1711, 1733, 1755, 1778, 1800, 1823, 1846, 1869, 1892, 1916,
	1939, 1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133, 2159, 2184,
	2209, 2235, 2260, 2286, 2312, 2339, 2365, 2392, 2419, 2446, 2473,
	2500, 2527, 2555, 2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
	2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022, 3052, 3083, 3114,
	3146, 3177, 3209, 3240, 3272, 3304, 3337, 3369, 3402, 3435, 3468,
	3501, 3535, 3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809, 3844,
	3879, 3915, 3950, 3986, 4022, 4059, 4095,
};

static u32 pp_igc_709[IGC_LUT_ENTRIES] = {
	0, 4, 7, 11, 14, 18, 21, 25, 29, 32, 36, 39, 43, 46, 50, 54, 57,
	61, 64, 68, 71, 75, 78, 82, 86, 90, 94, 98, 102, 107, 111, 115,
	120, 125, 130, 134, 139, 145, 150, 155, 161, 166, 172, 177, 183,
	189, 195, 201, 208, 214, 220, 227, 234, 240, 247, 254, 261, 269,
	276, 283, 291, 298, 306, 314, 322, 330, 338, 347, 355, 364, 372,
	381, 390, 399, 408, 417, 426, 436, 445, 455, 465, 474, 484, 495,
	505, 515, 525, 536, 547, 558, 568, 579, 591, 602, 613, 625, 636,
	648, 660, 672, 684, 696, 708, 721, 733, 746, 759, 772, 785, 798,
	811, 825, 838, 852, 865, 879, 893, 907, 922, 936, 950, 965, 980,
	995, 1010, 1025, 1040, 1055, 1071, 1086, 1102, 1118, 1134, 1150,
	1166, 1183, 1199, 1216, 1232, 1249, 1266, 1283, 1300, 1318, 1335,
	1353, 1370, 1388, 1406, 1424, 1443, 1461, 1479, 1498, 1517, 1536,
	1555, 1574, 1593, 1612, 1632, 1652, 1671, 1691, 1711, 1731, 1752,
	1772, 1793, 1813, 1834, 1855, 1876, 1897, 1919, 1940, 1962, 1984,
	2005, 2027, 2050, 2072, 2094, 2117, 2139, 2162, 2185, 2208, 2231,
	2255, 2278, 2302, 2325, 2349, 2373, 2397, 2422, 2446, 2471, 2495,
	2520, 2545, 2570, 2595, 2621, 2646, 2672, 2697, 2723, 2749, 2775,
	2802, 2828, 2855, 2881, 2908, 2935, 2962, 2990, 3017, 3044, 3072,
	3100, 3128, 3156, 3184, 3212, 3241, 3270, 3298, 3327, 3356, 3385,
	3415, 3444, 3474, 3503, 3533, 3563, 3594, 3624, 3654, 3685, 3716,
	3746, 3777, 3808, 3840, 3871, 3903, 3934, 3966, 3998, 4030, 4063,
	4095,
};

static u32 pp_igc_srgb[IGC_LUT_ENTRIES] = {
	0, 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 18, 20, 21, 23,
	25, 27, 29, 31, 33, 35, 37, 40, 42, 45, 48, 50, 53, 56, 59, 62,
	66, 69, 72, 76, 79, 83, 87, 91, 95, 99, 103, 107, 112, 116, 121,
	126, 131, 136, 141, 146, 151, 156, 162, 168, 173, 179, 185, 191,
	197, 204, 210, 216, 223, 230, 237, 244, 251, 258, 265, 273, 280,
	288, 296, 304, 312, 320, 329, 337, 346, 354, 363, 372, 381, 390,
	400, 409, 419, 428, 438, 448, 458, 469, 479, 490, 500, 511, 522,
	533, 544, 555, 567, 578, 590, 602, 614, 626, 639, 651, 664, 676,
	689, 702, 715, 728, 742, 755, 769, 783, 797, 811, 825, 840, 854,
	869, 884, 899, 914, 929, 945, 960, 976, 992, 1008, 1024, 1041,
	1057, 1074, 1091, 1108, 1125, 1142, 1159, 1177, 1195, 1213, 1231, 1249,
	1267, 1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420, 1440, 1459, 1480,
	1500, 1520, 1541, 1562, 1582, 1603, 1625, 1646, 1668, 1689, 1711, 1733,
	1755, 1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939, 1963, 1987, 2011,
	2035, 2059, 2084, 2109, 2133, 2159, 2184, 2209, 2235, 2260, 2286, 2312,
	2339, 2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555, 2583, 2611, 2639,
	2668, 2696, 2725, 2754, 2783, 2812, 2841, 2871, 2901, 2931, 2961, 2991,
	3022, 3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272, 3304, 3337, 3369,
	3402, 3435, 3468, 3501, 3535, 3568, 3602, 3636, 3670, 3705, 3739, 3774,
	3809, 3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095
};

static int pp_hist_lut_cache_params_v1_7(struct mdp_hist_lut_data *config,
				      struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	struct mdp_hist_lut_data_v1_7 *v17_cache_data = NULL, v17_usr_config;
	int ret = 0;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_v1_7;
	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->enhist_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->hist_lut_v17_data[disp_num];
		mdss_pp_res->enhist_disp_cfg[disp_num].cfg_payload =
		(void *) v17_cache_data;

		if (copy_from_user(&v17_usr_config, config->cfg_payload,
				   sizeof(v17_usr_config))) {
			pr_err("failed to copy v17 hist_lut\n");
			ret = -EFAULT;
			return ret;
		}
		if ((config->ops & MDP_PP_OPS_DISABLE)) {
			pr_debug("disable hist_lut\n");
			ret = 0;
			return ret;
		}
		memcpy(v17_cache_data, &v17_usr_config, sizeof(v17_usr_config));
		if (v17_usr_config.len != ENHIST_LUT_ENTRIES) {
			pr_err("Invalid table size %d exp %d\n",
				v17_usr_config.len, ENHIST_LUT_ENTRIES);
			ret = -EINVAL;
			return ret;
		}
		v17_cache_data->data = &res_cache->hist_lut[disp_num][0];
		if (copy_from_user(v17_cache_data->data, v17_usr_config.data,
				   v17_usr_config.len * sizeof(u32))) {
			pr_err("failed to copy v17 hist_lut->data\n");
			ret = -EFAULT;
			return ret;
		}
	}
	return ret;
}

static int pp_hist_lut_cache_params_pipe_v1_7(struct mdp_hist_lut_data *config,
			struct mdss_mdp_pipe *pipe)
{
	struct mdp_hist_lut_data_v1_7 *hist_lut_cache_data;
	struct mdp_hist_lut_data_v1_7 hist_lut_usr_config;
	int ret = 0;

	if (!config || !pipe) {
		pr_err("Invalid param config %pK pipe %pK\n",
			config, pipe);
		return -EINVAL;
	}

	if (config->ops & MDP_PP_OPS_DISABLE) {
		pr_debug("Disable Hist LUT on pipe %d\n", pipe->num);
		goto hist_lut_cache_pipe_exit;
	}

	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("Read op is not supported\n");
		return -EINVAL;
	}

	if (!config->cfg_payload) {
		pr_err("Hist LUT config payload invalid\n");
		return -EINVAL;
	}

	memcpy(&hist_lut_usr_config, config->cfg_payload,
		sizeof(struct mdp_hist_lut_data_v1_7));

	hist_lut_cache_data = pipe->pp_res.hist_lut_cfg_payload;
	if (!hist_lut_cache_data) {
		hist_lut_cache_data = kzalloc(
				sizeof(struct mdp_hist_lut_data_v1_7),
				GFP_KERNEL);
		if (!hist_lut_cache_data) {
			pr_err("failed to allocate cache_data\n");
			ret = -ENOMEM;
			goto hist_lut_cache_pipe_exit;
		} else
			pipe->pp_res.hist_lut_cfg_payload = hist_lut_cache_data;
	}

	*hist_lut_cache_data = hist_lut_usr_config;

	if (hist_lut_cache_data->len != ENHIST_LUT_ENTRIES) {
		pr_err("Invalid Hist LUT length %d\n",
			hist_lut_cache_data->len);
		ret = -EINVAL;
		goto hist_lut_cache_pipe_exit;
	}

	if (copy_from_user(pipe->pp_res.hist_lut,
			   hist_lut_usr_config.data,
			   sizeof(uint32_t) * hist_lut_cache_data->len)) {
		pr_err("Failed to copy usr Hist LUT data\n");
		ret = -EFAULT;
		goto hist_lut_cache_pipe_exit;
	}

	hist_lut_cache_data->data = pipe->pp_res.hist_lut;

hist_lut_cache_pipe_exit:
	if (ret || (config->ops & MDP_PP_OPS_DISABLE)) {
		kfree(pipe->pp_res.hist_lut_cfg_payload);
		pipe->pp_res.hist_lut_cfg_payload = NULL;
	}
	pipe->pp_cfg.hist_lut_cfg.cfg_payload =
			pipe->pp_res.hist_lut_cfg_payload;
	return ret;
}

int pp_hist_lut_cache_params(struct mdp_hist_lut_data *config,
			struct mdp_pp_cache_res *res_cache)
{
	int ret = 0;

	if (!config || !res_cache) {
		pr_err("invalid param config %pK res_cache %pK\n",
			config, res_cache);
		return -EINVAL;
	}
	if (res_cache->block != SSPP_VIG && res_cache->block != DSPP) {
		pr_err("invalid block for Hist LUT %d\n", res_cache->block);
		return -EINVAL;
	}
	if (!res_cache->mdss_pp_res && !res_cache->pipe_res) {
		pr_err("NULL payload for block %d mdss_pp_res %pK pipe_res %pK\n",
			res_cache->block, res_cache->mdss_pp_res,
			res_cache->pipe_res);
		return -EINVAL;
	}

	switch (config->version) {
	case mdp_hist_lut_v1_7:
		if (res_cache->block == DSPP) {
			ret = pp_hist_lut_cache_params_v1_7(config,
					res_cache->mdss_pp_res);
			if (ret)
				pr_err("failed to cache Hist LUT params for DSPP ret %d\n",
					ret);
		} else {
			ret = pp_hist_lut_cache_params_pipe_v1_7(config,
					res_cache->pipe_res);
			if (ret)
				pr_err("failed to cache Hist LUT params for SSPP ret %d\n",
					ret);
		}
		break;
	default:
		pr_err("unsupported hist_lut version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int pp_dither_cache_params_v1_7(struct mdp_dither_cfg_data *config,
			  struct mdss_pp_res_type *mdss_pp_res,
			  int copy_from_kernel)
{
	u32 disp_num;
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	struct mdp_dither_data_v1_7 *v17_cache_data = NULL, v17_usr_config;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_v1_7;

	if ((config->flags & MDSS_PP_SPLIT_MASK) == MDSS_PP_SPLIT_MASK) {
		pr_warn("Can't set both split bits\n");
		return -EINVAL;
	}

	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -ENOTSUPP;
	}

	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
	mdss_pp_res->dither_disp_cfg[disp_num] = *config;

	if (config->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("disable dither\n");
		ret = 0;
		goto dither_config_exit;
	}

	if (!(config->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("op for dither %d\n", config->flags);
		goto dither_config_exit;
	}

	v17_cache_data = &res_cache->dither_v17_data[disp_num];
	mdss_pp_res->dither_disp_cfg[disp_num].cfg_payload =
		(void *)v17_cache_data;
	if (copy_from_kernel) {
		memcpy(v17_cache_data, config->cfg_payload,
				sizeof(struct mdp_dither_data_v1_7));
	} else {
		if (copy_from_user(&v17_usr_config, config->cfg_payload,
				sizeof(v17_usr_config))) {
			pr_err("failed to copy v17 dither\n");
			ret = -EFAULT;
			goto dither_config_exit;
		}
		memcpy(v17_cache_data, &v17_usr_config, sizeof(v17_usr_config));
	}
	if (v17_cache_data->len &&
		v17_cache_data->len != MDP_DITHER_DATA_V1_7_SZ) {
		pr_err("invalid dither len %d expected %d\n",
			   v17_cache_data->len, MDP_DITHER_DATA_V1_7_SZ);
		ret = -EINVAL;
	}

dither_config_exit:
	return ret;
}

int pp_dither_cache_params(struct mdp_dither_cfg_data *config,
	struct mdss_pp_res_type *mdss_pp_res,
	int copy_from_kernel)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_dither_v1_7:
		ret = pp_dither_cache_params_v1_7(config, mdss_pp_res,
				copy_from_kernel);
		break;
	default:
		pr_err("unsupported dither version %d\n",
			config->version);
		break;
	}
	return ret;
}


static int pp_gamut_cache_params_v1_7(struct mdp_gamut_cfg_data *config,
				      struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num, tbl_sz;
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_gamut_data_v1_7 *v17_cache_data, v17_usr_config;
	u32 gamut_size = 0, scal_coff_size = 0, sz = 0, index = 0;
	u32 *tbl_gamut = NULL;
	int ret = 0, i = 0;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_v1_7;
	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	}

	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;

	/* Copy top level gamut cfg struct into PP res cache */
	memcpy(&mdss_pp_res->gamut_disp_cfg[disp_num], config,
			sizeof(struct mdp_gamut_cfg_data));

	v17_cache_data = &res_cache->gamut_v17_data[disp_num];
	mdss_pp_res->gamut_disp_cfg[disp_num].cfg_payload =
		(void *) v17_cache_data;
	tbl_gamut = v17_cache_data->c0_data[0];

	if ((config->flags & MDP_PP_OPS_DISABLE)) {
		pr_debug("disable gamut\n");
		ret = 0;
		goto gamut_config_exit;
	}

	if (copy_from_user(&v17_usr_config, config->cfg_payload,
			   sizeof(v17_usr_config))) {
		pr_err("failed to copy v17 gamut\n");
		ret = -EFAULT;
		goto gamut_config_exit;
	}
	if (v17_usr_config.mode != mdp_gamut_coarse_mode &&
	   v17_usr_config.mode != mdp_gamut_fine_mode) {
		pr_err("invalid gamut mode %d\n", v17_usr_config.mode);
		return -EINVAL;
	}
	if (!(config->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("op for gamut %d\n", config->flags);
		goto gamut_config_exit;
	}
	tbl_sz = (v17_usr_config.mode == mdp_gamut_fine_mode) ?
		MDP_GAMUT_TABLE_V1_7_SZ :
		 MDP_GAMUT_TABLE_V1_7_COARSE_SZ;
	v17_cache_data->mode = v17_usr_config.mode;
	v17_cache_data->map_en = v17_usr_config.map_en;
	/* sanity check for sizes */
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		if (v17_usr_config.tbl_size[i] != tbl_sz) {
			pr_err("invalid tbl size %d exp %d tbl index %d mode %d\n",
			       v17_usr_config.tbl_size[i], tbl_sz, i,
			       v17_usr_config.mode);
			ret = -EINVAL;
			goto gamut_config_exit;
		}
		gamut_size += v17_usr_config.tbl_size[i];
		if (i >= MDP_GAMUT_SCALE_OFF_TABLE_NUM)
			continue;
		if (v17_usr_config.tbl_scale_off_sz[i] !=
		    MDP_GAMUT_SCALE_OFF_SZ) {
			pr_err("invalid scale size %d exp %d scale index %d mode %d\n",
			       v17_usr_config.tbl_scale_off_sz[i],
			       MDP_GAMUT_SCALE_OFF_SZ, i,
			       v17_usr_config.mode);
			ret = -EINVAL;
			goto gamut_config_exit;
		}
		scal_coff_size += v17_usr_config.tbl_scale_off_sz[i];

	}
	/* gamut size should be accounted for c0, c1c2 table */
	sz = gamut_size * 2 + scal_coff_size;
	if (sz > GAMUT_TOTAL_TABLE_SIZE_V1_7) {
		pr_err("Invalid table size act %d max %d\n",
		      sz, GAMUT_TOTAL_TABLE_SIZE_V1_7);
		ret = -EINVAL;
		goto gamut_config_exit;
	}
	/* Allocate for fine mode other modes will fit */
	if (!tbl_gamut)
		tbl_gamut = vmalloc(GAMUT_TOTAL_TABLE_SIZE_V1_7 *
				    sizeof(u32));
	if (!tbl_gamut) {
		pr_err("failed to allocate buffer for gamut size %zd",
			(GAMUT_TOTAL_TABLE_SIZE_V1_7 * sizeof(u32)));
		ret = -ENOMEM;
		goto gamut_config_exit;
	}
	index = 0;
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		ret = copy_from_user(&tbl_gamut[index],
			v17_usr_config.c0_data[i],
			(sizeof(u32) * v17_usr_config.tbl_size[i]));
		if (ret) {
			pr_err("copying c0 table %d from userspace failed size %zd ret %d\n",
				i, (sizeof(u32) *
				v17_usr_config.tbl_size[i]), ret);
			ret = -EFAULT;
			goto gamut_memory_free_exit;
		}
		v17_cache_data->c0_data[i] = &tbl_gamut[index];
		v17_cache_data->tbl_size[i] =
			v17_usr_config.tbl_size[i];
		index += v17_usr_config.tbl_size[i];
		ret = copy_from_user(&tbl_gamut[index],
			v17_usr_config.c1_c2_data[i],
			(sizeof(u32) * v17_usr_config.tbl_size[i]));
		if (ret) {
			pr_err("copying c1_c2 table %d from userspace failed size %zd ret %d\n",
				i, (sizeof(u32) *
				v17_usr_config.tbl_size[i]), ret);
			ret = -EINVAL;
			goto gamut_memory_free_exit;
		}
		v17_cache_data->c1_c2_data[i] = &tbl_gamut[index];
		index += v17_usr_config.tbl_size[i];
	}
	for (i = 0; i < MDP_GAMUT_SCALE_OFF_TABLE_NUM; i++) {
		ret = copy_from_user(&tbl_gamut[index],
			v17_usr_config.scale_off_data[i],
			(sizeof(u32) *
			v17_usr_config.tbl_scale_off_sz[i]));
		if (ret) {
			pr_err("copying scale offset table %d from userspace failed size %zd ret %d\n",
				i, (sizeof(u32) *
				v17_usr_config.tbl_scale_off_sz[i]),
				ret);
			ret = -EFAULT;
			goto gamut_memory_free_exit;
		}
		v17_cache_data->tbl_scale_off_sz[i] =
			v17_usr_config.tbl_scale_off_sz[i];
		v17_cache_data->scale_off_data[i] = &tbl_gamut[index];
		index += v17_usr_config.tbl_scale_off_sz[i];
	}

gamut_config_exit:
	return ret;
gamut_memory_free_exit:
	vfree(tbl_gamut);
	for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
		v17_cache_data->c0_data[i] = NULL;
		v17_cache_data->c1_c2_data[i] = NULL;
		v17_cache_data->tbl_size[i] = 0;
		if (i < MDP_GAMUT_SCALE_OFF_TABLE_NUM) {
			v17_cache_data->scale_off_data[i] = NULL;
			v17_cache_data->tbl_scale_off_sz[i] = 0;
		}
	}
	return ret;
}

int pp_gamut_cache_params(struct mdp_gamut_cfg_data *config,
			  struct mdss_pp_res_type *mdss_pp_res)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_gamut_v1_7:
		ret = pp_gamut_cache_params_v1_7(config, mdss_pp_res);
		break;
	default:
		pr_err("unsupported gamut version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_pcc_cache_params_pipe_v1_7(struct mdp_pcc_cfg_data *config,
				      struct mdss_mdp_pipe *pipe)
{
	struct mdp_pcc_data_v1_7 *v17_cache_data = NULL, v17_usr_config;

	if (!pipe || !config) {
		pr_err("invalid params pipe %pK config %pK\n", pipe, config);
		return -EINVAL;
	}

	if (config->ops & MDP_PP_OPS_DISABLE) {
		pr_debug("disable ops set cleanup payload\n");
		goto cleanup;
	}

	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read ops not supported\n");
		return -EINVAL;
	}

	if (!config->cfg_payload) {
		pr_err("PCC config payload invalid\n");
		return -EINVAL;
	}

	memcpy(&v17_usr_config, config->cfg_payload,
			sizeof(v17_usr_config));

	if (!(config->ops & MDP_PP_OPS_WRITE)) {
		pr_debug("write ops not set value of flag is %d\n",
			config->ops);
		goto cleanup;
	}

	v17_cache_data = pipe->pp_res.pcc_cfg_payload;
	if (!v17_cache_data) {
		v17_cache_data = kzalloc(sizeof(struct mdp_pcc_data_v1_7),
						GFP_KERNEL);
		pipe->pp_res.pcc_cfg_payload = v17_cache_data;
	}
	if (!v17_cache_data) {
		pr_err("failed to allocate the pcc cache data\n");
		return -ENOMEM;
	}
	memcpy(v17_cache_data, &v17_usr_config, sizeof(v17_usr_config));
	pipe->pp_cfg.pcc_cfg_data.cfg_payload = v17_cache_data;
cleanup:
	if (config->ops & MDP_PP_OPS_DISABLE) {
		kfree(pipe->pp_res.pcc_cfg_payload);
		pipe->pp_res.pcc_cfg_payload = NULL;
		pipe->pp_cfg.pcc_cfg_data.cfg_payload = NULL;
	}
	return 0;
}

static int pp_pcc_cache_params_v1_7(struct mdp_pcc_cfg_data *config,
				      struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num;
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_pcc_data_v1_7 *v17_cache_data, v17_usr_config;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_v1_7;
	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->pcc_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->pcc_v17_data[disp_num];
		mdss_pp_res->pcc_disp_cfg[disp_num].cfg_payload =
			(void *) v17_cache_data;
		if (copy_from_user(&v17_usr_config, config->cfg_payload,
				   sizeof(v17_usr_config))) {
			pr_err("failed to copy v17 pcc\n");
			ret = -EFAULT;
			goto pcc_config_exit;
		}
		if ((config->ops & MDP_PP_OPS_DISABLE)) {
			pr_debug("disable pcc\n");
			ret = 0;
			goto pcc_config_exit;
		}
		if (!(config->ops & MDP_PP_OPS_WRITE)) {
			pr_debug("op for pcc %d\n", config->ops);
			goto pcc_config_exit;
		}
		memcpy(v17_cache_data, &v17_usr_config, sizeof(v17_usr_config));
	}
pcc_config_exit:
	return ret;
}

int pp_pcc_cache_params(struct mdp_pcc_cfg_data *config,
			struct mdp_pp_cache_res *res_cache)
{
	int ret = 0;
	if (!config || !res_cache) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, res_cache);
		return -EINVAL;
	}
	if (res_cache->block < SSPP_RGB || res_cache->block > DSPP) {
		pr_err("invalid block for PCC %d\n", res_cache->block);
		return -EINVAL;
	}
	if (!res_cache->mdss_pp_res && !res_cache->pipe_res) {
		pr_err("NULL payload for block %d mdss_pp_res %pK pipe_res %pK\n",
			res_cache->block, res_cache->mdss_pp_res,
			res_cache->pipe_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_pcc_v1_7:
		if (res_cache->block == DSPP) {
			ret = pp_pcc_cache_params_v1_7(config,
					res_cache->mdss_pp_res);
			if (ret)
				pr_err("caching for DSPP failed for PCC ret %d\n",
					ret);
		} else {
			ret = pp_pcc_cache_params_pipe_v1_7(config,
						res_cache->pipe_res);
			if (ret)
				pr_err("caching for SSPP failed for PCC ret %d block %d\n",
					ret, res_cache->block);
		}
		break;
	default:
		pr_err("unsupported pcc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_igc_lut_cache_params_v1_7(struct mdp_igc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    u32 copy_from_kernel)
{
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_igc_lut_data_v1_7 *v17_cache_data, v17_usr_config;
	u32 disp_num;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_v1_7;
	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->igc_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->igc_v17_data[disp_num];
		mdss_pp_res->igc_disp_cfg[disp_num].cfg_payload =
		(void *) v17_cache_data;
		if (!copy_from_kernel) {
			if (copy_from_user(&v17_usr_config,
					   config->cfg_payload,
					   sizeof(v17_usr_config))) {
				pr_err("failed to copy igc config\n");
				ret = -EFAULT;
				goto igc_config_exit;
			}
		} else {
			if (!config->cfg_payload) {
				pr_err("can't copy config info NULL payload\n");
				ret = -EINVAL;
				goto igc_config_exit;
			}
			memcpy(&v17_usr_config, config->cfg_payload,
			       sizeof(v17_usr_config));
		}
		if (!(config->ops & MDP_PP_OPS_WRITE)) {
			pr_debug("op for gamut %d\n", config->ops);
			goto igc_config_exit;
		}
		if (copy_from_kernel && (!v17_usr_config.c0_c1_data ||
		    !v17_usr_config.c2_data)) {
			pr_err("copy from kernel invalid params c0_c1_data %pK c2_data %pK\n",
				v17_usr_config.c0_c1_data,
				v17_usr_config.c2_data);
			ret = -EINVAL;
			goto igc_config_exit;
		}
		if (v17_usr_config.len != IGC_LUT_ENTRIES) {
			pr_err("Invalid table size %d exp %d\n",
				v17_usr_config.len, IGC_LUT_ENTRIES);
			ret = -EINVAL;
			goto igc_config_exit;
		}
		memcpy(v17_cache_data, &v17_usr_config,
		       sizeof(v17_usr_config));
		v17_cache_data->c0_c1_data =
		&res_cache->igc_table_c0_c1[disp_num][0];
		v17_cache_data->c2_data =
		&res_cache->igc_table_c2[disp_num][0];
		if (copy_from_kernel) {
			memcpy(v17_cache_data->c0_c1_data,
			       v17_usr_config.c0_c1_data,
			       v17_usr_config.len * sizeof(u32));
			memcpy(v17_cache_data->c2_data, v17_usr_config.c2_data,
			       v17_usr_config.len * sizeof(u32));
		} else {
			ret = copy_from_user(v17_cache_data->c0_c1_data,
					     v17_usr_config.c0_c1_data,
					     v17_usr_config.len * sizeof(u32));
			if (ret) {
				pr_err("copy from user failed for c0_c1_data size %zd ret %d\n",
				       v17_usr_config.len * sizeof(u32), ret);
				ret = -EFAULT;
				goto igc_config_exit;
			}
			ret = copy_from_user(v17_cache_data->c2_data,
					     v17_usr_config.c2_data,
					     v17_usr_config.len * sizeof(u32));
			if (ret) {
				pr_err("copy from user failed for c2_data size %zd ret %d\n",
				       v17_usr_config.len * sizeof(u32), ret);
				ret = -EFAULT;
				goto igc_config_exit;
			}
		}
	}
igc_config_exit:
	return ret;
}

static int pp_igc_lut_cache_params_pipe_v1_7(struct mdp_igc_lut_data *config,
			    struct mdss_mdp_pipe *pipe,
			    u32 copy_from_kernel)
{
	struct mdp_igc_lut_data_v1_7 *v17_cache_data = NULL, v17_usr_config;
	int ret = 0, fix_up = 0, i = 0;
	if (!config || !pipe) {
		pr_err("invalid param config %pK pipe %pK\n",
			config, pipe);
		return -EINVAL;
	}
	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	}

	if (!config->cfg_payload) {
		pr_err("can't copy config info NULL payload\n");
		ret = -EINVAL;
		goto igc_config_exit;
	}

	memcpy(&v17_usr_config, config->cfg_payload,
			sizeof(v17_usr_config));

	if (!(config->ops & MDP_PP_OPS_WRITE)) {
		pr_debug("op for gamut %d\n", config->ops);
		goto igc_config_exit;
	}

	switch (v17_usr_config.table_fmt) {
	case mdp_igc_custom:
		if (!v17_usr_config.c0_c1_data ||
		    !v17_usr_config.c2_data ||
		    v17_usr_config.len != IGC_LUT_ENTRIES) {
			pr_err("invalid c0_c1data %pK c2_data %pK tbl len %d\n",
					v17_usr_config.c0_c1_data,
					v17_usr_config.c2_data,
					v17_usr_config.len);
			ret = -EINVAL;
			goto igc_config_exit;
		}
		break;
	case mdp_igc_rec709:
		v17_usr_config.c0_c1_data = pp_igc_709;
		v17_usr_config.c2_data = pp_igc_709;
		v17_usr_config.len = IGC_LUT_ENTRIES;
		copy_from_kernel = 1;
		fix_up = 1;
		break;
	case mdp_igc_srgb:
		v17_usr_config.c0_c1_data = pp_igc_srgb;
		v17_usr_config.c2_data = pp_igc_srgb;
		v17_usr_config.len = IGC_LUT_ENTRIES;
		copy_from_kernel = 1;
		fix_up = 1;
		break;
	case mdp_igc_rec601:
		v17_usr_config.c0_c1_data = pp_igc_601;
		v17_usr_config.c2_data = pp_igc_601;
		v17_usr_config.len = IGC_LUT_ENTRIES;
		copy_from_kernel = 1;
		fix_up = 1;
		break;
	default:
		pr_err("invalid format %d\n",
				v17_usr_config.table_fmt);
		ret = -EINVAL;
		goto igc_config_exit;
	}
	v17_cache_data = pipe->pp_res.igc_cfg_payload;
	if (!v17_cache_data)
		v17_cache_data = kzalloc(sizeof(struct mdp_igc_lut_data_v1_7),
					GFP_KERNEL);
	if (!v17_cache_data) {
		ret = -ENOMEM;
		goto igc_config_exit;
	} else {
		pipe->pp_res.igc_cfg_payload = v17_cache_data;
		pipe->pp_cfg.igc_cfg.cfg_payload = v17_cache_data;
	}
	v17_cache_data->c0_c1_data = pipe->pp_res.igc_c0_c1;
	v17_cache_data->c2_data = pipe->pp_res.igc_c2;
	v17_cache_data->len = IGC_LUT_ENTRIES;
	if (copy_from_kernel) {
		memcpy(v17_cache_data->c0_c1_data,
				v17_usr_config.c0_c1_data,
				IGC_LUT_ENTRIES * sizeof(u32));
		memcpy(v17_cache_data->c2_data,
				v17_usr_config.c2_data,
				IGC_LUT_ENTRIES * sizeof(u32));
		if (fix_up) {
			for (i = 0; i < IGC_LUT_ENTRIES; i++)
				v17_cache_data->c0_c1_data[i]
					|= (v17_cache_data->c0_c1_data[i]
							<< IGC_C1_SHIFT);
		}
	} else {
		if (copy_from_user(v17_cache_data->c0_c1_data,
				v17_usr_config.c0_c1_data,
				IGC_LUT_ENTRIES * sizeof(u32))) {
			pr_err("error in copying the c0_c1_data of size %zd\n",
					IGC_LUT_ENTRIES * sizeof(u32));
			ret = -EFAULT;
			goto igc_config_exit;
		}
		if (copy_from_user(v17_cache_data->c2_data,
				v17_usr_config.c2_data,
				IGC_LUT_ENTRIES * sizeof(u32))) {
			pr_err("error in copying the c2_data of size %zd\n",
					IGC_LUT_ENTRIES * sizeof(u32));
			ret = -EFAULT;
		}
	}
igc_config_exit:
	if (ret || (config->ops & MDP_PP_OPS_DISABLE)) {
		kfree(v17_cache_data);
		pipe->pp_cfg.igc_cfg.cfg_payload = NULL;
		pipe->pp_res.igc_cfg_payload = NULL;
	}
	return ret;
}

int pp_igc_lut_cache_params(struct mdp_igc_lut_data *config,
			    struct mdp_pp_cache_res *res_cache,
			    u32 copy_from_kernel)
{
	int ret = 0;
	if (!config || !res_cache) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, res_cache);
		return -EINVAL;
	}
	if (res_cache->block < SSPP_RGB || res_cache->block > DSPP) {
		pr_err("invalid block for IGC %d\n", res_cache->block);
		return -EINVAL;
	}
	if (!res_cache->mdss_pp_res && !res_cache->pipe_res) {
		pr_err("NULL payload for block %d mdss_pp_res %pK pipe_res %pK\n",
			res_cache->block, res_cache->mdss_pp_res,
			res_cache->pipe_res);
		ret = -EINVAL;
		goto igc_exit;
	}
	switch (config->version) {
	case mdp_igc_v1_7:
		if (res_cache->block == DSPP) {
			ret = pp_igc_lut_cache_params_v1_7(config,
				     res_cache->mdss_pp_res, copy_from_kernel);
			if (ret)
				pr_err("failed to cache IGC params for DSPP ret %d\n",
					ret);

		} else {
			ret = pp_igc_lut_cache_params_pipe_v1_7(config,
				      res_cache->pipe_res, copy_from_kernel);
			if (ret)
				pr_err("failed to cache IGC params for SSPP ret %d\n",
					ret);
		}
		break;
	default:
		pr_err("unsupported igc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
igc_exit:
	return ret;
}

static int pp_pgc_lut_cache_params_v1_7(struct mdp_pgc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    int location)
{
	int ret = 0;
	u32 sz = 0;
	u32 disp_num;
	struct mdp_pgc_lut_data_v1_7 *v17_cache_data = NULL, v17_usr_config;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	if (location != DSPP && location != LM) {
		pr_err("Invalid location for pgc %d\n", location);
		return -EINVAL;
	}
	disp_num = PP_BLOCK(config->block) - MDP_LOGICAL_BLOCK_DISP_0;
	if (disp_num >= MDSS_BLOCK_DISP_NUM) {
		pr_err("invalid disp_num %d\n", disp_num);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_v1_7;
	if (!res_cache) {
		pr_err("invalid resource payload\n");
		return -EINVAL;
	}
	if (copy_from_user(&v17_usr_config, config->cfg_payload,
			   sizeof(v17_usr_config))) {
		pr_err("failed to copy from user config info\n");
		return -EFAULT;
	}
	if (v17_usr_config.len != PGC_LUT_ENTRIES) {
		pr_err("invalid entries for pgc act %d exp %d\n",
			v17_usr_config.len, PGC_LUT_ENTRIES);
		return -EFAULT;
	}
	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("ops read not supported\n");
		return -EINVAL;
	}
	if (!(config->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("ops write not set flags %d\n", config->flags);
		if (location == DSPP)
			mdss_pp_res->pgc_disp_cfg[disp_num].flags =
				config->flags;
		else
			mdss_pp_res->argc_disp_cfg[disp_num].flags =
				config->flags;
		return 0;
	}
	if (location == DSPP) {
		mdss_pp_res->pgc_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->pgc_dspp_v17_data[disp_num];
		v17_cache_data->c0_data = &res_cache->pgc_table_c0[disp_num][0];
		v17_cache_data->c1_data = &res_cache->pgc_table_c1[disp_num][0];
		v17_cache_data->c2_data = &res_cache->pgc_table_c2[disp_num][0];
		mdss_pp_res->pgc_disp_cfg[disp_num].cfg_payload =
			v17_cache_data;
	} else {
		mdss_pp_res->argc_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->pgc_lm_v17_data[disp_num];
		v17_cache_data->c0_data =
			&res_cache->pgc_lm_table_c0[disp_num][0];
		v17_cache_data->c1_data =
			&res_cache->pgc_lm_table_c1[disp_num][0];
		v17_cache_data->c2_data =
			&res_cache->pgc_lm_table_c2[disp_num][0];
		mdss_pp_res->argc_disp_cfg[disp_num].cfg_payload =
			v17_cache_data;
	}
	v17_cache_data->len = 0;
	sz = PGC_LUT_ENTRIES * sizeof(u32);
	if (copy_from_user(v17_cache_data->c0_data, v17_usr_config.c0_data,
			   sz)) {
		pr_err("failed to copy c0_data from user sz %d\n", sz);
		ret = -EFAULT;
		goto bail_out;
	}
	if (copy_from_user(v17_cache_data->c1_data, v17_usr_config.c1_data,
			   sz)) {
		pr_err("failed to copy c1_data from user sz %d\n", sz);
		ret = -EFAULT;
		goto bail_out;
	}
	if (copy_from_user(v17_cache_data->c2_data, v17_usr_config.c2_data,
			   sz)) {
		pr_err("failed to copy c2_data from user sz %d\n", sz);
		ret = -EFAULT;
		goto bail_out;
	}
	v17_cache_data->len = PGC_LUT_ENTRIES;
	return 0;
bail_out:
	if (location == DSPP)
		mdss_pp_res->pgc_disp_cfg[disp_num].flags = 0;
	else
		mdss_pp_res->argc_disp_cfg[disp_num].flags = 0;
	return ret;
}

int pp_pgc_lut_cache_params(struct mdp_pgc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res, int loc)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_pgc_v1_7:
		ret = pp_pgc_lut_cache_params_v1_7(config, mdss_pp_res, loc);
		break;
	default:
		pr_err("unsupported igc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_pa_cache_params_v1_7(struct mdp_pa_v2_cfg_data *config,
				   struct mdss_pp_res_type *mdss_pp_res)
{
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_pa_data_v1_7 *pa_cache_data, pa_usr_config;
	int disp_num, ret = 0;

	if (!config || !mdss_pp_res) {
		pr_err("Invalid param config %pK pp_res %pK\n",
			config, mdss_pp_res);
		return -EINVAL;
	}

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
			(config->block >= MDP_BLOCK_MAX)) {
		pr_err("Invalid config block %d\n", config->block);
		return -EINVAL;
	}

	if (!mdss_pp_res->pp_data_v1_7) {
		pr_err("Invalid pp_data_v1_7 %pK\n", mdss_pp_res->pp_data_v1_7);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_v1_7;
	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("Read op is not supported\n");
		return -EINVAL;
	}

	disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
	mdss_pp_res->pa_v2_disp_cfg[disp_num] = *config;
	pa_cache_data = &res_cache->pa_v17_data[disp_num];
	mdss_pp_res->pa_v2_disp_cfg[disp_num].cfg_payload =
		(void *) pa_cache_data;

	if (copy_from_user(&pa_usr_config, config->cfg_payload,
			   sizeof(pa_usr_config))) {
		pr_err("Failed to copy v1_7 PA\n");
		ret = -EFAULT;
		goto pa_config_exit;
	}

	if ((config->flags & MDP_PP_OPS_DISABLE)) {
		pr_debug("Disable PA\n");
		ret = 0;
		goto pa_config_exit;
	}

	if (!(config->flags & MDP_PP_OPS_WRITE)) {
		pr_debug("op for PA %d\n", config->flags);
		ret = 0;
		goto pa_config_exit;
	}

	memcpy(pa_cache_data, &pa_usr_config, sizeof(pa_usr_config));
	/* Copy six zone LUT if six zone is enabled to be written */
	if (config->flags & MDP_PP_PA_SIX_ZONE_ENABLE) {
		if (pa_usr_config.six_zone_len != MDP_SIX_ZONE_LUT_SIZE) {
			pr_err("Invalid six zone size, actual %d max %d\n",
					pa_usr_config.six_zone_len,
					MDP_SIX_ZONE_LUT_SIZE);
			ret = -EINVAL;
			goto pa_config_exit;
		}

		ret = copy_from_user(&res_cache->six_zone_lut_p0[disp_num][0],
				     pa_usr_config.six_zone_curve_p0,
				     pa_usr_config.six_zone_len * sizeof(u32));
		if (ret) {
			pr_err("copying six_zone_curve_p0 lut from userspace failed size %zd ret %d\n",
				(sizeof(u32) * pa_usr_config.six_zone_len),
				ret);
			ret = -EFAULT;
			goto pa_config_exit;
		}
		pa_cache_data->six_zone_curve_p0 =
			&res_cache->six_zone_lut_p0[disp_num][0];
		ret = copy_from_user(&res_cache->six_zone_lut_p1[disp_num][0],
				     pa_usr_config.six_zone_curve_p1,
				     pa_usr_config.six_zone_len * sizeof(u32));
		if (ret) {
			pr_err("copying six_zone_curve_p1 lut from userspace failed size %zd ret %d\n",
				(sizeof(u32) * pa_usr_config.six_zone_len),
				ret);
			ret = -EFAULT;
			goto pa_config_exit;
		}
		pa_cache_data->six_zone_curve_p1 =
			&res_cache->six_zone_lut_p1[disp_num][0];
	}

pa_config_exit:
	if (ret || config->flags & MDP_PP_OPS_DISABLE) {
		pa_cache_data->six_zone_len = 0;
		pa_cache_data->six_zone_curve_p0 = NULL;
		pa_cache_data->six_zone_curve_p1 = NULL;
	}
	return ret;
}

static int pp_pa_cache_params_pipe_v1_7(struct mdp_pa_v2_cfg_data *config,
			struct mdss_mdp_pipe *pipe)
{
	struct mdp_pa_data_v1_7 *pa_cache_data, pa_usr_config;
	int ret = 0;

	if (!config || !pipe) {
		pr_err("Invalid param config %pK pipe %pK\n",
			config, pipe);
		return -EINVAL;
	}

	if (config->flags & MDP_PP_OPS_DISABLE) {
		pr_debug("Disable PA on pipe %d\n", pipe->num);
		goto pa_cache_pipe_exit;
	}

	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("Read op is not supported\n");
		return -EINVAL;
	}

	if (!config->cfg_payload) {
		pr_err("invalid PA config payload\n");
		return -EINVAL;
	}

	memcpy(&pa_usr_config, config->cfg_payload,
			sizeof(struct mdp_pa_data_v1_7));

	pa_cache_data = pipe->pp_res.pa_cfg_payload;
	if (!pa_cache_data) {
		pa_cache_data = kzalloc(sizeof(struct mdp_pa_data_v1_7),
					GFP_KERNEL);
		if (!pa_cache_data) {
			pr_err("failed to allocate cache_data\n");
			ret = -ENOMEM;
			goto pa_cache_pipe_exit;
		} else
			pipe->pp_res.pa_cfg_payload = pa_cache_data;
	}

	*pa_cache_data = pa_usr_config;

	/* No six zone in SSPP */
	pa_cache_data->six_zone_len = 0;
	pa_cache_data->six_zone_curve_p0 = NULL;
	pa_cache_data->six_zone_curve_p1 = NULL;

pa_cache_pipe_exit:
	if (ret || (config->flags & MDP_PP_OPS_DISABLE)) {
		kfree(pipe->pp_res.pa_cfg_payload);
		pipe->pp_res.pa_cfg_payload = NULL;
	}
	pipe->pp_cfg.pa_v2_cfg_data.cfg_payload = pipe->pp_res.pa_cfg_payload;
	return ret;
}

int pp_pa_cache_params(struct mdp_pa_v2_cfg_data *config,
			struct mdp_pp_cache_res *res_cache)
{
	int ret = 0;
	if (!config || !res_cache) {
		pr_err("invalid param config %pK pp_res %pK\n",
			config, res_cache);
		return -EINVAL;
	}
	if (res_cache->block != SSPP_VIG && res_cache->block != DSPP) {
		pr_err("invalid block for PA %d\n", res_cache->block);
		return -EINVAL;
	}
	if (!res_cache->mdss_pp_res && !res_cache->pipe_res) {
		pr_err("NULL payload for block %d mdss_pp_res %pK pipe_res %pK\n",
			res_cache->block, res_cache->mdss_pp_res,
			res_cache->pipe_res);
		return -EINVAL;
	}

	switch (config->version) {
	case mdp_pa_v1_7:
		if (res_cache->block == DSPP) {
			ret = pp_pa_cache_params_v1_7(config,
					res_cache->mdss_pp_res);
			if (ret)
				pr_err("failed to cache PA params for DSPP ret %d\n",
					ret);
		} else {
			ret = pp_pa_cache_params_pipe_v1_7(config,
					res_cache->pipe_res);
			if (ret)
				pr_err("failed to cache PA params for SSPP ret %d\n",
					ret);

		}
		break;
	default:
		pr_err("unsupported pa version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int pp_copy_layer_igc_payload(struct mdp_overlay_pp_params *pp_info)
{
	void *cfg_payload = NULL;
	int ret = 0;

	switch (pp_info->igc_cfg.version) {
	case mdp_igc_v1_7:
		cfg_payload = kmalloc(
				sizeof(struct mdp_igc_lut_data_v1_7),
				GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}

		ret = copy_from_user(cfg_payload,
				pp_info->igc_cfg.cfg_payload,
				sizeof(struct mdp_igc_lut_data_v1_7));
		if (ret) {
			pr_err("layer list copy from user failed, IGC cfg payload = %pK\n",
				pp_info->igc_cfg.cfg_payload);
			ret = -EFAULT;
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("No version set, fallback to legacy IGC version\n");
		cfg_payload = NULL;
		break;
	}

exit:
	pp_info->igc_cfg.cfg_payload = cfg_payload;
	return ret;
}

int pp_copy_layer_hist_lut_payload(struct mdp_overlay_pp_params *pp_info)
{
	void *cfg_payload = NULL;
	int ret = 0;

	switch (pp_info->hist_lut_cfg.version) {
	case mdp_hist_lut_v1_7:
		cfg_payload = kmalloc(
				sizeof(struct mdp_hist_lut_data_v1_7),
				GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}

		ret = copy_from_user(cfg_payload,
				pp_info->hist_lut_cfg.cfg_payload,
				sizeof(struct mdp_hist_lut_data_v1_7));
		if (ret) {
			pr_err("layer list copy from user failed, Hist LUT cfg payload = %pK\n",
				pp_info->hist_lut_cfg.cfg_payload);
			ret = -EFAULT;
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("No version set, fallback to legacy Hist LUT version\n");
		cfg_payload = NULL;
		break;
	}

exit:
	pp_info->hist_lut_cfg.cfg_payload = cfg_payload;
	return ret;
}

int pp_copy_layer_pa_payload(struct mdp_overlay_pp_params *pp_info)
{
	void *cfg_payload = NULL;
	int ret = 0;

	switch (pp_info->pa_v2_cfg_data.version) {
	case mdp_pa_v1_7:
		cfg_payload = kmalloc(
				sizeof(struct mdp_pa_data_v1_7),
				GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}

		ret = copy_from_user(cfg_payload,
				pp_info->pa_v2_cfg_data.cfg_payload,
				sizeof(struct mdp_pa_data_v1_7));
		if (ret) {
			pr_err("layer list copy from user failed, PA cfg payload = %pK\n",
				pp_info->pa_v2_cfg_data.cfg_payload);
			ret = -EFAULT;
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("No version set, fallback to legacy PA version\n");
		cfg_payload = NULL;
		break;
	}

exit:
	pp_info->pa_v2_cfg_data.cfg_payload = cfg_payload;
	return ret;
}

int pp_copy_layer_pcc_payload(struct mdp_overlay_pp_params *pp_info)
{
	void *cfg_payload = NULL;
	int ret = 0;

	switch (pp_info->pcc_cfg_data.version) {
	case mdp_pcc_v1_7:
		cfg_payload = kmalloc(
				sizeof(struct mdp_pcc_data_v1_7),
				GFP_KERNEL);
		if (!cfg_payload) {
			ret = -ENOMEM;
			goto exit;
		}

		ret = copy_from_user(cfg_payload,
				pp_info->pcc_cfg_data.cfg_payload,
				sizeof(struct mdp_pcc_data_v1_7));
		if (ret) {
			pr_err("layer list copy from user failed, PCC cfg payload = %pK\n",
				pp_info->pcc_cfg_data.cfg_payload);
			ret = -EFAULT;
			kfree(cfg_payload);
			cfg_payload = NULL;
			goto exit;
		}
		break;
	default:
		pr_debug("No version set, fallback to legacy PCC version\n");
		cfg_payload = NULL;
		break;
	}

exit:
	pp_info->pcc_cfg_data.cfg_payload = cfg_payload;
	return ret;
}
