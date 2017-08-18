/* Copyright (c)2017-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/firmware.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/pm_opp.h>
#include <linux/jiffies.h>
#include <linux/clk/qcom.h>

#include "adreno.h"
#include "a6xx_reg.h"
#include "adreno_a6xx.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"
#include "adreno_perfcounter.h"
#include "adreno_ringbuffer.h"
#include "adreno_llc.h"
#include "kgsl_sharedmem.h"
#include "kgsl_log.h"
#include "kgsl.h"
#include "kgsl_hfi.h"
#include "kgsl_trace.h"
#include "kgsl_gmu.h"

#define MIN_HBB		13

static const struct adreno_vbif_data a630_vbif[] = {
	{A6XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000009},
	{A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_data a615_gbif[] = {
	{A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_data a640_gbif[] = {
	{A6XX_GBIF_QSB_SIDE0, 0x00071620},
	{A6XX_GBIF_QSB_SIDE1, 0x00071620},
	{A6XX_GBIF_QSB_SIDE2, 0x00071620},
	{A6XX_GBIF_QSB_SIDE3, 0x00071620},
	{A6XX_RBBM_GBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_platform a6xx_vbif_platforms[] = {
	{ adreno_is_a630, a630_vbif },
	{ adreno_is_a615_family, a615_gbif },
	{ adreno_is_a640, a640_gbif },
	{ adreno_is_a680, a640_gbif },
	{ adreno_is_a612, a640_gbif },
	{ adreno_is_a610, a640_gbif },
};

struct kgsl_hwcg_reg {
	unsigned int off;
	unsigned int val;
};
static const struct kgsl_hwcg_reg a630_hwcg_regs[] = {
	{A6XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_SP2, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_SP3, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_SP0, 0x02022220},
	{A6XX_RBBM_CLOCK_CNTL2_SP1, 0x02022220},
	{A6XX_RBBM_CLOCK_CNTL2_SP2, 0x02022220},
	{A6XX_RBBM_CLOCK_CNTL2_SP3, 0x02022220},
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A6XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A6XX_RBBM_CLOCK_DELAY_SP2, 0x00000080},
	{A6XX_RBBM_CLOCK_DELAY_SP3, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_HYST_SP2, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_HYST_SP3, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_CNTL_TP0, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_TP1, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_TP2, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_TP3, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP2, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP3, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP1, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP2, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP3, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_TP0, 0x00022222},
	{A6XX_RBBM_CLOCK_CNTL4_TP1, 0x00022222},
	{A6XX_RBBM_CLOCK_CNTL4_TP2, 0x00022222},
	{A6XX_RBBM_CLOCK_CNTL4_TP3, 0x00022222},
	{A6XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST_TP2, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST_TP3, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP2, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP3, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP1, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP2, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP3, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST4_TP0, 0x00077777},
	{A6XX_RBBM_CLOCK_HYST4_TP1, 0x00077777},
	{A6XX_RBBM_CLOCK_HYST4_TP2, 0x00077777},
	{A6XX_RBBM_CLOCK_HYST4_TP3, 0x00077777},
	{A6XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY_TP2, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY_TP3, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP2, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP3, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP2, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP3, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY4_TP0, 0x00011111},
	{A6XX_RBBM_CLOCK_DELAY4_TP1, 0x00011111},
	{A6XX_RBBM_CLOCK_DELAY4_TP2, 0x00011111},
	{A6XX_RBBM_CLOCK_DELAY4_TP3, 0x00011111},
	{A6XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A6XX_RBBM_CLOCK_HYST_UCHE, 0x00000004},
	{A6XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A6XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL_RB2, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL_RB3, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_RB0, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL2_RB1, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL2_RB2, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL2_RB3, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL_CCU0, 0x00002220},
	{A6XX_RBBM_CLOCK_CNTL_CCU1, 0x00002220},
	{A6XX_RBBM_CLOCK_CNTL_CCU2, 0x00002220},
	{A6XX_RBBM_CLOCK_CNTL_CCU3, 0x00002220},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU0, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU1, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU2, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU3, 0x00040F00},
	{A6XX_RBBM_CLOCK_CNTL_RAC, 0x05022022},
	{A6XX_RBBM_CLOCK_CNTL2_RAC, 0x00005555},
	{A6XX_RBBM_CLOCK_DELAY_RAC, 0x00000011},
	{A6XX_RBBM_CLOCK_HYST_RAC, 0x00445044},
	{A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x00222222},
	{A6XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A6XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A6XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A6XX_RBBM_CLOCK_DELAY_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ_2, 0x00000002},
	{A6XX_RBBM_CLOCK_MODE_HLSQ, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL_GMU_GX, 0x00000222},
	{A6XX_RBBM_CLOCK_DELAY_GMU_GX, 0x00000111},
	{A6XX_RBBM_CLOCK_HYST_GMU_GX, 0x00000555}
};

static const struct kgsl_hwcg_reg a615_hwcg_regs[] = {
	{A6XX_RBBM_CLOCK_CNTL_SP0,  0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP0,  0x0000F3CF},
	{A6XX_RBBM_CLOCK_CNTL_TP0,  0x02222222},
	{A6XX_RBBM_CLOCK_CNTL_TP1,  0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP1, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_TP0, 0x00022222},
	{A6XX_RBBM_CLOCK_CNTL4_TP1, 0x00022222},
	{A6XX_RBBM_CLOCK_HYST_TP0,  0x77777777},
	{A6XX_RBBM_CLOCK_HYST_TP1,  0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP1, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST4_TP0, 0x00077777},
	{A6XX_RBBM_CLOCK_HYST4_TP1, 0x00077777},
	{A6XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP1, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY4_TP0, 0x00011111},
	{A6XX_RBBM_CLOCK_DELAY4_TP1, 0x00011111},
	{A6XX_RBBM_CLOCK_CNTL_UCHE,  0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A6XX_RBBM_CLOCK_HYST_UCHE,  0x00000004},
	{A6XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A6XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_RB0, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL_CCU0, 0x00002020},
	{A6XX_RBBM_CLOCK_CNTL_CCU1, 0x00002220},
	{A6XX_RBBM_CLOCK_CNTL_CCU2, 0x00002220},
	{A6XX_RBBM_CLOCK_CNTL_CCU3, 0x00002220},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU0, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU1, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU2, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU3, 0x00040F00},
	{A6XX_RBBM_CLOCK_CNTL_RAC, 0x05022022},
	{A6XX_RBBM_CLOCK_CNTL2_RAC, 0x00005555},
	{A6XX_RBBM_CLOCK_DELAY_RAC, 0x00000011},
	{A6XX_RBBM_CLOCK_HYST_RAC, 0x00445044},
	{A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x00222222},
	{A6XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A6XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A6XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A6XX_RBBM_CLOCK_DELAY_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ_2, 0x00000002},
	{A6XX_RBBM_CLOCK_MODE_HLSQ, 0x00002222},
	{A6XX_RBBM_CLOCK_CNTL_GMU_GX, 0x00000222},
	{A6XX_RBBM_CLOCK_DELAY_GMU_GX, 0x00000111},
	{A6XX_RBBM_CLOCK_HYST_GMU_GX, 0x00000555}
};

static const struct kgsl_hwcg_reg a640_hwcg_regs[] = {
	{A6XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_CNTL_TP0, 0x02222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_TP0, 0x00022222},
	{A6XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY4_TP0, 0x00011111},
	{A6XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST4_TP0, 0x00077777},
	{A6XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_RB0, 0x01002222},
	{A6XX_RBBM_CLOCK_CNTL_CCU0, 0x00002220},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU0, 0x00040F00},
	{A6XX_RBBM_CLOCK_CNTL_RAC, 0x05222022},
	{A6XX_RBBM_CLOCK_CNTL2_RAC, 0x00005555},
	{A6XX_RBBM_CLOCK_DELAY_RAC, 0x00000011},
	{A6XX_RBBM_CLOCK_HYST_RAC, 0x00445044},
	{A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A6XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x00222222},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ_2, 0x00000002},
	{A6XX_RBBM_CLOCK_MODE_HLSQ, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A6XX_RBBM_CLOCK_DELAY_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A6XX_RBBM_CLOCK_HYST_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_TEX_FCHE, 0x00000222},
	{A6XX_RBBM_CLOCK_DELAY_TEX_FCHE, 0x00000111},
	{A6XX_RBBM_CLOCK_HYST_TEX_FCHE, 0x00000777},
	{A6XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_HYST_UCHE, 0x00000004},
	{A6XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A6XX_RBBM_ISDB_CNT, 0x00000182},
	{A6XX_RBBM_RAC_THRESHOLD_CNT, 0x00000000},
	{A6XX_RBBM_SP_HYST_CNT, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_GMU_GX, 0x00000222},
	{A6XX_RBBM_CLOCK_DELAY_GMU_GX, 0x00000111},
	{A6XX_RBBM_CLOCK_HYST_GMU_GX, 0x00000555},
};

static const struct kgsl_hwcg_reg a612_hwcg_regs[] = {
	{A6XX_RBBM_CLOCK_CNTL_SP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x00000081},
	{A6XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL3_TP0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL4_TP0, 0x00022222},
	{A6XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY3_TP0, 0x11111111},
	{A6XX_RBBM_CLOCK_DELAY4_TP0, 0x00011111},
	{A6XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST3_TP0, 0x77777777},
	{A6XX_RBBM_CLOCK_HYST4_TP0, 0x00077777},
	{A6XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_RB0, 0x01202222},
	{A6XX_RBBM_CLOCK_CNTL_CCU0, 0x00002220},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU0, 0x00040F00},
	{A6XX_RBBM_CLOCK_CNTL_RAC, 0x05522022},
	{A6XX_RBBM_CLOCK_CNTL2_RAC, 0x00005555},
	{A6XX_RBBM_CLOCK_DELAY_RAC, 0x00000011},
	{A6XX_RBBM_CLOCK_HYST_RAC, 0x00445044},
	{A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A6XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ_2, 0x00000002},
	{A6XX_RBBM_CLOCK_MODE_HLSQ, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A6XX_RBBM_CLOCK_DELAY_VFD, 0x00002222},
	{A6XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A6XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A6XX_RBBM_CLOCK_HYST_HLSQ, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A6XX_RBBM_CLOCK_HYST_UCHE, 0x00000004},
	{A6XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A6XX_RBBM_ISDB_CNT, 0x00000182},
	{A6XX_RBBM_RAC_THRESHOLD_CNT, 0x00000000},
	{A6XX_RBBM_SP_HYST_CNT, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_GMU_GX, 0x00000222},
	{A6XX_RBBM_CLOCK_DELAY_GMU_GX, 0x00000111},
	{A6XX_RBBM_CLOCK_HYST_GMU_GX, 0x00000555},
};

static const struct {
	int (*devfunc)(struct adreno_device *adreno_dev);
	const struct kgsl_hwcg_reg *regs;
	unsigned int count;
} a6xx_hwcg_registers[] = {
	{adreno_is_a630, a630_hwcg_regs, ARRAY_SIZE(a630_hwcg_regs)},
	{adreno_is_a615_family, a615_hwcg_regs, ARRAY_SIZE(a615_hwcg_regs)},
	{adreno_is_a640, a640_hwcg_regs, ARRAY_SIZE(a640_hwcg_regs)},
	{adreno_is_a680, a640_hwcg_regs, ARRAY_SIZE(a640_hwcg_regs)},
	{adreno_is_a612, a612_hwcg_regs, ARRAY_SIZE(a612_hwcg_regs)},
	{adreno_is_a610, a612_hwcg_regs, ARRAY_SIZE(a612_hwcg_regs)},
};

static struct a6xx_protected_regs {
	unsigned int base;
	unsigned int count;
	int read_protect;
} a6xx_protected_regs_group[] = {
	{ 0x600, 0x51, 0 },
	{ 0xAE50, 0x2, 1 },
	{ 0x9624, 0x13, 1 },
	{ 0x8630, 0x8, 1 },
	{ 0x9E70, 0x1, 1 },
	{ 0x9E78, 0x187, 1 },
	{ 0xF000, 0x810, 1 },
	{ 0xFC00, 0x3, 0 },
	{ 0x50E, 0x0, 1 },
	{ 0x50F, 0x0, 0 },
	{ 0x510, 0x0, 1 },
	{ 0x0, 0x4F9, 0 },
	{ 0x501, 0xA, 0 },
	{ 0x511, 0x44, 0 },
	{ 0xE00, 0x1, 1 },
	{ 0xE03, 0xB, 1 },
	{ 0x8E00, 0x0, 1 },
	{ 0x8E50, 0xF, 1 },
	{ 0xBE02, 0x0, 1 },
	{ 0xBE20, 0x11F3, 1 },
	{ 0x800, 0x82, 1 },
	{ 0x8A0, 0x8, 1 },
	{ 0x8AB, 0x19, 1 },
	{ 0x900, 0x4D, 1 },
	{ 0x98D, 0x76, 1 },
	{ 0x8D0, 0x23, 0 },
	{ 0x980, 0x4, 0 },
	{ 0xA630, 0x0, 1 },
};

/* IFPC & Preemption static powerup restore list */
static struct reg_list_pair {
	uint32_t offset;
	uint32_t val;
} a6xx_pwrup_reglist[] = {
	{ A6XX_VSC_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_GRAS_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_RB_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_PC_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_HLSQ_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_VFD_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_VPC_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_UCHE_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_SP_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_TPL1_ADDR_MODE_CNTL, 0x0 },
	{ A6XX_UCHE_WRITE_RANGE_MAX_LO, 0x0 },
	{ A6XX_UCHE_WRITE_RANGE_MAX_HI, 0x0 },
	{ A6XX_UCHE_TRAP_BASE_LO, 0x0 },
	{ A6XX_UCHE_TRAP_BASE_HI, 0x0 },
	{ A6XX_UCHE_WRITE_THRU_BASE_LO, 0x0 },
	{ A6XX_UCHE_WRITE_THRU_BASE_HI, 0x0 },
	{ A6XX_UCHE_GMEM_RANGE_MIN_LO, 0x0 },
	{ A6XX_UCHE_GMEM_RANGE_MIN_HI, 0x0 },
	{ A6XX_UCHE_GMEM_RANGE_MAX_LO, 0x0 },
	{ A6XX_UCHE_GMEM_RANGE_MAX_HI, 0x0 },
	{ A6XX_UCHE_FILTER_CNTL, 0x0 },
	{ A6XX_UCHE_CACHE_WAYS, 0x0 },
	{ A6XX_UCHE_MODE_CNTL, 0x0 },
	{ A6XX_RB_NC_MODE_CNTL, 0x0 },
	{ A6XX_TPL1_NC_MODE_CNTL, 0x0 },
	{ A6XX_SP_NC_MODE_CNTL, 0x0 },
	{ A6XX_PC_DBG_ECO_CNTL, 0x0 },
	{ A6XX_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE, 0x0 },
};

