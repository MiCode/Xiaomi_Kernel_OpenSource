/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#ifndef _ADRENO_A4XX_H_
#define _ADRENO_A4XX_H_

#include "a4xx_reg.h"

#define A4XX_IRQ_FLAGS \
	{ BIT(A4XX_INT_RBBM_GPU_IDLE), "RBBM_GPU_IDLE" }, \
	{ BIT(A4XX_INT_RBBM_REG_TIMEOUT), "RBBM_REG_TIMEOUT" }, \
	{ BIT(A4XX_INT_RBBM_ME_MS_TIMEOUT), "RBBM_ME_MS_TIMEOUT" }, \
	{ BIT(A4XX_INT_RBBM_PFP_MS_TIMEOUT), "RBBM_PFP_MS_TIMEOUT" }, \
	{ BIT(A4XX_INT_RBBM_ETS_MS_TIMEOUT), "RBBM_ETS_MS_TIMEOUT" }, \
	{ BIT(A4XX_INT_RBBM_ASYNC_OVERFLOW), "RBBM_ASYNC_OVERFLOW" }, \
	{ BIT(A4XX_INT_RBBM_GPC_ERR), "RBBM_GPC_ERR" }, \
	{ BIT(A4XX_INT_CP_SW), "CP_SW" }, \
	{ BIT(A4XX_INT_CP_OPCODE_ERROR), "CP_OPCODE_ERROR" }, \
	{ BIT(A4XX_INT_CP_RESERVED_BIT_ERROR), "CP_RESERVED_BIT_ERROR" }, \
	{ BIT(A4XX_INT_CP_HW_FAULT), "CP_HW_FAULT" }, \
	{ BIT(A4XX_INT_CP_DMA), "CP_DMA" }, \
	{ BIT(A4XX_INT_CP_IB2_INT), "CP_IB2_INT" }, \
	{ BIT(A4XX_INT_CP_IB1_INT), "CP_IB1_INT" }, \
	{ BIT(A4XX_INT_CP_RB_INT), "CP_RB_INT" }, \
	{ BIT(A4XX_INT_CP_REG_PROTECT_FAULT), "CP_REG_PROTECT_FAULT" }, \
	{ BIT(A4XX_INT_CP_RB_DONE_TS), "CP_RB_DONE_TS" }, \
	{ BIT(A4XX_INT_CP_VS_DONE_TS), "CP_VS_DONE_TS" }, \
	{ BIT(A4XX_INT_CP_PS_DONE_TS), "CP_PS_DONE_TS" }, \
	{ BIT(A4XX_INT_CACHE_FLUSH_TS), "CACHE_FLUSH_TS" }, \
	{ BIT(A4XX_INT_CP_AHB_ERROR_HALT), "CP_AHB_ERROR_HALT" }, \
	{ BIT(A4XX_INT_RBBM_ATB_BUS_OVERFLOW), "RBBM_ATB_BUS_OVERFLOW" }, \
	{ BIT(A4XX_INT_MISC_HANG_DETECT), "MISC_HANG_DETECT" }, \
	{ BIT(A4XX_INT_UCHE_OOB_ACCESS), "UCHE_OOB_ACCESS" }, \
	{ BIT(A4XX_INT_RBBM_DPM_CALC_ERR), "RBBM_DPM_CALC_ERR" }, \
	{ BIT(A4XX_INT_RBBM_DPM_EPOCH_ERR), "RBBM_DPM_CALC_ERR" }, \
	{ BIT(A4XX_INT_RBBM_DPM_THERMAL_YELLOW_ERR), \
		"RBBM_DPM_THERMAL_YELLOW_ERR" }, \
	{ BIT(A4XX_INT_RBBM_DPM_THERMAL_RED_ERR), "RBBM_DPM_THERMAL_RED_ERR" }

void a4xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

#endif
