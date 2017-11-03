/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include "kgsl_gmu.h"
#include "kgsl_trace.h"

#define MIN_HBB		13

#define A6XX_LLC_NUM_GPU_SCIDS		5
#define A6XX_GPU_LLC_SCID_NUM_BITS	5
#define A6XX_GPU_LLC_SCID_MASK \
	((1 << (A6XX_LLC_NUM_GPU_SCIDS * A6XX_GPU_LLC_SCID_NUM_BITS)) - 1)
#define A6XX_GPUHTW_LLC_SCID_SHIFT	25
#define A6XX_GPUHTW_LLC_SCID_MASK \
	(((1 << A6XX_GPU_LLC_SCID_NUM_BITS) - 1) << A6XX_GPUHTW_LLC_SCID_SHIFT)

#define A6XX_GPU_CX_REG_BASE		0x509E000
#define A6XX_GPU_CX_REG_SIZE		0x1000

static int _load_gmu_firmware(struct kgsl_device *device);

static const struct adreno_vbif_data a630_vbif[] = {
	{A6XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000009},
	{A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_data a615_gbif[] = {
	{A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_platform a6xx_vbif_platforms[] = {
	{ adreno_is_a630, a630_vbif },
	{ adreno_is_a615, a615_gbif },
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
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_DELAY_SP1, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_DELAY_SP2, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_DELAY_SP3, 0x0000F3CF},
	{A6XX_RBBM_CLOCK_HYST_SP0, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP1, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP2, 0x00000080},
	{A6XX_RBBM_CLOCK_HYST_SP3, 0x00000080},
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
	{A6XX_RBBM_CLOCK_HYST3_TP0, 0x07777777},
	{A6XX_RBBM_CLOCK_HYST3_TP1, 0x07777777},
	{A6XX_RBBM_CLOCK_HYST3_TP2, 0x07777777},
	{A6XX_RBBM_CLOCK_HYST3_TP3, 0x07777777},
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
	{A6XX_RBBM_CLOCK_CNTL_CCU0, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_CCU1, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_CCU2, 0x00000000},
	{A6XX_RBBM_CLOCK_CNTL_CCU3, 0x00000000},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU0, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU1, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU2, 0x00040F00},
	{A6XX_RBBM_CLOCK_HYST_RB_CCU3, 0x00040F00},
	{A6XX_RBBM_CLOCK_CNTL_RAC, 0x00022022},
	{A6XX_RBBM_CLOCK_CNTL2_RAC, 0x00005550},
	{A6XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A6XX_RBBM_CLOCK_HYST_RAC, 0x00445044},
	{A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222220},
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x00202222},
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
	{A6XX_RBBM_CLOCK_CNTL_SP0,  0x22222222},
	{A6XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A6XX_RBBM_CLOCK_DELAY_SP0, 0x00000081},
	{A6XX_RBBM_CLOCK_HYST_SP0,  0x0000F3CF},
	{A6XX_RBBM_CLOCK_CNTL_TP0,  0x22222222},
	{A6XX_RBBM_CLOCK_CNTL_TP1,  0x22222222},
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
	{A6XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
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

static const struct {
	int (*devfunc)(struct adreno_device *adreno_dev);
	const struct kgsl_hwcg_reg *regs;
	unsigned int count;
} a6xx_hwcg_registers[] = {
	{adreno_is_a630, a630_hwcg_regs, ARRAY_SIZE(a630_hwcg_regs)},
	{adreno_is_a615, a615_hwcg_regs, ARRAY_SIZE(a615_hwcg_regs)},
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
	{ 0xE00, 0xE, 1 },
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
		PAGE_SIZE, KGSL_MEMFLAGS_GPUREADONLY, 0,
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
	if (!kgsl_gmu_isenabled(KGSL_DEVICE(adreno_dev)))
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
		mmu_range = 1 << mmu_prot->range;
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

#define RBBM_CLOCK_CNTL_ON 0x8AA8AA02

static void a6xx_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct kgsl_hwcg_reg *regs;
	unsigned int value;
	int i, j;

	if (!test_bit(ADRENO_HWCG_CTRL, &adreno_dev->pwrctrl_flag))
		on = false;

	if (kgsl_gmu_isenabled(device)) {
		kgsl_gmu_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL,
			on ? 0x00020222 : 0);
		kgsl_gmu_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,
			on ? 0x00010111 : 0);
		kgsl_gmu_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL,
			on ? 0x00050555 : 0);
	}

	kgsl_regread(device, A6XX_RBBM_CLOCK_CNTL, &value);

	if (value == RBBM_CLOCK_CNTL_ON && on)
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

	/* Disable SP clock before programming HWCG registers */
	kgsl_gmu_regrmw(device, A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 1, 0);

	for (j = 0; j < a6xx_hwcg_registers[i].count; j++)
		kgsl_regwrite(device, regs[j].off, on ? regs[j].val : 0);

	/* Enable SP clock */
	kgsl_gmu_regrmw(device, A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 0, 1);

	/* enable top level HWCG */
	kgsl_regwrite(device, A6XX_RBBM_CLOCK_CNTL,
		on ? RBBM_CLOCK_CNTL_ON : 0);
}

#define LM_DEFAULT_LIMIT	6000

static uint32_t lm_limit(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_dev->lm_limit)
		return adreno_dev->lm_limit;

	if (of_property_read_u32(device->pdev->dev.of_node, "qcom,lm-limit",
		&adreno_dev->lm_limit))
		adreno_dev->lm_limit = LM_DEFAULT_LIMIT;

	return adreno_dev->lm_limit;
}

