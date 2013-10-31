/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/log2.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"

#include "a2xx_reg.h"
#include "a3xx_reg.h"

#define GSL_RB_NOP_SIZEDWORDS				2

/*
 * CP DEBUG settings for all cores:
 * DYNAMIC_CLK_DISABLE [27] - turn off the dynamic clock control
 * PROG_END_PTR_ENABLE [25] - Allow 128 bit writes to the VBIF
 */

#define CP_DEBUG_DEFAULT ((1 << 27) | (1 << 25))

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	BUG_ON(rb->wptr == 0);

	/* Let the pwrscale policy know that new commands have
	 been submitted. */
	kgsl_pwrscale_busy(rb->device);

	/*synchronize memory before informing the hardware of the
	 *new commands.
	 */
	mb();

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR, rb->wptr);
}

static int
adreno_ringbuffer_waitspace(struct adreno_ringbuffer *rb,
				struct adreno_context *context,
				unsigned int numcmds, int wptr_ahead)
{
	int nopcount;
	unsigned int freecmds;
	unsigned int *cmds;
	uint cmds_gpu;
	unsigned long wait_time;
	unsigned long wait_timeout = msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);
	unsigned long wait_time_part;
	unsigned int prev_reg_val[FT_DETECT_REGS_COUNT];
	unsigned int rptr;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	memset(prev_reg_val, 0, sizeof(prev_reg_val));

	/* if wptr ahead, fill the remaining with NOPs */
	if (wptr_ahead) {
		/* -1 for header */
		nopcount = rb->sizedwords - rb->wptr - 1;

		cmds = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		cmds_gpu = rb->buffer_desc.gpuaddr + sizeof(uint)*rb->wptr;

		GSL_RB_WRITE(rb->device, cmds, cmds_gpu,
				cp_nop_packet(nopcount));

		/* Make sure that rptr is not 0 before submitting
		 * commands at the end of ringbuffer. We do not
		 * want the rptr and wptr to become equal when
		 * the ringbuffer is not empty */
		do {
			rptr = adreno_get_rptr(rb);
		} while (!rptr);

		rb->wptr = 0;
	}

	wait_time = jiffies + wait_timeout;
	wait_time_part = jiffies + msecs_to_jiffies(KGSL_TIMEOUT_PART);
	/* wait for space in ringbuffer */
	while (1) {
		rptr = adreno_get_rptr(rb);

		freecmds = rptr - rb->wptr;

		if (freecmds == 0 || freecmds > numcmds)
			break;

		/* Dont wait for timeout, detect hang faster.
		 */

		if (kgsl_atomic_read(&adreno_dev->hang_intr_set))
			goto hang_detected;

		if (time_after(jiffies, wait_time_part)) {
			wait_time_part = jiffies +
				msecs_to_jiffies(KGSL_TIMEOUT_PART);

			if ((adreno_ft_detect(rb->device, prev_reg_val)))
				goto hang_detected;
		}

		if (time_after(jiffies, wait_time)) {
			KGSL_DRV_ERR(rb->device,
			"Timed out while waiting for freespace in ringbuffer "
			"rptr: 0x%x, wptr: 0x%x\n", rptr, rb->wptr);
			goto err;
		}

		continue;

hang_detected:
		KGSL_DRV_ERR(rb->device,
			"Hang detected while waiting for freespace in"
			"ringbuffer rptr: 0x%x, wptr: 0x%x\n",
			rptr, rb->wptr);

err:
		if (!adreno_dump_and_exec_ft(rb->device)) {
			if (context && context->flags & CTXT_FLAGS_GPU_HANG) {
				KGSL_CTXT_WARN(rb->device,
				"Context %p caused a gpu hang. Will not accept commands for context %d\n",
				context, context->base.id);
				return -EDEADLK;
			}
			wait_time = jiffies + wait_timeout;
		} else {
			/* GPU is hung and fault tolerance failed */
			BUG();
		}
	}
	return 0;
}

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
					struct adreno_context *context,
					unsigned int numcmds)
{
	unsigned int *ptr = NULL;
	int ret = 0;
	unsigned int rptr;
	BUG_ON(numcmds >= rb->sizedwords);

	rptr = adreno_get_rptr(rb);
	/* check for available space */
	if (rb->wptr >= rptr) {
		/* wptr ahead or equal to rptr */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS))
			ret = adreno_ringbuffer_waitspace(rb, context,
							numcmds, 1);
	} else {
		/* wptr behind rptr */
		if ((rb->wptr + numcmds) >= rptr)
			ret = adreno_ringbuffer_waitspace(rb, context,
							numcmds, 0);
		/* check for remaining space */
		/* reserve dwords for nop packet */
		if (!ret && (rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS))
			ret = adreno_ringbuffer_waitspace(rb, context,
							numcmds, 1);
	}

	if (!ret) {
		ptr = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		rb->wptr += numcmds;
	}

	return ptr;
}

static int _load_firmware(struct kgsl_device *device, const char *fwfile,
			  void **data, int *len)
{
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_ERR(device, "request_firmware(%s) failed: %d\n",
			     fwfile, ret);
		return ret;
	}

	*data = kmalloc(fw->size, GFP_KERNEL);

	if (*data) {
		memcpy(*data, fw->data, fw->size);
		*len = fw->size;
	} else
		KGSL_MEM_ERR(device, "kmalloc(%d) failed\n", fw->size);

	release_firmware(fw);
	return (*data != NULL) ? 0 : -ENOMEM;
}

int adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;

	if (adreno_dev->pm4_fw == NULL) {
		int len;
		void *ptr;

		ret = _load_firmware(device, adreno_dev->pm4_fwfile,
			&ptr, &len);

		if (ret)
			goto err;

		/* PM4 size is 3 dword aligned plus 1 dword of version */
		if (len % ((sizeof(uint32_t) * 3)) != sizeof(uint32_t)) {
			KGSL_DRV_ERR(device, "Bad firmware size: %d\n", len);
			ret = -EINVAL;
			kfree(ptr);
			goto err;
		}

		adreno_dev->pm4_fw_size = len / sizeof(uint32_t);
		adreno_dev->pm4_fw = ptr;
		adreno_dev->pm4_fw_version = adreno_dev->pm4_fw[1];
	}

err:
	return ret;
}

/**
 * adreno_ringbuffer_load_pm4_ucode() - Load pm4 ucode
 * @device: Pointer to a KGSL device
 * @start: Starting index in pm4 ucode to load
 * @addr: Address to load the pm4 ucode
 *
 * Load the pm4 ucode from @start at @addr.
 */
int adreno_ringbuffer_load_pm4_ucode(struct kgsl_device *device,
					unsigned int start, unsigned int addr)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	if (adreno_dev->pm4_fw == NULL) {
		int ret = adreno_ringbuffer_read_pm4_ucode(device);
		if (ret)
			return ret;
	}

	KGSL_DRV_INFO(device, "loading pm4 ucode version: %d\n",
		adreno_dev->pm4_fw_version);

	adreno_writereg(adreno_dev, ADRENO_REG_CP_DEBUG, CP_DEBUG_DEFAULT);
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_WADDR, addr);
	for (i = start; i < adreno_dev->pm4_fw_size; i++)
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_DATA,
					adreno_dev->pm4_fw[i]);

	return 0;
}

int adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;

	if (adreno_dev->pfp_fw == NULL) {
		int len;
		void *ptr;

		ret = _load_firmware(device, adreno_dev->pfp_fwfile,
			&ptr, &len);
		if (ret)
			goto err;

		/* PFP size shold be dword aligned */
		if (len % sizeof(uint32_t) != 0) {
			KGSL_DRV_ERR(device, "Bad firmware size: %d\n", len);
			ret = -EINVAL;
			kfree(ptr);
			goto err;
		}

		adreno_dev->pfp_fw_size = len / sizeof(uint32_t);
		adreno_dev->pfp_fw = ptr;
		adreno_dev->pfp_fw_version = adreno_dev->pfp_fw[5];
	}

err:
	return ret;
}

/**
 * adreno_ringbuffer_load_pfp_ucode() - Load pfp ucode
 * @device: Pointer to a KGSL device
 * @start: Starting index in pfp ucode to load
 * @addr: Address to load the pfp ucode
 *
 * Load the pfp ucode from @start at @addr.
 */
int adreno_ringbuffer_load_pfp_ucode(struct kgsl_device *device,
					unsigned int start, unsigned int addr)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	if (adreno_dev->pfp_fw == NULL) {
		int ret = adreno_ringbuffer_read_pfp_ucode(device);
		if (ret)
			return ret;
	}

	KGSL_DRV_INFO(device, "loading pfp ucode version: %d\n",
			adreno_dev->pfp_fw_version);

	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR,
						addr);
	for (i = start; i < adreno_dev->pfp_fw_size; i++)
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
						adreno_dev->pfp_fw[i]);

	return 0;
}

/**
 * _ringbuffer_start_common() - Ringbuffer start
 * @rb: Pointer to adreno ringbuffer
 *
 * Setup ringbuffer for GPU.
 */
int _ringbuffer_start_common(struct adreno_ringbuffer *rb)
{
	int status;
	/*cp_rb_cntl_u cp_rb_cntl; */
	union reg_cp_rb_cntl cp_rb_cntl;
	unsigned int rb_cntl;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (rb->flags & KGSL_FLAGS_STARTED)
		return 0;

	kgsl_sharedmem_set(rb->device, &rb->memptrs_desc, 0, 0,
			   sizeof(struct kgsl_rbmemptrs));

	kgsl_sharedmem_set(rb->device, &rb->buffer_desc, 0, 0xAA,
			   (rb->sizedwords << 2));

	if (adreno_is_a2xx(adreno_dev)) {
		kgsl_regwrite(device, REG_CP_RB_WPTR_BASE,
			(rb->memptrs_desc.gpuaddr
			+ GSL_RB_MEMPTRS_WPTRPOLL_OFFSET));

		/* setup WPTR delay */
		kgsl_regwrite(device, REG_CP_RB_WPTR_DELAY,
			0 /*0x70000010 */);
	}

	/*setup REG_CP_RB_CNTL */
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_CNTL, &rb_cntl);
	cp_rb_cntl.val = rb_cntl;

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2)
	 */
	cp_rb_cntl.f.rb_bufsz = ilog2(rb->sizedwords >> 1);

	/*
	 * Specify the quadwords to read before updating mem RPTR.
	 * Like above, pass the log2 representation of the blocksize
	 * in quadwords.
	*/
	cp_rb_cntl.f.rb_blksz = ilog2(KGSL_RB_BLKSIZE >> 3);

	if (adreno_is_a2xx(adreno_dev)) {
		/* WPTR polling */
		cp_rb_cntl.f.rb_poll_en = GSL_RB_CNTL_POLL_EN;
	}

	/* mem RPTR writebacks */
	cp_rb_cntl.f.rb_no_update =  GSL_RB_CNTL_NO_UPDATE;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL, cp_rb_cntl.val);

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
					rb->buffer_desc.gpuaddr);

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_RPTR_ADDR,
				rb->memptrs_desc.gpuaddr +
				GSL_RB_MEMPTRS_RPTR_OFFSET);

	if (adreno_is_a2xx(adreno_dev)) {
		/* explicitly clear all cp interrupts */
		kgsl_regwrite(device, REG_CP_INT_ACK, 0xFFFFFFFF);
	}

	/* setup scratch/timestamp */
	adreno_writereg(adreno_dev, ADRENO_REG_SCRATCH_ADDR,
				device->memstore.gpuaddr +
				KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
					soptimestamp));

	adreno_writereg(adreno_dev, ADRENO_REG_SCRATCH_UMSK,
			     GSL_RB_MEMPTRS_SCRATCH_MASK);

	/* CP ROQ queue sizes (bytes) - RB:16, ST:16, IB1:32, IB2:64 */
	if (adreno_is_a305(adreno_dev) || adreno_is_a305c(adreno_dev) ||
		adreno_is_a320(adreno_dev))
		kgsl_regwrite(device, REG_CP_QUEUE_THRESHOLDS, 0x000E0602);
	else if (adreno_is_a330(adreno_dev) || adreno_is_a305b(adreno_dev))
		kgsl_regwrite(device, REG_CP_QUEUE_THRESHOLDS, 0x003E2008);

	rb->wptr = 0;

	/* clear ME_HALT to start micro engine */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

	/* ME init is GPU specific, so jump into the sub-function */
	status = adreno_dev->gpudev->rb_init(adreno_dev, rb);
	if (status)
		return status;

	/* idle device to validate ME INIT */
	status = adreno_idle(device);

	if (status == 0)
		rb->flags |= KGSL_FLAGS_STARTED;

	return status;
}

