/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, 2018, 2020, The Linux Foundation. All rights reserved.
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
