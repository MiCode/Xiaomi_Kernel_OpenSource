/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __RS_PFM_H__
#define __RS_PFM_H__

/* VPU */
int rs_get_vpu_core_num(void);
int rs_get_vpu_opp_max(int core);
int rs_vpu_support_idletime(void);
int rs_get_vpu_curr_opp(int core);
int rs_get_vpu_ceiling_opp(int core);
int rs_vpu_opp_to_freq(int core, int step);

/* MDLA */
int rs_get_mdla_core_num(void);
int rs_get_mdla_opp_max(int core);
int rs_mdla_support_idletime(void);
int rs_get_mdla_curr_opp(int core);
int rs_get_mdla_ceiling_opp(int core);
int rs_mdla_opp_to_freq(int core, int step);

#endif
