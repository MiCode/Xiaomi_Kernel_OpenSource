/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include "adreno.h"
#include "adreno_a4xx.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"

#define ADRENO_RB_PREEMPT_TOKEN_DWORDS		125

static void a4xx_preemption_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int cur_rptr = adreno_get_rptr(adreno_dev->cur_rb);
	unsigned int next_rptr = adreno_get_rptr(adreno_dev->next_rb);

	KGSL_DRV_ERR(device,
		"Preemption timed out. cur_rb rptr/wptr %x/%x id %d, next_rb rptr/wptr %x/%x id %d, disp_state: %d\n",
		cur_rptr, adreno_dev->cur_rb->wptr, adreno_dev->cur_rb->id,
		next_rptr, adreno_dev->next_rb->wptr, adreno_dev->next_rb->id,
		atomic_read(&adreno_dev->preempt.state));

	adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
	adreno_dispatcher_schedule(device);
}

static unsigned int a4xx_preemption_token(struct adreno_device *adreno_dev,
			unsigned int *cmds, uint64_t gpuaddr)
{
	unsigned int *cmds_orig = cmds;

	/* Turn on preemption flag */
	/* preemption token - fill when pt switch command size is known */
	*cmds++ = cp_type3_packet(CP_PREEMPT_TOKEN, 3);
	*cmds++ = (uint)gpuaddr;
	*cmds++ = 1;
	/* generate interrupt on preemption completion */
	*cmds++ = 1 << CP_PREEMPT_ORDINAL_INTERRUPT;

	return (unsigned int) (cmds - cmds_orig);
}

unsigned int a4xx_preemption_pre_ibsubmit(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, unsigned int *cmds,
		struct kgsl_context *context)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds_orig = cmds;
	unsigned int cond_addr = device->memstore.gpuaddr +
		MEMSTORE_ID_GPU_ADDR(device, context->id, preempted);

	cmds += a4xx_preemption_token(adreno_dev, cmds, cond_addr);

	*cmds++ = cp_type3_packet(CP_COND_EXEC, 4);
	*cmds++ = cond_addr;
	*cmds++ = cond_addr;
	*cmds++ = 1;
	*cmds++ = 7;

	/* clear preemption flag */
	*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
	*cmds++ = cond_addr;
	*cmds++ = 0;
	*cmds++ = cp_type3_packet(CP_WAIT_MEM_WRITES, 1);
	*cmds++ = 0;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
	*cmds++ = 0;

	return (unsigned int) (cmds - cmds_orig);
}


static void a4xx_preemption_start(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	uint32_t val;

	/*
	 * Setup scratch registers from which the GPU will program the
	 * registers required to start execution of new ringbuffer
	 * set ringbuffer address
	 */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG8,
		rb->buffer_desc.gpuaddr);
	kgsl_regread(device, A4XX_CP_RB_CNTL, &val);
	/* scratch REG9 corresponds to CP_RB_CNTL register */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG9, val);
	/* scratch REG10 corresponds to rptr address */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG10,
		SCRATCH_RPTR_GPU_ADDR(device, rb->id));
	/* scratch REG11 corresponds to rptr */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG11, adreno_get_rptr(rb));
	/* scratch REG12 corresponds to wptr */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG12, rb->wptr);
	/*
	 * scratch REG13 corresponds to  IB1_BASE,
	 * 0 since we do not do switches in between IB's
	 */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG13, 0);
	/* scratch REG14 corresponds to IB1_BUFSZ */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG14, 0);
	/* scratch REG15 corresponds to IB2_BASE */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG15, 0);
	/* scratch REG16 corresponds to  IB2_BUFSZ */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG16, 0);
	/* scratch REG17 corresponds to GPR11 */
	kgsl_regwrite(device, A4XX_CP_SCRATCH_REG17, rb->gpr11);
}

static void a4xx_preemption_save(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regread(device, A4XX_CP_SCRATCH_REG23, &rb->gpr11);
}


