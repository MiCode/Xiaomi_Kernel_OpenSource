/* Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-attrs.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/scm.h>

#include "a5xx_reg.h"
#include "adreno_gpu.h"

struct a5xx_gpu {
	struct adreno_gpu base;
	struct platform_device *pdev;

	bool zap_loaded;
};
#define to_a5xx_gpu(x) container_of(x, struct a5xx_gpu, base)

#define A5XX_INT_MASK \
	((1 << A5XX_INT_RBBM_AHB_ERROR) |		\
	 (1 << A5XX_INT_RBBM_TRANSFER_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ME_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_PFP_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ETS_MS_TIMEOUT) |		\
	 (1 << A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW) |		\
	 (1 << A5XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A5XX_INT_CP_HW_ERROR) |	\
	 (1 << A5XX_INT_CP_IB1) |			\
	 (1 << A5XX_INT_CP_IB2) |			\
	 (1 << A5XX_INT_CP_RB) |			\
	 (1 << A5XX_INT_CP_CACHE_FLUSH_TS) |		\
	 (1 << A5XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A5XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A5XX_INT_UCHE_TRAP_INTR) |		\
	 (1 << A5XX_INT_CP_SW) |			\
	 (1 << A5XX_INT_GPMU_FIRMWARE) |                \
	 (1 << A5XX_INT_GPMU_VOLTAGE_DROOP))

static void a5xx_debug_status(struct msm_gpu *gpu)
{
	pr_warn("\tCP S:%x, INT status:%x HW F:%x, PROT S:%x\n",
			gpu_read(gpu, A5XX_RBBM_STATUS),
			gpu_read(gpu, A5XX_RBBM_INT_0_STATUS),
			gpu_read(gpu, A5XX_CP_HW_FAULT),
			gpu_read(gpu, A5XX_CP_PROTECT_STATUS));
	pr_warn("\tCP RPTR:%x, WPTR:%x\n",
			gpu_read(gpu, A5XX_CP_RB_RPTR),
			gpu_read(gpu, A5XX_CP_RB_WPTR));
}

/*
 * struct adreno_vbif_data - Describes vbif register value pair
 * @reg: Offset to vbif register
 * @val: The value that should be programmed in the register at reg
 */
struct adreno_vbif_data {
	unsigned int reg;
	unsigned int val;
};

/*
 * struct adreno_vbif_platform - Holds an array of vbif reg value pairs
 * for a particular core
 * @devfunc: Pointer to platform/core identification function
 * @vbif: Array of reg value pairs for vbif registers
 */
struct adreno_vbif_platform {
	int (*devfunc)(struct adreno_gpu *);
	const struct adreno_vbif_data *vbif;
};

static const struct adreno_vbif_data a530_vbif[] = {
	{A5XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003},
	{0, 0},
};

static const struct adreno_vbif_data a540_vbif[] = {
	{A5XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003},
	{A5XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000009},
	{0, 0},
};

static const struct adreno_vbif_platform a5xx_vbif_platforms[] = {
	{ adreno_is_a540, a540_vbif },
	{ adreno_is_a530, a530_vbif },
	{ adreno_is_a510, a530_vbif },
	{ adreno_is_a505_or_a506, a530_vbif },
};

/*
 * adreno_vbif_start() - Program VBIF registers, called in device start
 * @gpu: Pointer to device whose vbif data is to be programmed
 * @vbif_platforms: list register value pair of vbif for a family
 * of adreno cores
 * @num_platforms: Number of platforms contained in vbif_platforms
 */
static inline void adreno_vbif_start(struct adreno_gpu *adreno,
			const struct adreno_vbif_platform *vbif_platforms,
			int num_platforms)
{
	int i;
	const struct adreno_vbif_data *vbif = NULL;

	for (i = 0; i < num_platforms; i++) {
		if (vbif_platforms[i].devfunc(adreno)) {
			vbif = vbif_platforms[i].vbif;
			break;
		}
	}

	while ((vbif != NULL) && (vbif->reg != 0)) {
		gpu_write(&adreno->base, vbif->reg, vbif->val);
		vbif++;
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

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK)

struct kgsl_hwcg_reg {
	unsigned int off;
	unsigned int val;
};

static const struct kgsl_hwcg_reg a50x_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00FFFFF4},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00505555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};

static const struct kgsl_hwcg_reg a510_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00505555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};

static const struct kgsl_hwcg_reg a530_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP2, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP3, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP2, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP3, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP2, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP3, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP2, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP3, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP2, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP3, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP2, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP3, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP2, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP3, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB2, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB3, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU2, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU3, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00505555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU2, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU3, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_2, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_3, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};


