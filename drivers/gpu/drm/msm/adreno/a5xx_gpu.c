/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#include "msm_gem.h"
#include "msm_iommu.h"
#include "a5xx_gpu.h"

static void a5xx_flush(struct msm_gpu *gpu, struct msm_ringbuffer *ring)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	uint32_t wptr;
	unsigned long flags;

	spin_lock_irqsave(&ring->lock, flags);

	/* Copy the shadow to the actual register */
	ring->cur = ring->next;

	/* Make sure to wrap wptr if we need to */
	wptr = get_wptr(ring);

	spin_unlock_irqrestore(&ring->lock, flags);

	/* Make sure everything is posted before making a decision */
	mb();

	/* Update HW if this is the current ring and we are not in preempt */
	if (a5xx_gpu->cur_ring == ring && !a5xx_in_preempt(a5xx_gpu))
		gpu_write(gpu, REG_A5XX_CP_RB_WPTR, wptr);
}

static void a5xx_set_pagetable(struct msm_gpu *gpu, struct msm_ringbuffer *ring,
	struct msm_gem_address_space *aspace)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct msm_mmu *mmu = aspace->mmu;
	struct msm_iommu *iommu = to_msm_iommu(mmu);

	if (!iommu->ttbr0)
		return;

	/* Turn off protected mode */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 0);

	/* Turn on APIV mode to access critical regions */
	OUT_PKT4(ring, REG_A5XX_CP_CNTL, 1);
	OUT_RING(ring, 1);

	/* Make sure the ME is syncronized before staring the update */
	OUT_PKT7(ring, CP_WAIT_FOR_ME, 0);

	/* Execute the table update */
	OUT_PKT7(ring, CP_SMMU_TABLE_UPDATE, 3);
	OUT_RING(ring, lower_32_bits(iommu->ttbr0));
	OUT_RING(ring, upper_32_bits(iommu->ttbr0));
	OUT_RING(ring, iommu->contextidr);

	/*
	 * Write the new TTBR0 to the preemption records - this will be used to
	 * reload the pagetable if the current ring gets preempted out.
	 */
	OUT_PKT7(ring, CP_MEM_WRITE, 4);
	OUT_RING(ring, lower_32_bits(rbmemptr(adreno_gpu, ring->id, ttbr0)));
	OUT_RING(ring, upper_32_bits(rbmemptr(adreno_gpu, ring->id, ttbr0)));
	OUT_RING(ring, lower_32_bits(iommu->ttbr0));
	OUT_RING(ring, upper_32_bits(iommu->ttbr0));

	/* Also write the current contextidr (ASID) */
	OUT_PKT7(ring, CP_MEM_WRITE, 3);
	OUT_RING(ring, lower_32_bits(rbmemptr(adreno_gpu, ring->id,
		contextidr)));
	OUT_RING(ring, upper_32_bits(rbmemptr(adreno_gpu, ring->id,
		contextidr)));
	OUT_RING(ring, iommu->contextidr);

	/* Invalidate the draw state so we start off fresh */
	OUT_PKT7(ring, CP_SET_DRAW_STATE, 3);
	OUT_RING(ring, 0x40000);
	OUT_RING(ring, 1);
	OUT_RING(ring, 0);

	/* Turn off APRIV */
	OUT_PKT4(ring, REG_A5XX_CP_CNTL, 1);
	OUT_RING(ring, 0);

	/* Turn off protected mode */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 1);
}

static int a5xx_submit(struct msm_gpu *gpu, struct msm_gem_submit *submit)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring = gpu->rb[submit->ring];
	unsigned int i, ibs = 0;

	a5xx_set_pagetable(gpu, ring, submit->aspace);

	OUT_PKT7(ring, CP_PREEMPT_ENABLE_GLOBAL, 1);
	OUT_RING(ring, 0x02);

	/* Turn off protected mode to write to special registers */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 0);

	/* Set the save preemption record for the ring/command */
	OUT_PKT4(ring, REG_A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 2);
	OUT_RING(ring, lower_32_bits(a5xx_gpu->preempt_iova[submit->ring]));
	OUT_RING(ring, upper_32_bits(a5xx_gpu->preempt_iova[submit->ring]));

	/* Turn back on protected mode */
	OUT_PKT7(ring, CP_SET_PROTECTED_MODE, 1);
	OUT_RING(ring, 1);

	/* Enable local preemption for finegrain preemption */
	OUT_PKT7(ring, CP_PREEMPT_ENABLE_GLOBAL, 1);
	OUT_RING(ring, 0x02);

	/* Allow CP_CONTEXT_SWITCH_YIELD packets in the IB2 */
	OUT_PKT7(ring, CP_YIELD_ENABLE, 1);
	OUT_RING(ring, 0x02);

	/* Submit the commands */
	for (i = 0; i < submit->nr_cmds; i++) {
		switch (submit->cmd[i].type) {
		case MSM_SUBMIT_CMD_IB_TARGET_BUF:
			break;
		case MSM_SUBMIT_CMD_BUF:
			OUT_PKT7(ring, CP_INDIRECT_BUFFER_PFE, 3);
			OUT_RING(ring, lower_32_bits(submit->cmd[i].iova));
			OUT_RING(ring, upper_32_bits(submit->cmd[i].iova));
			OUT_RING(ring, submit->cmd[i].size);
			ibs++;
			break;
		}
	}

	/*
	 * Write the render mode to NULL (0) to indicate to the CP that the IBs
	 * are done rendering - otherwise a lucky preemption would start
	 * replaying from the last checkpoint
	 */
	OUT_PKT7(ring, CP_SET_RENDER_MODE, 5);
	OUT_RING(ring, 0);
	OUT_RING(ring, 0);
	OUT_RING(ring, 0);
	OUT_RING(ring, 0);
	OUT_RING(ring, 0);

	/* Turn off IB level preemptions */
	OUT_PKT7(ring, CP_YIELD_ENABLE, 1);
	OUT_RING(ring, 0x01);

	/* Write the fence to the scratch register */
	OUT_PKT4(ring, REG_A5XX_CP_SCRATCH_REG(2), 1);
	OUT_RING(ring, submit->fence);

	/*
	 * Execute a CACHE_FLUSH_TS event. This will ensure that the
	 * timestamp is written to the memory and then triggers the interrupt
	 */
	OUT_PKT7(ring, CP_EVENT_WRITE, 4);
	OUT_RING(ring, CACHE_FLUSH_TS | (1 << 31));

	OUT_RING(ring, lower_32_bits(rbmemptr(adreno_gpu, ring->id, fence)));
	OUT_RING(ring, upper_32_bits(rbmemptr(adreno_gpu, ring->id, fence)));
	OUT_RING(ring, submit->fence);

	/* Yield the floor on command completion */
	OUT_PKT7(ring, CP_CONTEXT_SWITCH_YIELD, 4);
	/*
	 * If dword[2:1] are non zero, they specify an address for the CP to
	 * write the value of dword[3] to on preemption complete. Write 0 to
	 * skip the write
	 */
	OUT_RING(ring, 0x00);
	OUT_RING(ring, 0x00);
	/* Data value - not used if the address above is 0 */
	OUT_RING(ring, 0x01);
	/* Set bit 0 to trigger an interrupt on preempt complete */
	OUT_RING(ring, 0x01);

	a5xx_flush(gpu, ring);

	/* Check to see if we need to start preemption */
	a5xx_preempt_trigger(gpu);

	return 0;
}