static void a6xx_patch_pwrup_reglist(struct adreno_device *adreno_dev)
{
	uint32_t i;

	/* Set up the register values */
	for (i = 0; i < ARRAY_SIZE(a6xx_pwrup_reglist); i++) {
		struct reg_list_pair *r = &a6xx_pwrup_reglist[i];

		kgsl_regread(KGSL_DEVICE(adreno_dev), r->offset, &r->val);
	}

	/* Copy Preemption register/data pairs */
	memcpy(adreno_dev->pwrup_reglist.hostptr, &a6xx_pwrup_reglist,
		sizeof(a6xx_pwrup_reglist));
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
	unsigned int bit, mal, mode, glbl_inv;
	unsigned int amsbc = 0;
	static bool patch_reglist;

	/* runtime adjust callbacks based on feature sets */
	if (!kgsl_gmu_isenabled(device))
		/* Legacy idle management if gmu is disabled */
		ADRENO_GPU_DEVICE(adreno_dev)->hw_isidle = NULL;
	/* enable hardware clockgating */
	a6xx_hwcg_set(adreno_dev, true);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM))
		adreno_dev->lm_threshold_count = A6XX_GMU_GENERAL_1;

	adreno_vbif_start(adreno_dev, a6xx_vbif_platforms,
			ARRAY_SIZE(a6xx_vbif_platforms));

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
				ADRENO_UCHE_GMEM_BASE);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_LO,
				ADRENO_UCHE_GMEM_BASE +
				adreno_dev->gmem_size - 1);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	kgsl_regwrite(device, A6XX_UCHE_FILTER_CNTL, 0x804);
	kgsl_regwrite(device, A6XX_UCHE_CACHE_WAYS, 0x4);

	kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x010000C0);
	kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);

	/* Setting the mem pool size */
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 128);

	/* Setting the primFifo thresholds default values */
	kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x300 << 11));

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, A6XX_CP_AHB_CNTL, 0x1);

	/* Turn on performance counters */
	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_CNTL, 0x1);

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,highest-bank-bit", &bit))
		bit = MIN_HBB;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,min-access-length", &mal))
		mal = 32;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,ubwc-mode", &mode))
		mode = 0;

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

	/* Set hang detection threshold to 0x1FFFFF * 16 cycles */
	kgsl_regwrite(device, A6XX_RBBM_INTERFACE_HANG_INT_CNTL,
					  (1 << 30) | 0x1fffff);

	kgsl_regwrite(device, A6XX_UCHE_CLIENT_PF, 1);

	/* Set TWOPASSUSEWFI in A6XX_PC_DBG_ECO_CNTL if requested */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_TWO_PASS_USE_WFI))
		kgsl_regrmw(device, A6XX_PC_DBG_ECO_CNTL, 0, (1 << 8));

	/* Enable the GMEM save/restore feature for preemption */
	if (adreno_is_preemption_setup_enabled(adreno_dev))
		kgsl_regwrite(device, A6XX_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
			0x1);

	if (!patch_reglist && (adreno_dev->pwrup_reglist.gpuaddr != 0)) {
		a6xx_patch_pwrup_reglist(adreno_dev);
		patch_reglist = true;
	}

	a6xx_preemption_start(adreno_dev);
	a6xx_protect_init(adreno_dev);
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
	void *zap;
	int ret = 0;

	gpuaddr = fw->memdesc.gpuaddr;
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	/* Load the zap shader firmware through PIL if its available */
	if (adreno_dev->gpucore->zap_name && !adreno_dev->zap_loaded) {
		zap = subsystem_get(adreno_dev->gpucore->zap_name);

		/* Return error if the zap shader cannot be loaded */
		if (IS_ERR_OR_NULL(zap)) {
			ret = (zap == NULL) ? -ENODEV : PTR_ERR(zap);
			zap = NULL;
		} else
			adreno_dev->zap_loaded = 1;
	}

	return ret;
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

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK | \
		CP_INIT_OPERATION_MODE_MASK | \
		CP_INIT_REGISTER_INIT_LIST)

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

	if (CP_INIT_MASK & CP_INIT_REGISTER_INIT_LIST) {
		uint64_t gpuaddr = adreno_dev->pwrup_reglist.gpuaddr;

		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);
		/* Size is in dwords */
		*cmds++ = 0;
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
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 12);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_type7_packet(CP_ME_INIT, 11);

	_set_ordinals(adreno_dev, cmds, 11);

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret)
		adreno_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");

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
	cmds += cp_gpuaddr(adreno_dev, cmds, 0);

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

	if (!adreno_is_preemption_execution_enabled(adreno_dev))
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

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
			rb->buffer_desc.gpuaddr);

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
				KGSL_MEMFLAGS_GPUREADONLY, 0, "ucode");

	if (!ret) {
		memcpy(firmware->memdesc.hostptr, &fw->data[4], fw->size - 4);
		firmware->size = (fw->size - 4) / sizeof(uint32_t);
		firmware->version = *(unsigned int *)&fw->data[4];
	}

	release_firmware(fw);
	return ret;
}

#define RSC_CMD_OFFSET 2
#define PDC_CMD_OFFSET 4

static void _regwrite(void __iomem *regbase,
		unsigned int offsetwords, unsigned int value)
{
	void __iomem *reg;

	reg = regbase + (offsetwords << 2);
	__raw_writel(value, reg);
}

/*
 * _load_gmu_rpmh_ucode() - Load the ucode into the GPU PDC/RSC blocks
 * PDC and RSC execute GPU power on/off RPMh sequence
 * @device: Pointer to KGSL device
 */
static void _load_gmu_rpmh_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;

	/* Disable SDE clock gating */
	kgsl_gmu_regwrite(device, A6XX_GPU_RSCC_RSC_STATUS0_DRV0, BIT(24));

	/* Setup RSC PDC handshake for sleep and wakeup */
	kgsl_gmu_regwrite(device, A6XX_RSCC_PDC_SLAVE_ID_DRV0, 1);
	kgsl_gmu_regwrite(device, A6XX_RSCC_HIDDEN_TCS_CMD0_DATA, 0);
	kgsl_gmu_regwrite(device, A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR, 0);
	kgsl_gmu_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET, 0);
	kgsl_gmu_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET, 0);
	kgsl_gmu_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET * 2,
			0x80000000);
	kgsl_gmu_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET * 2,
			0);
	kgsl_gmu_regwrite(device, A6XX_RSCC_OVERRIDE_START_ADDR, 0);
	kgsl_gmu_regwrite(device, A6XX_RSCC_PDC_SEQ_START_ADDR, 0x4520);
	kgsl_gmu_regwrite(device, A6XX_RSCC_PDC_MATCH_VALUE_LO, 0x4510);
	kgsl_gmu_regwrite(device, A6XX_RSCC_PDC_MATCH_VALUE_HI, 0x4514);

	/* Enable timestamp event for v1 only */
	if (adreno_is_a630v1(adreno_dev))
		kgsl_gmu_regwrite(device, A6XX_RSCC_TIMESTAMP_UNIT1_EN_DRV0, 1);

	/* Load RSC sequencer uCode for sleep and wakeup */
	kgsl_gmu_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0, 0xA7A506A0);
	kgsl_gmu_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 1, 0xA1E6A6E7);
	kgsl_gmu_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 2, 0xA2E081E1);
	kgsl_gmu_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 3, 0xE9A982E2);
	kgsl_gmu_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 4, 0x0020E8A8);

	/* Load PDC sequencer uCode for power up and power down sequence */
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_MEM_0, 0xFEBEA1E1);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_MEM_0 + 1, 0xA5A4A3A2);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_MEM_0 + 2, 0x8382A6E0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_MEM_0 + 3, 0xBCE3E284);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_MEM_0 + 4, 0x002081FC);

	/* Set TCS commands used by PDC sequence for low power modes */
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CMD_ENABLE_BANK, 7);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CONTROL, 0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CMD0_MSGID, 0x10108);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CMD0_ADDR, 0x30010);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS1_CMD0_DATA, 1);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET, 0x30000);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET, 0x0);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET * 2, 0x30080);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CMD_ENABLE_BANK, 7);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CONTROL, 0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CMD0_MSGID, 0x10108);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CMD0_ADDR, 0x30010);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_TCS3_CMD0_DATA, 2);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET, 0x30000);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET, 0x3);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET * 2, 0x30080);
	_regwrite(gmu->pdc_reg_virt,
			PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x3);

	/* Setup GPU PDC */
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_SEQ_START_ADDR, 0);
	_regwrite(gmu->pdc_reg_virt, PDC_GPU_ENABLE_PDC, 0x80000001);

	/* ensure no writes happen before the uCode is fully written */
	wmb();
}

#define GMU_START_TIMEOUT	10	/* ms */
#define GPU_START_TIMEOUT	100	/* ms */
#define GPU_RESET_TIMEOUT	1	/* ms */
#define GPU_RESET_TIMEOUT_US	10	/* us */

/*
 * timed_poll_check() - polling *gmu* register at given offset until
 * its value changed to match expected value. The function times
 * out and returns after given duration if register is not updated
 * as expected.
 *
 * @device: Pointer to KGSL device
 * @offset: Register offset
 * @expected_ret: expected register value that stops polling
 * @timout: number of jiffies to abort the polling
 * @mask: bitmask to filter register value to match expected_ret
 */
