// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_genc.h"
#include "adreno_perfcounter.h"
#include "adreno_pm4types.h"
#include "kgsl_device.h"

/*
 * For registers that do not get restored on power cycle, read the value and add
 * the stored shadow value
 */
static u64 genc_counter_read_norestore(struct adreno_device *adreno_dev,
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

static int genc_counter_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	int ret = 0;

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_RESTORE)
		ret = genc_perfcounter_update(adreno_dev, reg, true);
	else
		kgsl_regwrite(device, reg->select, countable);

	if (!ret)
		reg->value = 0;

	return ret;
}

static int genc_counter_inline_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[0];
	u32 cmds[3];
	int ret;

	if (!(device->state == KGSL_STATE_ACTIVE))
		return genc_counter_enable(adreno_dev, group, counter,
			countable);

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_RESTORE)
		genc_perfcounter_update(adreno_dev, reg, false);

	cmds[0] = cp_type7_packet(CP_WAIT_FOR_IDLE, 0);
	cmds[1] = cp_type4_packet(reg->select, 1);
	cmds[2] = countable;

	/* submit to highest priority RB always */
	ret = genc_ringbuffer_addcmds(adreno_dev, rb, NULL,
		F_NOTPROTECTED, cmds, 3, 0, NULL);
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

static u64 genc_counter_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32 hi, lo;

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* These registers are restored on power resume */
	return (((u64) hi) << 32) | lo;
}

static int genc_counter_gbif_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	unsigned int shift = counter << 3;
	unsigned int select = BIT(counter);

	if (countable > 0xff)
		return -EINVAL;

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_CLR, select, select);
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_CLR, select, 0);

	/* select the desired countable */
	kgsl_regrmw(device, reg->select, 0xff << shift, countable << shift);

	/* enable counter */
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_EN, select, select);

	reg->value = 0;
	return 0;
}

static int genc_counter_gbif_pwr_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	unsigned int shift = counter << 3;
	unsigned int select = BIT(16 + counter);

	if (countable > 0xff)
		return -EINVAL;

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_CLR, select, select);
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_CLR, select, 0);

	/* select the desired countable */
	kgsl_regrmw(device, reg->select, 0xff << shift, countable << shift);

	/* Enable the counter */
	kgsl_regrmw(device, GENC_GBIF_PERF_PWR_CNT_EN, select, select);

	reg->value = 0;
	return 0;
}

static int genc_counter_alwayson_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	return 0;
}

static u64 genc_counter_alwayson_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct adreno_perfcount_register *reg = &group->regs[counter];

	return genc_read_alwayson(adreno_dev) + reg->value;
}

static void genc_write_gmu_counter_enable(struct kgsl_device *device,
		struct adreno_perfcount_register *reg, u32 bit, u32 countable)
{
	kgsl_regrmw(device, reg->select, 0xff << bit, countable << bit);
}

static int genc_counter_gmu_xoclk_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	if (countable > 0xff)
		return -EINVAL;

	/*
	 * Counters [0:3] are in select 1 bit offsets 0, 8, 16 and 24
	 * Counters [4:5] are in select 2 bit offset 0, 8
	 * Counters [6:9] are in select 3 bit offset 0, 8, 16 and 24
	 */

	if (counter == 4 || counter == 5)
		counter -= 4;
	else if (counter >= 6)
		counter -= 6;

	genc_write_gmu_counter_enable(device, reg, counter * 8, countable);

	reg->value = 0;

	kgsl_regwrite(device, GENC_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	return 0;
}