static const struct kgsl_hwcg_reg a540_hwcg_regs[] = {
	{A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP2, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL_SP3, 0x02222222},
	{A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP2, 0x02222220},
	{A5XX_RBBM_CLOCK_CNTL2_SP3, 0x02222220},
	{A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP2, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_HYST_SP3, 0x0000F3CF},
	{A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP2, 0x00000080},
	{A5XX_RBBM_CLOCK_DELAY_SP3, 0x00000080},
	{A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_TP3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP2, 0x00002222},
	{A5XX_RBBM_CLOCK_CNTL3_TP3, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP2, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST2_TP3, 0x77777777},
	{A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP2, 0x00007777},
	{A5XX_RBBM_CLOCK_HYST3_TP3, 0x00007777},
	{A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP2, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY2_TP3, 0x11111111},
	{A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP2, 0x00001111},
	{A5XX_RBBM_CLOCK_DELAY3_TP3, 0x00001111},
	{A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB2, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL_RB3, 0x22222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB2, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL2_RB3, 0x00222222},
	{A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU2, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_CCU3, 0x00022220},
	{A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00505555},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU2, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RB_CCU3, 0x04040404},
	{A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_2, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_3, 0x00000002},
	{A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222},
	{A5XX_RBBM_CLOCK_HYST_GPMU, 0x00000222},
	{A5XX_RBBM_CLOCK_DELAY_GPMU, 0x00000770},
	{A5XX_RBBM_CLOCK_HYST_GPMU, 0x00000004}
};

static const struct {
	int (*devfunc)(struct adreno_gpu *gpu);
	const struct kgsl_hwcg_reg *regs;
	unsigned int count;
} a5xx_hwcg_registers[] = {
	{ adreno_is_a540, a540_hwcg_regs, ARRAY_SIZE(a540_hwcg_regs) },
	{ adreno_is_a530, a530_hwcg_regs, ARRAY_SIZE(a530_hwcg_regs) },
	{ adreno_is_a510, a510_hwcg_regs, ARRAY_SIZE(a510_hwcg_regs) },
};

void _hwcg_set(struct msm_gpu *gpu, bool on)
{
	struct adreno_gpu *adreno_dev = to_adreno_gpu(gpu);
	const struct kgsl_hwcg_reg *regs;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(a5xx_hwcg_registers); i++) {
		if (a5xx_hwcg_registers[i].devfunc(adreno_dev))
			break;
	}

	if (i == ARRAY_SIZE(a5xx_hwcg_registers))
		return;

	regs = a5xx_hwcg_registers[i].regs;

	for (j = 0; j < a5xx_hwcg_registers[i].count; j++)
		gpu_write(gpu, regs[j].off, on ? regs[j].val : 0);

	/* enable top level HWCG */
	gpu_write(gpu, A5XX_RBBM_CLOCK_CNTL, on ? 0xAAA8AA00 : 0);
	gpu_write(gpu, A5XX_RBBM_ISDB_CNT, on ? 0x00000182 : 0x00000180);
}

static void a5xx_me_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_dev = to_adreno_gpu(gpu);
	struct msm_ringbuffer *ring = gpu->rb;

	OUT_PKT7(ring, CP_ME_INIT, 8);
	OUT_RING(ring, CP_INIT_MASK);

	if (CP_INIT_MASK & CP_INIT_MAX_CONTEXT) {
		/*
		 * Multiple HW ctxs are unreliable on a530v1,
		 * use single hw context.
		 * Use multiple contexts if bit set, otherwise serialize:
		 *      3D (bit 0) 2D (bit 1)
		 */
		if (adreno_is_a530v1(adreno_dev))
			OUT_RING(ring, 0x00000000);
		else
			OUT_RING(ring, 0x00000003);
	}

	if (CP_INIT_MASK & CP_INIT_ERROR_DETECTION_CONTROL)
		OUT_RING(ring, 0x20000000);

	if (CP_INIT_MASK & CP_INIT_HEADER_DUMP) {
		/* Header dump address */
		OUT_RING(ring, 0x00000000);
		/* Header dump enable and dump size */
		OUT_RING(ring, 0x00000000);
	}

	if (CP_INIT_MASK & CP_INIT_DRAWCALL_FILTER_RANGE) {
		/* Start range */
		OUT_RING(ring, 0x00000000);
		/* End range (inclusive) */
		OUT_RING(ring, 0x00000000);
	}

	switch (adreno_dev->revn) {
	case ADRENO_REV_A510:
		/* Ucode workaround for token end syncs */
		OUT_RING(ring, 0x00000001);
		break;
	case ADRENO_REV_A505:
	case ADRENO_REV_A506:
	case ADRENO_REV_A530:
		/*
		 * Ucode workarounds for token end syncs,
		 * WFI after every direct-render 3D mode draw and
		 * WFI after every 2D Mode 3 draw.
		 */
		OUT_RING(ring, 0x0000000B);
		break;
	case ADRENO_REV_A540:
		/*
		 * WFI after every direct-render 3D mode draw and
		 * WFI after every 2D Mode 3 draw.
		 */
		OUT_RING(ring, 0x0000000A);
		break;
	default:
		OUT_RING(ring, 0x00000000); /* No ucode workarounds enabled */
	}
	OUT_RING(ring, 0x00000000); /* No ucode workarounds enabled */
	OUT_RING(ring, 0x00000000); /* No ucode workarounds enabled */

	gpu->funcs->flush(gpu);
	gpu->funcs->idle(gpu);

	OUT_PKT7(ring, 0x66, 1);
	OUT_RING(ring, 0x00000000);
	gpu->funcs->flush(gpu);
	gpu->funcs->idle(gpu);
}

static int _poll_gdsc_status(struct msm_gpu *gpu,
				unsigned int status_reg,
				unsigned int status_value)
{
	unsigned int reg, retry = 100;

	/* Bit 20 is the power on bit of SPTP and RAC GDSC status register */
	do {
		udelay(1);
		reg = gpu_read(gpu, status_reg);
	} while (((reg & BIT(20)) != (status_value << 20)) && retry--);
	if ((reg & BIT(20)) != (status_value << 20))
		return -ETIMEDOUT;
	return 0;
}

