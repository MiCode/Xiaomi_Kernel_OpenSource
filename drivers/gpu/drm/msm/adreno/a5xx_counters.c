/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "a5xx_gpu.h"

/*
 * Fixed counters are not selectable, they always count the same thing.
 * The countable is an index into the group: countable 0 = register 0,
 * etc and they have no select register
 */
static int a5xx_counter_get_fixed(struct msm_gpu *gpu,
		struct adreno_counter_group *group,
		u32 countable, u32 *lo, u32 *hi)
{
	if (countable >= group->nr_counters)
		return -EINVAL;

	if (lo)
		*lo = group->counters[countable].lo;
	if (hi)
		*hi = group->counters[countable].hi;

	return countable;
}

/*
 * Most counters are selectable in that they can be programmed to count
 * different events; in most cases there are many more countables than
 * counters. When a new counter is requested, first walk the list to see if any
 * other counters in that group are counting the same countable and if so reuse
 * that counter. If not find the first empty counter in the list and register
 * that for the desired countable. If we are out of counters too bad so sad.
 */
static int a5xx_counter_get(struct msm_gpu *gpu,
		struct adreno_counter_group *group,
		u32 countable, u32 *lo, u32 *hi)
{
	struct adreno_counter *counter;
	int i, empty = -1;

	spin_lock(&group->lock);

	for (i = 0; i < group->nr_counters; i++) {
		counter = &group->counters[i];

		if (counter->refcount) {
			if (counter->countable == countable) {
				counter->refcount++;

				if (lo)
					*lo = counter->lo;
				if (hi)
					*hi = counter->hi;

				spin_unlock(&group->lock);
				return i;
			}
		} else
			empty = (empty == -1) ? i : empty;
	}

	if (empty == -1) {
		spin_unlock(&group->lock);
		return -EBUSY;
	}

	counter = &group->counters[empty];

	counter->refcount = 1;
	counter->countable = countable;

	if (lo)
		*lo = counter->lo;
	if (hi)
		*hi = counter->hi;

	spin_unlock(&group->lock);

	if (group->funcs.enable)
		group->funcs.enable(gpu, group, empty);

	return empty;
}

/* The majority of the non-fixed counter selects can be programmed by the CPU */
static void a5xx_counter_enable_cpu(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	struct adreno_counter *counter = &group->counters[counterid];

	gpu_write(gpu, counter->sel, counter->countable);
}

static void a5xx_counter_enable_pm4(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring = gpu->rb[MSM_GPU_MAX_RINGS - 1];
	struct adreno_counter *counter = &group->counters[counterid];

	mutex_lock(&gpu->dev->struct_mutex);

	/* Turn off preemption for the duration of this command */
	OUT_PKT7(ring, CP_PREEMPT_ENABLE_GLOBAL, 1);
	OUT_RING(ring, 0x02);

	/* Turn off protected mode to write to special registers */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 0);

	/* Set the save preemption record for the ring/command */
	OUT_PKT4(ring, REG_A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 2);
	OUT_RING(ring, lower_32_bits(a5xx_gpu->preempt_iova[ring->id]));
	OUT_RING(ring, upper_32_bits(a5xx_gpu->preempt_iova[ring->id]));

	/* Turn back on protected mode */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 1);

	/* Idle the GPU */
	OUT_PKT7(ring, CP_WAIT_FOR_IDLE, 0);

	/* Enable the counter */
	OUT_PKT4(ring, counter->sel, 1);
	OUT_RING(ring, counter->countable);

	/* Re-enable preemption */
	OUT_PKT7(ring, CP_PREEMPT_ENABLE_GLOBAL, 1);
	OUT_RING(ring, 0x00);

	OUT_PKT7(ring, CP_PREEMPT_ENABLE_LOCAL, 1);
	OUT_RING(ring, 0x01);

	OUT_PKT7(ring, CP_YIELD_ENABLE, 1);
	OUT_RING(ring, 0x01);

	/* Yield */
	OUT_PKT7(ring, CP_CONTEXT_SWITCH_YIELD, 4);
	OUT_RING(ring, 0x00);
	OUT_RING(ring, 0x00);
	OUT_RING(ring, 0x01);
	OUT_RING(ring, 0x01);

	gpu->funcs->flush(gpu, ring);

	/* Preempt into our ring if we need to */
	a5xx_preempt_trigger(gpu);

	/* wait for the operation to complete */
	a5xx_idle(gpu, ring);

	mutex_unlock(&gpu->dev->struct_mutex);
}