static int genc_counter_gmu_gmuclk_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	if (countable > 0xff)
		return -EINVAL;

	/*
	 * The two counters are stuck into GMU_CX_GMU_POWER_COUNTER_SELECT_1
	 * at bit offset 16 and 24
	 */
	genc_write_gmu_counter_enable(device, reg,
		16 + (counter * 8), countable);

	kgsl_regwrite(device, GENC_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

static int genc_counter_gmu_perf_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	if (countable > 0xff)
		return -EINVAL;

	/*
	 * Counters [0:3] are in select 1 bit offsets 0, 8, 16 and 24
	 * Counters [4:5] are in select 2 bit offset 0, 8
	 */

	if (counter >= 4)
		counter -= 4;

	genc_write_gmu_counter_enable(device, reg, counter * 8, countable);

	kgsl_regwrite(device, GENC_GMU_CX_GMU_PERF_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

static struct adreno_perfcount_register genc_perfcounters_cp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_0_LO,
		GENC_RBBM_PERFCTR_CP_0_HI, 0, GENC_CP_PERFCTR_CP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_1_LO,
		GENC_RBBM_PERFCTR_CP_1_HI, 1, GENC_CP_PERFCTR_CP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_2_LO,
		GENC_RBBM_PERFCTR_CP_2_HI, 2, GENC_CP_PERFCTR_CP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_3_LO,
		GENC_RBBM_PERFCTR_CP_3_HI, 3, GENC_CP_PERFCTR_CP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_4_LO,
		GENC_RBBM_PERFCTR_CP_4_HI, 4, GENC_CP_PERFCTR_CP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_5_LO,
		GENC_RBBM_PERFCTR_CP_5_HI, 5, GENC_CP_PERFCTR_CP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_6_LO,
		GENC_RBBM_PERFCTR_CP_6_HI, 6, GENC_CP_PERFCTR_CP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_7_LO,
		GENC_RBBM_PERFCTR_CP_7_HI, 7, GENC_CP_PERFCTR_CP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_8_LO,
		GENC_RBBM_PERFCTR_CP_8_HI, 8, GENC_CP_PERFCTR_CP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_9_LO,
		GENC_RBBM_PERFCTR_CP_9_HI, 9, GENC_CP_PERFCTR_CP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_10_LO,
		GENC_RBBM_PERFCTR_CP_10_HI, 10, GENC_CP_PERFCTR_CP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_11_LO,
		GENC_RBBM_PERFCTR_CP_11_HI, 11, GENC_CP_PERFCTR_CP_SEL_11 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_12_LO,
		GENC_RBBM_PERFCTR_CP_12_HI, 12, GENC_CP_PERFCTR_CP_SEL_12 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CP_13_LO,
		GENC_RBBM_PERFCTR_CP_13_HI, 13, GENC_CP_PERFCTR_CP_SEL_13 },
};