/* IFPC only static powerup restore list */
static struct reg_list_pair a6xx_ifpc_pwrup_reglist[] = {
	{ A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x0 },
	{ A6XX_RBBM_GBIF_CLIENT_QOS_CNTL, 0x0 },
	{ A6XX_CP_CHICKEN_DBG, 0x0 },
	{ A6XX_CP_DBG_ECO_CNTL, 0x0 },
	{ A6XX_CP_PROTECT_CNTL, 0x0 },
	{ A6XX_CP_PROTECT_REG, 0x0 },
	{ A6XX_CP_PROTECT_REG+1, 0x0 },
	{ A6XX_CP_PROTECT_REG+2, 0x0 },
	{ A6XX_CP_PROTECT_REG+3, 0x0 },
	{ A6XX_CP_PROTECT_REG+4, 0x0 },
	{ A6XX_CP_PROTECT_REG+5, 0x0 },
	{ A6XX_CP_PROTECT_REG+6, 0x0 },
	{ A6XX_CP_PROTECT_REG+7, 0x0 },
	{ A6XX_CP_PROTECT_REG+8, 0x0 },
	{ A6XX_CP_PROTECT_REG+9, 0x0 },
	{ A6XX_CP_PROTECT_REG+10, 0x0 },
	{ A6XX_CP_PROTECT_REG+11, 0x0 },
	{ A6XX_CP_PROTECT_REG+12, 0x0 },
	{ A6XX_CP_PROTECT_REG+13, 0x0 },
	{ A6XX_CP_PROTECT_REG+14, 0x0 },
	{ A6XX_CP_PROTECT_REG+15, 0x0 },
	{ A6XX_CP_PROTECT_REG+16, 0x0 },
	{ A6XX_CP_PROTECT_REG+17, 0x0 },
	{ A6XX_CP_PROTECT_REG+18, 0x0 },
	{ A6XX_CP_PROTECT_REG+19, 0x0 },
	{ A6XX_CP_PROTECT_REG+20, 0x0 },
	{ A6XX_CP_PROTECT_REG+21, 0x0 },
	{ A6XX_CP_PROTECT_REG+22, 0x0 },
	{ A6XX_CP_PROTECT_REG+23, 0x0 },
	{ A6XX_CP_PROTECT_REG+24, 0x0 },
	{ A6XX_CP_PROTECT_REG+25, 0x0 },
	{ A6XX_CP_PROTECT_REG+26, 0x0 },
	{ A6XX_CP_PROTECT_REG+27, 0x0 },
	{ A6XX_CP_PROTECT_REG+28, 0x0 },
	{ A6XX_CP_PROTECT_REG+29, 0x0 },
	{ A6XX_CP_PROTECT_REG+30, 0x0 },
	{ A6XX_CP_PROTECT_REG+31, 0x0 },
	{ A6XX_CP_AHB_CNTL, 0x0 },
};

static struct reg_list_pair a615_pwrup_reglist[] = {
	{ A6XX_UCHE_GBIF_GX_CONFIG, 0x0 },
};

static struct reg_list_pair a6xx_ifpc_perfctr_reglist[] = {
	{ A6XX_RBBM_PERFCTR_CNTL, 0x0 },
};

static void _update_always_on_regs(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int *const regs = gpudev->reg_offsets->offsets;

	regs[ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO] =
		A6XX_CP_ALWAYS_ON_COUNTER_LO;
	regs[ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI] =
		A6XX_CP_ALWAYS_ON_COUNTER_HI;
}

static void a6xx_pwrup_reglist_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (kgsl_allocate_global(device, &adreno_dev->pwrup_reglist,
		PAGE_SIZE, 0, KGSL_MEMDESC_CONTIG | KGSL_MEMDESC_PRIVILEGED,
		"powerup_register_list")) {
		adreno_dev->pwrup_reglist.gpuaddr = 0;
		return;
	}

	kgsl_sharedmem_set(device, &adreno_dev->pwrup_reglist, 0, 0,
		PAGE_SIZE);
}

static void a6xx_init(struct adreno_device *adreno_dev)
{
	a6xx_crashdump_init(adreno_dev);

	/*
	 * If the GMU is not enabled, rewrite the offset for the always on
	 * counters to point to the CP always on instead of GMU always on
	 */
	if (!gmu_core_isenabled(KGSL_DEVICE(adreno_dev)))
		_update_always_on_regs(adreno_dev);

	a6xx_pwrup_reglist_init(adreno_dev);
}

/**
 * a6xx_protect_init() - Initializes register protection on a6xx
 * @device: Pointer to the device structure
 * Performs register writes to enable protected access to sensitive
 * registers
 */
static void a6xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_protected_registers *mmu_prot =
		kgsl_mmu_get_prot_regs(&device->mmu);
	int i, num_sets;
	int req_sets = ARRAY_SIZE(a6xx_protected_regs_group);
	int max_sets = adreno_dev->gpucore->num_protected_regs;
	unsigned int mmu_base = 0, mmu_range = 0, cur_range;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A6XX_CP_PROTECT_CNTL, 0x00000003);

	if (mmu_prot) {
		mmu_base = mmu_prot->base;
		mmu_range = mmu_prot->range;
		req_sets += DIV_ROUND_UP(mmu_range, 0x2000);
	}

	if (req_sets > max_sets)
		WARN(1, "Size exceeds the num of protection regs available\n");

	/* Protect GPU registers */
	num_sets = min_t(unsigned int,
		ARRAY_SIZE(a6xx_protected_regs_group), max_sets);
	for (i = 0; i < num_sets; i++) {
		struct a6xx_protected_regs *regs =
					&a6xx_protected_regs_group[i];

		kgsl_regwrite(device, A6XX_CP_PROTECT_REG + i,
				regs->base | (regs->count << 18) |
				(regs->read_protect << 31));
	}

	/* Protect MMU registers */
	if (mmu_prot) {
		while ((i < max_sets) && (mmu_range > 0)) {
			cur_range = min_t(unsigned int, mmu_range,
						0x2000);
			kgsl_regwrite(device, A6XX_CP_PROTECT_REG + i,
				mmu_base | ((cur_range - 1) << 18) | (1 << 31));

			mmu_base += cur_range;
			mmu_range -= cur_range;
			i++;
		}
	}
}

static void a6xx_enable_64bit(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, A6XX_CP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VSC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_GRAS_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_RB_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_PC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_HLSQ_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VFD_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VPC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_UCHE_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_SP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_TPL1_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL, 0x1);
}

static inline unsigned int
__get_rbbm_clock_cntl_on(struct adreno_device *adreno_dev)
{
	if (adreno_is_a630(adreno_dev))
		return 0x8AA8AA02;
	else if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev))
		return 0xAAA8AA82;
	else
		return 0x8AA8AA82;
}

static inline unsigned int
__get_gmu_ao_cgc_mode_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000022;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000222;
	else
		return 0x00020202;
}

static inline unsigned int
__get_gmu_ao_cgc_delay_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000011;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000111;
	else
		return 0x00010111;
}

static inline unsigned int
__get_gmu_ao_cgc_hyst_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000055;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000555;
	else
		return 0x00005555;
}

static void a6xx_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct kgsl_hwcg_reg *regs;
	unsigned int value;
	int i, j;

	if (!test_bit(ADRENO_HWCG_CTRL, &adreno_dev->pwrctrl_flag))
		on = false;

	if (gmu_core_isenabled(device)) {
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL,
			on ? __get_gmu_ao_cgc_mode_cntl(adreno_dev) : 0);
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,
			on ? __get_gmu_ao_cgc_delay_cntl(adreno_dev) : 0);
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL,
			on ? __get_gmu_ao_cgc_hyst_cntl(adreno_dev) : 0);
	}

	kgsl_regread(device, A6XX_RBBM_CLOCK_CNTL, &value);

	if (value == __get_rbbm_clock_cntl_on(adreno_dev) && on)
		return;

	if (value == 0 && !on)
		return;

	for (i = 0; i < ARRAY_SIZE(a6xx_hwcg_registers); i++) {
		if (a6xx_hwcg_registers[i].devfunc(adreno_dev))
			break;
	}

	if (i == ARRAY_SIZE(a6xx_hwcg_registers))
		return;

	regs = a6xx_hwcg_registers[i].regs;

	/*
	 * Disable SP clock before programming HWCG registers.
	 * A612 and A610 GPU is not having the GX power domain.
	 * Hence skip GMU_GX registers for A612 and A610.
	 */

	if (gmu_core_isenabled(device) && !adreno_is_a612(adreno_dev) &&
		!adreno_is_a610(adreno_dev))
		gmu_core_regrmw(device,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 1, 0);

	for (j = 0; j < a6xx_hwcg_registers[i].count; j++)
		kgsl_regwrite(device, regs[j].off, on ? regs[j].val : 0);

	/*
	 * Enable SP clock after programming HWCG registers.
	 * A612 and A610 GPU is not having the GX power domain.
	 * Hence skip GMU_GX registers for A612 and A610.
	 */
	if (gmu_core_isenabled(device) && !adreno_is_a612(adreno_dev) &&
		!adreno_is_a610(adreno_dev))
		gmu_core_regrmw(device,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 0, 1);

	/* enable top level HWCG */
	kgsl_regwrite(device, A6XX_RBBM_CLOCK_CNTL,
		on ? __get_rbbm_clock_cntl_on(adreno_dev) : 0);
}

static void a6xx_patch_pwrup_reglist(struct adreno_device *adreno_dev)
{
	uint32_t i;
	struct cpu_gpu_lock *lock;
	struct reg_list_pair *r;

	/* Set up the register values */
	for (i = 0; i < ARRAY_SIZE(a6xx_ifpc_pwrup_reglist); i++) {
		r = &a6xx_ifpc_pwrup_reglist[i];
		kgsl_regread(KGSL_DEVICE(adreno_dev), r->offset, &r->val);
	}

	for (i = 0; i < ARRAY_SIZE(a6xx_pwrup_reglist); i++) {
		r = &a6xx_pwrup_reglist[i];
		kgsl_regread(KGSL_DEVICE(adreno_dev), r->offset, &r->val);
	}

	lock = (struct cpu_gpu_lock *) adreno_dev->pwrup_reglist.hostptr;
	lock->flag_ucode = 0;
	lock->flag_kmd = 0;
	lock->turn = 0;

	/*
	 * The overall register list is composed of
	 * 1. Static IFPC-only registers
	 * 2. Static IFPC + preemption registers
	 * 2. Dynamic IFPC + preemption registers (ex: perfcounter selects)
	 *
	 * The CP views the second and third entries as one dynamic list
	 * starting from list_offset. Thus, list_length should be the sum
	 * of all three lists above (of which the third list will start off
	 * empty). And list_offset should be specified as the size in dwords
	 * of the static IFPC-only register list.
	 */
	lock->list_length = (sizeof(a6xx_ifpc_pwrup_reglist) +
			sizeof(a6xx_pwrup_reglist)) >> 2;
	lock->list_offset = sizeof(a6xx_ifpc_pwrup_reglist) >> 2;

	memcpy(adreno_dev->pwrup_reglist.hostptr + sizeof(*lock),
		a6xx_ifpc_pwrup_reglist, sizeof(a6xx_ifpc_pwrup_reglist));
	memcpy(adreno_dev->pwrup_reglist.hostptr + sizeof(*lock)
		+ sizeof(a6xx_ifpc_pwrup_reglist), a6xx_pwrup_reglist,
		sizeof(a6xx_pwrup_reglist));

	if (adreno_is_a615_family(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(a615_pwrup_reglist); i++) {
			r = &a615_pwrup_reglist[i];
			kgsl_regread(KGSL_DEVICE(adreno_dev),
				r->offset, &r->val);
		}

		memcpy(adreno_dev->pwrup_reglist.hostptr + sizeof(*lock)
			+ sizeof(a6xx_ifpc_pwrup_reglist)
			+ sizeof(a6xx_pwrup_reglist), a615_pwrup_reglist,
			sizeof(a615_pwrup_reglist));

		lock->list_length += sizeof(a615_pwrup_reglist) >> 2;
	}

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PERFCTRL_RETAIN)) {
		for (i = 0; i < ARRAY_SIZE(a6xx_ifpc_perfctr_reglist); i++) {
			r = &a6xx_ifpc_perfctr_reglist[i];
			kgsl_regread(KGSL_DEVICE(adreno_dev),
				r->offset, &r->val);
		}

		memcpy(adreno_dev->pwrup_reglist.hostptr + sizeof(*lock)
				+ sizeof(a6xx_ifpc_pwrup_reglist)
				+ sizeof(a6xx_pwrup_reglist),
				a6xx_ifpc_perfctr_reglist,
				sizeof(a6xx_ifpc_perfctr_reglist));

		lock->list_length += sizeof(a6xx_ifpc_perfctr_reglist) >> 2;
	}
}

/*
 * a6xx_start() - Device start
 * @adreno_dev: Pointer to adreno device
 *
 * a6xx device start
 */
