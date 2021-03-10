// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>

#include "a3xx_reg.h"
#include "a5xx_reg.h"
#include "a6xx_reg.h"
#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"


#define RB_HOSTPTR(_rb, _pos) \
	((unsigned int *) ((_rb)->buffer_desc->hostptr + \
		((_pos) * sizeof(unsigned int))))

#define RB_GPUADDR(_rb, _pos) \
	((_rb)->buffer_desc->gpuaddr + ((_pos) * sizeof(unsigned int)))

static inline bool is_internal_cmds(unsigned int flags)
{
	return (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE);
}

static void adreno_get_submit_time(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned long flags;
	struct adreno_context *drawctxt = rb->drawctxt_active;
	struct kgsl_context *context = &drawctxt->base;

	/*
	 * Here we are attempting to create a mapping between the
	 * GPU time domain (alwayson counter) and the CPU time domain
	 * (local_clock) by sampling both values as close together as
	 * possible. This is useful for many types of debugging and
	 * profiling. In order to make this mapping as accurate as
	 * possible, we must turn off interrupts to avoid running
	 * interrupt handlers between the two samples.
	 */

	local_irq_save(flags);

	time->ticks = gpudev->read_alwayson(adreno_dev);

	/* Trace the GPU time to create a mapping to ftrace time */
	trace_adreno_cmdbatch_sync(context->id, context->priority,
		drawctxt->timestamp, time->ticks);

	/* Get the kernel clock for time since boot */
	time->ktime = local_clock();

	/* Get the timeofday for the wall time (for the user) */
	getnstimeofday(&time->utime);

	local_irq_restore(flags);
}

static void adreno_ringbuffer_wptr(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&rb->preempt_lock, flags);
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {

		if (adreno_dev->cur_rb == rb) {
			/*
			 * Let the pwrscale policy know that new commands have
			 * been submitted.
			 */
			kgsl_pwrscale_busy(device);

			/*
			 * Ensure the write posted after a possible
			 * GMU wakeup (write could have dropped during wakeup)
			 */
			ret = adreno_gmu_fenced_write(adreno_dev,
				ADRENO_REG_CP_RB_WPTR, rb->_wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);
			rb->skip_inline_wptr = false;
		}
	} else {
		/*
		 * We skipped inline submission because of preemption state
		 * machine. Set things up so that we write the wptr to the
		 * hardware eventually.
		 */
		if (adreno_dev->cur_rb == rb)
			rb->skip_inline_wptr = true;
	}

	rb->wptr = rb->_wptr;
	spin_unlock_irqrestore(&rb->preempt_lock, flags);

	if (ret) {
		/*
		 * If WPTR update fails, take inline snapshot and trigger
		 * recovery.
		 */
		gmu_fault_snapshot(device);
		adreno_set_gpu_fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
		adreno_dispatcher_schedule(device);
	}
}

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);

	if (time != NULL) {
		adreno_get_submit_time(adreno_dev, rb, time);
		/* Put the timevalues in the profiling buffer */
		adreno_profile_submit_time(time);
	}

	adreno_ringbuffer_wptr(adreno_dev, rb);
}

int adreno_ringbuffer_submit_spin_nosync(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time, unsigned int timeout)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);

	adreno_ringbuffer_submit(rb, time);
	return adreno_spin_idle(adreno_dev, timeout);
}

/*
 * adreno_ringbuffer_submit_spin() - Submit the cmds and wait until GPU is idle
 * @rb: Pointer to ringbuffer
 * @time: Pointer to adreno_submit_time
 * @timeout: timeout value in ms
 *
 * Add commands to the ringbuffer and wait until GPU goes to idle. This routine
 * inserts a WHERE_AM_I packet to trigger a shadow rptr update. So, use
 * adreno_ringbuffer_submit_spin_nosync() if the previous cmd in the RB is a
 * CSY packet because CSY followed by WHERE_AM_I is not legal.
 */
