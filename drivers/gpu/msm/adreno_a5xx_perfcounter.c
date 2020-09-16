// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a5xx.h"
#include "adreno_perfcounter.h"
#include "adreno_pm4types.h"
#include "kgsl_device.h"

#define VBIF2_PERF_CNT_SEL_MASK 0x7F
/* offset of clear register from select register */
#define VBIF2_PERF_CLR_REG_SEL_OFF 8
/* offset of enable register from select register */
#define VBIF2_PERF_EN_REG_SEL_OFF 16
/* offset of clear register from the enable register */
#define VBIF2_PERF_PWR_CLR_REG_EN_OFF 8

static u64 a5xx_counter_read_norestore(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 hi, lo;

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	return ((((u64) hi) << 32) | lo) + reg->value;
}

static int a5xx_counter_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	kgsl_regwrite(device, reg->select, countable);
	reg->value = 0;

	return 0;
}

static int a5xx_counter_inline_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[0];
	u32 cmds[3];
	int ret;

	if (!(device->state == KGSL_STATE_ACTIVE))
		return a5xx_counter_enable(adreno_dev, group, counter,
			countable);

	cmds[0]  = cp_type7_packet(CP_WAIT_FOR_IDLE, 0);
	cmds[1] = cp_type4_packet(reg->select, 1);
	cmds[2] = countable;

	/* submit to highest priority RB always */
	ret = adreno_ringbuffer_issue_internal_cmds(rb,
			KGSL_CMD_FLAGS_PMODE, cmds, 3);
	if (ret)
		return ret;

	/*
	 * schedule dispatcher to make sure rb[0] is run, because
	 * if the current RB is not rb[0] and gpu is idle then
	 * rb[0] will not get scheduled to run
	 */
	if (adreno_dev->cur_rb != rb)
		adreno_dispatcher_schedule(device);

	/* wait for the above commands submitted to complete */
	ret = adreno_ringbuffer_waittimestamp(rb, rb->timestamp,
		ADRENO_IDLE_TIMEOUT);

	if (ret) {
		/*
		 * If we were woken up because of cancelling rb events
		 * either due to soft reset or adreno_stop, ignore the
		 * error and return 0 here. The perfcounter is already
		 * set up in software and it will be programmed in
		 * hardware when we wake up or come up after soft reset,
		 * by adreno_perfcounter_restore.
		 */
		if (ret == -EAGAIN)
			ret = 0;
		else
			dev_err(device->dev,
				     "Perfcounter %s/%u/%u start via commands failed %d\n",
				     group->name, counter, countable, ret);
	}

	if (!ret)
		reg->value = 0;

	return ret;
}

static int a5xx_counter_rbbm_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	if (adreno_is_a540(adreno_dev) && countable == A5XX_RBBM_ALWAYS_COUNT)
		return -EINVAL;

	return a5xx_counter_inline_enable(adreno_dev, group, counter, countable);
}

static u64 a5xx_counter_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32  hi, lo;

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	return (((u64) hi) << 32) | lo;
}

static int a5xx_counter_vbif_enable(struct adreno_device *adreno_dev,
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

static int a5xx_counter_vbif_pwr_enable(struct adreno_device *adreno_dev,
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

static int a5xx_counter_alwayson_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	return 0;
}

static u64 a5xx_counter_alwayson_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct adreno_perfcount_register *reg = &group->regs[counter];

	return a5xx_read_alwayson(adreno_dev) + reg->value;
}