static void a6xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	unsigned int bit, mal, mode, glbl_inv, channel;
	unsigned int amsbc = 0;
	static bool patch_reglist;

	/* runtime adjust callbacks based on feature sets */
	if (!gmu_core_gpmu_isenabled(device))
		/* Legacy idle management if gmu is disabled */
		ADRENO_GPU_DEVICE(adreno_dev)->hw_isidle = NULL;
	/* enable hardware clockgating */
	a6xx_hwcg_set(adreno_dev, true);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		adreno_dev->lm_threshold_count = A6XX_GMU_GENERAL_1;

	adreno_vbif_start(adreno_dev, a6xx_vbif_platforms,
			ARRAY_SIZE(a6xx_vbif_platforms));

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_LIMIT_UCHE_GBIF_RW))
		kgsl_regwrite(device, A6XX_UCHE_GBIF_GX_CONFIG, 0x10200F9);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, A6XX_UCHE_WRITE_RANGE_MAX_LO, 0xffffffc0);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_RANGE_MAX_HI, 0x0001ffff);
	kgsl_regwrite(device, A6XX_UCHE_TRAP_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, A6XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_THRU_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/* Program the GMEM VA range for the UCHE path */
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_LO,
				adreno_dev->uche_gmem_base);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_LO,
				adreno_dev->uche_gmem_base +
				adreno_dev->gmem_size - 1);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	kgsl_regwrite(device, A6XX_UCHE_FILTER_CNTL, 0x804);
	kgsl_regwrite(device, A6XX_UCHE_CACHE_WAYS, 0x4);

	/* ROQ sizes are twice as big on a640/a680 than on a630 */
	if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x02000140);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);
	} else if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x00800060);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x40201b16);
	} else {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x010000C0);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);
	}

	if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev)) {
		/* For A612 and A610 Mem pool size is reduced to 48 */
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 48);
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_DBG_ADDR, 47);
	} else {
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 128);
	}

	/* Setting the primFifo thresholds values */
	if (adreno_is_a640(adreno_dev))
		kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x400 << 11));
	else if (adreno_is_a680(adreno_dev))
		kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x800 << 11));
	else if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev))
		kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x100 << 11));
	else
		kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x300 << 11));

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, A6XX_CP_AHB_CNTL, 0x1);

	/* Turn on performance counters */
	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_CNTL, 0x1);

	/* Turn on GX_MEM retention */
	if (gmu_core_gpmu_isenabled(device) && adreno_is_a612(adreno_dev)) {
		kgsl_regwrite(device, A6XX_RBBM_BLOCK_GX_RETENTION_CNTL, 0x7FB);
		/* For CP IPC interrupt */
		kgsl_regwrite(device, A6XX_RBBM_INT_2_MASK, 0x00000010);
	}

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,highest-bank-bit", &bit))
		bit = MIN_HBB;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,min-access-length", &mal))
		mal = 32;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,ubwc-mode", &mode))
		mode = 0;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,macrotiling-channels", &channel))
		channel = 0; /* unknown and keep reset value */

	switch (mode) {
	case KGSL_UBWC_1_0:
		mode = 1;
		break;
	case KGSL_UBWC_2_0:
		mode = 0;
		break;
	case KGSL_UBWC_3_0:
		mode = 0;
		amsbc = 1; /* Only valid for A640 and A680 */
		break;
	default:
		break;
	}

	if (channel == 8)
		kgsl_regwrite(device, A6XX_RBBM_NC_MODE_CNTL, 1);

	if (bit >= 13 && bit <= 16)
		bit = (bit - 13) & 0x03;
	else
		bit = 0;

	mal = (mal == 64) ? 1 : 0;

	/* (1 << 29)globalInvFlushFilterDis bit needs to be set for A630 V1 */
	glbl_inv = (adreno_is_a630v1(adreno_dev)) ? 1 : 0;

	kgsl_regwrite(device, A6XX_RB_NC_MODE_CNTL, (amsbc << 4) | (mal << 3) |
							(bit << 1) | mode);
	kgsl_regwrite(device, A6XX_TPL1_NC_MODE_CNTL, (mal << 3) |
							(bit << 1) | mode);
	kgsl_regwrite(device, A6XX_SP_NC_MODE_CNTL, (mal << 3) | (bit << 1) |
								mode);

	kgsl_regwrite(device, A6XX_UCHE_MODE_CNTL, (glbl_inv << 29) |
						(mal << 23) | (bit << 21));

	if (adreno_is_a610(adreno_dev))
		/*
		 * Set hang detection threshold to 4 million
		 * cycles (0x3FFFF*16).
		 */
		kgsl_regwrite(device, A6XX_RBBM_INTERFACE_HANG_INT_CNTL,
						(1 << 30) | 0x3ffff);
	else
		/* Set hang detection threshold to 0xCFFFFF * 16 cycles */
		kgsl_regwrite(device, A6XX_RBBM_INTERFACE_HANG_INT_CNTL,
						(1 << 30) | 0xcfffff);

	kgsl_regwrite(device, A6XX_UCHE_CLIENT_PF, 1);

	/* Set TWOPASSUSEWFI in A6XX_PC_DBG_ECO_CNTL if requested */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_TWO_PASS_USE_WFI))
		kgsl_regrmw(device, A6XX_PC_DBG_ECO_CNTL, 0, (1 << 8));

	/* Enable the GMEM save/restore feature for preemption */
	if (adreno_is_preemption_enabled(adreno_dev))
		kgsl_regwrite(device, A6XX_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
			0x1);

	a6xx_protect_init(adreno_dev);

	if (!patch_reglist && (adreno_dev->pwrup_reglist.gpuaddr != 0)) {
		a6xx_patch_pwrup_reglist(adreno_dev);
		patch_reglist = true;
	}

	a6xx_preemption_start(adreno_dev);

	/*
	 * We start LM here because we want all the following to be up
	 * 1. GX HS
	 * 2. SPTPRAC
	 * 3. HFI
	 * At this point, we are guaranteed all.
	 */
	if (GMU_DEV_OP_VALID(gmu_dev_ops, enable_lm))
		gmu_dev_ops->enable_lm(device);
}

/*
 * a6xx_microcode_load() - Load microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_microcode_load(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	uint64_t gpuaddr;
	int ret = 0;

	gpuaddr = fw->memdesc.gpuaddr;
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	/*
	 * Do not invoke to load zap shader if MMU does
	 * not support secure mode.
	 */
	if (!device->mmu.secured)
		return 0;

	/* Load the zap shader firmware through PIL if its available */
	if (adreno_dev->gpucore->zap_name && !adreno_dev->zap_handle_ptr) {
		adreno_dev->zap_handle_ptr =
				subsystem_get(adreno_dev->gpucore->zap_name);

		/* Return error if the zap shader cannot be loaded */
		if (IS_ERR_OR_NULL(adreno_dev->zap_handle_ptr)) {
			ret = (adreno_dev->zap_handle_ptr == NULL) ?
				-ENODEV : PTR_ERR(adreno_dev->zap_handle_ptr);
			adreno_dev->zap_handle_ptr = NULL;
		}
	}

	return ret;
}

static void a6xx_zap_shader_unload(struct adreno_device *adreno_dev)
{
	if (!IS_ERR_OR_NULL(adreno_dev->zap_handle_ptr)) {
		subsystem_put(adreno_dev->zap_handle_ptr);
		adreno_dev->zap_handle_ptr = NULL;
	}
}

/*
 * CP_INIT_MAX_CONTEXT bit tells if the multiple hardware contexts can
 * be used at once of if they should be serialized
 */
#define CP_INIT_MAX_CONTEXT BIT(0)

/* Enables register protection mode */
#define CP_INIT_ERROR_DETECTION_CONTROL BIT(1)

/* Header dump information */
#define CP_INIT_HEADER_DUMP BIT(2) /* Reserved */

/* Default Reset states enabled for PFP and ME */
#define CP_INIT_DEFAULT_RESET_STATE BIT(3)

/* Drawcall filter range */
#define CP_INIT_DRAWCALL_FILTER_RANGE BIT(4)

/* Ucode workaround masks */
#define CP_INIT_UCODE_WORKAROUND_MASK BIT(5)

/*
 * Operation mode mask
 *
 * This ordinal provides the option to disable the
 * save/restore of performance counters across preemption.
 */
#define CP_INIT_OPERATION_MODE_MASK BIT(6)

/* Register initialization list */
#define CP_INIT_REGISTER_INIT_LIST BIT(7)

/* Register initialization list with spinlock */
#define CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK BIT(8)

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK | \
		CP_INIT_OPERATION_MODE_MASK | \
		CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK)

static void _set_ordinals(struct adreno_device *adreno_dev,
		unsigned int *cmds, unsigned int count)
{
	unsigned int *start = cmds;

	/* Enabled ordinal mask */
	*cmds++ = CP_INIT_MASK;

	if (CP_INIT_MASK & CP_INIT_MAX_CONTEXT)
		*cmds++ = 0x00000003;

	if (CP_INIT_MASK & CP_INIT_ERROR_DETECTION_CONTROL)
		*cmds++ = 0x20000000;

	if (CP_INIT_MASK & CP_INIT_HEADER_DUMP) {
		/* Header dump address */
		*cmds++ = 0x00000000;
		/* Header dump enable and dump size */
		*cmds++ = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_DRAWCALL_FILTER_RANGE) {
		/* Start range */
		*cmds++ = 0x00000000;
		/* End range (inclusive) */
		*cmds++ = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_UCODE_WORKAROUND_MASK)
		*cmds++ = 0x00000000;

	if (CP_INIT_MASK & CP_INIT_OPERATION_MODE_MASK)
		*cmds++ = 0x00000002;

	if (CP_INIT_MASK & CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK) {
		uint64_t gpuaddr = adreno_dev->pwrup_reglist.gpuaddr;

		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);
		*cmds++ =  0;

	} else if (CP_INIT_MASK & CP_INIT_REGISTER_INIT_LIST) {
		uint64_t gpuaddr = adreno_dev->pwrup_reglist.gpuaddr;

		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);
		/* Size is in dwords */
		*cmds++ = (sizeof(a6xx_ifpc_pwrup_reglist) +
			sizeof(a6xx_pwrup_reglist)) >> 2;
	}

	/* Pad rest of the cmds with 0's */
	while ((unsigned int)(cmds - start) < count)
		*cmds++ = 0x0;
}

/*
 * a6xx_send_cp_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
static int a6xx_send_cp_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 12);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_type7_packet(CP_ME_INIT, 11);

	_set_ordinals(adreno_dev, cmds, 11);

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret) {
		adreno_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");

		if (!adreno_is_a3xx(adreno_dev))
			kgsl_sharedmem_writel(device, &device->scratch,
					SCRATCH_RPTR_OFFSET(rb->id), 0);
		rb->wptr = 0;
		rb->_wptr = 0;
	}

	return ret;
}

/*
 * Follow the ME_INIT sequence with a preemption yield to allow the GPU to move
 * to a different ringbuffer, if desired
 */
static int _preemption_init(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, unsigned int *cmds,
		struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;

	/* Turn CP protection OFF */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 6);
	*cmds++ = 1;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->preemption_desc.gpuaddr);

	*cmds++ = 2;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->secure_preemption_desc.gpuaddr);

	/* Turn CP protection ON */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 1;

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);
	*cmds++ = 0;
	/* generate interrupt on preemption completion */
	*cmds++ = 0;

	return cmds - cmds_orig;
}

static int a6xx_post_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int *cmds, *start;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	cmds = adreno_ringbuffer_allocspace(rb, 42);
	if (IS_ERR(cmds)) {
		KGSL_DRV_ERR(device, "error allocating preemption init cmds");
		return PTR_ERR(cmds);
	}
	start = cmds;

	cmds += _preemption_init(adreno_dev, rb, cmds, NULL);

	rb->_wptr = rb->_wptr - (42 - (cmds - start));

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret)
		adreno_spin_idle_debug(adreno_dev,
			"hw preemption initialization failed to idle\n");

	return ret;
}

/*
 * a6xx_rb_start() - Start the ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @start_type: Warm or cold start
 */
static int a6xx_rb_start(struct adreno_device *adreno_dev,
			 unsigned int start_type)
{
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	uint64_t addr;
	int ret;

	addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);

	adreno_writereg64(adreno_dev, ADRENO_REG_CP_RB_RPTR_ADDR_LO,
				ADRENO_REG_CP_RB_RPTR_ADDR_HI, addr);

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL,
					A6XX_CP_RB_CNTL_DEFAULT);

	adreno_writereg64(adreno_dev, ADRENO_REG_CP_RB_BASE,
			ADRENO_REG_CP_RB_BASE_HI, rb->buffer_desc.gpuaddr);

	ret = a6xx_microcode_load(adreno_dev);
	if (ret)
		return ret;

	/* Clear the SQE_HALT to start the CP engine */
	kgsl_regwrite(device, A6XX_CP_SQE_CNTL, 1);

	ret = a6xx_send_cp_init(adreno_dev, rb);
	if (ret)
		return ret;

	/* GPU comes up in secured mode, make it unsecured by default */
	ret = adreno_set_unsecured_mode(adreno_dev, rb);
	if (ret)
		return ret;

	return a6xx_post_start(adreno_dev);
}

/*
 * a6xx_sptprac_enable() - Power on SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
static int a6xx_sptprac_enable(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev))
		return 0;

	return a6xx_gmu_sptprac_enable(adreno_dev);
}

/*
 * a6xx_sptprac_disable() - Power off SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
static void a6xx_sptprac_disable(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev))
		return;

	a6xx_gmu_sptprac_disable(adreno_dev);
}

/*
 * a6xx_sptprac_is_on() - Check if SPTP is on using pwr status register
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
bool a6xx_sptprac_is_on(struct adreno_device *adreno_dev)
{
	if (!adreno_has_sptprac_gdsc(adreno_dev))
		return true;

	return a6xx_gmu_sptprac_is_on(adreno_dev);
}

unsigned int a6xx_set_marker(
		unsigned int *cmds, enum adreno_cp_marker_type type)
{
	unsigned int cmd = 0;

	*cmds++ = cp_type7_packet(CP_SET_MARKER, 1);

	/*
	 * Indicate the beginning and end of the IB1 list with a SET_MARKER.
	 * Among other things, this will implicitly enable and disable
	 * preemption respectively. IFPC can also be disabled and enabled
	 * with a SET_MARKER. Bit 8 tells the CP the marker is for IFPC.
	 */
	switch (type) {
	case IFPC_DISABLE:
		cmd = 0x101;
		break;
	case IFPC_ENABLE:
		cmd = 0x100;
		break;
	case IB1LIST_START:
		cmd = 0xD;
		break;
	case IB1LIST_END:
		cmd = 0xE;
		break;
	}

	*cmds++ = cmd;
	return 2;
}

static int _load_firmware(struct kgsl_device *device, const char *fwfile,
			  struct adreno_firmware *firmware)
{
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_ERR(device, "request_firmware(%s) failed: %d\n",
				fwfile, ret);
		return ret;
	}

	ret = kgsl_allocate_global(device, &firmware->memdesc, fw->size - 4,
				KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_UCODE,
				"ucode");

	if (!ret) {
		memcpy(firmware->memdesc.hostptr, &fw->data[4], fw->size - 4);
		firmware->size = (fw->size - 4) / sizeof(uint32_t);
		firmware->version = *(unsigned int *)&fw->data[4];
	}

	release_firmware(fw);
	return ret;
}

/*
 * a6xx_gpu_keepalive() - GMU reg write to request GPU stays on
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @state: State to set: true is ON, false is OFF
 */
static inline void a6xx_gpu_keepalive(struct adreno_device *adreno_dev,
		bool state)
{
	if (!gmu_core_gpmu_isenabled(KGSL_DEVICE(adreno_dev)))
		return;

	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_PWR_COL_KEEPALIVE, state);
}

/* Bitmask for GPU idle status check */
#define GPUBUSYIGNAHB		BIT(23)
static bool a6xx_hw_isidle(struct adreno_device *adreno_dev)
{
	unsigned int reg;

	gmu_core_regread(KGSL_DEVICE(adreno_dev),
		A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg);
	if (reg & GPUBUSYIGNAHB)
		return false;
	return true;
}

