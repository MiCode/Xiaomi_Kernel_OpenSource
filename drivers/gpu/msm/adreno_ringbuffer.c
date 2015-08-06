/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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
	unsigned int rptr;

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

		if (time_after(jiffies, wait_time)) {
			KGSL_DRV_ERR(rb->device,
			"Timed out while waiting for freespace in ringbuffer "
			"rptr: 0x%x, wptr: 0x%x\n", rptr, rb->wptr);
			return -ETIMEDOUT;
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
	} else
		ptr = ERR_PTR(ret);

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
 * @end: Ending index of pm4 ucode to load
 * @addr: Address to load the pm4 ucode
 *
 * Load the pm4 ucode from @start at @addr.
 */
inline int adreno_ringbuffer_load_pm4_ucode(struct kgsl_device *device,
			unsigned int start, unsigned int end, unsigned int addr)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_RAM_WADDR, addr);
	for (i = start; i < end; i++)
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
 * @end: Ending index of pfp ucode to load
 * @addr: Address to load the pfp ucode
 *
 * Load the pfp ucode from @start at @addr.
 */
inline int adreno_ringbuffer_load_pfp_ucode(struct kgsl_device *device,
			unsigned int start, unsigned int end, unsigned int addr)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR, addr);
	for (i = start; i < end; i++)
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
						adreno_dev->pfp_fw[i]);

	return 0;
}

/**
 * _ringbuffer_bootstrap_ucode() - Bootstrap GPU Ucode
 * @rb: Pointer to adreno ringbuffer
 * @load_jt: If non zero only load Jump tables
 *
 * Bootstrap ucode for GPU
 * load_jt == 0, bootstrap full microcode
 * load_jt == 1, bootstrap jump tables of microcode
 *
 * For example a bootstrap packet would like below
 * Setup a type3 bootstrap packet
 * PFP size to bootstrap
 * PFP addr to write the PFP data
 * PM4 size to bootstrap
 * PM4 addr to write the PM4 data
 * PFP dwords from microcode to bootstrap
 * PM4 size dwords from microcode to bootstrap
 */
static int _ringbuffer_bootstrap_ucode(struct adreno_ringbuffer *rb,
					unsigned int load_jt)
{
	unsigned int *cmds, cmds_gpu, bootstrap_size;
	int i = 0;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int pm4_size, pm4_idx, pm4_addr, pfp_size, pfp_idx, pfp_addr;

	/* Only bootstrap jump tables of ucode */
	if (load_jt) {
		pm4_idx = adreno_dev->pm4_jt_idx;
		pm4_addr = adreno_dev->pm4_jt_addr;
		pfp_idx = adreno_dev->pfp_jt_idx;
		pfp_addr = adreno_dev->pfp_jt_addr;
	} else {
		/* Bootstrap full ucode */
		pm4_idx = 1;
		pm4_addr = 0;
		pfp_idx = 1;
		pfp_addr = 0;
	}

	pm4_size = (adreno_dev->pm4_fw_size - pm4_idx);
	pfp_size = (adreno_dev->pfp_fw_size - pfp_idx);

	/*
	 * Below set of commands register with PFP that 6f is the
	 * opcode for bootstrapping
	 */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR, 0x200);
	adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA, 0x6f0005);

	/* clear ME_HALT to start micro engine */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

	bootstrap_size = (pm4_size + pfp_size + 5);

	cmds = adreno_ringbuffer_allocspace(rb, NULL, bootstrap_size);
	if (cmds == NULL)
			return -ENOMEM;

	cmds_gpu = rb->buffer_desc.gpuaddr +
			sizeof(uint) * (rb->wptr - bootstrap_size);
	/* Construct the packet that bootsraps the ucode */
	GSL_RB_WRITE(rb->device, cmds, cmds_gpu,
			cp_type3_packet(CP_BOOTSTRAP_UCODE,
			(bootstrap_size - 1)));
	GSL_RB_WRITE(rb->device, cmds, cmds_gpu, pfp_size);
	GSL_RB_WRITE(rb->device, cmds, cmds_gpu, pfp_addr);
	GSL_RB_WRITE(rb->device, cmds, cmds_gpu, pm4_size);
	GSL_RB_WRITE(rb->device, cmds, cmds_gpu, pm4_addr);
	for (i = pfp_idx; i < adreno_dev->pfp_fw_size; i++)
		GSL_RB_WRITE(rb->device, cmds, cmds_gpu, adreno_dev->pfp_fw[i]);
	for (i = pm4_idx; i < adreno_dev->pm4_fw_size; i++)
		GSL_RB_WRITE(rb->device, cmds, cmds_gpu, adreno_dev->pm4_fw[i]);

	adreno_ringbuffer_submit(rb);
	/* idle device to validate bootstrap */
	return adreno_idle(device);
}