static struct adreno_perfcount_register genc_perfcounters_rbbm[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RBBM_0_LO,
		GENC_RBBM_PERFCTR_RBBM_0_HI, 14, GENC_RBBM_PERFCTR_RBBM_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RBBM_1_LO,
		GENC_RBBM_PERFCTR_RBBM_1_HI, 15, GENC_RBBM_PERFCTR_RBBM_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RBBM_2_LO,
		GENC_RBBM_PERFCTR_RBBM_2_HI, 16, GENC_RBBM_PERFCTR_RBBM_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RBBM_3_LO,
		GENC_RBBM_PERFCTR_RBBM_3_HI, 17, GENC_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_perfcount_register genc_perfcounters_pc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_0_LO,
		GENC_RBBM_PERFCTR_PC_0_HI, 18, GENC_PC_PERFCTR_PC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_1_LO,
		GENC_RBBM_PERFCTR_PC_1_HI, 19, GENC_PC_PERFCTR_PC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_2_LO,
		GENC_RBBM_PERFCTR_PC_2_HI, 20, GENC_PC_PERFCTR_PC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_3_LO,
		GENC_RBBM_PERFCTR_PC_3_HI, 21, GENC_PC_PERFCTR_PC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_4_LO,
		GENC_RBBM_PERFCTR_PC_4_HI, 22, GENC_PC_PERFCTR_PC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_5_LO,
		GENC_RBBM_PERFCTR_PC_5_HI, 23, GENC_PC_PERFCTR_PC_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_6_LO,
		GENC_RBBM_PERFCTR_PC_6_HI, 24, GENC_PC_PERFCTR_PC_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_PC_7_LO,
		GENC_RBBM_PERFCTR_PC_7_HI, 25, GENC_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_perfcount_register genc_perfcounters_vfd[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_0_LO,
		GENC_RBBM_PERFCTR_VFD_0_HI, 26, GENC_VFD_PERFCTR_VFD_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_1_LO,
		GENC_RBBM_PERFCTR_VFD_1_HI, 27, GENC_VFD_PERFCTR_VFD_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_2_LO,
		GENC_RBBM_PERFCTR_VFD_2_HI, 28, GENC_VFD_PERFCTR_VFD_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_3_LO,
		GENC_RBBM_PERFCTR_VFD_3_HI, 29, GENC_VFD_PERFCTR_VFD_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_4_LO,
		GENC_RBBM_PERFCTR_VFD_4_HI, 30, GENC_VFD_PERFCTR_VFD_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_5_LO,
		GENC_RBBM_PERFCTR_VFD_5_HI, 31, GENC_VFD_PERFCTR_VFD_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_6_LO,
		GENC_RBBM_PERFCTR_VFD_6_HI, 32, GENC_VFD_PERFCTR_VFD_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VFD_7_LO,
		GENC_RBBM_PERFCTR_VFD_7_HI, 33, GENC_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_perfcount_register genc_perfcounters_hlsq[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_0_LO,
		GENC_RBBM_PERFCTR_HLSQ_0_HI, 34, GENC_SP_PERFCTR_HLSQ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_1_LO,
		GENC_RBBM_PERFCTR_HLSQ_1_HI, 35, GENC_SP_PERFCTR_HLSQ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_2_LO,
		GENC_RBBM_PERFCTR_HLSQ_2_HI, 36, GENC_SP_PERFCTR_HLSQ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_3_LO,
		GENC_RBBM_PERFCTR_HLSQ_3_HI, 37, GENC_SP_PERFCTR_HLSQ_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_4_LO,
		GENC_RBBM_PERFCTR_HLSQ_4_HI, 38, GENC_SP_PERFCTR_HLSQ_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_HLSQ_5_LO,
		GENC_RBBM_PERFCTR_HLSQ_5_HI, 39, GENC_SP_PERFCTR_HLSQ_SEL_5 },
};

static struct adreno_perfcount_register genc_perfcounters_vpc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_0_LO,
		GENC_RBBM_PERFCTR_VPC_0_HI, 40, GENC_VPC_PERFCTR_VPC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_1_LO,
		GENC_RBBM_PERFCTR_VPC_1_HI, 41, GENC_VPC_PERFCTR_VPC_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_2_LO,
		GENC_RBBM_PERFCTR_VPC_2_HI, 42, GENC_VPC_PERFCTR_VPC_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_3_LO,
		GENC_RBBM_PERFCTR_VPC_3_HI, 43, GENC_VPC_PERFCTR_VPC_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_4_LO,
		GENC_RBBM_PERFCTR_VPC_4_HI, 44, GENC_VPC_PERFCTR_VPC_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VPC_5_LO,
		GENC_RBBM_PERFCTR_VPC_5_HI, 45, GENC_VPC_PERFCTR_VPC_SEL_5 },
};

static struct adreno_perfcount_register genc_perfcounters_ccu[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CCU_0_LO,
		GENC_RBBM_PERFCTR_CCU_0_HI, 46, GENC_RB_PERFCTR_CCU_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CCU_1_LO,
		GENC_RBBM_PERFCTR_CCU_1_HI, 47, GENC_RB_PERFCTR_CCU_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CCU_2_LO,
		GENC_RBBM_PERFCTR_CCU_2_HI, 48, GENC_RB_PERFCTR_CCU_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CCU_3_LO,
		GENC_RBBM_PERFCTR_CCU_3_HI, 49, GENC_RB_PERFCTR_CCU_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CCU_4_LO,
		GENC_RBBM_PERFCTR_CCU_4_HI, 50, GENC_RB_PERFCTR_CCU_SEL_4 },
};

static struct adreno_perfcount_register genc_perfcounters_tse[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TSE_0_LO,
		GENC_RBBM_PERFCTR_TSE_0_HI, 51, GENC_GRAS_PERFCTR_TSE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TSE_1_LO,
		GENC_RBBM_PERFCTR_TSE_1_HI, 52, GENC_GRAS_PERFCTR_TSE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TSE_2_LO,
		GENC_RBBM_PERFCTR_TSE_2_HI, 53, GENC_GRAS_PERFCTR_TSE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TSE_3_LO,
		GENC_RBBM_PERFCTR_TSE_3_HI, 54, GENC_GRAS_PERFCTR_TSE_SEL_3 },
};

