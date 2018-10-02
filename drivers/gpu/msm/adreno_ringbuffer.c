/* Copyright (c) 2002,2007-2018, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/log2.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"
#include "kgsl_pwrctrl.h"

#include "adreno.h"
#include "adreno_iommu.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"

#include "a3xx_reg.h"
#include "adreno_a5xx.h"

#define RB_HOSTPTR(_rb, _pos) \
	((unsigned int *) ((_rb)->buffer_desc.hostptr + \
		((_pos) * sizeof(unsigned int))))

#define RB_GPUADDR(_rb, _pos) \
	((_rb)->buffer_desc.gpuaddr + ((_pos) * sizeof(unsigned int)))

static inline bool is_internal_cmds(unsigned int flags)
{
	return (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE);
}

static void adreno_get_submit_time(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	unsigned long flags;
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

	/* Read always on registers */
	if (!adreno_is_a3xx(adreno_dev)) {
		adreno_readreg64(adreno_dev,
			ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
			ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
			&time->ticks);

		/* Mask hi bits as they may be incorrect on some targets */
		if (ADRENO_GPUREV(adreno_dev) >= 400 &&
				ADRENO_GPUREV(adreno_dev) <= ADRENO_REV_A530)
			time->ticks &= 0xFFFFFFFF;
	} else
		time->ticks = 0;

	/* Trace the GPU time to create a mapping to ftrace time */
	trace_adreno_cmdbatch_sync(rb->drawctxt_active, time->ticks);

	/* Get the kernel clock for time since boot */
	time->ktime = local_clock();

	/* Get the timeofday for the wall time (for the user) */
	getnstimeofday(&time->utime);

	local_irq_restore(flags);
}

static void adreno_ringbuffer_wptr(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&rb->preempt_lock, flags);
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {

		if (adreno_dev->cur_rb == rb) {
			/*
			 * Let the pwrscale policy know that new commands have
			 * been submitted.
			 */
			kgsl_pwrscale_busy(KGSL_DEVICE(adreno_dev));

			/*
			 * Ensure the write posted after a possible
			 * GMU wakeup (write could have dropped during wakeup)
			 */
			ret = adreno_gmu_fenced_write(adreno_dev,
				ADRENO_REG_CP_RB_WPTR, rb->_wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);

		}
	}

	rb->wptr = rb->_wptr;
	spin_unlock_irqrestore(&rb->preempt_lock, flags);

	if (ret) {
		/* If WPTR update fails, set the fault and trigger recovery */
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
	}

}

static void adreno_profile_submit_time(struct adreno_submit_time *time)
{
	struct kgsl_drawobj *drawobj;
	struct kgsl_drawobj_cmd *cmdobj;
	struct kgsl_mem_entry *entry;

	if (time == NULL)
		return;

	drawobj = time->drawobj;

	if (drawobj == NULL)
		return;

	cmdobj = CMDOBJ(drawobj);
	entry = cmdobj->profiling_buf_entry;

	if (entry) {
		struct kgsl_drawobj_profiling_buffer *profile_buffer;

		profile_buffer = kgsl_gpuaddr_to_vaddr(&entry->memdesc,
					cmdobj->profiling_buffer_gpuaddr);

		if (profile_buffer == NULL)
			return;

		/* Return kernel clock time to the the client if requested */
		if (drawobj->flags & KGSL_DRAWOBJ_PROFILING_KTIME) {
			uint64_t secs = time->ktime;

			profile_buffer->wall_clock_ns =
				do_div(secs, NSEC_PER_SEC);
			profile_buffer->wall_clock_s = secs;
		} else {
			profile_buffer->wall_clock_s = time->utime.tv_sec;
			profile_buffer->wall_clock_ns = time->utime.tv_nsec;
		}

		profile_buffer->gpu_ticks_queued = time->ticks;

		kgsl_memdesc_unmap(&entry->memdesc);
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

int adreno_ringbuffer_submit_spin(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time, unsigned int timeout)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);

	adreno_ringbuffer_submit(rb, time);
	return adreno_spin_idle(adreno_dev, timeout);
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

/**
 * adreno_ringbuffer_start() - Ringbuffer start
 * @adreno_dev: Pointer to adreno device
 * @start_type: Warm or cold start
 */
int adreno_ringbuffer_start(struct adreno_device *adreno_dev,
	unsigned int start_type)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	int i;

	/* Setup the ringbuffers state before we start */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		kgsl_sharedmem_set(device, &(rb->buffer_desc),
				0, 0xAA, KGSL_RB_SIZE);
		if (!adreno_is_a3xx(adreno_dev))
			kgsl_sharedmem_writel(device, &device->scratch,
					SCRATCH_RPTR_OFFSET(rb->id), 0);
		rb->wptr = 0;
		rb->_wptr = 0;
		rb->wptr_preempt_end = 0xFFFFFFFF;
	}

	/* start is specific GPU rb */
	return gpudev->rb_start(adreno_dev, start_type);
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

