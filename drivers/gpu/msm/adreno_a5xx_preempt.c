/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "adreno.h"
#include "adreno_a5xx.h"
#include "a5xx_reg.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct a5xx_cp_preemption_record, _field)

#define PREEMPT_SMMU_RECORD(_field) \
		offsetof(struct a5xx_cp_smmu_info, _field)

static void _update_wptr(struct adreno_device *adreno_dev, bool reset_timer)
{
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	unsigned int wptr;
	unsigned long flags;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);

	if (wptr != rb->wptr) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
			rb->wptr);
		/*
		 * In case something got submitted while preemption was on
		 * going, reset the timer.
		 */
		reset_timer = 1;
	}

	if (reset_timer)
		rb->dispatch_q.expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);
}

static inline bool adreno_move_preempt_state(struct adreno_device *adreno_dev,
	enum adreno_preempt_states old, enum adreno_preempt_states new)
{
	return (atomic_cmpxchg(&adreno_dev->preempt.state, old, new) == old);
}

static void _a5xx_preemption_done(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * In the very unlikely case that the power is off, do nothing - the
	 * state will be reset on power up and everybody will be happy
	 */

	if (!kgsl_state_is_awake(device))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status != 0) {
		KGSL_DRV_ERR(device,
			"Preemption not complete: status=%X cur=%d R/W=%X/%X next=%d R/W=%X/%X\n",
			status, adreno_dev->cur_rb->id,
			adreno_get_rptr(adreno_dev->cur_rb),
			adreno_dev->cur_rb->wptr, adreno_dev->next_rb->id,
			adreno_get_rptr(adreno_dev->next_rb),
			adreno_dev->next_rb->wptr);

		/* Set a fault and restart */
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		adreno_dispatcher_schedule(device);

		return;
	}

	del_timer_sync(&adreno_dev->preempt.timer);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb, 0);

	/* Clean up all the bits */
	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr for the new command queue */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	/* Clear the preempt state */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
}

static void _a5xx_preemption_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * If the power is on check the preemption status one more time - if it
	 * was successful then just transition to the complete state
	 */
	if (kgsl_state_is_awake(device)) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

		if (status == 0) {
			adreno_set_preempt_state(adreno_dev,
				ADRENO_PREEMPT_COMPLETE);

			adreno_dispatcher_schedule(device);
			return;
		}
	}

	KGSL_DRV_ERR(device,
		"Preemption timed out: cur=%d R/W=%X/%X, next=%d R/W=%X/%X\n",
		adreno_dev->cur_rb->id,
		adreno_get_rptr(adreno_dev->cur_rb), adreno_dev->cur_rb->wptr,
		adreno_dev->next_rb->id,
		adreno_get_rptr(adreno_dev->next_rb),
		adreno_dev->next_rb->wptr);

	adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
	adreno_dispatcher_schedule(device);
}

static void _a5xx_preemption_worker(struct work_struct *work)
{
	struct adreno_preemption *preempt = container_of(work,
		struct adreno_preemption, work);
	struct adreno_device *adreno_dev = container_of(preempt,
		struct adreno_device, preempt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Need to take the mutex to make sure that the power stays on */
	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_FAULTED))
		_a5xx_preemption_fault(adreno_dev);

	mutex_unlock(&device->mutex);
}

static void _a5xx_preemption_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;

	/* We should only be here from a triggered state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_FAULTED))
		return;

	/* Schedule the worker to take care of the details */
	queue_work(system_unbound_wq, &adreno_dev->preempt.work);
}

/* Find the highest priority active ringbuffer */
static struct adreno_ringbuffer *a5xx_next_ringbuffer(
		struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb;
	unsigned long flags;
	unsigned int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		bool empty;

		spin_lock_irqsave(&rb->preempt_lock, flags);
		empty = adreno_rb_empty(rb);
		spin_unlock_irqrestore(&rb->preempt_lock, flags);

		if (empty == false)
			return rb;
	}

	return NULL;
}