static struct adreno_perfcount_register genc_perfcounters_ras[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RAS_0_LO,
		GENC_RBBM_PERFCTR_RAS_0_HI, 55, GENC_GRAS_PERFCTR_RAS_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RAS_1_LO,
		GENC_RBBM_PERFCTR_RAS_1_HI, 56, GENC_GRAS_PERFCTR_RAS_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RAS_2_LO,
		GENC_RBBM_PERFCTR_RAS_2_HI, 57, GENC_GRAS_PERFCTR_RAS_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RAS_3_LO,
		GENC_RBBM_PERFCTR_RAS_3_HI, 58, GENC_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_perfcount_register genc_perfcounters_uche[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_0_LO,
		GENC_RBBM_PERFCTR_UCHE_0_HI, 59, GENC_UCHE_PERFCTR_UCHE_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_1_LO,
		GENC_RBBM_PERFCTR_UCHE_1_HI, 60, GENC_UCHE_PERFCTR_UCHE_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_2_LO,
		GENC_RBBM_PERFCTR_UCHE_2_HI, 61, GENC_UCHE_PERFCTR_UCHE_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_3_LO,
		GENC_RBBM_PERFCTR_UCHE_3_HI, 62, GENC_UCHE_PERFCTR_UCHE_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_4_LO,
		GENC_RBBM_PERFCTR_UCHE_4_HI, 63, GENC_UCHE_PERFCTR_UCHE_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_5_LO,
		GENC_RBBM_PERFCTR_UCHE_5_HI, 64, GENC_UCHE_PERFCTR_UCHE_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_6_LO,
		GENC_RBBM_PERFCTR_UCHE_6_HI, 65, GENC_UCHE_PERFCTR_UCHE_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_7_LO,
		GENC_RBBM_PERFCTR_UCHE_7_HI, 66, GENC_UCHE_PERFCTR_UCHE_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_8_LO,
		GENC_RBBM_PERFCTR_UCHE_8_HI, 67, GENC_UCHE_PERFCTR_UCHE_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_9_LO,
		GENC_RBBM_PERFCTR_UCHE_9_HI, 68, GENC_UCHE_PERFCTR_UCHE_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_10_LO,
		GENC_RBBM_PERFCTR_UCHE_10_HI, 69,
					GENC_UCHE_PERFCTR_UCHE_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_UCHE_11_LO,
		GENC_RBBM_PERFCTR_UCHE_11_HI, 70,
					GENC_UCHE_PERFCTR_UCHE_SEL_11 },
};

static struct adreno_perfcount_register genc_perfcounters_tp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_0_LO,
		GENC_RBBM_PERFCTR_TP_0_HI, 71, GENC_TPL1_PERFCTR_TP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_1_LO,
		GENC_RBBM_PERFCTR_TP_1_HI, 72, GENC_TPL1_PERFCTR_TP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_2_LO,
		GENC_RBBM_PERFCTR_TP_2_HI, 73, GENC_TPL1_PERFCTR_TP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_3_LO,
		GENC_RBBM_PERFCTR_TP_3_HI, 74, GENC_TPL1_PERFCTR_TP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_4_LO,
		GENC_RBBM_PERFCTR_TP_4_HI, 75, GENC_TPL1_PERFCTR_TP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_5_LO,
		GENC_RBBM_PERFCTR_TP_5_HI, 76, GENC_TPL1_PERFCTR_TP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_6_LO,
		GENC_RBBM_PERFCTR_TP_6_HI, 77, GENC_TPL1_PERFCTR_TP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_7_LO,
		GENC_RBBM_PERFCTR_TP_7_HI, 78, GENC_TPL1_PERFCTR_TP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_8_LO,
		GENC_RBBM_PERFCTR_TP_8_HI, 79, GENC_TPL1_PERFCTR_TP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_9_LO,
		GENC_RBBM_PERFCTR_TP_9_HI, 80, GENC_TPL1_PERFCTR_TP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_10_LO,
		GENC_RBBM_PERFCTR_TP_10_HI, 81, GENC_TPL1_PERFCTR_TP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_TP_11_LO,
		GENC_RBBM_PERFCTR_TP_11_HI, 82, GENC_TPL1_PERFCTR_TP_SEL_11 },
};