static int a5xx_counter_pwr_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	kgsl_regwrite(device, reg->select, countable);
	kgsl_regwrite(device, A5XX_GPMU_POWER_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

static int a5xx_counter_pwr_gpmu_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	unsigned int shift = (counter << 3) % (sizeof(unsigned int) * 8);

	if (adreno_is_a530(adreno_dev)) {
		if (countable > 43)
			return -EINVAL;
	} else if (adreno_is_a540(adreno_dev)) {
		if (countable > 47)
			return -EINVAL;
	}

	kgsl_regrmw(device, reg->select, 0xff << shift, countable << shift);
	kgsl_regwrite(device, A5XX_GPMU_POWER_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

static int a5xx_counter_pwr_alwayson_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	kgsl_regwrite(device, A5XX_GPMU_ALWAYS_ON_COUNTER_RESET, 1);

	reg->value = 0;
	return 0;
}

static struct adreno_perfcount_register a5xx_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_0_LO,
		A5XX_RBBM_PERFCTR_CP_0_HI, 0, A5XX_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_1_LO,
		A5XX_RBBM_PERFCTR_CP_1_HI, 1, A5XX_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_2_LO,
		A5XX_RBBM_PERFCTR_CP_2_HI, 2, A5XX_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_3_LO,
		A5XX_RBBM_PERFCTR_CP_3_HI, 3, A5XX_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_4_LO,
		A5XX_RBBM_PERFCTR_CP_4_HI, 4, A5XX_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_5_LO,
		A5XX_RBBM_PERFCTR_CP_5_HI, 5, A5XX_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_6_LO,
		A5XX_RBBM_PERFCTR_CP_6_HI, 6, A5XX_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CP_7_LO,
		A5XX_RBBM_PERFCTR_CP_7_HI, 7, A5XX_CP_PERFCTR_CP_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_rbbm[] = {
	/*
	 * A5XX_RBBM_PERFCTR_RBBM_0 is used for frequency scaling and omitted
	 * from the poool of available counters
	 */
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_1_LO,
		A5XX_RBBM_PERFCTR_RBBM_1_HI, 9, A5XX_RBBM_PERFCTR_RBBM_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_2_LO,
		A5XX_RBBM_PERFCTR_RBBM_2_HI, 10, A5XX_RBBM_PERFCTR_RBBM_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RBBM_3_LO,
		A5XX_RBBM_PERFCTR_RBBM_3_HI, 11, A5XX_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_0_LO,
		A5XX_RBBM_PERFCTR_PC_0_HI, 12, A5XX_PC_PERFCTR_PC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_1_LO,
		A5XX_RBBM_PERFCTR_PC_1_HI, 13, A5XX_PC_PERFCTR_PC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_2_LO,
		A5XX_RBBM_PERFCTR_PC_2_HI, 14, A5XX_PC_PERFCTR_PC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_3_LO,
		A5XX_RBBM_PERFCTR_PC_3_HI, 15, A5XX_PC_PERFCTR_PC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_4_LO,
		A5XX_RBBM_PERFCTR_PC_4_HI, 16, A5XX_PC_PERFCTR_PC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_5_LO,
		A5XX_RBBM_PERFCTR_PC_5_HI, 17, A5XX_PC_PERFCTR_PC_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_6_LO,
		A5XX_RBBM_PERFCTR_PC_6_HI, 18, A5XX_PC_PERFCTR_PC_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_PC_7_LO,
		A5XX_RBBM_PERFCTR_PC_7_HI, 19, A5XX_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_0_LO,
		A5XX_RBBM_PERFCTR_VFD_0_HI, 20, A5XX_VFD_PERFCTR_VFD_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_1_LO,
		A5XX_RBBM_PERFCTR_VFD_1_HI, 21, A5XX_VFD_PERFCTR_VFD_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_2_LO,
		A5XX_RBBM_PERFCTR_VFD_2_HI, 22, A5XX_VFD_PERFCTR_VFD_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_3_LO,
		A5XX_RBBM_PERFCTR_VFD_3_HI, 23, A5XX_VFD_PERFCTR_VFD_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_4_LO,
		A5XX_RBBM_PERFCTR_VFD_4_HI, 24, A5XX_VFD_PERFCTR_VFD_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_5_LO,
		A5XX_RBBM_PERFCTR_VFD_5_HI, 25, A5XX_VFD_PERFCTR_VFD_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_6_LO,
		A5XX_RBBM_PERFCTR_VFD_6_HI, 26, A5XX_VFD_PERFCTR_VFD_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VFD_7_LO,
		A5XX_RBBM_PERFCTR_VFD_7_HI, 27, A5XX_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_0_LO,
		A5XX_RBBM_PERFCTR_HLSQ_0_HI, 28, A5XX_HLSQ_PERFCTR_HLSQ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_1_LO,
		A5XX_RBBM_PERFCTR_HLSQ_1_HI, 29, A5XX_HLSQ_PERFCTR_HLSQ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_2_LO,
		A5XX_RBBM_PERFCTR_HLSQ_2_HI, 30, A5XX_HLSQ_PERFCTR_HLSQ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_3_LO,
		A5XX_RBBM_PERFCTR_HLSQ_3_HI, 31, A5XX_HLSQ_PERFCTR_HLSQ_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_4_LO,
		A5XX_RBBM_PERFCTR_HLSQ_4_HI, 32, A5XX_HLSQ_PERFCTR_HLSQ_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_5_LO,
		A5XX_RBBM_PERFCTR_HLSQ_5_HI, 33, A5XX_HLSQ_PERFCTR_HLSQ_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_6_LO,
		A5XX_RBBM_PERFCTR_HLSQ_6_HI, 34, A5XX_HLSQ_PERFCTR_HLSQ_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_HLSQ_7_LO,
		A5XX_RBBM_PERFCTR_HLSQ_7_HI, 35, A5XX_HLSQ_PERFCTR_HLSQ_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_0_LO,
		A5XX_RBBM_PERFCTR_VPC_0_HI, 36, A5XX_VPC_PERFCTR_VPC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_1_LO,
		A5XX_RBBM_PERFCTR_VPC_1_HI, 37, A5XX_VPC_PERFCTR_VPC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_2_LO,
		A5XX_RBBM_PERFCTR_VPC_2_HI, 38, A5XX_VPC_PERFCTR_VPC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VPC_3_LO,
		A5XX_RBBM_PERFCTR_VPC_3_HI, 39, A5XX_VPC_PERFCTR_VPC_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_0_LO,
		A5XX_RBBM_PERFCTR_CCU_0_HI, 40, A5XX_RB_PERFCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_1_LO,
		A5XX_RBBM_PERFCTR_CCU_1_HI, 41, A5XX_RB_PERFCTR_CCU_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_2_LO,
		A5XX_RBBM_PERFCTR_CCU_2_HI, 42, A5XX_RB_PERFCTR_CCU_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CCU_3_LO,
		A5XX_RBBM_PERFCTR_CCU_3_HI, 43, A5XX_RB_PERFCTR_CCU_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_0_LO,
		A5XX_RBBM_PERFCTR_TSE_0_HI, 44, A5XX_GRAS_PERFCTR_TSE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_1_LO,
		A5XX_RBBM_PERFCTR_TSE_1_HI, 45, A5XX_GRAS_PERFCTR_TSE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_2_LO,
		A5XX_RBBM_PERFCTR_TSE_2_HI, 46, A5XX_GRAS_PERFCTR_TSE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TSE_3_LO,
		A5XX_RBBM_PERFCTR_TSE_3_HI, 47, A5XX_GRAS_PERFCTR_TSE_SEL_3 },
};


