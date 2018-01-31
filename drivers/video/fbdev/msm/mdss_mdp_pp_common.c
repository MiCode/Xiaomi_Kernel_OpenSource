/*
 * Copyright (c) 2016, 2018, The Linux Foundation. All rights reserved.
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
#include "mdss_mdp_pp_common.h"

void pp_pa_set_sts(struct pp_sts_type *pp_sts,
		   struct mdp_pa_data_v1_7 *pa_data,
		   int enable_flag, int block_type)
{
	if (!pp_sts) {
		pr_err("invalid input pp_sts %pK\n", pp_sts);
		return;
	}

	pp_sts->pa_sts = 0;

	if (enable_flag & MDP_PP_OPS_DISABLE) {
		pp_sts->pa_sts &= ~PP_STS_ENABLE;
		return;
	} else if (enable_flag & MDP_PP_OPS_ENABLE) {
		pp_sts->pa_sts |= PP_STS_ENABLE;
	}

	if (!pa_data) {
		pr_err("invalid input pa_data %pK\n", pa_data);
		return;
	}

	/* Global HSV STS update */
	if (pa_data->mode & MDP_PP_PA_HUE_MASK)
		pp_sts->pa_sts |= PP_STS_PA_HUE_MASK;
	if (pa_data->mode & MDP_PP_PA_SAT_MASK)
		pp_sts->pa_sts |= PP_STS_PA_SAT_MASK;
	if (pa_data->mode & MDP_PP_PA_VAL_MASK)
		pp_sts->pa_sts |= PP_STS_PA_VAL_MASK;
	if (pa_data->mode & MDP_PP_PA_CONT_MASK)
		pp_sts->pa_sts |= PP_STS_PA_CONT_MASK;
	if (pa_data->mode & MDP_PP_PA_SAT_ZERO_EXP_EN)
		pp_sts->pa_sts |= PP_STS_PA_SAT_ZERO_EXP_EN;

	/* Memory Protect STS update */
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_HUE_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_HUE_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_SAT_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_SAT_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_VAL_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_VAL_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_CONT_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_CONT_EN;
	if (pa_data->mode & MDP_PP_PA_MEM_PROT_BLEND_EN)
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_BLEND_EN;
	if ((block_type == DSPP) &&
			(pa_data->mode & MDP_PP_PA_MEM_PROT_SIX_EN))
		pp_sts->pa_sts |= PP_STS_PA_MEM_PROT_SIX_EN;

	/* Memory Color STS update */
	if (pa_data->mode & MDP_PP_PA_MEM_COL_SKIN_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_SKIN_MASK;
	if (pa_data->mode & MDP_PP_PA_MEM_COL_SKY_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_SKY_MASK;
	if (pa_data->mode & MDP_PP_PA_MEM_COL_FOL_MASK)
		pp_sts->pa_sts |= PP_STS_PA_MEM_COL_FOL_MASK;

	/* Six Zone STS update */
	if (block_type == DSPP) {
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_HUE_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_HUE_MASK;
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_SAT_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_SAT_MASK;
		if (pa_data->mode & MDP_PP_PA_SIX_ZONE_VAL_MASK)
			pp_sts->pa_sts |= PP_STS_PA_SIX_ZONE_VAL_MASK;

		pp_sts_set_split_bits(&pp_sts->pa_sts, enable_flag);
	}
}