/*
 * a5xx_regulator_enable() - Enable any necessary HW regulators
 * @adreno_dev: The adreno device pointer
 *
 * Some HW blocks may need their regulators explicitly enabled
 * on a restart.  Clocks must be on during this call.
 */
static int _regulator_enable(struct msm_gpu *gpu)
{
	unsigned int ret;

	/*
	 * Turn on smaller power domain first to reduce voltage droop.
	 * Set the default register values; set SW_COLLAPSE to 0.
	 */
	gpu_write(gpu, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778000);
	/* Insert a delay between RAC and SPTP GDSC to reduce voltage droop */
	udelay(3);
	ret = _poll_gdsc_status(gpu, A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 1);
	if (ret) {
		pr_warn("RBCCU GDSC enable failed\n");
		return ret;
	}

	gpu_write(gpu, A5XX_GPMU_SP_POWER_CNTL, 0x778000);
	ret = _poll_gdsc_status(gpu, A5XX_GPMU_SP_PWR_CLK_STATUS, 1);
	if (ret) {
		pr_warn("SPTP GDSC enable failed\n");
		return ret;
	}

	return 0;
}

void _regulator_disable(struct msm_gpu *gpu)
{
	gpu_write(gpu, A5XX_GPMU_SP_POWER_CNTL, 0x778001);
	/*
	 * Insert a delay between SPTP and RAC GDSC to reduce voltage
	 * droop.
	 */
	udelay(3);
	if (_poll_gdsc_status(gpu,
				A5XX_GPMU_SP_PWR_CLK_STATUS, 0))
		pr_warn("SPTP GDSC disable failed\n");

	gpu_write(gpu, A5XX_GPMU_RBCCU_POWER_CNTL, 0x778001);
	if (_poll_gdsc_status(gpu,
				A5XX_GPMU_RBCCU_PWR_CLK_STATUS, 0))
		pr_warn("RBCCU GDSC disable failed\n");
	/* Reset VBIF before PC to avoid popping bogus FIFO entries */
	gpu_write(gpu, A5XX_RBBM_BLOCK_SW_RESET_CMD,
		0x003C0000);
	gpu_write(gpu, A5XX_RBBM_BLOCK_SW_RESET_CMD, 0);
}

