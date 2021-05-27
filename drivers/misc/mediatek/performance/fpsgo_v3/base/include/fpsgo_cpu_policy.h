/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __FPSGO_CPU_POLICY_H__
#define __FPSGO_CPU_POLICY_H__

void fpsgo_cpu_policy_init(void);
int fpsgo_get_cpu_policy_num(void);
int fpsgo_get_cpu_opp_info(int **opp_cnt, unsigned int ***opp_tbl);

#endif /* __FPSGO_CPU_POLICY_H__ */

