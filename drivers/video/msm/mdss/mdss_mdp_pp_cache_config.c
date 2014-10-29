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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"
#include "mdss_mdp_pp_cache_config.h"

static int pp_hist_lut_cache_params_v1_7(struct mdp_hist_lut_data *config,
				      struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	struct mdp_hist_lut_data_v1_7 *v17_cache_data = NULL, v17_usr_config;
	int ret = 0;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_res) {
		pr_err("invalid pp_data_res %p\n", mdss_pp_res->pp_data_res);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_res;
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

int pp_hist_lut_cache_params(struct mdp_hist_lut_data *config,
			  struct mdss_pp_res_type *mdss_pp_res)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_hist_lut_v1_7:
		ret = pp_hist_lut_cache_params_v1_7(config, mdss_pp_res);
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
			  struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num;
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	struct mdp_dither_data_v1_7 *v17_cache_data = NULL, v17_usr_config;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_res) {
		pr_err("invalid pp_data_res %p\n", mdss_pp_res->pp_data_res);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_res;

	if ((config->flags & MDSS_PP_SPLIT_MASK) == MDSS_PP_SPLIT_MASK) {
		pr_warn("Can't set both split bits\n");
		return -EINVAL;
	}

	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->dither_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->dither_v17_data[disp_num];
		mdss_pp_res->dither_disp_cfg[disp_num].cfg_payload =
			(void *)v17_cache_data;
		if (copy_from_user(&v17_usr_config, config->cfg_payload,
			sizeof(v17_usr_config))) {
			pr_err("failed to copy v17 dither\n");
			ret = -EFAULT;
			goto dither_config_exit;
		}
		if ((config->flags & MDP_PP_OPS_DISABLE)) {
			pr_debug("disable dither");
			ret = 0;
			goto dither_config_exit;
		}
		if (!(config->flags & MDP_PP_OPS_WRITE)) {
			pr_debug("op for dither %d\n", config->flags);
			goto dither_config_exit;
		}
		memcpy(v17_cache_data, &v17_usr_config, sizeof(v17_usr_config));
	}
dither_config_exit:
	return ret;
}

