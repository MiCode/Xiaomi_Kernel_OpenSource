/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _MTK_PBM_COMMON_
#define _MTK_PBM_COMMON_

struct pbm {
	u8 feature_en;
	u8 pbm_drv_done;
	u32 hpf_en;
	u32 manual_mode;
};

struct hpf {
	bool switch_md1;
	bool switch_gpu;
	bool switch_flash;

	bool md1_ccci_ready;

	int cpu_volt;
	int gpu_volt;
	int cpu_num;

	unsigned long loading_leakage;
	unsigned long loading_dlpt;
	unsigned long loading_md1;
	unsigned long loading_cpu;
	unsigned long loading_gpu;
	unsigned long loading_flash;
};

struct mrp {
	bool switch_md;
	bool switch_gpu;
	bool switch_flash;

	int cpu_volt;
	int gpu_volt;
	int cpu_num;

	unsigned long loading_dlpt;
	unsigned long loading_cpu;
	unsigned long loading_gpu;
};

#endif
