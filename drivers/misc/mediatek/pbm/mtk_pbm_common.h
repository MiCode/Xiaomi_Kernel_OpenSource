/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_PBM_COMMON_
#define _MTK_PBM_COMMON_

struct pbm {
	u8 pbm_stop;
	u8 pbm_drv_done;
	u32 hpf_en;
	u32 manual_mode;
};

struct hpf {
	bool switch_md1;
	bool switch_gpu;
	bool switch_flash;
	bool md1_ccci_ready;
	unsigned long loading_dlpt;
	unsigned long loading_md1;
	unsigned long loading_cpu;
	unsigned long loading_gpu;
	unsigned long loading_flash;
	unsigned long to_cpu_budget;
	unsigned long to_gpu_budget;
};

struct mrp {
	bool switch_md;
	bool switch_gpu;
	bool switch_flash;
	unsigned long loading_dlpt;
	unsigned long loading_cpu;
	unsigned long loading_gpu;
};

struct cpu_pbm_policy {
	unsigned int               cpu;
	unsigned int               num_cpus;
	unsigned int               power_weight;
	unsigned int               max_perf_state;
	unsigned int               power;
	unsigned int               freq;
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct em_perf_domain      *em;
	struct list_head           cpu_pbm_list;
};

typedef void (*pbm_callback)(unsigned int power);
struct pbm_callback_table {
	void (*pbmcb)(unsigned int power);
};


#endif
