/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
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
#include <linux/log2.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "kgsl_trace.h"
#include "kgsl_pwrctrl.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"

#include "a3xx_reg.h"
#include "adreno_a5xx.h"

#define GSL_RB_NOP_SIZEDWORDS				2

#define ADRENO_RB_PREEMPT_TOKEN_IB_DWORDS	50
#define ADRENO_RB_PREEMPT_TOKEN_DWORDS		125

#define RB_HOSTPTR(_rb, _pos) \
	((unsigned int *) ((_rb)->buffer_desc.hostptr + \
		((_pos) * sizeof(unsigned int))))

#define RB_GPUADDR(_rb, _pos) \
	((_rb)->buffer_desc.gpuaddr + ((_pos) * sizeof(unsigned int)))

static void _cff_write_ringbuffer(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	struct kgsl_device *device = &adreno_dev->dev;
	uint64_t gpuaddr;
	unsigned int *hostptr;
	size_t size;

	if (device->cff_dump_enable == 0)
		return;

	/*
	 * This code is predicated on the fact that we write a full block of
	 * stuff without wrapping
	 */
	BUG_ON(rb->wptr < rb->last_wptr);

	size = (rb->wptr - rb->last_wptr) * sizeof(unsigned int);

	hostptr = RB_HOSTPTR(rb, rb->last_wptr);
	gpuaddr = RB_GPUADDR(rb, rb->last_wptr);

	kgsl_cffdump_memcpy(device, gpuaddr, hostptr, size);
}

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	BUG_ON(rb->wptr == 0);

	/* Write the changes to CFF if so enabled */
	_cff_write_ringbuffer(rb);

	/*
	 * Read the current GPU ticks and wallclock for most accurate
	 * profiling
	 */

	if (time != NULL) {
		/*
		 * Here we are attempting to create a mapping between the
		 * GPU time domain (alwayson counter) and the CPU time domain
		 * (local_clock) by sampling both values as close together as
		 * possible. This is useful for many types of debugging and
		 * profiling. In order to make this mapping as accurate as
		 * possible, we must turn off interrupts to avoid running
		 * interrupt handlers between the two samples.
		 */
		unsigned long flags;
		local_irq_save(flags);

		/* Read always on registers */
		if (!adreno_is_a3xx(adreno_dev))
			adreno_readreg64(adreno_dev,
				ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
				ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
				&time->ticks);
		else
			time->ticks = 0;

		/* Get the kernel clock for time since boot */
		time->ktime = local_clock();

		/* Get the timeofday for the wall time (for the user) */
		getnstimeofday(&time->utime);

		local_irq_restore(flags);
	}

	/* Memory barrier before informing the hardware of new commands */
	mb();

	if (adreno_preempt_state(adreno_dev, ADRENO_DISPATCHER_PREEMPT_CLEAR) &&
		(adreno_dev->cur_rb == rb)) {
		/*
		 * Let the pwrscale policy know that new commands have
		 * been submitted.
		 */
		kgsl_pwrscale_busy(rb->device);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR, rb->wptr);
	}
}

static int
adreno_ringbuffer_waitspace(struct adreno_ringbuffer *rb,
				unsigned int numcmds, int wptr_ahead)
{
	int nopcount = 0;
	unsigned int freecmds;
	unsigned int wptr = rb->wptr;
	unsigned int *cmds = NULL;
	uint64_t gpuaddr;
	unsigned long wait_time;
	unsigned long wait_timeout = msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);
	unsigned int rptr;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	/* if wptr ahead, fill the remaining with NOPs */
	if (wptr_ahead) {
		/* -1 for header */
		nopcount = KGSL_RB_DWORDS - rb->wptr - 1;

		cmds = RB_HOSTPTR(rb, rb->wptr);
		gpuaddr = RB_GPUADDR(rb, rb->wptr);

		rptr = adreno_get_rptr(rb);
		/* For non current rb we don't expect the rptr to move */
		if ((adreno_dev->cur_rb != rb ||
				!adreno_preempt_state(adreno_dev,
				ADRENO_DISPATCHER_PREEMPT_CLEAR)) &&
			!rptr)
			return -ENOSPC;

		/* Make sure that rptr is not 0 before submitting
		 * commands at the end of ringbuffer. We do not
		 * want the rptr and wptr to become equal when
		 * the ringbuffer is not empty */
		wait_time = jiffies + wait_timeout;
		while (!rptr) {
			rptr = adreno_get_rptr(rb);
			if (time_after(jiffies, wait_time))
				return -ETIMEDOUT;
		}

		rb->wptr = 0;
	}

	rptr = adreno_get_rptr(rb);
	freecmds = rptr - rb->wptr;
	if (freecmds == 0 || freecmds > numcmds)
		goto done;

	/* non current rptr will not advance anyway or if preemption underway */
	if (adreno_dev->cur_rb != rb ||
		!adreno_preempt_state(adreno_dev,
			ADRENO_DISPATCHER_PREEMPT_CLEAR)) {
		rb->wptr = wptr;
		return -ENOSPC;
	}

	wait_time = jiffies + wait_timeout;
	/* wait for space in ringbuffer */
	while (1) {
		rptr = adreno_get_rptr(rb);

		freecmds = rptr - rb->wptr;

		if (freecmds == 0 || freecmds > numcmds)
			break;

		if (time_after(jiffies, wait_time)) {
			KGSL_DRV_ERR(rb->device,
			"Timed out waiting for freespace in RB rptr: 0x%x, wptr: 0x%x, rb id %d\n",
			rptr, wptr, rb->id);
			return -ETIMEDOUT;
		}
	}