int adreno_ringbuffer_submit_spin(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time, unsigned int timeout)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds;

	/* GPUs which support APRIV feature doesn't require a WHERE_AM_I */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV) ||
			adreno_is_a3xx(adreno_dev))
		return adreno_ringbuffer_submit_spin_nosync(rb, time, timeout);

	cmds = adreno_ringbuffer_allocspace(rb, 3);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_packet(adreno_dev, CP_WHERE_AM_I, 2);
	cmds += cp_gpuaddr(adreno_dev, cmds,
			SCRATCH_RPTR_GPU_ADDR(device, rb->id));

	return adreno_ringbuffer_submit_spin_nosync(rb, time, timeout);
}

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
		unsigned int dwords)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int rptr = adreno_get_rptr(rb);
	unsigned int ret;

	if (rptr <= rb->_wptr) {
		unsigned int *cmds;

		if (rb->_wptr + dwords <= (KGSL_RB_DWORDS - 2)) {
			ret = rb->_wptr;
			rb->_wptr = (rb->_wptr + dwords) % KGSL_RB_DWORDS;
			return RB_HOSTPTR(rb, ret);
		}

		/*
		 * There isn't enough space toward the end of ringbuffer. So
		 * look for space from the beginning of ringbuffer upto the
		 * read pointer.
		 */
		if (dwords < rptr) {
			cmds = RB_HOSTPTR(rb, rb->_wptr);
			*cmds = cp_packet(adreno_dev, CP_NOP,
				KGSL_RB_DWORDS - rb->_wptr - 1);
			rb->_wptr = dwords;
			return RB_HOSTPTR(rb, 0);
		}
	}

	if (rb->_wptr + dwords < rptr) {
		ret = rb->_wptr;
		rb->_wptr = (rb->_wptr + dwords) % KGSL_RB_DWORDS;
		return RB_HOSTPTR(rb, ret);
	}

	return ERR_PTR(-ENOSPC);
}

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb;
	int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i)
		kgsl_cancel_events(KGSL_DEVICE(adreno_dev), &(rb->events));
}

static int _rb_readtimestamp(struct kgsl_device *device,
		void *priv, enum kgsl_timestamp_type type,
		unsigned int *timestamp)
{
	return adreno_rb_readtimestamp(ADRENO_DEVICE(device), priv, type,
		timestamp);
}

static int _adreno_ringbuffer_init(struct adreno_device *adreno_dev,
		int id)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[id];
	unsigned int priv = 0;

	/*
	 * Allocate mem for storing RB pagetables and commands to
	 * switch pagetable
	 */
	if (IS_ERR_OR_NULL(rb->pagetable_desc)) {
		rb->pagetable_desc = kgsl_allocate_global(device, PAGE_SIZE,
			SZ_16K, 0, KGSL_MEMDESC_PRIVILEGED, "pagetable_desc");
		if (IS_ERR(rb->pagetable_desc))
			return PTR_ERR(rb->pagetable_desc);
	}

	/* allocate a chunk of memory to create user profiling IB1s */
	if (IS_ERR_OR_NULL(rb->profile_desc))
		rb->profile_desc = kgsl_allocate_global(device, PAGE_SIZE,
			0, KGSL_MEMFLAGS_GPUREADONLY, 0, "profile_desc");

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv |= KGSL_MEMDESC_PRIVILEGED;

	if (IS_ERR_OR_NULL(rb->buffer_desc)) {
		rb->buffer_desc = kgsl_allocate_global(device, KGSL_RB_SIZE,
			SZ_4K, KGSL_MEMFLAGS_GPUREADONLY, priv, "ringbuffer");
		if (IS_ERR(rb->buffer_desc))
			return PTR_ERR(rb->buffer_desc);
	}

	if (!list_empty(&rb->events.group))
		return 0;

	rb->id = id;
	kgsl_add_event_group(device, &rb->events, NULL, _rb_readtimestamp, rb,
		"rb_events-%d", id);

	rb->timestamp = 0;
	init_waitqueue_head(&rb->ts_expire_waitq);

	spin_lock_init(&rb->preempt_lock);

	return 0;
}

static void adreno_preemption_timer(struct timer_list *t)
{
	struct adreno_preemption *preempt = from_timer(preempt, t, timer);
	struct adreno_device *adreno_dev = container_of(preempt,
						struct adreno_device, preempt);

	/* We should only be here from a triggered state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_FAULTED))
		return;

	/* Schedule the worker to take care of the details */
	queue_work(system_unbound_wq, &adreno_dev->preempt.work);
}

int adreno_ringbuffer_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int i;
	int status = -ENOMEM;

	if (!adreno_is_a3xx(adreno_dev)) {
		unsigned int priv =
			KGSL_MEMDESC_RANDOM | KGSL_MEMDESC_PRIVILEGED;

		if (IS_ERR_OR_NULL(device->scratch)) {
			device->scratch = kgsl_allocate_global(device,
				PAGE_SIZE, 0, 0, priv, "scratch");

			if (IS_ERR(device->scratch))
				return PTR_ERR(device->scratch);
		}
	}

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		adreno_dev->num_ringbuffers =
			ARRAY_SIZE(adreno_dev->ringbuffers);
	else
		adreno_dev->num_ringbuffers = 1;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		status = _adreno_ringbuffer_init(adreno_dev, i);
		if (status) {
			adreno_ringbuffer_close(adreno_dev);
			return status;
		}
	}

	adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION)) {
		const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
		struct adreno_preemption *preempt = &adreno_dev->preempt;
		int ret;

		timer_setup(&preempt->timer, adreno_preemption_timer, 0);

		ret = gpudev->preemption_init(adreno_dev);
		WARN(ret, "adreno GPU preemption is disabled\n");
	}

	return 0;
}