/*
 * GPMU counters are selectable but the selects are muxed together in two
 * registers
 */
static void a5xx_counter_enable_gpmu(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	struct adreno_counter *counter = &group->counters[counterid];
	u32 reg;
	int shift;

	/*
	 * The selects for the GPMU counters are grouped together in two
	 * registers, a nibble for each counter. Counters 0-3 are located in
	 * GPMU_POWER_COUNTER_SELECT0 and 4-5 are in GPMU_POWER_COUNTER_SELECT1
	 */
	if (counterid <= 3) {
		shift = counterid << 3;
		reg = REG_A5XX_GPMU_POWER_COUNTER_SELECT_0;
	} else {
		shift = (counterid - 4) << 3;
		reg = REG_A5XX_GPMU_POWER_COUNTER_SELECT_1;
	}

	gpu_rmw(gpu, reg, 0xFF << shift, (counter->countable & 0xff) << shift);
}

/* VBIF counters are selectable but have their own programming process */
static void a5xx_counter_enable_vbif(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	struct adreno_counter *counter = &group->counters[counterid];

	gpu_write(gpu, REG_A5XX_VBIF_PERF_CNT_CLR(counterid), 1);
	gpu_write(gpu, REG_A5XX_VBIF_PERF_CNT_CLR(counterid), 0);
	gpu_write(gpu, REG_A5XX_VBIF_PERF_CNT_SEL(counterid),
		counter->countable);
	gpu_write(gpu, REG_A5XX_VBIF_PERF_CNT_EN(counterid), 1);
}

/*
 * VBIF power counters are not slectable but need to be cleared/enabled before
 * use
 */
static void a5xx_counter_enable_vbif_power(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	gpu_write(gpu, REG_A5XX_VBIF_PERF_PWR_CNT_CLR(counterid), 1);
	gpu_write(gpu, REG_A5XX_VBIF_PERF_PWR_CNT_CLR(counterid), 0);
	gpu_write(gpu, REG_A5XX_VBIF_PERF_PWR_CNT_EN(counterid), 1);
}

/* GPMU always on counter needs to be enabled before use */
static void a5xx_counter_enable_alwayson_power(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	gpu_write(gpu, REG_A5XX_GPMU_ALWAYS_ON_COUNTER_RESET, 1);
}

static u64 a5xx_counter_read(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	if (counterid >= group->nr_counters)
		return 0;

	return gpu_read64(gpu, group->counters[counterid].lo,
		group->counters[counterid].hi);
}

/*
 * Selectable counters that are no longer used reset the countable to 0 to mark
 * the counter as free
 */
static void a5xx_counter_put(struct msm_gpu *gpu,
		struct adreno_counter_group *group, int counterid)
{
	struct adreno_counter *counter;

	if (counterid >= group->nr_counters)
		return;

	counter = &group->counters[counterid];

	spin_lock(&group->lock);
	if (counter->refcount > 0)
		counter->refcount--;
	spin_unlock(&group->lock);
}

static struct adreno_counter a5xx_counters_alwayson[1] = {
	{ REG_A5XX_RBBM_ALWAYSON_COUNTER_LO,
		REG_A5XX_RBBM_ALWAYSON_COUNTER_HI },
};

static struct adreno_counter a5xx_counters_ccu[] = {
	{ REG_A5XX_RBBM_PERFCTR_CCU_0_LO, REG_A5XX_RBBM_PERFCTR_CCU_0_HI,
		REG_A5XX_RB_PERFCTR_CCU_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_CCU_1_LO, REG_A5XX_RBBM_PERFCTR_CCU_1_HI,
		REG_A5XX_RB_PERFCTR_CCU_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_CCU_2_LO, REG_A5XX_RBBM_PERFCTR_CCU_2_HI,
		REG_A5XX_RB_PERFCTR_CCU_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_CCU_3_LO, REG_A5XX_RBBM_PERFCTR_CCU_3_HI,
		REG_A5XX_RB_PERFCTR_CCU_SEL_3 },
};

static struct adreno_counter a5xx_counters_cmp[] = {
	{ REG_A5XX_RBBM_PERFCTR_CMP_0_LO, REG_A5XX_RBBM_PERFCTR_CMP_0_HI,
		REG_A5XX_RB_PERFCTR_CMP_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_CMP_1_LO, REG_A5XX_RBBM_PERFCTR_CMP_1_HI,
		REG_A5XX_RB_PERFCTR_CMP_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_CMP_2_LO, REG_A5XX_RBBM_PERFCTR_CMP_2_HI,
		REG_A5XX_RB_PERFCTR_CMP_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_CMP_3_LO, REG_A5XX_RBBM_PERFCTR_CMP_3_HI,
		REG_A5XX_RB_PERFCTR_CMP_SEL_3 },
};