/**
 * adreno_ringbuffer_warm_start() - Ringbuffer warm start
 * @rb: Pointer to adreno ringbuffer
 *
 * Start the ringbuffer but load only jump tables part of the
 * microcode.
 */
int adreno_ringbuffer_warm_start(struct adreno_ringbuffer *rb)
{
	int status;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* load the CP ucode */
	status = adreno_ringbuffer_load_pm4_ucode(device,
			adreno_dev->pm4_jt_idx, adreno_dev->pm4_jt_addr);
	if (status != 0)
		return status;

	/* load the prefetch parser ucode */
	status = adreno_ringbuffer_load_pfp_ucode(device,
			adreno_dev->pfp_jt_idx, adreno_dev->pfp_jt_addr);
	if (status != 0)
		return status;

	return _ringbuffer_start_common(rb);
}

int adreno_ringbuffer_start(struct adreno_ringbuffer *rb)
{
	int status;

	if (rb->flags & KGSL_FLAGS_STARTED)
		return 0;

	/* load the CP ucode */
	status = adreno_ringbuffer_load_pm4_ucode(rb->device, 1, 0);
	if (status != 0)
		return status;

	/* load the prefetch parser ucode */
	status = adreno_ringbuffer_load_pfp_ucode(rb->device, 1, 0);
	if (status != 0)
		return status;

	return _ringbuffer_start_common(rb);
}

void adreno_ringbuffer_stop(struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (rb->flags & KGSL_FLAGS_STARTED) {
		if (adreno_is_a200(adreno_dev))
			kgsl_regwrite(rb->device, REG_CP_ME_CNTL, 0x10000000);

		rb->flags &= ~KGSL_FLAGS_STARTED;
	}
}

int adreno_ringbuffer_init(struct kgsl_device *device)
{
	int status;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	rb->device = device;
	/*
	 * It is silly to convert this to words and then back to bytes
	 * immediately below, but most of the rest of the code deals
	 * in words, so we might as well only do the math once
	 */
	rb->sizedwords = KGSL_RB_SIZE >> 2;

	rb->buffer_desc.flags = KGSL_MEMFLAGS_GPUREADONLY;
	/* allocate memory for ringbuffer */
	status = kgsl_allocate_contiguous(&rb->buffer_desc,
		(rb->sizedwords << 2));

	if (status != 0) {
		adreno_ringbuffer_close(rb);
		return status;
	}

	/* allocate memory for polling and timestamps */
	/* This really can be at 4 byte alignment boundry but for using MMU
	 * we need to make it at page boundary */
	status = kgsl_allocate_contiguous(&rb->memptrs_desc,
		sizeof(struct kgsl_rbmemptrs));

	if (status != 0) {
		adreno_ringbuffer_close(rb);
		return status;
	}

	/* overlay structure on memptrs memory */
	rb->memptrs = (struct kgsl_rbmemptrs *) rb->memptrs_desc.hostptr;

	rb->global_ts = 0;

	return 0;
}

void adreno_ringbuffer_close(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	kgsl_sharedmem_free(&rb->buffer_desc);
	kgsl_sharedmem_free(&rb->memptrs_desc);

	kfree(adreno_dev->pfp_fw);
	kfree(adreno_dev->pm4_fw);

	adreno_dev->pfp_fw = NULL;
	adreno_dev->pm4_fw = NULL;

	memset(rb, 0, sizeof(struct adreno_ringbuffer));
}

