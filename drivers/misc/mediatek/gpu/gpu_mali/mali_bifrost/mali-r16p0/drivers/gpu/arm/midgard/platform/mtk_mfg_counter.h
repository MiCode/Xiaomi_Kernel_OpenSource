/*
 * Copyright (C) 2017 MediaTek Inc.
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

enum {
	PMU_OK = 0,
	PMU_NG = 1,
	/* reset PMU value if needed */
	PMU_RESET_VALUE = 2,
};

//MTK COUNTER
typedef enum {
	GPU_ACTIVE,
	EXEC_INSTR_COUNT,
	EXEC_CORE_ACTIVE,
	EXEC_ACTIVE,
	FRAG_ACTIVE,
	TILER_ACTIVE,
	TEX_FILT_NUM_OPERATIONS,
	LS_MEM_READ_FULL,
	LS_MEM_WRITE_FULL,
	LS_MEM_READ_SHORT,
	LS_MEM_WRITE_SHORT,
	L2_EXT_WRITE_BEATS,
	L2_EXT_READ_BEATS,
	L2_EXT_RRESP_0_127,
	L2_EXT_RRESP_128_191,
	L2_EXT_RRESP_192_255,
	L2_EXT_RRESP_256_319,
	L2_EXT_RRESP_320_383,
	L2_ANY_LOOKUP,
	PERF_COUNTER_LAST
} mtk_perf_counter;


extern int (*mtk_get_gpu_pmu_init_fp)(GPU_PMU *pmus, int pmu_size, int *ret_size);
extern int (*mtk_get_gpu_pmu_deinit_fp)(void);
extern int (*mtk_get_gpu_pmu_swapnreset_fp)(GPU_PMU *pmus, int pmu_size);
extern int (*mtk_get_gpu_pmu_swapnreset_stop_fp)(void);
/* Need to get current gpu freq from GPU DVFS module */
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);


int get_mali_pmu_counter(int i);
int gator_gpu_pmu_init(void);
int mtk_mfg_update_counter(void);
int find_name_pos(const char *name, int *pos);

void mtk_mfg_counter_init(void);
void mtk_mfg_counter_destroy(void);
void mali_gpu_pmu_stop(void);


#endif