static struct adreno_counter a5xx_counters_cp[] = {
	{ REG_A5XX_RBBM_PERFCTR_CP_0_LO, REG_A5XX_RBBM_PERFCTR_CP_0_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_CP_1_LO, REG_A5XX_RBBM_PERFCTR_CP_1_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_CP_2_LO, REG_A5XX_RBBM_PERFCTR_CP_2_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_CP_3_LO, REG_A5XX_RBBM_PERFCTR_CP_3_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_CP_4_LO, REG_A5XX_RBBM_PERFCTR_CP_4_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_CP_5_LO, REG_A5XX_RBBM_PERFCTR_CP_5_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_CP_6_LO, REG_A5XX_RBBM_PERFCTR_CP_6_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_CP_7_LO, REG_A5XX_RBBM_PERFCTR_CP_7_HI,
		REG_A5XX_CP_PERFCTR_CP_SEL_7 },
};

static struct adreno_counter a5xx_counters_hlsq[] = {
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_0_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_0_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_1_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_1_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_2_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_2_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_3_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_3_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_4_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_4_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_5_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_5_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_6_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_6_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_HLSQ_7_LO, REG_A5XX_RBBM_PERFCTR_HLSQ_7_HI,
		REG_A5XX_HLSQ_PERFCTR_HLSQ_SEL_7 },
};

static struct adreno_counter a5xx_counters_lrz[] = {
	{ REG_A5XX_RBBM_PERFCTR_LRZ_0_LO, REG_A5XX_RBBM_PERFCTR_LRZ_0_HI,
		REG_A5XX_GRAS_PERFCTR_LRZ_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_LRZ_1_LO, REG_A5XX_RBBM_PERFCTR_LRZ_1_HI,
		REG_A5XX_GRAS_PERFCTR_LRZ_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_LRZ_2_LO, REG_A5XX_RBBM_PERFCTR_LRZ_2_HI,
		REG_A5XX_GRAS_PERFCTR_LRZ_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_LRZ_3_LO, REG_A5XX_RBBM_PERFCTR_LRZ_3_HI,
		REG_A5XX_GRAS_PERFCTR_LRZ_SEL_3 },
};

static struct adreno_counter a5xx_counters_pc[] = {
	{ REG_A5XX_RBBM_PERFCTR_PC_0_LO, REG_A5XX_RBBM_PERFCTR_PC_0_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_PC_1_LO, REG_A5XX_RBBM_PERFCTR_PC_1_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_PC_2_LO, REG_A5XX_RBBM_PERFCTR_PC_2_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_PC_3_LO, REG_A5XX_RBBM_PERFCTR_PC_3_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_PC_4_LO, REG_A5XX_RBBM_PERFCTR_PC_4_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_PC_5_LO, REG_A5XX_RBBM_PERFCTR_PC_5_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_PC_6_LO, REG_A5XX_RBBM_PERFCTR_PC_6_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_PC_7_LO, REG_A5XX_RBBM_PERFCTR_PC_7_HI,
		REG_A5XX_PC_PERFCTR_PC_SEL_7 },
};

static struct adreno_counter a5xx_counters_ras[] = {
	{ REG_A5XX_RBBM_PERFCTR_RAS_0_LO, REG_A5XX_RBBM_PERFCTR_RAS_0_HI,
		REG_A5XX_GRAS_PERFCTR_RAS_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_RAS_1_LO, REG_A5XX_RBBM_PERFCTR_RAS_1_HI,
		REG_A5XX_GRAS_PERFCTR_RAS_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_RAS_2_LO, REG_A5XX_RBBM_PERFCTR_RAS_2_HI,
		REG_A5XX_GRAS_PERFCTR_RAS_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_RAS_3_LO, REG_A5XX_RBBM_PERFCTR_RAS_3_HI,
		REG_A5XX_GRAS_PERFCTR_RAS_SEL_3 },
};