void adreno_ringbuffer_close(struct adreno_device *adreno_dev)
{
	struct adreno_preemption *preempt = &adreno_dev->preempt;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		return;

	del_timer(&preempt->timer);
}

/*
 * cp_secure_mode() - Put GPU in trusted mode
 * @adreno_dev: Pointer to adreno device
 * @cmds: Pointer to cmds to be put in the ringbuffer
 * @set: 1 - secure mode, 0 - unsecure mode
 *
 * Add commands to the ringbuffer to put the GPU in secure mode
 * or unsecure mode based on the variable set.
 */
int cp_secure_mode(struct adreno_device *adreno_dev, uint *cmds,
				int set)
{
	uint *start = cmds;

	/*
	 * A5xx has a separate opcode specifically to put the GPU
	 * in and out of secure mode.
	 */
	*cmds++ = cp_packet(adreno_dev, CP_SET_SECURE_MODE, 1);
	*cmds++ = set;

	return cmds - start;
}

static inline int cp_mem_write(struct adreno_device *adreno_dev,
		unsigned int *cmds, uint64_t gpuaddr, unsigned int value)
{
	int dwords = 0;

	cmds[dwords++] = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
	dwords += cp_gpuaddr(adreno_dev, &cmds[dwords], gpuaddr);
	cmds[dwords++] = value;

	return dwords;
}

static bool _check_secured(struct adreno_context *drawctxt, unsigned int flags)
{
	return ((drawctxt->base.flags & KGSL_CONTEXT_SECURE) &&
		!is_internal_cmds(flags));
}