done:
	if (wptr_ahead) {
		*cmds = cp_packet(adreno_dev, CP_NOP, nopcount);
		kgsl_cffdump_write(rb->device, gpuaddr, *cmds);

	}
	return 0;
}

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
					unsigned int numcmds)
{
	unsigned int *ptr = NULL;
	int ret = 0;
	unsigned int rptr;
	BUG_ON(numcmds >= KGSL_RB_DWORDS);

	rptr = adreno_get_rptr(rb);
	/* check for available space */
	if (rb->wptr >= rptr) {
		/* wptr ahead or equal to rptr */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (KGSL_RB_DWORDS -
				GSL_RB_NOP_SIZEDWORDS))
			ret = adreno_ringbuffer_waitspace(rb, numcmds, 1);
	} else {
		/* wptr behind rptr */
		if ((rb->wptr + numcmds) >= rptr)
			ret = adreno_ringbuffer_waitspace(rb, numcmds, 0);
		/* check for remaining space */
		/* reserve dwords for nop packet */
		if (!ret && (rb->wptr + numcmds) > (KGSL_RB_DWORDS -
				GSL_RB_NOP_SIZEDWORDS))
			ret = adreno_ringbuffer_waitspace(rb, numcmds, 1);
	}

	if (!ret) {
		rb->last_wptr = rb->wptr;

		ptr = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		rb->wptr += numcmds;
	} else
		ptr = ERR_PTR(ret);

	return ptr;
}

/**
 * _ringbuffer_setup_common() - Ringbuffer start
 * @rb: Pointer to adreno ringbuffer
 *
 * Setup ringbuffer for GPU.
 */
static void _ringbuffer_setup_common(struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb_temp;
	int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb_temp, i) {
		kgsl_sharedmem_set(rb_temp->device,
			&(rb_temp->buffer_desc), 0,
			0xAA, KGSL_RB_SIZE);
		rb_temp->wptr = 0;
		rb_temp->rptr = 0;
		rb_temp->wptr_preempt_end = 0xFFFFFFFF;
		rb_temp->starve_timer_state =
		ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT;
		adreno_iommu_set_pt_generate_rb_cmds(rb_temp,
					device->mmu.defaultpagetable);
	}

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 * Also disable the host RPTR shadow register as it might be unreliable
	 * in certain circumstances.
	 */

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL,
		(ilog2(KGSL_RB_DWORDS >> 1) & 0x3F) |
		(1 << 27));

	adreno_writereg64(adreno_dev, ADRENO_REG_CP_RB_BASE,
			  ADRENO_REG_CP_RB_BASE_HI, rb->buffer_desc.gpuaddr);

	/* CP ROQ queue sizes (bytes) - RB:16, ST:16, IB1:32, IB2:64 */
	if (adreno_is_a305(adreno_dev) || adreno_is_a305c(adreno_dev) ||
		adreno_is_a306(adreno_dev) || adreno_is_a320(adreno_dev) ||
		adreno_is_a304(adreno_dev))
		kgsl_regwrite(device, A3XX_CP_QUEUE_THRESHOLDS, 0x000E0602);
	else if (adreno_is_a330(adreno_dev) || adreno_is_a305b(adreno_dev) ||
			adreno_is_a310(adreno_dev))
		kgsl_regwrite(device, A3XX_CP_QUEUE_THRESHOLDS, 0x003E2008);
}

/**
 * _ringbuffer_start_common() - Ringbuffer start
 * @rb: Pointer to adreno ringbuffer
 *
 * Start ringbuffer for GPU.
 */