/*
 * a6xx_microcode_read() - Read microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_microcode_read(struct adreno_device *adreno_dev)
{
	int ret;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	struct adreno_firmware *sqe_fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

	if (sqe_fw->memdesc.hostptr == NULL) {
		ret = _load_firmware(device, adreno_dev->gpucore->sqefw_name,
				sqe_fw);
		if (ret)
			return ret;
	}

	if (gmu_core_gpmu_isenabled(device) &&
			GMU_DEV_OP_VALID(gmu_dev_ops, load_firmware))
		return gmu_dev_ops->load_firmware(device);

	return 0;
}

static int a6xx_soft_reset(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	/*
	 * For the soft reset case with GMU enabled this part is done
	 * by the GMU firmware
	 */
	if (gmu_core_gpmu_isenabled(device))
		return 0;

	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 1);
	/*
	 * Do a dummy read to get a brief read cycle delay for the
	 * reset to take effect
	 */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, &reg);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 0);

	/* Clear GBIF client halt and CX arbiter halt */
	adreno_deassert_gbif_halt(adreno_dev);

	a6xx_sptprac_enable(adreno_dev);

	return 0;
}

static int64_t a6xx_read_throttling_counters(struct adreno_device *adreno_dev)
{
	int i;
	int64_t adj = -1;
	uint32_t counts[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	if (!gmu_core_isenabled(device))
		return 0;

	for (i = 0; i < ARRAY_SIZE(counts); i++) {
		if (!adreno_dev->gpmu_throttle_counters[i])
			counts[i] = 0;
		else
			counts[i] = counter_delta(KGSL_DEVICE(adreno_dev),
					adreno_dev->gpmu_throttle_counters[i],
					&busy->throttle_cycles[i]);
	}

	/*
	 * The adjustment is the number of cycles lost to throttling, which
	 * is calculated as a weighted average of the cycles throttled
	 * at 5% or 15% based on GMU FW version, 50%, and 90%.
	 * The adjustment is negative because in A6XX,
	 * the busy count includes the throttled cycles. Therefore, we want
	 * to remove them to prevent appearing to be busier than
	 * we actually are.
	 */
	if (GMU_VER_STEP(gmu->ver) > 0x104)
		adj *= ((counts[0] * 5) + (counts[1] * 50) + (counts[2] * 90))
			/ 100;
	else
		adj *= ((counts[0] * 15) + (counts[1] * 50) + (counts[2] * 90))
			/ 100;

	trace_kgsl_clock_throttling(0, counts[1], counts[2],
			counts[0], adj);
	return adj;
}

static void a6xx_count_throttles(struct adreno_device *adreno_dev,
	uint64_t adj)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	gmu_core_regread(KGSL_DEVICE(adreno_dev),
		adreno_dev->lm_threshold_count,
		&adreno_dev->lm_threshold_cross);
}

/**
 * a6xx_reset() - Helper function to reset the GPU
 * @device: Pointer to the KGSL device structure for the GPU
 * @fault: Type of fault. Needed to skip soft reset for MMU fault
 *
 * Try to reset the GPU to recover from a fault.  First, try to do a low latency
 * soft reset.  If the soft reset fails for some reason, then bring out the big
 * guns and toggle the footswitch.
 */
static int a6xx_reset(struct kgsl_device *device, int fault)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = -EINVAL;
	int i = 0;

	/* Use the regular reset sequence for No GMU */
	if (!gmu_core_gpmu_isenabled(device))
		return adreno_reset(device, fault);

	/* Transition from ACTIVE to RESET state */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_RESET);

	/* since device is officially off now clear start bit */
	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	/* Keep trying to start the device until it works */
	for (i = 0; i < NUM_TIMES_RESET_RETRY; i++) {
		ret = adreno_start(device, 0);
		if (!ret)
			break;

		msleep(20);
	}

	if (ret)
		return ret;

	if (i != 0)
		KGSL_DRV_WARN(device, "Device hard reset tried %d tries\n", i);

	/*
	 * If active_cnt is non-zero then the system was active before
	 * going into a reset - put it back in that state
	 */

	if (atomic_read(&device->active_cnt))
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
	else
		kgsl_pwrctrl_change_state(device, KGSL_STATE_NAP);

	return ret;
}

static void a6xx_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status1, status2;

	kgsl_regread(device, A6XX_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(A6XX_CP_OPCODE_ERROR)) {
		unsigned int opcode;

		kgsl_regwrite(device, A6XX_CP_SQE_STAT_ADDR, 1);
		kgsl_regread(device, A6XX_CP_SQE_STAT_DATA, &opcode);
		KGSL_DRV_CRIT_RATELIMIT(device,
				"CP opcode error interrupt | opcode=0x%8.8x\n",
				opcode);
	}
	if (status1 & BIT(A6XX_CP_UCODE_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device, "CP ucode error interrupt\n");
	if (status1 & BIT(A6XX_CP_HW_FAULT_ERROR)) {
		kgsl_regread(device, A6XX_CP_HW_FAULT, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Ringbuffer HW fault | status=%x\n",
			status2);
	}
	if (status1 & BIT(A6XX_CP_REGISTER_PROTECTION_ERROR)) {
		kgsl_regread(device, A6XX_CP_PROTECT_STATUS, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & (1 << 20) ? "READ" : "WRITE",
			status2 & 0x3FFFF, status2);
	}
	if (status1 & BIT(A6XX_CP_AHB_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP AHB error interrupt\n");
	if (status1 & BIT(A6XX_CP_VSD_PARITY_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP VSD decoder parity error\n");
	if (status1 & BIT(A6XX_CP_ILLEGAL_INSTR_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP Illegal instruction error\n");

}

static void a6xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	switch (bit) {
	case A6XX_INT_CP_AHB_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device, "CP: AHB bus error\n");
		break;
	case A6XX_INT_ATB_ASYNCFIFO_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB ASYNC overflow\n");
		break;
	case A6XX_INT_RBBM_ATB_BUS_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB bus overflow\n");
		break;
	case A6XX_INT_UCHE_OOB_ACCESS:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Out of bounds access\n");
		break;
	case A6XX_INT_UCHE_TRAP_INTR:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Trap interrupt\n");
		break;
	case A6XX_INT_TSB_WRITE_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device, "TSB: Write error interrupt\n");
		break;
	default:
		KGSL_DRV_CRIT_RATELIMIT(device, "Unknown interrupt %d\n", bit);
	}
}

/*
 * a6xx_llc_configure_gpu_scid() - Program the sub-cache ID for all GPU blocks
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpu_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpu_scid;
	uint32_t gpu_cntl1_val = 0;
	int i;

	gpu_scid = adreno_llc_get_scid(adreno_dev->gpu_llc_slice);
	for (i = 0; i < A6XX_LLC_NUM_GPU_SCIDS; i++)
		gpu_cntl1_val = (gpu_cntl1_val << A6XX_GPU_LLC_SCID_NUM_BITS)
			| gpu_scid;

	if (adreno_is_a640(adreno_dev) || adreno_is_a612(adreno_dev) ||
		adreno_is_a610(adreno_dev) || adreno_is_a680(adreno_dev)) {
		kgsl_regrmw(KGSL_DEVICE(adreno_dev), A6XX_GBIF_SCACHE_CNTL1,
			A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);
	} else {
		adreno_cx_misc_regrmw(adreno_dev,
				A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
				A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);
	}
}

/*
 * a6xx_llc_configure_gpuhtw_scid() - Program the SCID for GPU pagetables
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpuhtw_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpuhtw_scid;

	/*
	 * On A640, the GPUHTW SCID is configured via a NoC override in the
	 * XBL image.
	 */
	if (adreno_is_a640(adreno_dev) || adreno_is_a612(adreno_dev) ||
		adreno_is_a610(adreno_dev) || adreno_is_a680(adreno_dev))
		return;

	gpuhtw_scid = adreno_llc_get_scid(adreno_dev->gpuhtw_llc_slice);

	adreno_cx_misc_regrmw(adreno_dev,
			A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPUHTW_LLC_SCID_MASK,
			gpuhtw_scid << A6XX_GPUHTW_LLC_SCID_SHIFT);
}

/*
 * a6xx_llc_enable_overrides() - Override the page attributes
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_enable_overrides(struct adreno_device *adreno_dev)
{
	/*
	 * Attributes override through GBIF is not supported with MMU-500.
	 * Attributes are used as configured through SMMU pagetable entries.
	 */
	if (adreno_is_a640(adreno_dev) || adreno_is_a612(adreno_dev) ||
		adreno_is_a610(adreno_dev) || adreno_is_a680(adreno_dev))
		return;

	/*
	 * 0x3: readnoallocoverrideen=0
	 *      read-no-alloc=0 - Allocate lines on read miss
	 *      writenoallocoverrideen=1
	 *      write-no-alloc=1 - Do not allocates lines on write miss
	 */
	adreno_cx_misc_regwrite(adreno_dev,
			A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0, 0x3);
}

static const char * const fault_block[] = {
	[0] = "CP",
	[1] = "UCHE",
	[2] = "UCHE",
	[3] = "UCHE",
	[4] = "CCU",
	[5] = "unknown",
	[6] = "CDP Prefetch",
	[7] = "GPMU",
};

static const char *uche_client[7][3] = {
	{"SP | VSC | VPC | HLSQ | PC | LRZ", "TP", "VFD"},
	{"VSC | VPC | HLSQ | PC | LRZ", "TP | VFD", "SP"},
	{"SP | VPC | HLSQ | PC | LRZ", "TP | VFD", "VSC"},
	{"SP | VSC | HLSQ | PC | LRZ", "TP | VFD", "VPC"},
	{"SP | VSC | VPC | PC | LRZ", "TP | VFD", "HLSQ"},
	{"SP | VSC | VPC | HLSQ | LRZ", "TP | VFD", "PC"},
	{"SP | VSC | VPC | HLSQ | PC", "TP | VFD", "LRZ"},
};

static const char *a6xx_iommu_fault_block(struct kgsl_device *device,
		unsigned int fsynr1)
{
	unsigned int mid, uche_client_id = 0x5c00bd00;
	static char str[40];

	mid = fsynr1 & 0xff;

	if (mid >= ARRAY_SIZE(fault_block))
		return "unknown";
	/* Return the fault block except for UCHE */
	else if (!mid || (mid > 3))
		return fault_block[mid];

	mutex_lock(&device->mutex);

	if (!kgsl_state_is_awake(device)) {
		mutex_unlock(&device->mutex);
		return "UCHE: unknown";
	}

	kgsl_regread(device, A6XX_UCHE_CLIENT_PF, &uche_client_id);
	mutex_unlock(&device->mutex);

	/* Ignore the value if the gpu is in IFPC */
	if (uche_client_id == 0x5c00bd00)
		return "UCHE: unknown";

	uche_client_id &= A6XX_UCHE_CLIENT_PF_CLIENT_ID_MASK;
	snprintf(str, sizeof(str), "UCHE: %s",
			uche_client[uche_client_id][mid - 1]);
	return str;
}

static void a6xx_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_preemption_enabled(adreno_dev))
		a6xx_preemption_trigger(adreno_dev);

	adreno_dispatcher_schedule(device);
}

/*
 * a6xx_gpc_err_int_callback() - Isr for GPC error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void a6xx_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * GPC error is typically the result of mistake SW programming.
	 * Force GPU fault for this interrupt so that we can debug it
	 * with help of register dump.
	 */

	KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: GPC error\n");
	adreno_irqctrl(adreno_dev, 0);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_set_gpu_fault(adreno_dev, ADRENO_SOFT_FAULT);
	adreno_dispatcher_schedule(device);
}

#define A6XX_INT_MASK \
	((1 << A6XX_INT_CP_AHB_ERROR) |			\
	 (1 << A6XX_INT_ATB_ASYNCFIFO_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A6XX_INT_CP_SW) |			\
	 (1 << A6XX_INT_CP_HW_ERROR) |			\
	 (1 << A6XX_INT_CP_IB2) |			\
	 (1 << A6XX_INT_CP_IB1) |			\
	 (1 << A6XX_INT_CP_RB) |			\
	 (1 << A6XX_INT_CP_CACHE_FLUSH_TS) |		\
	 (1 << A6XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_HANG_DETECT) |		\
	 (1 << A6XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A6XX_INT_UCHE_TRAP_INTR) |		\
	 (1 << A6XX_INT_TSB_WRITE_ERROR))

