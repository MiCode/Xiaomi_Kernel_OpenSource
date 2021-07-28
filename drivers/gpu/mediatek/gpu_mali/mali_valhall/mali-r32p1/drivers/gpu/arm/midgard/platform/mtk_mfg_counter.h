// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MFG_COUNTER_H_
#define __MTK_MFG_COUNTER_H_

#include <mtk_gpu_utility.h>
#include <mtk_gpufreq.h>

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

void mtk_mfg_counter_init(void);
void mtk_mfg_counter_destroy(void);
int gator_gpu_pmu_init(void);
//stall counter
int mtk_gpu_stall_create_subfs(void);
void mtk_gpu_stall_delete_subfs(void);
void mtk_gpu_stall_start(void);
void mtk_gpu_stall_stop(void);
void mtk_GPU_STALL_RAW(unsigned int *diff, int size);

//MTK PMU COUNTER
typedef enum {
	VINSTR_GPU_FREQ,
	VINSTR_GPU_VOLT,
	VINSTR_GPU_LOADING,
	VINSTR_GPU_ACTIVE,
	VINSTR_EXEC_INSTR_FMA,
	VINSTR_EXEC_INSTR_CVT,
	VINSTR_EXEC_INSTR_SFU,
	VINSTR_EXEC_INSTR_MSG,
	VINSTR_EXEC_CORE_ACTIVE,
	VINSTR_FRAG_ACTIVE,
	VINSTR_TILER_ACTIVE,
	VINSTR_VARY_SLOT_32,
	VINSTR_VARY_SLOT_16,
	VINSTR_TEX_FILT_NUM_OPERATIONS,
	VINSTR_LS_MEM_READ_FULL,
	VINSTR_LS_MEM_WRITE_FULL,
	VINSTR_LS_MEM_READ_SHORT,
	VINSTR_LS_MEM_WRITE_SHORT,
	VINSTR_L2_EXT_WRITE_BEATS,
	VINSTR_L2_EXT_READ_BEATS,
	VINSTR_L2_EXT_RRESP_0_127,
	VINSTR_L2_EXT_RRESP_128_191,
	VINSTR_L2_EXT_RRESP_192_255,
	VINSTR_L2_EXT_RRESP_256_319,
	VINSTR_L2_EXT_RRESP_320_383,
	VINSTR_L2_ANY_LOOKUP,
	VINSTR_JS0_ACTIVE,
	VINSTR_JS1_ACTIVE,
	VINSTR_STALL0,
	VINSTR_STALL1,
	VINSTR_STALL2,
	VINSTR_STALL3,
	VINSTR_TRIANGLES,
	VINSTR_POINTS,
	VINSTR_LINES,
	VINSTR_LS_MEM_ATOMIC,
	VINSTR_PERF_COUNTER_LAST
} mtk_vinstr_perf_counter;

static unsigned int gpu_pmu_index[] = {
	  [VINSTR_GPU_ACTIVE] = 0x206
	, [VINSTR_EXEC_INSTR_FMA] = 0x39B
	, [VINSTR_EXEC_INSTR_CVT] = 0x39C
	, [VINSTR_EXEC_INSTR_SFU] = 0x39D
	, [VINSTR_EXEC_INSTR_MSG] = 0x39E
	, [VINSTR_EXEC_CORE_ACTIVE] = 0x39A
	, [VINSTR_FRAG_ACTIVE] = 0x384
	, [VINSTR_TILER_ACTIVE] = 0x244
	, [VINSTR_VARY_SLOT_32] = 0x3B2
	, [VINSTR_VARY_SLOT_16] = 0x3B3
	, [VINSTR_TEX_FILT_NUM_OPERATIONS] = 0x3A7
	, [VINSTR_LS_MEM_READ_FULL] = 0x3AC
	, [VINSTR_LS_MEM_WRITE_FULL] = 0x3AE
	, [VINSTR_LS_MEM_READ_SHORT] = 0x3AD
	, [VINSTR_LS_MEM_WRITE_SHORT] = 0x3AF
	, [VINSTR_L2_EXT_WRITE_BEATS] = 0x4AF
	, [VINSTR_L2_EXT_READ_BEATS] = 0x4A0
	, [VINSTR_L2_EXT_RRESP_0_127] = 0x4A5
	, [VINSTR_L2_EXT_RRESP_128_191] = 0x4A6
	, [VINSTR_L2_EXT_RRESP_192_255] = 0x4A7
	, [VINSTR_L2_EXT_RRESP_256_319] = 0x4A8
	, [VINSTR_L2_EXT_RRESP_320_383] = 0x4A9
	, [VINSTR_L2_ANY_LOOKUP] = 0x899
	, [VINSTR_JS0_ACTIVE] = 0x20A
	, [VINSTR_JS1_ACTIVE] = 0x212
	, [VINSTR_TRIANGLES] = 0x246
	, [VINSTR_POINTS] = 0x248
	, [VINSTR_LINES] = 0x247
	, [VINSTR_LS_MEM_ATOMIC] = 0x3B0
};


#endif
