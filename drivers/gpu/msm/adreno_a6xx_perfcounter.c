// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a6xx.h"
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

/* offset of clear register from select register for GBIF */
#define GBIF_PERF_CLR_REG_SEL_OFF 1
/* offset of enable register from select register for GBIF*/
#define GBIF_PERF_EN_REG_SEL_OFF  2
/* offset of clear register from the power enable register for GBIF*/
#define GBIF_PWR_CLR_REG_EN_OFF    1
#define GBIF_PWR_SEL_REG_EN_OFF  3

#define GBIF_PERF_SEL_RMW_MASK   0xFF
#define GBIF_PWR_SEL_RMW_MASK    0xFF
#define GBIF_PWR_EN_CLR_RMW_MASK 0x10000

static void a6xx_counter_load(struct adreno_device *adreno_dev,
		struct adreno_perfcount_register *reg)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int index = reg->load_bit / 32;
	u32 enable = BIT(reg->load_bit & 31);

	/*
	 * a650 and a660 currently have the perfcounter values saved via
	 * retention in the GMU.
	 */
	if (adreno_is_a650(adreno_dev) || adreno_is_a660(adreno_dev))
		return;

	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_LOAD_VALUE_LO,
		lower_32_bits(reg->value));

	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_LOAD_VALUE_HI,
		upper_32_bits(reg->value));

	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_LOAD_CMD0 + index, enable);
}

/*
 * For registers that do not get restored on power cycle, read the value and add
 * the stored shadow value
 */
static u64 a6xx_counter_read_norestore(struct adreno_device *adreno_dev,
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

static int a6xx_counter_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	int ret = 0;

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_RESTORE)
		ret = a6xx_perfcounter_update(adreno_dev, reg, true);
	else
		kgsl_regwrite(device, reg->select, countable);

	if (!ret)
		reg->value = 0;

	return ret;
}

static int a6xx_counter_inline_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[0];
	u32 cmds[3];
	int ret;


	if (!(device->state == KGSL_STATE_ACTIVE))
		return a6xx_counter_enable(adreno_dev, group, counter,
			countable);

	if (group->flags & ADRENO_PERFCOUNTER_GROUP_RESTORE)
		a6xx_perfcounter_update(adreno_dev, reg, false);

	cmds[0]  = cp_type7_packet(CP_WAIT_FOR_IDLE, 0);
	cmds[1] = cp_type4_packet(reg->select, 1);
	cmds[2] = countable;

	/* submit to highest priority RB always */
	ret = a6xx_ringbuffer_addcmds(adreno_dev, rb, NULL,
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

static u64 a6xx_counter_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	u32  hi, lo;

	kgsl_regread(device, reg->offset, &lo);
	kgsl_regread(device, reg->offset_hi, &hi);

	/* These registers are restored on power resume */
	return (((u64) hi) << 32) | lo;
}

static int a6xx_counter_gbif_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	unsigned int shift = counter << 3;
	unsigned int perfctr_mask = 1 << counter;

	if (countable > VBIF2_PERF_CNT_SEL_MASK)
		return -EINVAL;

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regrmw(device, reg->select - GBIF_PERF_CLR_REG_SEL_OFF,
		perfctr_mask, perfctr_mask);
	kgsl_regrmw(device, reg->select - GBIF_PERF_CLR_REG_SEL_OFF,
		perfctr_mask, 0);
		/* select the desired countable */
	kgsl_regrmw(device, reg->select,
		GBIF_PERF_SEL_RMW_MASK << shift, countable << shift);
	/* enable counter */
	kgsl_regrmw(device, reg->select - GBIF_PERF_EN_REG_SEL_OFF,
		perfctr_mask, perfctr_mask);

	reg->value = 0;
	return 0;
}

static int a630_counter_vbif_enable(struct adreno_device *adreno_dev,
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

static int a630_counter_vbif_pwr_enable(struct adreno_device *adreno_dev,
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

static int a6xx_counter_gbif_pwr_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];
	unsigned int shift = counter << 3;
	unsigned int perfctr_mask = GBIF_PWR_EN_CLR_RMW_MASK << counter;

	/*
	 * Write 1, followed by 0 to CLR register for
	 * clearing the counter
	 */
	kgsl_regrmw(device, reg->select + GBIF_PWR_CLR_REG_EN_OFF,
		perfctr_mask, perfctr_mask);
	kgsl_regrmw(device, reg->select + GBIF_PWR_CLR_REG_EN_OFF,
		perfctr_mask, 0);
	/* select the desired countable */
	kgsl_regrmw(device, reg->select + GBIF_PWR_SEL_REG_EN_OFF,
		GBIF_PWR_SEL_RMW_MASK << shift, countable << shift);
	/* Enable the counter */
	kgsl_regrmw(device, reg->select, perfctr_mask, perfctr_mask);

	reg->value = 0;
	return 0;
}

static int a6xx_counter_alwayson_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	return 0;
}

static u64 a6xx_counter_alwayson_read(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter)
{
	struct adreno_perfcount_register *reg = &group->regs[counter];

	return a6xx_read_alwayson(adreno_dev) + reg->value;
}

