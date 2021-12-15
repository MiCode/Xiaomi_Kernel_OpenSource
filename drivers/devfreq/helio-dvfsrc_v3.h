/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __HELIO_DVFSRC_V3_H
#define __HELIO_DVFSRC_V3_H
#include "helio-dvfsrc-opp.h"

extern int is_dvfsrc_enabled(void);
extern void dvfsrc_set_power_model_ddr_request(unsigned int level);

/* met profile function */
extern int vcorefs_get_num_opp(void);
extern int vcorefs_get_opp_info_num(void);
extern char **vcorefs_get_opp_info_name(void);
extern unsigned int *vcorefs_get_opp_info(void);
extern int vcorefs_get_src_req_num(void);
extern char **vcorefs_get_src_req_name(void);
extern unsigned int *vcorefs_get_src_req(void);
extern u32 vcorefs_get_md_scenario(void);
extern int get_cur_ddr_ratio(void);
extern int is_dvfsrc_opp_fixed(void);

#endif /* __HELIO_DVFSRC_V3_H */