static int _ringbuffer_start_common(struct adreno_ringbuffer *rb)
{
	int status;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* clear ME_HALT to start micro engine */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

	/* ME init is GPU specific, so jump into the sub-function */
	status = gpudev->rb_init(adreno_dev, rb);
	if (status)
		return status;

	/* idle device to validate ME INIT */
	status = adreno_spin_idle(device);
	if (status) {
		KGSL_DRV_ERR(rb->device,
		"ringbuffer initialization failed to idle\n");
		kgsl_device_snapshot(device, NULL);
	}
	return status;
}

/**
 * adreno_ringbuffer_warm_start() - Ringbuffer warm start
 * @adreno_dev: Pointer to adreno device
 *
 * Start the ringbuffer but load only jump tables part of the
 * microcode. Only need to start the current active ringbuffer
 * do not mess with inactive ringbuffers state because they
 * could contain valid commands.
 */
int adreno_ringbuffer_warm_start(struct adreno_device *adreno_dev)
{
	int status;
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	_ringbuffer_setup_common(rb);

	status = gpudev->microcode_load(adreno_dev, ADRENO_START_WARM);
	if (status)
		return status;

	return _ringbuffer_start_common(rb);
}

/**
 * adreno_ringbuffer_cold_start() - Ringbuffer cold start
 * @adreno_dev: Pointer to adreno device
 *
 * Start the ringbuffers from power collapse. All ringbuffers are started.
 */
int adreno_ringbuffer_cold_start(struct adreno_device *adreno_dev)
{
	int status;
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	_ringbuffer_setup_common(rb);

	status = gpudev->microcode_load(adreno_dev, ADRENO_START_COLD);
	if (status)
		return status;

	return _ringbuffer_start_common(rb);
}

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_ringbuffer *rb;
	int i;
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i)
		kgsl_cancel_events(device, &(rb->events));
}

static int _adreno_ringbuffer_init(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb, int id)
{
	int ret;
	char name[64];

	rb->device = &adreno_dev->dev;
	rb->id = id;

	snprintf(name, sizeof(name), "rb_events-%d", id);
	kgsl_add_event_group(&rb->events, NULL, name,
		adreno_rb_readtimestamp, rb);
	rb->timestamp = 0;
	init_waitqueue_head(&rb->ts_expire_waitq);

	/*
	 * Allocate mem for storing RB pagetables and commands to
	 * switch pagetable
	 */
	ret = kgsl_allocate_global(&adreno_dev->dev, &rb->pagetable_desc,
		PAGE_SIZE, 0, KGSL_MEMDESC_PRIVILEGED);
	if (ret)
		return ret;

	ret = kgsl_allocate_global(&adreno_dev->dev, &rb->buffer_desc,
			KGSL_RB_SIZE, KGSL_MEMFLAGS_GPUREADONLY, 0);
	return ret;
}

int adreno_ringbuffer_init(struct kgsl_device *device)
{
	int status = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	int i;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		adreno_dev->num_ringbuffers = gpudev->num_prio_levels;
	else
		adreno_dev->num_ringbuffers = 1;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		status = _adreno_ringbuffer_init(adreno_dev, rb, i);
		if (status)
			break;
	}
	if (status)
		adreno_ringbuffer_close(adreno_dev);
	else
		adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);

	return status;
}

static void _adreno_ringbuffer_close(struct adreno_ringbuffer *rb)
{
	kgsl_free_global(&rb->pagetable_desc);
	kgsl_free_global(&rb->preemption_desc);

	memset(&rb->pt_update_desc, 0, sizeof(struct kgsl_memdesc));

	kgsl_free_global(&rb->buffer_desc);
	kgsl_del_event_group(&rb->events);
	memset(rb, 0, sizeof(struct adreno_ringbuffer));
}

void adreno_ringbuffer_close(struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb;
	int i;

	kfree(adreno_dev->pfp_fw);
	kfree(adreno_dev->pm4_fw);

	adreno_dev->pfp_fw = NULL;
	adreno_dev->pm4_fw = NULL;

	kgsl_free_global(&adreno_dev->pm4);
	kgsl_free_global(&adreno_dev->pfp);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i)
		_adreno_ringbuffer_close(rb);
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
static int cp_secure_mode(struct adreno_device *adreno_dev, uint *cmds, int set)
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