static struct adreno_irq_funcs a6xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - UNUSED */
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback),
	ADRENO_IRQ_CALLBACK(a6xx_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(a6xx_preemption_callback),/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a6xx_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
	ADRENO_IRQ_CALLBACK(NULL),  /* 10 - CP_CCU_FLUSH_DEPTH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 11 - CP_CCU_FLUSH_COLOR_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 12 - CP_CCU_RESOLVE_TS */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 15 - CP_RB_INT */
	ADRENO_IRQ_CALLBACK(NULL), /* 16 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - CP_WT_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - UNUSED */
	ADRENO_IRQ_CALLBACK(a6xx_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 21 - UNUSED */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	/* 23 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 28 - TSBWRITEERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

static struct adreno_irq a6xx_irq = {
	.funcs = a6xx_irq_funcs,
	.mask = A6XX_INT_MASK,
};

static bool adreno_is_qdss_dbg_register(struct kgsl_device *device,
		unsigned int offsetwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return adreno_dev->qdss_gfx_virt &&
		(offsetwords >= (adreno_dev->qdss_gfx_base >> 2)) &&
		(offsetwords < (adreno_dev->qdss_gfx_base +
				adreno_dev->qdss_gfx_len) >> 2);
}


static void adreno_qdss_gfx_dbg_regread(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int qdss_gfx_offset;

	if (!adreno_is_qdss_dbg_register(device, offsetwords))
		return;

	qdss_gfx_offset = (offsetwords << 2) - adreno_dev->qdss_gfx_base;
	*value = __raw_readl(adreno_dev->qdss_gfx_virt + qdss_gfx_offset);

	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
	rmb();
}

static void adreno_qdss_gfx_dbg_regwrite(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int value)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int qdss_gfx_offset;

	if (!adreno_is_qdss_dbg_register(device, offsetwords))
		return;

	qdss_gfx_offset = (offsetwords << 2) - adreno_dev->qdss_gfx_base;
	trace_kgsl_regwrite(device, offsetwords, value);

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, adreno_dev->qdss_gfx_virt + qdss_gfx_offset);
}

static void adreno_gx_regread(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	if (adreno_is_qdss_dbg_register(device, offsetwords))
		adreno_qdss_gfx_dbg_regread(device, offsetwords, value);
	else
		kgsl_regread(device, offsetwords, value);
}

static void adreno_gx_regwrite(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int value)
{
	if (adreno_is_qdss_dbg_register(device, offsetwords))
		adreno_qdss_gfx_dbg_regwrite(device, offsetwords, value);
	else
		kgsl_regwrite(device, offsetwords, value);
}

static struct adreno_coresight_register a6xx_coresight_regs[] = {
	{ A6XX_DBGC_CFG_DBGBUS_SEL_A },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_B },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_C },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_D },
	{ A6XX_DBGC_CFG_DBGBUS_CNTLT },
	{ A6XX_DBGC_CFG_DBGBUS_CNTLM },
	{ A6XX_DBGC_CFG_DBGBUS_OPL },
	{ A6XX_DBGC_CFG_DBGBUS_OPE },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_2 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_3 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_2 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_3 },
	{ A6XX_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_0 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_2 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_3 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_0 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_1 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_2 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_3 },
	{ A6XX_DBGC_CFG_DBGBUS_NIBBLEE },
	{ A6XX_DBGC_CFG_DBGBUS_PTRC0 },
	{ A6XX_DBGC_CFG_DBGBUS_PTRC1 },
	{ A6XX_DBGC_CFG_DBGBUS_LOADREG },
	{ A6XX_DBGC_CFG_DBGBUS_IDX },
	{ A6XX_DBGC_CFG_DBGBUS_CLRC },
	{ A6XX_DBGC_CFG_DBGBUS_LOADIVT },
	{ A6XX_DBGC_VBIF_DBG_CNTL },
	{ A6XX_DBGC_DBG_LO_HI_GPIO },
	{ A6XX_DBGC_EXT_TRACE_BUS_CNTL },
	{ A6XX_DBGC_READ_AHB_THROUGH_DBG },
	{ A6XX_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ A6XX_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ A6XX_DBGC_EVT_CFG },
	{ A6XX_DBGC_EVT_INTF_SEL_0 },
	{ A6XX_DBGC_EVT_INTF_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_CFG },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_0 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_2 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_3 },
	{ A6XX_DBGC_PERF_ATB_TRIG_INTF_SEL_0 },
	{ A6XX_DBGC_PERF_ATB_TRIG_INTF_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_DRAIN_CMD },
	{ A6XX_DBGC_ECO_CNTL },
	{ A6XX_DBGC_AHB_DBG_CNTL },
	{ A6XX_SP0_ISDB_ISDB_EN },
	{ A6XX_SP0_ISDB_ISDB_SAC_CFG },
	{ A6XX_SP0_ISDB_ISDB_SAC_ADDR_0 },
	{ A6XX_SP0_ISDB_ISDB_SAC_ADDR_1 },
	{ A6XX_SP0_ISDB_ISDB_SAC_MASK_0 },
	{ A6XX_SP0_ISDB_ISDB_SAC_MASK_1 },
	{ A6XX_SP0_ISDB_ISDB_SHADER_ID_CFG },
	{ A6XX_SP0_ISDB_ISDB_WAVE_ID_CFG },
	{ A6XX_HLSQ_ISDB_ISDB_HLSQ_ISDB_CL_WGID_CTRL },
	{ A6XX_HLSQ_ISDB_ISDB_HLSQ_ISDB_CL_WGID_X },
	{ A6XX_HLSQ_ISDB_ISDB_HLSQ_ISDB_CL_WGID_Y },
	{ A6XX_HLSQ_ISDB_ISDB_HLSQ_ISDB_CL_WGID_Z },
	{ A6XX_SP0_ISDB_ISDB_BRKPT_CFG },
	{ A6XX_SP1_ISDB_ISDB_EN },
	{ A6XX_SP1_ISDB_ISDB_SAC_CFG },
	{ A6XX_SP1_ISDB_ISDB_SAC_ADDR_0 },
	{ A6XX_SP1_ISDB_ISDB_SAC_ADDR_1 },
	{ A6XX_SP1_ISDB_ISDB_SAC_MASK_0 },
	{ A6XX_SP1_ISDB_ISDB_SAC_MASK_1 },
	{ A6XX_SP1_ISDB_ISDB_SHADER_ID_CFG },
	{ A6XX_SP1_ISDB_ISDB_WAVE_ID_CFG },
	{ A6XX_SP1_ISDB_ISDB_BRKPT_CFG },
};

static struct adreno_coresight_register a6xx_coresight_regs_cx[] = {
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_A },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_B },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_C },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_D },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CNTLT },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CNTLM },
	{ A6XX_CX_DBGC_CFG_DBGBUS_OPL },
	{ A6XX_CX_DBGC_CFG_DBGBUS_OPE },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_NIBBLEE },
	{ A6XX_CX_DBGC_CFG_DBGBUS_PTRC0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_PTRC1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_LOADREG },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IDX },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CLRC },
	{ A6XX_CX_DBGC_CFG_DBGBUS_LOADIVT },
	{ A6XX_CX_DBGC_VBIF_DBG_CNTL },
	{ A6XX_CX_DBGC_DBG_LO_HI_GPIO },
	{ A6XX_CX_DBGC_EXT_TRACE_BUS_CNTL },
	{ A6XX_CX_DBGC_READ_AHB_THROUGH_DBG },
	{ A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ A6XX_CX_DBGC_EVT_CFG },
	{ A6XX_CX_DBGC_EVT_INTF_SEL_0 },
	{ A6XX_CX_DBGC_EVT_INTF_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_CFG },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_0 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_2 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_3 },
	{ A6XX_CX_DBGC_PERF_ATB_TRIG_INTF_SEL_0 },
	{ A6XX_CX_DBGC_PERF_ATB_TRIG_INTF_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_DRAIN_CMD },
	{ A6XX_CX_DBGC_ECO_CNTL },
	{ A6XX_CX_DBGC_AHB_DBG_CNTL },
};

static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_a, &a6xx_coresight_regs[0]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_b, &a6xx_coresight_regs[1]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_c, &a6xx_coresight_regs[2]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_d, &a6xx_coresight_regs[3]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlt, &a6xx_coresight_regs[4]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlm, &a6xx_coresight_regs[5]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_opl, &a6xx_coresight_regs[6]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ope, &a6xx_coresight_regs[7]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_0, &a6xx_coresight_regs[8]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_1, &a6xx_coresight_regs[9]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_2, &a6xx_coresight_regs[10]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_3, &a6xx_coresight_regs[11]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_0, &a6xx_coresight_regs[12]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_1, &a6xx_coresight_regs[13]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_2, &a6xx_coresight_regs[14]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_3, &a6xx_coresight_regs[15]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_0, &a6xx_coresight_regs[16]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_1, &a6xx_coresight_regs[17]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_0, &a6xx_coresight_regs[18]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_1, &a6xx_coresight_regs[19]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_2, &a6xx_coresight_regs[20]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_3, &a6xx_coresight_regs[21]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_0, &a6xx_coresight_regs[22]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_1, &a6xx_coresight_regs[23]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_2, &a6xx_coresight_regs[24]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_3, &a6xx_coresight_regs[25]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_nibblee, &a6xx_coresight_regs[26]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc0, &a6xx_coresight_regs[27]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc1, &a6xx_coresight_regs[28]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadreg, &a6xx_coresight_regs[29]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_idx, &a6xx_coresight_regs[30]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_clrc, &a6xx_coresight_regs[31]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadivt, &a6xx_coresight_regs[32]);
static ADRENO_CORESIGHT_ATTR(vbif_dbg_cntl, &a6xx_coresight_regs[33]);
static ADRENO_CORESIGHT_ATTR(dbg_lo_hi_gpio, &a6xx_coresight_regs[34]);
static ADRENO_CORESIGHT_ATTR(ext_trace_bus_cntl, &a6xx_coresight_regs[35]);
static ADRENO_CORESIGHT_ATTR(read_ahb_through_dbg, &a6xx_coresight_regs[36]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf1, &a6xx_coresight_regs[37]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf2, &a6xx_coresight_regs[38]);
static ADRENO_CORESIGHT_ATTR(evt_cfg, &a6xx_coresight_regs[39]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_0, &a6xx_coresight_regs[40]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_1, &a6xx_coresight_regs[41]);
static ADRENO_CORESIGHT_ATTR(perf_atb_cfg, &a6xx_coresight_regs[42]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_0, &a6xx_coresight_regs[43]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_1, &a6xx_coresight_regs[44]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_2, &a6xx_coresight_regs[45]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_3, &a6xx_coresight_regs[46]);
static ADRENO_CORESIGHT_ATTR(perf_atb_trig_intf_sel_0,
				&a6xx_coresight_regs[47]);
static ADRENO_CORESIGHT_ATTR(perf_atb_trig_intf_sel_1,
				&a6xx_coresight_regs[48]);
static ADRENO_CORESIGHT_ATTR(perf_atb_drain_cmd, &a6xx_coresight_regs[49]);
static ADRENO_CORESIGHT_ATTR(eco_cntl, &a6xx_coresight_regs[50]);
static ADRENO_CORESIGHT_ATTR(ahb_dbg_cntl, &a6xx_coresight_regs[51]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_en, &a6xx_coresight_regs[52]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_sac_cfg, &a6xx_coresight_regs[53]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_sac_addr_0,
				&a6xx_coresight_regs[54]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_sac_addr_1,
				&a6xx_coresight_regs[55]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_sac_mask_0,
				&a6xx_coresight_regs[56]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_sac_mask_1,
				&a6xx_coresight_regs[57]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_shader_id_cfg,
				&a6xx_coresight_regs[58]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_wave_id_cfg,
				&a6xx_coresight_regs[59]);
static ADRENO_CORESIGHT_ATTR(hlsq_isdb_isdb_hlsq_isdb_cl_wgid_ctrl,
				&a6xx_coresight_regs[60]);
static ADRENO_CORESIGHT_ATTR(hlsq_isdb_isdb_hlsq_isdb_cl_wgid_x,
				&a6xx_coresight_regs[61]);
static ADRENO_CORESIGHT_ATTR(hlsq_isdb_isdb_hlsq_isdb_cl_wgid_y,
				&a6xx_coresight_regs[62]);
static ADRENO_CORESIGHT_ATTR(hlsq_isdb_isdb_hlsq_isdb_cl_wgid_z,
				&a6xx_coresight_regs[63]);
static ADRENO_CORESIGHT_ATTR(sp0_isdb_isdb_brkpt_cfg, &a6xx_coresight_regs[64]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_en, &a6xx_coresight_regs[65]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_sac_cfg, &a6xx_coresight_regs[66]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_sac_addr_0,
				&a6xx_coresight_regs[67]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_sac_addr_1,
				&a6xx_coresight_regs[68]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_sac_mask_0,
				&a6xx_coresight_regs[69]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_sac_mask_1,
				&a6xx_coresight_regs[70]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_shader_id_cfg,
				&a6xx_coresight_regs[71]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_wave_id_cfg,
				&a6xx_coresight_regs[72]);
static ADRENO_CORESIGHT_ATTR(sp1_isdb_isdb_brkpt_cfg,
				&a6xx_coresight_regs[73]);


/*CX debug registers*/
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_a,
				&a6xx_coresight_regs_cx[0]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_b,
				&a6xx_coresight_regs_cx[1]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_c,
				&a6xx_coresight_regs_cx[2]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_d,
				&a6xx_coresight_regs_cx[3]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlt,
				&a6xx_coresight_regs_cx[4]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlm,
				&a6xx_coresight_regs_cx[5]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_opl,
				&a6xx_coresight_regs_cx[6]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ope,
				&a6xx_coresight_regs_cx[7]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_0,
				&a6xx_coresight_regs_cx[8]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_1,
				&a6xx_coresight_regs_cx[9]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_2,
				&a6xx_coresight_regs_cx[10]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_3,
				&a6xx_coresight_regs_cx[11]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_0,
				&a6xx_coresight_regs_cx[12]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_1,
				&a6xx_coresight_regs_cx[13]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_2,
				&a6xx_coresight_regs_cx[14]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_3,
				&a6xx_coresight_regs_cx[15]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_0,
				&a6xx_coresight_regs_cx[16]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_1,
				&a6xx_coresight_regs_cx[17]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_0,
				&a6xx_coresight_regs_cx[18]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_1,
				&a6xx_coresight_regs_cx[19]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_2,
				&a6xx_coresight_regs_cx[20]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_3,
				&a6xx_coresight_regs_cx[21]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_0,
				&a6xx_coresight_regs_cx[22]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_1,
				&a6xx_coresight_regs_cx[23]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_2,
				&a6xx_coresight_regs_cx[24]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_3,
				&a6xx_coresight_regs_cx[25]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_nibblee,
				&a6xx_coresight_regs_cx[26]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc0,
				&a6xx_coresight_regs_cx[27]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc1,
				&a6xx_coresight_regs_cx[28]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadreg,
				&a6xx_coresight_regs_cx[29]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_idx,
				&a6xx_coresight_regs_cx[30]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_clrc,
				&a6xx_coresight_regs_cx[31]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadivt,
				&a6xx_coresight_regs_cx[32]);
static ADRENO_CORESIGHT_ATTR(cx_vbif_dbg_cntl,
				&a6xx_coresight_regs_cx[33]);
static ADRENO_CORESIGHT_ATTR(cx_dbg_lo_hi_gpio,
				&a6xx_coresight_regs_cx[34]);
static ADRENO_CORESIGHT_ATTR(cx_ext_trace_bus_cntl,
				&a6xx_coresight_regs_cx[35]);
static ADRENO_CORESIGHT_ATTR(cx_read_ahb_through_dbg,
				&a6xx_coresight_regs_cx[36]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf1,
				&a6xx_coresight_regs_cx[37]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf2,
				&a6xx_coresight_regs_cx[38]);
static ADRENO_CORESIGHT_ATTR(cx_evt_cfg,
				&a6xx_coresight_regs_cx[39]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_0,
				&a6xx_coresight_regs_cx[40]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_1,
				&a6xx_coresight_regs_cx[41]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_cfg,
				&a6xx_coresight_regs_cx[42]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_0,
				&a6xx_coresight_regs_cx[43]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_1,
				&a6xx_coresight_regs_cx[44]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_2,
				&a6xx_coresight_regs_cx[45]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_3,
				&a6xx_coresight_regs_cx[46]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_trig_intf_sel_0,
				&a6xx_coresight_regs_cx[47]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_trig_intf_sel_1,
				&a6xx_coresight_regs_cx[48]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_drain_cmd,
				&a6xx_coresight_regs_cx[49]);
static ADRENO_CORESIGHT_ATTR(cx_eco_cntl,
				&a6xx_coresight_regs_cx[50]);
static ADRENO_CORESIGHT_ATTR(cx_ahb_dbg_cntl,
				&a6xx_coresight_regs_cx[51]);

static struct attribute *a6xx_coresight_attrs[] = {
	&coresight_attr_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_vbif_dbg_cntl.attr.attr,
	&coresight_attr_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_evt_cfg.attr.attr,
	&coresight_attr_evt_intf_sel_0.attr.attr,
	&coresight_attr_evt_intf_sel_1.attr.attr,
	&coresight_attr_perf_atb_cfg.attr.attr,
	&coresight_attr_perf_atb_counter_sel_0.attr.attr,
	&coresight_attr_perf_atb_counter_sel_1.attr.attr,
	&coresight_attr_perf_atb_counter_sel_2.attr.attr,
	&coresight_attr_perf_atb_counter_sel_3.attr.attr,
	&coresight_attr_perf_atb_trig_intf_sel_0.attr.attr,
	&coresight_attr_perf_atb_trig_intf_sel_1.attr.attr,
	&coresight_attr_perf_atb_drain_cmd.attr.attr,
	&coresight_attr_eco_cntl.attr.attr,
	&coresight_attr_ahb_dbg_cntl.attr.attr,
	&coresight_attr_sp0_isdb_isdb_en.attr.attr,
	&coresight_attr_sp0_isdb_isdb_sac_cfg.attr.attr,
	&coresight_attr_sp0_isdb_isdb_sac_addr_0.attr.attr,
	&coresight_attr_sp0_isdb_isdb_sac_addr_1.attr.attr,
	&coresight_attr_sp0_isdb_isdb_sac_mask_0.attr.attr,
	&coresight_attr_sp0_isdb_isdb_sac_mask_1.attr.attr,
	&coresight_attr_sp0_isdb_isdb_shader_id_cfg.attr.attr,
	&coresight_attr_sp0_isdb_isdb_wave_id_cfg.attr.attr,
	&coresight_attr_hlsq_isdb_isdb_hlsq_isdb_cl_wgid_ctrl.attr.attr,
	&coresight_attr_hlsq_isdb_isdb_hlsq_isdb_cl_wgid_x.attr.attr,
	&coresight_attr_hlsq_isdb_isdb_hlsq_isdb_cl_wgid_y.attr.attr,
	&coresight_attr_hlsq_isdb_isdb_hlsq_isdb_cl_wgid_z.attr.attr,
	&coresight_attr_sp0_isdb_isdb_brkpt_cfg.attr.attr,
	&coresight_attr_sp1_isdb_isdb_en.attr.attr,
	&coresight_attr_sp1_isdb_isdb_sac_cfg.attr.attr,
	&coresight_attr_sp1_isdb_isdb_sac_addr_0.attr.attr,
	&coresight_attr_sp1_isdb_isdb_sac_addr_1.attr.attr,
	&coresight_attr_sp1_isdb_isdb_sac_mask_0.attr.attr,
	&coresight_attr_sp1_isdb_isdb_sac_mask_1.attr.attr,
	&coresight_attr_sp1_isdb_isdb_shader_id_cfg.attr.attr,
	&coresight_attr_sp1_isdb_isdb_wave_id_cfg.attr.attr,
	&coresight_attr_sp1_isdb_isdb_brkpt_cfg.attr.attr,
	NULL,
};

/*cx*/
static struct attribute *a6xx_coresight_attrs_cx[] = {
	&coresight_attr_cx_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_cx_vbif_dbg_cntl.attr.attr,
	&coresight_attr_cx_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_cx_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_cx_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_cx_evt_cfg.attr.attr,
	&coresight_attr_cx_evt_intf_sel_0.attr.attr,
	&coresight_attr_cx_evt_intf_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_cfg.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_0.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_2.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_3.attr.attr,
	&coresight_attr_cx_perf_atb_trig_intf_sel_0.attr.attr,
	&coresight_attr_cx_perf_atb_trig_intf_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_drain_cmd.attr.attr,
	&coresight_attr_cx_eco_cntl.attr.attr,
	&coresight_attr_cx_ahb_dbg_cntl.attr.attr,
	NULL,
};

static const struct attribute_group a6xx_coresight_group = {
	.attrs = a6xx_coresight_attrs,
};

static const struct attribute_group *a6xx_coresight_groups[] = {
	&a6xx_coresight_group,
	NULL,
};

static const struct attribute_group a6xx_coresight_group_cx = {
	.attrs = a6xx_coresight_attrs_cx,
};

static const struct attribute_group *a6xx_coresight_groups_cx[] = {
	&a6xx_coresight_group_cx,
	NULL,
};

static struct adreno_coresight a6xx_coresight = {
	.registers = a6xx_coresight_regs,
	.count = ARRAY_SIZE(a6xx_coresight_regs),
	.groups = a6xx_coresight_groups,
	.read = adreno_gx_regread,
	.write = adreno_gx_regwrite,
};

static struct adreno_coresight a6xx_coresight_cx = {
	.registers = a6xx_coresight_regs_cx,
	.count = ARRAY_SIZE(a6xx_coresight_regs_cx),
	.groups = a6xx_coresight_groups_cx,
	.read = adreno_cx_dbgc_regread,
	.write = adreno_cx_dbgc_regwrite,
};

static struct adreno_perfcount_register a6xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_0_LO,
		A6XX_RBBM_PERFCTR_CP_0_HI, 0, A6XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_1_LO,
		A6XX_RBBM_PERFCTR_CP_1_HI, 1, A6XX_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_2_LO,
		A6XX_RBBM_PERFCTR_CP_2_HI, 2, A6XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_3_LO,
		A6XX_RBBM_PERFCTR_CP_3_HI, 3, A6XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_4_LO,
		A6XX_RBBM_PERFCTR_CP_4_HI, 4, A6XX_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_5_LO,
		A6XX_RBBM_PERFCTR_CP_5_HI, 5, A6XX_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_6_LO,
		A6XX_RBBM_PERFCTR_CP_6_HI, 6, A6XX_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_7_LO,
		A6XX_RBBM_PERFCTR_CP_7_HI, 7, A6XX_CP_PERFCTR_CP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_8_LO,
		A6XX_RBBM_PERFCTR_CP_8_HI, 8, A6XX_CP_PERFCTR_CP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_9_LO,
		A6XX_RBBM_PERFCTR_CP_9_HI, 9, A6XX_CP_PERFCTR_CP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_10_LO,
		A6XX_RBBM_PERFCTR_CP_10_HI, 10, A6XX_CP_PERFCTR_CP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_11_LO,
		A6XX_RBBM_PERFCTR_CP_11_HI, 11, A6XX_CP_PERFCTR_CP_SEL_11 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_12_LO,
		A6XX_RBBM_PERFCTR_CP_12_HI, 12, A6XX_CP_PERFCTR_CP_SEL_12 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CP_13_LO,
		A6XX_RBBM_PERFCTR_CP_13_HI, 13, A6XX_CP_PERFCTR_CP_SEL_13 },
};

