/*
 * Copyright (c) 2016, 2018,The Linux Foundation. All rights reserved.
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
#ifndef MDSS_MDP_PP_COMMON_H
#define MDSS_MDP_PP_COMMON_H

#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"

#define JUMP_REGISTERS_OFF(n) ((n) * (sizeof(uint32_t)))
#define REG_MASK(n) ((BIT(n)) - 1)
#define REG_MASK_SHIFT(n, shift) ((REG_MASK(n)) << (shift))

void pp_pa_set_sts(struct pp_sts_type *pp_sts,
		   struct mdp_pa_data_v1_7 *pa_data,
		   int enable_flag, int block_type);
#endif