static int
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				unsigned int flags, unsigned int *cmds,
				unsigned int sizedwords, uint32_t timestamp,
				struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	unsigned int *ringcmds, *start;
	unsigned int total_sizedwords = sizedwords;
	unsigned int i;
	unsigned int context_id = 0;
	uint64_t gpuaddr = rb->device->memstore.gpuaddr;
	bool profile_ready;
	struct adreno_context *drawctxt = rb->drawctxt_active;
	bool secured_ctxt = false;

	if (drawctxt != NULL && kgsl_context_detached(&drawctxt->base) &&
		!(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE))
		return -ENOENT;

	rb->timestamp++;

	/* If this is a internal IB, use the global timestamp for it */
	if (!drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE))
		timestamp = rb->timestamp;
	else
		context_id = drawctxt->base.id;

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
		!(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE);

	/* reserve space to temporarily turn off protected mode
	*  error checking if needed
	*/
	total_sizedwords += flags & KGSL_CMD_FLAGS_PMODE ? 4 : 0;
	/* 2 dwords to store the start of command sequence */
	total_sizedwords += 2;
	/* internal ib command identifier for the ringbuffer */
	total_sizedwords += (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE) ? 2 : 0;

	total_sizedwords += (secured_ctxt) ? 26 : 0;

	/* Add two dwords for the CP_INTERRUPT */
	total_sizedwords +=
		(drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) ?  2 : 0;

	/* context rollover */
	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 3;

	/* For HLSQ updates below */
	if (adreno_is_a4xx(adreno_dev) || adreno_is_a3xx(adreno_dev))
		total_sizedwords += 4;

	/*
	 * a5xx uses 64 bit memory address. pm4 commands that involve read/write
	 * from memory take 4 bytes more than a4xx because of 64 bit addressing.
	 * This function is shared between gpucores, so reserve the max size
	 * required in ringbuffer and adjust the write pointer depending on
	 * gpucore at the end of this function.
	 */
	total_sizedwords += 4; /* sop timestamp */
	total_sizedwords += 5; /* eop timestamp */

	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		total_sizedwords += 4; /* global timestamp without cache
					* flush for non-zero context */
	}

	if (flags & KGSL_CMD_FLAGS_WFI)
		total_sizedwords += 2; /* WFI */

	if (profile_ready)
		total_sizedwords += 8;   /* space for pre_ib and post_ib */

	/* Add space for the power on shader fixup if we need it */
	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP)
		total_sizedwords += 9;

	ringcmds = adreno_ringbuffer_allocspace(rb, total_sizedwords);
	if (IS_ERR(ringcmds))
		return PTR_ERR(ringcmds);

	start = ringcmds;

	*ringcmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*ringcmds++ = KGSL_CMD_IDENTIFIER;

	if (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE) {
		*ringcmds++ = cp_packet(adreno_dev, CP_NOP, 1);
		*ringcmds++ = KGSL_CMD_INTERNAL_IDENTIFIER;
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

	/* start-of-pipeline timestamp */
	*ringcmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE))
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			gpuaddr + KGSL_MEMSTORE_OFFSET(context_id,
			soptimestamp));
	else
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds,
			gpuaddr + KGSL_MEMSTORE_RB_OFFSET(rb, soptimestamp));
	*ringcmds++ = timestamp;

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

	/* Add any postIB required for profiling if it is enabled and has
	   assigned counters */
	if (profile_ready)
		adreno_profile_postib_processing(adreno_dev, &flags, &ringcmds);

	/*
	 * end-of-pipeline timestamp.  If per context timestamps is not
	 * enabled, then drawctxt will be NULL or internal command flag will be
	 * set and hence the rb timestamp will be used in else statement below.
	 */
	*ringcmds++ = cp_mem_packet(adreno_dev, CP_EVENT_WRITE, 3, 1);
	*ringcmds++ = CACHE_FLUSH_TS;

	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds, gpuaddr +
				KGSL_MEMSTORE_OFFSET(context_id, eoptimestamp));
		*ringcmds++ = timestamp;
		*ringcmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds, gpuaddr +
				KGSL_MEMSTORE_RB_OFFSET(rb, eoptimestamp));
		*ringcmds++ = rb->timestamp;
	} else {
		ringcmds += cp_gpuaddr(adreno_dev, ringcmds, gpuaddr +
				KGSL_MEMSTORE_RB_OFFSET(rb, eoptimestamp));
		*ringcmds++ = timestamp;
	}

	if (drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		*ringcmds++ = cp_packet(adreno_dev, CP_INTERRUPT, 1);
		*ringcmds++ = CP_INTERRUPT_RB;
	}

	if (adreno_is_a3xx(adreno_dev)) {
		/* Dummy set-constant to trigger context rollover */
		*ringcmds++ = cp_packet(adreno_dev, CP_SET_CONSTANT, 2);
		*ringcmds++ =
			(0x4<<16) | (A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000);
		*ringcmds++ = 0;
	}

	if (flags & KGSL_CMD_FLAGS_WFI) {
		ringcmds += cp_wait_for_idle(adreno_dev, ringcmds);
	}

	if (secured_ctxt)
		ringcmds += cp_secure_mode(adreno_dev, ringcmds, 0);

	/*
	 *  Allocate total_sizedwords space in RB, this is the max space
	 *  required. If we have commands less than the space reserved in RB
	 *  adjust the wptr accordingly
	 */
	rb->wptr = rb->wptr - (total_sizedwords - (ringcmds - start));

	adreno_ringbuffer_submit(rb, time);

	return 0;
}