static int timed_poll_check(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout, unsigned int mask)
{
	unsigned long t;
	unsigned int value;

	t = jiffies + msecs_to_jiffies(timeout);

	do {
		kgsl_gmu_regread(device, offset, &value);
		if ((value & mask) == expected_ret)
			return 0;
		/* Wait 100us to reduce unnecessary AHB bus traffic */
		udelay(100);
	} while (!time_after(jiffies, t));

	/* Double check one last time */
	kgsl_gmu_regread(device, offset, &value);
	if ((value & mask) == expected_ret)
		return 0;

	return -EINVAL;
}

/*
 * The lowest 16 bits of this value are the number of XO clock cycles
 * for main hysteresis. This is the first hysteresis. Here we set it
 * to 0x1680 cycles, or 300 us. The highest 16 bits of this value are
 * the number of XO clock cycles for short hysteresis. This happens
 * after main hysteresis. Here we set it to 0xA cycles, or 0.5 us.
 */
#define GMU_PWR_COL_HYST 0x000A1680

/*
 * a6xx_gmu_power_config() - Configure and enable GMU's low power mode
 * setting based on ADRENO feature flags.
 * @device: Pointer to KGSL device
 */
static void a6xx_gmu_power_config(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;

	/* Configure registers for idle setting. The setting is cumulative */

	/* Disable GMU WB/RB buffer */
	kgsl_gmu_regwrite(device, A6XX_GMU_SYS_BUS_CONFIG, 0x1);

	kgsl_gmu_regwrite(device,
		A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,  0x9C40400);

	switch (gmu->idle_level) {
	case GPU_HW_MIN_VOLT:
		kgsl_gmu_regrmw(device, A6XX_GMU_RPMH_CTRL, 0,
				MIN_BW_ENABLE_MASK);
		kgsl_gmu_regrmw(device, A6XX_GMU_RPMH_HYST_CTRL, 0,
				MIN_BW_HYST);
		/* fall through */
	case GPU_HW_NAP:
		kgsl_gmu_regrmw(device, A6XX_GMU_GPU_NAP_CTRL, 0,
				HW_NAP_ENABLE_MASK);
		/* fall through */
	case GPU_HW_IFPC:
		kgsl_gmu_regwrite(device, A6XX_GMU_PWR_COL_INTER_FRAME_HYST,
				GMU_PWR_COL_HYST);
		kgsl_gmu_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL, 0,
				IFPC_ENABLE_MASK);
		/* fall through */
	case GPU_HW_SPTP_PC:
		kgsl_gmu_regwrite(device, A6XX_GMU_PWR_COL_SPTPRAC_HYST,
				GMU_PWR_COL_HYST);
		kgsl_gmu_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL, 0,
				SPTP_ENABLE_MASK);
		/* fall through */
	default:
		break;
	}

	/* ACD feature enablement */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
		test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		kgsl_gmu_regrmw(device, A6XX_GMU_BOOT_KMD_LM_HANDSHAKE, 0,
				BIT(10));

	/* Enable RPMh GPU client */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_RPMH))
		kgsl_gmu_regrmw(device, A6XX_GMU_RPMH_CTRL, 0,
				RPMH_ENABLE_MASK);
}

/*
 * a6xx_gmu_start() - Start GMU and wait until FW boot up.
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_start(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;

	/* Write 1 first to make sure the GMU is reset */
	kgsl_gmu_regwrite(device, A6XX_GMU_CM3_SYSRESET, 1);

	/* Make sure putting in reset doesn't happen after clearing */
	wmb();

	/* Bring GMU out of reset */
	kgsl_gmu_regwrite(device, A6XX_GMU_CM3_SYSRESET, 0);
	if (timed_poll_check(device,
			A6XX_GMU_CM3_FW_INIT_RESULT,
			0xBABEFACE,
			GMU_START_TIMEOUT,
			0xFFFFFFFF)) {
		dev_err(&gmu->pdev->dev, "GMU doesn't boot\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * a6xx_gmu_hfi_start() - Write registers and start HFI.
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_hfi_start(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;

	kgsl_gmu_regrmw(device, A6XX_GMU_GMU2HOST_INTR_MASK,
			HFI_IRQ_MSGQ_MASK, 0);
	kgsl_gmu_regwrite(device, A6XX_GMU_HFI_CTRL_INIT, 1);

	if (timed_poll_check(device,
			A6XX_GMU_HFI_CTRL_STATUS,
			BIT(0),
			GMU_START_TIMEOUT,
			BIT(0))) {
		dev_err(&gmu->pdev->dev, "GMU HFI init failed\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * a6xx_oob_set() - Set OOB interrupt to GMU.
 * @adreno_dev: Pointer to adreno device
 * @set_mask: set_mask is a bitmask that defines a set of OOB
 *	interrupts to trigger.
 * @check_mask: check_mask is a bitmask that provides a set of
 *	OOB ACK bits. check_mask usually matches set_mask to
 *	ensure OOBs are handled.
 * @clear_mask: After GMU handles a OOB interrupt, GMU driver
 *	clears the interrupt. clear_mask is a bitmask defines
 *	a set of OOB interrupts to clear.
 */
static int a6xx_oob_set(struct adreno_device *adreno_dev,
		unsigned int set_mask, unsigned int check_mask,
		unsigned int clear_mask)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;
	int ret = 0;

	if (!kgsl_gmu_isenabled(device))
		return 0;

	kgsl_gmu_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, set_mask);

	if (timed_poll_check(device,
			A6XX_GMU_GMU2HOST_INTR_INFO,
			check_mask,
			GPU_START_TIMEOUT,
			check_mask)) {
		ret = -ETIMEDOUT;
		dev_err(&gmu->pdev->dev,
			"OOB set timed out, mask %x\n", set_mask);
		WARN_ON(true);
	}

	kgsl_gmu_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, clear_mask);

	trace_kgsl_gmu_oob_set(set_mask);
	return ret;
}

/*
 * a6xx_oob_clear() - Clear a previously set  OOB request.
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @clear_mask: Bitmask that provides the OOB bits to clear
 */
static inline void a6xx_oob_clear(struct adreno_device *adreno_dev,
		unsigned int clear_mask)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!kgsl_gmu_isenabled(device))
		return;

	kgsl_gmu_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, clear_mask);
	trace_kgsl_gmu_oob_clear(clear_mask);
}

/*
 * a6xx_gpu_keepalive() - GMU reg write to request GPU stays on
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @state: State to set: true is ON, false is OFF
 */
static inline void a6xx_gpu_keepalive(struct adreno_device *adreno_dev,
		bool state)
{
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_PWR_COL_KEEPALIVE, state);
}

#define SPTPRAC_POWERON_CTRL_MASK	0x00778000
#define SPTPRAC_POWEROFF_CTRL_MASK	0x00778001
#define SPTPRAC_POWEROFF_STATUS_MASK	BIT(2)
#define SPTPRAC_POWERON_STATUS_MASK	BIT(3)
#define SPTPRAC_CTRL_TIMEOUT		10 /* ms */
#define A6XX_RETAIN_FF_ENABLE_ENABLE_MASK BIT(11)

