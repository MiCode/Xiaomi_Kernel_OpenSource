/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_MFG_COUNTER_H_
#define __MTK_MFG_COUNTER_H_

#include <mt-plat/mtk_gpu_utility.h>
#define MALI_HWC_TYPES					4
#define MALI_COUNTERS_PER_BLOCK			64

enum {
	PMU_OK = 0,
	PMU_NG = 1,
	/* reset PMU value if needed */
	PMU_RESET_VALUE = 2,
};

extern int (*mtk_get_gpu_pmu_init_fp)(struct GPU_PMU *pmus, int pmu_size, int *ret_size);
extern int (*mtk_get_gpu_pmu_deinit_fp)(void);
extern int (*mtk_get_gpu_pmu_swapnreset_fp)(struct GPU_PMU *pmus, int pmu_size);
extern int (*mtk_get_gpu_pmu_swapnreset_stop_fp)(void);
/* Need to get current gpu freq from GPU DVFS module */
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);

void mtk_mfg_counter_init(void);
void mtk_mfg_counter_destroy(void);
int gator_gpu_pmu_init(void);
//stall counter
int mtk_gpu_stall_create_subfs(void);
void mtk_gpu_stall_delete_subfs(void);
void mtk_gpu_stall_start(void);
void mtk_gpu_stall_stop(void);
void mtk_GPU_STALL_RAW(unsigned int *diff, int size);


#endif