static struct adreno_counter a5xx_counters_rb[] = {
	{ REG_A5XX_RBBM_PERFCTR_RB_0_LO, REG_A5XX_RBBM_PERFCTR_RB_0_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_RB_1_LO, REG_A5XX_RBBM_PERFCTR_RB_1_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_RB_2_LO, REG_A5XX_RBBM_PERFCTR_RB_2_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_RB_3_LO, REG_A5XX_RBBM_PERFCTR_RB_3_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_RB_4_LO, REG_A5XX_RBBM_PERFCTR_RB_4_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_RB_5_LO, REG_A5XX_RBBM_PERFCTR_RB_5_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_RB_6_LO, REG_A5XX_RBBM_PERFCTR_RB_6_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_RB_7_LO, REG_A5XX_RBBM_PERFCTR_RB_7_HI,
		REG_A5XX_RB_PERFCTR_RB_SEL_7 },
};

static struct adreno_counter a5xx_counters_rbbm[] = {
	{ REG_A5XX_RBBM_PERFCTR_RBBM_0_LO, REG_A5XX_RBBM_PERFCTR_RBBM_0_HI,
		REG_A5XX_RBBM_PERFCTR_RBBM_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_RBBM_1_LO, REG_A5XX_RBBM_PERFCTR_RBBM_1_HI,
		REG_A5XX_RBBM_PERFCTR_RBBM_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_RBBM_2_LO, REG_A5XX_RBBM_PERFCTR_RBBM_2_HI,
		REG_A5XX_RBBM_PERFCTR_RBBM_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_RBBM_3_LO, REG_A5XX_RBBM_PERFCTR_RBBM_3_HI,
		REG_A5XX_RBBM_PERFCTR_RBBM_SEL_3 },
};

static struct adreno_counter a5xx_counters_sp[] = {
	{ REG_A5XX_RBBM_PERFCTR_SP_0_LO, REG_A5XX_RBBM_PERFCTR_SP_0_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_SP_1_LO, REG_A5XX_RBBM_PERFCTR_SP_1_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_SP_2_LO, REG_A5XX_RBBM_PERFCTR_SP_2_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_SP_3_LO, REG_A5XX_RBBM_PERFCTR_SP_3_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_SP_4_LO, REG_A5XX_RBBM_PERFCTR_SP_4_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_SP_5_LO, REG_A5XX_RBBM_PERFCTR_SP_5_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_SP_6_LO, REG_A5XX_RBBM_PERFCTR_SP_6_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_SP_7_LO, REG_A5XX_RBBM_PERFCTR_SP_7_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_7 },
	{ REG_A5XX_RBBM_PERFCTR_SP_8_LO, REG_A5XX_RBBM_PERFCTR_SP_8_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_8 },
	{ REG_A5XX_RBBM_PERFCTR_SP_9_LO, REG_A5XX_RBBM_PERFCTR_SP_9_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_9 },
	{ REG_A5XX_RBBM_PERFCTR_SP_10_LO, REG_A5XX_RBBM_PERFCTR_SP_10_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_10 },
	{ REG_A5XX_RBBM_PERFCTR_SP_11_LO, REG_A5XX_RBBM_PERFCTR_SP_11_HI,
		REG_A5XX_SP_PERFCTR_SP_SEL_11 },
};

static struct adreno_counter a5xx_counters_tp[] = {
	{ REG_A5XX_RBBM_PERFCTR_TP_0_LO, REG_A5XX_RBBM_PERFCTR_TP_0_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_TP_1_LO, REG_A5XX_RBBM_PERFCTR_TP_1_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_TP_2_LO, REG_A5XX_RBBM_PERFCTR_TP_2_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_TP_3_LO, REG_A5XX_RBBM_PERFCTR_TP_3_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_TP_4_LO, REG_A5XX_RBBM_PERFCTR_TP_4_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_TP_5_LO, REG_A5XX_RBBM_PERFCTR_TP_5_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_TP_6_LO, REG_A5XX_RBBM_PERFCTR_TP_6_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_TP_7_LO, REG_A5XX_RBBM_PERFCTR_TP_7_HI,
		REG_A5XX_TPL1_PERFCTR_TP_SEL_7 },
};

static struct adreno_counter a5xx_counters_tse[] = {
	{ REG_A5XX_RBBM_PERFCTR_TSE_0_LO, REG_A5XX_RBBM_PERFCTR_TSE_0_HI,
		REG_A5XX_GRAS_PERFCTR_TSE_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_TSE_1_LO, REG_A5XX_RBBM_PERFCTR_TSE_1_HI,
		REG_A5XX_GRAS_PERFCTR_TSE_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_TSE_2_LO, REG_A5XX_RBBM_PERFCTR_TSE_2_HI,
		REG_A5XX_GRAS_PERFCTR_TSE_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_TSE_3_LO, REG_A5XX_RBBM_PERFCTR_TSE_3_HI,
		REG_A5XX_GRAS_PERFCTR_TSE_SEL_3 },
};