static struct adreno_perfcount_register a5xx_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_0_LO,
		A5XX_RBBM_PERFCTR_RAS_0_HI, 48, A5XX_GRAS_PERFCTR_RAS_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_1_LO,
		A5XX_RBBM_PERFCTR_RAS_1_HI, 49, A5XX_GRAS_PERFCTR_RAS_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_2_LO,
		A5XX_RBBM_PERFCTR_RAS_2_HI, 50, A5XX_GRAS_PERFCTR_RAS_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RAS_3_LO,
		A5XX_RBBM_PERFCTR_RAS_3_HI, 51, A5XX_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_0_LO,
		A5XX_RBBM_PERFCTR_UCHE_0_HI, 52, A5XX_UCHE_PERFCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_1_LO,
		A5XX_RBBM_PERFCTR_UCHE_1_HI, 53, A5XX_UCHE_PERFCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_2_LO,
		A5XX_RBBM_PERFCTR_UCHE_2_HI, 54, A5XX_UCHE_PERFCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_3_LO,
		A5XX_RBBM_PERFCTR_UCHE_3_HI, 55, A5XX_UCHE_PERFCTR_UCHE_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_4_LO,
		A5XX_RBBM_PERFCTR_UCHE_4_HI, 56, A5XX_UCHE_PERFCTR_UCHE_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_5_LO,
		A5XX_RBBM_PERFCTR_UCHE_5_HI, 57, A5XX_UCHE_PERFCTR_UCHE_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_6_LO,
		A5XX_RBBM_PERFCTR_UCHE_6_HI, 58, A5XX_UCHE_PERFCTR_UCHE_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_UCHE_7_LO,
		A5XX_RBBM_PERFCTR_UCHE_7_HI, 59, A5XX_UCHE_PERFCTR_UCHE_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_0_LO,
		A5XX_RBBM_PERFCTR_TP_0_HI, 60, A5XX_TPL1_PERFCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_1_LO,
		A5XX_RBBM_PERFCTR_TP_1_HI, 61, A5XX_TPL1_PERFCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_2_LO,
		A5XX_RBBM_PERFCTR_TP_2_HI, 62, A5XX_TPL1_PERFCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_3_LO,
		A5XX_RBBM_PERFCTR_TP_3_HI, 63, A5XX_TPL1_PERFCTR_TP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_4_LO,
		A5XX_RBBM_PERFCTR_TP_4_HI, 64, A5XX_TPL1_PERFCTR_TP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_5_LO,
		A5XX_RBBM_PERFCTR_TP_5_HI, 65, A5XX_TPL1_PERFCTR_TP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_6_LO,
		A5XX_RBBM_PERFCTR_TP_6_HI, 66, A5XX_TPL1_PERFCTR_TP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_TP_7_LO,
		A5XX_RBBM_PERFCTR_TP_7_HI, 67, A5XX_TPL1_PERFCTR_TP_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_0_LO,
		A5XX_RBBM_PERFCTR_SP_0_HI, 68, A5XX_SP_PERFCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_1_LO,
		A5XX_RBBM_PERFCTR_SP_1_HI, 69, A5XX_SP_PERFCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_2_LO,
		A5XX_RBBM_PERFCTR_SP_2_HI, 70, A5XX_SP_PERFCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_3_LO,
		A5XX_RBBM_PERFCTR_SP_3_HI, 71, A5XX_SP_PERFCTR_SP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_4_LO,
		A5XX_RBBM_PERFCTR_SP_4_HI, 72, A5XX_SP_PERFCTR_SP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_5_LO,
		A5XX_RBBM_PERFCTR_SP_5_HI, 73, A5XX_SP_PERFCTR_SP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_6_LO,
		A5XX_RBBM_PERFCTR_SP_6_HI, 74, A5XX_SP_PERFCTR_SP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_7_LO,
		A5XX_RBBM_PERFCTR_SP_7_HI, 75, A5XX_SP_PERFCTR_SP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_8_LO,
		A5XX_RBBM_PERFCTR_SP_8_HI, 76, A5XX_SP_PERFCTR_SP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_9_LO,
		A5XX_RBBM_PERFCTR_SP_9_HI, 77, A5XX_SP_PERFCTR_SP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_10_LO,
		A5XX_RBBM_PERFCTR_SP_10_HI, 78, A5XX_SP_PERFCTR_SP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_SP_11_LO,
		A5XX_RBBM_PERFCTR_SP_11_HI, 79, A5XX_SP_PERFCTR_SP_SEL_11 },
};