/*
 * a6xx_sptprac_enable() - Power on SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
static int a6xx_sptprac_enable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;

	if (!gmu->pdev)
		return -EINVAL;

	kgsl_gmu_regwrite(device, A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWERON_CTRL_MASK);

	if (timed_poll_check(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS,
			SPTPRAC_POWERON_STATUS_MASK,
			SPTPRAC_CTRL_TIMEOUT,
			SPTPRAC_POWERON_STATUS_MASK)) {
		dev_err(&gmu->pdev->dev, "power on SPTPRAC fail\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * a6xx_sptprac_disable() - Power of SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
static void a6xx_sptprac_disable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;

	if (!gmu->pdev)
		return;

	/* Ensure that retention is on */
	kgsl_gmu_regrmw(device, A6XX_GPU_CC_GX_GDSCR, 0,
			A6XX_RETAIN_FF_ENABLE_ENABLE_MASK);

	kgsl_gmu_regwrite(device, A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWEROFF_CTRL_MASK);

	if (timed_poll_check(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS,
			SPTPRAC_POWEROFF_STATUS_MASK,
			SPTPRAC_CTRL_TIMEOUT,
			SPTPRAC_POWEROFF_STATUS_MASK))
		dev_err(&gmu->pdev->dev, "power off SPTPRAC fail\n");
}

#define SPTPRAC_POWER_OFF	BIT(2)
#define SP_CLK_OFF		BIT(4)
#define GX_GDSC_POWER_OFF	BIT(6)
#define GX_CLK_OFF		BIT(7)

/*
 * a6xx_gx_is_on() - Check if GX is on using pwr status register
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
static bool a6xx_gx_is_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int val;

	if (!kgsl_gmu_isenabled(device))
		return true;

	kgsl_gmu_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return !(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF));
}

/*
 * a6xx_sptprac_is_on() - Check if SPTP is on using pwr status register
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
static bool a6xx_sptprac_is_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int val;

	if (!kgsl_gmu_isenabled(device))
		return true;

	kgsl_gmu_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return !(val & (SPTPRAC_POWER_OFF | SP_CLK_OFF));
}

/*
 * a6xx_gfx_rail_on() - request GMU to power GPU at given OPP.
 * @device: Pointer to KGSL device
 *
 */
static int a6xx_gfx_rail_on(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gmu_device *gmu = &device->gmu;
	struct arc_vote_desc *default_opp;
	unsigned int perf_idx;
	int ret;

	perf_idx = pwr->num_pwrlevels - pwr->default_pwrlevel - 1;
	default_opp = &gmu->rpmh_votes.gx_votes[perf_idx];

	kgsl_gmu_regwrite(device, A6XX_GMU_BOOT_SLUMBER_OPTION,
			OOB_BOOT_OPTION);
	kgsl_gmu_regwrite(device, A6XX_GMU_GX_VOTE_IDX, default_opp->pri_idx);
	kgsl_gmu_regwrite(device, A6XX_GMU_MX_VOTE_IDX, default_opp->sec_idx);

	ret = a6xx_oob_set(adreno_dev, OOB_BOOT_SLUMBER_SET_MASK,
			OOB_BOOT_SLUMBER_CHECK_MASK,
			OOB_BOOT_SLUMBER_CLEAR_MASK);

	if (ret)
		dev_err(&gmu->pdev->dev, "Boot OOB timed out\n");

	return ret;
}

#define GMU_POWER_STATE_SLUMBER 15

/*
 * a6xx_notify_slumber() - initiate request to GMU to prepare to slumber
 * @device: Pointer to KGSL device
 */
static int a6xx_notify_slumber(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gmu_device *gmu = &device->gmu;
	int bus_level = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;
	int perf_idx = gmu->num_gpupwrlevels - pwr->default_pwrlevel - 1;
	int ret, state;

	/* Disable the power counter so that the GMU is not busy */
	kgsl_gmu_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	/* Turn off SPTPRAC if we own it */
	if (gmu->idle_level < GPU_HW_SPTP_PC)
		a6xx_sptprac_disable(adreno_dev);

	if (!ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = hfi_notify_slumber(gmu, perf_idx, bus_level);
		goto out;
	}

	kgsl_gmu_regwrite(device, A6XX_GMU_BOOT_SLUMBER_OPTION,
			OOB_SLUMBER_OPTION);
	kgsl_gmu_regwrite(device, A6XX_GMU_GX_VOTE_IDX, bus_level);
	kgsl_gmu_regwrite(device, A6XX_GMU_MX_VOTE_IDX, perf_idx);

	ret = a6xx_oob_set(adreno_dev, OOB_BOOT_SLUMBER_SET_MASK,
			OOB_BOOT_SLUMBER_CHECK_MASK,
			OOB_BOOT_SLUMBER_CLEAR_MASK);
	a6xx_oob_clear(adreno_dev, OOB_BOOT_SLUMBER_CLEAR_MASK);

	if (ret)
		dev_err(&gmu->pdev->dev, "Notify slumber OOB timed out\n");
	else {
		kgsl_gmu_regread(device,
			A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &state);
		if (state != GPU_HW_SLUMBER) {
			dev_err(&gmu->pdev->dev,
					"Failed to prepare for slumber: 0x%x\n",
					state);
			ret = -EINVAL;
		}
	}

out:
	/* Make sure the fence is in ALLOW mode */
	kgsl_gmu_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
	return ret;
}

static int a6xx_rpmh_power_on_gpu(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;
	struct device *dev = &gmu->pdev->dev;
	int val;

	kgsl_gmu_regread(device, A6XX_GPU_CC_GX_DOMAIN_MISC, &val);
	if (!(val & 0x1))
		dev_err_ratelimited(&gmu->pdev->dev,
			"GMEM CLAMP IO not set while GFX rail off\n");

	/* RSC wake sequence */
	kgsl_gmu_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, BIT(1));

	/* Write request before polling */
	wmb();

	if (timed_poll_check(device,
			A6XX_GMU_RSCC_CONTROL_ACK,
			BIT(1),
			GPU_START_TIMEOUT,
			BIT(1))) {
		dev_err(dev, "Failed to do GPU RSC power on\n");
		return -EINVAL;
	}

	if (timed_poll_check(device,
			A6XX_RSCC_SEQ_BUSY_DRV0,
			0,
			GPU_START_TIMEOUT,
			0xFFFFFFFF))
		goto error_rsc;

	kgsl_gmu_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 0);

	/* Enable the power counter because it was disabled before slumber */
	kgsl_gmu_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	return 0;
error_rsc:
	dev_err(dev, "GPU RSC sequence stuck in waking up GPU\n");
	return -EINVAL;
}

static int a6xx_rpmh_power_off_gpu(struct kgsl_device *device)
{
	struct gmu_device *gmu = &device->gmu;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	/* RSC sleep sequence is different on v1 */
	if (adreno_is_a630v1(adreno_dev))
		kgsl_gmu_regwrite(device, A6XX_RSCC_TIMESTAMP_UNIT1_EN_DRV0, 1);

	kgsl_gmu_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 1);
	wmb();

	if (adreno_is_a630v1(adreno_dev))
		ret = timed_poll_check(device,
				A6XX_RSCC_TIMESTAMP_UNIT1_OUTPUT_DRV0,
				BIT(0),
				GPU_START_TIMEOUT,
				BIT(0));
	else
		ret = timed_poll_check(device,
				A6XX_GPU_RSCC_RSC_STATUS0_DRV0,
				BIT(16),
				GPU_START_TIMEOUT,
				BIT(16));

	if (ret) {
		dev_err(&gmu->pdev->dev, "GPU RSC power off fail\n");
		return -ETIMEDOUT;
	}

	/* Read to clear the timestamp valid signal. Don't care what we read. */
	if (adreno_is_a630v1(adreno_dev)) {
		kgsl_gmu_regread(device,
				A6XX_RSCC_TIMESTAMP_UNIT0_TIMESTAMP_L_DRV0,
				&ret);
		kgsl_gmu_regread(device,
				A6XX_RSCC_TIMESTAMP_UNIT0_TIMESTAMP_H_DRV0,
				&ret);
	}

	kgsl_gmu_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 0);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
			test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		kgsl_gmu_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 0);

	return 0;
}