static struct adreno_counter a5xx_counters_uche[] = {
	{ REG_A5XX_RBBM_PERFCTR_UCHE_0_LO, REG_A5XX_RBBM_PERFCTR_UCHE_0_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_1_LO, REG_A5XX_RBBM_PERFCTR_UCHE_1_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_2_LO, REG_A5XX_RBBM_PERFCTR_UCHE_2_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_3_LO, REG_A5XX_RBBM_PERFCTR_UCHE_3_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_4_LO, REG_A5XX_RBBM_PERFCTR_UCHE_4_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_5_LO, REG_A5XX_RBBM_PERFCTR_UCHE_5_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_6_LO, REG_A5XX_RBBM_PERFCTR_UCHE_6_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_UCHE_7_LO, REG_A5XX_RBBM_PERFCTR_UCHE_7_HI,
		REG_A5XX_UCHE_PERFCTR_UCHE_SEL_7 },
};

static struct adreno_counter a5xx_counters_vfd[] = {
	{ REG_A5XX_RBBM_PERFCTR_VFD_0_LO, REG_A5XX_RBBM_PERFCTR_VFD_0_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_1_LO, REG_A5XX_RBBM_PERFCTR_VFD_1_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_2_LO, REG_A5XX_RBBM_PERFCTR_VFD_2_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_3_LO, REG_A5XX_RBBM_PERFCTR_VFD_3_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_3 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_4_LO, REG_A5XX_RBBM_PERFCTR_VFD_4_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_4 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_5_LO, REG_A5XX_RBBM_PERFCTR_VFD_5_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_5 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_6_LO, REG_A5XX_RBBM_PERFCTR_VFD_6_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_6 },
	{ REG_A5XX_RBBM_PERFCTR_VFD_7_LO, REG_A5XX_RBBM_PERFCTR_VFD_7_HI,
		REG_A5XX_VFD_PERFCTR_VFD_SEL_7 },
};

static struct adreno_counter a5xx_counters_vpc[] = {
	{ REG_A5XX_RBBM_PERFCTR_VPC_0_LO, REG_A5XX_RBBM_PERFCTR_VPC_0_HI,
		REG_A5XX_VPC_PERFCTR_VPC_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_VPC_1_LO, REG_A5XX_RBBM_PERFCTR_VPC_1_HI,
		REG_A5XX_VPC_PERFCTR_VPC_SEL_1 },
	{ REG_A5XX_RBBM_PERFCTR_VPC_2_LO, REG_A5XX_RBBM_PERFCTR_VPC_2_HI,
		REG_A5XX_VPC_PERFCTR_VPC_SEL_2 },
	{ REG_A5XX_RBBM_PERFCTR_VPC_3_LO, REG_A5XX_RBBM_PERFCTR_VPC_3_HI,
		REG_A5XX_VPC_PERFCTR_VPC_SEL_3 },
};

static struct adreno_counter a5xx_counters_vsc[] = {
	{ REG_A5XX_RBBM_PERFCTR_VSC_0_LO, REG_A5XX_RBBM_PERFCTR_VSC_0_HI,
		REG_A5XX_VSC_PERFCTR_VSC_SEL_0 },
	{ REG_A5XX_RBBM_PERFCTR_VSC_1_LO, REG_A5XX_RBBM_PERFCTR_VSC_1_HI,
		REG_A5XX_VSC_PERFCTR_VSC_SEL_1 },
};

static struct adreno_counter a5xx_counters_power_ccu[] = {
	{ REG_A5XX_CCU_POWER_COUNTER_0_LO, REG_A5XX_CCU_POWER_COUNTER_0_HI,
		REG_A5XX_RB_POWERCTR_CCU_SEL_0 },
	{ REG_A5XX_CCU_POWER_COUNTER_1_LO, REG_A5XX_CCU_POWER_COUNTER_1_HI,
		REG_A5XX_RB_POWERCTR_CCU_SEL_1 },
};