static struct adreno_perfcount_register a5xx_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_0_LO,
		A5XX_RBBM_PERFCTR_RB_0_HI, 80, A5XX_RB_PERFCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_1_LO,
		A5XX_RBBM_PERFCTR_RB_1_HI, 81, A5XX_RB_PERFCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_2_LO,
		A5XX_RBBM_PERFCTR_RB_2_HI, 82, A5XX_RB_PERFCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_3_LO,
		A5XX_RBBM_PERFCTR_RB_3_HI, 83, A5XX_RB_PERFCTR_RB_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_4_LO,
		A5XX_RBBM_PERFCTR_RB_4_HI, 84, A5XX_RB_PERFCTR_RB_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_5_LO,
		A5XX_RBBM_PERFCTR_RB_5_HI, 85, A5XX_RB_PERFCTR_RB_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_6_LO,
		A5XX_RBBM_PERFCTR_RB_6_HI, 86, A5XX_RB_PERFCTR_RB_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_RB_7_LO,
		A5XX_RBBM_PERFCTR_RB_7_HI, 87, A5XX_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_0_LO,
		A5XX_RBBM_PERFCTR_VSC_0_HI, 88, A5XX_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_VSC_1_LO,
		A5XX_RBBM_PERFCTR_VSC_1_HI, 89, A5XX_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_perfcount_register a5xx_perfcounters_lrz[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_0_LO,
		A5XX_RBBM_PERFCTR_LRZ_0_HI, 90, A5XX_GRAS_PERFCTR_LRZ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_1_LO,
		A5XX_RBBM_PERFCTR_LRZ_1_HI, 91, A5XX_GRAS_PERFCTR_LRZ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_2_LO,
		A5XX_RBBM_PERFCTR_LRZ_2_HI, 92, A5XX_GRAS_PERFCTR_LRZ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_LRZ_3_LO,
		A5XX_RBBM_PERFCTR_LRZ_3_HI, 93, A5XX_GRAS_PERFCTR_LRZ_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_cmp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_0_LO,
		A5XX_RBBM_PERFCTR_CMP_0_HI, 94, A5XX_RB_PERFCTR_CMP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_1_LO,
		A5XX_RBBM_PERFCTR_CMP_1_HI, 95, A5XX_RB_PERFCTR_CMP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_2_LO,
		A5XX_RBBM_PERFCTR_CMP_2_HI, 96, A5XX_RB_PERFCTR_CMP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_PERFCTR_CMP_3_LO,
		A5XX_RBBM_PERFCTR_CMP_3_HI, 97, A5XX_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW0,
		A5XX_VBIF_PERF_CNT_HIGH0, -1, A5XX_VBIF_PERF_CNT_SEL0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW1,
		A5XX_VBIF_PERF_CNT_HIGH1, -1, A5XX_VBIF_PERF_CNT_SEL1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW2,
		A5XX_VBIF_PERF_CNT_HIGH2, -1, A5XX_VBIF_PERF_CNT_SEL2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_CNT_LOW3,
		A5XX_VBIF_PERF_CNT_HIGH3, -1, A5XX_VBIF_PERF_CNT_SEL3 },
};