int pp_dither_cache_params(struct mdp_dither_cfg_data *config,
	struct mdss_pp_res_type *mdss_pp_res)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %pi pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_dither_v1_7:
		ret = pp_dither_cache_params_v1_7(config, mdss_pp_res);
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
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_res) {
		pr_err("invalid pp_data_res %p\n", mdss_pp_res->pp_data_res);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_res;
	if (config->flags & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->gamut_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->gamut_v17_data[disp_num];
		mdss_pp_res->gamut_disp_cfg[disp_num].cfg_payload =
			(void *) v17_cache_data;
		tbl_gamut = v17_cache_data->c0_data[0];

		if (copy_from_user(&v17_usr_config, config->cfg_payload,
				   sizeof(v17_usr_config))) {
			pr_err("failed to copy v17 gamut\n");
			ret = -EFAULT;
			goto gamut_config_exit;
		}
		if ((config->flags & MDP_PP_OPS_DISABLE)) {
			pr_debug("disable gamut\n");
			ret = 0;
			goto gamut_memory_free_exit;
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
		/* sanity check for sizes */
		for (i = 0; i < MDP_GAMUT_TABLE_NUM_V1_7; i++) {
			if (v17_usr_config.tbl_size[i] != tbl_sz) {
				pr_err("invalid tbl_sz %d exp %d for mode %d\n",
				       v17_usr_config.tbl_size[i], tbl_sz,
				       v17_usr_config.mode);
				goto gamut_config_exit;
			}
			gamut_size += v17_usr_config.tbl_size[i];
			if (i >= MDP_GAMUT_SCALE_OFF_TABLE_NUM)
				continue;
			if (v17_usr_config.tbl_scale_off_sz[i] !=
			    MDP_GAMUT_SCALE_OFF_SZ) {
				pr_err("invalid scale_sz %d exp %d for mode %d\n",
				       v17_usr_config.tbl_scale_off_sz[i],
				       MDP_GAMUT_SCALE_OFF_SZ,
				       v17_usr_config.mode);
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
		pr_err("invalid param config %p pp_res %p\n",
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

static int pp_pcc_cache_params_v1_7(struct mdp_pcc_cfg_data *config,
				      struct mdss_pp_res_type *mdss_pp_res)
{
	u32 disp_num;
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_pcc_data_v1_7 *v17_cache_data, v17_usr_config;

	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}

	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_res) {
		pr_err("invalid pp_data_res %p\n", mdss_pp_res->pp_data_res);
		return -EINVAL;
	}

	res_cache = mdss_pp_res->pp_data_res;
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
			  struct mdss_pp_res_type *mdss_pp_res)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_pcc_v1_7:
		ret = pp_pcc_cache_params_v1_7(config, mdss_pp_res);
		break;
	default:
		pr_err("unsupported pcc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int pp_igc_lut_cache_params_v1_7(struct mdp_igc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    u32 copy_from_kernel)
{
	int ret = 0;
	struct mdss_pp_res_type_v1_7 *res_cache;
	struct mdp_igc_lut_data_v1_7 *v17_cache_data, v17_usr_config;
	u32 disp_num;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	if ((config->block < MDP_LOGICAL_BLOCK_DISP_0) ||
		(config->block >= MDP_BLOCK_MAX)) {
		pr_err("invalid config block %d\n", config->block);
		return -EINVAL;
	}
	if (!mdss_pp_res->pp_data_res) {
		pr_err("invalid pp_data_res %p\n", mdss_pp_res->pp_data_res);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_res;
	if (config->ops & MDP_PP_OPS_READ) {
		pr_err("read op is not supported\n");
		return -EINVAL;
	} else {
		disp_num = config->block - MDP_LOGICAL_BLOCK_DISP_0;
		mdss_pp_res->igc_disp_cfg[disp_num] = *config;
		v17_cache_data = &res_cache->igc_v17_data[disp_num];
		mdss_pp_res->igc_disp_cfg[disp_num].cfg_payload =
		(void *) v17_cache_data;
		if (copy_from_user(&v17_usr_config, config->cfg_payload,
				   sizeof(v17_usr_config))) {
			pr_err("failed to copy igc config\n");
			ret = -EFAULT;
			goto igc_config_exit;
		}
		if (!(config->ops & MDP_PP_OPS_WRITE)) {
			pr_debug("op for gamut %d\n", config->ops);
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

int pp_igc_lut_cache_params(struct mdp_igc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    u32 copy_from_kernel)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_igc_v1_7:
		ret = pp_igc_lut_cache_params_v1_7(config, mdss_pp_res,
						  copy_from_kernel);
		break;
	default:
		pr_err("unsupported igc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pp_pgc_lut_cache_params_v1_7(struct mdp_pgc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res,
			    int location, int cnt)
{
	int ret = 0, max_cnt = 0;
	u32 sz = 0;
	struct mdp_pgc_lut_data_v1_7 *v17_cache_data = NULL, v17_usr_config;
	struct mdss_pp_res_type_v1_7 *res_cache = NULL;
	if (location != DSPP && location != LM) {
		pr_err("Invalid location for pgc %d\n", location);
		return -EINVAL;
	}
	max_cnt = (location == DSPP) ? MDSS_BLOCK_DISP_NUM :
		   MDSS_BLOCK_LM_NUM;
	if (cnt >= max_cnt) {
		pr_err("invalid layer count %d max is %d location is %d\n",
			cnt, max_cnt, location);
		return -EINVAL;
	}
	res_cache = mdss_pp_res->pp_data_res;
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
			mdss_pp_res->pgc_disp_cfg[cnt].flags = config->flags;
		else
			mdss_pp_res->argc_disp_cfg[cnt].flags = config->flags;
		return 0;
	}
	if (location == DSPP) {
		mdss_pp_res->pgc_disp_cfg[cnt] = *config;
		v17_cache_data = &res_cache->pgc_dspp_v17_data[cnt];
		v17_cache_data->c0_data = &res_cache->pgc_table_c0[cnt][0];
		v17_cache_data->c1_data = &res_cache->pgc_table_c1[cnt][0];
		v17_cache_data->c2_data = &res_cache->pgc_table_c2[cnt][0];
		mdss_pp_res->pgc_disp_cfg[cnt].cfg_payload = v17_cache_data;
	} else {
		mdss_pp_res->argc_disp_cfg[cnt] = *config;
		v17_cache_data = &res_cache->pgc_lm_v17_data[cnt];
		v17_cache_data->c0_data = &res_cache->pgc_lm_table_c0[cnt][0];
		v17_cache_data->c1_data = &res_cache->pgc_lm_table_c1[cnt][0];
		v17_cache_data->c2_data = &res_cache->pgc_lm_table_c2[cnt][0];
		mdss_pp_res->argc_disp_cfg[cnt].cfg_payload = v17_cache_data;
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
		mdss_pp_res->pgc_disp_cfg[cnt].flags = 0;
	else
		mdss_pp_res->argc_disp_cfg[cnt].flags = 0;
	return ret;
}

int pp_pgc_lut_cache_params(struct mdp_pgc_lut_data *config,
			    struct mdss_pp_res_type *mdss_pp_res, int loc,
			    int cnt)
{
	int ret = 0;
	if (!config || !mdss_pp_res) {
		pr_err("invalid param config %p pp_res %p\n",
			config, mdss_pp_res);
		return -EINVAL;
	}
	switch (config->version) {
	case mdp_pgc_v1_7:
		ret = pp_pgc_lut_cache_params_v1_7(config, mdss_pp_res,
						   loc, cnt);
		break;
	default:
		pr_err("unsupported igc version %d\n",
			config->version);
		ret = -EINVAL;
		break;
	}
	return ret;
}
