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
#include "kgsl_trace.h"
#include "kgsl_pwrctrl.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"

#include "a3xx_reg.h"
#include "adreno_a4xx.h"

#define ADRENO_NUM_RINGBUFFERS 1
#define GSL_RB_NOP_SIZEDWORDS				2

#define RB_HOSTPTR(_rb, _pos) \
	((unsigned int *) ((_rb)->buffer_desc.hostptr + \
		((_pos) * sizeof(unsigned int))))

#define RB_GPUADDR(_rb, _pos) \
	((_rb)->buffer_desc.gpuaddr + ((_pos) * sizeof(unsigned int)))

static void _cff_write_ringbuffer(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int gpuaddr;
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

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	BUG_ON(rb->wptr == 0);

	/* Let the pwrscale policy know that new commands have
	 been submitted. */
	kgsl_pwrscale_busy(rb->device);

	/* Write the changes to CFF if so enabled */
	_cff_write_ringbuffer(rb);

	/* Memory barrier before informing the hardware of new commands */
	mb();

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_WPTR, rb->wptr);
}

static int
adreno_ringbuffer_waitspace(struct adreno_ringbuffer *rb,
				unsigned int numcmds, int wptr_ahead)
{
	int nopcount;
	unsigned int freecmds;
	unsigned int *cmds;
	unsigned int gpuaddr;
	unsigned long wait_time;
	unsigned long wait_timeout = msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);
	unsigned long wait_time_part;
	unsigned int rptr;

	/* if wptr ahead, fill the remaining with NOPs */
	if (wptr_ahead) {
		/* -1 for header */
		nopcount = KGSL_RB_DWORDS - rb->wptr - 1;

		cmds = RB_HOSTPTR(rb, rb->wptr);
		gpuaddr = RB_GPUADDR(rb, rb->wptr);

		*cmds = cp_nop_packet(nopcount);
		kgsl_cffdump_write(rb->device, gpuaddr, *cmds);

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
	}

	release_firmware(fw);
	return (*data != NULL) ? 0 : -ENOMEM;
}

void adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (adreno_dev->pm4_fw == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			adreno_dev->gpucore->pm4fw_name, &ptr, &len);

		if (ret)
			goto err;

		/* PM4 size is 3 dword aligned plus 1 dword of version */
		if (len % ((sizeof(uint32_t) * 3)) != sizeof(uint32_t)) {
			KGSL_DRV_ERR(device, "Bad pm4 microcode size: %d\n",
				len);
			kfree(ptr);
			goto err;
		}

		adreno_dev->pm4_fw_size = len / sizeof(uint32_t);
		adreno_dev->pm4_fw = ptr;
		adreno_dev->pm4_fw_version = adreno_dev->pm4_fw[1];
	}

	return;