static int
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				struct adreno_context *context,
				unsigned int flags, unsigned int *cmds,
				int sizedwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	unsigned int *ringcmds;
	unsigned int total_sizedwords = sizedwords;
	unsigned int i;
	unsigned int rcmd_gpu;
	unsigned int context_id = KGSL_MEMSTORE_GLOBAL;
	unsigned int gpuaddr = rb->device->memstore.gpuaddr;
	unsigned int timestamp;
	bool profile_ready;

	/*
	 * If in stream ib profiling is enabled and there are counters
	 * assigned, then space needs to be reserved for profiling.  This
	 * space in the ringbuffer is always consumed (might be filled with
	 * NOPs in error case.  profile_ready needs to be consistent through
	 * the _addcmds call since it is allocating additional ringbuffer
	 * command space.
	 */
	profile_ready = !adreno_is_a2xx(adreno_dev) && context &&
		adreno_profile_assignments_ready(&adreno_dev->profile) &&
		!(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE);

	/*
	 * if the context was not created with per context timestamp
	 * support, we must use the global timestamp since issueibcmds
	 * will be returning that one, or if an internal issue then
	 * use global timestamp.
	 */
	if ((context && (context->flags & CTXT_FLAGS_PER_CONTEXT_TS)) &&
		!(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE))
		context_id = context->base.id;

	/* reserve space to temporarily turn off protected mode
	*  error checking if needed
	*/
	total_sizedwords += flags & KGSL_CMD_FLAGS_PMODE ? 4 : 0;
	/* 2 dwords to store the start of command sequence */
	total_sizedwords += 2;
	/* internal ib command identifier for the ringbuffer */
	total_sizedwords += (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE) ? 2 : 0;

	/* Add CP_COND_EXEC commands to generate CP_INTERRUPT */
	total_sizedwords += context ? 13 : 0;

	if ((context) && (context->flags & CTXT_FLAGS_PER_CONTEXT_TS) &&
		(flags & (KGSL_CMD_FLAGS_INTERNAL_ISSUE |
		KGSL_CMD_FLAGS_GET_INT)))
			total_sizedwords += 2;

	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 7;

	if (adreno_is_a2xx(adreno_dev))
		total_sizedwords += 2; /* CP_WAIT_FOR_IDLE */

	total_sizedwords += 2; /* scratchpad ts for fault tolerance */
	total_sizedwords += 3; /* sop timestamp */
	total_sizedwords += 4; /* eop timestamp */

	if (KGSL_MEMSTORE_GLOBAL != context_id)
		total_sizedwords += 3; /* global timestamp without cache
					* flush for non-zero context */

	if (adreno_is_a20x(adreno_dev))
		total_sizedwords += 2; /* CACHE_FLUSH */

	if (flags & KGSL_CMD_FLAGS_EOF)
		total_sizedwords += 2;

	if (profile_ready)
		total_sizedwords += 6;   /* space for pre_ib and post_ib */

	/* Add space for the power on shader fixup if we need it */
	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP)
		total_sizedwords += 5;

	ringcmds = adreno_ringbuffer_allocspace(rb, context, total_sizedwords);
	if (!ringcmds)
		return -ENOSPC;

	rcmd_gpu = rb->buffer_desc.gpuaddr
		+ sizeof(uint)*(rb->wptr-total_sizedwords);

	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, cp_nop_packet(1));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, KGSL_CMD_IDENTIFIER);

	if (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, cp_nop_packet(1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				KGSL_CMD_INTERNAL_IDENTIFIER);
	}

	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, cp_nop_packet(1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				KGSL_PWRON_FIXUP_IDENTIFIER);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			CP_HDR_INDIRECT_BUFFER_PFD);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			adreno_dev->pwron_fixup.gpuaddr);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			adreno_dev->pwron_fixup_dwords);
	}

	/* Add any IB required for profiling if it is enabled */
	if (profile_ready)
		adreno_profile_preib_processing(rb->device, context->base.id,
				&flags, &ringcmds, &rcmd_gpu);

	/* always increment the global timestamp. once. */
	rb->global_ts++;

	if (KGSL_MEMSTORE_GLOBAL != context_id)
		timestamp = context->timestamp;
	else
		timestamp = rb->global_ts;

	/* scratchpad ts for fault tolerance */
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
		cp_type0_packet(adreno_getreg(adreno_dev,
			ADRENO_REG_CP_TIMESTAMP), 1));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			rb->global_ts);

	/* start-of-pipeline timestamp */
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_MEM_WRITE, 2));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, (gpuaddr +
		KGSL_MEMSTORE_OFFSET(context_id, soptimestamp)));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, timestamp);

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* disable protected mode error checking */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0);
	}

	for (i = 0; i < sizedwords; i++) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, *cmds);
		cmds++;
	}

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* re-enable protected mode error checking */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 1);
	}

	/* HW Workaround for MMU Page fault
	* due to memory getting free early before
	* GPU completes it.
	*/
	if (adreno_is_a2xx(adreno_dev)) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_WAIT_FOR_IDLE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0x00);
	}

	if (adreno_is_a3xx(adreno_dev)) {
		/*
		 * Flush HLSQ lazy updates to make sure there are no
		 * resources pending for indirect loads after the timestamp
		 */

		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_EVENT_WRITE, 1));
		GSL_RB_WRITE(rb->device, ringcmds,
			rcmd_gpu, 0x07); /* HLSQ_FLUSH */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_WAIT_FOR_IDLE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0x00);
	}

	/* Add any postIB required for profiling if it is enabled and has
	   assigned counters */
	if (profile_ready)
		adreno_profile_postib_processing(rb->device, &flags,
						 &ringcmds, &rcmd_gpu);

	/*
	 * end-of-pipeline timestamp.  If per context timestamps is not
	 * enabled, then context_id will be KGSL_MEMSTORE_GLOBAL so all
	 * eop timestamps will work out.
	 */
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_EVENT_WRITE, 3));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, CACHE_FLUSH_TS);
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, (gpuaddr +
		KGSL_MEMSTORE_OFFSET(context_id, eoptimestamp)));
	GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, timestamp);

	if (KGSL_MEMSTORE_GLOBAL != context_id) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_MEM_WRITE, 2));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, (gpuaddr +
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
				eoptimestamp)));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			rb->global_ts);
	}

	if (adreno_is_a20x(adreno_dev)) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_EVENT_WRITE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, CACHE_FLUSH);
	}

	if (context) {
		/* Conditional execution based on memory values */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_COND_EXEC, 4));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, (gpuaddr +
			KGSL_MEMSTORE_OFFSET(
				context_id, ts_cmp_enable)) >> 2);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, (gpuaddr +
			KGSL_MEMSTORE_OFFSET(
				context_id, ref_wait_ts)) >> 2);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, timestamp);
		/* # of conditional command DWORDs */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 8);

		/* Clear the ts_cmp_enable for the context */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_MEM_WRITE, 2));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, gpuaddr +
			KGSL_MEMSTORE_OFFSET(
				context_id, ts_cmp_enable));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0x0);

		/* Clear the ts_cmp_enable for the global timestamp */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_MEM_WRITE, 2));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, gpuaddr +
			KGSL_MEMSTORE_OFFSET(
				KGSL_MEMSTORE_GLOBAL, ts_cmp_enable));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0x0);

		/* Trigger the interrupt */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_INTERRUPT, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				CP_INT_CNTL__RB_INT_MASK);
	}

	/*
	 * If per context timestamps are enabled and any of the kgsl
	 * internal commands want INT to be generated trigger the INT
	*/
	if ((context) && (context->flags & CTXT_FLAGS_PER_CONTEXT_TS) &&
		(flags & (KGSL_CMD_FLAGS_INTERNAL_ISSUE |
		KGSL_CMD_FLAGS_GET_INT))) {
			GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				cp_type3_packet(CP_INTERRUPT, 1));
			GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				CP_INT_CNTL__RB_INT_MASK);
	}

	if (adreno_is_a3xx(adreno_dev)) {
		/* Dummy set-constant to trigger context rollover */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_CONSTANT, 2));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			(0x4<<16)|(A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0);
	}

	if (flags & KGSL_CMD_FLAGS_EOF) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, cp_nop_packet(1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				KGSL_END_OF_FRAME_IDENTIFIER);
	}

	adreno_ringbuffer_submit(rb);

	return 0;
}