#define KMASK(start, n) (GENMASK((start + n), (start)))

static void isense_cold_trimm(struct kgsl_device *device)
{
	unsigned int reg;
	struct gmu_device *gmu = &device->gmu;

	kgsl_gmu_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 1);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_AMP_CALIBRATION_DONE, 0);

	kgsl_gmu_regwrite(device, A6XX_GPU_GMU_CX_GMU_ISENSE_CTRL, 0x1);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_AMP_CALIBRATION_CONTROL3,
		0x00000F8F);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_AMP_CALIBRATION_CONTROL2,
		0x00705161);
	udelay(10);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_ENABLE_REG, 0x3);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_A_SENSOR_CTRL_0, 0x10040a);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_A_SENSOR_CTRL_2, 0x10040a);

	kgsl_gmu_regread(device, A6XX_GPU_CS_SENSOR_GENERAL_STATUS, &reg);
	if ((reg & BIT(CS_PWR_ON_STATUS)) != (1 << CS_PWR_ON_STATUS)) {
		dev_err(&gmu->pdev->dev, "ERROR - ISENSE power-up\n");
		return;
	}

	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_CALIBRATION_CONTROL1,
		KMASK(AMP_TRIM_TIMER, 15), 70 << AMP_TRIM_TIMER);
	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_CALIBRATION_CONTROL1,
		KMASK(AMP_SW_TRIM_START, 1), 0 << AMP_SW_TRIM_START);
	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_CALIBRATION_CONTROL1,
		KMASK(AMP_SW_TRIM_START, 1), 1 << AMP_SW_TRIM_START);

	if (timed_poll_check(device, A6XX_GPU_CS_SENSOR_GENERAL_STATUS,
		BIT(SS_AMPTRIM_DONE), GMU_START_TIMEOUT,
		BIT(SS_AMPTRIM_DONE))) {
		dev_err(&gmu->pdev->dev, "ISENSE SS_AMPTRIM failure\n");
		return;
	}

	kgsl_gmu_regread(device, A6XX_GPU_CS_AMP_CALIBRATION_STATUS1_0, &reg);
	if (reg & AMP_ERR) {
		kgsl_gmu_regread(device, A6XX_GPU_CS_AMP_CALIBRATION_STATUS1_0,
			&reg);
		dev_err(&gmu->pdev->dev,
			"ISENSE ERROR:trimming GX 0x%08x\n", reg);
		return;
	}

	kgsl_gmu_regread(device, A6XX_GPU_CS_AMP_CALIBRATION_STATUS1_2, &reg);
	if (reg & AMP_ERR) {
		kgsl_gmu_regread(device, A6XX_GPU_CS_AMP_CALIBRATION_STATUS1_2,
			&reg);
		dev_err(&gmu->pdev->dev,
			"ISENSE ERROR:trimming SPTPRAC 0x%08x\n", reg);
		return;
	}

	kgsl_gmu_regwrite(device, A6XX_GPU_CS_AMP_CALIBRATION_DONE, 1);
	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_PERIOD_CTRL,
		KMASK(TRIM_CNT_VALUE, 13), 20 << TRIM_CNT_VALUE);
	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_PERIOD_CTRL,
		KMASK(RUNTIME_CNT_VALUE, 9), 50 << RUNTIME_CNT_VALUE);

	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_PERIOD_CTRL,
		KMASK(TRIM_ENABLE, 1), 1 << TRIM_ENABLE);
	udelay(4);
	kgsl_gmu_regrmw(device, A6XX_GPU_CS_AMP_PERIOD_CTRL,
		KMASK(TRIM_ENABLE, 1), 0 << TRIM_ENABLE);
	kgsl_gmu_regwrite(device, A6XX_GPU_CS_AMP_CALIBRATION_DONE, 1);

}

#define GPU_LIMIT_THRESHOLD_ENABLE	BIT(31)
/*
 * a6xx_gmu_fw_start() - set up GMU and start FW
 * @device: Pointer to KGSL device
 * @boot_state: State of the GMU being started
 */
static int a6xx_gmu_fw_start(struct kgsl_device *device,
		unsigned int boot_state)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	int ret, i;
	unsigned int chipid = 0;

	switch (boot_state) {
	case GMU_RESET:
		/* fall through */
	case GMU_COLD_BOOT:
		/* Turn on TCM retention */
		kgsl_gmu_regwrite(device, A6XX_GMU_GENERAL_7, 1);

		if (!test_and_set_bit(GMU_BOOT_INIT_DONE, &gmu->flags))
			_load_gmu_rpmh_ucode(device);
		else if (boot_state != GMU_RESET) {
			ret = a6xx_rpmh_power_on_gpu(device);
			if (ret)
				return ret;
		}

		if (gmu->load_mode == TCM_BOOT) {
			/* Load GMU image via AHB bus */
			for (i = 0; i < MAX_GMUFW_SIZE; i++)
				kgsl_gmu_regwrite(device,
						A6XX_GMU_CM3_ITCM_START + i,
						*((uint32_t *) gmu->fw_image.
						hostptr + i));

			/* Prevent leaving reset before the FW is written */
			wmb();
		} else {
			dev_err(&gmu->pdev->dev, "Incorrect GMU load mode %d\n",
					gmu->load_mode);
			return -EINVAL;
		}
		break;
	case GMU_WARM_BOOT:
		ret = a6xx_rpmh_power_on_gpu(device);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	/* Clear init result to make sure we are getting fresh value */
	kgsl_gmu_regwrite(device, A6XX_GMU_CM3_FW_INIT_RESULT, 0);
	kgsl_gmu_regwrite(device, A6XX_GMU_CM3_BOOT_CONFIG, gmu->load_mode);

	kgsl_gmu_regwrite(device, A6XX_GMU_HFI_QTBL_ADDR,
			mem_addr->gmuaddr);
	kgsl_gmu_regwrite(device, A6XX_GMU_HFI_QTBL_INFO, 1);

	kgsl_gmu_regwrite(device, A6XX_GMU_AHB_FENCE_RANGE_0,
			FENCE_RANGE_MASK);

	/* Pass chipid to GMU FW, must happen before starting GMU */

	/* Keep Core and Major bitfields unchanged */
	chipid = adreno_dev->chipid & 0xFFFF0000;

	/*
	 * Compress minor and patch version into 8 bits
	 * Bit 15-12: minor version
	 * Bit 11-8: patch version
	 */
	chipid = chipid | (ADRENO_CHIPID_MINOR(adreno_dev->chipid) << 12)
			| (ADRENO_CHIPID_PATCH(adreno_dev->chipid) << 8);

	kgsl_gmu_regwrite(device, A6XX_GMU_HFI_SFR_ADDR, chipid);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
		test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag)) {
		kgsl_gmu_regwrite(device, A6XX_GPU_GMU_CX_GMU_PWR_THRESHOLD,
			GPU_LIMIT_THRESHOLD_ENABLE | lm_limit(adreno_dev));
		isense_cold_trimm(device);
	}

	/* Configure power control and bring the GMU out of reset */
	a6xx_gmu_power_config(device);
	ret = a6xx_gmu_start(device);
	if (ret)
		return ret;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = a6xx_gfx_rail_on(device);
		if (ret) {
			a6xx_oob_clear(adreno_dev,
					OOB_BOOT_SLUMBER_CLEAR_MASK);
			return ret;
		}
	}

	if (gmu->idle_level < GPU_HW_SPTP_PC) {
		ret = a6xx_sptprac_enable(adreno_dev);
		if (ret)
			return ret;
	}

	ret = a6xx_gmu_hfi_start(device);
	if (ret)
		return ret;

	/* Make sure the write to start HFI happens before sending a message */
	wmb();
	return ret;
}

