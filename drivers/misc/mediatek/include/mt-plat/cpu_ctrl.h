/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _CPU_CTRL_H
#define _CPU_CTRL_H


#include <mtk_ppm_api.h>

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
	CPU_MAX_KIR
};

enum {
	CPU_ISO_KIR_PERF = 0,
	CPU_ISO_KIR_FPSGO,
	CPU_ISO_KIR_CPU_CTRL,
	CPU_ISO_MAX_KIR
};

extern unsigned int __attribute__((weak)) mt_ppm_userlimit_cpu_freq(unsigned int cluster_num,
	struct ppm_limit_data *data);
extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);
extern int update_userlimit_cpu_freq(int kicker, int num_cluster
				, struct ppm_limit_data *freq_limit);
extern int update_userlimit_cpu_core(int kicker, int num_cluster
				, struct ppm_limit_data *core_limit);
extern int sched_isolate_cpu(int cpu);
extern int sched_deisolate_cpu(int cpu);

int update_isolation_cpu(int kicker, int enable, int cpu);

#endif /* _CPU_CTRL_H */