int
adreno_ringbuffer_issuecmds(struct adreno_ringbuffer *rb,
				unsigned int flags,
				unsigned int *cmds,
				int sizedwords)
{
	flags |= KGSL_CMD_FLAGS_INTERNAL_ISSUE;

	return adreno_ringbuffer_addcmds(rb, flags, cmds,
		sizedwords, 0, NULL);
}

/**
 * _ringbuffer_verify_ib() - Check if an IB's size is within a permitted limit
 * @device: The kgsl device pointer
 * @ibdesc: Pointer to the IB descriptor
 */
static inline bool _ringbuffer_verify_ib(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_memobj_node *ib)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_process_private *private = dev_priv->process_priv;

	/* The maximum allowable size for an IB in the CP is 0xFFFFF dwords */
	if (ib->size == 0 || ((ib->size >> 2) > 0xFFFFF)) {
		pr_context(device, context, "ctxt %d invalid ib size %lld\n",
			context->id, ib->size);
		return false;
	}

	/* Make sure that the address is mapped */
	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, ib->gpuaddr)) {
		pr_context(device, context, "ctxt %d invalid ib gpuaddr %llX\n",
			context->id, ib->gpuaddr);
		return false;
	}

	return true;
}

int
adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_cmdbatch *cmdbatch,
				uint32_t *timestamp)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct kgsl_memobj_node *ib;
	int ret;

	if (kgsl_context_invalid(context))
		return -EDEADLK;

	/* Verify the IBs before they get queued */
	list_for_each_entry(ib, &cmdbatch->cmdlist, node)
		if (_ringbuffer_verify_ib(dev_priv, context, ib) == false)
			return -EINVAL;

	/* wait for the suspend gate */
	wait_for_completion(&device->cmdbatch_gate);

	/*
	 * Clear the wake on touch bit to indicate an IB has been
	 * submitted since the last time we set it. But only clear
	 * it when we have rendering commands.
	 */
	if (!(cmdbatch->flags & KGSL_CMDBATCH_MARKER)
		&& !(cmdbatch->flags & KGSL_CMDBATCH_SYNC))
		device->flags &= ~KGSL_FLAG_WAKE_ON_TOUCH;

	/* Queue the command in the ringbuffer */
	ret = adreno_dispatcher_queue_cmd(adreno_dev, drawctxt, cmdbatch,
		timestamp);

	/*
	 * Return -EPROTO if the device has faulted since the last time we
	 * checked - userspace uses this to perform post-fault activities
	 */
	if (!ret && test_and_clear_bit(ADRENO_CONTEXT_FAULT, &context->priv))
		ret = -EPROTO;

	return ret;
}

void adreno_ringbuffer_set_constraint(struct kgsl_device *device,
			struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_context *context = cmdbatch->context;
	/*
	 * Check if the context has a constraint and constraint flags are
	 * set.
	 */
	if (context->pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(cmdbatch->flags & KGSL_CONTEXT_PWR_CONSTRAINT)))
		kgsl_pwrctrl_set_constraint(device, &context->pwr_constraint,
						context->id);
}

static inline int _get_alwayson_counter(struct adreno_device *adreno_dev,
		unsigned int *cmds, uint64_t gpuaddr)
{
	unsigned int *p = cmds;

	*p++ = cp_mem_packet(adreno_dev, CP_REG_TO_MEM, 2, 1);
	*p++ = adreno_getreg(adreno_dev, ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO) |
		(1 << 30) | (2 << 18);
	p += cp_gpuaddr(adreno_dev, p, gpuaddr);

	return (unsigned int)(p - cmds);
}