static int
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				unsigned int flags, unsigned int *cmds,
				unsigned int sizedwords, uint32_t timestamp,
				struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *ringcmds, *start;
	unsigned int total_sizedwords = sizedwords;
	unsigned int i;
	unsigned int context_id = 0;
	bool profile_ready;
	struct adreno_context *drawctxt = rb->drawctxt_active;
	struct kgsl_context *context = NULL;
	bool secured_ctxt = false;
	static unsigned int _seq_cnt;

	if (drawctxt != NULL && kgsl_context_detached(&drawctxt->base) &&
		!is_internal_cmds(flags))
		return -ENOENT;

	/* On fault return error so that we don't keep submitting */
	if (adreno_gpu_fault(adreno_dev) != 0)
		return -EPROTO;

	rb->timestamp++;

	/* If this is a internal IB, use the global timestamp for it */
	if (!drawctxt || is_internal_cmds(flags))
		timestamp = rb->timestamp;
	else {
		context_id = drawctxt->base.id;
		context = &drawctxt->base;
	}

	/*
	 * Note that we cannot safely take drawctxt->mutex here without
	 * potential mutex inversion with device->mutex which is held
	 * here. As a result, any other code that accesses this variable
	 * must also use device->mutex.
	 */
	if (drawctxt) {
		drawctxt->internal_timestamp = rb->timestamp;
		secured_ctxt = _check_secured(drawctxt, flags);
	}

	/*
	 * If in stream ib profiling is enabled and there are counters
	 * assigned, then space needs to be reserved for profiling.  This
	 * space in the ringbuffer is always consumed (might be filled with
	 * NOPs in error case.  profile_ready needs to be consistent through
	 * the _addcmds call since it is allocating additional ringbuffer
	 * command space.
	 */
	profile_ready = drawctxt &&
		adreno_profile_assignments_ready(&adreno_dev->profile) &&
		!is_internal_cmds(flags);

	/*
	 * reserve space to temporarily turn off protected mode
	 * error checking if needed
	 */
	if ((flags & KGSL_CMD_FLAGS_PMODE) &&
		!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		total_sizedwords += 4;

	/* 2 dwords to store the start of command sequence */
	total_sizedwords += 2;
	/* internal ib command identifier for the ringbuffer */
	total_sizedwords += is_internal_cmds(flags) ? 2 : 0;

	total_sizedwords += (secured_ctxt) ? 26 : 0;

	/* _seq mem write for each submission */
	if (adreno_is_a5xx(adreno_dev))
		total_sizedwords += 4;

	/* context rollover */
	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 3;

	/* For HLSQ updates below */
	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 4;

	if (gpudev->preemption_pre_ibsubmit &&
			adreno_is_preemption_enabled(adreno_dev))
		total_sizedwords += 27;

	if (gpudev->preemption_post_ibsubmit &&
			adreno_is_preemption_enabled(adreno_dev))
		total_sizedwords += 10;
	else if (!adreno_is_a3xx(adreno_dev) &&
			!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		total_sizedwords += 3;

	/*
	 * a5xx uses 64 bit memory address. pm4 commands that involve read/write
	 * from memory take 4 bytes more than a4xx because of 64 bit addressing.
	 * This function is shared between gpucores, so reserve the max size
	 * required in ringbuffer and adjust the write pointer depending on
	 * gpucore at the end of this function.
	 */
	total_sizedwords += 8; /* sop timestamp */
	total_sizedwords += 5; /* eop timestamp */

	if (drawctxt && !is_internal_cmds(flags)) {
		/* global timestamp with cache flush ts for non-zero context */
		total_sizedwords += 5;
	}

	if (flags & KGSL_CMD_FLAGS_WFI)
		total_sizedwords += 2; /* WFI */

	if (profile_ready)
		total_sizedwords += 8;   /* space for pre_ib and post_ib */

	/* Add space for the power on shader fixup if we need it */
	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP)
		total_sizedwords += 9;

	/* Don't insert any commands if stall on fault is not supported. */
	if ((ADRENO_GPUREV(adreno_dev) > 500) && !adreno_is_a510(adreno_dev)) {
		/*
		 * WAIT_MEM_WRITES - needed in the stall on fault case
		 * to prevent out of order CP operations that can result
		 * in a CACHE_FLUSH_TS interrupt storm
		 */
		if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
				&adreno_dev->ft_pf_policy))
			total_sizedwords += 1;
	}

	if (gpudev->set_marker)
		total_sizedwords += 4;

	ringcmds = adreno_ringbuffer_allocspace(rb, total_sizedwords);
	if (IS_ERR(ringcmds))
		return PTR_ERR(ringcmds);

	start = ringcmds;

	ringcmds += cp_identifier(adreno_dev, ringcmds, CMD_IDENTIFIER);

	if (adreno_is_preemption_enabled(adreno_dev) &&
				gpudev->preemption_pre_ibsubmit)
		ringcmds += gpudev->preemption_pre_ibsubmit(
					adreno_dev, rb, ringcmds, context);

	if (is_internal_cmds(flags))
		ringcmds += cp_identifier(adreno_dev, ringcmds,
			CMD_INTERNAL_IDENTIFIER);

	if (gpudev->set_marker)
		ringcmds += gpudev->set_marker(ringcmds, IFPC_DISABLE);

	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP) {
		/* Disable protected mode for the fixup */
		ringcmds += cp_protected_mode(adreno_dev, ringcmds, 0);

		ringcmds += cp_identifier(adreno_dev, ringcmds,
			PWRON_FIXUP_IDENTIFIER);
		*ringcmds++ = cp_mem_packet(adreno_dev,
				CP_INDIRECT_BUFFER_PFE, 2, 1);
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
				adreno_dev->pwron_fixup->gpuaddr);
		*ringcmds++ = adreno_dev->pwron_fixup_dwords;

		/* Re-enable protected mode */
		ringcmds += cp_protected_mode(adreno_dev, ringcmds, 1);
	}

	/* Add any IB required for profiling if it is enabled */
	if (profile_ready)
		adreno_profile_preib_processing(adreno_dev, drawctxt,
				&flags, &ringcmds);

	/* start-of-pipeline timestamp for the context */
	if (drawctxt && !is_internal_cmds(flags))
		ringcmds += cp_mem_write(adreno_dev, ringcmds,
			MEMSTORE_ID_GPU_ADDR(device, context_id, soptimestamp),
			timestamp);

	/* start-of-pipeline timestamp for the ringbuffer */
	ringcmds += cp_mem_write(adreno_dev, ringcmds,
		MEMSTORE_RB_GPU_ADDR(device, rb, soptimestamp), rb->timestamp);

	if (secured_ctxt)
		ringcmds += cp_secure_mode(adreno_dev, ringcmds, 1);

	/*
	 * For kernel commands disable protected mode. For user commands turn on
	 * protected mode universally to avoid the possibility that somebody
	 * managed to get this far with protected mode turned off.
	 *
	 * If the target supports apriv control then we don't need this step
	 * since all the permisisons will already be managed for us
	 */

	if ((flags & KGSL_CMD_FLAGS_PMODE) &&
		!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		ringcmds += cp_protected_mode(adreno_dev, ringcmds, 0);

	for (i = 0; i < sizedwords; i++)
		*ringcmds++ = cmds[i];

	/* re-enable protected mode error checking */
	if ((flags & KGSL_CMD_FLAGS_PMODE) &&
			!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		ringcmds += cp_protected_mode(adreno_dev, ringcmds, 1);

	/*
	 * Flush HLSQ lazy updates to make sure there are no
	 * resources pending for indirect loads after the timestamp
	 */
	if (adreno_is_a3xx(adreno_dev)) {
		*ringcmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
		*ringcmds++ = 0x07; /* HLSQ_FLUSH */
		ringcmds += cp_wait_for_idle(adreno_dev, ringcmds);
	}

	/*
	 * Add any postIB required for profiling if it is enabled and has
	 * assigned counters
	 */
	if (profile_ready)
		adreno_profile_postib_processing(adreno_dev, &flags, &ringcmds);

	/* Don't insert any commands if stall on fault is not supported. */
	if ((ADRENO_GPUREV(adreno_dev) > 500) && !adreno_is_a510(adreno_dev)) {
		/*
		 * WAIT_MEM_WRITES - needed in the stall on fault case
		 * to prevent out of order CP operations that can result
		 * in a CACHE_FLUSH_TS interrupt storm
		 */
		if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
				&adreno_dev->ft_pf_policy))
			*ringcmds++ = cp_packet(adreno_dev,
						CP_WAIT_MEM_WRITES, 0);
	}

	/*
	 * Do a unique memory write from the GPU. This can be used in
	 * early detection of timestamp interrupt storms to stave
	 * off system collapse.
	 */
	if (adreno_is_a5xx(adreno_dev))
		ringcmds += cp_mem_write(adreno_dev, ringcmds,
				MEMSTORE_ID_GPU_ADDR(device,
				KGSL_MEMSTORE_GLOBAL,
				ref_wait_ts), ++_seq_cnt);

	/*
	 * end-of-pipeline timestamp.  If per context timestamps is not
	 * enabled, then drawctxt will be NULL or internal command flag will be
	 * set and hence the rb timestamp will be used in else statement below.
	 */
	*ringcmds++ = cp_mem_packet(adreno_dev, CP_EVENT_WRITE, 3, 1);
	if (drawctxt || is_internal_cmds(flags))
		*ringcmds++ = CACHE_FLUSH_TS | (1 << 31);
	else
		*ringcmds++ = CACHE_FLUSH_TS;

	if (drawctxt && !is_internal_cmds(flags)) {
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			MEMSTORE_ID_GPU_ADDR(device, context_id, eoptimestamp));
		*ringcmds++ = timestamp;

		/* Write the end of pipeline timestamp to the ringbuffer too */
		*ringcmds++ = cp_mem_packet(adreno_dev, CP_EVENT_WRITE, 3, 1);
		*ringcmds++ = CACHE_FLUSH_TS;
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp));
		*ringcmds++ = rb->timestamp;
	} else {
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp));
		*ringcmds++ = timestamp;
	}

	if (gpudev->set_marker)
		ringcmds += gpudev->set_marker(ringcmds, IFPC_ENABLE);

	if (adreno_is_a3xx(adreno_dev)) {
		/* Dummy set-constant to trigger context rollover */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_CONSTANT, 2);
		*ringcmds++ =
			(0x4<<16) | (A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000);
		*ringcmds++ = 0;
	}

	if (flags & KGSL_CMD_FLAGS_WFI)
		ringcmds += cp_wait_for_idle(adreno_dev, ringcmds);

	if (secured_ctxt)
		ringcmds += cp_secure_mode(adreno_dev, ringcmds, 0);

	if (gpudev->preemption_post_ibsubmit &&
				adreno_is_preemption_enabled(adreno_dev))
		ringcmds += gpudev->preemption_post_ibsubmit(adreno_dev,
			ringcmds);
	else if (!adreno_is_a3xx(adreno_dev) &&
			!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV)) {
		*ringcmds++ = cp_packet(adreno_dev, CP_WHERE_AM_I, 2);
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
				SCRATCH_RPTR_GPU_ADDR(device, rb->id));
	}

	/*
	 * If we have more ringbuffer commands than space reserved
	 * in ringbuffer BUG() to fix this because it will lead to
	 * weird errors.
	 */
	BUG_ON((ringcmds - start) > total_sizedwords);

	/*
	 *  Allocate total_sizedwords space in RB, this is the max space
	 *  required. If we have commands less than the space reserved in RB
	 *  adjust the wptr accordingly.
	 */
	rb->_wptr = rb->_wptr - (total_sizedwords - (ringcmds - start));

	adreno_ringbuffer_submit(rb, time);

	return 0;
}