unsigned int
adreno_ringbuffer_issuecmds(struct kgsl_device *device,
						struct adreno_context *drawctxt,
						unsigned int flags,
						unsigned int *cmds,
						int sizedwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	if (device->state & KGSL_STATE_HUNG)
		return kgsl_readtimestamp(device, KGSL_MEMSTORE_GLOBAL,
					KGSL_TIMESTAMP_RETIRED);

	flags |= KGSL_CMD_FLAGS_INTERNAL_ISSUE;

	return adreno_ringbuffer_addcmds(rb, drawctxt, flags, cmds,
							sizedwords);
}

static bool _parse_ibs(struct kgsl_device_private *dev_priv, uint gpuaddr,
			   int sizedwords);

static bool
_handle_type3(struct kgsl_device_private *dev_priv, uint *hostaddr)
{
	unsigned int opcode = cp_type3_opcode(*hostaddr);
	switch (opcode) {
	case CP_INDIRECT_BUFFER_PFD:
	case CP_INDIRECT_BUFFER_PFE:
	case CP_COND_INDIRECT_BUFFER_PFE:
	case CP_COND_INDIRECT_BUFFER_PFD:
		return _parse_ibs(dev_priv, hostaddr[1], hostaddr[2]);
	case CP_NOP:
	case CP_WAIT_FOR_IDLE:
	case CP_WAIT_REG_MEM:
	case CP_WAIT_REG_EQ:
	case CP_WAT_REG_GTE:
	case CP_WAIT_UNTIL_READ:
	case CP_WAIT_IB_PFD_COMPLETE:
	case CP_REG_RMW:
	case CP_REG_TO_MEM:
	case CP_MEM_WRITE:
	case CP_MEM_WRITE_CNTR:
	case CP_COND_EXEC:
	case CP_COND_WRITE:
	case CP_EVENT_WRITE:
	case CP_EVENT_WRITE_SHD:
	case CP_EVENT_WRITE_CFL:
	case CP_EVENT_WRITE_ZPD:
	case CP_DRAW_INDX:
	case CP_DRAW_INDX_2:
	case CP_DRAW_INDX_BIN:
	case CP_DRAW_INDX_2_BIN:
	case CP_VIZ_QUERY:
	case CP_SET_STATE:
	case CP_SET_CONSTANT:
	case CP_IM_LOAD:
	case CP_IM_LOAD_IMMEDIATE:
	case CP_LOAD_CONSTANT_CONTEXT:
	case CP_INVALIDATE_STATE:
	case CP_SET_SHADER_BASES:
	case CP_SET_BIN_MASK:
	case CP_SET_BIN_SELECT:
	case CP_SET_BIN_BASE_OFFSET:
	case CP_SET_BIN_DATA:
	case CP_CONTEXT_UPDATE:
	case CP_INTERRUPT:
	case CP_IM_STORE:
	case CP_LOAD_STATE:
		break;
	/* these shouldn't come from userspace */
	case CP_ME_INIT:
	case CP_SET_PROTECTED_MODE:
	default:
		KGSL_CMD_ERR(dev_priv->device, "bad CP opcode %0x\n", opcode);
		return false;
		break;
	}

	return true;
}

static bool
_handle_type0(struct kgsl_device_private *dev_priv, uint *hostaddr)
{
	unsigned int reg = type0_pkt_offset(*hostaddr);
	unsigned int cnt = type0_pkt_size(*hostaddr);
	if (reg < 0x0192 || (reg + cnt) >= 0x8000) {
		KGSL_CMD_ERR(dev_priv->device, "bad type0 reg: 0x%0x cnt: %d\n",
			     reg, cnt);
		return false;
	}
	return true;
}