/* adreno_rindbuffer_submitcmd - submit userspace IBs to the GPU */
int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch, struct adreno_submit_time *time)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_memobj_node *ib;
	unsigned int numibs = 0;
	unsigned int *link;
	unsigned int *cmds;
	struct kgsl_context *context;
	struct adreno_context *drawctxt;
	bool use_preamble = true;
	bool cmdbatch_user_profiling = false;
	bool cmdbatch_kernel_profiling = false;
	int flags = KGSL_CMD_FLAGS_NONE;
	int ret;
	struct adreno_ringbuffer *rb;
	struct kgsl_cmdbatch_profiling_buffer *profile_buffer = NULL;
	unsigned int dwords = 0;
	struct adreno_submit_time local;
	uint64_t cond_addr;

	struct kgsl_mem_entry *entry = cmdbatch->profiling_buf_entry;
	if (entry)
		profile_buffer = kgsl_gpuaddr_to_vaddr(&entry->memdesc,
					cmdbatch->profiling_buffer_gpuaddr);

	context = cmdbatch->context;
	drawctxt = ADRENO_CONTEXT(context);

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdbatch->cmdlist, node)
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
		(!test_bit(CMDBATCH_FLAG_SKIP, &cmdbatch->priv))) {

		set_bit(KGSL_FT_SKIPCMD, &cmdbatch->fault_recovery);
		cmdbatch->fault_policy = drawctxt->fault_policy;
		set_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &cmdbatch->priv);

		/* if context is detached print fault recovery */
		adreno_fault_skipcmd_detached(device, drawctxt, cmdbatch);

		/* clear the drawctxt flags */
		clear_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv);
		drawctxt->fault_policy = 0;
	}

	/*When preamble is enabled, the preamble buffer with state restoration
	commands are stored in the first node of the IB chain. We can skip that
	if a context switch hasn't occured */

	if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) &&
		!test_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &cmdbatch->priv) &&
		(rb->drawctxt_active == drawctxt))
		use_preamble = false;

	/*
	 * In skip mode don't issue the draw IBs but keep all the other
	 * accoutrements of a submision (including the interrupt) to keep
	 * the accounting sane. Set start_index and numibs to 0 to just
	 * generate the start and end markers and skip everything else
	 */
	if (test_bit(CMDBATCH_FLAG_SKIP, &cmdbatch->priv)) {
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

	if (cmdbatch->flags & KGSL_CMDBATCH_PROFILING &&
		!adreno_is_a3xx(adreno_dev) && profile_buffer) {
		cmdbatch_user_profiling = true;
		dwords += 6;

		/*
		 * REG_TO_MEM packet on A5xx needs another ordinal.
		 * Add 2 more dwords since we do profiling before and after.
		 */
		if (adreno_is_a5xx(adreno_dev))
			dwords += 2;

		/*
		 * we want to use an adreno_submit_time struct to get the
		 * precise moment when the command is submitted to the
		 * ringbuffer.  If an upstream caller already passed down a
		 * pointer piggyback on that otherwise use a local struct
		 */

		if (time == NULL)
			time = &local;
	}

	if (test_bit(CMDBATCH_FLAG_PROFILE, &cmdbatch->priv)) {
		cmdbatch_kernel_profiling = true;
		dwords += 6;
		if (adreno_is_a5xx(adreno_dev))
			dwords += 2;
	}

	link = kzalloc(sizeof(unsigned int) *  dwords, GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto done;
	}

	cmds = link;

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_START_OF_IB_IDENTIFIER;

	if (cmdbatch_kernel_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			adreno_dev->cmdbatch_profile_buffer.gpuaddr +
			ADRENO_CMDBATCH_PROFILE_OFFSET(cmdbatch->profile_index,
				started));
	}

	/*
	 * Add cmds to read the GPU ticks at the start of the cmdbatch and
	 * write it into the appropriate cmdbatch profiling buffer offset
	 */
	if (cmdbatch_user_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			cmdbatch->profiling_buffer_gpuaddr +
			offsetof(struct kgsl_cmdbatch_profiling_buffer,
			gpu_ticks_submitted));
	}

	if (numibs) {
		list_for_each_entry(ib, &cmdbatch->cmdlist, node) {
			if ((ib->priv & MEMOBJ_PREAMBLE) &&
				adreno_is_preemption_enabled(adreno_dev) &&
				gpudev->preemption_pre_ibsubmit) {

				cond_addr = device->memstore.gpuaddr +
					KGSL_MEMSTORE_OFFSET(context->id,
					 preempted);
				cmds += gpudev->preemption_pre_ibsubmit(
						adreno_dev, rb, cmds, context,
						cond_addr, ib);
			}

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
	}

	if (gpudev->preemption_post_ibsubmit &&
				adreno_is_preemption_enabled(adreno_dev))
		cmds += gpudev->preemption_post_ibsubmit(adreno_dev,
					rb, cmds, context);

	if (cmdbatch_kernel_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			adreno_dev->cmdbatch_profile_buffer.gpuaddr +
			ADRENO_CMDBATCH_PROFILE_OFFSET(cmdbatch->profile_index,
				retired));
	}

	/*
	 * Add cmds to read the GPU ticks at the end of the cmdbatch and
	 * write it into the appropriate cmdbatch profiling buffer offset
	 */
	if (cmdbatch_user_profiling) {
		cmds += _get_alwayson_counter(adreno_dev, cmds,
			cmdbatch->profiling_buffer_gpuaddr +
			offsetof(struct kgsl_cmdbatch_profiling_buffer,
			gpu_ticks_retired));
	}

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_END_OF_IB_IDENTIFIER;

	ret = adreno_drawctxt_switch(adreno_dev, rb, drawctxt, cmdbatch->flags);

	/*
	 * In the unlikely event of an error in the drawctxt switch,
	 * treat it like a hang
	 */
	if (ret)
		goto done;

	if (test_bit(CMDBATCH_FLAG_WFI, &cmdbatch->priv))
		flags = KGSL_CMD_FLAGS_WFI;

	/*
	 * For some targets, we need to execute a dummy shader operation after a
	 * power collapse
	 */

	if (test_and_clear_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv) &&
		test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
		flags |= KGSL_CMD_FLAGS_PWRON_FIXUP;

	/* Set the constraints before adding to ringbuffer */
	adreno_ringbuffer_set_constraint(device, cmdbatch);

	/* CFF stuff executed only if CFF is enabled */
	kgsl_cffdump_capture_ib_desc(device, context, cmdbatch);


	ret = adreno_ringbuffer_addcmds(rb, flags,
					&link[0], (cmds - link),
					cmdbatch->timestamp, time);

	if (!ret) {
		cmdbatch->global_ts = drawctxt->internal_timestamp;

		/* Put the timevalues in the profiling buffer */
		if (cmdbatch_user_profiling) {
			profile_buffer->wall_clock_s = time->utime.tv_sec;
			profile_buffer->wall_clock_ns = time->utime.tv_nsec;
			profile_buffer->gpu_ticks_queued = time->ticks;
		}
	}

	kgsl_cffdump_regpoll(device,
		adreno_getreg(adreno_dev, ADRENO_REG_RBBM_STATUS) << 2,
		0x00000000, 0x80000000);