int
adreno_ringbuffer_issue_internal_cmds(struct adreno_ringbuffer *rb,
				unsigned int flags,
				unsigned int *cmds,
				int sizedwords)
{
	flags |= KGSL_CMD_FLAGS_INTERNAL_ISSUE;

	return adreno_ringbuffer_addcmds(rb, flags, cmds,
		sizedwords, 0, NULL);
}

static void adreno_ringbuffer_set_constraint(struct kgsl_device *device,
			struct kgsl_drawobj *drawobj)
{
	struct kgsl_context *context = drawobj->context;
	unsigned long flags = drawobj->flags;

	/*
	 * Check if the context has a constraint and constraint flags are
	 * set.
	 */
	if (context->pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(drawobj->flags & KGSL_CONTEXT_PWR_CONSTRAINT)))
		kgsl_pwrctrl_set_constraint(device, &context->pwr_constraint,
						context->id);

	if (context->l3_pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(flags & KGSL_CONTEXT_PWR_CONSTRAINT))) {

		if (IS_ERR(device->l3_icc)) {
			dev_err_once(device->dev,
				"l3_icc path not available\n");
			return;
		}

		switch (context->l3_pwr_constraint.type) {
		case KGSL_CONSTRAINT_L3_PWRLEVEL: {
			unsigned int sub_type;
			unsigned int new_l3;
			int ret = 0;

			sub_type = context->l3_pwr_constraint.sub_type;

			/*
			 * If an L3 constraint is already set, set the new
			 * one only if it is higher.
			 */
			new_l3 = max_t(unsigned int, sub_type + 1,
					device->cur_l3_pwrlevel);
			new_l3 = min_t(unsigned int, new_l3,
					device->num_l3_pwrlevels - 1);

			if (device->cur_l3_pwrlevel == new_l3)
				return;

			ret = icc_set_bw(device->l3_icc, 0,
					device->l3_freq[new_l3]);

			if (!ret) {
				trace_kgsl_constraint(device,
					KGSL_CONSTRAINT_L3_PWRLEVEL, new_l3, 1);
				device->cur_l3_pwrlevel = new_l3;
			} else {
				dev_err_ratelimited(device->dev,
						       "Could not set l3_vote: %d\n",
						       ret);
			}
			break;
			}
		}
	}
}