/**
 * _ringbuffer_setup_common() - Ringbuffer start
 * @rb: Pointer to adreno ringbuffer
 *
 * Setup ringbuffer for GPU.
 */
void _ringbuffer_setup_common(struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	kgsl_sharedmem_set(rb->device, &rb->buffer_desc, 0, 0xAA,
			   (rb->sizedwords << 2));

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 * Also disable the host RPTR shadow register as it might be unreliable
	 * in certain circumstances.
	 */

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL,
		(ilog2(rb->sizedwords >> 1) & 0x3F) |
		(1 << 27));

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
					rb->buffer_desc.gpuaddr);

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
}

/**
 * _ringbuffer_start_common() - Ringbuffer start
 * @rb: Pointer to adreno ringbuffer
 *
 * Start ringbuffer for GPU.
 */
int _ringbuffer_start_common(struct adreno_ringbuffer *rb)
{
	int status;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

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

	if (rb->flags & KGSL_FLAGS_STARTED)
		return 0;

	_ringbuffer_setup_common(rb);

	/* If bootstrapping if supported to load jump tables */
	if (adreno_bootstrap_ucode(adreno_dev)) {
		status = _ringbuffer_bootstrap_ucode(rb, 1);
		if (status != 0)
			return status;

	} else {
		/* load the CP jump tables using AHB writes */
		status = adreno_ringbuffer_load_pm4_ucode(device,
			adreno_dev->pm4_jt_idx, adreno_dev->pm4_fw_size,
			adreno_dev->pm4_jt_addr);
		if (status != 0)
			return status;

		/* load the prefetch parser jump tables using AHB writes */
		status = adreno_ringbuffer_load_pfp_ucode(device,
			adreno_dev->pfp_jt_idx, adreno_dev->pfp_fw_size,
			adreno_dev->pfp_jt_addr);
		if (status != 0)
			return status;
	}

	status = _ringbuffer_start_common(rb);

	return status;
}

/**
 * adreno_ringbuffer_cold_start() - Ringbuffer cold start
 * @rb: Pointer to adreno ringbuffer
 *
 * Start the ringbuffer from power collapse.
 */
int adreno_ringbuffer_cold_start(struct adreno_ringbuffer *rb)
{
	int status;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (rb->flags & KGSL_FLAGS_STARTED)
		return 0;

	_ringbuffer_setup_common(rb);

	/* If bootstrapping if supported to load ucode */
	if (adreno_bootstrap_ucode(adreno_dev)) {

		/*
		 * load first adreno_dev->pm4_bstrp_size +
		 * adreno_dev->pfp_bstrp_size microcode dwords using AHB write,
		 * this small microcode has dispatcher + booter, this initial
		 * microcode enables CP to understand CP_BOOTSTRAP_UCODE packet
		 * in function _ringbuffer_bootstrap_ucode. CP_BOOTSTRAP_UCODE
		 * packet loads rest of the microcode.
		 */

		status = adreno_ringbuffer_load_pm4_ucode(rb->device, 1,
					adreno_dev->pm4_bstrp_size+1, 0);
		if (status != 0)
			return status;

		status = adreno_ringbuffer_load_pfp_ucode(rb->device, 1,
					adreno_dev->pfp_bstrp_size+1, 0);
		if (status != 0)
			return status;

		/* Bootstrap rest of the ucode here */
		status = _ringbuffer_bootstrap_ucode(rb, 0);
		if (status != 0)
			return status;

	} else {
		/* load the CP ucode using AHB writes */
		status = adreno_ringbuffer_load_pm4_ucode(rb->device, 1,
					adreno_dev->pm4_fw_size, 0);
		if (status != 0)
			return status;

		/* load the prefetch parser ucode using AHB writes */
		status = adreno_ringbuffer_load_pfp_ucode(rb->device, 1,
					adreno_dev->pfp_fw_size, 0);
		if (status != 0)
			return status;
	}

	status = _ringbuffer_start_common(rb);

	return status;
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

	rb->global_ts = 0;

	return 0;
}