done:
	/* Corresponding unmap to the memdesc map of profile_buffer */
	if (entry)
		kgsl_memdesc_unmap(&entry->memdesc);


	trace_kgsl_issueibcmds(device, context->id, cmdbatch,
			numibs, cmdbatch->timestamp,
			cmdbatch->flags, ret, drawctxt->type);

	kfree(link);
	return ret;
}

/**
 * adreno_ringbuffer_mmu_clk_disable_event() - Callback function that
 * disables the MMU clocks.
 * @device: Device pointer
 * @context: The ringbuffer context pointer
 * @data: Pointer containing the adreno_mmu_disable_clk_param structure
 * @type: The event call type (RETIRED or CANCELLED)
 */
static void adreno_ringbuffer_mmu_clk_disable_event(struct kgsl_device *device,
			struct kgsl_event_group *group, void *data, int type)
{
	kgsl_mmu_disable_clk(&device->mmu);
}

/*
 * adreno_ringbuffer_mmu_disable_clk_on_ts() - Sets up event to disable MMU
 * clocks
 * @device - The kgsl device pointer
 * @rb: The ringbuffer in whose event list the event is added
 * @timestamp: The timestamp on which the event should trigger
 *
 * Creates an event to disable the MMU clocks on timestamp and if event
 * already exists then updates the timestamp of disabling the MMU clocks
 * with the passed in ts if it is greater than the current value at which
 * the clocks will be disabled
 * Return - void
 */
void
adreno_ringbuffer_mmu_disable_clk_on_ts(struct kgsl_device *device,
			struct adreno_ringbuffer *rb, unsigned int timestamp)
{
	if (kgsl_add_event(device, &(rb->events), timestamp,
		adreno_ringbuffer_mmu_clk_disable_event, NULL)) {
		KGSL_DRV_ERR(device,
			"Failed to add IOMMU disable clk event\n");
	}
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
	struct kgsl_device *device = rb->device;
	int ret;
	unsigned long wait_time;