void a5xx_preemption_trigger(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *next;
	uint64_t ttbr0;
	unsigned int contextidr;
	unsigned long flags;

	/* Put ourselves into a possible trigger state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_NONE, ADRENO_PREEMPT_START))
		return;

	/* Get the next ringbuffer to preempt in */
	next = a5xx_next_ringbuffer(adreno_dev);

	/*
	 * Nothing to do if every ringbuffer is empty or if the current
	 * ringbuffer is the only active one
	 */
	if (next == NULL || next == adreno_dev->cur_rb) {
		/*
		 * Update any critical things that might have been skipped while
		 * we were looking for a new ringbuffer
		 */

		if (next != NULL) {
			_update_wptr(adreno_dev, false);

			mod_timer(&adreno_dev->dispatcher.timer,
				adreno_dev->cur_rb->dispatch_q.expires);
		}

		adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
		return;
	}

	/* Turn off the dispatcher timer */
	del_timer(&adreno_dev->dispatcher.timer);

	/*
	 * This is the most critical section - we need to take care not to race
	 * until we have programmed the CP for the switch
	 */

	spin_lock_irqsave(&next->preempt_lock, flags);

	/*
	 * Get the pagetable from the pagetable info.
	 * The pagetable_desc is allocated and mapped at probe time, and
	 * preemption_desc at init time, so no need to check if
	 * sharedmem accesses to these memdescs succeed.
	 */
	kgsl_sharedmem_readq(&next->pagetable_desc, &ttbr0,
		PT_INFO_OFFSET(ttbr0));
	kgsl_sharedmem_readl(&next->pagetable_desc, &contextidr,
		PT_INFO_OFFSET(contextidr));

	kgsl_sharedmem_writel(device, &next->preemption_desc,
		PREEMPT_RECORD(wptr), next->wptr);

	spin_unlock_irqrestore(&next->preempt_lock, flags);

	/* And write it to the smmu info */
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(ttbr0), ttbr0);
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(context_idr), contextidr);

	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO,
		lower_32_bits(next->preemption_desc.gpuaddr));
	kgsl_regwrite(device, A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_HI,
		upper_32_bits(next->preemption_desc.gpuaddr));

	adreno_dev->next_rb = next;

	/* Start the timer to detect a stuck preemption */
	mod_timer(&adreno_dev->preempt.timer,
		jiffies + msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	trace_adreno_preempt_trigger(adreno_dev->cur_rb, adreno_dev->next_rb,
		1);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	/* Trigger the preemption */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PREEMPT, 1);
}

void a5xx_preempt_callback(struct adreno_device *adreno_dev, int bit)
{
	unsigned int status;

	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_PENDING))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status != 0) {
		KGSL_DRV_ERR(KGSL_DEVICE(adreno_dev),
			"preempt interrupt with non-zero status: %X\n", status);

		/*
		 * Under the assumption that this is a race between the
		 * interrupt and the register, schedule the worker to clean up.
		 * If the status still hasn't resolved itself by the time we get
		 * there then we have to assume something bad happened
		 */
		adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE);
		adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
		return;
	}

	del_timer(&adreno_dev->preempt.timer);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb, 0);

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr if it changed while preemption was ongoing */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	a5xx_preemption_trigger(adreno_dev);
}

void a5xx_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE))
		_a5xx_preemption_done(adreno_dev);

	a5xx_preemption_trigger(adreno_dev);

	mutex_unlock(&device->mutex);
}

unsigned int a5xx_preemption_pre_ibsubmit(
			struct adreno_device *adreno_dev,
			struct adreno_ringbuffer *rb,
			unsigned int *cmds, struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = rb->preemption_desc.gpuaddr;
	unsigned int preempt_style = 0;

	if (context) {
		/*
		 * Preemption from secure to unsecure needs Zap shader to be
		 * run to clear all secure content. CP does not know during
		 * preemption if it is switching between secure and unsecure
		 * contexts so restrict Secure contexts to be preempted at
		 * ringbuffer level.
		 */
		if (context->flags & KGSL_CONTEXT_SECURE)
			preempt_style = KGSL_CONTEXT_PREEMPT_STYLE_RINGBUFFER;
		else
			preempt_style = ADRENO_PREEMPT_STYLE(context->flags);
	}

	/*
	 * CP_PREEMPT_ENABLE_GLOBAL(global preemption) can only be set by KMD
	 * in ringbuffer.
	 * 1) set global preemption to 0x0 to disable global preemption.
	 *    Only RB level preemption is allowed in this mode
	 * 2) Set global preemption to defer(0x2) for finegrain preemption.
	 *    when global preemption is set to defer(0x2),
	 *    CP_PREEMPT_ENABLE_LOCAL(local preemption) determines the
	 *    preemption point. Local preemption
	 *    can be enabled by both UMD(within IB) and KMD.
	 */
	*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_GLOBAL, 1);
	*cmds++ = ((preempt_style == KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN)
				? 2 : 0);

	/* Turn CP protection OFF */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	/*
	 * CP during context switch will save context switch info to
	 * a5xx_cp_preemption_record pointed by CONTEXT_SWITCH_SAVE_ADDR
	 */
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_LO, 1);
	*cmds++ = lower_32_bits(gpuaddr);
	*cmds++ = cp_type4_packet(A5XX_CP_CONTEXT_SWITCH_SAVE_ADDR_HI, 1);
	*cmds++ = upper_32_bits(gpuaddr);

	/* Turn CP protection ON */
	*cmds++ = cp_type7_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 1;

	/*
	 * Enable local preemption for finegrain preemption in case of
	 * a misbehaving IB
	 */
	if (preempt_style == KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN) {
		*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
		*cmds++ = 1;
	} else {
		*cmds++ = cp_type7_packet(CP_PREEMPT_ENABLE_LOCAL, 1);
		*cmds++ = 0;
	}

	/* Enable CP_CONTEXT_SWITCH_YIELD packets in the IB2s */
	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 2;

	return (unsigned int) (cmds - cmds_orig);
}

