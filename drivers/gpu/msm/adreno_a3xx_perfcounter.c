// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_perfcounter.h"
#include "kgsl_device.h"

/* Bit flag for RBMM_PERFCTR_CTL */
#define RBBM_PERFCTR_CTL_ENABLE		0x00000001
#define VBIF2_PERF_CNT_SEL_MASK 0x7F
/* offset of clear register from select register */
#define VBIF2_PERF_CLR_REG_SEL_OFF 8
/* offset of enable register from select register */
#define VBIF2_PERF_EN_REG_SEL_OFF 16
/* offset of clear register from the enable register */
#define VBIF2_PERF_PWR_CLR_REG_EN_OFF 8

static int a3xx_counter_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	kgsl_regwrite(device, reg->select, countable);
	reg->value = 0;

	return 0;
}

static u64 a3xx_counter_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 val, hi, lo;

	kgsl_regread(device, A3XX_RBBM_PERFCTR_CTL, &val);
	kgsl_regwrite(device, A3XX_RBBM_PERFCTR_CTL,
		val & ~RBBM_PERFCTR_CTL_ENABLE);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	kgsl_regwrite(device, A3XX_RBBM_PERFCTR_CTL, val);

	return (((u64) hi) << 32) | lo;
}

static int a3xx_counter_pwr_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	return 0;
}

static u64 a3xx_counter_pwr_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 val, hi, lo;

	kgsl_regread(device, A3XX_RBBM_RBBM_CTL, &val);

	/* Freeze the counter so we can read it */
	if (!counter)
		kgsl_regwrite(device, A3XX_RBBM_RBBM_CTL, val & ~0x10000);
	else
		kgsl_regwrite(device, A3XX_RBBM_RBBM_CTL, val & ~0x20000);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	kgsl_regwrite(device, A3XX_RBBM_RBBM_CTL, val);

	return ((((u64) hi) << 32) | lo) + reg->value;
}

static int a3xx_counter_vbif_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	if (countable > VBIF2_PERF_CNT_SEL_MASK)
		return -EINVAL;

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regwrite(device,
		reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 1);
	kgsl_regwrite(device,
		reg->select - VBIF2_PERF_CLR_REG_SEL_OFF, 0);
	kgsl_regwrite(device,
		reg->select, countable & VBIF2_PERF_CNT_SEL_MASK);
	/* enable reg is 8 DWORDS before select reg */
	kgsl_regwrite(device,
		reg->select - VBIF2_PERF_EN_REG_SEL_OFF, 1);

	kgsl_regwrite(device, reg->select, countable);

	reg->value = 0;
	return 0;
}

static u64 a3xx_counter_vbif_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 hi, lo;

	/* freeze counter */
	kgsl_regwrite(device, reg->select - VBIF2_PERF_EN_REG_SEL_OFF, 0);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* un-freeze counter */
	kgsl_regwrite(device, reg->select - VBIF2_PERF_EN_REG_SEL_OFF, 1);

	return ((((u64) hi) << 32) | lo) + reg->value;
}

static int a3xx_counter_vbif_pwr_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regwrite(device, reg->select +
		VBIF2_PERF_PWR_CLR_REG_EN_OFF, 1);
	kgsl_regwrite(device, reg->select +
		VBIF2_PERF_PWR_CLR_REG_EN_OFF, 0);
	kgsl_regwrite(device, reg->select, 1);

	reg->value = 0;
	return 0;
}

static u64 a3xx_counter_vbif_pwr_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 hi, lo;

	/* freeze counter */
	kgsl_regwrite(device, reg->select, 0);

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* un-freeze counter */
	kgsl_regwrite(device, reg->select, 1);

	return ((((u64) hi) << 32) | lo) + reg->value;
}

/*
 * Define the available perfcounter groups - these get used by
 * adreno_perfcounter_get and adreno_perfcounter_put
 */