void adreno_ringbuffer_close(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);

	kgsl_sharedmem_free(&rb->buffer_desc);

	kfree(adreno_dev->pfp_fw);
	kfree(adreno_dev->pm4_fw);

	adreno_dev->pfp_fw = NULL;
	adreno_dev->pm4_fw = NULL;

	memset(rb, 0, sizeof(struct adreno_ringbuffer));
}

static int
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				struct adreno_context *drawctxt,
				unsigned int flags, unsigned int *cmds,
				int sizedwords, uint32_t timestamp)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	unsigned int *ringcmds;
	unsigned int total_sizedwords = sizedwords;
	unsigned int i;
	unsigned int rcmd_gpu;
	unsigned int context_id;
	unsigned int gpuaddr = rb->device->memstore.gpuaddr;
	bool profile_ready;

	if (drawctxt != NULL && kgsl_context_detached(&drawctxt->base))
		return -EINVAL;

	rb->global_ts++;

	/* If this is a internal IB, use the global timestamp for it */
	if (!drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		timestamp = rb->global_ts;
		context_id = KGSL_MEMSTORE_GLOBAL;
	} else {
		context_id = drawctxt->base.id;
	}

	/*
	 * Note that we cannot safely take drawctxt->mutex here without
	 * potential mutex inversion with device->mutex which is held
	 * here. As a result, any other code that accesses this variable
	 * must also use device->mutex.
	 */
	if (drawctxt)
		drawctxt->internal_timestamp = rb->global_ts;

	/*
	 * If in stream ib profiling is enabled and there are counters
	 * assigned, then space needs to be reserved for profiling.  This
	 * space in the ringbuffer is always consumed (might be filled with
	 * NOPs in error case.  profile_ready needs to be consistent through
	 * the _addcmds call since it is allocating additional ringbuffer
	 * command space.
	 */
	profile_ready = !adreno_is_a2xx(adreno_dev) && drawctxt &&
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

	/* Add two dwords for the CP_INTERRUPT */
	total_sizedwords +=
		(drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) ?  2 : 0;

	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 7;

	if (adreno_is_a2xx(adreno_dev))
		total_sizedwords += 2; /* CP_WAIT_FOR_IDLE */

	total_sizedwords += 3; /* sop timestamp */
	total_sizedwords += 4; /* eop timestamp */

	if (adreno_is_a20x(adreno_dev))
		total_sizedwords += 2; /* CACHE_FLUSH */

	if (drawctxt) {
		total_sizedwords += 3; /* global timestamp without cache
					* flush for non-zero context */
	}

	if (adreno_is_a20x(adreno_dev))
		total_sizedwords += 2; /* CACHE_FLUSH */

	if (flags & KGSL_CMD_FLAGS_WFI)
		total_sizedwords += 2; /* WFI */

	if (profile_ready)
		total_sizedwords += 6;   /* space for pre_ib and post_ib */

	/* Add space for the power on shader fixup if we need it */
	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP)
		total_sizedwords += 9;

	ringcmds = adreno_ringbuffer_allocspace(rb, drawctxt, total_sizedwords);

	if (IS_ERR(ringcmds))
		return PTR_ERR(ringcmds);
	if (ringcmds == NULL)
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
		/* Disable protected mode for the fixup */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0);

		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, cp_nop_packet(1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
				KGSL_PWRON_FIXUP_IDENTIFIER);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			CP_HDR_INDIRECT_BUFFER_PFD);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			adreno_dev->pwron_fixup.gpuaddr);
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			adreno_dev->pwron_fixup_dwords);

		/* Re-enable protected mode */
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 1);
	}

	/* Add any IB required for profiling if it is enabled */
	if (profile_ready)
		adreno_profile_preib_processing(rb->device, drawctxt->base.id,
				&flags, &ringcmds, &rcmd_gpu);

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

	if (drawctxt) {
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

	if (drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
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

	if (flags & KGSL_CMD_FLAGS_WFI) {
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu,
			cp_type3_packet(CP_WAIT_FOR_IDLE, 1));
		GSL_RB_WRITE(rb->device, ringcmds, rcmd_gpu, 0x00000000);
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

	flags |= KGSL_CMD_FLAGS_INTERNAL_ISSUE;

	return adreno_ringbuffer_addcmds(rb, drawctxt, flags, cmds,
		sizedwords, 0);
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

/**
 * _ringbuffer_verify_ib() - parse an IB and verify that it is correct
 * @dev_priv: Pointer to the process struct
 * @ibdesc: Pointer to the IB descriptor
 *
 * This function only gets called if debugging is enabled  - it walks the IB and
 * does additional level parsing and verification above and beyond what KGSL
 * core does
 */
static inline bool _ringbuffer_verify_ib(struct kgsl_device_private *dev_priv,
		struct kgsl_memobj_node *ib)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Check that the size of the IBs is under the allowable limit */
	if (ib->sizedwords == 0 || ib->sizedwords > 0xFFFFF) {
		KGSL_DRV_ERR(device, "Invalid IB size 0x%X\n",
				ib->sizedwords);
		return false;
	}

	if (unlikely(adreno_dev->ib_check_level >= 1) &&
		!_parse_ibs(dev_priv, ib->gpuaddr, ib->sizedwords)) {
		KGSL_DRV_ERR(device, "Could not verify the IBs\n");
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

	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		return -EDEADLK;

	/* Verify the IBs before they get queued */
	list_for_each_entry(ib, &cmdbatch->cmdlist, node)
		if (!_ringbuffer_verify_ib(dev_priv, ib))
			return -EINVAL;

	/* wait for the suspend gate */
	wait_for_completion(&device->cmdbatch_gate);

	/*
	 * Clear the wake on touch bit to indicate an IB has been submitted
	 * since the last time we set it
	 */

	device->flags &= ~KGSL_FLAG_WAKE_ON_TOUCH;

	/* Queue the command in the ringbuffer */
	ret = adreno_dispatcher_queue_cmd(adreno_dev, drawctxt, cmdbatch,
		timestamp);

	if (ret)
		KGSL_DRV_ERR(device,
			"adreno_dispatcher_queue_cmd returned %d\n", ret);

	/*
	 * Return -EPROTO if the device has faulted since the last time we
	 * checked - userspace uses this to perform post-fault activities
	 */
	if (!ret && test_and_clear_bit(ADRENO_CONTEXT_FAULT, &drawctxt->priv))
		ret = -EPROTO;

	return ret;
}

unsigned int adreno_ringbuffer_get_constraint(struct kgsl_device *device,
				struct kgsl_context *context)
{
	unsigned int pwrlevel = device->pwrctrl.active_pwrlevel;

	switch (context->pwr_constraint.type) {
	case KGSL_CONSTRAINT_PWRLEVEL: {
		switch (context->pwr_constraint.sub_type) {
		case KGSL_CONSTRAINT_PWR_MAX:
			pwrlevel = device->pwrctrl.max_pwrlevel;
			break;
		case KGSL_CONSTRAINT_PWR_MIN:
			pwrlevel = device->pwrctrl.min_pwrlevel;
			break;
		default:
			break;
		}
	}
	break;

	}

	return pwrlevel;
}

void adreno_ringbuffer_set_constraint(struct kgsl_device *device,
			struct kgsl_cmdbatch *cmdbatch)
{
	unsigned int constraint;
	struct kgsl_context *context = cmdbatch->context;
	/*
	 * Check if the context has a constraint and constraint flags are
	 * set.
	 */
	if (context->pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(cmdbatch->flags & KGSL_CMDBATCH_PWR_CONSTRAINT))) {

		constraint = adreno_ringbuffer_get_constraint(device, context);

		/*
		 * If a constraint is already set, set a new
		 * constraint only if it is faster
		 */
		if ((device->pwrctrl.constraint.type ==
			KGSL_CONSTRAINT_NONE) || (constraint <
			device->pwrctrl.constraint.hint.pwrlevel.level)) {

			kgsl_pwrctrl_pwrlevel_change(device, constraint);
			device->pwrctrl.constraint.type =
					context->pwr_constraint.type;
			device->pwrctrl.constraint.hint.
					pwrlevel.level = constraint;
		}

		device->pwrctrl.constraint.expires = jiffies +
			device->pwrctrl.interval_timeout;
	}

}

/* adreno_rindbuffer_submitcmd - submit userspace IBs to the GPU */
int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_memobj_node *ib;
	unsigned int numibs = 0;
	unsigned int *link;
	unsigned int *cmds;
	struct kgsl_context *context;
	struct adreno_context *drawctxt;
	bool use_preamble = true;
	int flags = KGSL_CMD_FLAGS_NONE;
	int ret;

	context = cmdbatch->context;
	drawctxt = ADRENO_CONTEXT(context);

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdbatch->cmdlist, node)
		numibs++;

	/* process any profiling results that are available into the log_buf */
	adreno_profile_process_results(device);

	/*
	 * If SKIP CMD flag is set for current context
	 * a) set SKIPCMD as fault_recovery for current commandbatch
	 * b) store context's commandbatch fault_policy in current
	 *    commandbatch fault_policy and clear context's commandbatch
	 *    fault_policy
	 * c) force preamble for commandbatch
	 */
	if (test_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->priv) &&
		(!test_bit(CMDBATCH_FLAG_SKIP, &cmdbatch->priv))) {

		set_bit(KGSL_FT_SKIPCMD, &cmdbatch->fault_recovery);
		cmdbatch->fault_policy = drawctxt->fault_policy;
		set_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &cmdbatch->priv);

		/* if context is detached print fault recovery */
		adreno_fault_skipcmd_detached(device, drawctxt, cmdbatch);

		/* clear the drawctxt flags */
		clear_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->priv);
		drawctxt->fault_policy = 0;
	}

	/*When preamble is enabled, the preamble buffer with state restoration
	commands are stored in the first node of the IB chain. We can skip that
	if a context switch hasn't occured */

	if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) &&
		!test_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &cmdbatch->priv) &&
		(adreno_dev->drawctxt_active == drawctxt))
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
	 * Worst case size:
	 * 2 - start of IB identifier
	 * 1 - skip preamble
	 * 3 * numibs - 3 per IB
	 * 2 - end of IB identifier
	 */
	cmds = link = kzalloc(sizeof(unsigned int) * (numibs * 3 + 5),
				GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto done;
	}

	*cmds++ = cp_nop_packet(1);
	*cmds++ = KGSL_START_OF_IB_IDENTIFIER;

	if (numibs) {
		list_for_each_entry(ib, &cmdbatch->cmdlist, node) {
			/* use the preamble? */
			if ((ib->priv & MEMOBJ_PREAMBLE) &&
					(use_preamble == false))
				*cmds++ = cp_nop_packet(3);
			/*
			 * Skip 0 sized IBs - these are presumed to have been
			 * removed from consideration by the FT policy
			 */

			if (ib->priv & MEMOBJ_SKIP)
				*cmds++ = cp_nop_packet(2);
			else
				*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;

			*cmds++ = ib->gpuaddr;
			*cmds++ = ib->sizedwords;
		}
	}

	*cmds++ = cp_nop_packet(1);
	*cmds++ = KGSL_END_OF_IB_IDENTIFIER;

	ret = kgsl_setstate(&device->mmu, context->id,
		      kgsl_mmu_pt_get_flags(device->mmu.hwpagetable,
					device->id));

	if (ret)
		goto done;

	ret = adreno_drawctxt_switch(adreno_dev, drawctxt, cmdbatch->flags);

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

	ret = adreno_ringbuffer_addcmds(&adreno_dev->ringbuffer,
					drawctxt,
					flags,
					&link[0], (cmds - link),
					cmdbatch->timestamp);

#ifdef CONFIG_MSM_KGSL_CFF_DUMP
	if (ret)
		goto done;
	/*
	 * insert wait for idle after every IB1
	 * this is conservative but works reliably and is ok
	 * even for performance simulations
	 */
	ret = adreno_idle(device);
#endif

done:
	device->pwrctrl.irq_last = 0;
	kgsl_trace_issueibcmds(device, context->id, cmdbatch,
		numibs, cmdbatch->timestamp, cmdbatch->flags, ret,
		drawctxt->type);

	kfree(link);
	return ret;
}