static int a5xx_hw_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx = to_a5xx_gpu(adreno_gpu);
	uint32_t iova;
	int ret;

	gpu_write(gpu, A5XX_RBBM_SW_RESET_CMD, 1);
	gpu_read(gpu, A5XX_RBBM_SW_RESET_CMD);
	gpu_write(gpu, A5XX_RBBM_SW_RESET_CMD, 0);

	_regulator_enable(gpu);

	/* Set system to 64bit mode. */
	gpu_write(gpu, A5XX_CP_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_VSC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_GRAS_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_RB_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_PC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_HLSQ_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_VFD_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_VPC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_UCHE_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_SP_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_TPL1_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, A5XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL, 0x1);

	adreno_vbif_start(adreno_gpu, a5xx_vbif_platforms,
			ARRAY_SIZE(a5xx_vbif_platforms));

	/* Make all blocks contribute to the GPU BUSY perf counter */
	gpu_write(gpu, A5XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/*
	 * Enable the RBBM error reporting bits.  This lets us get
	 * useful information on failure
	 */
	gpu_write(gpu, A5XX_RBBM_AHB_CNTL0, 0x00000001);

	/* TODO: quirk  ADRENO_QUIRK_FAULT_DETECT_MASK */


	/*
	 * TODO:
	 * Turn on hang detection for a530 v2 and beyond. This spews a
	 * lot of useful information into the RBBM registers on a hang.
	 */


	/* Turn on performance counters */
	gpu_write(gpu, A5XX_RBBM_PERFCTR_CNTL, 0x01);

	/*
	 * This is to increase performance by restricting VFD's cache access,
	 * so that LRZ and other data get evicted less.
	 */
	gpu_write(gpu, A5XX_UCHE_CACHE_WAYS, 0x02);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	gpu_write(gpu, A5XX_UCHE_TRAP_BASE_LO, 0xffff0000);
	gpu_write(gpu, A5XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	gpu_write(gpu, A5XX_UCHE_WRITE_THRU_BASE_LO, 0xffff0000);
	gpu_write(gpu, A5XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/* Program the GMEM VA range for the UCHE path */
	gpu_write(gpu, A5XX_UCHE_GMEM_RANGE_MIN_LO,
				ADRENO_UCHE_GMEM_BASE);
	gpu_write(gpu, A5XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	gpu_write(gpu, A5XX_UCHE_GMEM_RANGE_MAX_LO,
				ADRENO_UCHE_GMEM_BASE +
				adreno_gpu->gmem - 1);
	gpu_write(gpu, A5XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	/*
	 * Below CP registers are 0x0 by default, program init
	 * values based on a5xx flavor.
	 */
	if (adreno_is_a505_or_a506(adreno_gpu)) {
		gpu_write(gpu, A5XX_CP_MEQ_THRESHOLDS, 0x20);
		gpu_write(gpu, A5XX_CP_MERCIU_SIZE, 0x400);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_2, 0x40000030);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_1, 0x20100D0A);
	} else if (adreno_is_a510(adreno_gpu)) {
		gpu_write(gpu, A5XX_CP_MEQ_THRESHOLDS, 0x20);
		gpu_write(gpu, A5XX_CP_MERCIU_SIZE, 0x20);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_2, 0x40000030);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_1, 0x20100D0A);
	} else {
		gpu_write(gpu, A5XX_CP_MEQ_THRESHOLDS, 0x40);
		gpu_write(gpu, A5XX_CP_MERCIU_SIZE, 0x40);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_2, 0x80000060);
		gpu_write(gpu, A5XX_CP_ROQ_THRESHOLDS_1, 0x40201B16);
	}

	/*
	 * vtxFifo and primFifo thresholds default values
	 * are different.
	 */
	if (adreno_is_a505_or_a506(adreno_gpu))
		gpu_write(gpu, A5XX_PC_DBG_ECO_CNTL,
						(0x100 << 11 | 0x100 << 22));
	else if (adreno_is_a510(adreno_gpu))
		gpu_write(gpu, A5XX_PC_DBG_ECO_CNTL,
						(0x200 << 11 | 0x200 << 22));
	else
		gpu_write(gpu, A5XX_PC_DBG_ECO_CNTL,
						(0x400 << 11 | 0x300 << 22));

	gpu_write(gpu, A5XX_PC_DBG_ECO_CNTL, 0xc0200100);
	/*
	 * A5x USP LDST non valid pixel wrongly update read combine offset
	 * In A5xx we added optimization for read combine. There could be cases
	 * on a530 v1 there is no valid pixel but the active masks is not
	 * cleared and the offset can be wrongly updated if the invalid address
	 * can be combined. The wrongly latched value will make the returning
	 * data got shifted at wrong offset. workaround this issue by disabling
	 * LD combine, bit[25] of SP_DBG_ECO_CNTL (sp chicken bit[17]) need to
	 * be set to 1, default is 0(enable)
	 */
	if (adreno_is_a530v1(adreno_gpu))
		gpu_write_mask(gpu, A5XX_SP_DBG_ECO_CNTL, 0, (1 << 25));

	/* TODO: Quirk ADRENO_QUIRK_TWO_PASS_USE_WFI */

	/* Set the USE_RETENTION_FLOPS chicken bit */
	gpu_write(gpu, A5XX_CP_CHICKEN_DBG, 0x02000000);

	/* TODO: Enable ISDB mode if requested */
	gpu_write(gpu, A5XX_RBBM_AHB_CNTL1, 0xA6FFFFFF);
	_hwcg_set(gpu, true);

	gpu_write(gpu, A5XX_RBBM_AHB_CNTL2, 0x0000003F);

	/* TODO: for qcom,highest-bank-bit  */
	gpu_write(gpu, A5XX_TPL1_MODE_CNTL, 0x100);
	gpu_write(gpu, A5XX_RB_MODE_CNTL, 4);


	/* TODO: for if preemption is enabled */

	/* TODO: a5xx_protect_init  */

	ret = adreno_hw_init(gpu);
	if (ret)
		return ret;

	ret = msm_gem_get_iova(adreno_gpu->pm4_bo, gpu->id,
			&iova);
	if (ret)
		return ret;

	gpu_write(gpu, A5XX_CP_PM4_INSTR_BASE_LO,
			lower_32_bits(iova));
	gpu_write(gpu, A5XX_CP_PM4_INSTR_BASE_HI,
			upper_32_bits(iova));

	ret = msm_gem_get_iova(adreno_gpu->pfp_bo, gpu->id,
			&iova);
	if (ret)
		return ret;

	gpu_write(gpu, A5XX_CP_PFP_INSTR_BASE_LO,
			lower_32_bits(iova));
	gpu_write(gpu, A5XX_CP_PFP_INSTR_BASE_HI,
			upper_32_bits(iova));

	/*
	 * Resume call to write the zap shader base address into the
	 * appropriate register,
	 * skip if retention is supported for the CPZ register
	 */
	pr_warn("loading %s\n", adreno_gpu->info->zap_name);
	msleep(20);
	if (a5xx->zap_loaded == true) {
		int ret;
		struct scm_desc desc = {0};

		desc.args[0] = 0;
		desc.args[1] = 13;
		desc.arginfo = SCM_ARGS(2);

		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, 0xA), &desc);
		if (ret) {
			pr_err("SCM resume call failed with error %d\n", ret);
			return ret;
		}
	} else {
		/*
		 * Load the zap shader firmware through PIL
		 */
		void *ptr;

		ptr = subsystem_get(adreno_gpu->info->zap_name);
		/* Return error if the zap shader cannot be loaded */
		if (IS_ERR_OR_NULL(ptr))
			return (ptr == NULL) ? -ENODEV : PTR_ERR(ptr);
		a5xx->zap_loaded = true;
	}

	gpu_write(gpu, A5XX_CP_ME_CNTL, 0);

	do {
		gpu_write(gpu, A5XX_RBBM_INT_CLEAR_CMD,
				BIT(A5XX_INT_CP_CACHE_FLUSH_TS));
		ret = gpu_read(gpu, A5XX_RBBM_INT_0_STATUS);
	} while (ret & BIT(A5XX_INT_CP_CACHE_FLUSH_TS));

	gpu_write(gpu, A5XX_RBBM_INT_0_MASK, A5XX_INT_MASK);

	a5xx_me_init(gpu);
	return ret;
}