static int a4xx_submit_preempt_token(struct adreno_ringbuffer *rb,
					struct adreno_ringbuffer *incoming_rb)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *ringcmds, *start;
	int ptname;
	struct kgsl_pagetable *pt;
	int pt_switch_sizedwords = 0, total_sizedwords = 20;
	unsigned link[ADRENO_RB_PREEMPT_TOKEN_DWORDS];
	uint i;

	if (incoming_rb->preempted_midway) {

		kgsl_sharedmem_readl(&incoming_rb->pagetable_desc,
			&ptname, PT_INFO_OFFSET(current_rb_ptname));
		pt = kgsl_mmu_get_pt_from_ptname(&(device->mmu),
			ptname);
		/* set the ringbuffer for incoming RB */
		pt_switch_sizedwords =
			adreno_iommu_set_pt_generate_cmds(incoming_rb,
							&link[0], pt);
		total_sizedwords += pt_switch_sizedwords;
	}

	/*
	 *  Allocate total_sizedwords space in RB, this is the max space
	 *  required.
	 */
	ringcmds = adreno_ringbuffer_allocspace(rb, total_sizedwords);

	if (IS_ERR(ringcmds))
		return PTR_ERR(ringcmds);

	start = ringcmds;

	*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
	*ringcmds++ = 0;

	if (incoming_rb->preempted_midway) {
		for (i = 0; i < pt_switch_sizedwords; i++)
			*ringcmds++ = link[i];
	}

	*ringcmds++ = cp_register(adreno_dev, adreno_getreg(adreno_dev,
			ADRENO_REG_CP_PREEMPT_DISABLE), 1);
	*ringcmds++ = 0;

	*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
	*ringcmds++ = 1;

	ringcmds += a4xx_preemption_token(adreno_dev, ringcmds,
				device->memstore.gpuaddr +
				MEMSTORE_RB_OFFSET(rb, preempted));

	if ((uint)(ringcmds - start) > total_sizedwords)
		KGSL_DRV_ERR(device, "Insufficient rb size allocated\n");

	/*
	 * If we have commands less than the space reserved in RB
	 *  adjust the wptr accordingly
	 */
	rb->wptr = rb->wptr - (total_sizedwords - (uint)(ringcmds - start));

	/* submit just the preempt token */
	mb();
	kgsl_pwrscale_busy(device);
	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR, rb->wptr);
	return 0;
}

static void a4xx_preempt_trig_state(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int rbbase, val;
	int ret;

	/*
	 * Hardware not yet idle means that preemption interrupt
	 * may still occur, nothing to do here until interrupt signals
	 * completion of preemption, just return here
	 */
	if (!adreno_hw_isidle(adreno_dev))
		return;

	/*
	 * We just changed states, reschedule dispatcher to change
	 * preemption states
	 */

	if (atomic_read(&adreno_dev->preempt.state) !=
		ADRENO_PREEMPT_TRIGGERED) {
		adreno_dispatcher_schedule(device);
		return;
	}

	/*
	 * H/W is idle and we did not get a preemption interrupt, may
	 * be device went idle w/o encountering any preempt token or
	 * we already preempted w/o interrupt
	 */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_BASE, &rbbase);
	 /* Did preemption occur, if so then change states and return */
	if (rbbase != adreno_dev->cur_rb->buffer_desc.gpuaddr) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_DEBUG, &val);
		if (val && rbbase == adreno_dev->next_rb->buffer_desc.gpuaddr) {
			KGSL_DRV_INFO(device,
			"Preemption completed without interrupt\n");
			trace_adreno_hw_preempt_trig_to_comp(adreno_dev->cur_rb,
					adreno_dev->next_rb,
					adreno_get_rptr(adreno_dev->cur_rb),
					adreno_get_rptr(adreno_dev->next_rb));
			adreno_set_preempt_state(adreno_dev,
				ADRENO_PREEMPT_COMPLETE);
			adreno_dispatcher_schedule(device);
			return;
		}
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		/* reschedule dispatcher to take care of the fault */
		adreno_dispatcher_schedule(device);
		return;
	}
	/*
	 * Check if preempt token was submitted after preemption trigger, if so
	 * then preemption should have occurred, since device is already idle it
	 * means something went wrong - trigger FT
	 */
	if (adreno_dev->preempt.token_submit) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		/* reschedule dispatcher to take care of the fault */
		adreno_dispatcher_schedule(device);
		return;
	}
	/*
	 * Preempt token was not submitted after preemption trigger so device
	 * may have gone idle before preemption could occur, if there are
	 * commands that got submitted to current RB after triggering preemption
	 * then submit them as those commands may have a preempt token in them
	 */
	if (!adreno_rb_empty(adreno_dev->cur_rb)) {
		/*
		 * Memory barrier before informing the
		 * hardware of new commands
		 */
		mb();
		kgsl_pwrscale_busy(device);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
			adreno_dev->cur_rb->wptr);
		return;
	}

	/* Submit preempt token to make preemption happen */
	ret = adreno_drawctxt_switch(adreno_dev, adreno_dev->cur_rb,
		NULL, 0);
	if (ret)
		KGSL_DRV_ERR(device,
			"Unable to switch context to NULL: %d\n", ret);

	ret = a4xx_submit_preempt_token(adreno_dev->cur_rb,
						adreno_dev->next_rb);
	if (ret)
		KGSL_DRV_ERR(device,
			"Unable to submit preempt token: %d\n", ret);

	adreno_dev->preempt.token_submit = true;
	adreno_dev->cur_rb->wptr_preempt_end = adreno_dev->cur_rb->wptr;
	trace_adreno_hw_preempt_token_submit(adreno_dev->cur_rb,
			adreno_dev->next_rb,
			adreno_get_rptr(adreno_dev->cur_rb),
			adreno_get_rptr(adreno_dev->next_rb));
}

