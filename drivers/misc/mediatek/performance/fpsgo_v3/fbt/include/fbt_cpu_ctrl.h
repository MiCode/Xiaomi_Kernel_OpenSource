/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_CTRL_H__
#define __FBT_CPU_CTRL_H__

extern int powerhal_tid;

int fbt_cpu_ctrl_init(void);
int fbt_cpu_ctrl_exit(void);
int fbt_set_cpu_freq_ceiling(int num, int *freq);

#endif