static void a5xx_recover(struct msm_gpu *gpu)
{
	adreno_dump_info(gpu);
	a5xx_debug_status(gpu);
	adreno_recover(gpu);
}

static void a5xx_destroy(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);

	DBG("%s", gpu->name);

	adreno_gpu_cleanup(adreno_gpu);

	kfree(a5xx_gpu);
}

static int a5xx_idle(struct msm_gpu *gpu)
{
	bool timeout = true;
	unsigned long __t = jiffies + ADRENO_IDLE_TIMEOUT;
	unsigned int rptr, wptr;
	unsigned int status;

	/* wait for ringbuffer to drain: */
	do {
		rptr = gpu_read(gpu, A5XX_CP_RB_RPTR);
		wptr = gpu_read(gpu, A5XX_CP_RB_WPTR);
		if (rptr != wptr)
			continue;

		status = gpu_read(gpu, A5XX_RBBM_STATUS);
		if ((status & 0xfffffffe) == 0) {
			timeout = false;
			break;
		}
	} while (time_before(jiffies, __t));

	if (timeout) {
		DRM_ERROR("%s: timeout waiting to drain ringbuffer!\n",
				gpu->name);
		DRM_ERROR("RPTR:%x WPTR:%x\n", rptr, wptr);
		DRM_ERROR("Status:%x\n", (status & 0xfffffffe));
		return -ETIMEDOUT;
	}
	return 0;
}

static void a5xx_err_checker(struct msm_gpu *gpu, unsigned int bit)
{
	unsigned int reg;

	switch (bit) {
	case A5XX_INT_RBBM_AHB_ERROR: {
		reg = gpu_read(gpu, A5XX_RBBM_AHB_ERROR_STATUS);

		/*
		 * Return the word address of the erroring register so that it
		 * matches the register specification
		 */
		DRM_ERROR(
			"RBBM | AHB bus error | %s | addr=%x | ports=%x:%x\n",
			reg & (1 << 28) ? "WRITE" : "READ",
			(reg & 0xFFFFF) >> 2, (reg >> 20) & 0x3,
			(reg >> 24) & 0xF);

		/* Clear the error */
		gpu_write(gpu, A5XX_RBBM_AHB_CMD, (1 << 4));
		break;
	}
	case A5XX_INT_RBBM_TRANSFER_TIMEOUT:
		DRM_ERROR("RBBM: AHB transfer timeout\n");
		break;
	case A5XX_INT_RBBM_ME_MS_TIMEOUT:
		reg = gpu_read(gpu, A5XX_RBBM_AHB_ME_SPLIT_STATUS);
		DRM_ERROR("RBBM | ME master split timeout | status=%x\n", reg);
		break;
	case A5XX_INT_RBBM_PFP_MS_TIMEOUT:
		reg = gpu_read(gpu, A5XX_RBBM_AHB_PFP_SPLIT_STATUS);
		DRM_ERROR("RBBM | PFP master split timeout | status=%x\n", reg);
		break;
	case A5XX_INT_RBBM_ETS_MS_TIMEOUT:
		DRM_ERROR("RBBM: ME master split timeout\n");
		break;
	case A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW:
		DRM_ERROR("RBBM: ATB ASYNC overflow\n");
		break;
	case A5XX_INT_RBBM_ATB_BUS_OVERFLOW:
		DRM_ERROR("RBBM: ATB bus overflow\n");
		break;
	case A5XX_INT_UCHE_OOB_ACCESS:
		DRM_ERROR("UCHE: Out of bounds access\n");
		DRM_ERROR("UCACHE:0x%x\n",  gpu_read(gpu, 0xeb0));
		break;
	case A5XX_INT_UCHE_TRAP_INTR:
		DRM_ERROR("UCHE: Trap interrupt\n");
		break;
	case A5XX_INT_GPMU_VOLTAGE_DROOP:
		DRM_ERROR("GPMU: Voltage droop\n");
		break;
	case A5XX_INT_CP_CACHE_FLUSH_TS:
		break;
	default:
		DRM_ERROR("Unknown interrupt %d\n", bit);
	}
}

static irqreturn_t a5xx_irq(struct msm_gpu *gpu)
{
	uint32_t status;
	int i;

	status = gpu_read(gpu, A5XX_RBBM_INT_0_STATUS);
	for (i = 0; i < 32; i++) {
		if ((1 << i) & status)
			a5xx_err_checker(gpu, i);
	}

	gpu_write(gpu, A5XX_RBBM_INT_CLEAR_CMD, status);

	msm_gpu_retire(gpu);

	return IRQ_HANDLED;
}