static struct adreno_perfcount_register genc_perfcounters_sp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_0_LO,
		GENC_RBBM_PERFCTR_SP_0_HI, 83, GENC_SP_PERFCTR_SP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_1_LO,
		GENC_RBBM_PERFCTR_SP_1_HI, 84, GENC_SP_PERFCTR_SP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_2_LO,
		GENC_RBBM_PERFCTR_SP_2_HI, 85, GENC_SP_PERFCTR_SP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_3_LO,
		GENC_RBBM_PERFCTR_SP_3_HI, 86, GENC_SP_PERFCTR_SP_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_4_LO,
		GENC_RBBM_PERFCTR_SP_4_HI, 87, GENC_SP_PERFCTR_SP_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_5_LO,
		GENC_RBBM_PERFCTR_SP_5_HI, 88, GENC_SP_PERFCTR_SP_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_6_LO,
		GENC_RBBM_PERFCTR_SP_6_HI, 89, GENC_SP_PERFCTR_SP_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_7_LO,
		GENC_RBBM_PERFCTR_SP_7_HI, 90, GENC_SP_PERFCTR_SP_SEL_7 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_8_LO,
		GENC_RBBM_PERFCTR_SP_8_HI, 91, GENC_SP_PERFCTR_SP_SEL_8 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_9_LO,
		GENC_RBBM_PERFCTR_SP_9_HI, 92, GENC_SP_PERFCTR_SP_SEL_9 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_10_LO,
		GENC_RBBM_PERFCTR_SP_10_HI, 93, GENC_SP_PERFCTR_SP_SEL_10 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_11_LO,
		GENC_RBBM_PERFCTR_SP_11_HI, 94, GENC_SP_PERFCTR_SP_SEL_11 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_12_LO,
		GENC_RBBM_PERFCTR_SP_12_HI, 95, GENC_SP_PERFCTR_SP_SEL_12 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_13_LO,
		GENC_RBBM_PERFCTR_SP_13_HI, 96, GENC_SP_PERFCTR_SP_SEL_13 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_14_LO,
		GENC_RBBM_PERFCTR_SP_14_HI, 97, GENC_SP_PERFCTR_SP_SEL_14 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_15_LO,
		GENC_RBBM_PERFCTR_SP_15_HI, 98, GENC_SP_PERFCTR_SP_SEL_15 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_16_LO,
		GENC_RBBM_PERFCTR_SP_16_HI, 99, GENC_SP_PERFCTR_SP_SEL_16 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_17_LO,
		GENC_RBBM_PERFCTR_SP_17_HI, 100, GENC_SP_PERFCTR_SP_SEL_17 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_18_LO,
		GENC_RBBM_PERFCTR_SP_18_HI, 101, GENC_SP_PERFCTR_SP_SEL_18 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_19_LO,
		GENC_RBBM_PERFCTR_SP_19_HI, 102, GENC_SP_PERFCTR_SP_SEL_19 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_20_LO,
		GENC_RBBM_PERFCTR_SP_20_HI, 103, GENC_SP_PERFCTR_SP_SEL_20 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_21_LO,
		GENC_RBBM_PERFCTR_SP_21_HI, 104, GENC_SP_PERFCTR_SP_SEL_21 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_22_LO,
		GENC_RBBM_PERFCTR_SP_22_HI, 105, GENC_SP_PERFCTR_SP_SEL_22 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_SP_23_LO,
		GENC_RBBM_PERFCTR_SP_23_HI, 106, GENC_SP_PERFCTR_SP_SEL_23 },
};

