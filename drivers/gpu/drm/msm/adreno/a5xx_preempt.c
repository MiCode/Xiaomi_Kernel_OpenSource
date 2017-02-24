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

static void *alloc_kernel_bo(struct drm_device *drm, struct msm_gpu *gpu,
		size_t size, uint32_t flags, struct drm_gem_object **bo,
		u64 *iova)
{
	struct drm_gem_object *_bo;
	u64 _iova;
	void *ptr;
	int ret;

	mutex_lock(&drm->struct_mutex);
	_bo = msm_gem_new(drm, size, flags);
	mutex_unlock(&drm->struct_mutex);

	if (IS_ERR(_bo))
		return _bo;

	ret = msm_gem_get_iova(_bo, gpu->aspace, &_iova);
	if (ret)
		goto out;

	ptr = msm_gem_vaddr(_bo);
	if (!ptr) {
		ret = -ENOMEM;
		goto out;
	}

	if (bo)
		*bo = _bo;
	if (iova)
		*iova = _iova;

	return ptr;
out:
	drm_gem_object_unreference_unlocked(_bo);
	return ERR_PTR(ret);
}

/*
 * Try to transition the preemption state from old to new. Return
 * true on success or false if the original state wasn't 'old'
 */
static inline bool try_preempt_state(struct a5xx_gpu *a5xx_gpu,
		enum preempt_state old, enum preempt_state new)
{
	enum preempt_state cur = atomic_cmpxchg(&a5xx_gpu->preempt_state,
		old, new);

	return (cur == old);
}

/*
 * Force the preemption state to the specified state.  This is used in cases
 * where the current state is known and won't change
 */
static inline void set_preempt_state(struct a5xx_gpu *gpu,
		enum preempt_state new)
{
	/*
	 * preempt_state may be read by other cores trying to trigger a
	 * preemption or in the interrupt handler so barriers are needed
	 * before...
	 */
	smp_mb__before_atomic();
	atomic_set(&gpu->preempt_state, new);
	/* ... and after*/
	smp_mb__after_atomic();
}

/* Write the most recent wptr for the given ring into the hardware */
static inline void update_wptr(struct msm_gpu *gpu, struct msm_ringbuffer *ring)
{
	unsigned long flags;
	uint32_t wptr;

	if (!ring)
		return;

	spin_lock_irqsave(&ring->lock, flags);
	wptr = get_wptr(ring);
	spin_unlock_irqrestore(&ring->lock, flags);

	gpu_write(gpu, REG_A5XX_CP_RB_WPTR, wptr);
}

/* Return the highest priority ringbuffer with something in it */
static struct msm_ringbuffer *get_next_ring(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	unsigned long flags;
	int i;

	for (i = gpu->nr_rings - 1; i >= 0; i--) {
		bool empty;
		struct msm_ringbuffer *ring = gpu->rb[i];

		spin_lock_irqsave(&ring->lock, flags);
		empty = (get_wptr(ring) == adreno_gpu->memptrs->rptr[ring->id]);
		spin_unlock_irqrestore(&ring->lock, flags);

		if (!empty)
			return ring;
	}

	return NULL;
}

static void a5xx_preempt_timer(unsigned long data)
{
	struct a5xx_gpu *a5xx_gpu = (struct a5xx_gpu *) data;
	struct msm_gpu *gpu = &a5xx_gpu->base.base;
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;

	if (!try_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED, PREEMPT_FAULTED))
		return;

	dev_err(dev->dev, "%s: preemption timed out\n", gpu->name);
	queue_work(priv->wq, &gpu->recover_work);
}

/* Try to trigger a preemption switch */
void a5xx_preempt_trigger(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	unsigned long flags;
	struct msm_ringbuffer *ring;

	if (gpu->nr_rings == 1)
		return;

	/*
	 * Try to start preemption by moving from NONE to START. If
	 * unsuccessful, a preemption is already in flight
	 */
	if (!try_preempt_state(a5xx_gpu, PREEMPT_NONE, PREEMPT_START))
		return;

	/* Get the next ring to preempt to */
	ring = get_next_ring(gpu);

	/*
	 * If no ring is populated or the highest priority ring is the current
	 * one do nothing except to update the wptr to the latest and greatest
	 */
	if (!ring || (a5xx_gpu->cur_ring == ring)) {
		update_wptr(gpu, ring);

		/* Set the state back to NONE */
		set_preempt_state(a5xx_gpu, PREEMPT_NONE);
		return;
	}

	/* Make sure the wptr doesn't update while we're in motion */
	spin_lock_irqsave(&ring->lock, flags);
	a5xx_gpu->preempt[ring->id]->wptr = get_wptr(ring);
	spin_unlock_irqrestore(&ring->lock, flags);

	/* Do read barrier to make sure we have updated pagetable info */
	rmb();

	/* Set the SMMU info for the preemption */
	if (a5xx_gpu->smmu_info) {
		a5xx_gpu->smmu_info->ttbr0 =
			adreno_gpu->memptrs->ttbr0[ring->id];
		a5xx_gpu->smmu_info->contextidr =
			adreno_gpu->memptrs->contextidr[ring->id];
	}

	/* Set the address of the incoming preemption record */
	gpu_write64(gpu, REG_A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO,
		REG_A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_HI,
		a5xx_gpu->preempt_iova[ring->id]);

	a5xx_gpu->next_ring = ring;

	/* Start a timer to catch a stuck preemption */
	mod_timer(&a5xx_gpu->preempt_timer, jiffies + msecs_to_jiffies(10000));

	/* Set the preemption state to triggered */
	set_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED);

	/* Make sure everything is written before hitting the button */
	wmb();

	/* And actually start the preemption */
	gpu_write(gpu, REG_A5XX_CP_CONTEXT_SWITCH_CNTL, 1);
}