/*
 * a6xx_gmu_dcvs_nohfi() - request GMU to do DCVS without using HFI
 * @device: Pointer to KGSL device
 * @perf_idx: Index into GPU performance level table defined in
 *	HFI DCVS table message
 * @bw_idx: Index into GPU b/w table defined in HFI b/w table message
 *
 */
static int a6xx_gmu_dcvs_nohfi(struct kgsl_device *device,
		unsigned int perf_idx, unsigned int bw_idx)
{
	struct hfi_dcvs_cmd dcvs_cmd = {
		.ack_type = ACK_NONBLOCK,
		.freq = {
			.perf_idx = perf_idx,
			.clkset_opt = OPTION_AT_LEAST,
		},
		.bw = {
			.bw_idx = bw_idx,
		},
	};
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;
	union gpu_perf_vote vote;
	int ret;

	kgsl_gmu_regwrite(device, A6XX_GMU_DCVS_ACK_OPTION, dcvs_cmd.ack_type);

	vote.fvote = dcvs_cmd.freq;
	kgsl_gmu_regwrite(device, A6XX_GMU_DCVS_PERF_SETTING, vote.raw);

	vote.bvote = dcvs_cmd.bw;
	kgsl_gmu_regwrite(device, A6XX_GMU_DCVS_BW_SETTING, vote.raw);

	ret = a6xx_oob_set(adreno_dev, OOB_DCVS_SET_MASK, OOB_DCVS_CHECK_MASK,
		OOB_DCVS_CLEAR_MASK);

	if (ret) {
		dev_err(&gmu->pdev->dev, "DCVS OOB timed out\n");
		goto done;
	}

	kgsl_gmu_regread(device, A6XX_GMU_DCVS_RETURN, &ret);
	if (ret)
		dev_err(&gmu->pdev->dev, "OOB DCVS error %d\n", ret);

done:
	a6xx_oob_clear(adreno_dev, OOB_DCVS_CLEAR_MASK);

	return ret;
}

static bool a6xx_hw_isidle(struct adreno_device *adreno_dev)
{
	unsigned int reg;

	kgsl_gmu_regread(KGSL_DEVICE(adreno_dev),
		A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg);
	if (reg & GPUBUSYIGNAHB)
		return false;
	return true;
}

static int a6xx_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int reg;
	unsigned long t;

	if (!kgsl_gmu_isenabled(device))
		return 0;

	t = jiffies + msecs_to_jiffies(GMU_IDLE_TIMEOUT);
	while (!time_after(jiffies, t)) {
		adreno_read_gmureg(ADRENO_DEVICE(device),
				ADRENO_REG_GMU_RPMH_POWER_STATE, &reg);

		/* SPTPRAC PC has the same idle level as IFPC */
		if ((reg == gmu->idle_level) ||
				(gmu->idle_level == GPU_HW_SPTP_PC &&
				reg == GPU_HW_IFPC)) {
			/* IFPC is not complete until GX is off */
			if (gmu->idle_level != GPU_HW_IFPC ||
					!gpudev->gx_is_on(adreno_dev))
				return 0;
		}

		/* Wait 100us to reduce unnecessary AHB bus traffic */
		udelay(100);
	}

	/* Check one last time */
	adreno_read_gmureg(ADRENO_DEVICE(device),
			ADRENO_REG_GMU_RPMH_POWER_STATE, &reg);
	if ((reg == gmu->idle_level) ||
			(gmu->idle_level == GPU_HW_SPTP_PC &&
			reg == GPU_HW_IFPC)) {
		if (gmu->idle_level != GPU_HW_IFPC ||
				!gpudev->gx_is_on(adreno_dev))
			return 0;
	}

	dev_err(&gmu->pdev->dev,
			"Timeout waiting for lowest idle level: %d\n", reg);
	return -ETIMEDOUT;
}

static int a6xx_wait_for_gmu_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;
	unsigned int status, status2;

	if (timed_poll_check(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, GMU_START_TIMEOUT, CXGXCPUBUSYIGNAHB)) {
		kgsl_gmu_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &status);
		kgsl_gmu_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status=0x%x, status2=0x%x\n",
				status, status2);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * _load_gmu_firmware() - Load the ucode into the GPMU RAM & PDC/RSC
 * @device: Pointer to KGSL device
 */
static int _load_gmu_firmware(struct kgsl_device *device)
{
	const struct firmware *fw = NULL;
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	int image_size, ret =  -EINVAL;

	/* there is no GMU */
	if (!kgsl_gmu_isenabled(device))
		return 0;

	/* GMU fw already saved and verified so do nothing new */
	if (gmu->fw_image.hostptr != 0)
		return 0;

	if (gpucore->gpmufw_name == NULL)
		return -EINVAL;

	ret = request_firmware(&fw, gpucore->gpmufw_name, device->dev);
	if (ret || fw == NULL) {
		KGSL_CORE_ERR("request_firmware (%s) failed: %d\n",
				gpucore->gpmufw_name, ret);
		return ret;
	}

	image_size = PAGE_ALIGN(fw->size);

	ret = allocate_gmu_image(gmu, image_size);

	/* load into shared memory with GMU */
	if (!ret)
		memcpy(gmu->fw_image.hostptr, fw->data, fw->size);

	release_firmware(fw);

	return ret;
}

/*
 * a6xx_microcode_read() - Read microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_microcode_read(struct adreno_device *adreno_dev)
{
	int ret;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *sqe_fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

	if (sqe_fw->memdesc.hostptr == NULL) {
		ret = _load_firmware(device, adreno_dev->gpucore->sqefw_name,
				sqe_fw);
		if (ret)
			return ret;
	}

	return _load_gmu_firmware(device);
}

#define VBIF_RESET_ACK_TIMEOUT	100
#define VBIF_RESET_ACK_MASK	0x00f0

static int a6xx_soft_reset(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;
	unsigned long time;
	bool vbif_acked = false;

	/*
	 * For the soft reset case with GMU enabled this part is done
	 * by the GMU firmware
	 */
	if (kgsl_gmu_isenabled(device) &&
		!test_bit(ADRENO_DEVICE_HARD_RESET, &adreno_dev->priv))
		return 0;


	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 1);
	/*
	 * Do a dummy read to get a brief read cycle delay for the
	 * reset to take effect
	 */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, &reg);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 0);

	/* Wait for the VBIF reset ack to complete */
	time = jiffies + msecs_to_jiffies(VBIF_RESET_ACK_TIMEOUT);

	do {
		kgsl_regread(device, A6XX_RBBM_VBIF_GX_RESET_STATUS, &reg);
		if ((reg & VBIF_RESET_ACK_MASK) == VBIF_RESET_ACK_MASK) {
			vbif_acked = true;
			break;
		}
		cpu_relax();
	} while (!time_after(jiffies, time));

	if (!vbif_acked)
		return -ETIMEDOUT;

	a6xx_sptprac_enable(adreno_dev);

	return 0;
}

