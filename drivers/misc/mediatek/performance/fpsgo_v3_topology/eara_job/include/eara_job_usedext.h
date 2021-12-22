/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef EARA_JOB_USEDEXT_H
#define EARA_JOB_USEDEXT_H

enum device_component {
	DEVICE_CPU,
	DEVICE_GPU,
	DEVICE_VPU,
	DEVICE_MDLA,
	DEVICE_NEON
};
extern struct ppm_cobra_data *ppm_cobra_pass_tbl(void);
extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);
#endif

