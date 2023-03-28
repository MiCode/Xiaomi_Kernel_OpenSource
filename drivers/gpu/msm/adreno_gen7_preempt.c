// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#define PREEMPT_RECORD(_field) \
		offsetof(struct gen7_cp_preemption_record, _field)

#define PREEMPT_SMMU_RECORD(_field) \
		offsetof(struct gen7_cp_smmu_info, _field)

static void _update_wptr(struct adreno_device *adreno_dev, bool reset_timer,
	bool atomic)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	if (!atomic) {
		/*
		 * We might have skipped updating the wptr in case we are in
		 * dispatcher context. Do it now.
		 */
		if (rb->skip_inline_wptr) {

			ret = gen7_fenced_write(adreno_dev,
				GEN7_CP_RB_WPTR, rb->wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);

			reset_timer = true;
			rb->skip_inline_wptr = false;
		}
	} else {
		unsigned int wptr;

		kgsl_regread(device, GEN7_CP_RB_WPTR, &wptr);
		if (wptr != rb->wptr) {
			kgsl_regwrite(device, GEN7_CP_RB_WPTR, rb->wptr);
			reset_timer = true;
		}
	}

	if (reset_timer)
		rb->dispatch_q.expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);

	if (!atomic) {
		/* If WPTR update fails, set the fault and trigger recovery */
		if (ret) {
			gmu_core_fault_snapshot(device);
			adreno_dispatcher_fault(adreno_dev,
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
		}
	}
}

static void _power_collapse_set(struct adreno_device *adreno_dev, bool val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	gmu_core_regwrite(device,
			 GEN7_GMU_PWR_COL_PREEMPT_KEEPALIVE, (val ? 1 : 0));
}

static void _gen7_preemption_done(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * In the very unlikely case that the power is off, do nothing - the
	 * state will be reset on power up and everybody will be happy
	 */

	if (!kgsl_state_is_awake(device))
		return;

	kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_CNTL, &status);

	if (status & 0x1) {
		dev_err(device->dev,
			"Preemption not complete: status=%X cur=%d R/W=%X/%X next=%d R/W=%X/%X\n",
			status, adreno_dev->cur_rb->id,
			adreno_get_rptr(adreno_dev->cur_rb),
			adreno_dev->cur_rb->wptr,
			adreno_dev->next_rb->id,
			adreno_get_rptr(adreno_dev->next_rb),
			adreno_dev->next_rb->wptr);

		/* Set a fault and restart */
		adreno_dispatcher_fault(adreno_dev, ADRENO_PREEMPT_FAULT);

		return;
	}

	adreno_dev->preempt.count++;

	del_timer_sync(&adreno_dev->preempt.timer);

	kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

	/* Clean up all the bits */
	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr for the new command queue */
	_update_wptr(adreno_dev, true, false);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	/* Clear the preempt state */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
}

static void _gen7_preemption_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	/*
	 * If the power is on check the preemption status one more time - if it
	 * was successful then just transition to the complete state
	 */
	if (kgsl_state_is_awake(device)) {
		kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_CNTL, &status);

		if (!(status & 0x1)) {
			adreno_set_preempt_state(adreno_dev,
				ADRENO_PREEMPT_COMPLETE);

			adreno_dispatcher_schedule(device);
			return;
		}
	}

	dev_err(device->dev,
		"Preemption Fault: cur=%d R/W=0x%x/0x%x, next=%d R/W=0x%x/0x%x\n",
		adreno_dev->cur_rb->id,
		adreno_get_rptr(adreno_dev->cur_rb),
		adreno_dev->cur_rb->wptr,
		adreno_dev->next_rb->id,
		adreno_get_rptr(adreno_dev->next_rb),
		adreno_dev->next_rb->wptr);

	adreno_dispatcher_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
}

static void _gen7_preemption_worker(struct work_struct *work)
{
	struct adreno_preemption *preempt = container_of(work,
		struct adreno_preemption, work);
	struct adreno_device *adreno_dev = container_of(preempt,
		struct adreno_device, preempt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Need to take the mutex to make sure that the power stays on */
	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_FAULTED))
		_gen7_preemption_fault(adreno_dev);

	mutex_unlock(&device->mutex);
}