static inline int _get_alwayson_counter(struct adreno_device *adreno_dev,
		unsigned int *cmds, uint64_t gpuaddr)
{
	unsigned int *p = cmds;

	*p++ = cp_mem_packet(adreno_dev, CP_REG_TO_MEM, 2, 1);

	/*
	 * For some a5x the alwayson_hi read through CPU
	 * will be masked. Only do 32 bit CP reads for keeping the
	 * numbers consistent
	 */
	if (adreno_is_a5xx(adreno_dev)) {
		if (ADRENO_GPUREV(adreno_dev) <= ADRENO_REV_A530)
			*p++ = A5XX_RBBM_ALWAYSON_COUNTER_LO;
		else
			*p++ = A5XX_RBBM_ALWAYSON_COUNTER_LO |
				(1 << 30) | (2 << 18);
	} else if (adreno_is_a6xx(adreno_dev)) {
		*p++ = A6XX_CP_ALWAYS_ON_COUNTER_LO |
			(1 << 30) | (2 << 18);
	}

	p += cp_gpuaddr(adreno_dev, p, gpuaddr);

	return (unsigned int)(p - cmds);
}

/* This is the maximum possible size for 64 bit targets */
#define PROFILE_IB_DWORDS 4
#define PROFILE_IB_SLOTS (PAGE_SIZE / (PROFILE_IB_DWORDS << 2))

static int set_user_profiling(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, u32 *cmds, u64 gpuaddr)
{
	int dwords, index = 0;
	u64 ib_gpuaddr;
	u32 *ib;

	if (IS_ERR(rb->profile_desc))
		return 0;

	ib = ((u32 *) rb->profile_desc->hostptr) +
		(rb->profile_index * PROFILE_IB_DWORDS);
	ib_gpuaddr = rb->profile_desc->gpuaddr +
		(rb->profile_index * (PROFILE_IB_DWORDS << 2));

	dwords = _get_alwayson_counter(adreno_dev, ib, gpuaddr);

	/* Make an indirect buffer for the request */
	cmds[index++] = cp_mem_packet(adreno_dev, CP_INDIRECT_BUFFER_PFE, 2, 1);
	index += cp_gpuaddr(adreno_dev, &cmds[index], ib_gpuaddr);
	cmds[index++] = dwords;

	rb->profile_index = (rb->profile_index + 1) % PROFILE_IB_SLOTS;

	return index;
}

