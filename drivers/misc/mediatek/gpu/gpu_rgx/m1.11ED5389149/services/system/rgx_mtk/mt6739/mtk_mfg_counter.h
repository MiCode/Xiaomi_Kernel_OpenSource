/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_MFG_COUNTER__
#define __MTK_MFG_COUNTER__

#include <mt-plat/mtk_gpu_utility.h>

/* mt6799 performance counter */
#define RGX_PERF_CONTROL                (0x6000)
#define RGX_CR_PERF_TA_PHASES           (0x00006008)
#define RGX_CR_PERF_3D_PHASES           (0x00006010)
#define RGX_CR_PERF_COMPUTE_PHASES      (0x00006018)
#define RGX_CR_PERF_TA_CYCLES           (0x00006020)
#define RGX_CR_PERF_3D_CYCLES           (0x00006028)
#define RGX_CR_PERF_COMPUTE_CYCLES      (0x00006030)
#define RGX_CR_PERF_TA_OR_3D_CYCLES     (0x00006038)
#define RGX_CR_PERF_INITIAL_TA_CYCLES   (0x00006040)
#define RGX_CR_PERF_FINAL_3D_CYCLES     (0x00006048)
#define RGX_CR_PERF_3D_SPINUP_CYCLES    (0x00006220)

#define RGX_CR_PERF_SLC0_READS          (0x000060A0)
#define RGX_CR_PERF_SLC0_WRITES         (0x000060A8)
#define RGX_CR_PERF_SLC0_BYTE_WRITES    (0x000060B0)
#define RGX_CR_PERF_SLC0_READ_STALLS    (0x000060B8)
#define RGX_CR_PERF_SLC0_WRITE_STALLS   (0x000060C0)

#define RGX_CR_PERF_SLC1_READS          (0x000060C8)
#define RGX_CR_PERF_SLC1_WRITES         (0x000060D0)
#define RGX_CR_PERF_SLC1_BYTE_WRITES    (0x000060D8)
#define RGX_CR_PERF_SLC1_READ_STALLS    (0x000060E0)
#define RGX_CR_PERF_SLC1_WRITE_STALLS   (0x000060E8)
#define RGX_CR_PERF_SLC2_READS          (0x00006140)
#define RGX_CR_PERF_SLC2_WRITES         (0x00006148)
#define RGX_CR_PERF_SLC2_BYTE_WRITES    (0x00006150)
#define RGX_CR_PERF_SLC2_READ_STALLS    (0x00006158)
#define RGX_CR_PERF_SLC2_WRITE_STALLS   (0x00006160)
#define RGX_CR_PERF_SLC3_READS          (0x00006168)
#define RGX_CR_PERF_SLC3_WRITES         (0x00006170)
#define RGX_CR_PERF_SLC3_BYTE_WRITES    (0x00006178)
#define RGX_CR_PERF_SLC3_READ_STALLS    (0x00006180)
#define RGX_CR_PERF_SLC3_WRITE_STALLS   (0x00006188)

extern int (*mtk_get_gpu_pmu_init_fp)(GPU_PMU *pmus, int pmu_size, int *ret_size);
extern int (*mtk_get_gpu_pmu_swapnreset_fp)(GPU_PMU *pmus, int pmu_size);

void mtk_mfg_counter_init(void);
void mtk_mfg_counter_destroy(void);

#endif