/* Find the highest priority active ringbuffer */
static struct adreno_ringbuffer *gen7_next_ringbuffer(
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

		if (!empty)
			return rb;
	}

	return NULL;
}

void gen7_preemption_trigger(struct adreno_device *adreno_dev, bool atomic)
{
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct adreno_ringbuffer *next;
	u64 ttbr0, gpuaddr;
	u32 contextidr, cntl;
	unsigned long flags;
	struct adreno_preemption *preempt = &adreno_dev->preempt;

	/* Put ourselves into a possible trigger state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_NONE, ADRENO_PREEMPT_START))
		return;

	/* Get the next ringbuffer to preempt in */
	next = gen7_next_ringbuffer(adreno_dev);

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
			_update_wptr(adreno_dev, false, atomic);

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

	/* Get the pagetable from the pagetable info. */
	kgsl_sharedmem_readq(device->scratch, &ttbr0,
		SCRATCH_RB_OFFSET(next->id, ttbr0));
	kgsl_sharedmem_readl(device->scratch, &contextidr,
		SCRATCH_RB_OFFSET(next->id, contextidr));

	kgsl_sharedmem_writel(next->preemption_desc,
		PREEMPT_RECORD(wptr), next->wptr);

	spin_unlock_irqrestore(&next->preempt_lock, flags);

	/* And write it to the smmu info */
	if (kgsl_mmu_is_perprocess(&device->mmu)) {
		kgsl_sharedmem_writeq(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(ttbr0), ttbr0);
		kgsl_sharedmem_writel(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(context_idr), contextidr);
	}

	kgsl_sharedmem_readq(preempt->scratch, &gpuaddr,
		next->id * sizeof(u64));

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
	_power_collapse_set(adreno_dev, true);

	/*
	 * Fenced writes on this path will make sure the GPU is woken up
	 * in case it was power collapsed by the GMU.
	 */
	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	/*
	 * Above fence writes will make sure GMU comes out of
	 * IFPC state if its was in IFPC state but it doesn't
	 * guarantee that GMU FW actually moved to ACTIVE state
	 * i.e. wake-up from IFPC is complete.
	 * Wait for GMU to move to ACTIVE state before triggering
	 * preemption. This is require to make sure CP doesn't
	 * interrupt GMU during wake-up from IFPC.
	 */
	if (!atomic && gmu_core_dev_wait_for_active_transition(device))
		goto err;

	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
		lower_32_bits(next->secure_preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
		upper_32_bits(next->secure_preemption_desc->gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
		lower_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	if (gen7_fenced_write(adreno_dev,
		GEN7_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
		upper_32_bits(gpuaddr),
		FENCE_STATUS_WRITEDROPPED1_MASK))
		goto err;

	adreno_dev->next_rb = next;

	/* Start the timer to detect a stuck preemption */
	mod_timer(&adreno_dev->preempt.timer,
		jiffies + msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	cntl = (preempt->preempt_level << 6) | 0x01;

	/* Skip save/restore during L1 preemption */
	if (preempt->skipsaverestore)
		cntl |= (1 << 9);

	/* Enable GMEM save/restore across preemption */
	if (preempt->usesgmem)
		cntl |= (1 << 8);

	trace_adreno_preempt_trigger(adreno_dev->cur_rb, adreno_dev->next_rb,
		cntl);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	if (gen7_core->qos_value)
		kgsl_sharedmem_writel(preempt->scratch,
			PREEMPT_SCRATCH_OFFSET(QOS_VALUE_IDX),
			gen7_core->qos_value[next->id]);

	/* Trigger the preemption */
	if (gen7_fenced_write(adreno_dev, GEN7_CP_CONTEXT_SWITCH_CNTL, cntl,
					FENCE_STATUS_WRITEDROPPED1_MASK)) {
		adreno_dev->next_rb = NULL;
		del_timer(&adreno_dev->preempt.timer);
		goto err;
	}

	return;
err:
	/* If fenced write fails, take inline snapshot and trigger recovery */
	if (!in_interrupt()) {
		gmu_core_fault_snapshot(device);
		adreno_dispatcher_fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
	} else {
		adreno_dispatcher_fault(adreno_dev, ADRENO_GMU_FAULT);
	}
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);
	/* Clear the keep alive */
	_power_collapse_set(adreno_dev, false);

}

void gen7_preemption_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status;

	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_PENDING))
		return;

	kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_CNTL, &status);

	if (status & 0x1) {
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
			"preempt interrupt with non-zero status: %X\n",
			status);

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

	adreno_dev->preempt.count++;

	/*
	 * We can now safely clear the preemption keepalive bit, allowing
	 * power collapse to resume its regular activity.
	 */
	_power_collapse_set(adreno_dev, false);

	del_timer(&adreno_dev->preempt.timer);

	kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_LEVEL_STATUS, &status);

	trace_adreno_preempt_done(adreno_dev->cur_rb, adreno_dev->next_rb,
		status);

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->next_rb = NULL;

	/* Update the wptr if it changed while preemption was ongoing */
	_update_wptr(adreno_dev, true, true);

	/* Update the dispatcher timer for the new command queue */
	mod_timer(&adreno_dev->dispatcher.timer,
		adreno_dev->cur_rb->dispatch_q.expires);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	gen7_preemption_trigger(adreno_dev, true);
}