static struct adreno_ringbuffer *a4xx_next_ringbuffer(
		struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb, *next = NULL;
	int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (!adreno_rb_empty(rb) && next == NULL) {
			next = rb;
			continue;
		}

		if (!adreno_disp_preempt_fair_sched)
			continue;

		switch (rb->starve_timer_state) {
		case ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT:
			if (!adreno_rb_empty(rb) &&
				adreno_dev->cur_rb != rb) {
				rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_INIT;
				rb->sched_timer = jiffies;
			}
			break;
		case ADRENO_DISPATCHER_RB_STARVE_TIMER_INIT:
			if (time_after(jiffies, rb->sched_timer +
				msecs_to_jiffies(
					adreno_dispatch_starvation_time))) {
				rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED;
				/* halt dispatcher to remove starvation */
				adreno_get_gpu_halt(adreno_dev);
			}
			break;
		case ADRENO_DISPATCHER_RB_STARVE_TIMER_SCHEDULED:
			/*
			 * If the RB has not been running for the minimum
			 * time slice then allow it to run
			 */
			if (!adreno_rb_empty(rb) && time_before(jiffies,
				adreno_dev->cur_rb->sched_timer +
				msecs_to_jiffies(adreno_dispatch_time_slice)))
				next = rb;
			else
				rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT;
			break;
		case ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED:
		default:
			break;
		}
	}

	return next;
}

static void a4xx_preempt_clear_state(struct adreno_device *adreno_dev)

{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *highest_busy_rb;
	int switch_low_to_high;
	int ret;

	/* Device not awake means there is nothing to do */
	if (!kgsl_state_is_awake(device))
		return;

	highest_busy_rb = a4xx_next_ringbuffer(adreno_dev);
	if (!highest_busy_rb || highest_busy_rb == adreno_dev->cur_rb)
		return;

	switch_low_to_high = adreno_compare_prio_level(
					highest_busy_rb->id,
					adreno_dev->cur_rb->id);

	if (switch_low_to_high < 0) {
		/*
		 * if switching to lower priority make sure that the rptr and
		 * wptr are equal, when the lower rb is not starved
		 */
		if (!adreno_rb_empty(adreno_dev->cur_rb))
			return;
		/*
		 * switch to default context because when we switch back
		 * to higher context then its not known which pt will
		 * be current, so by making it default here the next
		 * commands submitted will set the right pt
		 */
		ret = adreno_drawctxt_switch(adreno_dev,
				adreno_dev->cur_rb,
				NULL, 0);
		/*
		 * lower priority RB has to wait until space opens up in
		 * higher RB
		 */
		if (ret) {
			KGSL_DRV_ERR(device,
				"Unable to switch context to NULL: %d",
				ret);

			return;
		}

		adreno_writereg(adreno_dev,
			ADRENO_REG_CP_PREEMPT_DISABLE, 1);
	}

	/*
	 * setup registers to do the switch to highest priority RB
	 * which is not empty or may be starving away(poor thing)
	 */
	a4xx_preemption_start(adreno_dev, highest_busy_rb);

	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_TRIGGERED);

	adreno_dev->next_rb = highest_busy_rb;
	mod_timer(&adreno_dev->preempt.timer, jiffies +
		msecs_to_jiffies(ADRENO_PREEMPT_TIMEOUT));

	trace_adreno_hw_preempt_clear_to_trig(adreno_dev->cur_rb,
			adreno_dev->next_rb,
			adreno_get_rptr(adreno_dev->cur_rb),
			adreno_get_rptr(adreno_dev->next_rb));
	/* issue PREEMPT trigger */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PREEMPT, 1);

	/* submit preempt token packet to ensure preemption */
	if (switch_low_to_high < 0) {
		ret = a4xx_submit_preempt_token(
			adreno_dev->cur_rb, adreno_dev->next_rb);
		KGSL_DRV_ERR(device,
			"Unable to submit preempt token: %d\n", ret);
		adreno_dev->preempt.token_submit = true;
		adreno_dev->cur_rb->wptr_preempt_end = adreno_dev->cur_rb->wptr;
	} else {
		adreno_dev->preempt.token_submit = false;
		adreno_dispatcher_schedule(device);
		adreno_dev->cur_rb->wptr_preempt_end = 0xFFFFFFFF;
	}
}