static const unsigned int a5xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0004, 0x0021, 0x0023, 0x0024, 0x0026, 0x0026,
	0x0028, 0x002B, 0x002E, 0x0034, 0x0037, 0x0044, 0x0047, 0x0066,
	0x0068, 0x0095, 0x009C, 0x0170, 0x0174, 0x01AF,
	/* CP */
	0x0200, 0x0233, 0x0240, 0x0250, 0x04C0, 0x04DD, 0x0500, 0x050B,
	0x0578, 0x058F,
	/* VSC */
	0x0C00, 0x0C03, 0x0C08, 0x0C41, 0x0C50, 0x0C51,
	/* GRAS */
	0x0C80, 0x0C81, 0x0C88, 0x0C8F,
	/* RB */
	0x0CC0, 0x0CC0, 0x0CC4, 0x0CD2,
	/* PC */
	0x0D00, 0x0D0C, 0x0D10, 0x0D17, 0x0D20, 0x0D23,
	/* VFD */
	0x0E40, 0x0E4A,
	/* VPC */
	0x0E60, 0x0E61, 0x0E63, 0x0E68,
	/* UCHE */
	0x0E80, 0x0E84, 0x0E88, 0x0E95,

	/* VMIDMT */
	0x1000, 0x1000, 0x1002, 0x1002, 0x1004, 0x1004, 0x1008, 0x100A,
	0x100C, 0x100D, 0x100F, 0x1010, 0x1012, 0x1016, 0x1024, 0x1024,
	0x1027, 0x1027, 0x1100, 0x1100, 0x1102, 0x1102, 0x1104, 0x1104,
	0x1110, 0x1110, 0x1112, 0x1116, 0x1124, 0x1124, 0x1300, 0x1300,
	0x1380, 0x1380,

	/* GRAS CTX 0 */
	0x2000, 0x2004, 0x2008, 0x2067, 0x2070, 0x2078, 0x207B, 0x216E,
	/* PC CTX 0 */
	0x21C0, 0x21C6, 0x21D0, 0x21D0, 0x21D9, 0x21D9, 0x21E5, 0x21E7,
	/* VFD CTX 0 */
	0x2200, 0x2204, 0x2208, 0x22A9,
	/* GRAS CTX 1 */
	0x2400, 0x2404, 0x2408, 0x2467, 0x2470, 0x2478, 0x247B, 0x256E,
	/* PC CTX 1 */
	0x25C0, 0x25C6, 0x25D0, 0x25D0, 0x25D9, 0x25D9, 0x25E5, 0x25E7,
	/* VFD CTX 1 */
	0x2600, 0x2604, 0x2608, 0x26A9,
	/* XPU */
	0x2C00, 0x2C01, 0x2C10, 0x2C10, 0x2C12, 0x2C16, 0x2C1D, 0x2C20,
	0x2C28, 0x2C28, 0x2C30, 0x2C30, 0x2C32, 0x2C36, 0x2C40, 0x2C40,
	0x2C50, 0x2C50, 0x2C52, 0x2C56, 0x2C80, 0x2C80, 0x2C94, 0x2C95,
	/* VBIF */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x301D, 0x3020, 0x3022,
	0x3024, 0x3026, 0x3028, 0x302A, 0x302C, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x3038, 0x3038, 0x303C, 0x303D, 0x3040, 0x3040,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305B, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x330C, 0x330C,
	0x3310, 0x3310, 0x3400, 0x3401, 0x3410, 0x3410, 0x3412, 0x3416,
	0x341D, 0x3420, 0x3428, 0x3428, 0x3430, 0x3430, 0x3432, 0x3436,
	0x3440, 0x3440, 0x3450, 0x3450, 0x3452, 0x3456, 0x3480, 0x3480,
	0x3494, 0x3495, 0x4000, 0x4000, 0x4002, 0x4002, 0x4004, 0x4004,
	0x4008, 0x400A, 0x400C, 0x400D, 0x400F, 0x4012, 0x4014, 0x4016,
	0x401D, 0x401D, 0x4020, 0x4027, 0x4060, 0x4062, 0x4200, 0x4200,
	0x4300, 0x4300, 0x4400, 0x4400, 0x4500, 0x4500, 0x4800, 0x4802,
	0x480F, 0x480F, 0x4811, 0x4811, 0x4813, 0x4813, 0x4815, 0x4816,
	0x482B, 0x482B, 0x4857, 0x4857, 0x4883, 0x4883, 0x48AF, 0x48AF,
	0x48C5, 0x48C5, 0x48E5, 0x48E5, 0x4905, 0x4905, 0x4925, 0x4925,
	0x4945, 0x4945, 0x4950, 0x4950, 0x495B, 0x495B, 0x4980, 0x498E,
	0x4B00, 0x4B00, 0x4C00, 0x4C00, 0x4D00, 0x4D00, 0x4E00, 0x4E00,
	0x4E80, 0x4E80, 0x4F00, 0x4F00, 0x4F08, 0x4F08, 0x4F10, 0x4F10,
	0x4F18, 0x4F18, 0x4F20, 0x4F20, 0x4F30, 0x4F30, 0x4F60, 0x4F60,
	0x4F80, 0x4F81, 0x4F88, 0x4F89, 0x4FEE, 0x4FEE, 0x4FF3, 0x4FF3,
	0x6000, 0x6001, 0x6008, 0x600F, 0x6014, 0x6016, 0x6018, 0x601B,
	0x61FD, 0x61FD, 0x623C, 0x623C, 0x6380, 0x6380, 0x63A0, 0x63A0,
	0x63C0, 0x63C1, 0x63C8, 0x63C9, 0x63D0, 0x63D4, 0x63D6, 0x63D6,
	0x63EE, 0x63EE, 0x6400, 0x6401, 0x6408, 0x640F, 0x6414, 0x6416,
	0x6418, 0x641B, 0x65FD, 0x65FD, 0x663C, 0x663C, 0x6780, 0x6780,
	0x67A0, 0x67A0, 0x67C0, 0x67C1, 0x67C8, 0x67C9, 0x67D0, 0x67D4,
	0x67D6, 0x67D6, 0x67EE, 0x67EE, 0x6800, 0x6801, 0x6808, 0x680F,
	0x6814, 0x6816, 0x6818, 0x681B, 0x69FD, 0x69FD, 0x6A3C, 0x6A3C,
	0x6B80, 0x6B80, 0x6BA0, 0x6BA0, 0x6BC0, 0x6BC1, 0x6BC8, 0x6BC9,
	0x6BD0, 0x6BD4, 0x6BD6, 0x6BD6, 0x6BEE, 0x6BEE,
	~0 /* sentinel */
};