void gen7_preemption_prepare_postamble(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	u32 *postamble, count = 0;

	/*
	 * First 28 dwords of the device scratch buffer are used to store shadow rb data.
	 * Reserve 15 dwords in the device scratch buffer from SCRATCH_POSTAMBLE_OFFSET for
	 * KMD postamble pm4 packets. This should be in *device->scratch* so that userspace
	 * cannot access it.
	 */
	postamble = device->scratch->hostptr + SCRATCH_POSTAMBLE_OFFSET;

	/* Reserve 11 dwords in the device scratch buffer to clear perfcounters */
	if (!adreno_dev->perfcounter) {
		postamble[count++] = cp_type7_packet(CP_REG_RMW, 3);
		postamble[count++] = GEN7_RBBM_PERFCTR_SRAM_INIT_CMD;
		postamble[count++] = 0x0;
		postamble[count++] = 0x1;

		postamble[count++] = cp_type7_packet(CP_WAIT_REG_MEM, 6);
		postamble[count++] = 0x3;
		postamble[count++] = GEN7_RBBM_PERFCTR_SRAM_INIT_STATUS;
		postamble[count++] = 0x0;
		postamble[count++] = 0x1;
		postamble[count++] = 0x1;
		postamble[count++] = 0x0;
	}

	/*
	 * Reserve 4 dwords in the scratch buffer for dynamic QOS control feature. To ensure QOS
	 * value is updated for first preemption, send it during bootup.
	 */
	if (gen7_core->qos_value) {
		postamble[count++] = cp_type7_packet(CP_MEM_TO_REG, 3);
		postamble[count++] = GEN7_RBBM_GBIF_CLIENT_QOS_CNTL;
		postamble[count++] = lower_32_bits(PREEMPT_SCRATCH_ADDR(adreno_dev, QOS_VALUE_IDX));
		postamble[count++] = upper_32_bits(PREEMPT_SCRATCH_ADDR(adreno_dev, QOS_VALUE_IDX));
	}

	preempt->postamble_len = count;
}

void gen7_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_COMPLETE))
		_gen7_preemption_done(adreno_dev);

	gen7_preemption_trigger(adreno_dev, false);

	mutex_unlock(&device->mutex);
}

u32 gen7_preemption_pre_ibsubmit(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 *cmds)
{
	u32 *cmds_orig = cmds;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	if (test_and_set_bit(ADRENO_RB_SET_PSEUDO_DONE, &rb->flags))
		goto done;

	*cmds++ = cp_type7_packet(CP_THREAD_CONTROL, 1);
	*cmds++ = CP_SET_THREAD_BR;

	*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 12);

	/* NULL SMMU_INFO buffer - we track in KMD */
	*cmds++ = SET_PSEUDO_SMMU_INFO;
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);

	*cmds++ = SET_PSEUDO_PRIV_NON_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds, rb->preemption_desc->gpuaddr);

	*cmds++ = SET_PSEUDO_PRIV_SECURE_SAVE_ADDR;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->secure_preemption_desc->gpuaddr);

	/*
	 * There is no need to specify this address when we are about to
	 * trigger preemption. This is because CP internally stores this
	 * address specified here in the CP_SET_PSEUDO_REGISTER payload to
	 * the context record and thus knows from where to restore
	 * the saved perfcounters for the new ringbuffer.
	 */
	*cmds++ = SET_PSEUDO_COUNTER;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->perfcounter_save_restore_desc->gpuaddr);