void a5xx_preempt_irq(struct msm_gpu *gpu)
{
	uint32_t status;
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;

	if (!try_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED, PREEMPT_PENDING))
		return;

	/* Delete the preemption watchdog timer */
	del_timer(&a5xx_gpu->preempt_timer);

	/*
	 * The hardware should be setting CP_CONTEXT_SWITCH_CNTL to zero before
	 * firing the interrupt, but there is a non zero chance of a hardware
	 * condition or a software race that could set it again before we have a
	 * chance to finish. If that happens, log and go for recovery
	 */
	status = gpu_read(gpu, REG_A5XX_CP_CONTEXT_SWITCH_CNTL);
	if (unlikely(status)) {
		set_preempt_state(a5xx_gpu, PREEMPT_FAULTED);
		dev_err(dev->dev, "%s: Preemption failed to complete\n",
			gpu->name);
		queue_work(priv->wq, &gpu->recover_work);
		return;
	}

	a5xx_gpu->cur_ring = a5xx_gpu->next_ring;
	a5xx_gpu->next_ring = NULL;

	update_wptr(gpu, a5xx_gpu->cur_ring);

	set_preempt_state(a5xx_gpu, PREEMPT_NONE);
}

void a5xx_preempt_hw_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring;
	int i;

	if (gpu->nr_rings > 1) {
		/* Clear the preemption records */
		FOR_EACH_RING(gpu, ring, i) {
			if (ring) {
				a5xx_gpu->preempt[ring->id]->wptr = 0;
				a5xx_gpu->preempt[ring->id]->rptr = 0;
				a5xx_gpu->preempt[ring->id]->rbase = ring->iova;
			}
		}
	}

	/* Tell the CP where to find the smmu_info buffer */
	gpu_write64(gpu, REG_A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
		REG_A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
		a5xx_gpu->smmu_info_iova);

	/* Reset the preemption state */
	set_preempt_state(a5xx_gpu, PREEMPT_NONE);

	/* Always come up on rb 0 */
	a5xx_gpu->cur_ring = gpu->rb[0];
}

static int preempt_init_ring(struct a5xx_gpu *a5xx_gpu,
		struct msm_ringbuffer *ring)
{
	struct adreno_gpu *adreno_gpu = &a5xx_gpu->base;
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct a5xx_preempt_record *ptr;
	struct drm_gem_object *bo;
	u64 iova;

	ptr = alloc_kernel_bo(gpu->dev, gpu,
		A5XX_PREEMPT_RECORD_SIZE + A5XX_PREEMPT_COUNTER_SIZE,
		MSM_BO_UNCACHED | MSM_BO_PRIVILEGED,
		&bo, &iova);

	if (IS_ERR(ptr))
		return PTR_ERR(ptr);

	a5xx_gpu->preempt_bo[ring->id] = bo;
	a5xx_gpu->preempt_iova[ring->id] = iova;
	a5xx_gpu->preempt[ring->id] = ptr;

	/* Set up the defaults on the preemption record */

	ptr->magic = A5XX_PREEMPT_RECORD_MAGIC;
	ptr->info = 0;
	ptr->data = 0;
	ptr->cntl = MSM_GPU_RB_CNTL_DEFAULT;
	ptr->rptr_addr = rbmemptr(adreno_gpu, ring->id, rptr);
	ptr->counter = iova + A5XX_PREEMPT_RECORD_SIZE;

	return 0;
}

void a5xx_preempt_fini(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring;
	int i;

	FOR_EACH_RING(gpu, ring, i) {
		if (!ring || !a5xx_gpu->preempt_bo[i])
			continue;

		if (a5xx_gpu->preempt_iova[i])
			msm_gem_put_iova(a5xx_gpu->preempt_bo[i], gpu->aspace);

		drm_gem_object_unreference_unlocked(a5xx_gpu->preempt_bo[i]);

		a5xx_gpu->preempt_bo[i] = NULL;
	}

	if (a5xx_gpu->smmu_info_bo) {
		if (a5xx_gpu->smmu_info_iova)
			msm_gem_put_iova(a5xx_gpu->smmu_info_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(a5xx_gpu->smmu_info_bo);
		a5xx_gpu->smmu_info_bo = NULL;
	}
}

void a5xx_preempt_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct msm_ringbuffer *ring;
	struct a5xx_smmu_info *ptr;
	struct drm_gem_object *bo;
	uint64_t iova;
	int i;

	/* No preemption if we only have one ring */
	if (gpu->nr_rings <= 1)
		return;

	FOR_EACH_RING(gpu, ring, i) {
		if (!ring)
			continue;

		if (preempt_init_ring(a5xx_gpu, ring))
			goto fail;
	}

	if (msm_iommu_allow_dynamic(gpu->aspace->mmu)) {
		ptr = alloc_kernel_bo(gpu->dev, gpu,
			sizeof(struct a5xx_smmu_info),
			MSM_BO_UNCACHED | MSM_BO_PRIVILEGED,
			&bo, &iova);

		if (IS_ERR(ptr))
			goto fail;

		ptr->magic = A5XX_SMMU_INFO_MAGIC;

		a5xx_gpu->smmu_info_bo = bo;
		a5xx_gpu->smmu_info_iova = iova;
		a5xx_gpu->smmu_info = ptr;
	}

	setup_timer(&a5xx_gpu->preempt_timer, a5xx_preempt_timer,
		(unsigned long) a5xx_gpu);

	return;
fail:
	/*
	 * On any failure our adventure is over. Clean up and
	 * set nr_rings to 1 to force preemption off
	 */
	a5xx_preempt_fini(gpu);
	gpu->nr_rings = 1;
}