static struct adreno_perfcount_register a5xx_perfcounters_vbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW0,
		A5XX_VBIF_PERF_PWR_CNT_HIGH0, -1, A5XX_VBIF_PERF_PWR_CNT_EN0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW1,
		A5XX_VBIF_PERF_PWR_CNT_HIGH1, -1, A5XX_VBIF_PERF_PWR_CNT_EN1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_VBIF_PERF_PWR_CNT_LOW2,
		A5XX_VBIF_PERF_PWR_CNT_HIGH2, -1, A5XX_VBIF_PERF_PWR_CNT_EN2 },
};

static struct adreno_perfcount_register a5xx_perfcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RBBM_ALWAYSON_COUNTER_LO,
		A5XX_RBBM_ALWAYSON_COUNTER_HI, -1 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_0_LO,
		A5XX_SP_POWER_COUNTER_0_HI, -1, A5XX_SP_POWERCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_1_LO,
		A5XX_SP_POWER_COUNTER_1_HI, -1, A5XX_SP_POWERCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_2_LO,
		A5XX_SP_POWER_COUNTER_2_HI, -1, A5XX_SP_POWERCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_SP_POWER_COUNTER_3_LO,
		A5XX_SP_POWER_COUNTER_3_HI, -1, A5XX_SP_POWERCTR_SP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_0_LO,
		A5XX_TP_POWER_COUNTER_0_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_1_LO,
		A5XX_TP_POWER_COUNTER_1_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_2_LO,
		A5XX_TP_POWER_COUNTER_2_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_TP_POWER_COUNTER_3_LO,
		A5XX_TP_POWER_COUNTER_3_HI, -1, A5XX_TPL1_POWERCTR_TP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_0_LO,
		A5XX_RB_POWER_COUNTER_0_HI, -1, A5XX_RB_POWERCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_1_LO,
		A5XX_RB_POWER_COUNTER_1_HI, -1, A5XX_RB_POWERCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_2_LO,
		A5XX_RB_POWER_COUNTER_2_HI, -1, A5XX_RB_POWERCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_RB_POWER_COUNTER_3_LO,
		A5XX_RB_POWER_COUNTER_3_HI, -1, A5XX_RB_POWERCTR_RB_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CCU_POWER_COUNTER_0_LO,
		A5XX_CCU_POWER_COUNTER_0_HI, -1, A5XX_RB_POWERCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CCU_POWER_COUNTER_1_LO,
		A5XX_CCU_POWER_COUNTER_1_HI, -1, A5XX_RB_POWERCTR_CCU_SEL_1 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_0_LO,
		A5XX_UCHE_POWER_COUNTER_0_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_1_LO,
		A5XX_UCHE_POWER_COUNTER_1_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_2_LO,
		A5XX_UCHE_POWER_COUNTER_2_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_UCHE_POWER_COUNTER_3_LO,
		A5XX_UCHE_POWER_COUNTER_3_HI, -1,
		A5XX_UCHE_POWERCTR_UCHE_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_0_LO,
		A5XX_CP_POWER_COUNTER_0_HI, -1, A5XX_CP_POWERCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_1_LO,
		A5XX_CP_POWER_COUNTER_1_HI, -1, A5XX_CP_POWERCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_2_LO,
		A5XX_CP_POWER_COUNTER_2_HI, -1, A5XX_CP_POWERCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_CP_POWER_COUNTER_3_LO,
		A5XX_CP_POWER_COUNTER_3_HI, -1, A5XX_CP_POWERCTR_CP_SEL_3 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_gpmu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_0_LO,
		A5XX_GPMU_POWER_COUNTER_0_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_1_LO,
		A5XX_GPMU_POWER_COUNTER_1_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_2_LO,
		A5XX_GPMU_POWER_COUNTER_2_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_3_LO,
		A5XX_GPMU_POWER_COUNTER_3_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_4_LO,
		A5XX_GPMU_POWER_COUNTER_4_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_POWER_COUNTER_5_LO,
		A5XX_GPMU_POWER_COUNTER_5_HI, -1,
		A5XX_GPMU_POWER_COUNTER_SELECT_1 },
};