static struct adreno_perfcount_register genc_perfcounters_rb[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_0_LO,
		GENC_RBBM_PERFCTR_RB_0_HI, 107, GENC_RB_PERFCTR_RB_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_1_LO,
		GENC_RBBM_PERFCTR_RB_1_HI, 108, GENC_RB_PERFCTR_RB_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_2_LO,
		GENC_RBBM_PERFCTR_RB_2_HI, 109, GENC_RB_PERFCTR_RB_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_3_LO,
		GENC_RBBM_PERFCTR_RB_3_HI, 110, GENC_RB_PERFCTR_RB_SEL_3 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_4_LO,
		GENC_RBBM_PERFCTR_RB_4_HI, 111, GENC_RB_PERFCTR_RB_SEL_4 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_5_LO,
		GENC_RBBM_PERFCTR_RB_5_HI, 112, GENC_RB_PERFCTR_RB_SEL_5 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_6_LO,
		GENC_RBBM_PERFCTR_RB_6_HI, 113, GENC_RB_PERFCTR_RB_SEL_6 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_RB_7_LO,
		GENC_RBBM_PERFCTR_RB_7_HI, 114, GENC_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_perfcount_register genc_perfcounters_vsc[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VSC_0_LO,
		GENC_RBBM_PERFCTR_VSC_0_HI, 115, GENC_VSC_PERFCTR_VSC_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_VSC_1_LO,
		GENC_RBBM_PERFCTR_VSC_1_HI, 116, GENC_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_perfcount_register genc_perfcounters_lrz[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_LRZ_0_LO,
		GENC_RBBM_PERFCTR_LRZ_0_HI, 117, GENC_GRAS_PERFCTR_LRZ_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_LRZ_1_LO,
		GENC_RBBM_PERFCTR_LRZ_1_HI, 118, GENC_GRAS_PERFCTR_LRZ_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_LRZ_2_LO,
		GENC_RBBM_PERFCTR_LRZ_2_HI, 119, GENC_GRAS_PERFCTR_LRZ_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_LRZ_3_LO,
		GENC_RBBM_PERFCTR_LRZ_3_HI, 120, GENC_GRAS_PERFCTR_LRZ_SEL_3 },
};

static struct adreno_perfcount_register genc_perfcounters_cmp[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CMP_0_LO,
		GENC_RBBM_PERFCTR_CMP_0_HI, 121, GENC_RB_PERFCTR_CMP_SEL_0 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CMP_1_LO,
		GENC_RBBM_PERFCTR_CMP_1_HI, 122, GENC_RB_PERFCTR_CMP_SEL_1 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CMP_2_LO,
		GENC_RBBM_PERFCTR_CMP_2_HI, 123, GENC_RB_PERFCTR_CMP_SEL_2 },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_RBBM_PERFCTR_CMP_3_LO,
		GENC_RBBM_PERFCTR_CMP_3_HI, 124, GENC_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_perfcount_register genc_perfcounters_gbif[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PERF_CNT_LOW0,
		GENC_GBIF_PERF_CNT_HIGH0, -1, GENC_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PERF_CNT_LOW1,
		GENC_GBIF_PERF_CNT_HIGH1, -1, GENC_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PERF_CNT_LOW2,
		GENC_GBIF_PERF_CNT_HIGH2, -1, GENC_GBIF_PERF_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PERF_CNT_LOW3,
		GENC_GBIF_PERF_CNT_HIGH3, -1, GENC_GBIF_PERF_CNT_SEL },
};

static struct adreno_perfcount_register genc_perfcounters_gbif_pwr[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PWR_CNT_LOW0,
		GENC_GBIF_PWR_CNT_HIGH0, -1, GENC_GBIF_PERF_PWR_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PWR_CNT_LOW1,
		GENC_GBIF_PWR_CNT_HIGH1, -1, GENC_GBIF_PERF_PWR_CNT_SEL },
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_GBIF_PWR_CNT_LOW2,
		GENC_GBIF_PWR_CNT_HIGH2, -1, GENC_GBIF_PERF_PWR_CNT_SEL },
};

#define GMU_COUNTER(lo, hi, sel) \
	{ .countable = KGSL_PERFCOUNTER_NOT_USED, \
	  .offset = lo, .offset_hi = hi, .select = sel }

static struct adreno_perfcount_register genc_perfcounters_gmu_xoclk[] = {
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_6_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_6_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_7_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_7_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_8_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_8_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_9_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_9_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
};