static const struct {
	u32 offset;
	u32 value;
} a5xx_hwcg[] = {
	{REG_A5XX_RBBM_CLOCK_CNTL_SP0, 0x02222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_SP1, 0x02222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_SP2, 0x02222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_SP3, 0x02222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_SP0, 0x02222220},
	{REG_A5XX_RBBM_CLOCK_CNTL2_SP1, 0x02222220},
	{REG_A5XX_RBBM_CLOCK_CNTL2_SP2, 0x02222220},
	{REG_A5XX_RBBM_CLOCK_CNTL2_SP3, 0x02222220},
	{REG_A5XX_RBBM_CLOCK_HYST_SP0, 0x0000F3CF},
	{REG_A5XX_RBBM_CLOCK_HYST_SP1, 0x0000F3CF},
	{REG_A5XX_RBBM_CLOCK_HYST_SP2, 0x0000F3CF},
	{REG_A5XX_RBBM_CLOCK_HYST_SP3, 0x0000F3CF},
	{REG_A5XX_RBBM_CLOCK_DELAY_SP0, 0x00000080},
	{REG_A5XX_RBBM_CLOCK_DELAY_SP1, 0x00000080},
	{REG_A5XX_RBBM_CLOCK_DELAY_SP2, 0x00000080},
	{REG_A5XX_RBBM_CLOCK_DELAY_SP3, 0x00000080},
	{REG_A5XX_RBBM_CLOCK_CNTL_TP0, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_TP1, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_TP2, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_TP3, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_TP0, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_TP1, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_TP2, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_TP3, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL3_TP0, 0x00002222},
	{REG_A5XX_RBBM_CLOCK_CNTL3_TP1, 0x00002222},
	{REG_A5XX_RBBM_CLOCK_CNTL3_TP2, 0x00002222},
	{REG_A5XX_RBBM_CLOCK_CNTL3_TP3, 0x00002222},
	{REG_A5XX_RBBM_CLOCK_HYST_TP0, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST_TP1, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST_TP2, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST_TP3, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST2_TP0, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST2_TP1, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST2_TP2, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST2_TP3, 0x77777777},
	{REG_A5XX_RBBM_CLOCK_HYST3_TP0, 0x00007777},
	{REG_A5XX_RBBM_CLOCK_HYST3_TP1, 0x00007777},
	{REG_A5XX_RBBM_CLOCK_HYST3_TP2, 0x00007777},
	{REG_A5XX_RBBM_CLOCK_HYST3_TP3, 0x00007777},
	{REG_A5XX_RBBM_CLOCK_DELAY_TP0, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY_TP1, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY_TP2, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY_TP3, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY2_TP0, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY2_TP1, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY2_TP2, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY2_TP3, 0x11111111},
	{REG_A5XX_RBBM_CLOCK_DELAY3_TP0, 0x00001111},
	{REG_A5XX_RBBM_CLOCK_DELAY3_TP1, 0x00001111},
	{REG_A5XX_RBBM_CLOCK_DELAY3_TP2, 0x00001111},
	{REG_A5XX_RBBM_CLOCK_DELAY3_TP3, 0x00001111},
	{REG_A5XX_RBBM_CLOCK_CNTL_UCHE, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_UCHE, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL3_UCHE, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL4_UCHE, 0x00222222},
	{REG_A5XX_RBBM_CLOCK_HYST_UCHE, 0x00444444},
	{REG_A5XX_RBBM_CLOCK_DELAY_UCHE, 0x00000002},
	{REG_A5XX_RBBM_CLOCK_CNTL_RB0, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_RB1, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_RB2, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_RB3, 0x22222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_RB0, 0x00222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_RB1, 0x00222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_RB2, 0x00222222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_RB3, 0x00222222},
	{REG_A5XX_RBBM_CLOCK_CNTL_CCU0, 0x00022220},
	{REG_A5XX_RBBM_CLOCK_CNTL_CCU1, 0x00022220},
	{REG_A5XX_RBBM_CLOCK_CNTL_CCU2, 0x00022220},
	{REG_A5XX_RBBM_CLOCK_CNTL_CCU3, 0x00022220},
	{REG_A5XX_RBBM_CLOCK_CNTL_RAC, 0x05522222},
	{REG_A5XX_RBBM_CLOCK_CNTL2_RAC, 0x00505555},
	{REG_A5XX_RBBM_CLOCK_HYST_RB_CCU0, 0x04040404},
	{REG_A5XX_RBBM_CLOCK_HYST_RB_CCU1, 0x04040404},
	{REG_A5XX_RBBM_CLOCK_HYST_RB_CCU2, 0x04040404},
	{REG_A5XX_RBBM_CLOCK_HYST_RB_CCU3, 0x04040404},
	{REG_A5XX_RBBM_CLOCK_HYST_RAC, 0x07444044},
	{REG_A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_0, 0x00000002},
	{REG_A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_1, 0x00000002},
	{REG_A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_2, 0x00000002},
	{REG_A5XX_RBBM_CLOCK_DELAY_RB_CCU_L1_3, 0x00000002},
	{REG_A5XX_RBBM_CLOCK_DELAY_RAC, 0x00010011},
	{REG_A5XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM, 0x04222222},
	{REG_A5XX_RBBM_CLOCK_MODE_GPC, 0x02222222},
	{REG_A5XX_RBBM_CLOCK_MODE_VFD, 0x00002222},
	{REG_A5XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM, 0x00000000},
	{REG_A5XX_RBBM_CLOCK_HYST_GPC, 0x04104004},
	{REG_A5XX_RBBM_CLOCK_HYST_VFD, 0x00000000},
	{REG_A5XX_RBBM_CLOCK_DELAY_HLSQ, 0x00000000},
	{REG_A5XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM, 0x00004000},
	{REG_A5XX_RBBM_CLOCK_DELAY_GPC, 0x00000200},
	{REG_A5XX_RBBM_CLOCK_DELAY_VFD, 0x00002222}
};

void a5xx_set_hwcg(struct msm_gpu *gpu, bool state)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(a5xx_hwcg); i++)
		gpu_write(gpu, a5xx_hwcg[i].offset,
			state ? a5xx_hwcg[i].value : 0);

	/* There are a few additional registers just for A540 */
	if (adreno_is_a540(adreno_gpu)) {
		gpu_write(gpu, REG_A5XX_RBBM_CLOCK_DELAY_GPMU,
			state  ? 0x770 : 0);
		gpu_write(gpu, REG_A5XX_RBBM_CLOCK_HYST_GPMU,
			state ? 0x004 : 0);
	}

	gpu_write(gpu, REG_A5XX_RBBM_CLOCK_CNTL, state ? 0xAAA8AA00 : 0);
	gpu_write(gpu, REG_A5XX_RBBM_ISDB_CNT, state ? 0x182 : 0x180);
}

