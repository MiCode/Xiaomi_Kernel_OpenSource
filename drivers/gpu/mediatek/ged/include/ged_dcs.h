/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __GED_DCS_H__
#define __GED_DCS_H__

#include "ged_type.h"

#define DCS_POLICY_MARGIN    (250)
#define DCS_MIN_OPP_CNT      (4)
#define DCS_DEFAULT_MIN_CORE (5)

struct dcs_core_mask {
	unsigned int core_mask;
	unsigned int core_num;
};

struct dcs_virtual_opp {
	int idx;
	unsigned int freq;
	unsigned int freq_real;
	int core_num;
};

GED_ERROR ged_dcs_init_platform_info(void);
void ged_dcs_exit(void);
struct gpufreq_core_mask_info *dcs_get_avail_mask_table(void);

int dcs_get_dcs_opp_setting(void);
int dcs_get_cur_core_num(void);
int dcs_get_max_core_num(void);
int dcs_get_avail_mask_num(void);
int dcs_set_core_mask(unsigned int core_mask, unsigned int core_num);
int dcs_restore_max_core_mask(void);
int is_dcs_enable(void);
void dcs_enable(int enable);

#endif /* __GED_DCS_H__ */
