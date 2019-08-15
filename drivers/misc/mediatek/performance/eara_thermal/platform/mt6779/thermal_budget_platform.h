/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef EARA_THRM_PB_PLAT_H
#define EARA_THRM_PB_PLAT_H

#define GPU_OPP_RANGE	5
#define CPU_OPP_NUM	16
#define KEEP_L_CORE	1

#define CLUSTER_B	1
#define CLUSTER_L	0
#define CPU_CORE_NUM	8

extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
int eara_thrm_get_nr_clusters(void);
unsigned int eara_thrm_get_freq_by_idx(int cluster, int opp);

#endif