/*
 * Traverse IBs and dump them to test vector. Detect swap by inspecting
 * register writes, keeping note of the current state, and dump
 * framebuffer config to test vector
 */
static bool _parse_ibs(struct kgsl_device_private *dev_priv,
			   uint gpuaddr, int sizedwords)
{
	static uint level; /* recursion level */
	bool ret = false;
	uint *hostaddr, *hoststart;
	int dwords_left = sizedwords; /* dwords left in the current command
					 buffer */
	struct kgsl_mem_entry *entry;

	entry = kgsl_sharedmem_find_region(dev_priv->process_priv,
					   gpuaddr, sizedwords * sizeof(uint));
	if (entry == NULL) {
		KGSL_CMD_ERR(dev_priv->device,
			     "no mapping for gpuaddr: 0x%08x\n", gpuaddr);
		return false;
	}

	hostaddr = (uint *)kgsl_gpuaddr_to_vaddr(&entry->memdesc, gpuaddr);
	if (hostaddr == NULL) {
		KGSL_CMD_ERR(dev_priv->device,
			     "no mapping for gpuaddr: 0x%08x\n", gpuaddr);
		return false;
	}

	hoststart = hostaddr;

	level++;

	KGSL_CMD_INFO(dev_priv->device, "ib: gpuaddr:0x%08x, wc:%d, hptr:%p\n",
		gpuaddr, sizedwords, hostaddr);

	mb();
	while (dwords_left > 0) {
		bool cur_ret = true;
		int count = 0; /* dword count including packet header */

		switch (*hostaddr >> 30) {
		case 0x0: /* type-0 */
			count = (*hostaddr >> 16)+2;
			cur_ret = _handle_type0(dev_priv, hostaddr);
			break;
		case 0x1: /* type-1 */
			count = 2;
			break;
		case 0x3: /* type-3 */
			count = ((*hostaddr >> 16) & 0x3fff) + 2;
			cur_ret = _handle_type3(dev_priv, hostaddr);
			break;
		default:
			KGSL_CMD_ERR(dev_priv->device, "unexpected type: "
				"type:%d, word:0x%08x @ 0x%p, gpu:0x%08x\n",
				*hostaddr >> 30, *hostaddr, hostaddr,
				gpuaddr+4*(sizedwords-dwords_left));
			cur_ret = false;
			count = dwords_left;
			break;
		}

		if (!cur_ret) {
			KGSL_CMD_ERR(dev_priv->device,
				"bad sub-type: #:%d/%d, v:0x%08x"
				" @ 0x%p[gb:0x%08x], level:%d\n",
				sizedwords-dwords_left, sizedwords, *hostaddr,
				hostaddr, gpuaddr+4*(sizedwords-dwords_left),
				level);

			if (ADRENO_DEVICE(dev_priv->device)->ib_check_level
				>= 2)
				print_hex_dump(KERN_ERR,
					level == 1 ? "IB1:" : "IB2:",
					DUMP_PREFIX_OFFSET, 32, 4, hoststart,
					sizedwords*4, 0);
			goto done;
		}

		/* jump to next packet */
		dwords_left -= count;
		hostaddr += count;
		if (dwords_left < 0) {
			KGSL_CMD_ERR(dev_priv->device,
				"bad count: c:%d, #:%d/%d, "
				"v:0x%08x @ 0x%p[gb:0x%08x], level:%d\n",
				count, sizedwords-(dwords_left+count),
				sizedwords, *(hostaddr-count), hostaddr-count,
				gpuaddr+4*(sizedwords-(dwords_left+count)),
				level);
			if (ADRENO_DEVICE(dev_priv->device)->ib_check_level
				>= 2)
				print_hex_dump(KERN_ERR,
					level == 1 ? "IB1:" : "IB2:",
					DUMP_PREFIX_OFFSET, 32, 4, hoststart,
					sizedwords*4, 0);
			goto done;
		}
	}

	ret = true;
done:
	if (!ret)
		KGSL_DRV_ERR(dev_priv->device,
			"parsing failed: gpuaddr:0x%08x, "
			"host:0x%p, wc:%d\n", gpuaddr, hoststart, sizedwords);

	level--;

	return ret;
}