static int _adreno_ringbuffer_probe(struct adreno_device *adreno_dev,
		int id)
{
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[id];
	int ret;
	char name[64];

	rb->id = id;

	snprintf(name, sizeof(name), "rb_events-%d", id);
	kgsl_add_event_group(&rb->events, NULL, name,
		_rb_readtimestamp, rb);
	rb->timestamp = 0;
	init_waitqueue_head(&rb->ts_expire_waitq);

	spin_lock_init(&rb->preempt_lock);

	/*
	 * Allocate mem for storing RB pagetables and commands to
	 * switch pagetable
	 */
	ret = kgsl_allocate_global(KGSL_DEVICE(adreno_dev), &rb->pagetable_desc,
		PAGE_SIZE, 0, KGSL_MEMDESC_PRIVILEGED, "pagetable_desc");
	if (ret)
		return ret;
	return kgsl_allocate_global(KGSL_DEVICE(adreno_dev), &rb->buffer_desc,
			KGSL_RB_SIZE, KGSL_MEMFLAGS_GPUREADONLY,
			0, "ringbuffer");
}

int adreno_ringbuffer_probe(struct adreno_device *adreno_dev, bool nopreempt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int i;
	int status = -ENOMEM;

	if (!adreno_is_a3xx(adreno_dev)) {
		status = kgsl_allocate_global(device, &device->scratch,
				PAGE_SIZE, 0, 0, "scratch");
		if (status != 0)
			return status;
	}

	if (nopreempt == false && ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		adreno_dev->num_ringbuffers = gpudev->num_prio_levels;
	else
		adreno_dev->num_ringbuffers = 1;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		status = _adreno_ringbuffer_probe(adreno_dev, i);
		if (status != 0)
			break;
	}

	if (status)
		adreno_ringbuffer_close(adreno_dev);
	else
		adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);

	return status;
}

static void _adreno_ringbuffer_close(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_free_global(device, &rb->pagetable_desc);
	kgsl_free_global(device, &rb->preemption_desc);

	kgsl_free_global(device, &rb->buffer_desc);
	kgsl_del_event_group(&rb->events);
	memset(rb, 0, sizeof(struct adreno_ringbuffer));
}

void adreno_ringbuffer_close(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	int i;

	if (!adreno_is_a3xx(adreno_dev))
		kgsl_free_global(device, &device->scratch);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i)
		_adreno_ringbuffer_close(adreno_dev, rb);
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

	if (adreno_is_a4xx(adreno_dev)) {
		cmds += cp_wait_for_idle(adreno_dev, cmds);
		/*
		 * The two commands will stall the PFP until the PFP-ME-AHB
		 * is drained and the GPU is idle. As soon as this happens,
		 * the PFP will start moving again.
		 */
		cmds += cp_wait_for_me(adreno_dev, cmds);

		/*
		 * Below commands are processed by ME. GPU will be
		 * idle when they are processed. But the PFP will continue
		 * to fetch instructions at the same time.
		 */
		*cmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 0;
		*cmds++ = cp_packet(adreno_dev, CP_WIDE_REG_WRITE, 2);
		*cmds++ = adreno_getreg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TRUST_CONTROL);
		*cmds++ = set;
		*cmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 1;

		/* Stall PFP until all above commands are complete */
		cmds += cp_wait_for_me(adreno_dev, cmds);
	} else {
		/*
		 * A5xx has a separate opcode specifically to put the GPU
		 * in and out of secure mode.
		 */
		*cmds++ = cp_packet(adreno_dev, CP_SET_SECURE_MODE, 1);
		*cmds++ = set;
	}

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

