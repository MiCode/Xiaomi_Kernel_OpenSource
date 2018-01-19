/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include "adreno_a6xx.h"
#include "a6xx_reg.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct a6xx_cp_preemption_record, _field)

#define PREEMPT_SMMU_RECORD(_field) \
		offsetof(struct a6xx_cp_smmu_info, _field)

enum {
	SET_PSEUDO_REGISTER_SAVE_REGISTER_SMMU_INFO = 0,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_NON_SECURE_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_SECURE_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_NON_PRIV_SAVE_ADDR,
	SET_PSEUDO_REGISTER_SAVE_REGISTER_COUNTER,
};

static void _update_wptr(struct adreno_device *adreno_dev, bool reset_timer)
{
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	unsigned int wptr;
	unsigned long flags;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/*
	 * Need to make sure GPU is up before we read the
	 * WPTR as fence doesn't wake GPU on read operation.
	 */
	if (in_interrupt() == 0) {
		int status;

		if (gpudev->oob_set) {
			status = gpudev->oob_set(adreno_dev,
				OOB_PREEMPTION_SET_MASK,
				OOB_PREEMPTION_CHECK_MASK,
				OOB_PREEMPTION_CLEAR_MASK);
			if (status)
				return;
		}
	}


	spin_lock_irqsave(&rb->preempt_lock, flags);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);

	if (wptr != rb->wptr) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
			rb->wptr);
		/*
		 * In case something got submitted while preemption was on
		 * going, reset the timer.
		 */
		reset_timer = true;
	}

	if (reset_timer)
		rb->dispatch_q.expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);

	if (in_interrupt() == 0) {
		if (gpudev->oob_clear)
			gpudev->oob_clear(adreno_dev,
				OOB_PREEMPTION_CLEAR_MASK);
	}
}

static inline bool adreno_move_preempt_state(struct adreno_device *adreno_dev,
	enum adreno_preempt_states old, enum adreno_preempt_states new)
{
	return (atomic_cmpxchg(&adreno_dev->preempt.state, old, new) == old);
}

static void _a6xx_preemption_done(struct adreno_device *adreno_dev)
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

	if (status & 0x1) {
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

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

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

static void _a6xx_preemption_fault(struct adreno_device *adreno_dev)
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

static void _a6xx_preemption_worker(struct work_struct *work)
{
	struct adreno_preemption *preempt = container_of(work,
		struct adreno_preemption, work);
	struct adreno_device *adreno_dev = container_of(preempt,
		struct adreno_device, preempt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Need to take the mutex to make sure that the power stays on */
	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_FAULTED))
		_a6xx_preemption_fault(adreno_dev);

	mutex_unlock(&device->mutex);
}