int
adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_ibdesc *ibdesc,
				unsigned int numibs,
				uint32_t *timestamp,
				unsigned int flags)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int *link = 0;
	unsigned int *cmds;
	unsigned int i, cmdflags;
	struct adreno_context *drawctxt = NULL;
	unsigned int start_index = 0;
	int ret;

	if (device->state & KGSL_STATE_HUNG) {
		ret = -EBUSY;
		goto done;
	}

	if (!(adreno_dev->ringbuffer.flags & KGSL_FLAGS_STARTED) ||
	      context == NULL || ibdesc == 0 || numibs == 0) {
		ret = -EINVAL;
		goto done;
	}
	drawctxt = ADRENO_CONTEXT(context);

	if (drawctxt->flags & CTXT_FLAGS_GPU_HANG) {
		ret = -EDEADLK;
		goto done;
	}

	cmdflags = (flags & KGSL_CMD_FLAGS_EOF);

	/* process any profiling results that are available into the log_buf */
	adreno_profile_process_results(device);

	/*When preamble is enabled, the preamble buffer with state restoration
	commands are stored in the first node of the IB chain. We can skip that
	if a context switch hasn't occured */

	if (drawctxt->flags & CTXT_FLAGS_PREAMBLE &&
		adreno_dev->drawctxt_active == drawctxt)
		start_index = 1;

	if (drawctxt->flags & CTXT_FLAGS_SKIP_EOF) {
		if (flags & KGSL_CMD_FLAGS_EOF)
			drawctxt->flags &= ~CTXT_FLAGS_SKIP_EOF;
		if (start_index)
			numibs = 1;
		else
			numibs = 0;
	}

	cmds = link = kzalloc(sizeof(unsigned int) * (numibs * 3 + 4),
				GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto done;
	}

	if (!start_index) {
		*cmds++ = cp_nop_packet(1);
		*cmds++ = KGSL_START_OF_IB_IDENTIFIER;
	} else {
		*cmds++ = cp_nop_packet(4);
		*cmds++ = KGSL_START_OF_IB_IDENTIFIER;
		*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
		*cmds++ = ibdesc[0].gpuaddr;
		*cmds++ = ibdesc[0].sizedwords;
	}
	for (i = start_index; i < numibs; i++) {
		if (unlikely(adreno_dev->ib_check_level >= 1 &&
		    !_parse_ibs(dev_priv, ibdesc[i].gpuaddr,
				ibdesc[i].sizedwords))) {
			ret = -EINVAL;
			goto done;
		}

		if (ibdesc[i].sizedwords == 0) {
			ret = -EINVAL;
			goto done;
		}

		*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
		*cmds++ = ibdesc[i].gpuaddr;
		*cmds++ = ibdesc[i].sizedwords;
	}

	*cmds++ = cp_nop_packet(1);
	*cmds++ = KGSL_END_OF_IB_IDENTIFIER;

	kgsl_setstate(&device->mmu, context->id,
		      kgsl_mmu_pt_get_flags(device->mmu.hwpagetable,
					device->id));

	adreno_drawctxt_switch(adreno_dev, drawctxt, flags);

	if (test_and_clear_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv) &&
		test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
			cmdflags |= KGSL_CMD_FLAGS_PWRON_FIXUP;

	if (drawctxt->flags & CTXT_FLAGS_USER_GENERATED_TS) {
		if (timestamp_cmp(drawctxt->timestamp, *timestamp) >= 0) {
			KGSL_DRV_ERR(device,
				"Invalid user generated ts <%d:0x%x>, "
				"less than last issued ts <%d:0x%x>\n",
				context->id, *timestamp, context->id,
				drawctxt->timestamp);
			return -ERANGE;
		}
		drawctxt->timestamp = *timestamp;
	} else
		drawctxt->timestamp++;

	ret = adreno_ringbuffer_addcmds(&adreno_dev->ringbuffer,
					drawctxt,
					cmdflags,
					&link[0], (cmds - link));
	if (ret)
		goto done;

	if (drawctxt->flags & CTXT_FLAGS_PER_CONTEXT_TS)
		*timestamp = drawctxt->timestamp;
	else
		*timestamp = adreno_dev->ringbuffer.global_ts;

#ifdef CONFIG_MSM_KGSL_CFF_DUMP
	/*
	 * insert wait for idle after every IB1
	 * this is conservative but works reliably and is ok
	 * even for performance simulations
	 */
	adreno_idle(device);
#endif

	/*
	 * If context hung and recovered then return error so that the
	 * application may handle it
	 */
	if (drawctxt->flags & CTXT_FLAGS_GPU_HANG_FT) {
		drawctxt->flags &= ~CTXT_FLAGS_GPU_HANG_FT;
		ret = -EPROTO;
	} else
		ret = 0;

done:
	device->pwrctrl.irq_last = 0;
	kgsl_trace_issueibcmds(device, context ? context->id : 0, ibdesc,
		numibs, *timestamp, flags, ret,
		drawctxt ? drawctxt->type : 0);

	kfree(link);
	return ret;
}

static void _turn_preamble_on_for_ib_seq(struct adreno_ringbuffer *rb,
				unsigned int rb_rptr)
{
	unsigned int temp_rb_rptr = rb_rptr;
	unsigned int size = rb->buffer_desc.size;
	unsigned int val[2];
	int i = 0;
	bool check = false;
	bool cmd_start = false;

	/* Go till the start of the ib sequence and turn on preamble */
	while (temp_rb_rptr / sizeof(unsigned int) != rb->wptr) {
		kgsl_sharedmem_readl(&rb->buffer_desc, &val[i], temp_rb_rptr);
		if (check && KGSL_START_OF_IB_IDENTIFIER == val[i]) {
			/* decrement i */
			i = (i + 1) % 2;
			if (val[i] == cp_nop_packet(4)) {
				temp_rb_rptr = adreno_ringbuffer_dec_wrapped(
						temp_rb_rptr, size);
				kgsl_sharedmem_writel(rb->device,
					&rb->buffer_desc,
					temp_rb_rptr, cp_nop_packet(1));
			}
			KGSL_FT_INFO(rb->device,
			"Turned preamble on at offset 0x%x\n",
			temp_rb_rptr / 4);
			break;
		}
		/* If you reach beginning of next command sequence then exit
		 * First command encountered is the current one so don't break
		 * on that. */
		if (KGSL_CMD_IDENTIFIER == val[i]) {
			if (cmd_start)
				break;
			cmd_start = true;
		}

		i = (i + 1) % 2;
		if (1 == i)
			check = true;
		temp_rb_rptr = adreno_ringbuffer_inc_wrapped(temp_rb_rptr,
								size);
	}
}