static void a6xx_write_gmu_counter_enable(struct kgsl_device *device,
		struct adreno_perfcount_register *reg, u32 bit, u32 countable)
{
	u32 val;

	kgsl_regread(device, reg->select, &val);
	val &= ~(0xff << bit);
	val |= countable << bit;
	kgsl_regwrite(device, reg->select, val);

}

static int a6xx_counter_gmu_xoclk_enable(struct adreno_device *adreno_dev,
		const struct adreno_perfcount_group *group,
		unsigned int counter, unsigned int countable)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_perfcount_register *reg = &group->regs[counter];

	if (countable > 0xff)
		return -EINVAL;

	if (counter >= 6 && !adreno_is_a660(adreno_dev))
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

	a6xx_write_gmu_counter_enable(device, reg, counter * 8, countable);

	reg->value = 0;

	kgsl_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	return 0;
}

static int a6xx_counter_gmu_gmuclk_enable(struct adreno_device *adreno_dev,
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
	a6xx_write_gmu_counter_enable(device, reg,
		16 + (counter * 8), countable);

	kgsl_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

static int a6xx_counter_gmu_perf_enable(struct adreno_device *adreno_dev,
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

	a6xx_write_gmu_counter_enable(device, reg, counter * 8, countable);

	kgsl_regwrite(device, A6XX_GMU_CX_GMU_PERF_COUNTER_ENABLE, 1);

	reg->value = 0;
	return 0;
}

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
		A6XX_RBBM_PERFCTR_RBBM_0_HI, 14, A6XX_RBBM_PERFCTR_RBBM_SEL_0 },
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

#define GMU_COUNTER(lo, hi, sel) \
	{ .countable = KGSL_PERFCOUNTER_NOT_USED, \
	  .offset = lo, .offset_hi = hi, .select = sel }

#define GMU_COUNTER_RESERVED(lo, hi, sel) \
	{ .countable = KGSL_PERFCOUNTER_BROKEN, \
	  .offset = lo, .offset_hi = hi, .select = sel }

static struct adreno_perfcount_register a6xx_perfcounters_gmu_xoclk[] = {
	/*
	 * COUNTER_XOCLK_0 and COUNTER_XOCLK_4 are used for the GPU
	 * busy and ifpc count. Mark them as reserved to ensure they
	 * are not re-used.
	 */
	GMU_COUNTER_RESERVED(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0),
	GMU_COUNTER_RESERVED(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_6_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_6_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_7_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_7_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_8_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_8_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_9_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_9_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_2),
};

static struct adreno_perfcount_register a6xx_perfcounters_gmu_gmuclk[] = {
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_GMUCLK_0_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_GMUCLK_0_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
	GMU_COUNTER(A6XX_GMU_CX_GMU_POWER_COUNTER_GMUCLK_1_L,
		A6XX_GMU_CX_GMU_POWER_COUNTER_GMUCLK_1_H,
		A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1),
};

static struct adreno_perfcount_register a6xx_perfcounters_gmu_perf[] = {
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_0_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_0_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_1_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_1_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_2_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_2_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_3_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_3_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_0),
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_4_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_4_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_1),
	GMU_COUNTER(A6XX_GMU_CX_GMU_PERF_COUNTER_5_L,
		A6XX_GMU_CX_GMU_PERF_COUNTER_5_H,
		A6XX_GMU_CX_GMU_PERF_COUNTER_SELECT_1),
};

static struct adreno_perfcount_register a6xx_perfcounters_alwayson[] = {
	{ KGSL_PERFCOUNTER_NOT_USED, 0, 0, A6XX_CP_ALWAYS_ON_COUNTER_LO,
		A6XX_CP_ALWAYS_ON_COUNTER_HI, -1 },
};

/*
 * ADRENO_PERFCOUNTER_GROUP_RESTORE flag is enabled by default
 * because most of the perfcounter groups need to be restored
 * as part of preemption and IFPC. Perfcounter groups that are
 * not restored as part of preemption and IFPC should be defined
 * using A6XX_PERFCOUNTER_GROUP_FLAGS macro
 */
#define A6XX_PERFCOUNTER_GROUP(offset, name, enable, read, load) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a6xx, offset, name, \
	ADRENO_PERFCOUNTER_GROUP_RESTORE, enable, read, load)

#define A6XX_PERFCOUNTER_GROUP_FLAGS(offset, name, flags, enable, read, load) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(a6xx, offset, name, flags, enable, \
			read, load)

#define A6XX_REGULAR_PERFCOUNTER_GROUP(offset, name) \
	A6XX_PERFCOUNTER_GROUP(offset, name, \
		a6xx_counter_enable, a6xx_counter_read, a6xx_counter_load)