/* adreno_rindbuffer_submitcmd - submit userspace IBs to the GPU */
int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct kgsl_memobj_node *ib;
	unsigned int numibs = 0;
	unsigned int *link;
	unsigned int *cmds;
	struct kgsl_context *context;
	struct adreno_context *drawctxt;
	bool use_preamble = true;
	bool user_profiling = false;
	bool kernel_profiling = false;
	int flags = KGSL_CMD_FLAGS_NONE;
	int ret;
	struct adreno_ringbuffer *rb;
	unsigned int dwords = 0;
	struct adreno_submit_time local;
	bool set_ib1list_marker = false;

	memset(&local, 0x0, sizeof(local));

	context = drawobj->context;
	drawctxt = ADRENO_CONTEXT(context);

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdobj->cmdlist, node)
		numibs++;

	rb = drawctxt->rb;

	/* process any profiling results that are available into the log_buf */
	adreno_profile_process_results(adreno_dev);

	/*
	 * If SKIP CMD flag is set for current context
	 * a) set SKIPCMD as fault_recovery for current commandbatch
	 * b) store context's commandbatch fault_policy in current
	 *    commandbatch fault_policy and clear context's commandbatch
	 *    fault_policy
	 * c) force preamble for commandbatch
	 */
	if (test_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv) &&
		(!test_bit(CMDOBJ_SKIP, &cmdobj->priv))) {

		set_bit(KGSL_FT_SKIPCMD, &cmdobj->fault_recovery);
		cmdobj->fault_policy = drawctxt->fault_policy;
		set_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv);

		/* if context is detached print fault recovery */
		adreno_fault_skipcmd_detached(adreno_dev, drawctxt, drawobj);

		/* clear the drawctxt flags */
		clear_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv);
		drawctxt->fault_policy = 0;
	}

	/*
	 * When preamble is enabled, the preamble buffer with state restoration
	 * commands are stored in the first node of the IB chain.
	 * We can skip that if a context switch hasn't occurred.
	 */

	if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) &&
		!test_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv) &&
		(rb->drawctxt_active == drawctxt))
		use_preamble = false;

	/*
	 * In skip mode don't issue the draw IBs but keep all the other
	 * accoutrements of a submision (including the interrupt) to keep
	 * the accounting sane. Set start_index and numibs to 0 to just
	 * generate the start and end markers and skip everything else
	 */
	if (test_bit(CMDOBJ_SKIP, &cmdobj->priv)) {
		use_preamble = false;
		numibs = 0;
	}

	/*
	 * a5xx uses 64 bit memory address. pm4 commands that involve read/write
	 * from memory take 4 bytes more than a4xx because of 64 bit addressing.
	 * This function is shared between gpucores, so reserve the max size
	 * required and adjust the number of commands before calling addcmds.
	 * Each submission needs 7 dwords max for wrappers and other red tape.
	 */
	dwords = 7;

	/* Each IB takes up 30 dwords in worst case */
	dwords += (numibs * 30);

	if (drawobj->flags & KGSL_DRAWOBJ_PROFILING &&
		!adreno_is_a3xx(adreno_dev) &&
		(cmdobj->profiling_buf_entry != NULL)) {
		user_profiling = true;

		/*
		 * User side profiling uses two IB1s, one before with 4 dwords
		 * per INDIRECT_BUFFER_PFE call
		 */
		dwords += 8;

		/*
		 * we want to use an adreno_submit_time struct to get the
		 * precise moment when the command is submitted to the
		 * ringbuffer.  If an upstream caller already passed down a
		 * pointer piggyback on that otherwise use a local struct
		 */

		if (time == NULL)
			time = &local;

		time->drawobj = drawobj;
	}

	if (test_bit(CMDOBJ_PROFILE, &cmdobj->priv)) {
		kernel_profiling = true;
		dwords += 6;
		if (!ADRENO_LEGACY_PM4(adreno_dev))
			dwords += 2;
	}

	if (adreno_is_preemption_enabled(adreno_dev))
		if (gpudev->preemption_yield_enable)
			dwords += 8;

	if (gpudev->set_marker && numibs) {
		set_ib1list_marker = true;
		dwords += 4;
	}

	if (gpudev->ccu_invalidate)
		dwords += 4;

	link = kcalloc(dwords, sizeof(unsigned int), GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto done;
	}

	cmds = link;

	cmds += cp_identifier(adreno_dev, cmds, START_IB_IDENTIFIER);

	if (kernel_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			adreno_dev->profile_buffer->gpuaddr +
			ADRENO_DRAWOBJ_PROFILE_OFFSET(cmdobj->profile_index,
				started));
	}

	/*
	 * Add IB1 to read the GPU ticks at the start of command obj and
	 * write it into the appropriate command obj profiling buffer offset
	 */
	if (user_profiling) {
		cmds += set_user_profiling(adreno_dev, rb, cmds,
			cmdobj->profiling_buffer_gpuaddr +
			offsetof(struct kgsl_drawobj_profiling_buffer,
			gpu_ticks_submitted));
	}

	if (numibs) {
		if (set_ib1list_marker)
			cmds += gpudev->set_marker(cmds, IB1LIST_START);

		list_for_each_entry(ib, &cmdobj->cmdlist, node) {
			/*
			 * Skip 0 sized IBs - these are presumed to have been
			 * removed from consideration by the FT policy
			 */
			if (ib->priv & MEMOBJ_SKIP ||
				(ib->priv & MEMOBJ_PREAMBLE && !use_preamble))
				*cmds++ = cp_mem_packet(adreno_dev, CP_NOP,
						3, 1);

			*cmds++ = cp_mem_packet(adreno_dev,
					CP_INDIRECT_BUFFER_PFE, 2, 1);
			cmds += cp_gpuaddr(adreno_dev, cmds, ib->gpuaddr);
			/*
			 * Never allow bit 20 (IB_PRIV) to be set. All IBs MUST
			 * run at reduced privilege
			 */
			*cmds++ = (unsigned int) ((ib->size >> 2) & 0xfffff);

			/* preamble is required on only for first command */
			use_preamble = false;
		}

		if (set_ib1list_marker)
			cmds += gpudev->set_marker(cmds, IB1LIST_END);
	}

	if (gpudev->ccu_invalidate)
		cmds += gpudev->ccu_invalidate(adreno_dev, cmds);

	if (adreno_is_preemption_enabled(adreno_dev))
		if (gpudev->preemption_yield_enable)
			cmds += gpudev->preemption_yield_enable(cmds);

	if (kernel_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			adreno_dev->profile_buffer->gpuaddr +
			ADRENO_DRAWOBJ_PROFILE_OFFSET(cmdobj->profile_index,
				retired));
	}

	/*
	 * Add IB1 to read the GPU ticks at the end of command obj and
	 * write it into the appropriate command obj profiling buffer offset
	 */
	if (user_profiling) {
		cmds += set_user_profiling(adreno_dev, rb, cmds,
			cmdobj->profiling_buffer_gpuaddr +
			offsetof(struct kgsl_drawobj_profiling_buffer,
			gpu_ticks_retired));
	}

	cmds += cp_identifier(adreno_dev, cmds, END_IB_IDENTIFIER);

	/* Context switches commands should *always* be on the GPU */
	ret = adreno_drawctxt_switch(adreno_dev, rb, drawctxt);

	/*
	 * In the unlikely event of an error in the drawctxt switch,
	 * treat it like a hang
	 */
	if (ret) {
		/*
		 * It is "normal" to get a -ENOSPC or a -ENOENT. Don't log it,
		 * the upper layers know how to handle it
		 */
		if (ret != -ENOSPC && ret != -ENOENT)
			dev_err(device->dev,
				     "Unable to switch draw context: %d\n",
				     ret);
		goto done;
	}

	if (test_bit(CMDOBJ_WFI, &cmdobj->priv))
		flags = KGSL_CMD_FLAGS_WFI;

	/*
	 * For some targets, we need to execute a dummy shader operation after a
	 * power collapse
	 */

	if (test_and_clear_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv) &&
		test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
		flags |= KGSL_CMD_FLAGS_PWRON_FIXUP;

	/* Set the constraints before adding to ringbuffer */
	adreno_ringbuffer_set_constraint(device, drawobj);

	ret = adreno_ringbuffer_addcmds(rb, flags,
					&link[0], (cmds - link),
					drawobj->timestamp, time);

	if (!ret) {
		set_bit(KGSL_CONTEXT_PRIV_SUBMITTED, &context->priv);
		cmdobj->global_ts = drawctxt->internal_timestamp;
	}