void adreno_ringbuffer_extract(struct adreno_ringbuffer *rb,
				struct adreno_ft_data *ft_data)
{
	struct kgsl_device *device = rb->device;
	unsigned int rb_rptr = ft_data->start_of_replay_cmds;
	unsigned int good_rb_idx = 0, bad_rb_idx = 0, temp_rb_idx = 0;
	unsigned int last_good_cmd_end_idx = 0, last_bad_cmd_end_idx = 0;
	unsigned int cmd_start_idx = 0;
	unsigned int val1 = 0;
	int copy_rb_contents = 0;
	unsigned int temp_rb_rptr;
	struct kgsl_context *k_ctxt;
	struct adreno_context *a_ctxt;
	unsigned int size = rb->buffer_desc.size;
	unsigned int *temp_rb_buffer = ft_data->rb_buffer;
	int *rb_size = &ft_data->rb_size;
	unsigned int *bad_rb_buffer = ft_data->bad_rb_buffer;
	int *bad_rb_size = &ft_data->bad_rb_size;
	unsigned int *good_rb_buffer = ft_data->good_rb_buffer;
	int *good_rb_size = &ft_data->good_rb_size;

	/*
	 * If the start index from where commands need to be copied is invalid
	 * then no need to save off any commands
	 */
	if (0xFFFFFFFF == ft_data->start_of_replay_cmds)
		return;

	k_ctxt = kgsl_context_get(device, ft_data->context_id);

	if (k_ctxt) {
		a_ctxt = ADRENO_CONTEXT(k_ctxt);
		if (a_ctxt->flags & CTXT_FLAGS_PREAMBLE)
			_turn_preamble_on_for_ib_seq(rb, rb_rptr);
		kgsl_context_put(k_ctxt);
	}
	k_ctxt = NULL;

	/* Walk the rb from the context switch. Omit any commands
	 * for an invalid context. */
	while ((rb_rptr / sizeof(unsigned int)) != rb->wptr) {
		kgsl_sharedmem_readl(&rb->buffer_desc, &val1, rb_rptr);

		if (KGSL_CMD_IDENTIFIER == val1) {
			/* Start is the NOP dword that comes before
			 * KGSL_CMD_IDENTIFIER */
			cmd_start_idx = temp_rb_idx - 1;
			if ((copy_rb_contents) && (good_rb_idx))
				last_good_cmd_end_idx = good_rb_idx - 1;
			if ((!copy_rb_contents) && (bad_rb_idx))
				last_bad_cmd_end_idx = bad_rb_idx - 1;
		}

		/* check for context switch indicator */
		if (val1 == KGSL_CONTEXT_TO_MEM_IDENTIFIER) {
			unsigned int temp_idx, val2;
			/* increment by 3 to get to the context_id */
			temp_rb_rptr = rb_rptr + (3 * sizeof(unsigned int)) %
					size;
			kgsl_sharedmem_readl(&rb->buffer_desc, &val2,
						temp_rb_rptr);

			/* if context switches to a context that did not cause
			 * hang then start saving the rb contents as those
			 * commands can be executed */
			k_ctxt = kgsl_context_get(rb->device, val2);

			if (k_ctxt) {
				a_ctxt = ADRENO_CONTEXT(k_ctxt);

			/* If we are changing to a good context and were not
			 * copying commands then copy over commands to the good
			 * context */
			if (!copy_rb_contents && ((k_ctxt &&
				!(a_ctxt->flags & CTXT_FLAGS_GPU_HANG)) ||
				!k_ctxt)) {
				for (temp_idx = cmd_start_idx;
					temp_idx < temp_rb_idx;
					temp_idx++)
					good_rb_buffer[good_rb_idx++] =
						temp_rb_buffer[temp_idx];
				ft_data->last_valid_ctx_id = val2;
				copy_rb_contents = 1;
				/* remove the good commands from bad buffer */
				bad_rb_idx = last_bad_cmd_end_idx;
			} else if (copy_rb_contents && k_ctxt &&
				(a_ctxt->flags & CTXT_FLAGS_GPU_HANG)) {

				/* If we are changing back to a bad context
				 * from good ctxt and were not copying commands
				 * to bad ctxt then copy over commands to
				 * the bad context */
				for (temp_idx = cmd_start_idx;
					temp_idx < temp_rb_idx;
					temp_idx++)
					bad_rb_buffer[bad_rb_idx++] =
						temp_rb_buffer[temp_idx];
				/* If we are changing to bad context then
				 * remove the dwords we copied for this
				 * sequence from the good buffer */
				good_rb_idx = last_good_cmd_end_idx;
				copy_rb_contents = 0;
			}
			}
			kgsl_context_put(k_ctxt);
		}

		if (copy_rb_contents)
			good_rb_buffer[good_rb_idx++] = val1;
		else
			bad_rb_buffer[bad_rb_idx++] = val1;

		/* Copy both good and bad commands to temp buffer */
		temp_rb_buffer[temp_rb_idx++] = val1;

		rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr, size);
	}
	*good_rb_size = good_rb_idx;
	*bad_rb_size = bad_rb_idx;
	*rb_size = temp_rb_idx;
}

void
adreno_ringbuffer_restore(struct adreno_ringbuffer *rb, unsigned int *rb_buff,
			int num_rb_contents)
{
	int i;
	unsigned int *ringcmds;
	unsigned int rcmd_gpu;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	if (!num_rb_contents)
		return;

	if (num_rb_contents > (rb->buffer_desc.size - rb->wptr)) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_RPTR, 0);
		BUG_ON(num_rb_contents > rb->buffer_desc.size);
	}
	ringcmds = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
	rcmd_gpu = rb->buffer_desc.gpuaddr + sizeof(unsigned int) * rb->wptr;
	for (i = 0; i < num_rb_contents; i++)
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, rb_buff[i]);
	rb->wptr += num_rb_contents;
	adreno_ringbuffer_submit(rb);
}