static struct adreno_perfcount_register a3xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_CP_0_LO,
		A3XX_RBBM_PERFCTR_CP_0_HI, 0, A3XX_CP_PERFCOUNTER_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RBBM_0_LO,
		A3XX_RBBM_PERFCTR_RBBM_0_HI, 1, A3XX_RBBM_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RBBM_1_LO,
		A3XX_RBBM_PERFCTR_RBBM_1_HI, 2, A3XX_RBBM_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_0_LO,
		A3XX_RBBM_PERFCTR_PC_0_HI, 3, A3XX_PC_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_1_LO,
		A3XX_RBBM_PERFCTR_PC_1_HI, 4, A3XX_PC_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_2_LO,
		A3XX_RBBM_PERFCTR_PC_2_HI, 5, A3XX_PC_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PC_3_LO,
		A3XX_RBBM_PERFCTR_PC_3_HI, 6, A3XX_PC_PERFCOUNTER3_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VFD_0_LO,
		A3XX_RBBM_PERFCTR_VFD_0_HI, 7, A3XX_VFD_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VFD_1_LO,
		A3XX_RBBM_PERFCTR_VFD_1_HI, 8, A3XX_VFD_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_0_LO,
		A3XX_RBBM_PERFCTR_HLSQ_0_HI, 9,
		A3XX_HLSQ_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_1_LO,
		A3XX_RBBM_PERFCTR_HLSQ_1_HI, 10,
		A3XX_HLSQ_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_2_LO,
		A3XX_RBBM_PERFCTR_HLSQ_2_HI, 11,
		A3XX_HLSQ_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_3_LO,
		A3XX_RBBM_PERFCTR_HLSQ_3_HI, 12,
		A3XX_HLSQ_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_4_LO,
		A3XX_RBBM_PERFCTR_HLSQ_4_HI, 13,
		A3XX_HLSQ_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_HLSQ_5_LO,
		A3XX_RBBM_PERFCTR_HLSQ_5_HI, 14,
		A3XX_HLSQ_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VPC_0_LO,
		A3XX_RBBM_PERFCTR_VPC_0_HI, 15, A3XX_VPC_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_VPC_1_LO,
		A3XX_RBBM_PERFCTR_VPC_1_HI, 16, A3XX_VPC_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TSE_0_LO,
		A3XX_RBBM_PERFCTR_TSE_0_HI, 17, A3XX_GRAS_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TSE_1_LO,
		A3XX_RBBM_PERFCTR_TSE_1_HI, 18, A3XX_GRAS_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RAS_0_LO,
		A3XX_RBBM_PERFCTR_RAS_0_HI, 19, A3XX_GRAS_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RAS_1_LO,
		A3XX_RBBM_PERFCTR_RAS_1_HI, 20, A3XX_GRAS_PERFCOUNTER3_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_0_LO,
		A3XX_RBBM_PERFCTR_UCHE_0_HI, 21,
		A3XX_UCHE_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_1_LO,
		A3XX_RBBM_PERFCTR_UCHE_1_HI, 22,
		A3XX_UCHE_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_2_LO,
		A3XX_RBBM_PERFCTR_UCHE_2_HI, 23,
		A3XX_UCHE_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_3_LO,
		A3XX_RBBM_PERFCTR_UCHE_3_HI, 24,
		A3XX_UCHE_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_4_LO,
		A3XX_RBBM_PERFCTR_UCHE_4_HI, 25,
		A3XX_UCHE_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_UCHE_5_LO,
		A3XX_RBBM_PERFCTR_UCHE_5_HI, 26,
		A3XX_UCHE_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_0_LO,
		A3XX_RBBM_PERFCTR_TP_0_HI, 27, A3XX_TP_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_1_LO,
		A3XX_RBBM_PERFCTR_TP_1_HI, 28, A3XX_TP_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_2_LO,
		A3XX_RBBM_PERFCTR_TP_2_HI, 29, A3XX_TP_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_3_LO,
		A3XX_RBBM_PERFCTR_TP_3_HI, 30, A3XX_TP_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_4_LO,
		A3XX_RBBM_PERFCTR_TP_4_HI, 31, A3XX_TP_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_TP_5_LO,
		A3XX_RBBM_PERFCTR_TP_5_HI, 32, A3XX_TP_PERFCOUNTER5_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_0_LO,
		A3XX_RBBM_PERFCTR_SP_0_HI, 33, A3XX_SP_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_1_LO,
		A3XX_RBBM_PERFCTR_SP_1_HI, 34, A3XX_SP_PERFCOUNTER1_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_2_LO,
		A3XX_RBBM_PERFCTR_SP_2_HI, 35, A3XX_SP_PERFCOUNTER2_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_3_LO,
		A3XX_RBBM_PERFCTR_SP_3_HI, 36, A3XX_SP_PERFCOUNTER3_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_4_LO,
		A3XX_RBBM_PERFCTR_SP_4_HI, 37, A3XX_SP_PERFCOUNTER4_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_5_LO,
		A3XX_RBBM_PERFCTR_SP_5_HI, 38, A3XX_SP_PERFCOUNTER5_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_6_LO,
		A3XX_RBBM_PERFCTR_SP_6_HI, 39, A3XX_SP_PERFCOUNTER6_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_SP_7_LO,
		A3XX_RBBM_PERFCTR_SP_7_HI, 40, A3XX_SP_PERFCOUNTER7_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RB_0_LO,
		A3XX_RBBM_PERFCTR_RB_0_HI, 41, A3XX_RB_PERFCOUNTER0_SELECT },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_RB_1_LO,
		A3XX_RBBM_PERFCTR_RB_1_HI, 42, A3XX_RB_PERFCOUNTER1_SELECT },
};