static int a5xx_me_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct msm_ringbuffer *ring = gpu->rb[0];

	OUT_PKT7(ring, CP_ME_INIT, 8);

	OUT_RING(ring, 0x0000002F);

	/* Enable multiple hardware contexts */
	OUT_RING(ring, 0x00000003);

	/* Enable error detection */
	OUT_RING(ring, 0x20000000);

	/* Don't enable header dump */
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	/* Specify workarounds for various microcode issues */
	if (adreno_is_a530(adreno_gpu)) {
		/* Workaround for token end syncs
		 * Force a WFI after every direct-render 3D mode draw and every
		 * 2D mode 3 draw
		 */
		OUT_RING(ring, 0x0000000B);
	} else {
		/* No workarounds enabled */
		OUT_RING(ring, 0x00000000);
	}

	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	gpu->funcs->flush(gpu, ring);
	return a5xx_idle(gpu, ring) ? 0 : -EINVAL;
}

static int a5xx_preempt_start(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring = gpu->rb[0];

	if (gpu->nr_rings == 1)
		return 0;

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

	OUT_PKT7(ring, CP_PREEMPT_ENABLE_GLOBAL, 1);
	OUT_RING(ring, 0x00);

	OUT_PKT7(ring, CP_PREEMPT_ENABLE_LOCAL, 1);
	OUT_RING(ring, 0x01);

	OUT_PKT7(ring, CP_YIELD_ENABLE, 1);
	OUT_RING(ring, 0x01);

	/* Yield the floor on command completion */
	OUT_PKT7(ring, CP_CONTEXT_SWITCH_YIELD, 4);
	OUT_RING(ring, 0x00);
	OUT_RING(ring, 0x00);
	OUT_RING(ring, 0x01);
	OUT_RING(ring, 0x01);

	gpu->funcs->flush(gpu, ring);

	return a5xx_idle(gpu, ring) ? 0 : -EINVAL;
}


static struct drm_gem_object *a5xx_ucode_load_bo(struct msm_gpu *gpu,
		const struct firmware *fw, u64 *iova)
{
	struct drm_device *drm = gpu->dev;
	struct drm_gem_object *bo;
	void *ptr;

	mutex_lock(&drm->struct_mutex);
	bo = msm_gem_new(drm, fw->size - 4,
		MSM_BO_UNCACHED | MSM_BO_GPU_READONLY);
	mutex_unlock(&drm->struct_mutex);

	if (IS_ERR(bo))
		return bo;

	ptr = msm_gem_vaddr(bo);
	if (!ptr) {
		drm_gem_object_unreference_unlocked(bo);
		return ERR_PTR(-ENOMEM);
	}

	if (iova) {
		int ret = msm_gem_get_iova(bo, gpu->aspace, iova);

		if (ret) {
			drm_gem_object_unreference_unlocked(bo);
			return ERR_PTR(ret);
		}
	}

	memcpy(ptr, &fw->data[4], fw->size - 4);
	return bo;
}

static int a5xx_ucode_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	int ret;

	if (!a5xx_gpu->pm4_bo) {
		a5xx_gpu->pm4_bo = a5xx_ucode_load_bo(gpu, adreno_gpu->pm4,
			&a5xx_gpu->pm4_iova);

		if (IS_ERR(a5xx_gpu->pm4_bo)) {
			ret = PTR_ERR(a5xx_gpu->pm4_bo);
			a5xx_gpu->pm4_bo = NULL;
			dev_err(gpu->dev->dev, "could not allocate PM4: %d\n",
				ret);
			return ret;
		}
	}

	if (!a5xx_gpu->pfp_bo) {
		a5xx_gpu->pfp_bo = a5xx_ucode_load_bo(gpu, adreno_gpu->pfp,
			&a5xx_gpu->pfp_iova);

		if (IS_ERR(a5xx_gpu->pfp_bo)) {
			ret = PTR_ERR(a5xx_gpu->pfp_bo);
			a5xx_gpu->pfp_bo = NULL;
			dev_err(gpu->dev->dev, "could not allocate PFP: %d\n",
				ret);
			return ret;
		}
	}

	gpu_write64(gpu, REG_A5XX_CP_ME_INSTR_BASE_LO,
		REG_A5XX_CP_ME_INSTR_BASE_HI, a5xx_gpu->pm4_iova);

	gpu_write64(gpu, REG_A5XX_CP_PFP_INSTR_BASE_LO,
		REG_A5XX_CP_PFP_INSTR_BASE_HI, a5xx_gpu->pfp_iova);

	return 0;
}

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/scm.h>

static void a5xx_zap_shader_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	const char *name;
	void *ptr;

	/* If no zap shader was defined, we'll assume that none is needed */
	if (of_property_read_string(GPU_OF_NODE(gpu), "qcom,zap-shader", &name))
		return;

	/*
	 * If the zap shader has already been loaded then just ask the SCM to
	 * re-initialize the registers (not needed if CPZ retention is a thing)
	 */
	if (test_bit(A5XX_ZAP_SHADER_LOADED, &a5xx_gpu->flags)) {
		int ret;
		struct scm_desc desc = { 0 };

		if (of_property_read_bool(GPU_OF_NODE(gpu),
			"qcom,cpz-retention"))
			return;

		desc.args[0] = 0;
		desc.args[1] = 13;
		desc.arginfo = SCM_ARGS(2);

		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, 0x0A), &desc);
		if (ret)
			DRM_ERROR(
				"%s: zap-shader resume failed with error %d\n",
				gpu->name, ret);

		return;
	}

	ptr = subsystem_get(name);

	if (IS_ERR_OR_NULL(ptr)) {
		DRM_ERROR("%s: Unable to load the zap shader: %ld\n", gpu->name,
			IS_ERR(ptr) ? PTR_ERR(ptr) : -ENODEV);
	} else {
		set_bit(A5XX_ZAP_SHADER_LOADED, &a5xx_gpu->flags);
	}
}
#else
static void a5xx_zap_shader_init(struct msm_gpu *gpu)
{
	if (of_find_property(GPU_OF_NODE(gpu), "qcom,zap-shader", NULL))
		return;

	DRM_INFO_ONCE("%s: Zap shader is defined but loader isn't available\n",
		gpu->name);
}
#endif

#define A5XX_INT_MASK (A5XX_RBBM_INT_0_MASK_RBBM_AHB_ERROR | \
	  A5XX_RBBM_INT_0_MASK_RBBM_TRANSFER_TIMEOUT | \
	  A5XX_RBBM_INT_0_MASK_RBBM_ME_MS_TIMEOUT | \
	  A5XX_RBBM_INT_0_MASK_RBBM_PFP_MS_TIMEOUT | \
	  A5XX_RBBM_INT_0_MASK_RBBM_ETS_MS_TIMEOUT | \
	  A5XX_RBBM_INT_0_MASK_RBBM_ATB_ASYNC_OVERFLOW | \
	  A5XX_RBBM_INT_0_MASK_CP_HW_ERROR | \
	  A5XX_RBBM_INT_0_MASK_MISC_HANG_DETECT | \
	  A5XX_RBBM_INT_0_MASK_CP_SW | \
	  A5XX_RBBM_INT_0_MASK_CP_CACHE_FLUSH_TS | \
	  A5XX_RBBM_INT_0_MASK_UCHE_OOB_ACCESS | \
	  A5XX_RBBM_INT_0_MASK_GPMU_VOLTAGE_DROOP)

