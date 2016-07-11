/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _ADRENO_A5XX_H_
#define _ADRENO_A5XX_H_

#include "a5xx_reg.h"

#define A5XX_IRQ_FLAGS \
	{ BIT(A5XX_INT_RBBM_GPU_IDLE), "RBBM_GPU_IDLE" }, \
	{ BIT(A5XX_INT_RBBM_AHB_ERROR), "RBBM_AHB_ERR" }, \
	{ BIT(A5XX_INT_RBBM_TRANSFER_TIMEOUT), "RBBM_TRANSFER_TIMEOUT" }, \
	{ BIT(A5XX_INT_RBBM_ME_MS_TIMEOUT), "RBBM_ME_MS_TIMEOUT" }, \
	{ BIT(A5XX_INT_RBBM_PFP_MS_TIMEOUT), "RBBM_PFP_MS_TIMEOUT" }, \
	{ BIT(A5XX_INT_RBBM_ETS_MS_TIMEOUT), "RBBM_ETS_MS_TIMEOUT" }, \
	{ BIT(A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW), "RBBM_ATB_ASYNC_OVERFLOW" }, \
	{ BIT(A5XX_INT_RBBM_GPC_ERROR), "RBBM_GPC_ERR" }, \
	{ BIT(A5XX_INT_CP_SW), "CP_SW" }, \
	{ BIT(A5XX_INT_CP_HW_ERROR), "CP_OPCODE_ERROR" }, \
	{ BIT(A5XX_INT_CP_CCU_FLUSH_DEPTH_TS), "CP_CCU_FLUSH_DEPTH_TS" }, \
	{ BIT(A5XX_INT_CP_CCU_FLUSH_COLOR_TS), "CP_CCU_FLUSH_COLOR_TS" }, \
	{ BIT(A5XX_INT_CP_CCU_RESOLVE_TS), "CP_CCU_RESOLVE_TS" }, \
	{ BIT(A5XX_INT_CP_IB2), "CP_IB2_INT" }, \
	{ BIT(A5XX_INT_CP_IB1), "CP_IB1_INT" }, \
	{ BIT(A5XX_INT_CP_RB), "CP_RB_INT" }, \
	{ BIT(A5XX_INT_CP_UNUSED_1), "CP_UNUSED_1" }, \
	{ BIT(A5XX_INT_CP_RB_DONE_TS), "CP_RB_DONE_TS" }, \
	{ BIT(A5XX_INT_CP_WT_DONE_TS), "CP_WT_DONE_TS" }, \
	{ BIT(A5XX_INT_UNKNOWN_1), "UNKNOWN_1" }, \
	{ BIT(A5XX_INT_CP_CACHE_FLUSH_TS), "CP_CACHE_FLUSH_TS" }, \
	{ BIT(A5XX_INT_UNUSED_2), "UNUSED_2" }, \
	{ BIT(A5XX_INT_RBBM_ATB_BUS_OVERFLOW), "RBBM_ATB_BUS_OVERFLOW" }, \
	{ BIT(A5XX_INT_MISC_HANG_DETECT), "MISC_HANG_DETECT" }, \
	{ BIT(A5XX_INT_UCHE_OOB_ACCESS), "UCHE_OOB_ACCESS" }, \
	{ BIT(A5XX_INT_UCHE_TRAP_INTR), "UCHE_TRAP_INTR" }, \
	{ BIT(A5XX_INT_DEBBUS_INTR_0), "DEBBUS_INTR_0" }, \
	{ BIT(A5XX_INT_DEBBUS_INTR_1), "DEBBUS_INTR_1" }, \
	{ BIT(A5XX_INT_GPMU_VOLTAGE_DROOP), "GPMU_VOLTAGE_DROOP" }, \
	{ BIT(A5XX_INT_GPMU_FIRMWARE), "GPMU_FIRMWARE" }, \
	{ BIT(A5XX_INT_ISDB_CPU_IRQ), "ISDB_CPU_IRQ" }, \
	{ BIT(A5XX_INT_ISDB_UNDER_DEBUG), "ISDB_UNDER_DEBUG" }