static struct adreno_counter a5xx_counters_power_cp[] = {
	{ REG_A5XX_CP_POWER_COUNTER_0_LO, REG_A5XX_CP_POWER_COUNTER_0_HI,
		REG_A5XX_CP_POWERCTR_CP_SEL_0 },
	{ REG_A5XX_CP_POWER_COUNTER_1_LO, REG_A5XX_CP_POWER_COUNTER_1_HI,
		REG_A5XX_CP_POWERCTR_CP_SEL_1 },
	{ REG_A5XX_CP_POWER_COUNTER_2_LO, REG_A5XX_CP_POWER_COUNTER_2_HI,
		REG_A5XX_CP_POWERCTR_CP_SEL_2 },
	{ REG_A5XX_CP_POWER_COUNTER_3_LO, REG_A5XX_CP_POWER_COUNTER_3_HI,
		REG_A5XX_CP_POWERCTR_CP_SEL_3 },
};

static struct adreno_counter a5xx_counters_power_rb[] = {
	{ REG_A5XX_RB_POWER_COUNTER_0_LO, REG_A5XX_RB_POWER_COUNTER_0_HI,
		REG_A5XX_RB_POWERCTR_RB_SEL_0 },
	{ REG_A5XX_RB_POWER_COUNTER_1_LO, REG_A5XX_RB_POWER_COUNTER_1_HI,
		REG_A5XX_RB_POWERCTR_RB_SEL_1 },
	{ REG_A5XX_RB_POWER_COUNTER_2_LO, REG_A5XX_RB_POWER_COUNTER_2_HI,
		REG_A5XX_RB_POWERCTR_RB_SEL_2 },
	{ REG_A5XX_RB_POWER_COUNTER_3_LO, REG_A5XX_RB_POWER_COUNTER_3_HI,
		REG_A5XX_RB_POWERCTR_RB_SEL_3 },
};

static struct adreno_counter a5xx_counters_power_sp[] = {
	{ REG_A5XX_SP_POWER_COUNTER_0_LO, REG_A5XX_SP_POWER_COUNTER_0_HI,
		REG_A5XX_SP_POWERCTR_SP_SEL_0 },
	{ REG_A5XX_SP_POWER_COUNTER_1_LO, REG_A5XX_SP_POWER_COUNTER_1_HI,
		REG_A5XX_SP_POWERCTR_SP_SEL_1 },
	{ REG_A5XX_SP_POWER_COUNTER_2_LO, REG_A5XX_SP_POWER_COUNTER_2_HI,
		REG_A5XX_SP_POWERCTR_SP_SEL_2 },
	{ REG_A5XX_SP_POWER_COUNTER_3_LO, REG_A5XX_SP_POWER_COUNTER_3_HI,
		REG_A5XX_SP_POWERCTR_SP_SEL_3 },
};

static struct adreno_counter a5xx_counters_power_tp[] = {
	{ REG_A5XX_TP_POWER_COUNTER_0_LO, REG_A5XX_TP_POWER_COUNTER_0_HI,
		REG_A5XX_TPL1_POWERCTR_TP_SEL_0 },
	{ REG_A5XX_TP_POWER_COUNTER_1_LO, REG_A5XX_TP_POWER_COUNTER_1_HI,
		REG_A5XX_TPL1_POWERCTR_TP_SEL_1 },
	{ REG_A5XX_TP_POWER_COUNTER_2_LO, REG_A5XX_TP_POWER_COUNTER_2_HI,
		REG_A5XX_TPL1_POWERCTR_TP_SEL_2 },
	{ REG_A5XX_TP_POWER_COUNTER_3_LO, REG_A5XX_TP_POWER_COUNTER_3_HI,
		REG_A5XX_TPL1_POWERCTR_TP_SEL_3 },
};

static struct adreno_counter a5xx_counters_power_uche[] = {
	{ REG_A5XX_UCHE_POWER_COUNTER_0_LO, REG_A5XX_UCHE_POWER_COUNTER_0_HI,
		REG_A5XX_UCHE_POWERCTR_UCHE_SEL_0 },
	{ REG_A5XX_UCHE_POWER_COUNTER_1_LO, REG_A5XX_UCHE_POWER_COUNTER_1_HI,
		REG_A5XX_UCHE_POWERCTR_UCHE_SEL_1 },
	{ REG_A5XX_UCHE_POWER_COUNTER_2_LO, REG_A5XX_UCHE_POWER_COUNTER_2_HI,
		REG_A5XX_UCHE_POWERCTR_UCHE_SEL_2 },
	{ REG_A5XX_UCHE_POWER_COUNTER_3_LO, REG_A5XX_UCHE_POWER_COUNTER_3_HI,
		REG_A5XX_UCHE_POWERCTR_UCHE_SEL_3 },
};