	/* force a timeout from caller for the wait */
	BUG_ON(0 == msecs);

	ret = kgsl_add_event(device, &rb->events, timestamp,
		adreno_ringbuffer_wait_callback, NULL);
	if (ret)
		return ret;

	mutex_unlock(&device->mutex);

	wait_time = msecs_to_jiffies(msecs);
	if (0 == wait_event_timeout(rb->ts_expire_waitq,
		!kgsl_event_pending(device, &rb->events, timestamp,
				adreno_ringbuffer_wait_callback, NULL),
		wait_time))
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

/**
 * adreno_ringbuffer_pt_switch_cmds() - Add commands to switch the pagetable
 * @rb: The ringbuffer on which the commands are going to be submitted
 * @cmds: The pointer where the commands are copied
 *
 * Returns the number of DWORDS added to cmds pointer
 */
static int
adreno_ringbuffer_pt_switch_cmds(struct adreno_ringbuffer *rb,
				unsigned int *cmds,
				struct kgsl_pagetable *pt)
{
	unsigned int *cmds_orig = cmds;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	cmds += adreno_iommu_set_apriv(adreno_dev, cmds, 1);
	if (ADRENO_FEATURE(adreno_dev, ADRENO_HAS_REG_TO_REG_CMDS))
		cmds += adreno_iommu_set_pt_ib(rb, cmds, pt);
	else
		cmds += adreno_iommu_set_pt_generate_cmds(rb, cmds, pt);

	cmds += adreno_iommu_set_apriv(adreno_dev, cmds, 0);

	return cmds - cmds_orig;
}


/**
 * adreno_ringbuffer_submit_preempt_token() - Submit a preempt token
 * @rb: Ringbuffer in which the token is submitted
 * @incoming_rb: The RB to which the GPU switches when this preemption
 * token is executed.
 *
 * Called to make sure that an outstanding preemption request is
 * granted.
 */
int adreno_ringbuffer_submit_preempt_token(struct adreno_ringbuffer *rb,
					struct adreno_ringbuffer *incoming_rb)
{
	unsigned int *ringcmds, *start;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	struct kgsl_device *device = &(adreno_dev->dev);
	struct kgsl_iommu *iommu = device->mmu.priv;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ptname;
	struct kgsl_pagetable *pt;
	int pt_switch_sizedwords = 0, total_sizedwords = 20;
	unsigned link[ADRENO_RB_PREEMPT_TOKEN_DWORDS];
	uint i;
	uint64_t ttbr0;

	if (incoming_rb->preempted_midway) {

		if (adreno_is_a5xx(adreno_dev)) {
			kgsl_sharedmem_readq(&rb->pagetable_desc, &ttbr0,
				offsetof(struct adreno_ringbuffer_pagetable_info
				, ttbr0));
			kgsl_sharedmem_writeq(rb->device, &iommu->smmu_info,
				offsetof(struct a5xx_cp_smmu_info, ttbr0),
				ttbr0);
		} else {
			kgsl_sharedmem_readl(&incoming_rb->pagetable_desc,
				&ptname, offsetof(
				struct adreno_ringbuffer_pagetable_info,
				current_rb_ptname));
			pt = kgsl_mmu_get_pt_from_ptname(&(rb->device->mmu),
				ptname);
			/*
			 * always expect a valid pt, else pt refcounting is
			 * messed up or current pt tracking has a bug which
			 * could lead to eventual disaster
			 */
			BUG_ON(!pt);
			/* set the ringbuffer for incoming RB */
			pt_switch_sizedwords = adreno_ringbuffer_pt_switch_cmds(
								incoming_rb,
								&link[0], pt);

			total_sizedwords = total_sizedwords +
						pt_switch_sizedwords;
		}
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

	ringcmds += gpudev->preemption_token(adreno_dev, rb, ringcmds,
				rb->device->memstore.gpuaddr +
				KGSL_MEMSTORE_RB_OFFSET(rb, preempted));

	if ((uint)(ringcmds - start) > total_sizedwords) {
		KGSL_DRV_ERR(device, "Insufficient rb size allocated\n");
		BUG();
	}

	/*
	 * If we have commands less than the space reserved in RB
	 *  adjust the wptr accordingly
	 */
	rb->wptr = rb->wptr - (total_sizedwords - (uint)(ringcmds - start));

	/* submit just the preempt token */
	mb();
	kgsl_pwrscale_busy(rb->device);
	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR, rb->wptr);
	return 0;
}