static struct adreno_perfcount_register a6xx_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RBBM_0_LO,
		A6XX_RBBM_PERFCTR_RBBM_0_HI, 15, A6XX_RBBM_PERFCTR_RBBM_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RBBM_1_LO,
		A6XX_RBBM_PERFCTR_RBBM_1_HI, 15, A6XX_RBBM_PERFCTR_RBBM_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RBBM_2_LO,
		A6XX_RBBM_PERFCTR_RBBM_2_HI, 16, A6XX_RBBM_PERFCTR_RBBM_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RBBM_3_LO,
		A6XX_RBBM_PERFCTR_RBBM_3_HI, 17, A6XX_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_0_LO,
		A6XX_RBBM_PERFCTR_PC_0_HI, 18, A6XX_PC_PERFCTR_PC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_1_LO,
		A6XX_RBBM_PERFCTR_PC_1_HI, 19, A6XX_PC_PERFCTR_PC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_2_LO,
		A6XX_RBBM_PERFCTR_PC_2_HI, 20, A6XX_PC_PERFCTR_PC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_3_LO,
		A6XX_RBBM_PERFCTR_PC_3_HI, 21, A6XX_PC_PERFCTR_PC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_4_LO,
		A6XX_RBBM_PERFCTR_PC_4_HI, 22, A6XX_PC_PERFCTR_PC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_5_LO,
		A6XX_RBBM_PERFCTR_PC_5_HI, 23, A6XX_PC_PERFCTR_PC_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_6_LO,
		A6XX_RBBM_PERFCTR_PC_6_HI, 24, A6XX_PC_PERFCTR_PC_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_PC_7_LO,
		A6XX_RBBM_PERFCTR_PC_7_HI, 25, A6XX_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_perfcount_register a6xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_0_LO,
		A6XX_RBBM_PERFCTR_VFD_0_HI, 26, A6XX_VFD_PERFCTR_VFD_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_1_LO,
		A6XX_RBBM_PERFCTR_VFD_1_HI, 27, A6XX_VFD_PERFCTR_VFD_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_2_LO,
		A6XX_RBBM_PERFCTR_VFD_2_HI, 28, A6XX_VFD_PERFCTR_VFD_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_3_LO,
		A6XX_RBBM_PERFCTR_VFD_3_HI, 29, A6XX_VFD_PERFCTR_VFD_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_4_LO,
		A6XX_RBBM_PERFCTR_VFD_4_HI, 30, A6XX_VFD_PERFCTR_VFD_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_5_LO,
		A6XX_RBBM_PERFCTR_VFD_5_HI, 31, A6XX_VFD_PERFCTR_VFD_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_6_LO,
		A6XX_RBBM_PERFCTR_VFD_6_HI, 32, A6XX_VFD_PERFCTR_VFD_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VFD_7_LO,
		A6XX_RBBM_PERFCTR_VFD_7_HI, 33, A6XX_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_perfcount_register a6xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_0_LO,
		A6XX_RBBM_PERFCTR_HLSQ_0_HI, 34, A6XX_HLSQ_PERFCTR_HLSQ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_1_LO,
		A6XX_RBBM_PERFCTR_HLSQ_1_HI, 35, A6XX_HLSQ_PERFCTR_HLSQ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_2_LO,
		A6XX_RBBM_PERFCTR_HLSQ_2_HI, 36, A6XX_HLSQ_PERFCTR_HLSQ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_3_LO,
		A6XX_RBBM_PERFCTR_HLSQ_3_HI, 37, A6XX_HLSQ_PERFCTR_HLSQ_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_4_LO,
		A6XX_RBBM_PERFCTR_HLSQ_4_HI, 38, A6XX_HLSQ_PERFCTR_HLSQ_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_HLSQ_5_LO,
		A6XX_RBBM_PERFCTR_HLSQ_5_HI, 39, A6XX_HLSQ_PERFCTR_HLSQ_SEL_5 },
};

static struct adreno_perfcount_register a6xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_0_LO,
		A6XX_RBBM_PERFCTR_VPC_0_HI, 40, A6XX_VPC_PERFCTR_VPC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_1_LO,
		A6XX_RBBM_PERFCTR_VPC_1_HI, 41, A6XX_VPC_PERFCTR_VPC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_2_LO,
		A6XX_RBBM_PERFCTR_VPC_2_HI, 42, A6XX_VPC_PERFCTR_VPC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_3_LO,
		A6XX_RBBM_PERFCTR_VPC_3_HI, 43, A6XX_VPC_PERFCTR_VPC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_4_LO,
		A6XX_RBBM_PERFCTR_VPC_4_HI, 44, A6XX_VPC_PERFCTR_VPC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VPC_5_LO,
		A6XX_RBBM_PERFCTR_VPC_5_HI, 45, A6XX_VPC_PERFCTR_VPC_SEL_5 },
};

static struct adreno_perfcount_register a6xx_perfcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CCU_0_LO,
		A6XX_RBBM_PERFCTR_CCU_0_HI, 46, A6XX_RB_PERFCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CCU_1_LO,
		A6XX_RBBM_PERFCTR_CCU_1_HI, 47, A6XX_RB_PERFCTR_CCU_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CCU_2_LO,
		A6XX_RBBM_PERFCTR_CCU_2_HI, 48, A6XX_RB_PERFCTR_CCU_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CCU_3_LO,
		A6XX_RBBM_PERFCTR_CCU_3_HI, 49, A6XX_RB_PERFCTR_CCU_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CCU_4_LO,
		A6XX_RBBM_PERFCTR_CCU_4_HI, 50, A6XX_RB_PERFCTR_CCU_SEL_4 },
};

static struct adreno_perfcount_register a6xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TSE_0_LO,
		A6XX_RBBM_PERFCTR_TSE_0_HI, 51, A6XX_GRAS_PERFCTR_TSE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TSE_1_LO,
		A6XX_RBBM_PERFCTR_TSE_1_HI, 52, A6XX_GRAS_PERFCTR_TSE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TSE_2_LO,
		A6XX_RBBM_PERFCTR_TSE_2_HI, 53, A6XX_GRAS_PERFCTR_TSE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TSE_3_LO,
		A6XX_RBBM_PERFCTR_TSE_3_HI, 54, A6XX_GRAS_PERFCTR_TSE_SEL_3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RAS_0_LO,
		A6XX_RBBM_PERFCTR_RAS_0_HI, 55, A6XX_GRAS_PERFCTR_RAS_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RAS_1_LO,
		A6XX_RBBM_PERFCTR_RAS_1_HI, 56, A6XX_GRAS_PERFCTR_RAS_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RAS_2_LO,
		A6XX_RBBM_PERFCTR_RAS_2_HI, 57, A6XX_GRAS_PERFCTR_RAS_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RAS_3_LO,
		A6XX_RBBM_PERFCTR_RAS_3_HI, 58, A6XX_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_0_LO,
		A6XX_RBBM_PERFCTR_UCHE_0_HI, 59, A6XX_UCHE_PERFCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_1_LO,
		A6XX_RBBM_PERFCTR_UCHE_1_HI, 60, A6XX_UCHE_PERFCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_2_LO,
		A6XX_RBBM_PERFCTR_UCHE_2_HI, 61, A6XX_UCHE_PERFCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_3_LO,
		A6XX_RBBM_PERFCTR_UCHE_3_HI, 62, A6XX_UCHE_PERFCTR_UCHE_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_4_LO,
		A6XX_RBBM_PERFCTR_UCHE_4_HI, 63, A6XX_UCHE_PERFCTR_UCHE_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_5_LO,
		A6XX_RBBM_PERFCTR_UCHE_5_HI, 64, A6XX_UCHE_PERFCTR_UCHE_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_6_LO,
		A6XX_RBBM_PERFCTR_UCHE_6_HI, 65, A6XX_UCHE_PERFCTR_UCHE_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_7_LO,
		A6XX_RBBM_PERFCTR_UCHE_7_HI, 66, A6XX_UCHE_PERFCTR_UCHE_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_8_LO,
		A6XX_RBBM_PERFCTR_UCHE_8_HI, 67, A6XX_UCHE_PERFCTR_UCHE_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_9_LO,
		A6XX_RBBM_PERFCTR_UCHE_9_HI, 68, A6XX_UCHE_PERFCTR_UCHE_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_10_LO,
		A6XX_RBBM_PERFCTR_UCHE_10_HI, 69,
					A6XX_UCHE_PERFCTR_UCHE_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_UCHE_11_LO,
		A6XX_RBBM_PERFCTR_UCHE_11_HI, 70,
					A6XX_UCHE_PERFCTR_UCHE_SEL_11 },
};