static struct adreno_perfcount_register genc_perfcounters_gmu_gmuclk[] = {
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_GMUCLK_0_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_GMUCLK_0_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(GENC_GMU_CX_GMU_POWER_COUNTER_GMUCLK_1_L,
		GENC_GMU_CX_GMU_POWER_COUNTER_GMUCLK_1_H,
		GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
};

static struct adreno_perfcount_register genc_perfcounters_gmu_perf[] = {
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_0_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_0_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_1_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_1_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_2_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_2_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_3_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_3_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_4_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_4_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_1),
	GMU_COUNTER(GENC_GMU_CX_GMU_PERF_COUNTER_5_L,
		GENC_GMU_CX_GMU_PERF_COUNTER_5_H,
		GENC_GMU_CX_GMU_PERF_COUNTER_SELECT_1),
};

static struct adreno_perfcount_register genc_perfcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, GENC_CP_ALWAYS_ON_COUNTER_LO,
		GENC_CP_ALWAYS_ON_COUNTER_HI, -1 },
};

/*
 * ADRENO_PERFCOUNTER_GROUP_RESTORE flag is enabled by default
 * because most of the perfcounter groups need to be restored
 * as part of preemption and IFPC. Perfcounter groups that are
 * not restored as part of preemption and IFPC should be defined
 * using GENC_PERFCOUNTER_GROUP_FLAGS macro
 */

#define GENC_PERFCOUNTER_GROUP_FLAGS(core, offset, name, flags, \
		enable, read) \
	[KGSL_PERFCOUNTER_GROUP_##offset] = { core##_perfcounters_##name, \
	ARRAY_SIZE(core##_perfcounters_##name), __stringify(name), flags, \
	enable, read }

#define GENC_PERFCOUNTER_GROUP(offset, name, enable, read) \
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, offset, name, \
	ADRENO_PERFCOUNTER_GROUP_RESTORE, enable, read)

#define GENC_REGULAR_PERFCOUNTER_GROUP(offset, name) \
	GENC_PERFCOUNTER_GROUP(offset, name, \
		genc_counter_enable, genc_counter_read)

static const struct adreno_perfcount_group genc_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	GENC_REGULAR_PERFCOUNTER_GROUP(CP, cp),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, RBBM, rbbm, 0,
		genc_counter_enable, genc_counter_read),
	GENC_REGULAR_PERFCOUNTER_GROUP(PC, pc),
	GENC_REGULAR_PERFCOUNTER_GROUP(VFD, vfd),
	GENC_PERFCOUNTER_GROUP(HLSQ, hlsq,
		genc_counter_inline_enable, genc_counter_read),
	GENC_REGULAR_PERFCOUNTER_GROUP(VPC, vpc),
	GENC_REGULAR_PERFCOUNTER_GROUP(CCU, ccu),
	GENC_REGULAR_PERFCOUNTER_GROUP(CMP, cmp),
	GENC_REGULAR_PERFCOUNTER_GROUP(TSE, tse),
	GENC_REGULAR_PERFCOUNTER_GROUP(RAS, ras),
	GENC_REGULAR_PERFCOUNTER_GROUP(LRZ, lrz),
	GENC_REGULAR_PERFCOUNTER_GROUP(UCHE, uche),
	GENC_PERFCOUNTER_GROUP(TP, tp,
		genc_counter_inline_enable, genc_counter_read),
	GENC_PERFCOUNTER_GROUP(SP, sp,
		genc_counter_inline_enable, genc_counter_read),
	GENC_REGULAR_PERFCOUNTER_GROUP(RB, rb),
	GENC_REGULAR_PERFCOUNTER_GROUP(VSC, vsc),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, VBIF, gbif, 0,
		genc_counter_gbif_enable, genc_counter_read_norestore),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, VBIF_PWR, gbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		genc_counter_gbif_pwr_enable, genc_counter_read_norestore),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		genc_counter_alwayson_enable, genc_counter_alwayson_read),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, GMU_XOCLK, gmu_xoclk, 0,
		genc_counter_gmu_xoclk_enable, genc_counter_read_norestore),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, GMU_GMUCLK, gmu_gmuclk, 0,
		genc_counter_gmu_gmuclk_enable, genc_counter_read_norestore),
	GENC_PERFCOUNTER_GROUP_FLAGS(genc, GMU_PERF, gmu_perf, 0,
		genc_counter_gmu_perf_enable, genc_counter_read_norestore),
};

const struct adreno_perfcounters adreno_genc_perfcounters = {
	genc_perfcounter_groups,
	ARRAY_SIZE(genc_perfcounter_groups),
};