done:
	trace_kgsl_issueibcmds(device, context->id, numibs, drawobj->timestamp,
			drawobj->flags, ret, drawctxt->type);

	kfree(link);
	return ret;
}

/**
 * adreno_ringbuffer_wait_callback() - Callback function for event registered
 * on a ringbuffer timestamp
 * @device: Device for which the the callback is valid
 * @context: The context of the event
 * @priv: The private parameter of the event
 * @result: Result of the event trigger
 */
static void adreno_ringbuffer_wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group,
		void *priv, int result)
{
	struct adreno_ringbuffer *rb = group->priv;

	wake_up_all(&rb->ts_expire_waitq);
}

/* check if timestamp is greater than the current rb timestamp */
static inline int adreno_ringbuffer_check_timestamp(
			struct adreno_ringbuffer *rb,
			unsigned int timestamp, int type)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int ts;

	adreno_rb_readtimestamp(adreno_dev, rb, type, &ts);
	return (timestamp_cmp(ts, timestamp) >= 0);
}


/**
 * adreno_ringbuffer_waittimestamp() - Wait for a RB timestamp
 * @rb: The ringbuffer to wait on
 * @timestamp: The timestamp to wait for
 * @msecs: The wait timeout period
 */
int adreno_ringbuffer_waittimestamp(struct adreno_ringbuffer *rb,
					unsigned int timestamp,
					unsigned int msecs)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;
	unsigned long wait_time;

	/* check immediately if timeout is 0 */
	if (msecs == 0)
		return adreno_ringbuffer_check_timestamp(rb,
			timestamp, KGSL_TIMESTAMP_RETIRED) ? 0 : -EBUSY;

	ret = kgsl_add_event(device, &rb->events, timestamp,
		adreno_ringbuffer_wait_callback, NULL);
	if (ret)
		return ret;

	mutex_unlock(&device->mutex);

	wait_time = msecs_to_jiffies(msecs);
	if (wait_event_timeout(rb->ts_expire_waitq,
		!kgsl_event_pending(device, &rb->events, timestamp,
				adreno_ringbuffer_wait_callback, NULL),
		wait_time) == 0)
		ret  = -ETIMEDOUT;

	mutex_lock(&device->mutex);
	/*
	 * after wake up make sure that expected timestamp has retired
	 * because the wakeup could have happened due to a cancel event
	 */
	if (!ret && !adreno_ringbuffer_check_timestamp(rb,
		timestamp, KGSL_TIMESTAMP_RETIRED)) {
		ret = -EAGAIN;
	}

	return ret;
}