#define A6XX_STATE_OF_CHILD             (BIT(4) | BIT(5))
#define A6XX_IDLE_FULL_LLM              BIT(0)
#define A6XX_WAKEUP_ACK                 BIT(1)
#define A6XX_IDLE_FULL_ACK              BIT(0)
#define A6XX_VBIF_XIN_HALT_CTRL1_ACKS   (BIT(0) | BIT(1) | BIT(2) | BIT(3))

static void a6xx_isense_disable(struct kgsl_device *device)
{
	unsigned int val;
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	kgsl_gmu_regread(device, A6XX_GPU_CS_ENABLE_REG, &val);
	if (val) {
		kgsl_gmu_regwrite(device, A6XX_GPU_CS_ENABLE_REG, 0);
		kgsl_gmu_regwrite(device, A6XX_GMU_ISENSE_CTRL, 0);
	}
}

static int a6xx_llm_glm_handshake(struct kgsl_device *device)
{
	unsigned int val;
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = &device->gmu;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return 0;

	kgsl_gmu_regread(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, &val);
	if (!(val & A6XX_STATE_OF_CHILD)) {
		kgsl_gmu_regrmw(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, 0, BIT(4));
		kgsl_gmu_regrmw(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, 0,
				A6XX_IDLE_FULL_LLM);
		if (timed_poll_check(device, A6XX_GMU_LLM_GLM_SLEEP_STATUS,
				A6XX_IDLE_FULL_ACK, GPU_RESET_TIMEOUT,
				A6XX_IDLE_FULL_ACK)) {
			dev_err(&gmu->pdev->dev, "LLM-GLM handshake failed\n");
			return -EINVAL;
		}
	}

	return 0;
}


static void a6xx_count_throttles(struct adreno_device *adreno_dev,
	uint64_t adj)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	kgsl_gmu_regread(KGSL_DEVICE(adreno_dev),
		adreno_dev->lm_threshold_count,
		&adreno_dev->lm_threshold_cross);
}

static int a6xx_complete_rpmh_votes(struct kgsl_device *device)
{
	int ret = 0;

	if (!kgsl_gmu_isenabled(device))
		return ret;

	ret |= timed_poll_check(device, A6XX_RSCC_TCS0_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS1_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS2_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS3_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));

	return ret;
}

static int a6xx_gmu_suspend(struct kgsl_device *device)
{
	/* Max GX clients on A6xx is 2: GMU and KMD */
	int ret = 0, max_client_num = 2;
	struct gmu_device *gmu = &device->gmu;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* do it only if LM feature is enabled */
	/* Disable ISENSE if it's on */
	a6xx_isense_disable(device);

	/* LLM-GLM handshake sequence */
	a6xx_llm_glm_handshake(device);

	/* If SPTP_RAC is on, turn off SPTP_RAC HS */
	a6xx_sptprac_disable(adreno_dev);

	/* Disconnect GPU from BUS is not needed if CX GDSC goes off later */

	/* Check no outstanding RPMh voting */
	a6xx_complete_rpmh_votes(device);

	if (gmu->gx_gdsc) {
		if (regulator_is_enabled(gmu->gx_gdsc)) {
			/* Switch gx gdsc control from GMU to CPU
			 * force non-zero reference count in clk driver
			 * so next disable call will turn
			 * off the GDSC
			 */
			ret = regulator_enable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx enable\n");

			while ((max_client_num)) {
				ret = regulator_disable(gmu->gx_gdsc);
				if (!regulator_is_enabled(gmu->gx_gdsc))
					break;
				max_client_num -= 1;
			}

			if (!max_client_num)
				dev_err(&gmu->pdev->dev,
					"suspend fail: cannot disable gx\n");
		}
	}

	return ret;
}

/*
 * a6xx_rpmh_gpu_pwrctrl() - GPU power control via RPMh/GMU interface
 * @adreno_dev: Pointer to adreno device
 * @mode: requested power mode
 * @arg1: first argument for mode control
 * @arg2: second argument for mode control
 */
static int a6xx_rpmh_gpu_pwrctrl(struct adreno_device *adreno_dev,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = &device->gmu;
	int ret;

	switch (mode) {
	case GMU_FW_START:
		ret = a6xx_gmu_fw_start(device, arg1);
		break;
	case GMU_SUSPEND:
		ret = a6xx_gmu_suspend(device);
		break;
	case GMU_FW_STOP:
		ret = a6xx_rpmh_power_off_gpu(device);
		break;
	case GMU_DCVS_NOHFI:
		ret = a6xx_gmu_dcvs_nohfi(device, arg1, arg2);
		break;
	case GMU_NOTIFY_SLUMBER:
		ret = a6xx_notify_slumber(device);
		break;
	default:
		dev_err(&gmu->pdev->dev,
				"unsupported GMU power ctrl mode:%d\n", mode);
		ret = -EINVAL;
		break;
	}

	return ret;
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
	if (!kgsl_gmu_isenabled(device))
		return adreno_reset(device, fault);

	/* Transition from ACTIVE to RESET state */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_RESET);

	/* Try soft reset first */
	if (!(fault & ADRENO_IOMMU_PAGE_FAULT)) {
		int acked;

		/* NMI */
		kgsl_gmu_regwrite(device, A6XX_GMU_NMI_CONTROL_STATUS, 0);
		kgsl_gmu_regwrite(device, A6XX_GMU_CM3_CFG, (1 << 9));

		for (i = 0; i < 10; i++) {
			kgsl_gmu_regread(device,
					A6XX_GMU_NMI_CONTROL_STATUS, &acked);

			/* NMI FW ACK recevied */
			if (acked == 0x1)
				break;

			udelay(100);
		}

		if (acked)
			ret = adreno_soft_reset(device);
		if (ret)
			KGSL_DEV_ERR_ONCE(device, "Device soft reset failed\n");
	}
	if (ret) {
		/* If soft reset failed/skipped, then pull the power */
		set_bit(ADRENO_DEVICE_HARD_RESET, &adreno_dev->priv);
		/* since device is officially off now clear start bit */
		clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

		/* Keep trying to start the device until it works */
		for (i = 0; i < NUM_TIMES_RESET_RETRY; i++) {
			ret = adreno_start(device, 0);
			if (!ret)
				break;

			msleep(20);
		}
	}

	clear_bit(ADRENO_DEVICE_HARD_RESET, &adreno_dev->priv);

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
	default:
		KGSL_DRV_CRIT_RATELIMIT(device, "Unknown interrupt %d\n", bit);
	}
}

/* GPU System Cache control registers */
#define A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0   0x4
#define A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1   0x8

static inline void _reg_rmw(void __iomem *regaddr,
	unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	val = __raw_readl(regaddr);
	/* Make sure the above read completes before we proceed  */
	rmb();
	val &= ~mask;
	__raw_writel(val | bits, regaddr);
	/* Make sure the above write posts before we proceed*/
	wmb();
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
	void __iomem *gpu_cx_reg;

	gpu_scid = adreno_llc_get_scid(adreno_dev->gpu_llc_slice);
	for (i = 0; i < A6XX_LLC_NUM_GPU_SCIDS; i++)
		gpu_cntl1_val = (gpu_cntl1_val << A6XX_GPU_LLC_SCID_NUM_BITS)
			| gpu_scid;

	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	_reg_rmw(gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);
	iounmap(gpu_cx_reg);
}