static struct adreno_perfcount_register a3xx_perfcounters_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_RBBM_PERFCTR_PWR_0_LO,
		A3XX_RBBM_PERFCTR_PWR_0_HI, -1, 0 },
	/*
	 * A3XX_RBBM_PERFCTR_PWR_1_LO is used for frequency scaling and removed
	 * from the pool of available counters
	 */
};

static struct adreno_perfcount_register a3xx_perfcounters_vbif2[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW0,
		A3XX_VBIF2_PERF_CNT_HIGH0, -1, A3XX_VBIF2_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW1,
		A3XX_VBIF2_PERF_CNT_HIGH1, -1, A3XX_VBIF2_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW2,
		A3XX_VBIF2_PERF_CNT_HIGH2, -1, A3XX_VBIF2_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A3XX_VBIF2_PERF_CNT_LOW3,
		A3XX_VBIF2_PERF_CNT_HIGH3, -1, A3XX_VBIF2_PERF_CNT_SEL3 },
};
/*
 * Placing EN register in select field since vbif perf counters
 * don't have select register to program
 */
static struct adreno_perfcount_register a3xx_perfcounters_vbif2_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW0,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH0, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW1,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH1, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0,
		0, A3XX_VBIF2_PERF_PWR_CNT_LOW2,
		A3XX_VBIF2_PERF_PWR_CNT_HIGH2, -1,
		A3XX_VBIF2_PERF_PWR_CNT_EN2 },
};

#define A3XX_PERFCOUNTER_GROUP(offset, name, enable, read) \
	ADRENO_PERFCOUNTER_GROUP(a3xx, offset, name, enable, read)

#define A3XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags, enable, read) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a3xx, offset, name, flags, enable, read)

static const struct adreno_perfcount_group
a3xx_perfcounter_groups[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A3XX_PERFCOUNTER_GROUP(CP, cp,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(RBBM, rbbm,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(PC, pc,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(VFD, vfd,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(HLSQ, hlsq,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(VPC, vpc,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(TSE, tse,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(RAS, ras,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(UCHE, uche,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(TP, tp,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(SP, sp,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP(RB, rb,
		a3xx_counter_enable, a3xx_counter_read),
	A3XX_PERFCOUNTER_GROUP_FLAGS(PWR, pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a3xx_counter_pwr_enable, a3xx_counter_pwr_read),
	A3XX_PERFCOUNTER_GROUP(VBIF, vbif2,
		a3xx_counter_vbif_enable, a3xx_counter_vbif_read),
	A3XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif2_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a3xx_counter_vbif_pwr_enable, a3xx_counter_vbif_pwr_read),

};

const struct adreno_perfcounters adreno_a3xx_perfcounters = {
	a3xx_perfcounter_groups,
	ARRAY_SIZE(a3xx_perfcounter_groups),
};