static int a5xx_hw_init(struct msm_gpu *gpu)
{
	struct msm_drm_private *priv = gpu->dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	int ret, bit = 0;

	pm_qos_update_request(&gpu->pm_qos_req_dma, 101);

	gpu_write(gpu, REG_A5XX_VBIF_ROUND_ROBIN_QOS_ARB, 0x00000003);
	if (adreno_is_a540(adreno_gpu))
		gpu_write(gpu, REG_A5XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000009);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	gpu_write(gpu, REG_A5XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/* Enable RBBM error reporting bits */
	gpu_write(gpu, REG_A5XX_RBBM_AHB_CNTL0, 0x00000001);

	if (adreno_gpu->quirks & ADRENO_QUIRK_FAULT_DETECT_MASK) {
		/*
		 * Mask out the activity signals from RB1-3 to avoid false
		 * positives
		 */

		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL11,
			0xF0000000);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL12,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL13,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL14,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL15,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL16,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL17,
			0xFFFFFFFF);
		gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_MASK_CNTL18,
			0xFFFFFFFF);
	}

	/* Enable fault detection */
	gpu_write(gpu, REG_A5XX_RBBM_INTERFACE_HANG_INT_CNTL,
		(1 << 30) | 0xFFFF);

	/* Turn on performance counters */
	gpu_write(gpu, REG_A5XX_RBBM_PERFCTR_CNTL, 0x01);

	/* Increase VFD cache access so LRZ and other data gets evicted less */
	gpu_write(gpu, REG_A5XX_UCHE_CACHE_WAYS, 0x02);

	/* Disable L2 bypass in the UCHE */
	gpu_write(gpu, REG_A5XX_UCHE_TRAP_BASE_LO, 0xFFFF0000);
	gpu_write(gpu, REG_A5XX_UCHE_TRAP_BASE_HI, 0x0001FFFF);
	gpu_write(gpu, REG_A5XX_UCHE_WRITE_THRU_BASE_LO, 0xFFFF0000);
	gpu_write(gpu, REG_A5XX_UCHE_WRITE_THRU_BASE_HI, 0x0001FFFF);

	/* Set the GMEM VA range [0x100000:0x100000 + gpu->gmem - 1] */
	gpu_write64(gpu, REG_A5XX_UCHE_GMEM_RANGE_MIN_LO,
		REG_A5XX_UCHE_GMEM_RANGE_MIN_LO, 0x00100000);

	gpu_write64(gpu, REG_A5XX_UCHE_GMEM_RANGE_MAX_LO,
		REG_A5XX_UCHE_GMEM_RANGE_MAX_HI,
		0x00100000 + adreno_gpu->gmem - 1);

	gpu_write(gpu, REG_A5XX_CP_MEQ_THRESHOLDS, 0x40);
	gpu_write(gpu, REG_A5XX_CP_MERCIU_SIZE, 0x40);
	gpu_write(gpu, REG_A5XX_CP_ROQ_THRESHOLDS_2, 0x80000060);
	gpu_write(gpu, REG_A5XX_CP_ROQ_THRESHOLDS_1, 0x40201B16);

	gpu_write(gpu, REG_A5XX_PC_DBG_ECO_CNTL, (0x400 << 11 | 0x300 << 22));

	if (adreno_gpu->quirks & ADRENO_QUIRK_TWO_PASS_USE_WFI)
		gpu_rmw(gpu, REG_A5XX_PC_DBG_ECO_CNTL, 0, (1 << 8));

	gpu_write(gpu, REG_A5XX_PC_DBG_ECO_CNTL, 0xc0200100);

	/* Enable USE_RETENTION_FLOPS */
	gpu_write(gpu, REG_A5XX_CP_CHICKEN_DBG, 0x02000000);

	/* Enable ME/PFP split notification */
	gpu_write(gpu, REG_A5XX_RBBM_AHB_CNTL1, 0xA6FFFFFF);

	/* Enable HWCG */
	a5xx_set_hwcg(gpu, true);

	gpu_write(gpu, REG_A5XX_RBBM_AHB_CNTL2, 0x0000003F);

	/* Set the highest bank bit if specified in the device tree */
	if (!of_property_read_u32(pdev->dev.of_node, "qcom,highest-bank-bit",
		&bit)) {
		if (bit >= 13 && bit <= 16) {
			bit -= 13;

			gpu_write(gpu, REG_A5XX_TPL1_MODE_CNTL, bit << 7);
			gpu_write(gpu, REG_A5XX_RB_MODE_CNTL, bit << 1);

			if (adreno_is_a540(adreno_gpu))
				gpu_write(gpu, REG_A5XX_UCHE_DBG_ECO_CNTL_2,
					bit);
		}
	}

	/* Try to load and initialize the zap shader if applicable */
	a5xx_zap_shader_init(gpu);

	/* Protect registers from the CP */
	gpu_write(gpu, REG_A5XX_CP_PROTECT_CNTL, 0x00000007);

	/* RBBM */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(0), ADRENO_PROTECT_RW(0x04, 4));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(1), ADRENO_PROTECT_RW(0x08, 8));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(2), ADRENO_PROTECT_RW(0x10, 16));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(3), ADRENO_PROTECT_RW(0x20, 32));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(4), ADRENO_PROTECT_RW(0x40, 64));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(5), ADRENO_PROTECT_RW(0x80, 64));

	/* Content protect */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(6),
		ADRENO_PROTECT_RW(REG_A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
			16));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(7),
		ADRENO_PROTECT_RW(REG_A5XX_RBBM_SECVID_TRUST_CNTL, 2));

	/* CP */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(8), ADRENO_PROTECT_RW(0x800, 64));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(9), ADRENO_PROTECT_RW(0x840, 8));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(10), ADRENO_PROTECT_RW(0x880, 32));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(11), ADRENO_PROTECT_RW(0xAA0, 1));

	/* RB */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(12), ADRENO_PROTECT_RW(0xCC0, 1));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(13), ADRENO_PROTECT_RW(0xCF0, 2));

	/* VPC */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(14), ADRENO_PROTECT_RW(0xE68, 8));
	gpu_write(gpu, REG_A5XX_CP_PROTECT(15), ADRENO_PROTECT_RW(0xE70, 4));

	/* UCHE */
	gpu_write(gpu, REG_A5XX_CP_PROTECT(16), ADRENO_PROTECT_RW(0xE80, 16));

	if (adreno_is_a530(adreno_gpu))
		gpu_write(gpu, REG_A5XX_CP_PROTECT(17),
			ADRENO_PROTECT_RW(0x10000, 0x8000));

	gpu_write(gpu, REG_A5XX_RBBM_SECVID_TSB_CNTL, 0);
	/*
	 * Disable the trusted memory range - we don't actually supported secure
	 * memory rendering at this point in time and we don't want to block off
	 * part of the virtual memory space.
	 */
	gpu_write64(gpu, REG_A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
		REG_A5XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI, 0x00000000);
	gpu_write(gpu, REG_A5XX_RBBM_SECVID_TSB_TRUSTED_SIZE, 0x00000000);

	/* Put the GPU into 64 bit by default */
	gpu_write(gpu, REG_A5XX_CP_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_VSC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_GRAS_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_RB_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_PC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_HLSQ_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_VFD_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_VPC_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_UCHE_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_SP_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_TPL1_ADDR_MODE_CNTL, 0x1);
	gpu_write(gpu, REG_A5XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL, 0x1);

	/* Load the GPMU firmware before starting the HW init */
	a5xx_gpmu_ucode_init(gpu);

	ret = adreno_hw_init(gpu);
	if (ret)
		return ret;

	a5xx_preempt_hw_init(gpu);

	ret = a5xx_ucode_init(gpu);
	if (ret)
		return ret;

	/* Disable the interrupts through the initial bringup stage */
	gpu_write(gpu, REG_A5XX_RBBM_INT_0_MASK, A5XX_INT_MASK);

	/* Clear ME_HALT to start the micro engine */
	gpu_write(gpu, REG_A5XX_CP_PFP_ME_CNTL, 0);
	ret = a5xx_me_init(gpu);
	if (ret)
		return ret;

	/*
	 * Send a pipeline event stat to get misbehaving counters to start
	 * ticking correctly
	 */
	if (adreno_is_a530(adreno_gpu)) {
		OUT_PKT7(gpu->rb[0], CP_EVENT_WRITE, 1);
		OUT_RING(gpu->rb[0], 0x0F);

		gpu->funcs->flush(gpu, gpu->rb[0]);
		if (!a5xx_idle(gpu, gpu->rb[0]))
			return -EINVAL;
	}

	/*
	 * If a zap shader was specified in the device tree, assume that we are
	 * on a secure device that blocks access to the RBBM_SECVID registers
	 * so we need to use the CP to switch out of secure mode. If a zap
	 * shader was NOT specified then we assume we are on an unlocked device.
	 * If we guessed wrong then the access to the register will probably
	 * cause a XPU violation.
	 */
	if (test_bit(A5XX_ZAP_SHADER_LOADED, &a5xx_gpu->flags)) {
		struct msm_ringbuffer *ring = gpu->rb[0];

		OUT_PKT7(ring, CP_SET_SECURE_MODE, 1);
		OUT_RING(ring, 0x00000000);

		gpu->funcs->flush(gpu, gpu->rb[0]);
		if (!a5xx_idle(gpu, gpu->rb[0]))
			return -EINVAL;
	} else {
		/* Print a warning so if we die, we know why */
		dev_warn_once(gpu->dev->dev,
			"Zap shader not enabled - using SECVID_TRUST_CNTL instead\n");
		gpu_write(gpu, REG_A5XX_RBBM_SECVID_TRUST_CNTL, 0x0);
	}

	/* Next, start the power */
	ret = a5xx_power_init(gpu);
	if (ret)
		return ret;


	/* Last step - yield the ringbuffer */
	a5xx_preempt_start(gpu);

	pm_qos_update_request(&gpu->pm_qos_req_dma, 501);

	return 0;
}