static struct adreno_perfcount_register a6xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_0_LO,
		A6XX_RBBM_PERFCTR_TP_0_HI, 71, A6XX_TPL1_PERFCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_1_LO,
		A6XX_RBBM_PERFCTR_TP_1_HI, 72, A6XX_TPL1_PERFCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_2_LO,
		A6XX_RBBM_PERFCTR_TP_2_HI, 73, A6XX_TPL1_PERFCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_3_LO,
		A6XX_RBBM_PERFCTR_TP_3_HI, 74, A6XX_TPL1_PERFCTR_TP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_4_LO,
		A6XX_RBBM_PERFCTR_TP_4_HI, 75, A6XX_TPL1_PERFCTR_TP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_5_LO,
		A6XX_RBBM_PERFCTR_TP_5_HI, 76, A6XX_TPL1_PERFCTR_TP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_6_LO,
		A6XX_RBBM_PERFCTR_TP_6_HI, 77, A6XX_TPL1_PERFCTR_TP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_7_LO,
		A6XX_RBBM_PERFCTR_TP_7_HI, 78, A6XX_TPL1_PERFCTR_TP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_8_LO,
		A6XX_RBBM_PERFCTR_TP_8_HI, 79, A6XX_TPL1_PERFCTR_TP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_9_LO,
		A6XX_RBBM_PERFCTR_TP_9_HI, 80, A6XX_TPL1_PERFCTR_TP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_10_LO,
		A6XX_RBBM_PERFCTR_TP_10_HI, 81, A6XX_TPL1_PERFCTR_TP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_TP_11_LO,
		A6XX_RBBM_PERFCTR_TP_11_HI, 82, A6XX_TPL1_PERFCTR_TP_SEL_11 },
};

static struct adreno_perfcount_register a6xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_0_LO,
		A6XX_RBBM_PERFCTR_SP_0_HI, 83, A6XX_SP_PERFCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_1_LO,
		A6XX_RBBM_PERFCTR_SP_1_HI, 84, A6XX_SP_PERFCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_2_LO,
		A6XX_RBBM_PERFCTR_SP_2_HI, 85, A6XX_SP_PERFCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_3_LO,
		A6XX_RBBM_PERFCTR_SP_3_HI, 86, A6XX_SP_PERFCTR_SP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_4_LO,
		A6XX_RBBM_PERFCTR_SP_4_HI, 87, A6XX_SP_PERFCTR_SP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_5_LO,
		A6XX_RBBM_PERFCTR_SP_5_HI, 88, A6XX_SP_PERFCTR_SP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_6_LO,
		A6XX_RBBM_PERFCTR_SP_6_HI, 89, A6XX_SP_PERFCTR_SP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_7_LO,
		A6XX_RBBM_PERFCTR_SP_7_HI, 90, A6XX_SP_PERFCTR_SP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_8_LO,
		A6XX_RBBM_PERFCTR_SP_8_HI, 91, A6XX_SP_PERFCTR_SP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_9_LO,
		A6XX_RBBM_PERFCTR_SP_9_HI, 92, A6XX_SP_PERFCTR_SP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_10_LO,
		A6XX_RBBM_PERFCTR_SP_10_HI, 93, A6XX_SP_PERFCTR_SP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_11_LO,
		A6XX_RBBM_PERFCTR_SP_11_HI, 94, A6XX_SP_PERFCTR_SP_SEL_11 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_12_LO,
		A6XX_RBBM_PERFCTR_SP_12_HI, 95, A6XX_SP_PERFCTR_SP_SEL_12 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_13_LO,
		A6XX_RBBM_PERFCTR_SP_13_HI, 96, A6XX_SP_PERFCTR_SP_SEL_13 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_14_LO,
		A6XX_RBBM_PERFCTR_SP_14_HI, 97, A6XX_SP_PERFCTR_SP_SEL_14 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_15_LO,
		A6XX_RBBM_PERFCTR_SP_15_HI, 98, A6XX_SP_PERFCTR_SP_SEL_15 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_16_LO,
		A6XX_RBBM_PERFCTR_SP_16_HI, 99, A6XX_SP_PERFCTR_SP_SEL_16 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_17_LO,
		A6XX_RBBM_PERFCTR_SP_17_HI, 100, A6XX_SP_PERFCTR_SP_SEL_17 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_18_LO,
		A6XX_RBBM_PERFCTR_SP_18_HI, 101, A6XX_SP_PERFCTR_SP_SEL_18 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_19_LO,
		A6XX_RBBM_PERFCTR_SP_19_HI, 102, A6XX_SP_PERFCTR_SP_SEL_19 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_20_LO,
		A6XX_RBBM_PERFCTR_SP_20_HI, 103, A6XX_SP_PERFCTR_SP_SEL_20 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_21_LO,
		A6XX_RBBM_PERFCTR_SP_21_HI, 104, A6XX_SP_PERFCTR_SP_SEL_21 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_22_LO,
		A6XX_RBBM_PERFCTR_SP_22_HI, 105, A6XX_SP_PERFCTR_SP_SEL_22 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_SP_23_LO,
		A6XX_RBBM_PERFCTR_SP_23_HI, 106, A6XX_SP_PERFCTR_SP_SEL_23 },
};

static struct adreno_perfcount_register a6xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_0_LO,
		A6XX_RBBM_PERFCTR_RB_0_HI, 107, A6XX_RB_PERFCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_1_LO,
		A6XX_RBBM_PERFCTR_RB_1_HI, 108, A6XX_RB_PERFCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_2_LO,
		A6XX_RBBM_PERFCTR_RB_2_HI, 109, A6XX_RB_PERFCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_3_LO,
		A6XX_RBBM_PERFCTR_RB_3_HI, 110, A6XX_RB_PERFCTR_RB_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_4_LO,
		A6XX_RBBM_PERFCTR_RB_4_HI, 111, A6XX_RB_PERFCTR_RB_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_5_LO,
		A6XX_RBBM_PERFCTR_RB_5_HI, 112, A6XX_RB_PERFCTR_RB_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_6_LO,
		A6XX_RBBM_PERFCTR_RB_6_HI, 113, A6XX_RB_PERFCTR_RB_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_RB_7_LO,
		A6XX_RBBM_PERFCTR_RB_7_HI, 114, A6XX_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_perfcount_register a6xx_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VSC_0_LO,
		A6XX_RBBM_PERFCTR_VSC_0_HI, 115, A6XX_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_VSC_1_LO,
		A6XX_RBBM_PERFCTR_VSC_1_HI, 116, A6XX_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_perfcount_register a6xx_perfcounters_lrz[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_LRZ_0_LO,
		A6XX_RBBM_PERFCTR_LRZ_0_HI, 117, A6XX_GRAS_PERFCTR_LRZ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_LRZ_1_LO,
		A6XX_RBBM_PERFCTR_LRZ_1_HI, 118, A6XX_GRAS_PERFCTR_LRZ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_LRZ_2_LO,
		A6XX_RBBM_PERFCTR_LRZ_2_HI, 119, A6XX_GRAS_PERFCTR_LRZ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_LRZ_3_LO,
		A6XX_RBBM_PERFCTR_LRZ_3_HI, 120, A6XX_GRAS_PERFCTR_LRZ_SEL_3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_cmp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CMP_0_LO,
		A6XX_RBBM_PERFCTR_CMP_0_HI, 121, A6XX_RB_PERFCTR_CMP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CMP_1_LO,
		A6XX_RBBM_PERFCTR_CMP_1_HI, 122, A6XX_RB_PERFCTR_CMP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CMP_2_LO,
		A6XX_RBBM_PERFCTR_CMP_2_HI, 123, A6XX_RB_PERFCTR_CMP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_RBBM_PERFCTR_CMP_3_LO,
		A6XX_RBBM_PERFCTR_CMP_3_HI, 124, A6XX_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_vbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_CNT_LOW0,
		A6XX_VBIF_PERF_CNT_HIGH0, -1, A6XX_VBIF_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_CNT_LOW1,
		A6XX_VBIF_PERF_CNT_HIGH1, -1, A6XX_VBIF_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_CNT_LOW2,
		A6XX_VBIF_PERF_CNT_HIGH2, -1, A6XX_VBIF_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_CNT_LOW3,
		A6XX_VBIF_PERF_CNT_HIGH3, -1, A6XX_VBIF_PERF_CNT_SEL3 },
};

static struct adreno_perfcount_register a6xx_perfcounters_vbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_PWR_CNT_LOW0,
		A6XX_VBIF_PERF_PWR_CNT_HIGH0, -1, A6XX_VBIF_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_PWR_CNT_LOW1,
		A6XX_VBIF_PERF_PWR_CNT_HIGH1, -1, A6XX_VBIF_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_VBIF_PERF_PWR_CNT_LOW2,
		A6XX_VBIF_PERF_PWR_CNT_HIGH2, -1, A6XX_VBIF_PERF_PWR_CNT_EN2 },
};


static struct adreno_perfcount_register a6xx_perfcounters_gbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PERF_CNT_LOW0,
		A6XX_GBIF_PERF_CNT_HIGH0, -1, A6XX_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PERF_CNT_LOW1,
		A6XX_GBIF_PERF_CNT_HIGH1, -1, A6XX_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PERF_CNT_LOW2,
		A6XX_GBIF_PERF_CNT_HIGH2, -1, A6XX_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PERF_CNT_LOW3,
		A6XX_GBIF_PERF_CNT_HIGH3, -1, A6XX_GBIF_PERF_CNT_SEL },
};

static struct adreno_perfcount_register a6xx_perfcounters_gbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PWR_CNT_LOW0,
		A6XX_GBIF_PWR_CNT_HIGH0, -1, A6XX_GBIF_PERF_PWR_CNT_EN },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PWR_CNT_LOW1,
		A6XX_GBIF_PWR_CNT_HIGH1, -1, A6XX_GBIF_PERF_PWR_CNT_EN },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_GBIF_PWR_CNT_LOW2,
		A6XX_GBIF_PWR_CNT_HIGH2, -1, A6XX_GBIF_PERF_PWR_CNT_EN },
};

static struct adreno_perfcount_register a6xx_perfcounters_pwr[] = {
	{ KGSL_PERFCOUNTER_BROKEN, 0, 0, 0, 0, -1, 0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_H, -1, 0 },
};

static struct adreno_perfcount_register a6xx_perfcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_CP_ALWAYS_ON_COUNTER_LO,
		A6XX_CP_ALWAYS_ON_COUNTER_HI, -1 },
};

static struct adreno_perfcount_register a6xx_pwrcounters_gpmu[] = {
	/*
	 * A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0 is used for the GPU
	 * busy count (see the PWR group above). Mark it as broken
	 * so it's not re-used.
	 */
	{ KGSL_PERFCOUNTER_BROKEN, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1, },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_H, -1,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1, },
};

/*
 * ADRENO_PERFCOUNTER_GROUP_RESTORE flag is enabled by default
 * because most of the perfcounter groups need to be restored
 * as part of preemption and IFPC. Perfcounter groups that are
 * not restored as part of preemption and IFPC should be defined
 * using A6XX_PERFCOUNTER_GROUP_FLAGS macro
 */
#define A6XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a6xx, offset, name, \
	ADRENO_PERFCOUNTER_GROUP_RESTORE)

#define A6XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a6xx, offset, name, flags)

#define A6XX_POWER_COUNTER_GROUP(offset, name) \
	ADRENO_POWER_COUNTER_GROUP(a6xx, offset, name)

static struct adreno_perfcount_group a6xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A6XX_PERFCOUNTER_GROUP(CP, cp),
	A6XX_PERFCOUNTER_GROUP_FLAGS(RBBM, rbbm, 0),
	A6XX_PERFCOUNTER_GROUP(PC, pc),
	A6XX_PERFCOUNTER_GROUP(VFD, vfd),
	A6XX_PERFCOUNTER_GROUP(HLSQ, hlsq),
	A6XX_PERFCOUNTER_GROUP(VPC, vpc),
	A6XX_PERFCOUNTER_GROUP(CCU, ccu),
	A6XX_PERFCOUNTER_GROUP(CMP, cmp),
	A6XX_PERFCOUNTER_GROUP(TSE, tse),
	A6XX_PERFCOUNTER_GROUP(RAS, ras),
	A6XX_PERFCOUNTER_GROUP(LRZ, lrz),
	A6XX_PERFCOUNTER_GROUP(UCHE, uche),
	A6XX_PERFCOUNTER_GROUP(TP, tp),
	A6XX_PERFCOUNTER_GROUP(SP, sp),
	A6XX_PERFCOUNTER_GROUP(RB, rb),
	A6XX_PERFCOUNTER_GROUP(VSC, vsc),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF, vbif, 0),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A6XX_PERFCOUNTER_GROUP_FLAGS(PWR, pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A6XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED),
	A6XX_POWER_COUNTER_GROUP(GPMU, gpmu),
};

static struct adreno_perfcounters a6xx_perfcounters = {
	a6xx_perfcounter_groups,
	ARRAY_SIZE(a6xx_perfcounter_groups),
};

/* Program the GMU power counter to count GPU busy cycles */
static int a6xx_enable_pwr_counters(struct adreno_device *adreno_dev,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * We have a limited number of power counters. Since we're not using
	 * total GPU cycle count, return error if requested.
	 */
	if (counter == 0)
		return -EINVAL;

	/* We can use GPU without GMU and allow it to count GPU busy cycles */
	if (!gmu_core_isenabled(device) &&
			!kgsl_is_register_offset(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK))
		return -ENODEV;

	kgsl_regwrite(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK, 0xFF000000);

	/*
	 * A610 GPU has only one power counter fixed to count GPU busy
	 * cycles with no select register.
	 */
	if (!adreno_is_a610(adreno_dev))
		kgsl_regrmw(device,
			A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, 0xFF, 0x20);
	kgsl_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0x1);

	return 0;
}

static void a6xx_efuse_gaming_bin(struct adreno_device *adreno_dev)
{
	unsigned int val;
	unsigned int gaming_bin[3];
	struct kgsl_device *device = &adreno_dev->dev;

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		"qcom,gpu-gaming-bin", gaming_bin, 3))
		return;

	adreno_efuse_read_u32(adreno_dev, gaming_bin[0], &val);

	/* If fuse bit is set that means its not a gaming bin */
	adreno_dev->gaming_bin = !((val & gaming_bin[1]) >> gaming_bin[2]);
}

static void a6xx_efuse_speed_bin(struct adreno_device *adreno_dev)
{
	unsigned int val;
	unsigned int speed_bin[3];
	struct kgsl_device *device = &adreno_dev->dev;

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		"qcom,gpu-speed-bin", speed_bin, 3))
		return;

	adreno_efuse_read_u32(adreno_dev, speed_bin[0], &val);

	adreno_dev->speed_bin = (val & speed_bin[1]) >> speed_bin[2];
}

static void a6xx_efuse_power_features(struct adreno_device *adreno_dev)
{
	a6xx_efuse_speed_bin(adreno_dev);

	if (!adreno_dev->speed_bin)
		clear_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);
}

