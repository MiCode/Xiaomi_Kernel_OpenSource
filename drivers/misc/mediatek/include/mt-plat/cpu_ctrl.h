/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _CPU_CTRL_H
#define _CPU_CTRL_H

enum {
	CPU_KIR_PERF = 0,
	CPU_KIR_FPSGO,
	CPU_KIR_WIFI,
	CPU_KIR_BOOT,
	CPU_KIR_TOUCH,
	CPU_KIR_PERFTOUCH,
	CPU_KIR_USB,
	CPU_KIR_AMMS,
	CPU_KIR_GBE,
	CPU_KIR_CCCI,
	CPU_KIR_SYSLIM,
	CPU_KIR_CORE_CTL,
	CPU_MAX_KIR
};

enum {
	CPU_ISO_KIR_PERF_CORE = 0,
	CPU_ISO_KIR_PERF_ISO,
	CPU_ISO_KIR_FPSGO,
	CPU_ISO_KIR_CPU_CTRL,
	CPU_ISO_MAX_KIR
};

struct cpu_ctrl_data {
	int min;
	int max;
};

extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);
extern int update_userlimit_cpu_freq(int kicker, int num_cluster
				, struct cpu_ctrl_data *freq_limit);
extern int update_userlimit_cpu_core(int kicker, int num_cluster
				, struct cpu_ctrl_data *core_limit);

#endif /* _CPU_CTRL_H */