err:
	KGSL_DRV_FATAL(device, "Failed to read pm4 microcode %s\n",
		adreno_dev->gpucore->pm4fw_name);
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
static inline int adreno_ringbuffer_load_pm4_ucode(struct kgsl_device *device,
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

void adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (adreno_dev->pfp_fw == NULL) {
		int len;
		void *ptr;

		int ret = _load_firmware(device,
			adreno_dev->gpucore->pfpfw_name, &ptr, &len);
		if (ret)
			goto err;

		/* PFP size shold be dword aligned */
		if (len % sizeof(uint32_t) != 0) {
			KGSL_DRV_ERR(device, "Bad PFP microcode size: %d\n",
				len);
			kfree(ptr);
			goto err;
		}

		adreno_dev->pfp_fw_size = len / sizeof(uint32_t);
		adreno_dev->pfp_fw = ptr;
		adreno_dev->pfp_fw_version = adreno_dev->pfp_fw[5];
	}

	return;

err:
	KGSL_DRV_FATAL(device, "Failed to read pfp microcode %s\n",
		adreno_dev->gpucore->pfpfw_name);
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
static inline int adreno_ringbuffer_load_pfp_ucode(struct kgsl_device *device,
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
	unsigned int *cmds, bootstrap_size;
	int i = 0;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int pm4_size, pm4_idx, pm4_addr, pfp_size, pfp_idx, pfp_addr;

	/* Only bootstrap jump tables of ucode */
	if (load_jt) {
		pm4_idx = adreno_dev->gpucore->pm4_jt_idx;
		pm4_addr = adreno_dev->gpucore->pm4_jt_addr;
		pfp_idx = adreno_dev->gpucore->pfp_jt_idx;
		pfp_addr = adreno_dev->gpucore->pfp_jt_addr;
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
	 * Overwrite the first entry in the jump table with the special
	 * bootstrap opcode
	 */

	if (adreno_is_a4xx(adreno_dev)) {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR,
			0x400);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
			 0x6f0009);
		bootstrap_size = (pm4_size + pfp_size + 5 + 6);
	} else {
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_ADDR,
			0x200);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_PFP_UCODE_DATA,
			 0x6f0005);
		bootstrap_size = (pm4_size + pfp_size + 5);
	}

	/* clear ME_HALT to start micro engine */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, 0);

	cmds = adreno_ringbuffer_allocspace(rb, bootstrap_size);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

	/* Construct the packet that bootsraps the ucode */
	*cmds++ = cp_type3_packet(CP_BOOTSTRAP_UCODE, (bootstrap_size - 1));
	*cmds++ = pfp_size;
	*cmds++ = pfp_addr;
	*cmds++ = pm4_size;
	*cmds++ = pm4_addr;

/**
 * Theory of operation:
 *
 * In A4x, we cannot have the PFP executing instructions while its instruction
 * RAM is loading. We load the PFP's instruction RAM using type-0 writes
 * from the ME.
 *
 * To make sure the PFP is not fetching instructions at the same time,
 * we put it in a one-instruction loop:
 *    mvc (ME), (ringbuffer)
 * which executes repeatedly until all of the data has been moved from
 * the ring buffer to the ME.
 */
	if (adreno_is_a4xx(adreno_dev)) {
		for (i = pm4_idx; i < adreno_dev->pm4_fw_size; i++)
			*cmds++ = adreno_dev->pm4_fw[i];
		for (i = pfp_idx; i < adreno_dev->pfp_fw_size; i++)
			*cmds++ = adreno_dev->pfp_fw[i];

		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = 0x20000000 + A4XX_CP_RB_WPTR;
		*cmds++ = 0xffffffff;
		*cmds++ = 0x00000002;
		*cmds++ = cp_type3_packet(CP_INTERRUPT, 1);
		*cmds++ = 0;

		rb->wptr = rb->wptr - 2;
		adreno_ringbuffer_submit(rb);
		rb->wptr = rb->wptr + 2;
	} else {
		for (i = pfp_idx; i < adreno_dev->pfp_fw_size; i++)
			*cmds++ = adreno_dev->pfp_fw[i];
		for (i = pm4_idx; i < adreno_dev->pm4_fw_size; i++)
			*cmds++ = adreno_dev->pm4_fw[i];
		adreno_ringbuffer_submit(rb);
	}

	/* idle device to validate bootstrap */
	return adreno_idle(device);
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
	int i;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		kgsl_sharedmem_set(rb->device,
			&(adreno_dev->ringbuffers[i].buffer_desc), 0,
			0xAA, KGSL_RB_SIZE);
		adreno_dev->ringbuffers[i].wptr = 0;
		adreno_dev->ringbuffers[i].rptr = 0;
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

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
					rb->buffer_desc.gpuaddr);

	/* setup scratch/timestamp */
	adreno_writereg(adreno_dev, ADRENO_REG_SCRATCH_ADDR,
				device->memstore.gpuaddr +
				KGSL_MEMSTORE_RB_OFFSET(rb,
					soptimestamp));

	adreno_writereg(adreno_dev, ADRENO_REG_SCRATCH_UMSK,
			     GSL_RB_MEMPTRS_SCRATCH_MASK);

	/* CP ROQ queue sizes (bytes) - RB:16, ST:16, IB1:32, IB2:64 */
	if (adreno_is_a305(adreno_dev) || adreno_is_a305c(adreno_dev) ||
		adreno_is_a306(adreno_dev) || adreno_is_a320(adreno_dev))
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
	struct kgsl_device *device = rb->device;

	_ringbuffer_setup_common(rb);

	/* If bootstrapping if supported to load jump tables */
	if (adreno_bootstrap_ucode(adreno_dev)) {
		status = _ringbuffer_bootstrap_ucode(rb, 1);
		if (status != 0)
			return status;

	} else {
		/* load the CP jump tables using AHB writes */
		status = adreno_ringbuffer_load_pm4_ucode(device,
			adreno_dev->gpucore->pm4_jt_idx,
			adreno_dev->pm4_fw_size,
			adreno_dev->gpucore->pm4_jt_addr);
		if (status != 0)
			return status;

		/* load the prefetch parser jump tables using AHB writes */
		status = adreno_ringbuffer_load_pfp_ucode(device,
			adreno_dev->gpucore->pfp_jt_idx,
			adreno_dev->pfp_fw_size,
			adreno_dev->gpucore->pfp_jt_addr);
		if (status != 0)
			return status;
	}

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


	_ringbuffer_setup_common(rb);

	/* If bootstrapping if supported to load ucode */
	if (adreno_bootstrap_ucode(adreno_dev)) {

		/*
		 * load first pm4_bstrp_size + pfp_bstrp_size microcode dwords
		 * using AHB write, this small microcode has dispatcher + booter
		 * this initial microcode enables CP to understand
		 * CP_BOOTSTRAP_UCODE packet in function
		 * _ringbuffer_bootstrap_ucode. CP_BOOTSTRAP_UCODE packet loads
		 * rest of the microcode.
		 */

		status = adreno_ringbuffer_load_pm4_ucode(rb->device, 1,
			adreno_dev->gpucore->pm4_bstrp_size+1, 0);
		if (status != 0)
			return status;

		status = adreno_ringbuffer_load_pfp_ucode(rb->device, 1,
			adreno_dev->gpucore->pfp_bstrp_size+1, 0);
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

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	int i;
	for (i = 0; i < adreno_dev->num_ringbuffers; i++)
		kgsl_cancel_events(device,
			&(adreno_dev->ringbuffers[i].mmu_events));
}