static const struct {
	int (*check)(struct adreno_device *adreno_dev);
	void (*func)(struct adreno_device *adreno_dev);
} a6xx_efuse_funcs[] = {
	{ adreno_is_a615_family, a6xx_efuse_speed_bin },
	{ adreno_is_a612, a6xx_efuse_speed_bin },
	{ adreno_is_a610, a6xx_efuse_gaming_bin },
	{ adreno_is_a640, a6xx_efuse_power_features },
};

static void a6xx_check_features(struct adreno_device *adreno_dev)
{
	unsigned int i;

	if (adreno_efuse_map(adreno_dev))
		return;
	for (i = 0; i < ARRAY_SIZE(a6xx_efuse_funcs); i++) {
		if (a6xx_efuse_funcs[i].check(adreno_dev))
			a6xx_efuse_funcs[i].func(adreno_dev);
	}

	adreno_efuse_unmap(adreno_dev);
}
static void a6xx_platform_setup(struct adreno_device *adreno_dev)
{
	uint64_t addr;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* Calculate SP local and private mem addresses */
	addr = ALIGN(adreno_dev->uche_gmem_base + adreno_dev->gmem_size,
					SZ_64K);
	adreno_dev->sp_local_gpuaddr = addr;
	adreno_dev->sp_pvt_gpuaddr = addr + SZ_64K;

	if (adreno_has_gbif(adreno_dev)) {
		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs =
				a6xx_perfcounters_gbif;
		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF].reg_count
				= ARRAY_SIZE(a6xx_perfcounters_gbif);

		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs =
				a6xx_perfcounters_gbif_pwr;
		a6xx_perfcounter_groups[
			KGSL_PERFCOUNTER_GROUP_VBIF_PWR].reg_count
				= ARRAY_SIZE(a6xx_perfcounters_gbif_pwr);

		gpudev->gbif_client_halt_mask = A6XX_GBIF_CLIENT_HALT_MASK;
		gpudev->gbif_arb_halt_mask = A6XX_GBIF_ARB_HALT_MASK;
		gpudev->gbif_gx_halt_mask = A6XX_GBIF_GX_HALT_MASK;
	} else
		gpudev->vbif_xin_halt_ctrl0_mask =
				A6XX_VBIF_XIN_HALT_CTRL0_MASK;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		set_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag);

	/* Check efuse bits for various capabilties */
	a6xx_check_features(adreno_dev);
}


static unsigned int a6xx_ccu_invalidate(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	/* CCU_INVALIDATE_DEPTH */
	*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
	*cmds++ = 24;

	/* CCU_INVALIDATE_COLOR */
	*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
	*cmds++ = 25;

	return 4;
}

/* Register offset defines for A6XX, in order of enum adreno_regs */
static unsigned int a6xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {

	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A6XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, A6XX_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_LO,
				A6XX_CP_RB_RPTR_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_HI,
				A6XX_CP_RB_RPTR_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A6XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A6XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A6XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A6XX_CP_SQE_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CNTL, A6XX_CP_MISC_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_HW_FAULT, A6XX_CP_HW_FAULT),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A6XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, A6XX_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A6XX_CP_IB1_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A6XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, A6XX_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A6XX_CP_IB2_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_ADDR, A6XX_CP_ROQ_DBG_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_DATA, A6XX_CP_ROQ_DBG_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT, A6XX_CP_CONTEXT_SWITCH_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
			A6XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
			A6XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_LEVEL_STATUS,
			A6XX_CP_CONTEXT_SWITCH_LEVEL_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A6XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, A6XX_RBBM_STATUS3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_CTL, A6XX_RBBM_PERFCTR_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A6XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A6XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
					A6XX_RBBM_PERFCTR_LOAD_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
					A6XX_RBBM_PERFCTR_LOAD_CMD3),

	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A6XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A6XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A6XX_RBBM_CLOCK_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A6XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A6XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
					  A6XX_RBBM_BLOCK_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
					  A6XX_RBBM_BLOCK_SW_RESET_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
				A6XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
				A6XX_RBBM_PERFCTR_LOAD_VALUE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_VERSION, A6XX_VBIF_VERSION),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL0,
				A6XX_VBIF_XIN_HALT_CTRL0),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL1,
				A6XX_VBIF_XIN_HALT_CTRL1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_GPR0_CNTL, A6XX_RBBM_GPR0_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_VBIF_GX_RESET_STATUS,
				A6XX_RBBM_VBIF_GX_RESET_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_GBIF_HALT,
				A6XX_RBBM_GBIF_HALT),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_GBIF_HALT_ACK,
				A6XX_RBBM_GBIF_HALT_ACK),
	ADRENO_REG_DEFINE(ADRENO_REG_GBIF_HALT, A6XX_GBIF_HALT),
	ADRENO_REG_DEFINE(ADRENO_REG_GBIF_HALT_ACK, A6XX_GBIF_HALT_ACK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
				A6XX_GMU_ALWAYS_ON_COUNTER_L),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
				A6XX_GMU_ALWAYS_ON_COUNTER_H),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_AHB_FENCE_CTRL,
				A6XX_GMU_AO_AHB_FENCE_CTRL),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_INTERRUPT_EN,
				A6XX_GMU_AO_INTERRUPT_EN),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
				A6XX_GMU_AO_HOST_INTERRUPT_CLR),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_STATUS,
				A6XX_GMU_AO_HOST_INTERRUPT_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
				A6XX_GMU_AO_HOST_INTERRUPT_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_PWR_COL_KEEPALIVE,
				A6XX_GMU_GMU_PWR_COL_KEEPALIVE),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AHB_FENCE_STATUS,
				A6XX_GMU_AHB_FENCE_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HFI_CTRL_STATUS,
				A6XX_GMU_HFI_CTRL_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HFI_VERSION_INFO,
				A6XX_GMU_HFI_VERSION_INFO),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HFI_SFR_ADDR,
				A6XX_GMU_HFI_SFR_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_RPMH_POWER_STATE,
				A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
				A6XX_GMU_GMU2HOST_INTR_CLR),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_INFO,
				A6XX_GMU_GMU2HOST_INTR_INFO),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
				A6XX_GMU_GMU2HOST_INTR_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HOST2GMU_INTR_SET,
				A6XX_GMU_HOST2GMU_INTR_SET),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HOST2GMU_INTR_CLR,
				A6XX_GMU_HOST2GMU_INTR_CLR),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_HOST2GMU_INTR_RAW_INFO,
				A6XX_GMU_HOST2GMU_INTR_RAW_INFO),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_NMI_CONTROL_STATUS,
				A6XX_GMU_NMI_CONTROL_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_CM3_CFG,
				A6XX_GMU_CM3_CFG),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_RBBM_INT_UNMASKED_STATUS,
				A6XX_GMU_RBBM_INT_UNMASKED_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TRUST_CONTROL,
				A6XX_RBBM_SECVID_TRUST_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
				A6XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
				A6XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
				A6XX_RBBM_SECVID_TSB_TRUSTED_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SECVID_TSB_CONTROL,
				A6XX_RBBM_SECVID_TSB_CNTL),
};

static const struct adreno_reg_offsets a6xx_reg_offsets = {
	.offsets = a6xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

static void a6xx_perfcounter_init(struct adreno_device *adreno_dev)
{
	/*
	 * A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4/5 is not present on A612.
	 * Mark them as broken so that they can't be used.
	 */
	if (adreno_is_a612(adreno_dev)) {
		a6xx_pwrcounters_gpmu[4].countable = KGSL_PERFCOUNTER_BROKEN;
		a6xx_pwrcounters_gpmu[5].countable = KGSL_PERFCOUNTER_BROKEN;
	} else if (adreno_is_a610(adreno_dev)) {
		/*
		 * A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1 to 5 are not
		 * present on A610. Mark them as broken so that they
		 * can't be used.
		 */
		a6xx_pwrcounters_gpmu[1].countable = KGSL_PERFCOUNTER_BROKEN;
		a6xx_pwrcounters_gpmu[2].countable = KGSL_PERFCOUNTER_BROKEN;
		a6xx_pwrcounters_gpmu[3].countable = KGSL_PERFCOUNTER_BROKEN;
		a6xx_pwrcounters_gpmu[4].countable = KGSL_PERFCOUNTER_BROKEN;
		a6xx_pwrcounters_gpmu[5].countable = KGSL_PERFCOUNTER_BROKEN;
	}
}

static int a6xx_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct cpu_gpu_lock *lock = adreno_dev->pwrup_reglist.hostptr;
	struct reg_list_pair *reg_pair = (struct reg_list_pair *)(lock + 1);
	unsigned int i;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	int ret = 0;

	lock->flag_kmd = 1;
	/* Write flag_kmd before turn */
	wmb();
	lock->turn = 0;
	/* Write these fields before looping */
	mb();

	/*
	 * Spin here while GPU ucode holds the lock, lock->flag_ucode will
	 * be set to 0 after GPU ucode releases the lock. Minimum wait time
	 * is 1 second and this should be enough for GPU to release the lock
	 */
	while (lock->flag_ucode == 1 && lock->turn == 0) {
		cpu_relax();
		/* Get the latest updates from GPU */
		rmb();
		/*
		 * Make sure we wait at least 1sec for the lock,
		 * if we did not get it after 1sec return an error.
		 */
		if (time_after(jiffies, timeout) &&
			(lock->flag_ucode == 1 && lock->turn == 0)) {
			ret = -EBUSY;
			goto unlock;
		}
	}

	/* Read flag_ucode and turn before list_length */
	rmb();
	/*
	 * If the perfcounter select register is already present in reglist
	 * update it, otherwise append the <select register, value> pair to
	 * the end of the list.
	 */
	for (i = 0; i < lock->list_length >> 1; i++)
		if (reg_pair[i].offset == reg->select)
			break;
	/*
	 * If the perfcounter selct register is not present overwrite last entry
	 * with new entry and add RBBM perf counter enable at the end.
	 */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_PERFCTRL_RETAIN) &&
			(i == lock->list_length >> 1)) {
		reg_pair[i-1].offset = reg->select;
		reg_pair[i-1].val = reg->countable;

		/* Enable perf counter after performance counter selections */
		reg_pair[i].offset = A6XX_RBBM_PERFCTR_CNTL;
		reg_pair[i].val = 1;

	} else {
		/*
		 * If perf counter select register is already present in reglist
		 * just update list without adding the RBBM perfcontrol enable.
		 */
		reg_pair[i].offset = reg->select;
		reg_pair[i].val = reg->countable;
	}

	if (i == lock->list_length >> 1)
		lock->list_length += 2;

	if (update_reg)
		kgsl_regwrite(device, reg->select, reg->countable);

unlock:
	/* All writes done before releasing the lock */
	wmb();
	lock->flag_kmd = 0;
	return ret;
}

static void a6xx_clk_set_options(struct adreno_device *adreno_dev,
	const char *name, struct clk *clk, bool on)
{
	if (!adreno_is_a610(adreno_dev))
		return;

	/* Handle clock settings for GFX PSCBCs */
	if (on) {
		if (!strcmp(name, "mem_iface_clk")) {
			clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		} else if (!strcmp(name, "core_clk")) {
			clk_set_flags(clk, CLKFLAG_RETAIN_PERIPH);
			clk_set_flags(clk, CLKFLAG_RETAIN_MEM);
		}
	} else {
		if (!strcmp(name, "core_clk")) {
			clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		}
	}
}

/*
 * Secure buffers cannot be preserved during hibernation.
 * Issue hyp_assign call to assign non-used internal secure
 * buffers to kernel.
 * This function will fail if there is an active secure context
 * since we cannot remove the content from user secure buffer.
 */
static int a6xx_secure_pt_hibernate(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	unsigned int i = 0;
	int ret;

	if (adreno_drawctxt_has_secure(device)) {
		KGSL_DRV_ERR(device,
		    "Secure context is active, cannot hibernate secure PT\n");
		goto fail;
	}

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->secure_preemption_desc.sgt) {
			ret = kgsl_unlock_sgt(rb->secure_preemption_desc.sgt);
			if (ret) {
				KGSL_DRV_ERR(device,
				    "kgsl_unlock_sgt failed ret %d\n", ret);
				goto fail;
			}
		}
	}

	return 0;

fail:
	while (i > 0) {
		rb = &(adreno_dev->ringbuffers[i - 1]);
		if (rb->secure_preemption_desc.sgt)
			kgsl_lock_sgt(rb->secure_preemption_desc.sgt,
					rb->secure_preemption_desc.size);
		i--;
	}
	return -EBUSY;
}

static int a6xx_secure_pt_restore(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	unsigned int i;
	int ret;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->secure_preemption_desc.sgt) {
			ret = kgsl_lock_sgt(rb->secure_preemption_desc.sgt,
					rb->secure_preemption_desc.size);
			if (ret) {
				KGSL_DRV_ERR(device,
				    "kgsl_lock_sgt failed ret %d\n", ret);
				return ret;
			}
		}
	}

	return 0;
}

struct adreno_gpudev adreno_a6xx_gpudev = {
	.reg_offsets = &a6xx_reg_offsets,
	.start = a6xx_start,
	.snapshot = a6xx_snapshot,
	.irq = &a6xx_irq,
	.irq_trace = trace_kgsl_a5xx_irq_status,
	.num_prio_levels = KGSL_PRIORITY_MAX_RB_LEVELS,
	.platform_setup = a6xx_platform_setup,
	.init = a6xx_init,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_sptprac_enable,
	.regulator_disable = a6xx_sptprac_disable,
	.perfcounters = &a6xx_perfcounters,
	.enable_pwr_counters = a6xx_enable_pwr_counters,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.count_throttles = a6xx_count_throttles,
	.microcode_read = a6xx_microcode_read,
	.enable_64bit = a6xx_enable_64bit,
	.llc_configure_gpu_scid = a6xx_llc_configure_gpu_scid,
	.llc_configure_gpuhtw_scid = a6xx_llc_configure_gpuhtw_scid,
	.llc_enable_overrides = a6xx_llc_enable_overrides,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a6xx_hw_isidle, /* Replaced by NULL if GMU is disabled */
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_reset,
	.soft_reset = a6xx_soft_reset,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_close = a6xx_preemption_close,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.preemption_context_destroy = a6xx_preemption_context_destroy,
	.sptprac_is_on = a6xx_sptprac_is_on,
	.ccu_invalidate = a6xx_ccu_invalidate,
	.perfcounter_init = a6xx_perfcounter_init,
	.perfcounter_update = a6xx_perfcounter_update,
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
	.clk_set_options = a6xx_clk_set_options,
	.snapshot_preemption = a6xx_snapshot_preemption,
	.zap_shader_unload = a6xx_zap_shader_unload,
	.secure_pt_hibernate = a6xx_secure_pt_hibernate,
	.secure_pt_restore = a6xx_secure_pt_restore,
};
