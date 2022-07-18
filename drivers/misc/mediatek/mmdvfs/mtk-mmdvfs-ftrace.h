/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef MTK_MMDVFS_FTRACE_H
#define MTK_MMDVFS_FTRACE_H

void ftrace_record_opp_v1(unsigned long rec, unsigned long opp);
void ftrace_pwr_opp_v3(unsigned long pwr, unsigned long opp);
void ftrace_user_opp_v3_vcore(unsigned long user, unsigned long opp);
void ftrace_user_opp_v3_vmm(unsigned long user, unsigned long opp);

#endif