static struct adreno_counter a5xx_counters_vbif[] = {
	{ REG_A5XX_VBIF_PERF_CNT_LOW0, REG_A5XX_VBIF_PERF_CNT_HIGH0 },
	{ REG_A5XX_VBIF_PERF_CNT_LOW1, REG_A5XX_VBIF_PERF_CNT_HIGH1 },
	{ REG_A5XX_VBIF_PERF_CNT_LOW2, REG_A5XX_VBIF_PERF_CNT_HIGH2 },
	{ REG_A5XX_VBIF_PERF_CNT_LOW3, REG_A5XX_VBIF_PERF_CNT_HIGH3 },
};

static struct adreno_counter a5xx_counters_gpmu[] = {
	{ REG_A5XX_GPMU_POWER_COUNTER_0_LO, REG_A5XX_GPMU_POWER_COUNTER_0_HI },
	{ REG_A5XX_GPMU_POWER_COUNTER_1_LO, REG_A5XX_GPMU_POWER_COUNTER_1_HI },
	{ REG_A5XX_GPMU_POWER_COUNTER_2_LO, REG_A5XX_GPMU_POWER_COUNTER_2_HI },
	{ REG_A5XX_GPMU_POWER_COUNTER_3_LO, REG_A5XX_GPMU_POWER_COUNTER_3_HI },
	{ REG_A5XX_GPMU_POWER_COUNTER_4_LO, REG_A5XX_GPMU_POWER_COUNTER_4_HI },
	{ REG_A5XX_GPMU_POWER_COUNTER_5_LO, REG_A5XX_GPMU_POWER_COUNTER_5_HI },
};

static struct adreno_counter a5xx_counters_vbif_power[] = {
	{ REG_A5XX_VBIF_PERF_PWR_CNT_LOW0, REG_A5XX_VBIF_PERF_PWR_CNT_HIGH0 },
	{ REG_A5XX_VBIF_PERF_PWR_CNT_LOW1, REG_A5XX_VBIF_PERF_PWR_CNT_HIGH1 },
	{ REG_A5XX_VBIF_PERF_PWR_CNT_LOW2, REG_A5XX_VBIF_PERF_PWR_CNT_HIGH2 },
};

static struct adreno_counter a5xx_counters_alwayson_power[] = {
	{ REG_A5XX_GPMU_ALWAYS_ON_COUNTER_LO,
		REG_A5XX_GPMU_ALWAYS_ON_COUNTER_HI },
};

#define DEFINE_COUNTER_GROUP(_name, _array, _get, _enable, _put) \
static struct adreno_counter_group _name = { \
	.counters = _array, \
	.nr_counters = ARRAY_SIZE(_array), \
	.lock = __SPIN_LOCK_UNLOCKED(_name.lock), \
	.funcs = { \
		.get = _get, \
		.enable = _enable, \
		.read = a5xx_counter_read, \
		.put = _put, \
	}, \
}

#define DEFAULT_COUNTER_GROUP(_name, _array) DEFINE_COUNTER_GROUP(_name, \
	_array, a5xx_counter_get, a5xx_counter_enable_cpu, a5xx_counter_put)

#define SPTP_COUNTER_GROUP(_name, _array) DEFINE_COUNTER_GROUP(_name, \
	_array, a5xx_counter_get, a5xx_counter_enable_pm4, a5xx_counter_put)

/* "standard" counters */
DEFAULT_COUNTER_GROUP(a5xx_counter_group_cp, a5xx_counters_cp);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_rbbm, a5xx_counters_rbbm);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_pc, a5xx_counters_pc);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_vfd, a5xx_counters_vfd);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_vpc, a5xx_counters_vpc);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_ccu, a5xx_counters_ccu);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_cmp, a5xx_counters_cmp);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_tse, a5xx_counters_tse);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_ras, a5xx_counters_ras);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_uche, a5xx_counters_uche);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_rb, a5xx_counters_rb);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_vsc, a5xx_counters_vsc);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_lrz, a5xx_counters_lrz);

/* SP/TP counters */
SPTP_COUNTER_GROUP(a5xx_counter_group_hlsq, a5xx_counters_hlsq);
SPTP_COUNTER_GROUP(a5xx_counter_group_tp, a5xx_counters_tp);
SPTP_COUNTER_GROUP(a5xx_counter_group_sp, a5xx_counters_sp);