/*
 * a6xx_llc_configure_gpuhtw_scid() - Program the SCID for GPU pagetables
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpuhtw_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpuhtw_scid;
	void __iomem *gpu_cx_reg;

	gpuhtw_scid = adreno_llc_get_scid(adreno_dev->gpuhtw_llc_slice);

	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	_reg_rmw(gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPUHTW_LLC_SCID_MASK,
			gpuhtw_scid << A6XX_GPUHTW_LLC_SCID_SHIFT);
	iounmap(gpu_cx_reg);
}

/*
 * a6xx_llc_enable_overrides() - Override the page attributes
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_enable_overrides(struct adreno_device *adreno_dev)
{
	void __iomem *gpu_cx_reg;

	/*
	 * 0x3: readnoallocoverrideen=0
	 *      read-no-alloc=0 - Allocate lines on read miss
	 *      writenoallocoverrideen=1
	 *      write-no-alloc=1 - Do not allocates lines on write miss
	 */
	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	__raw_writel(0x3, gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0);
	/* Make sure the above write posts before we proceed*/
	wmb();
	iounmap(gpu_cx_reg);
}

static const char *fault_block[8] = {
	[0] = "CP",
	[1] = "UCHE",
	[2] = "VFD",
	[3] = "UCHE",
	[4] = "CCU",
	[5] = "unknown",
	[6] = "CDP Prefetch",
	[7] = "GPMU",
};

static const char *uche_client[8] = {
	[0] = "VFD",
	[1] = "SP",
	[2] = "VSC",
	[3] = "VPC",
	[4] = "HLSQ",
	[5] = "PC",
	[6] = "LRZ",
	[7] = "unknown",
};

static const char *a6xx_iommu_fault_block(struct adreno_device *adreno_dev,
						unsigned int fsynr1)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int client_id;
	unsigned int uche_client_id;

	client_id = fsynr1 & 0xff;

	if (client_id >= ARRAY_SIZE(fault_block))
		return "unknown";
	else if (client_id != 3)
		return fault_block[client_id];

	mutex_lock(&device->mutex);
	kgsl_regread(device, A6XX_UCHE_CLIENT_PF, &uche_client_id);
	mutex_unlock(&device->mutex);

	return uche_client[uche_client_id & A6XX_UCHE_CLIENT_PF_CLIENT_ID_MASK];
}

static void a6xx_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_preemption_execution_enabled(adreno_dev))
		a6xx_preemption_trigger(adreno_dev);

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
	 (1 << A6XX_INT_UCHE_TRAP_INTR))

static struct adreno_irq_funcs a6xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - UNUSED */
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback),
	ADRENO_IRQ_CALLBACK(NULL), /* 7 - GPC_ERR */
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
	ADRENO_IRQ_CALLBACK(NULL), /* 28 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

static struct adreno_irq a6xx_irq = {
	.funcs = a6xx_irq_funcs,
	.mask = A6XX_INT_MASK,
};

static struct adreno_snapshot_sizes a6xx_snap_sizes = {
	.cp_pfp = 0x33,
	.roq = 0x400,
};

static struct adreno_snapshot_data a6xx_snapshot_data = {
	.sect_sizes = &a6xx_snap_sizes,
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

#define A6XX_PERFCOUNTER_GROUP(offset, name) \
	ADRENO_PERFCOUNTER_GROUP(a6xx, offset, name)

#define A6XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a6xx, offset, name, flags)

#define A6XX_POWER_COUNTER_GROUP(offset, name) \
	ADRENO_POWER_COUNTER_GROUP(a6xx, offset, name)

static struct adreno_perfcount_group a6xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A6XX_PERFCOUNTER_GROUP(CP, cp),
	A6XX_PERFCOUNTER_GROUP(RBBM, rbbm),
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
	A6XX_PERFCOUNTER_GROUP(VBIF, vbif),
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

	if (!device->gmu.pdev)
		return -ENODEV;

	kgsl_regwrite(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK, 0xFF000000);
	kgsl_regrmw(device,
			A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, 0xFF, 0x20);
	kgsl_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0x1);

	return 0;
}

static void a6xx_platform_setup(struct adreno_device *adreno_dev)
{
	uint64_t addr;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* Calculate SP local and private mem addresses */
	addr = ALIGN(ADRENO_UCHE_GMEM_BASE + adreno_dev->gmem_size, SZ_64K);
	adreno_dev->sp_local_gpuaddr = addr;
	adreno_dev->sp_pvt_gpuaddr = addr + SZ_64K;

	if (adreno_has_gbif(adreno_dev)) {
		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF].regs =
				a6xx_perfcounters_gbif;
		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF].reg_count
				= ARRAY_SIZE(a6xx_perfcounters_gbif);

		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF_PWR].regs =
				a6xx_perfcounters_gbif_pwr;
		a6xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_VBIF].reg_count
				= ARRAY_SIZE(a6xx_perfcounters_gbif_pwr);

		gpudev->vbif_xin_halt_ctrl0_mask =
				A6XX_GBIF_HALT_MASK;
	} else
		gpudev->vbif_xin_halt_ctrl0_mask =
				A6XX_VBIF_XIN_HALT_CTRL0_MASK;
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

struct adreno_gpudev adreno_a6xx_gpudev = {
	.reg_offsets = &a6xx_reg_offsets,
	.start = a6xx_start,
	.snapshot = a6xx_snapshot,
	.snapshot_gmu = a6xx_snapshot_gmu,
	.irq = &a6xx_irq,
	.snapshot_data = &a6xx_snapshot_data,
	.irq_trace = trace_kgsl_a5xx_irq_status,
	.num_prio_levels = KGSL_PRIORITY_MAX_RB_LEVELS,
	.platform_setup = a6xx_platform_setup,
	.init = a6xx_init,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_sptprac_enable,
	.regulator_disable = a6xx_sptprac_disable,
	.perfcounters = &a6xx_perfcounters,
	.enable_pwr_counters = a6xx_enable_pwr_counters,
	.count_throttles = a6xx_count_throttles,
	.microcode_read = a6xx_microcode_read,
	.enable_64bit = a6xx_enable_64bit,
	.llc_configure_gpu_scid = a6xx_llc_configure_gpu_scid,
	.llc_configure_gpuhtw_scid = a6xx_llc_configure_gpuhtw_scid,
	.llc_enable_overrides = a6xx_llc_enable_overrides,
	.oob_set = a6xx_oob_set,
	.oob_clear = a6xx_oob_clear,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.rpmh_gpu_pwrctrl = a6xx_rpmh_gpu_pwrctrl,
	.hw_isidle = a6xx_hw_isidle, /* Replaced by NULL if GMU is disabled */
	.wait_for_lowest_idle = a6xx_wait_for_lowest_idle,
	.wait_for_gmu_idle = a6xx_wait_for_gmu_idle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_reset,
	.soft_reset = a6xx_soft_reset,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.preemption_context_destroy = a6xx_preemption_context_destroy,
	.gx_is_on = a6xx_gx_is_on,
	.sptprac_is_on = a6xx_sptprac_is_on,
	.ccu_invalidate = a6xx_ccu_invalidate,
};