static const struct adreno_perfcount_group a630_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A6XX_REGULAR_PERFCOUNTER_GROUP(CP, cp),
	A6XX_PERFCOUNTER_GROUP_FLAGS(RBBM, rbbm, 0,
		a6xx_counter_enable, a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(PC, pc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VFD, vfd),
	A6XX_PERFCOUNTER_GROUP(HLSQ, hlsq, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VPC, vpc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CCU, ccu),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CMP, cmp),
	A6XX_REGULAR_PERFCOUNTER_GROUP(TSE, tse),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RAS, ras),
	A6XX_REGULAR_PERFCOUNTER_GROUP(LRZ, lrz),
	A6XX_REGULAR_PERFCOUNTER_GROUP(UCHE, uche),
	A6XX_PERFCOUNTER_GROUP(TP, tp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_PERFCOUNTER_GROUP(SP, sp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RB, rb),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VSC, vsc),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF, vbif, 0,
		a630_counter_vbif_enable, a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, vbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED, a630_counter_vbif_pwr_enable,
		a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a6xx_counter_alwayson_enable, a6xx_counter_alwayson_read, NULL),
};

static const struct adreno_perfcount_group
a6xx_legacy_perfcounter_groups [KGSL_PERFCOUNTER_GROUP_MAX] = {
	A6XX_REGULAR_PERFCOUNTER_GROUP(CP, cp),
	A6XX_PERFCOUNTER_GROUP_FLAGS(RBBM, rbbm, 0,
		a6xx_counter_enable, a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(PC, pc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VFD, vfd),
	A6XX_PERFCOUNTER_GROUP(HLSQ, hlsq, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VPC, vpc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CCU, ccu),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CMP, cmp),
	A6XX_REGULAR_PERFCOUNTER_GROUP(TSE, tse),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RAS, ras),
	A6XX_REGULAR_PERFCOUNTER_GROUP(LRZ, lrz),
	A6XX_REGULAR_PERFCOUNTER_GROUP(UCHE, uche),
	A6XX_PERFCOUNTER_GROUP(TP, tp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_PERFCOUNTER_GROUP(SP, sp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RB, rb),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VSC, vsc),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF, gbif, 0,
		a6xx_counter_gbif_enable, a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, gbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED, a6xx_counter_gbif_pwr_enable,
		a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a6xx_counter_alwayson_enable, a6xx_counter_alwayson_read, NULL),
};

static const struct adreno_perfcount_group a6xx_perfcounter_groups
				[KGSL_PERFCOUNTER_GROUP_MAX] = {
	A6XX_REGULAR_PERFCOUNTER_GROUP(CP, cp),
	A6XX_PERFCOUNTER_GROUP_FLAGS(RBBM, rbbm, 0,
		a6xx_counter_enable, a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(PC, pc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VFD, vfd),
	A6XX_PERFCOUNTER_GROUP(HLSQ, hlsq, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VPC, vpc),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CCU, ccu),
	A6XX_REGULAR_PERFCOUNTER_GROUP(CMP, cmp),
	A6XX_REGULAR_PERFCOUNTER_GROUP(TSE, tse),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RAS, ras),
	A6XX_REGULAR_PERFCOUNTER_GROUP(LRZ, lrz),
	A6XX_REGULAR_PERFCOUNTER_GROUP(UCHE, uche),
	A6XX_PERFCOUNTER_GROUP(TP, tp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_PERFCOUNTER_GROUP(SP, sp, a6xx_counter_inline_enable,
			a6xx_counter_read, a6xx_counter_load),
	A6XX_REGULAR_PERFCOUNTER_GROUP(RB, rb),
	A6XX_REGULAR_PERFCOUNTER_GROUP(VSC, vsc),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF, gbif, 0,
		a6xx_counter_gbif_enable, a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(VBIF_PWR, gbif_pwr,
		ADRENO_PERFCOUNTER_GROUP_FIXED, a6xx_counter_gbif_pwr_enable,
		a6xx_counter_read_norestore, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(ALWAYSON, alwayson,
		ADRENO_PERFCOUNTER_GROUP_FIXED,
		a6xx_counter_alwayson_enable, a6xx_counter_alwayson_read, NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(GMU_XOCLK, gmu_xoclk, 0,
		a6xx_counter_gmu_xoclk_enable, a6xx_counter_read_norestore,
		NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(GMU_GMUCLK, gmu_gmuclk, 0,
		a6xx_counter_gmu_gmuclk_enable, a6xx_counter_read_norestore,
		NULL),
	A6XX_PERFCOUNTER_GROUP_FLAGS(GMU_PERF, gmu_perf, 0,
		a6xx_counter_gmu_perf_enable, a6xx_counter_read_norestore,
		NULL),
};

/* a610, a612, a616, a618 and a619 do not have the GMU registers.
 * a605, a608, a615, a630, a640 and a680 don't have enough room in the
 * CP_PROTECT registers so the GMU counters are not accessible
 */
const struct adreno_perfcounters adreno_a6xx_legacy_perfcounters = {
	a6xx_legacy_perfcounter_groups,
	ARRAY_SIZE(a6xx_legacy_perfcounter_groups),
};

const struct adreno_perfcounters adreno_a630_perfcounters = {
	a630_perfcounter_groups,
	ARRAY_SIZE(a630_perfcounter_groups),
};

const struct adreno_perfcounters adreno_a6xx_perfcounters = {
	a6xx_perfcounter_groups,
	ARRAY_SIZE(a6xx_perfcounter_groups),
};