int a5xx_preemption_yield_enable(unsigned int *cmds)
{
	/*
	 * SRM -- set render mode (ex binning, direct render etc)
	 * SRM is set by UMD usually at start of IB to tell CP the type of
	 * preemption.
	 * KMD needs to set SRM to NULL to indicate CP that rendering is
	 * done by IB.
	 */
	*cmds++ = cp_type7_packet(CP_SET_RENDER_MODE, 5);
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;

	*cmds++ = cp_type7_packet(CP_YIELD_ENABLE, 1);
	*cmds++ = 1;

	return 8;
}

unsigned int a5xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	int dwords = 0;

	cmds[dwords++] = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	/* Write NULL to the address to skip the data write */
	dwords += cp_gpuaddr(adreno_dev, &cmds[dwords], 0x0);
	cmds[dwords++] = 1;
	/* generate interrupt on preemption completion */
	cmds[dwords++] = 1;

	return dwords;
}

void a5xx_preemption_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *rb;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	/* smmu_info is allocated and mapped in a5xx_preemption_iommu_init */
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(magic), A5XX_CP_SMMU_INFO_MAGIC_REF);
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(ttbr0), MMU_DEFAULT_TTBR0(device));

	/* The CP doesn't use the asid record, so poison it */
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(asid), 0xDECAFBAD);
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(context_idr),
		MMU_DEFAULT_CONTEXTIDR(device));

	adreno_writereg64(adreno_dev,
			ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
			ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
			iommu->smmu_info.gpuaddr);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		/*
		 * preemption_desc is allocated and mapped at init time,
		 * so no need to check sharedmem_writel return value
		 */
		kgsl_sharedmem_writel(device, &rb->preemption_desc,
			PREEMPT_RECORD(rptr), 0);
		kgsl_sharedmem_writel(device, &rb->preemption_desc,
			PREEMPT_RECORD(wptr), 0);

		adreno_ringbuffer_set_pagetable(rb,
			device->mmu.defaultpagetable);
	}

}

static int a5xx_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, uint64_t counteraddr)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = kgsl_allocate_global(device, &rb->preemption_desc,
		A5XX_CP_CTXRECORD_SIZE_IN_BYTES, 0, KGSL_MEMDESC_PRIVILEGED,
		"preemption_desc");
	if (ret)
		return ret;

	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(magic), A5XX_CP_CTXRECORD_MAGIC_REF);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(info), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(data), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(cntl), A5XX_CP_RB_CNTL_DEFAULT);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(rptr), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(wptr), 0);
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(rptr_addr), SCRATCH_RPTR_GPU_ADDR(device,
			rb->id));
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(rbase), rb->buffer_desc.gpuaddr);
	kgsl_sharedmem_writeq(device, &rb->preemption_desc,
		PREEMPT_RECORD(counter), counteraddr);

	return 0;
}

#ifdef CONFIG_QCOM_KGSL_IOMMU
static int a5xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	/* Allocate mem for storing preemption smmu record */
	return kgsl_allocate_global(device, &iommu->smmu_info, PAGE_SIZE,
		KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_PRIVILEGED,
		"smmu_info");
}

static void a5xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	kgsl_free_global(device, &iommu->smmu_info);
}

#else
static int a5xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	return -ENODEV;
}

static void a5xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
}
#endif

static void a5xx_preemption_close(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	unsigned int i;

	del_timer(&preempt->timer);
	kgsl_free_global(device, &preempt->counters);
	a5xx_preemption_iommu_close(adreno_dev);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		kgsl_free_global(device, &rb->preemption_desc);
	}
}

int a5xx_preemption_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	int ret;
	unsigned int i;
	uint64_t addr;

	/* We are dependent on IOMMU to make preemption go on the CP side */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return -ENODEV;

	INIT_WORK(&preempt->work, _a5xx_preemption_worker);

	setup_timer(&preempt->timer, _a5xx_preemption_timer,
		(unsigned long) adreno_dev);

	/* Allocate mem for storing preemption counters */
	ret = kgsl_allocate_global(device, &preempt->counters,
		adreno_dev->num_ringbuffers *
		A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE, 0, 0,
		"preemption_counters");
	if (ret)
		goto err;

	addr = preempt->counters.gpuaddr;

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = a5xx_preemption_ringbuffer_init(adreno_dev, rb, addr);
		if (ret)
			goto err;

		addr += A5XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE;
	}

	ret = a5xx_preemption_iommu_init(adreno_dev);

err:
	if (ret)
		a5xx_preemption_close(device);

	return ret;
}
