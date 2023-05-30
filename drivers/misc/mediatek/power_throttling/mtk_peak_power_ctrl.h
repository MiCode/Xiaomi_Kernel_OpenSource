/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_PEAK_POWER_CTRL_
#define _MTK_PEAK_POWER_CTRL_

#define PPC_MAX_CLUSTER_NUM 4

struct cpu_ppc_policy {
	unsigned int            cpu;
	struct cpufreq_policy   *policy;
	struct freq_qos_request qos_req;
	struct list_head        cpu_ppc_list;
};

struct cpu_freq_t {
	unsigned int cluster;
	unsigned int freq;
};

struct cpu_freq_thr_t {
	unsigned int cluster_num;
	struct cpu_freq_t cur_freq[PPC_MAX_CLUSTER_NUM];
	struct cpu_freq_t thr_freq[PPC_MAX_CLUSTER_NUM];
};

struct ppc_ctrl_t {
	unsigned int gpu_limit_state;
	unsigned int cpu_limit_state;
	unsigned int source_state;
	struct cpu_freq_thr_t *cpu_freq_thr_info;
};

typedef void (*ppc_callback)(unsigned int power);
struct ppc_callback_table {
	void (*ppccb)(unsigned int power);
};

#endif