static struct adreno_perfcount_register a5xx_pwrcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A5XX_GPMU_ALWAYS_ON_COUNTER_LO,
		A5XX_GPMU_ALWAYS_ON_COUNTER_HI, -1 },
};

#define A5XX_PERFCOUNTER_GROUP(offset, name, enable, read) \
	ADRENO_PERFCOUNTER_GROUP(a5xx, offset, name, enable, read)

#define A5XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags, enable, read) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a5xx, offset, name, flags, enable, read)


#define A5XX_POWER_COUNTER_GROUP(offset, name, enable, read) \
	[KGSL_PERFCOUNTER_GROUP_##offset##_PWR] = { a5xx_pwrcounters_##name, \
	ARRAY_SIZE(a5xx_pwrcounters_##name), __stringify(name##_pwr), 0, \
	enable, read }

static struct adreno_perfcount_group a5xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A5XX_PERFCOUNTER_GROUP(CP, cp,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(RBBM, rbbm,
		a5xx_counter_rbbm_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(PC, pc,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(VFD, vfd,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(HLSQ, hlsq,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(VPC, vpc,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(CCU, ccu,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(CMP, cmp,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(TSE, tse,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(RAS, ras,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(LRZ, lrz,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(UCHE, uche,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(TP, tp,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(SP, sp,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(RB, rb,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(VSC, vsc,
		a5xx_counter_inline_enable, a5xx_counter_read),
	A5XX_PERFCOUNTER_GROUP(VBIF, vbif,
		a5xx_counter_vbif_enable, a5xx_counter_read_norestore),
	A5XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a5xx_counter_vbif_pwr_enable, a5xx_counter_read_norestore),
	A5XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a5xx_counter_alwayson_enable, a5xx_counter_alwayson_read),
	A5XX_POWER_COUNTER_GROUP(SP, sp,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(TP, tp,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(RB, rb,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(CCU, ccu,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(UCHE, uche,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(CP, cp,
		a5xx_counter_pwr_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(GPMU, gpmu,
		a5xx_counter_pwr_gpmu_enable, a5xx_counter_read_norestore),
	A5XX_POWER_COUNTER_GROUP(ALWAYSON, alwayson,
		a5xx_counter_pwr_alwayson_enable, a5xx_counter_read_norestore),
};

const struct adreno_perfcounters adreno_a5xx_perfcounters = {
	a5xx_perfcounter_groups,
	ARRAY_SIZE(a5xx_perfcounter_groups),
};