#define A5XX_CP_CTXRECORD_MAGIC_REF     0x27C4BAFCUL
/* Size of each CP preemption record */
#define A5XX_CP_CTXRECORD_SIZE_IN_BYTES     0x10000
/* Size of the preemption counter block (in bytes) */
#define A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE   (16 * 4)

/**
 * struct a5xx_cp_preemption_record - CP context record for
 * preemption.
 * @magic: (00) Value at this offset must be equal to
 * A5XX_CP_CTXRECORD_MAGIC_REF.
 * @info: (04) Type of record. Written non-zero (usually) by CP.
 * we must set to zero for all ringbuffers.
 * @data: (08) DATA field in SET_RENDER_MODE or checkpoint packets.
 * Written by CP when switching out. Not used on switch-in.
 * we must initialize to zero.
 * @cntl: (12) RB_CNTL, saved and restored by CP.
 * @rptr: (16) RB_RPTR, saved and restored by CP.
 * @wptr: (20) RB_WPTR, saved and restored by CP.
 * @rptr_addr: (24) RB_RPTR_ADDR_LO|HI saved and restored.
 * rbase: (32) RB_BASE_LO|HI saved and restored.
 * counter: (40) Pointer to preemption counter
 */
struct a5xx_cp_preemption_record {
	uint32_t  magic;
	uint32_t  info;
	uint32_t  data;
	uint32_t  cntl;
	uint32_t  rptr;
	uint32_t  wptr;
	uint64_t  rptr_addr;
	uint64_t  rbase;
	uint64_t  counter;
};

#define A5XX_CP_SMMU_INFO_MAGIC_REF     0x3618CDA3UL

/**
 * struct a5xx_cp_smmu_info - CP preemption SMMU info.
 * @magic: (00) The value at this offset must be equal to
 * A5XX_CP_SMMU_INFO_MAGIC_REF.
 * @_pad4: (04) Reserved/padding
 * @ttbr0: (08) Base address of the page table for the
 * incoming context.
 * @context_idr: (16) Context Identification Register value.
 */
struct a5xx_cp_smmu_info {
	uint32_t  magic;
	uint32_t  _pad4;
	uint64_t  ttbr0;
	uint32_t  asid;
	uint32_t  context_idr;
};

void a5xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);
unsigned int a5xx_num_registers(void);

void a5xx_crashdump_init(struct adreno_device *adreno_dev);

void a5xx_hwcg_set(struct adreno_device *adreno_dev, bool on);

/* GPMU interrupt multiplexor */
#define FW_INTR_INFO			(0)
#define LLM_ACK_ERR_INTR		(1)
#define ISENS_TRIM_ERR_INTR		(2)
#define ISENS_ERR_INTR			(3)
#define ISENS_IDLE_ERR_INTR		(4)
#define ISENS_PWR_ON_ERR_INTR		(5)
#define WDOG_EXPITED			(31)

#define VALID_GPMU_IRQ (\
	BIT(FW_INTR_INFO) | \
	BIT(LLM_ACK_ERR_INTR) | \
	BIT(ISENS_TRIM_ERR_INTR) | \
	BIT(ISENS_ERR_INTR) | \
	BIT(ISENS_IDLE_ERR_INTR) | \
	BIT(ISENS_PWR_ON_ERR_INTR) | \
	BIT(WDOG_EXPITED))

/* A5XX_GPMU_GPMU_LLM_GLM_SLEEP_CTRL */
#define STATE_OF_CHILD			GENMASK(5, 4)
#define STATE_OF_CHILD_01		BIT(4)
#define STATE_OF_CHILD_11		(BIT(4) | BIT(5))
#define IDLE_FULL_LM_SLEEP		BIT(0)

/* A5XX_GPMU_GPMU_LLM_GLM_SLEEP_STATUS */
#define WAKEUP_ACK			BIT(1)
#define IDLE_FULL_ACK			BIT(0)