static void a5xx_recover(struct msm_gpu *gpu)
{
	adreno_dump_info(gpu);

	msm_gpu_snapshot(gpu, gpu->snapshot);

	/* Reset the GPU so it can work again */
	gpu_write(gpu, REG_A5XX_RBBM_SW_RESET_CMD, 1);
	gpu_read(gpu, REG_A5XX_RBBM_SW_RESET_CMD);
	gpu_write(gpu, REG_A5XX_RBBM_SW_RESET_CMD, 0);

	adreno_recover(gpu);
}

static void a5xx_destroy(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);

	DBG("%s", gpu->name);

	a5xx_preempt_fini(gpu);

	if (a5xx_gpu->pm4_bo) {
		if (a5xx_gpu->pm4_iova)
			msm_gem_put_iova(a5xx_gpu->pm4_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(a5xx_gpu->pm4_bo);
	}

	if (a5xx_gpu->pfp_bo) {
		if (a5xx_gpu->pfp_iova)
			msm_gem_put_iova(a5xx_gpu->pfp_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(a5xx_gpu->pfp_bo);
	}

	if (a5xx_gpu->gpmu_bo) {
		if (a5xx_gpu->gpmu_iova)
			msm_gem_put_iova(a5xx_gpu->gpmu_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(a5xx_gpu->gpmu_bo);
	}

	adreno_gpu_cleanup(adreno_gpu);
	kfree(a5xx_gpu);
}

static inline bool _a5xx_check_idle(struct msm_gpu *gpu)
{
	if (gpu_read(gpu, REG_A5XX_RBBM_STATUS) & ~A5XX_RBBM_STATUS_HI_BUSY)
		return false;

	/*
	 * Nearly every abnormality ends up pausing the GPU and triggering a
	 * fault so we can safely just watch for this one interrupt to fire
	 */
	return !(gpu_read(gpu, REG_A5XX_RBBM_INT_0_STATUS) &
		A5XX_RBBM_INT_0_MASK_MISC_HANG_DETECT);
}

bool a5xx_idle(struct msm_gpu *gpu, struct msm_ringbuffer *ring)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);

	if (ring != a5xx_gpu->cur_ring) {
		WARN(1, "Tried to idle a non-current ringbuffer\n");
		return false;
	}

	/* wait for CP to drain ringbuffer: */
	if (!adreno_idle(gpu, ring))
		return false;

	if (spin_until(_a5xx_check_idle(gpu))) {
		DRM_ERROR(
			"%s: timeout waiting for GPU RB %d to idle: status %8.8X rptr/wptr: %4.4X/%4.4X irq %8.8X\n",
			gpu->name, ring->id,
			gpu_read(gpu, REG_A5XX_RBBM_STATUS),
			gpu_read(gpu, REG_A5XX_CP_RB_RPTR),
			gpu_read(gpu, REG_A5XX_CP_RB_WPTR),
			gpu_read(gpu, REG_A5XX_RBBM_INT_0_STATUS));

		return false;
	}

	return true;
}