/* Power counters */
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_ccu, a5xx_counters_power_ccu);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_cp, a5xx_counters_power_cp);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_rb, a5xx_counters_power_rb);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_sp, a5xx_counters_power_sp);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_tp, a5xx_counters_power_tp);
DEFAULT_COUNTER_GROUP(a5xx_counter_group_power_uche, a5xx_counters_power_uche);

DEFINE_COUNTER_GROUP(a5xx_counter_group_alwayson, a5xx_counters_alwayson,
	a5xx_counter_get_fixed, NULL, NULL);
DEFINE_COUNTER_GROUP(a5xx_counter_group_vbif, a5xx_counters_vbif,
	a5xx_counter_get, a5xx_counter_enable_vbif, a5xx_counter_put);
DEFINE_COUNTER_GROUP(a5xx_counter_group_gpmu, a5xx_counters_gpmu,
	a5xx_counter_get, a5xx_counter_enable_gpmu, a5xx_counter_put);
DEFINE_COUNTER_GROUP(a5xx_counter_group_vbif_power, a5xx_counters_vbif_power,
	a5xx_counter_get_fixed, a5xx_counter_enable_vbif_power, NULL);
DEFINE_COUNTER_GROUP(a5xx_counter_group_alwayson_power,
		a5xx_counters_alwayson_power, a5xx_counter_get_fixed,
		a5xx_counter_enable_alwayson_power, NULL);

static const struct adreno_counter_group *a5xx_counter_groups[] = {
	[MSM_COUNTER_GROUP_ALWAYSON] = &a5xx_counter_group_alwayson,
	[MSM_COUNTER_GROUP_CCU] = &a5xx_counter_group_ccu,
	[MSM_COUNTER_GROUP_CMP] = &a5xx_counter_group_cmp,
	[MSM_COUNTER_GROUP_CP] = &a5xx_counter_group_cp,
	[MSM_COUNTER_GROUP_HLSQ] = &a5xx_counter_group_hlsq,
	[MSM_COUNTER_GROUP_LRZ] = &a5xx_counter_group_lrz,
	[MSM_COUNTER_GROUP_PC] = &a5xx_counter_group_pc,
	[MSM_COUNTER_GROUP_RAS] = &a5xx_counter_group_ras,
	[MSM_COUNTER_GROUP_RB] = &a5xx_counter_group_rb,
	[MSM_COUNTER_GROUP_RBBM] = &a5xx_counter_group_rbbm,
	[MSM_COUNTER_GROUP_SP] = &a5xx_counter_group_sp,
	[MSM_COUNTER_GROUP_TP] = &a5xx_counter_group_tp,
	[MSM_COUNTER_GROUP_TSE] = &a5xx_counter_group_tse,
	[MSM_COUNTER_GROUP_UCHE] = &a5xx_counter_group_uche,
	[MSM_COUNTER_GROUP_VFD] = &a5xx_counter_group_vfd,
	[MSM_COUNTER_GROUP_VPC] = &a5xx_counter_group_vpc,
	[MSM_COUNTER_GROUP_VSC] = &a5xx_counter_group_vsc,
	[MSM_COUNTER_GROUP_VBIF] = &a5xx_counter_group_vbif,
	[MSM_COUNTER_GROUP_GPMU_PWR] = &a5xx_counter_group_gpmu,
	[MSM_COUNTER_GROUP_CCU_PWR] = &a5xx_counter_group_power_ccu,
	[MSM_COUNTER_GROUP_CP_PWR] = &a5xx_counter_group_power_cp,
	[MSM_COUNTER_GROUP_RB_PWR] = &a5xx_counter_group_power_rb,
	[MSM_COUNTER_GROUP_SP_PWR] = &a5xx_counter_group_power_sp,
	[MSM_COUNTER_GROUP_TP_PWR] = &a5xx_counter_group_power_tp,
	[MSM_COUNTER_GROUP_UCHE_PWR] = &a5xx_counter_group_power_uche,
	[MSM_COUNTER_GROUP_VBIF_PWR] = &a5xx_counter_group_vbif_power,
	[MSM_COUNTER_GROUP_ALWAYSON_PWR] =
		&a5xx_counter_group_alwayson_power,
};

int a5xx_counters_init(struct adreno_gpu *adreno_gpu)
{
	adreno_gpu->counter_groups = a5xx_counter_groups;
	adreno_gpu->nr_counter_groups = ARRAY_SIZE(a5xx_counter_groups);

	return 0;
}