#ifdef CONFIG_DEBUG_FS
static void a5xx_show(struct msm_gpu *gpu, struct seq_file *m)
{
	gpu->funcs->pm_resume(gpu);

	seq_printf(m, "status:   %08x\n",
			gpu_read(gpu, A5XX_RBBM_STATUS));
	seq_printf(m, "s0:   %08d\n", gpu_read(gpu, 0xb78));
	seq_printf(m, "s1:   %08d\n", gpu_read(gpu, 0xb79));
	seq_printf(m, "s2:   %08d\n", gpu_read(gpu, 0xb7a));
	seq_printf(m, "s3:   %08d\n", gpu_read(gpu, 0xb7b));
	seq_printf(m, "s4:   %08d\n", gpu_read(gpu, 0xb7c));
	seq_printf(m, "s5:   %08d\n", gpu_read(gpu, 0xb7d));
	seq_printf(m, "s6:   %08d\n", gpu_read(gpu, 0xb7e));
	seq_printf(m, "s7:   %08d\n", gpu_read(gpu, 0xb7f));

	seq_printf(m, "rptr: %08x, wptr %08x\n",
			gpu_read(gpu, A5XX_CP_RB_RPTR),
			gpu_read(gpu, A5XX_CP_RB_WPTR));
	gpu->funcs->pm_suspend(gpu);
}
#endif

/* Register offset defines for A5XX, in order of enum adreno_regs */
static const unsigned int a5xx_register_offsets[REG_ADRENO_REGISTER_MAX] = {
	REG_ADRENO_DEFINE(REG_ADRENO_CP_WFI_PEND_CTR, A5XX_CP_WFI_PEND_CTR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_BASE, A5XX_CP_RB_BASE),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_BASE_HI, A5XX_CP_RB_BASE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_RPTR, A5XX_CP_RB_RPTR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_WPTR, A5XX_CP_RB_WPTR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_CNTL, A5XX_CP_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_ME_CNTL, A5XX_CP_ME_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_CNTL, A5XX_CP_RB_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB1_BASE, A5XX_CP_IB1_BASE),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB1_BASE_HI, A5XX_CP_IB1_BASE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB1_BUFSZ, A5XX_CP_IB1_BUFSZ),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB2_BASE, A5XX_CP_IB2_BASE),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB2_BASE_HI, A5XX_CP_IB2_BASE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_IB2_BUFSZ, A5XX_CP_IB2_BUFSZ),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_ROQ_ADDR, A5XX_CP_ROQ_DBG_ADDR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_ROQ_DATA, A5XX_CP_ROQ_DBG_DATA),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_MERCIU_ADDR, A5XX_CP_MERCIU_DBG_ADDR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_MERCIU_DATA, A5XX_CP_MERCIU_DBG_DATA_1),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_MERCIU_DATA2,
				A5XX_CP_MERCIU_DBG_DATA_2),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_MEQ_ADDR, A5XX_CP_MEQ_DBG_ADDR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_MEQ_DATA, A5XX_CP_MEQ_DBG_DATA),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_PROTECT_REG_0, A5XX_CP_PROTECT_REG_0),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_PREEMPT, A5XX_CP_CONTEXT_SWITCH_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_PREEMPT_DEBUG, ADRENO_REG_SKIP),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_PREEMPT_DISABLE, ADRENO_REG_SKIP),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
				A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_STATUS, A5XX_RBBM_STATUS),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_STATUS3, A5XX_RBBM_STATUS3),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_CTL, A5XX_RBBM_PERFCTR_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_CMD0,
				A5XX_RBBM_PERFCTR_LOAD_CMD0),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_CMD1,
				A5XX_RBBM_PERFCTR_LOAD_CMD1),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_CMD2,
				A5XX_RBBM_PERFCTR_LOAD_CMD2),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_CMD3,
				A5XX_RBBM_PERFCTR_LOAD_CMD3),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_INT_0_MASK, A5XX_RBBM_INT_0_MASK),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_INT_0_STATUS, A5XX_RBBM_INT_0_STATUS),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_CLOCK_CTL, A5XX_RBBM_CLOCK_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_INT_CLEAR_CMD,
				A5XX_RBBM_INT_CLEAR_CMD),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SW_RESET_CMD, A5XX_RBBM_SW_RESET_CMD),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_BLOCK_SW_RESET_CMD,
				A5XX_RBBM_BLOCK_SW_RESET_CMD),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_BLOCK_SW_RESET_CMD2,
				A5XX_RBBM_BLOCK_SW_RESET_CMD2),
	REG_ADRENO_DEFINE(REG_ADRENO_UCHE_INVALIDATE0, A5XX_UCHE_INVALIDATE0),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_VALUE_LO,
				A5XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_PERFCTR_LOAD_VALUE_HI,
				A5XX_RBBM_PERFCTR_LOAD_VALUE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TRUST_CONTROL,
				A5XX_RBBM_SECVID_TRUST_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TRUST_CONFIG,
				A5XX_RBBM_SECVID_TRUST_CONFIG),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TSB_CONTROL,
				A5XX_RBBM_SECVID_TSB_CNTL),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_BASE,
				A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
				A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_SECVID_TSB_TRUSTED_SIZE,
				A5XX_RBBM_SECVID_TSB_TRUSTED_SIZE),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_ALWAYSON_COUNTER_LO,
				A5XX_RBBM_ALWAYSON_COUNTER_LO),
	REG_ADRENO_DEFINE(REG_ADRENO_RBBM_ALWAYSON_COUNTER_HI,
				A5XX_RBBM_ALWAYSON_COUNTER_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_VBIF_XIN_HALT_CTRL0,
				A5XX_VBIF_XIN_HALT_CTRL0),
	REG_ADRENO_DEFINE(REG_ADRENO_VBIF_XIN_HALT_CTRL1,
				A5XX_VBIF_XIN_HALT_CTRL1),
	REG_ADRENO_DEFINE(REG_ADRENO_VBIF_VERSION,
				A5XX_VBIF_VERSION),
};