static void a5xx_cp_err_irq(struct msm_gpu *gpu)
{
	u32 status = gpu_read(gpu, REG_A5XX_CP_INTERRUPT_STATUS);

	if (status & A5XX_CP_INT_CP_OPCODE_ERROR) {
		u32 val;

		gpu_write(gpu, REG_A5XX_CP_PFP_STAT_ADDR, 0);

		/*
		 * REG_A5XX_CP_PFP_STAT_DATA is indexed, and we want index 1 so
		 * read it twice
		 */

		gpu_read(gpu, REG_A5XX_CP_PFP_STAT_DATA);
		val = gpu_read(gpu, REG_A5XX_CP_PFP_STAT_DATA);

		dev_err_ratelimited(gpu->dev->dev, "CP | opcode error | possible opcode=0x%8.8X\n",
			val);
	}

	if (status & A5XX_CP_INT_CP_HW_FAULT_ERROR)
		dev_err_ratelimited(gpu->dev->dev, "CP | HW fault | status=0x%8.8X\n",
			gpu_read(gpu, REG_A5XX_CP_HW_FAULT));

	if (status & A5XX_CP_INT_CP_DMA_ERROR)
		dev_err_ratelimited(gpu->dev->dev, "CP | DMA error\n");

	if (status & A5XX_CP_INT_CP_REGISTER_PROTECTION_ERROR) {
		u32 val = gpu_read(gpu, REG_A5XX_CP_PROTECT_STATUS);

		dev_err_ratelimited(gpu->dev->dev,
			"CP | protected mode error | %s | addr=0x%8.8X | status=0x%8.8X\n",
			val & (1 << 24) ? "WRITE" : "READ",
			(val & 0xFFFFF) >> 2, val);
	}

	if (status & A5XX_CP_INT_CP_AHB_ERROR) {
		u32 status = gpu_read(gpu, REG_A5XX_CP_AHB_FAULT);
		const char *access[16] = { "reserved", "reserved",
			"timestamp lo", "timestamp hi", "pfp read", "pfp write",
			"", "", "me read", "me write", "", "", "crashdump read",
			"crashdump write" };

		dev_err_ratelimited(gpu->dev->dev,
			"CP | AHB error | addr=%X access=%s error=%d | status=0x%8.8X\n",
			status & 0xFFFFF, access[(status >> 24) & 0xF],
			(status & (1 << 31)), status);
	}
}

static void a5xx_rbbm_err_irq(struct msm_gpu *gpu, u32 status)
{
	if (status & A5XX_RBBM_INT_0_MASK_RBBM_AHB_ERROR) {
		u32 val = gpu_read(gpu, REG_A5XX_RBBM_AHB_ERROR_STATUS);

		dev_err_ratelimited(gpu->dev->dev,
			"RBBM | AHB bus error | %s | addr=0x%X | ports=0x%X:0x%X\n",
			val & (1 << 28) ? "WRITE" : "READ",
			(val & 0xFFFFF) >> 2, (val >> 20) & 0x3,
			(val >> 24) & 0xF);

		/* Clear the error */
		gpu_write(gpu, REG_A5XX_RBBM_AHB_CMD, (1 << 4));

		/* Clear the interrupt */
		gpu_write(gpu, REG_A5XX_RBBM_INT_CLEAR_CMD,
			A5XX_RBBM_INT_0_MASK_RBBM_AHB_ERROR);
	}

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_TRANSFER_TIMEOUT)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | AHB transfer timeout\n");

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_ME_MS_TIMEOUT)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | ME master split | status=0x%X\n",
			gpu_read(gpu, REG_A5XX_RBBM_AHB_ME_SPLIT_STATUS));

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_PFP_MS_TIMEOUT)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | PFP master split | status=0x%X\n",
			gpu_read(gpu, REG_A5XX_RBBM_AHB_PFP_SPLIT_STATUS));

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_ETS_MS_TIMEOUT)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | ETS master split | status=0x%X\n",
			gpu_read(gpu, REG_A5XX_RBBM_AHB_ETS_SPLIT_STATUS));

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_ATB_ASYNC_OVERFLOW)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | ATB ASYNC overflow\n");

	if (status & A5XX_RBBM_INT_0_MASK_RBBM_ATB_BUS_OVERFLOW)
		dev_err_ratelimited(gpu->dev->dev, "RBBM | ATB bus overflow\n");
}

static void a5xx_uche_err_irq(struct msm_gpu *gpu)
{
	uint64_t addr = (uint64_t) gpu_read(gpu, REG_A5XX_UCHE_TRAP_LOG_HI);

	addr |= gpu_read(gpu, REG_A5XX_UCHE_TRAP_LOG_LO);

	dev_err_ratelimited(gpu->dev->dev, "UCHE | Out of bounds access | addr=0x%llX\n",
		addr);
}

static void a5xx_gpmu_err_irq(struct msm_gpu *gpu)
{
	dev_err_ratelimited(gpu->dev->dev, "GPMU | voltage droop\n");
}

static void a5xx_fault_detect_irq(struct msm_gpu *gpu)
{
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_ringbuffer *ring = gpu->funcs->active_ring(gpu);

	dev_err(dev->dev, "gpu fault ring %d fence %x status %8.8X rb %4.4x/%4.4x ib1 %16.16llX/%4.4x ib2 %16.16llX/%4.4x\n",
		ring ? ring->id : -1, adreno_submitted_fence(gpu, ring),
		gpu_read(gpu, REG_A5XX_RBBM_STATUS),
		gpu_read(gpu, REG_A5XX_CP_RB_RPTR),
		gpu_read(gpu, REG_A5XX_CP_RB_WPTR),
		gpu_read64(gpu, REG_A5XX_CP_IB1_BASE, REG_A5XX_CP_IB1_BASE_HI),
		gpu_read(gpu, REG_A5XX_CP_IB1_BUFSZ),
		gpu_read64(gpu, REG_A5XX_CP_IB2_BASE, REG_A5XX_CP_IB2_BASE_HI),
		gpu_read(gpu, REG_A5XX_CP_IB2_BUFSZ));

	/* Turn off the hangcheck timer to keep it from bothering us */
	del_timer(&gpu->hangcheck_timer);

	queue_work(priv->wq, &gpu->recover_work);
}

#define RBBM_ERROR_MASK \
	(A5XX_RBBM_INT_0_MASK_RBBM_AHB_ERROR | \
	A5XX_RBBM_INT_0_MASK_RBBM_TRANSFER_TIMEOUT | \
	A5XX_RBBM_INT_0_MASK_RBBM_ME_MS_TIMEOUT | \
	A5XX_RBBM_INT_0_MASK_RBBM_PFP_MS_TIMEOUT | \
	A5XX_RBBM_INT_0_MASK_RBBM_ETS_MS_TIMEOUT | \
	A5XX_RBBM_INT_0_MASK_RBBM_ATB_ASYNC_OVERFLOW)

static irqreturn_t a5xx_irq(struct msm_gpu *gpu)
{
	u32 status = gpu_read(gpu, REG_A5XX_RBBM_INT_0_STATUS);

	/*
	 * Clear all the interrupts except for RBBM_AHB_ERROR
	 * which needs to be cleared after the error condition
	 * is cleared otherwise it will storm
	 */
	gpu_write(gpu, REG_A5XX_RBBM_INT_CLEAR_CMD,
			status & ~A5XX_RBBM_INT_0_MASK_RBBM_AHB_ERROR);

	if (status & RBBM_ERROR_MASK)
		a5xx_rbbm_err_irq(gpu, status);

	if (status & A5XX_RBBM_INT_0_MASK_CP_HW_ERROR)
		a5xx_cp_err_irq(gpu);

	if (status & A5XX_RBBM_INT_0_MASK_MISC_HANG_DETECT)
		a5xx_fault_detect_irq(gpu);

	if (status & A5XX_RBBM_INT_0_MASK_UCHE_OOB_ACCESS)
		a5xx_uche_err_irq(gpu);

	if (status & A5XX_RBBM_INT_0_MASK_GPMU_VOLTAGE_DROOP)
		a5xx_gpmu_err_irq(gpu);

	if (status & A5XX_RBBM_INT_0_MASK_CP_CACHE_FLUSH_TS)
		msm_gpu_retire(gpu);

	if (status & A5XX_RBBM_INT_0_MASK_CP_SW)
		a5xx_preempt_irq(gpu);

	return IRQ_HANDLED;
}