static int
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				unsigned int flags, unsigned int *cmds,
				unsigned int sizedwords, uint32_t timestamp,
				struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

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
		if (drawctxt->base.flags & KGSL_CONTEXT_SECURE)
			secured_ctxt = true;
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
	total_sizedwords += flags & KGSL_CMD_FLAGS_PMODE ? 4 : 0;
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
	if (adreno_is_a4xx(adreno_dev) || adreno_is_a3xx(adreno_dev))
		total_sizedwords += 4;

	if (gpudev->preemption_pre_ibsubmit &&
			adreno_is_preemption_enabled(adreno_dev))
		total_sizedwords += 27;

	if (gpudev->preemption_post_ibsubmit &&
			adreno_is_preemption_enabled(adreno_dev))
		total_sizedwords += 10;

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
		/* global timestamp without cache flush for non-zero context */
		total_sizedwords += 4;
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

	*ringcmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*ringcmds++ = KGSL_CMD_IDENTIFIER;

	if (adreno_is_preemption_enabled(adreno_dev) &&
				gpudev->preemption_pre_ibsubmit)
		ringcmds += gpudev->preemption_pre_ibsubmit(
					adreno_dev, rb, ringcmds, context);

	if (is_internal_cmds(flags)) {
		*ringcmds++ = cp_packet(adreno_dev, CP_NOP, 1);
		*ringcmds++ = KGSL_CMD_INTERNAL_IDENTIFIER;
	}

	if (gpudev->set_marker) {
		/* Firmware versions before 1.49 do not support IFPC markers */
		if (adreno_is_a6xx(adreno_dev) && (fw->version & 0xFFF) < 0x149)
			ringcmds += gpudev->set_marker(ringcmds, IB1LIST_START);
		else
			ringcmds += gpudev->set_marker(ringcmds, IFPC_DISABLE);
	}

	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP) {
		/* Disable protected mode for the fixup */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 0;

		*ringcmds++ = cp_packet(adreno_dev, CP_NOP, 1);
		*ringcmds++ = KGSL_PWRON_FIXUP_IDENTIFIER;
		*ringcmds++ = cp_mem_packet(adreno_dev,
				CP_INDIRECT_BUFFER_PFE, 2, 1);
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
				adreno_dev->pwron_fixup.gpuaddr);
		*ringcmds++ = adreno_dev->pwron_fixup_dwords;

		/* Re-enable protected mode */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 1;
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

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* disable protected mode error checking */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 0;
	}

	for (i = 0; i < sizedwords; i++)
		*ringcmds++ = cmds[i];

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* re-enable protected mode error checking */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 1;
	}

	/*
	 * Flush HLSQ lazy updates to make sure there are no
	 * resources pending for indirect loads after the timestamp
	 */
	if (adreno_is_a4xx(adreno_dev) || adreno_is_a3xx(adreno_dev)) {
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
		ringcmds += cp_mem_write(adreno_dev, ringcmds,
			MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp),
			rb->timestamp);
	} else {
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp));
		*ringcmds++ = timestamp;
	}

	if (gpudev->set_marker) {
		if (adreno_is_a6xx(adreno_dev) && (fw->version & 0xFFF) < 0x149)
			ringcmds += gpudev->set_marker(ringcmds, IB1LIST_END);
		else
			ringcmds += gpudev->set_marker(ringcmds, IFPC_ENABLE);
	}

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

	/*
	 * If we have more ringbuffer commands than space reserved
	 * in ringbuffer BUG() to fix this because it will lead to
	 * weird errors.
	 */
	if ((ringcmds - start) > total_sizedwords)
		BUG();
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

		if (!device->l3_clk) {
			KGSL_DEV_ERR_ONCE(device,
				"l3_vote clk not available\n");
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

			ret = clk_set_rate(device->l3_clk,
					device->l3_freq[new_l3]);

			if (!ret)
				device->cur_l3_pwrlevel = new_l3;
			else
				KGSL_DRV_ERR_RATELIMIT(device,
					"Could not set l3_vote: %d\n",
					ret);
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
	 * For a4x and some a5x the alwayson_hi read through CPU
	 * will be masked. Only do 32 bit CP reads for keeping the
	 * numbers consistent
	 */
	if (ADRENO_GPUREV(adreno_dev) >= 400 &&
		ADRENO_GPUREV(adreno_dev) <= ADRENO_REV_A530)
		*p++ = adreno_getreg(adreno_dev,
			ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO);
	else
		*p++ = adreno_getreg(adreno_dev,
			ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO) |
			(1 << 30) | (2 << 18);
	p += cp_gpuaddr(adreno_dev, p, gpuaddr);

	return (unsigned int)(p - cmds);
}