static int _adreno_ringbuffer_init(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb, int id)
{
	char name[64];

	rb->device = &adreno_dev->dev;
	rb->id = id;

	snprintf(name, sizeof(name), "rb_events-%d", id);
	kgsl_add_event_group(&rb->events, NULL, name,
		adreno_rb_readtimestamp, rb);
	snprintf(name, sizeof(name), "rb_mmu_events-%d", id);
	kgsl_add_event_group(&rb->mmu_events, NULL, name,
		adreno_rb_readtimestamp, rb);
	rb->timestamp = 0;

	return kgsl_allocate_global(&adreno_dev->dev, &rb->buffer_desc,
			KGSL_RB_SIZE, KGSL_MEMFLAGS_GPUREADONLY);
}

int adreno_ringbuffer_init(struct kgsl_device *device)
{
	int status = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int i;

	adreno_dev->num_ringbuffers = gpudev->num_prio_levels;
	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		status = _adreno_ringbuffer_init(adreno_dev,
			&(adreno_dev->ringbuffers[i]), i);
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
	if (rb->buffer_desc.hostptr)
		kgsl_free_global(&rb->buffer_desc);
	kgsl_del_event_group(&rb->events);
	kgsl_del_event_group(&rb->mmu_events);
	memset(rb, 0, sizeof(struct adreno_ringbuffer));
}