static const u32 a5xx_register_offsets[REG_ADRENO_REGISTER_MAX] = {
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_BASE, REG_A5XX_CP_RB_BASE),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_BASE_HI, REG_A5XX_CP_RB_BASE_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_RPTR_ADDR, REG_A5XX_CP_RB_RPTR_ADDR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_RPTR_ADDR_HI,
		REG_A5XX_CP_RB_RPTR_ADDR_HI),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_RPTR, REG_A5XX_CP_RB_RPTR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_WPTR, REG_A5XX_CP_RB_WPTR),
	REG_ADRENO_DEFINE(REG_ADRENO_CP_RB_CNTL, REG_A5XX_CP_RB_CNTL),
};

static const u32 a5xx_registers[] = {
	0x0000, 0x0002, 0x0004, 0x0020, 0x0022, 0x0026, 0x0029, 0x002b,
	0x002e, 0x0035, 0x0038, 0x0042, 0x0044, 0x0044, 0x0047, 0x0095,
	0x0097, 0x00bb, 0x03a0, 0x0464, 0x0469, 0x046f, 0x04d2, 0x04d3,
	0x04e0, 0x0533, 0x0540, 0x0555, 0x0800, 0x081a, 0x081f, 0x0841,
	0x0860, 0x0860, 0x0880, 0x08a0, 0x0b00, 0x0b12, 0x0b14, 0x0b28,
	0x0b78, 0x0b7f, 0x0bb0, 0x0bbd, 0x0bc0, 0x0bc6, 0x0bd0, 0x0c53,
	0x0c60, 0x0c61, 0x0c80, 0x0c82, 0x0c84, 0x0c85, 0x0c90, 0x0c9b,
	0x0ca0, 0x0ca0, 0x0cb0, 0x0cb2, 0x0cc1, 0x0cc1, 0x0cc4, 0x0cc7,
	0x0ccc, 0x0ccc, 0x0cd0, 0x0cdb, 0x0ce0, 0x0ce5, 0x0ce8, 0x0ce8,
	0x0cec, 0x0cf1, 0x0cfb, 0x0d0e, 0x0d10, 0x0d17, 0x0d20, 0x0d23,
	0x0d30, 0x0d30, 0x0e40, 0x0e43, 0x0e4a, 0x0e4a, 0x0e50, 0x0e57,
	0x0e60, 0x0e7c, 0x0e80, 0x0e8e, 0x0e90, 0x0e96, 0x0ea0, 0x0eab,
	0x0eb0, 0x0eb2, 0x2100, 0x211e, 0x2140, 0x2145, 0x2180, 0x2185,
	0x2500, 0x251e, 0x2540, 0x2545, 0x2580, 0x2585, 0x3000, 0x3014,
	0x3018, 0x302c, 0x3030, 0x3030, 0x3034, 0x3036, 0x303c, 0x303d,
	0x3040, 0x3040, 0x3042, 0x3042, 0x3049, 0x3049, 0x3058, 0x3058,
	0x305a, 0x3061, 0x3064, 0x3068, 0x306c, 0x306d, 0x3080, 0x3088,
	0x308b, 0x308c, 0x3090, 0x3094, 0x3098, 0x3098, 0x309c, 0x309c,
	0x3124, 0x3124, 0x340c, 0x340c, 0x3410, 0x3410, 0x3800, 0x3801,
	0xa800, 0xa800, 0xa820, 0xa828, 0xa840, 0xa87d, 0xa880, 0xa88d,
	0xa890, 0xa8a3, 0xa8a8, 0xa8aa, 0xa8c0, 0xa8c3, 0xa8c6, 0xa8ca,
	0xa8cc, 0xa8cf, 0xa8d1, 0xa8d8, 0xa8dc, 0xa8dc, 0xa8e0, 0xa8f5,
	0xac00, 0xac06, 0xac40, 0xac47, 0xac60, 0xac62, 0xac80, 0xac82,
	0xb800, 0xb808, 0xb80c, 0xb812, 0xb814, 0xb817, 0xb900, 0xb904,
	0xb906, 0xb90a, 0xb90c, 0xb90f, 0xb920, 0xb924, 0xb926, 0xb92a,
	0xb92c, 0xb92f, 0xb940, 0xb944, 0xb946, 0xb94a, 0xb94c, 0xb94f,
	0xb960, 0xb964, 0xb966, 0xb96a, 0xb96c, 0xb96f, 0xb980, 0xb984,
	0xb986, 0xb98a, 0xb98c, 0xb98f, 0xb9a0, 0xb9b0, 0xb9b8, 0xb9ba,
	0xd200, 0xd23f, 0xe000, 0xe006, 0xe010, 0xe09a, 0xe0a0, 0xe0a4,
	0xe0aa, 0xe0eb, 0xe100, 0xe105, 0xe140, 0xe147, 0xe150, 0xe187,
	0xe1a0, 0xe1a9, 0xe1b0, 0xe1b6, 0xe1c0, 0xe1c7, 0xe1d0, 0xe1d1,
	0xe200, 0xe201, 0xe210, 0xe21c, 0xe240, 0xe268, 0xe280, 0xe280,
	0xe282, 0xe2a3, 0xe2a5, 0xe2c2, 0xe380, 0xe38f, 0xe3b0, 0xe3b0,
	0xe400, 0xe405, 0xe408, 0xe4e9, 0xe4f0, 0xe4f0, 0xe800, 0xe806,
	0xe810, 0xe89a, 0xe8a0, 0xe8a4, 0xe8aa, 0xe8eb, 0xe900, 0xe905,
	0xe940, 0xe947, 0xe950, 0xe987, 0xe9a0, 0xe9a9, 0xe9b0, 0xe9b6,
	0xe9c0, 0xe9c7, 0xe9d0, 0xe9d1, 0xea00, 0xea01, 0xea10, 0xea1c,
	0xea40, 0xea68, 0xea80, 0xea80, 0xea82, 0xeaa3, 0xeaa5, 0xeac2,
	0xeb80, 0xeb8f, 0xebb0, 0xebb0, 0xec00, 0xec05, 0xec08, 0xece9,
	0xecf0, 0xecf0, 0xf400, 0xf400, 0xf800, 0xf807,
	~0
};

