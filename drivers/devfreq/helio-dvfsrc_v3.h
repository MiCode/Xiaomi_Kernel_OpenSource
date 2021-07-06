/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