/* adreno_rindbuffer_submitcmd - submit userspace IBs to the GPU */
int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
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
		dwords += 6;

		/*
		 * REG_TO_MEM packet on A5xx and above needs another ordinal.
		 * Add 2 more dwords since we do profiling before and after.
		 */
		if (!ADRENO_LEGACY_PM4(adreno_dev))
			dwords += 2;

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

	/*
	 * Prior to SQE FW version 1.49, there was only one marker for
	 * both preemption and IFPC. Only include the IB1LIST markers if
	 * we are using a firmware that supports them.
	 */
	if (gpudev->set_marker && numibs && adreno_is_a6xx(adreno_dev) &&
			((fw->version & 0xFFF) >= 0x149)) {
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

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_START_OF_IB_IDENTIFIER;

	if (kernel_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			adreno_dev->profile_buffer.gpuaddr +
			ADRENO_DRAWOBJ_PROFILE_OFFSET(cmdobj->profile_index,
				started));
	}

	/*
	 * Add cmds to read the GPU ticks at the start of command obj and
	 * write it into the appropriate command obj profiling buffer offset
	 */
	if (user_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
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
				(ib->priv & MEMOBJ_PREAMBLE &&
				use_preamble == false))
				*cmds++ = cp_mem_packet(adreno_dev, CP_NOP,
						3, 1);

			*cmds++ = cp_mem_packet(adreno_dev,
					CP_INDIRECT_BUFFER_PFE, 2, 1);
			cmds += cp_gpuaddr(adreno_dev, cmds, ib->gpuaddr);
			*cmds++ = (unsigned int) ib->size >> 2;
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
			adreno_dev->profile_buffer.gpuaddr +
			ADRENO_DRAWOBJ_PROFILE_OFFSET(cmdobj->profile_index,
				retired));
	}

	/*
	 * Add cmds to read the GPU ticks at the end of command obj and
	 * write it into the appropriate command obj profiling buffer offset
	 */
	if (user_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			cmdobj->profiling_buffer_gpuaddr +
			offsetof(struct kgsl_drawobj_profiling_buffer,
			gpu_ticks_retired));
	}

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_END_OF_IB_IDENTIFIER;

	/* Context switches commands should *always* be on the GPU */
	ret = adreno_drawctxt_switch(adreno_dev, rb, drawctxt,
		ADRENO_CONTEXT_SWITCH_FORCE_GPU);

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
			KGSL_DRV_ERR(device,
				"Unable to switch draw context: %d\n", ret);
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
