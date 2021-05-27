/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_CTRL_H__
#define __FBT_CPU_CTRL_H__

struct cpu_ctrl_data {
	int min;
	int max;
};
#define CPU_KIR_FPSGO 1

extern int powerhal_tid;

int fbt_cpu_ctrl_init(void);
int fbt_cpu_ctrl_exit(void);
int fbt_set_cpu_freq_ceiling(int num, int *freq);
void update_userlimit_cpu_freq(int kicker, int cluster_num, struct cpu_ctrl_data *pld);

#endif