void adreno_ringbuffer_close(struct adreno_device *adreno_dev)
{
	int i;

	kfree(adreno_dev->pfp_fw);
	kfree(adreno_dev->pm4_fw);

	adreno_dev->pfp_fw = NULL;
	adreno_dev->pm4_fw = NULL;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++)
		_adreno_ringbuffer_close(&(adreno_dev->ringbuffers[i]));
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
	unsigned int context_id = 0;
	unsigned int gpuaddr = rb->device->memstore.gpuaddr;
	bool profile_ready;

	if (drawctxt != NULL && kgsl_context_detached(&drawctxt->base))
		return -EINVAL;

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
	if (drawctxt)
		drawctxt->internal_timestamp = rb->timestamp;

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

	/* Add two dwords for the CP_INTERRUPT */
	total_sizedwords +=
		(drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) ?  2 : 0;

	/* context rollover */
	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 3;

	/* For HLSQ updates below */
	if (adreno_is_a4xx(adreno_dev) || adreno_is_a3xx(adreno_dev))
		total_sizedwords += 4;

	total_sizedwords += 3; /* sop timestamp */
	total_sizedwords += 4; /* eop timestamp */

	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		total_sizedwords += 3; /* global timestamp without cache
					* flush for non-zero context */
	}

	if (flags & KGSL_CMD_FLAGS_WFI)
		total_sizedwords += 2; /* WFI */

	if (profile_ready)
		total_sizedwords += 6;   /* space for pre_ib and post_ib */

	/* Add space for the power on shader fixup if we need it */
	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP)
		total_sizedwords += 9;

	ringcmds = adreno_ringbuffer_allocspace(rb, total_sizedwords);
	if (IS_ERR(ringcmds))
		return PTR_ERR(ringcmds);
	if (ringcmds == NULL)
		return -ENOSPC;

	*ringcmds++ = cp_nop_packet(1);
	*ringcmds++ = KGSL_CMD_IDENTIFIER;

	if (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE) {
		*ringcmds++ = cp_nop_packet(1);
		*ringcmds++ = KGSL_CMD_INTERNAL_IDENTIFIER;
	}

	if (flags & KGSL_CMD_FLAGS_PWRON_FIXUP) {
		/* Disable protected mode for the fixup */
		*ringcmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 0;

		*ringcmds++ = cp_nop_packet(1);
		*ringcmds++ = KGSL_PWRON_FIXUP_IDENTIFIER;
		*ringcmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
		*ringcmds++ = adreno_dev->pwron_fixup.gpuaddr;
		*ringcmds++ = adreno_dev->pwron_fixup_dwords;

		/* Re-enable protected mode */
		*ringcmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 1;
	}

	/* Add any IB required for profiling if it is enabled */
	if (profile_ready)
		adreno_profile_preib_processing(adreno_dev, drawctxt,
				&flags, &ringcmds);

	/* start-of-pipeline timestamp */
	*ringcmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE))
		*ringcmds++ = gpuaddr +
			KGSL_MEMSTORE_OFFSET(context_id, soptimestamp);
	else
		*ringcmds++ = gpuaddr +
			KGSL_MEMSTORE_RB_OFFSET(rb, soptimestamp);
	*ringcmds++ = timestamp;

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* disable protected mode error checking */
		*ringcmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 0;
	}

	for (i = 0; i < sizedwords; i++)
		*ringcmds++ = cmds[i];

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* re-enable protected mode error checking */
		*ringcmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*ringcmds++ = 1;
	}

	/*
	 * Flush HLSQ lazy updates to make sure there are no
	 * resources pending for indirect loads after the timestamp
	 */

	*ringcmds++ = cp_type3_packet(CP_EVENT_WRITE, 1);
	*ringcmds++ = 0x07; /* HLSQ_FLUSH */
	*ringcmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*ringcmds++ = 0x00;

	/* Add any postIB required for profiling if it is enabled and has
	   assigned counters */
	if (profile_ready)
		adreno_profile_postib_processing(adreno_dev, &flags, &ringcmds);

	/*
	 * end-of-pipeline timestamp.  If per context timestamps is not
	 * enabled, then drawctxt will be NULL or internal command flag will be
	 * set and hence the rb timestamp will be used in else statement below.
	 */
	*ringcmds++ = cp_type3_packet(CP_EVENT_WRITE, 3);
	*ringcmds++ = CACHE_FLUSH_TS;

	if (drawctxt && !(flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		*ringcmds++ = gpuaddr + KGSL_MEMSTORE_OFFSET(context_id,
							eoptimestamp);
		*ringcmds++ = timestamp;
		*ringcmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
		*ringcmds++ = gpuaddr +
			KGSL_MEMSTORE_RB_OFFSET(rb, eoptimestamp);
		*ringcmds++ = rb->timestamp;
	} else {
		*ringcmds++ = gpuaddr + KGSL_MEMSTORE_RB_OFFSET(rb,
							eoptimestamp);
		*ringcmds++ = timestamp;
	}

	if (drawctxt || (flags & KGSL_CMD_FLAGS_INTERNAL_ISSUE)) {
		*ringcmds++ = cp_type3_packet(CP_INTERRUPT, 1);
		*ringcmds++ = CP_INTERRUPT_RB;
	}

	if (adreno_is_a3xx(adreno_dev)) {
		/* Dummy set-constant to trigger context rollover */
		*ringcmds++ = cp_type3_packet(CP_SET_CONSTANT, 2);
		*ringcmds++ =
			(0x4<<16) | (A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000);
		*ringcmds++ = 0;
	}

	if (flags & KGSL_CMD_FLAGS_WFI) {
		*ringcmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
		*ringcmds++ = 0x00000000;
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
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);

	flags |= KGSL_CMD_FLAGS_INTERNAL_ISSUE;
	if (drawctxt)
		rb = adreno_ctx_get_rb(adreno_dev, drawctxt);

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

	hostaddr = kgsl_gpuaddr_to_vaddr(&entry->memdesc, gpuaddr);
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
		KGSL_DRV_ERR(device, "Invalid IB size 0x%zX\n",
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

	if (kgsl_context_invalid(context))
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
	bool secured_ctxt = false;
	bool cmdbatch_profiling = false;
	int flags = KGSL_CMD_FLAGS_NONE;
	int ret;
	struct adreno_ringbuffer *rb;
	struct kgsl_cmdbatch_profiling_buffer *profile_buffer = NULL;

	struct kgsl_mem_entry *entry = cmdbatch->profiling_buf_entry;
	if (entry)
		profile_buffer = kgsl_gpuaddr_to_vaddr(&entry->memdesc,
					cmdbatch->profiling_buffer_gpuaddr);

	context = cmdbatch->context;
	drawctxt = ADRENO_CONTEXT(context);

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdbatch->cmdlist, node)
		numibs++;

	rb = adreno_ctx_get_rb(adreno_dev, drawctxt);

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
	 * 6 - secure IB start
	 * 1 - skip preamble
	 * 3 * numibs - 3 per IB
	 * 6 - secure IB end
	 * 2 - end of IB identifier
	 */
	if (context->flags & KGSL_CONTEXT_SECURE)
		secured_ctxt = true;

	if (cmdbatch->flags & KGSL_CMDBATCH_PROFILING &&
		adreno_is_a4xx(adreno_dev) && profile_buffer)
		cmdbatch_profiling = true;

	cmds = link = kzalloc(sizeof(unsigned int) * (numibs * 3 + 5 +
					(secured_ctxt ? 14 : 0) +
					(cmdbatch_profiling ? 6 : 0)),
				GFP_KERNEL);
	if (!link) {
		ret = -ENOMEM;
		goto done;
	}

	*cmds++ = cp_nop_packet(1);
	*cmds++ = KGSL_START_OF_IB_IDENTIFIER;

	if (secured_ctxt) {
		*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 0;
		*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
		*cmds++ = A4XX_RBBM_SECVID_TRUST_CONTROL;
		*cmds++ = 1;
		*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 1;
	}

	/*
	 * Add cmds to read the GPU ticks at the start of the cmdbatch and
	 * write it into the appropriate cmdbatch profiling buffer offset
	 */
	if (cmdbatch_profiling) {
		*cmds++ = cp_type3_packet(CP_REG_TO_MEM, 2);
		*cmds++ = adreno_getreg(adreno_dev,
				ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO) |
				(1 << 30) | (2 << 18);
		*cmds++ = cmdbatch->profiling_buffer_gpuaddr +
				offsetof(struct kgsl_cmdbatch_profiling_buffer,
				gpu_ticks_submitted);
	}

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

	/*
	 * Add cmds to read the GPU ticks at the end of the cmdbatch and
	 * write it into the appropriate cmdbatch profiling buffer offset
	 */
	if (cmdbatch_profiling) {
		*cmds++ = cp_type3_packet(CP_REG_TO_MEM, 2);
		*cmds++ = adreno_getreg(adreno_dev,
				ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO) |
				(1 << 30) | (2 << 18);
		*cmds++ = cmdbatch->profiling_buffer_gpuaddr +
				offsetof(struct kgsl_cmdbatch_profiling_buffer,
				gpu_ticks_retired);
	}

	if (secured_ctxt) {
		*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 0;
		*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
		*cmds++ = A4XX_RBBM_SECVID_TRUST_CONTROL;
		*cmds++ = 0;
		*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		*cmds++ = 1;
	}

	*cmds++ = cp_nop_packet(1);
	*cmds++ = KGSL_END_OF_IB_IDENTIFIER;

	ret = kgsl_setstate(&device->mmu, context->id,
		      kgsl_mmu_pt_get_flags(device->mmu.hwpagetable,
					device->id));

	if (ret)
		goto done;

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

	/* Put the wall clock and gpu timer values in the profiling buffer */
	if (cmdbatch_profiling) {
		struct timespec ts;
		do_posix_clock_monotonic_gettime(&ts);
		profile_buffer->wall_clock_s = ts.tv_sec;
		profile_buffer->wall_clock_ns = ts.tv_nsec;
		profile_buffer->gpu_ticks_queued =
			a4xx_alwayson_counter_read(adreno_dev);
	}

	ret = adreno_ringbuffer_addcmds(rb, drawctxt, flags,
					&link[0], (cmds - link),
					cmdbatch->timestamp);

	/* Corresponding unmap to the memdesc map of profile_buffer */
	if (entry)
		kgsl_memdesc_unmap(&entry->memdesc);

	kgsl_cffdump_regpoll(device,
		adreno_getreg(adreno_dev, ADRENO_REG_RBBM_STATUS) << 2,
		0x00000000, 0x80000000);
done:
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
			struct kgsl_context *context, void *data, int type)
{
	struct adreno_ringbuffer_mmu_disable_clk_param *param = data;
	kgsl_mmu_disable_clk(&device->mmu, param->unit);
	/* Free param we are done using it */
	kfree(param);
}

/*
 * adreno_ringbuffer_mmu_disable_clk_on_ts() - Sets up event to disable MMU
 * clocks
 * @device - The kgsl device pointer
 * @rb: The ringbuffer in whose event list the event is added
 * @unit: IOMMU unit for which clocks are to be turned off
 *
 * Creates an event to disable the MMU clocks on timestamp and if event
 * already exists then updates the timestamp of disabling the MMU clocks
 * with the passed in ts if it is greater than the current value at which
 * the clocks will be disabled
 * Return - void
 */
void
adreno_ringbuffer_mmu_disable_clk_on_ts(struct kgsl_device *device,
			struct adreno_ringbuffer *rb, int unit)
{
	struct adreno_ringbuffer_mmu_disable_clk_param *param;

	param = kmalloc(sizeof(*param), GFP_KERNEL);
	if (!param)
		return;

	param->rb = rb;
	param->unit = unit;
	param->ts = rb->timestamp;

	if (kgsl_add_event(device, &(rb->mmu_events),
		param->ts, adreno_ringbuffer_mmu_clk_disable_event, param)) {
		KGSL_DRV_ERR(device,
			"Failed to add IOMMU disable clk event\n");
		kfree(param);
	}
}