static void _a6xx_preemption_timer(unsigned long data)
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
static struct adreno_ringbuffer *a6xx_next_ringbuffer(
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

void a6xx_preemption_trigger(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *next;
	uint64_t ttbr0, gpuaddr;
	unsigned int contextidr, cntl;
	unsigned long flags;
	struct adreno_preemption *preempt = &adreno_dev->preempt;

	cntl = (((preempt->preempt_level << 6) & 0xC0) |
		((preempt->skipsaverestore << 9) & 0x200) |
		((preempt->usesgmem << 8) & 0x100) | 0x1);

	/* Put ourselves into a possible trigger state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_NONE, ADRENO_PREEMPT_START))
		return;

	/* Get the next ringbuffer to preempt in */
	next = a6xx_next_ringbuffer(adreno_dev);

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

	preempt->count++;

	spin_unlock_irqrestore(&next->preempt_lock, flags);

	/* And write it to the smmu info */
	kgsl_sharedmem_writeq(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(ttbr0), ttbr0);
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(context_idr), contextidr);

	kgsl_sharedmem_readq(&device->scratch, &gpuaddr,
		SCRATCH_PREEMPTION_CTXT_RESTORE_ADDR_OFFSET(next->id));

	/*
	 * Set a keepalive bit before the first preemption register write.
	 * This is required since while each individual write to the context
	 * switch registers will wake the GPU from collapse, it will not in
	 * itself cause GPU activity. Thus, the GPU could technically be
	 * re-collapsed between subsequent register writes leading to a
	 * prolonged preemption sequence. The keepalive bit prevents any
	 * further power collapse while it is set.
	 * It is more efficient to use a keepalive+wake-on-fence approach here
	 * rather than an OOB. Both keepalive and the fence are effectively
	 * free when the GPU is already powered on, whereas an OOB requires an
	 * unconditional handshake with the GMU.
	 */
	kgsl_gmu_regrmw(device, A6XX_GMU_AO_SPARE_CNTL, 0x0, 0x2);

	/*
	 * Fenced writes on this path will make sure the GPU is woken up
	 * in case it was power collapsed by the GMU.
	 */
	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->secure_preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->secure_preemption_desc.gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
		lower_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_gmu_fenced_write(adreno_dev,
		ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
		upper_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK);

	adreno_dev->next_rb = next;

	/* Start the timer to detect a stuck preemption */
	mod_timer(&adreno_dev->preempt.timer,
		jiffies + msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	trace_adreno_preempt_trigger(adreno_dev->cur_rb, adreno_dev->next_rb,
		cntl);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	/* Trigger the preemption */
	adreno_gmu_fenced_write(adreno_dev, ADRENO_REG_CP_PREEMPT, cntl,
		FENCE_STATUS_WRITEDROPPED1_MASK);

	/*
	 * Once preemption has been requested with the final register write,
	 * the preemption process starts and the GPU is considered busy.
	 * We can now safely clear the preemption keepalive bit, allowing
	 * power collapse to resume its regular activity.
	 */
	kgsl_gmu_regrmw(device, A6XX_GMU_AO_SPARE_CNTL, 0x2, 0x0);
}

void a6xx_preemption_callback(struct adreno_device *adreno_dev, int bit)
{
	unsigned int status;

	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_PENDING))
		return;

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &status);

	if (status & 0x1) {
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

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr if it changed while preemption was ongoing */
	_update_wptr(adreno_dev, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	a6xx_preemption_trigger(adreno_dev);
}

void a6xx_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE))
		_a6xx_preemption_done(adreno_dev);

	a6xx_preemption_trigger(adreno_dev);

	mutex_unlock(&device->mutex);
}

unsigned int a6xx_preemption_pre_ibsubmit(
		struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		unsigned int *cmds, struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;
	uint64_t gpuaddr = 0;

	if (context) {
		gpuaddr = context->user_ctxt_record->memdesc.gpuaddr;
		*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 15);
	} else {
		*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 12);
	}

	/* NULL SMMU_INFO buffer - we track in KMD */
	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_SMMU_INFO;
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);

	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_NON_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds, rb->preemption_desc.gpuaddr);

	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_PRIV_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->secure_preemption_desc.gpuaddr);

	if (context) {

		*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_NON_PRIV_SAVE_ADDR;
		cmds += cp_gpuaddr(adreno_dev, cmds, gpuaddr);
	}

	/*
	 * There is no need to specify this address when we are about to
	 * trigger preemption. This is because CP internally stores this
	 * address specified here in the CP_SET_PSEUDO_REGISTER payload to
	 * the context record and thus knows from where to restore
	 * the saved perfcounters for the new ringbuffer.
	 */
	*cmds++ = SET_PSEUDO_REGISTER_SAVE_REGISTER_COUNTER;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->perfcounter_save_restore_desc.gpuaddr);

	if (context) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
		struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
		struct adreno_ringbuffer *rb = drawctxt->rb;
		uint64_t dest =
			SCRATCH_PREEMPTION_CTXT_RESTORE_GPU_ADDR(device,
			rb->id);

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);
	}

	return (unsigned int) (cmds - cmds_orig);
}

unsigned int a6xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	unsigned int *cmds_orig = cmds;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;

	if (rb) {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
		uint64_t dest = SCRATCH_PREEMPTION_CTXT_RESTORE_GPU_ADDR(device,
			rb->id);

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = 0;
		*cmds++ = 0;
	}

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);
	*cmds++ = 1;
	*cmds++ = 0;

	return (unsigned int) (cmds - cmds_orig);
}

void a6xx_preemption_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct adreno_ringbuffer *rb;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	/* smmu_info is allocated and mapped in a6xx_preemption_iommu_init */
	kgsl_sharedmem_writel(device, &iommu->smmu_info,
		PREEMPT_SMMU_RECORD(magic), A6XX_CP_SMMU_INFO_MAGIC_REF);
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