static int a5xx_pm_resume(struct msm_gpu *gpu)
{
	int ret;

	/* Turn on the core power */
	ret = msm_gpu_pm_resume(gpu);
	if (ret)
		return ret;

	/* Turn the RBCCU domain first to limit the chances of voltage droop */
	gpu_write(gpu, REG_A5XX_GPMU_RBCCU_POWER_CNTL, 0x778000);

	/* Wait 3 usecs before polling */
	udelay(3);

	ret = spin_usecs(gpu, 20, REG_A5XX_GPMU_RBCCU_PWR_CLK_STATUS,
		(1 << 20), (1 << 20));
	if (ret) {
		DRM_ERROR("%s: timeout waiting for RBCCU GDSC enable: %X\n",
			gpu->name,
			gpu_read(gpu, REG_A5XX_GPMU_RBCCU_PWR_CLK_STATUS));
		return ret;
	}

	/* Turn on the SP domain */
	gpu_write(gpu, REG_A5XX_GPMU_SP_POWER_CNTL, 0x778000);
	ret = spin_usecs(gpu, 20, REG_A5XX_GPMU_SP_PWR_CLK_STATUS,
		(1 << 20), (1 << 20));
	if (ret)
		DRM_ERROR("%s: timeout waiting for SP GDSC enable\n",
			gpu->name);

	return ret;
}

static int a5xx_pm_suspend(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);

	/* Clear the VBIF pipe before shutting down */

	gpu_write(gpu, REG_A5XX_VBIF_XIN_HALT_CTRL0, 0xF);
	spin_until((gpu_read(gpu, REG_A5XX_VBIF_XIN_HALT_CTRL1) & 0xF) == 0xF);

	gpu_write(gpu, REG_A5XX_VBIF_XIN_HALT_CTRL0, 0);

	/*
	 * Reset the VBIF before power collapse to avoid issue with FIFO
	 * entries
	 */

	if (adreno_is_a530(adreno_gpu)) {
		/* These only need to be done for A530 */
		gpu_write(gpu, REG_A5XX_RBBM_BLOCK_SW_RESET_CMD, 0x003C0000);
		gpu_write(gpu, REG_A5XX_RBBM_BLOCK_SW_RESET_CMD, 0x00000000);
	}

	return msm_gpu_pm_suspend(gpu);
}

static int a5xx_get_timestamp(struct msm_gpu *gpu, uint64_t *value)
{
	*value = gpu_read64(gpu, REG_A5XX_RBBM_PERFCTR_CP_0_LO,
		REG_A5XX_RBBM_PERFCTR_CP_0_HI);

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void a5xx_show(struct msm_gpu *gpu, struct seq_file *m)
{
	gpu->funcs->pm_resume(gpu);

	seq_printf(m, "status:   %08x\n",
			gpu_read(gpu, REG_A5XX_RBBM_STATUS));
	gpu->funcs->pm_suspend(gpu);

	adreno_show(gpu, m);
}
#endif

static struct msm_ringbuffer *a5xx_active_ring(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);

	return a5xx_gpu->cur_ring;
}

static const struct adreno_gpu_funcs funcs = {
	.base = {
		.get_param = adreno_get_param,
		.hw_init = a5xx_hw_init,
		.pm_suspend = a5xx_pm_suspend,
		.pm_resume = a5xx_pm_resume,
		.recover = a5xx_recover,
		.last_fence = adreno_last_fence,
		.submitted_fence = adreno_submitted_fence,
		.submit = a5xx_submit,
		.flush = a5xx_flush,
		.active_ring = a5xx_active_ring,
		.irq = a5xx_irq,
		.destroy = a5xx_destroy,
#ifdef CONFIG_DEBUG_FS
		.show = a5xx_show,
#endif
		.snapshot = a5xx_snapshot,
	},
	.get_timestamp = a5xx_get_timestamp,
};

/* Read the limits management leakage from the efuses */
static void a530_efuse_leakage(struct platform_device *pdev,
		struct adreno_gpu *adreno_gpu, void *base,
		size_t size)
{
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	unsigned int row0, row2;
	unsigned int leakage_pwr_on, coeff;

	if (size < 0x148)
		return;

	/* Leakage */
	row0 = readl_relaxed(base + 0x134);
	row2 = readl_relaxed(base + 0x144);

	/* Read barrier to get the previous two reads */
	rmb();

	/* Get the leakage coefficient from device tree */
	if (of_property_read_u32(pdev->dev.of_node,
		"qcom,base-leakage-coefficent", &coeff))
		return;

	leakage_pwr_on = ((row2 >> 2) & 0xFF) * (1 << (row0 >> 1) & 0x03);
	a5xx_gpu->lm_leakage = (leakage_pwr_on << 16) |
		((leakage_pwr_on * coeff) / 100);
}

/* Read the speed bin from the efuses */
static void a530_efuse_bin(struct platform_device *pdev,
		struct adreno_gpu *adreno_gpu, void *base,
		size_t size)
{
	uint32_t speed_bin[3];
	uint32_t val;

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,gpu-speed-bin", speed_bin, 3))
		return;

	if (size < speed_bin[0] + 4)
		return;

	val = readl_relaxed(base + speed_bin[0]);

	adreno_gpu->speed_bin = (val & speed_bin[1]) >> speed_bin[2];
}

/* Read target specific configuration from the efuses */
static void a5xx_efuses_read(struct platform_device *pdev,
		struct adreno_gpu *adreno_gpu)
{
	struct adreno_platform_config *config = pdev->dev.platform_data;
	const struct adreno_info *info = adreno_info(config->rev);
	struct resource *res;
	void *base;

	/*
	 * The adreno_gpu->revn mechanism isn't set up yet so we need to check
	 * it directly here
	 */
	if (info->revn != 530)
		return;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"qfprom_memory");
	if (!res)
		return;

	base = ioremap(res->start, resource_size(res));
	if (!base)
		return;

	a530_efuse_bin(pdev, adreno_gpu, base, resource_size(res));
	a530_efuse_leakage(pdev, adreno_gpu, base, resource_size(res));

	iounmap(base);
}

struct msm_gpu *a5xx_gpu_init(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	struct a5xx_gpu *a5xx_gpu = NULL;
	struct adreno_gpu *adreno_gpu;
	struct msm_gpu *gpu;
	int ret;

	if (!pdev) {
		dev_err(dev->dev, "No A5XX device is defined\n");
		return ERR_PTR(-ENXIO);
	}

	a5xx_gpu = kzalloc(sizeof(*a5xx_gpu), GFP_KERNEL);
	if (!a5xx_gpu)
		return ERR_PTR(-ENOMEM);

	adreno_gpu = &a5xx_gpu->base;
	gpu = &adreno_gpu->base;

	a5xx_gpu->pdev = pdev;
	adreno_gpu->registers = a5xx_registers;
	adreno_gpu->reg_offsets = a5xx_register_offsets;

	a5xx_gpu->lm_leakage = 0x4E001A;

	/* Check the efuses for some configuration */
	a5xx_efuses_read(pdev, adreno_gpu);

	ret = adreno_gpu_init(dev, pdev, adreno_gpu, &funcs, 4);
	if (ret) {
		a5xx_destroy(&(a5xx_gpu->base.base));
		return ERR_PTR(ret);
	}

	/* Set up the preemption specific bits and pieces for each ringbuffer */
	a5xx_preempt_init(gpu);

	return gpu;
}