static const struct adreno_gpu_funcs funcs = {
	.base = {
		.get_param = adreno_get_param,
		.hw_init = a5xx_hw_init,
		.pm_suspend = msm_gpu_pm_suspend,
		.pm_resume = msm_gpu_pm_resume,
		.recover = a5xx_recover,
		.last_fence = adreno_last_fence,
		.submit = adreno_submit,
		.flush = adreno_flush,
		.idle = a5xx_idle,
		.irq = a5xx_irq,
		.destroy = a5xx_destroy,
#ifdef CONFIG_DEBUG_FS
		.show = a5xx_show,
#endif
	},
};

struct msm_gpu *a5xx_gpu_init(struct drm_device *dev)
{
	struct a5xx_gpu *a5xx_gpu = NULL;
	struct adreno_gpu *adreno_gpu;
	struct msm_gpu *gpu;
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	int ret;
	const struct firmware *fw;
	size_t size;
	void *vaddr;

	if (!pdev) {
		dev_err(dev->dev, "no a5xx device\n");
		ret = -ENXIO;
		goto fail;
	}

	a5xx_gpu = kzalloc(sizeof(*a5xx_gpu), GFP_KERNEL);

	adreno_gpu = &a5xx_gpu->base;
	gpu = &adreno_gpu->base;

	a5xx_gpu->pdev = pdev;

	gpu->perfcntrs = NULL;
	gpu->num_perfcntrs = 0;

	adreno_gpu->registers = a5xx_registers;
	adreno_gpu->reg_offsets = a5xx_register_offsets;

	ret = adreno_gpu_init(dev, pdev, adreno_gpu, &funcs);
	if (ret)
		goto fail;

	if (!gpu->mmu) {
		/* TODO we think it is possible to configure the GPU to
		 * restrict access to VRAM carveout.  But the required
		 * registers are unknown.  For now just bail out and
		 * limp along with just modesetting.  If it turns out
		 * to not be possible to restrict access, then we must
		 * implement a cmdstream validator.
		 */
		dev_err(dev->dev, "No memory protection without IOMMU\n");
		ret = -ENXIO;
		goto fail;
	}

	fw = adreno_gpu->pm4;
	size = fw->size - 4;
	if (size == 0 || size > SIZE_MAX)
		goto fail;

	mutex_lock(&gpu->dev->struct_mutex);
	adreno_gpu->pm4_bo = msm_gem_new(gpu->dev, size,
			MSM_BO_UNCACHED);
	mutex_unlock(&gpu->dev->struct_mutex);

	if (IS_ERR(adreno_gpu->pm4_bo)) {
		ret = PTR_ERR(adreno_gpu->pm4_bo);
		adreno_gpu->pm4_bo = NULL;
		DRM_ERROR("%s: create pm4 bo failed\n", __func__);
		goto fail;
	}

	vaddr = msm_gem_vaddr(adreno_gpu->pm4_bo);
	if (!vaddr) {
		DRM_ERROR("could not vmap pm4\n");
		goto fail;
	}
	adreno_gpu->pm4_vaddr = vaddr;

	memcpy(vaddr, &fw->data[4], size);

	fw = adreno_gpu->pfp;
	size = fw->size - 4;
	if (size == 0 || size > SIZE_MAX)
		goto fail;

	mutex_lock(&gpu->dev->struct_mutex);
	adreno_gpu->pfp_bo = msm_gem_new(gpu->dev, size,
			MSM_BO_UNCACHED);
	mutex_unlock(&gpu->dev->struct_mutex);

	if (IS_ERR(adreno_gpu->pfp_bo)) {
		ret = PTR_ERR(adreno_gpu->pfp_bo);
		adreno_gpu->pfp_bo = NULL;
		DRM_ERROR("%s: create pfp bo failed\n", __func__);
		goto fail;
	}

	vaddr = msm_gem_vaddr(adreno_gpu->pfp_bo);
	if (!vaddr) {
		DRM_ERROR("could not vmap pfp\n");
		goto fail;
	}
	adreno_gpu->pfp_vaddr = vaddr;

	memcpy(vaddr, &fw->data[4], size);

	return gpu;

fail:
	return ERR_PTR(ret);
}