static void a4xx_preempt_complete_state(struct adreno_device *adreno_dev)

{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int wptr, rbbase;
	unsigned int val, val1;
	unsigned int prevrptr;

	del_timer_sync(&adreno_dev->preempt.timer);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT, &val);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_PREEMPT_DEBUG, &val1);

	if (val || !val1) {
		KGSL_DRV_ERR(device,
		"Invalid state after preemption CP_PREEMPT: %08x, CP_PREEMPT_DEBUG: %08x\n",
		val, val1);
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		adreno_dispatcher_schedule(device);
		return;
	}
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_BASE, &rbbase);
	if (rbbase != adreno_dev->next_rb->buffer_desc.gpuaddr) {
		KGSL_DRV_ERR(device,
		"RBBASE incorrect after preemption, expected %x got %016llx\b",
		rbbase,
		adreno_dev->next_rb->buffer_desc.gpuaddr);
		adreno_set_gpu_fault(adreno_dev, ADRENO_PREEMPT_FAULT);
		adreno_dispatcher_schedule(device);
		return;
	}

	a4xx_preemption_save(adreno_dev, adreno_dev->cur_rb);

	/* new RB is the current RB */
	trace_adreno_hw_preempt_comp_to_clear(adreno_dev->next_rb,
			adreno_dev->cur_rb,
			adreno_get_rptr(adreno_dev->next_rb),
			adreno_get_rptr(adreno_dev->cur_rb));

	adreno_dev->prev_rb = adreno_dev->cur_rb;
	adreno_dev->cur_rb = adreno_dev->next_rb;
	adreno_dev->cur_rb->preempted_midway = 0;
	adreno_dev->cur_rb->wptr_preempt_end = 0xFFFFFFFF;
	adreno_dev->next_rb = NULL;

	if (adreno_disp_preempt_fair_sched) {
		/* starved rb is now scheduled so unhalt dispatcher */
		if (ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED ==
			adreno_dev->cur_rb->starve_timer_state)
			adreno_put_gpu_halt(adreno_dev);
		adreno_dev->cur_rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_SCHEDULED;
		adreno_dev->cur_rb->sched_timer = jiffies;
		/*
		 * If the outgoing RB is has commands then set the
		 * busy time for it
		 */
		if (!adreno_rb_empty(adreno_dev->prev_rb)) {
			adreno_dev->prev_rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_INIT;
			adreno_dev->prev_rb->sched_timer = jiffies;
		} else {
			adreno_dev->prev_rb->starve_timer_state =
				ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT;
		}
	}
	adreno_set_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE);

	prevrptr = adreno_get_rptr(adreno_dev->prev_rb);

	if (adreno_compare_prio_level(adreno_dev->prev_rb->id,
				adreno_dev->cur_rb->id) < 0) {
		if (adreno_dev->prev_rb->wptr_preempt_end != prevrptr)
			adreno_dev->prev_rb->preempted_midway = 1;
	}

	/* submit wptr if required for new rb */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);
	if (adreno_dev->cur_rb->wptr != wptr) {
		kgsl_pwrscale_busy(device);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR,
					adreno_dev->cur_rb->wptr);
	}
	/* clear preemption register */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PREEMPT_DEBUG, 0);
}

void a4xx_preemption_schedule(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return;

	mutex_lock(&device->mutex);

	switch (atomic_read(&adreno_dev->preempt.state)) {
	case ADRENO_PREEMPT_NONE:
		a4xx_preempt_clear_state(adreno_dev);
		break;
	case ADRENO_PREEMPT_TRIGGERED:
		a4xx_preempt_trig_state(adreno_dev);
		/*
		 * if we transitioned to next state then fall-through
		 * processing to next state
		 */
		if (!adreno_in_preempt_state(adreno_dev,
			ADRENO_PREEMPT_COMPLETE))
			break;
	case ADRENO_PREEMPT_COMPLETE:
		a4xx_preempt_complete_state(adreno_dev);
		break;
	default:
		break;
	}

	mutex_unlock(&device->mutex);
}

int a4xx_preemption_init(struct adreno_device *adreno_dev)
{
	setup_timer(&adreno_dev->preempt.timer, a4xx_preemption_timer,
		(unsigned long) adreno_dev);

	return 0;
}