done:
	if (drawctxt) {
		struct adreno_ringbuffer *rb = drawctxt->rb;
		u64 dest = PREEMPT_SCRATCH_ADDR(adreno_dev, rb->id);
		u64 gpuaddr = drawctxt->base.user_ctxt_record->memdesc.gpuaddr;

		*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 2);
		cmds += cp_gpuaddr(adreno_dev, cmds, dest);
		*cmds++ = lower_32_bits(gpuaddr);
		*cmds++ = upper_32_bits(gpuaddr);

		if (adreno_dev->preempt.postamble_len) {
			u64 kmd_postamble_addr = SCRATCH_POSTAMBLE_ADDR(KGSL_DEVICE(adreno_dev));

			*cmds++ = cp_type7_packet(CP_SET_AMBLE, 3);
			*cmds++ = lower_32_bits(kmd_postamble_addr);
			*cmds++ = upper_32_bits(kmd_postamble_addr);
			*cmds++ = FIELD_PREP(GENMASK(22, 20), CP_KMD_AMBLE_TYPE)
				| (FIELD_PREP(GENMASK(19, 0), adreno_dev->preempt.postamble_len));
		}
	}

	return (unsigned int) (cmds - cmds_orig);
}

u32 gen7_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
		u32 *cmds)
{
	u32 index = 0;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	if (adreno_dev->cur_rb) {
		u64 dest = PREEMPT_SCRATCH_ADDR(adreno_dev, adreno_dev->cur_rb->id);

		cmds[index++] = cp_type7_packet(CP_MEM_WRITE, 4);
		cmds[index++] = lower_32_bits(dest);
		cmds[index++] = upper_32_bits(dest);
		cmds[index++] = 0;
		cmds[index++] = 0;
	}

	cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[index++] = CP_SET_THREAD_BOTH;
	cmds[index++] = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds[index++] = 0;
	cmds[index++] = 0;
	cmds[index++] = 1;
	cmds[index++] = 0;

	return index;
}

void gen7_preemption_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct adreno_ringbuffer *rb;
	unsigned int i;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	/* Force the state to be clear */
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	if (kgsl_mmu_is_perprocess(&device->mmu)) {
		/* smmu_info is allocated and mapped in gen7_preemption_iommu_init */
		kgsl_sharedmem_writel(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(magic), GEN7_CP_SMMU_INFO_MAGIC_REF);
		kgsl_sharedmem_writeq(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(ttbr0), MMU_DEFAULT_TTBR0(device));

		/* The CP doesn't use the asid record, so poison it */
		kgsl_sharedmem_writel(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(asid), 0xdecafbad);
		kgsl_sharedmem_writel(iommu->smmu_info,
			PREEMPT_SMMU_RECORD(context_idr), 0);

		kgsl_regwrite(device, GEN7_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
			lower_32_bits(iommu->smmu_info->gpuaddr));

		kgsl_regwrite(device, GEN7_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
			upper_32_bits(iommu->smmu_info->gpuaddr));
	}

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		kgsl_sharedmem_writel(rb->preemption_desc,
			PREEMPT_RECORD(rptr), 0);
		kgsl_sharedmem_writel(rb->preemption_desc,
			PREEMPT_RECORD(wptr), 0);

		adreno_ringbuffer_set_pagetable(device, rb,
			device->mmu.defaultpagetable);

		clear_bit(ADRENO_RB_SET_PSEUDO_DONE, &rb->flags);
	}
}

static void reset_rb_preempt_record(struct adreno_device *adreno_dev,
	struct adreno_ringbuffer *rb)
{
	memset(rb->preemption_desc->hostptr, 0x0, rb->preemption_desc->size);