/* A5XX_GPMU_TEMP_SENSOR_CONFIG */
#define GPMU_BCL_ENABLED		BIT(4)
#define GPMU_LLM_ENABLED		BIT(9)
#define GPMU_ISENSE_STATUS		GENMASK(3, 0)
#define GPMU_ISENSE_END_POINT_CAL_ERR	BIT(0)

/* A5XX_GPU_CS_AMP_CALIBRATION_CONTROL1 */
#define AMP_SW_TRIM_START		BIT(0)

/* A5XX_GPU_CS_SENSOR_GENERAL_STATUS */
#define SS_AMPTRIM_DONE			BIT(11)
#define CS_PWR_ON_STATUS		BIT(10)

/* A5XX_GPU_CS_AMP_CALIBRATION_STATUS*_* */
#define AMP_OUT_OF_RANGE_ERR		BIT(4)
#define AMP_OFFSET_CHECK_MAX_ERR	BIT(2)
#define AMP_OFFSET_CHECK_MIN_ERR	BIT(1)

/* A5XX_GPU_CS_AMP_CALIBRATION_DONE */
#define SW_OPAMP_CAL_DONE           BIT(0)

#define AMP_CALIBRATION_ERR (AMP_OFFSET_CHECK_MIN_ERR | \
		AMP_OFFSET_CHECK_MAX_ERR | AMP_OUT_OF_RANGE_ERR)

#define AMP_CALIBRATION_RETRY_CNT	3
#define AMP_CALIBRATION_TIMEOUT		6

/* A5XX_GPMU_GPMU_PWR_THRESHOLD */
#define PWR_THRESHOLD_VALID		0x80000000
/* AGC */
#define AGC_INIT_BASE			A5XX_GPMU_DATA_RAM_BASE
#define AGC_INIT_MSG_MAGIC		(AGC_INIT_BASE + 5)
#define AGC_MSG_BASE			(AGC_INIT_BASE + 7)

#define AGC_MSG_STATE			(AGC_MSG_BASE + 0)
#define AGC_MSG_COMMAND			(AGC_MSG_BASE + 1)
#define AGC_MSG_PAYLOAD_SIZE		(AGC_MSG_BASE + 3)
#define AGC_MSG_PAYLOAD			(AGC_MSG_BASE + 5)

#define AGC_INIT_MSG_VALUE		0xBABEFACE
#define AGC_POWER_CONFIG_PRODUCTION_ID	1

#define AGC_LM_CONFIG			(136/4)
#define AGC_LM_CONFIG_ENABLE_GPMU_ADAPTIVE (1)

#define AGC_LM_CONFIG_ENABLE_ERROR	(3 << 4)

#define AGC_THROTTLE_SEL_DCS		(1 << 8)

#define AGC_LLM_ENABLED			(1 << 16)
#define	AGC_GPU_VERSION_MASK		GENMASK(18, 17)
#define AGC_GPU_VERSION_SHIFT		17
#define AGC_BCL_ENABLED			(1 << 24)


#define AGC_LEVEL_CONFIG		(140/4)

#define LM_DCVS_LIMIT			2
/* FW file tages */
#define GPMU_FIRMWARE_ID		2
#define GPMU_SEQUENCE_ID		3
#define GPMU_INST_RAM_SIZE		0xFFF

#define HEADER_MAJOR			1
#define HEADER_MINOR			2
#define HEADER_DATE			3
#define HEADER_TIME			4
#define HEADER_SEQUENCE			5

#define MAX_HEADER_SIZE			10

#define LM_SEQUENCE_ID			1
#define MAX_SEQUENCE_ID			3

/* LM defaults */
#define LM_DEFAULT_LIMIT		6000
#define A530_DEFAULT_LEAKAGE		0x004E001A

static inline bool lm_on(struct adreno_device *adreno_dev)
{
	return ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
		test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);
}
#endif
