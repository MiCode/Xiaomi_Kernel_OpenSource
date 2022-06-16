/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_CTRL_H__
#define __FBT_CPU_CTRL_H__

typedef	void (*cfp_notifier_fn_t)(int);

struct cpu_ctrl_data {
	int min;
	int max;
};

#define CPU_KIR_FPSGO 1

enum {
	CFP_KIR_FPSGO = 0,
	CFP_KIR_THER_AWARE,
	CFP_KIR_MAX_NUM,
};

int fbt_cpu_ctrl_init(void);
int fbt_cpu_ctrl_exit(void);
int fbt_set_cpu_freq_ceiling(int num, int *freq);
void update_userlimit_cpu_freq(int kicker, int cluster_num, struct cpu_ctrl_data *pld);
void fbt_cpu_L_ceiling_min(int freq);
int fbt_cpu_ctrl_get_ceil(void);

int cfp_mon_enable(int kicker, cfp_notifier_fn_t cb);
int cfp_mon_disable(int kicker);

#endif