static int a6xx_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
	struct adreno_ringbuffer *rb, uint64_t counteraddr)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = kgsl_allocate_global(device, &rb->preemption_desc,
		A6XX_CP_CTXRECORD_SIZE_IN_BYTES, 0, KGSL_MEMDESC_PRIVILEGED,
		"preemption_desc");
	if (ret)
		return ret;

	ret = kgsl_allocate_user(device, &rb->secure_preemption_desc,
		A6XX_CP_CTXRECORD_SIZE_IN_BYTES,
		KGSL_MEMFLAGS_SECURE | KGSL_MEMDESC_PRIVILEGED);
	if (ret)
		return ret;

	ret = kgsl_iommu_map_global_secure_pt_entry(device,
				&rb->secure_preemption_desc);
	if (ret)
		return ret;

	ret = kgsl_allocate_global(device, &rb->perfcounter_save_restore_desc,
		A6XX_CP_PERFCOUNTER_SAVE_RESTORE_SIZE, 0,
		KGSL_MEMDESC_PRIVILEGED, "perfcounter_save_restore_desc");
	if (ret)
		return ret;

	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(magic), A6XX_CP_CTXRECORD_MAGIC_REF);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(info), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(data), 0);
	kgsl_sharedmem_writel(device, &rb->preemption_desc,
		PREEMPT_RECORD(cntl), A6XX_CP_RB_CNTL_DEFAULT);
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
static int a6xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	/* Allocate mem for storing preemption smmu record */
	return kgsl_allocate_global(device, &iommu->smmu_info, PAGE_SIZE,
		KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_PRIVILEGED,
		"smmu_info");
}

static void a6xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	kgsl_free_global(device, &iommu->smmu_info);
}
#else
static int a6xx_preemption_iommu_init(struct adreno_device *adreno_dev)
{
	return -ENODEV;
}

static void a6xx_preemption_iommu_close(struct adreno_device *adreno_dev)
{
}
#endif

static void a6xx_preemption_close(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	unsigned int i;

	del_timer(&preempt->timer);
	kgsl_free_global(device, &preempt->counters);
	a6xx_preemption_iommu_close(adreno_dev);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		kgsl_free_global(device, &rb->preemption_desc);
		kgsl_free_global(device, &rb->perfcounter_save_restore_desc);
		kgsl_iommu_unmap_global_secure_pt_entry(device,
				&rb->secure_preemption_desc);
		kgsl_sharedmem_free(&rb->secure_preemption_desc);
	}
}

int a6xx_preemption_init(struct adreno_device *adreno_dev)
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

	INIT_WORK(&preempt->work, _a6xx_preemption_worker);

	setup_timer(&preempt->timer, _a6xx_preemption_timer,
		(unsigned long) adreno_dev);

	/* Allocate mem for storing preemption counters */
	ret = kgsl_allocate_global(device, &preempt->counters,
		adreno_dev->num_ringbuffers *
		A6XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE, 0, 0,
		"preemption_counters");
	if (ret)
		goto err;

	addr = preempt->counters.gpuaddr;

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = a6xx_preemption_ringbuffer_init(adreno_dev, rb, addr);
		if (ret)
			goto err;

		addr += A6XX_CP_CTXRECORD_PREEMPTION_COUNTER_SIZE;
	}

	ret = a6xx_preemption_iommu_init(adreno_dev);

err:
	if (ret)
		a6xx_preemption_close(device);

	return ret;
}

void a6xx_preemption_context_destroy(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	gpumem_free_entry(context->user_ctxt_record);

	/* Put the extra ref from gpumem_alloc_entry() */
	kgsl_mem_entry_put(context->user_ctxt_record);
}

int a6xx_preemption_context_init(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t flags = 0;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	if (context->flags & KGSL_CONTEXT_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	/*
	 * gpumem_alloc_entry takes an extra refcount. Put it only when
	 * destroying the context to keep the context record valid
	 */
	context->user_ctxt_record = gpumem_alloc_entry(context->dev_priv,
			A6XX_CP_CTXRECORD_USER_RESTORE_SIZE, flags);
	if (IS_ERR(context->user_ctxt_record)) {
		int ret = PTR_ERR(context->user_ctxt_record);

		context->user_ctxt_record = NULL;
		return ret;
	}

	return 0;
}