	kgsl_sharedmem_writel(rb->preemption_desc,
		PREEMPT_RECORD(magic), GEN7_CP_CTXRECORD_MAGIC_REF);
	kgsl_sharedmem_writel(rb->preemption_desc,
		PREEMPT_RECORD(cntl), GEN7_CP_RB_CNTL_DEFAULT);
	kgsl_sharedmem_writeq(rb->preemption_desc,
		PREEMPT_RECORD(rptr_addr), SCRATCH_RB_GPU_ADDR(
		KGSL_DEVICE(adreno_dev), rb->id, rptr));
	kgsl_sharedmem_writeq(rb->preemption_desc,
		PREEMPT_RECORD(rbase), rb->buffer_desc->gpuaddr);
	kgsl_sharedmem_writeq(rb->preemption_desc,
		PREEMPT_RECORD(bv_rptr_addr), SCRATCH_RB_GPU_ADDR(
		KGSL_DEVICE(adreno_dev), rb->id, bv_rptr));
}

void gen7_reset_preempt_records(struct adreno_device *adreno_dev)
{
	int i;
	struct adreno_ringbuffer *rb;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		reset_rb_preempt_record(adreno_dev, rb);
	}
}

static int gen7_preemption_ringbuffer_init(struct adreno_device *adreno_dev,
	struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	u64 ctxt_record_size = GEN7_CP_CTXRECORD_SIZE_IN_BYTES;
	int ret;

	if (gen7_core->ctxt_record_size)
		ctxt_record_size = gen7_core->ctxt_record_size;

	ret = adreno_allocate_global(device, &rb->preemption_desc,
		ctxt_record_size, SZ_16K, 0,
		KGSL_MEMDESC_PRIVILEGED, "preemption_desc");
	if (ret)
		return ret;

	ret = adreno_allocate_global(device, &rb->secure_preemption_desc,
		ctxt_record_size, 0,
		KGSL_MEMFLAGS_SECURE, KGSL_MEMDESC_PRIVILEGED,
		"secure_preemption_desc");
	if (ret)
		return ret;

	ret = adreno_allocate_global(device, &rb->perfcounter_save_restore_desc,
		GEN7_CP_PERFCOUNTER_SAVE_RESTORE_SIZE, 0, 0,
		KGSL_MEMDESC_PRIVILEGED,
		"perfcounter_save_restore_desc");
	if (ret)
		return ret;

	reset_rb_preempt_record(adreno_dev, rb);

	return 0;
}

int gen7_preemption_init(struct adreno_device *adreno_dev)
{
	u32 flags = ADRENO_FEATURE(adreno_dev, ADRENO_APRIV) ? KGSL_MEMDESC_PRIVILEGED : 0;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	struct adreno_ringbuffer *rb;
	int ret;
	unsigned int i;

	/* We are dependent on IOMMU to make preemption go on the CP side */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_IOMMU)
		return -ENODEV;

	INIT_WORK(&preempt->work, _gen7_preemption_worker);

	/* Allocate mem for storing preemption switch record */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = gen7_preemption_ringbuffer_init(adreno_dev, rb);
		if (ret)
			return ret;
	}

	ret = adreno_allocate_global(device, &preempt->scratch, PAGE_SIZE,
			0, 0, flags, "preempt_scratch");
	if (ret)
		return ret;

	/* Allocate mem for storing preemption smmu record */
	if (kgsl_mmu_is_perprocess(&device->mmu)) {
		ret = adreno_allocate_global(device, &iommu->smmu_info, PAGE_SIZE, 0,
			KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_PRIVILEGED,
			"smmu_info");
		if (ret)
			return ret;
	}

	set_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
	return 0;
}

int gen7_preemption_context_init(struct kgsl_context *context)
{
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u64 flags = 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		return 0;

	if (context->flags & KGSL_CONTEXT_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	if (is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	/*
	 * gpumem_alloc_entry takes an extra refcount. Put it only when
	 * destroying the context to keep the context record valid
	 */
	context->user_ctxt_record = gpumem_alloc_entry(context->dev_priv,
			GEN7_CP_CTXRECORD_USER_RESTORE_SIZE, flags);
	if (IS_ERR(context->user_ctxt_record)) {
		int ret = PTR_ERR(context->user_ctxt_record);

		context->user_ctxt_record = NULL;
		return ret;
	}

	return 0;
}
