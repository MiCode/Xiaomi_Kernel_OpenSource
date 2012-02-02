/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
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
	BUG_ON(rb->wptr == 0);

	/* Let the pwrscale policy know that new commands have
	 been submitted. */
	kgsl_pwrscale_busy(rb->device);

	/*synchronize memory before informing the hardware of the
	 *new commands.
	 */
	mb();

	adreno_regwrite(rb->device, REG_CP_RB_WPTR, rb->wptr);
}

static void
adreno_ringbuffer_waitspace(struct adreno_ringbuffer *rb, unsigned int numcmds,
			  int wptr_ahead)
{
	int nopcount;
	unsigned int freecmds;
	unsigned int *cmds;
	uint cmds_gpu;

	/* if wptr ahead, fill the remaining with NOPs */
	if (wptr_ahead) {
		/* -1 for header */
		nopcount = rb->sizedwords - rb->wptr - 1;

		cmds = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
		cmds_gpu = rb->buffer_desc.gpuaddr + sizeof(uint)*rb->wptr;

		GSL_RB_WRITE(cmds, cmds_gpu, cp_nop_packet(nopcount));

		/* Make sure that rptr is not 0 before submitting
		 * commands at the end of ringbuffer. We do not
		 * want the rptr and wptr to become equal when
		 * the ringbuffer is not empty */
		do {
			GSL_RB_GET_READPTR(rb, &rb->rptr);
		} while (!rb->rptr);

		rb->wptr++;

		adreno_ringbuffer_submit(rb);

		rb->wptr = 0;
	}

	/* wait for space in ringbuffer */
	do {
		GSL_RB_GET_READPTR(rb, &rb->rptr);

		freecmds = rb->rptr - rb->wptr;

	} while ((freecmds != 0) && (freecmds <= numcmds));
}

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
					     unsigned int numcmds)
{
	unsigned int	*ptr = NULL;

	BUG_ON(numcmds >= rb->sizedwords);

	GSL_RB_GET_READPTR(rb, &rb->rptr);
	/* check for available space */
	if (rb->wptr >= rb->rptr) {
		/* wptr ahead or equal to rptr */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS))
			adreno_ringbuffer_waitspace(rb, numcmds, 1);
	} else {
		/* wptr behind rptr */
		if ((rb->wptr + numcmds) >= rb->rptr)
			adreno_ringbuffer_waitspace(rb, numcmds, 0);
		/* check for remaining space */
		/* reserve dwords for nop packet */
		if ((rb->wptr + numcmds) > (rb->sizedwords -
				GSL_RB_NOP_SIZEDWORDS))
			adreno_ringbuffer_waitspace(rb, numcmds, 1);
	}

	ptr = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
	rb->wptr += numcmds;

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

static int adreno_ringbuffer_load_pm4_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i, ret = 0;

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
	}

	KGSL_DRV_INFO(device, "loading pm4 ucode version: %d\n",
		adreno_dev->pm4_fw[0]);

	adreno_regwrite(device, REG_CP_DEBUG, 0x02000000);
	adreno_regwrite(device, REG_CP_ME_RAM_WADDR, 0);
	for (i = 1; i < adreno_dev->pm4_fw_size; i++)
		adreno_regwrite(device, REG_CP_ME_RAM_DATA,
				     adreno_dev->pm4_fw[i]);
err:
	return ret;
}

static int adreno_ringbuffer_load_pfp_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i, ret = 0;

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
	}

	KGSL_DRV_INFO(device, "loading pfp ucode version: %d\n",
		adreno_dev->pfp_fw[0]);

	adreno_regwrite(device, adreno_dev->gpudev->reg_cp_pfp_ucode_addr, 0);
	for (i = 1; i < adreno_dev->pfp_fw_size; i++)
		adreno_regwrite(device,
			adreno_dev->gpudev->reg_cp_pfp_ucode_data,
			adreno_dev->pfp_fw[i]);
err:
	return ret;
}

int adreno_ringbuffer_start(struct adreno_ringbuffer *rb, unsigned int init_ram)
{
	int status;
	/*cp_rb_cntl_u cp_rb_cntl; */
	union reg_cp_rb_cntl cp_rb_cntl;
	unsigned int rb_cntl;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (rb->flags & KGSL_FLAGS_STARTED)
		return 0;

	if (init_ram) {
		rb->timestamp = 0;
		GSL_RB_INIT_TIMESTAMP(rb);
	}

	kgsl_sharedmem_set(&rb->memptrs_desc, 0, 0,
			   sizeof(struct kgsl_rbmemptrs));

	kgsl_sharedmem_set(&rb->buffer_desc, 0, 0xAA,
			   (rb->sizedwords << 2));

	if (adreno_is_a2xx(adreno_dev)) {
		adreno_regwrite(device, REG_CP_RB_WPTR_BASE,
			(rb->memptrs_desc.gpuaddr
			+ GSL_RB_MEMPTRS_WPTRPOLL_OFFSET));

		/* setup WPTR delay */
		adreno_regwrite(device, REG_CP_RB_WPTR_DELAY,
			0 /*0x70000010 */);
	}

	/*setup REG_CP_RB_CNTL */
	adreno_regread(device, REG_CP_RB_CNTL, &rb_cntl);
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

	adreno_regwrite(device, REG_CP_RB_CNTL, cp_rb_cntl.val);

	adreno_regwrite(device, REG_CP_RB_BASE, rb->buffer_desc.gpuaddr);

	adreno_regwrite(device, REG_CP_RB_RPTR_ADDR,
			     rb->memptrs_desc.gpuaddr +
			     GSL_RB_MEMPTRS_RPTR_OFFSET);

	if (adreno_is_a3xx(adreno_dev)) {
		/* enable access protection to privileged registers */
		adreno_regwrite(device, A3XX_CP_PROTECT_CTRL, 0x00000007);

		/* RBBM registers */
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_0, 0x63000040);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_1, 0x62000080);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_2, 0x600000CC);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_3, 0x60000108);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_4, 0x64000140);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_5, 0x66000400);

		/* CP registers */
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_6, 0x65000700);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_7, 0x610007D8);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_8, 0x620007E0);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_9, 0x61001178);
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_A, 0x64001180);

		/* RB registers */
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_B, 0x60003300);

		/* VBIF registers */
		adreno_regwrite(device, A3XX_CP_PROTECT_REG_C, 0x6B00C000);
	}

	if (adreno_is_a2xx(adreno_dev)) {
		/* explicitly clear all cp interrupts */
		adreno_regwrite(device, REG_CP_INT_ACK, 0xFFFFFFFF);
	}

	/* setup scratch/timestamp */
	adreno_regwrite(device, REG_SCRATCH_ADDR,
			     device->memstore.gpuaddr +
			     KGSL_DEVICE_MEMSTORE_OFFSET(soptimestamp));

	adreno_regwrite(device, REG_SCRATCH_UMSK,
			     GSL_RB_MEMPTRS_SCRATCH_MASK);

	/* load the CP ucode */

	status = adreno_ringbuffer_load_pm4_ucode(device);
	if (status != 0)
		return status;

	/* load the prefetch parser ucode */
	status = adreno_ringbuffer_load_pfp_ucode(device);
	if (status != 0)
		return status;

	adreno_regwrite(device, REG_CP_QUEUE_THRESHOLDS, 0x000C0804);

	rb->rptr = 0;
	rb->wptr = 0;

	/* clear ME_HALT to start micro engine */
	adreno_regwrite(device, REG_CP_ME_CNTL, 0);

	/* ME init is GPU specific, so jump into the sub-function */
	adreno_dev->gpudev->rb_init(adreno_dev, rb);

	/* idle device to validate ME INIT */
	status = adreno_idle(device, KGSL_TIMEOUT_DEFAULT);

	if (status == 0)
		rb->flags |= KGSL_FLAGS_STARTED;

	return status;
}

void adreno_ringbuffer_stop(struct adreno_ringbuffer *rb)
{
	if (rb->flags & KGSL_FLAGS_STARTED) {
		/* ME_HALT */
		adreno_regwrite(rb->device, REG_CP_ME_CNTL, 0x10000000);
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

static uint32_t
adreno_ringbuffer_addcmds(struct adreno_ringbuffer *rb,
				unsigned int flags, unsigned int *cmds,
				int sizedwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	unsigned int *ringcmds;
	unsigned int timestamp;
	unsigned int total_sizedwords = sizedwords + 6;
	unsigned int i;
	unsigned int rcmd_gpu;

	/* reserve space to temporarily turn off protected mode
	*  error checking if needed
	*/
	total_sizedwords += flags & KGSL_CMD_FLAGS_PMODE ? 4 : 0;
	total_sizedwords += !(flags & KGSL_CMD_FLAGS_NO_TS_CMP) ? 7 : 0;
	total_sizedwords += !(flags & KGSL_CMD_FLAGS_NOT_KERNEL_CMD) ? 2 : 0;

	if (adreno_is_a3xx(adreno_dev))
		total_sizedwords += 7;

	ringcmds = adreno_ringbuffer_allocspace(rb, total_sizedwords);
	rcmd_gpu = rb->buffer_desc.gpuaddr
		+ sizeof(uint)*(rb->wptr-total_sizedwords);

	if (!(flags & KGSL_CMD_FLAGS_NOT_KERNEL_CMD)) {
		GSL_RB_WRITE(ringcmds, rcmd_gpu, cp_nop_packet(1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, KGSL_CMD_IDENTIFIER);
	}
	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* disable protected mode error checking */
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 0);
	}

	for (i = 0; i < sizedwords; i++) {
		GSL_RB_WRITE(ringcmds, rcmd_gpu, *cmds);
		cmds++;
	}

	if (flags & KGSL_CMD_FLAGS_PMODE) {
		/* re-enable protected mode error checking */
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_PROTECTED_MODE, 1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 1);
	}

	rb->timestamp++;
	timestamp = rb->timestamp;

	/* start-of-pipeline and end-of-pipeline timestamps */
	GSL_RB_WRITE(ringcmds, rcmd_gpu, cp_type0_packet(REG_CP_TIMESTAMP, 1));
	GSL_RB_WRITE(ringcmds, rcmd_gpu, rb->timestamp);

	if (adreno_is_a3xx(adreno_dev)) {
		/*
		 * FLush HLSQ lazy updates to make sure there are no
		 * rsources pending for indirect loads after the timestamp
		 */

		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_EVENT_WRITE, 1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 0x07); /* HLSQ_FLUSH */
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_WAIT_FOR_IDLE, 1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 0x00);
	}

	GSL_RB_WRITE(ringcmds, rcmd_gpu, cp_type3_packet(CP_EVENT_WRITE, 3));
	GSL_RB_WRITE(ringcmds, rcmd_gpu, CACHE_FLUSH_TS);
	GSL_RB_WRITE(ringcmds, rcmd_gpu,
		     (rb->device->memstore.gpuaddr +
		      KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp)));
	GSL_RB_WRITE(ringcmds, rcmd_gpu, rb->timestamp);

	if (!(flags & KGSL_CMD_FLAGS_NO_TS_CMP)) {
		/* Conditional execution based on memory values */
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_COND_EXEC, 4));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, (rb->device->memstore.gpuaddr +
			KGSL_DEVICE_MEMSTORE_OFFSET(ts_cmp_enable)) >> 2);
		GSL_RB_WRITE(ringcmds, rcmd_gpu, (rb->device->memstore.gpuaddr +
			KGSL_DEVICE_MEMSTORE_OFFSET(ref_wait_ts)) >> 2);
		GSL_RB_WRITE(ringcmds, rcmd_gpu, rb->timestamp);
		/* # of conditional command DWORDs */
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 2);
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_INTERRUPT, 1));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, CP_INT_CNTL__RB_INT_MASK);
	}

	if (adreno_is_a3xx(adreno_dev)) {
		/* Dummy set-constant to trigger context rollover */
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			cp_type3_packet(CP_SET_CONSTANT, 2));
		GSL_RB_WRITE(ringcmds, rcmd_gpu,
			(0x4<<16)|(A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000));
		GSL_RB_WRITE(ringcmds, rcmd_gpu, 0);
	}

	adreno_ringbuffer_submit(rb);

	/* return timestamp of issued coREG_ands */
	return timestamp;
}

void
adreno_ringbuffer_issuecmds(struct kgsl_device *device,
						unsigned int flags,
						unsigned int *cmds,
						int sizedwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;

	if (device->state & KGSL_STATE_HUNG)
		return;
	adreno_ringbuffer_addcmds(rb, flags, cmds, sizedwords);
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
	unsigned int *link;
	unsigned int *cmds;
	unsigned int i;
	struct adreno_context *drawctxt;
	unsigned int start_index = 0;

	if (device->state & KGSL_STATE_HUNG)
		return -EBUSY;
	if (!(adreno_dev->ringbuffer.flags & KGSL_FLAGS_STARTED) ||
	      context == NULL || ibdesc == 0 || numibs == 0)
		return -EINVAL;

	drawctxt = context->devctxt;

	if (drawctxt->flags & CTXT_FLAGS_GPU_HANG) {
		KGSL_CTXT_WARN(device, "Context %p caused a gpu hang.."
			" will not accept commands for this context\n",
			drawctxt);
		return -EDEADLK;
	}
	link = kzalloc(sizeof(unsigned int) * numibs * 3, GFP_KERNEL);
	cmds = link;
	if (!link) {
		KGSL_MEM_ERR(device, "Failed to allocate memory for for command"
			" submission, size %x\n", numibs * 3);
		return -ENOMEM;
	}

	/*When preamble is enabled, the preamble buffer with state restoration
	commands are stored in the first node of the IB chain. We can skip that
	if a context switch hasn't occured */

	if (drawctxt->flags & CTXT_FLAGS_PREAMBLE &&
		adreno_dev->drawctxt_active == drawctxt)
		start_index = 1;

	for (i = start_index; i < numibs; i++) {
		(void)kgsl_cffdump_parse_ibs(dev_priv, NULL,
			ibdesc[i].gpuaddr, ibdesc[i].sizedwords, false);

		*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
		*cmds++ = ibdesc[i].gpuaddr;
		*cmds++ = ibdesc[i].sizedwords;
	}

	kgsl_setstate(device,
		      kgsl_mmu_pt_get_flags(device->mmu.hwpagetable,
					device->id));

	adreno_drawctxt_switch(adreno_dev, drawctxt, flags);

	*timestamp = adreno_ringbuffer_addcmds(&adreno_dev->ringbuffer,
					KGSL_CMD_FLAGS_NOT_KERNEL_CMD,
					&link[0], (cmds - link));

	KGSL_CMD_INFO(device, "ctxt %d g %08x numibs %d ts %d\n",
		context->id, (unsigned int)ibdesc, numibs, *timestamp);

	kfree(link);

#ifdef CONFIG_MSM_KGSL_CFF_DUMP
	/*
	 * insert wait for idle after every IB1
	 * this is conservative but works reliably and is ok
	 * even for performance simulations
	 */
	adreno_idle(device, KGSL_TIMEOUT_DEFAULT);
#endif

	return 0;
}

int adreno_ringbuffer_extract(struct adreno_ringbuffer *rb,
				unsigned int *temp_rb_buffer,
				int *rb_size)
{
	struct kgsl_device *device = rb->device;
	unsigned int rb_rptr;
	unsigned int retired_timestamp;
	unsigned int temp_idx = 0;
	unsigned int value;
	unsigned int val1;
	unsigned int val2;
	unsigned int val3;
	unsigned int copy_rb_contents = 0;
	unsigned int cur_context;
	unsigned int j;

	GSL_RB_GET_READPTR(rb, &rb->rptr);

	retired_timestamp = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);
	KGSL_DRV_ERR(device, "GPU successfully executed till ts: %x\n",
			retired_timestamp);
	/*
	 * We need to go back in history by 4 dwords from the current location
	 * of read pointer as 4 dwords are read to match the end of a command.
	 * Also, take care of wrap around when moving back
	 */
	if (rb->rptr >= 4)
		rb_rptr = (rb->rptr - 4) * sizeof(unsigned int);
	else
		rb_rptr = rb->buffer_desc.size -
			((4 - rb->rptr) * sizeof(unsigned int));
	/* Read the rb contents going backwards to locate end of last
	 * sucessfully executed command */
	while ((rb_rptr / sizeof(unsigned int)) != rb->wptr) {
		kgsl_sharedmem_readl(&rb->buffer_desc, &value, rb_rptr);
		if (value == retired_timestamp) {
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			kgsl_sharedmem_readl(&rb->buffer_desc, &val1, rb_rptr);
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			kgsl_sharedmem_readl(&rb->buffer_desc, &val2, rb_rptr);
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			kgsl_sharedmem_readl(&rb->buffer_desc, &val3, rb_rptr);
			/* match the pattern found at the end of a command */
			if ((val1 == 2 &&
				val2 == cp_type3_packet(CP_INTERRUPT, 1)
				&& val3 == CP_INT_CNTL__RB_INT_MASK) ||
				(val1 == cp_type3_packet(CP_EVENT_WRITE, 3)
				&& val2 == CACHE_FLUSH_TS &&
				val3 == (rb->device->memstore.gpuaddr +
				KGSL_DEVICE_MEMSTORE_OFFSET(eoptimestamp)))) {
				rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
				KGSL_DRV_ERR(device,
					"Found end of last executed "
					"command at offset: %x\n",
					rb_rptr / sizeof(unsigned int));
				break;
			} else {
				if (rb_rptr < (3 * sizeof(unsigned int)))
					rb_rptr = rb->buffer_desc.size -
						(3 * sizeof(unsigned int))
							+ rb_rptr;
				else
					rb_rptr -= (3 * sizeof(unsigned int));
			}
		}

		if (rb_rptr == 0)
			rb_rptr = rb->buffer_desc.size - sizeof(unsigned int);
		else
			rb_rptr -= sizeof(unsigned int);
	}

	if ((rb_rptr / sizeof(unsigned int)) == rb->wptr) {
		KGSL_DRV_ERR(device,
			"GPU recovery from hang not possible because last"
			" successful timestamp is overwritten\n");
		return -EINVAL;
	}
	/* rb_rptr is now pointing to the first dword of the command following
	 * the last sucessfully executed command sequence. Assumption is that
	 * GPU is hung in the command sequence pointed by rb_rptr */
	/* make sure the GPU is not hung in a command submitted by kgsl
	 * itself */
	kgsl_sharedmem_readl(&rb->buffer_desc, &val1, rb_rptr);
	kgsl_sharedmem_readl(&rb->buffer_desc, &val2,
				adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size));
	if (val1 == cp_nop_packet(1) && val2 == KGSL_CMD_IDENTIFIER) {
		KGSL_DRV_ERR(device,
			"GPU recovery from hang not possible because "
			"of hang in kgsl command\n");
		return -EINVAL;
	}

	/* current_context is the context that is presently active in the
	 * GPU, i.e the context in which the hang is caused */
	kgsl_sharedmem_readl(&device->memstore, &cur_context,
		KGSL_DEVICE_MEMSTORE_OFFSET(current_context));
	while ((rb_rptr / sizeof(unsigned int)) != rb->wptr) {
		kgsl_sharedmem_readl(&rb->buffer_desc, &value, rb_rptr);
		rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
						rb->buffer_desc.size);
		/* check for context switch indicator */
		if (value == KGSL_CONTEXT_TO_MEM_IDENTIFIER) {
			kgsl_sharedmem_readl(&rb->buffer_desc, &value, rb_rptr);
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			BUG_ON(value != cp_type3_packet(CP_MEM_WRITE, 2));
			kgsl_sharedmem_readl(&rb->buffer_desc, &val1, rb_rptr);
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			BUG_ON(val1 != (device->memstore.gpuaddr +
				KGSL_DEVICE_MEMSTORE_OFFSET(current_context)));
			kgsl_sharedmem_readl(&rb->buffer_desc, &value, rb_rptr);
			rb_rptr = adreno_ringbuffer_inc_wrapped(rb_rptr,
							rb->buffer_desc.size);
			BUG_ON((copy_rb_contents == 0) &&
				(value == cur_context));
			/*
			 * If we were copying the commands and got to this point
			 * then we need to remove the 3 commands that appear
			 * before KGSL_CONTEXT_TO_MEM_IDENTIFIER
			 */
			if (temp_idx)
				temp_idx -= 3;
			/* if context switches to a context that did not cause
			 * hang then start saving the rb contents as those
			 * commands can be executed */
			if (value != cur_context) {
				copy_rb_contents = 1;
				temp_rb_buffer[temp_idx++] = cp_nop_packet(1);
				temp_rb_buffer[temp_idx++] =
						KGSL_CMD_IDENTIFIER;
				temp_rb_buffer[temp_idx++] = cp_nop_packet(1);
				temp_rb_buffer[temp_idx++] =
						KGSL_CONTEXT_TO_MEM_IDENTIFIER;
				temp_rb_buffer[temp_idx++] =
					cp_type3_packet(CP_MEM_WRITE, 2);
				temp_rb_buffer[temp_idx++] = val1;
				temp_rb_buffer[temp_idx++] = value;
			} else {
				copy_rb_contents = 0;
			}
		} else if (copy_rb_contents)
			temp_rb_buffer[temp_idx++] = value;
	}

	*rb_size = temp_idx;
	KGSL_DRV_ERR(device, "Extracted rb contents, size: %x\n", *rb_size);
	for (temp_idx = 0; temp_idx < *rb_size;) {
		char str[80];
		int idx = 0;
		if ((temp_idx + 8) <= *rb_size)
			j = 8;
		else
			j = *rb_size - temp_idx;
		for (; j != 0; j--)
			idx += scnprintf(str + idx, 80 - idx,
				"%8.8X ", temp_rb_buffer[temp_idx++]);
		printk(KERN_ALERT "%s", str);
	}
	return 0;
}

void
adreno_ringbuffer_restore(struct adreno_ringbuffer *rb, unsigned int *rb_buff,
			int num_rb_contents)
{
	int i;
	unsigned int *ringcmds;
	unsigned int rcmd_gpu;

	if (!num_rb_contents)
		return;

	if (num_rb_contents > (rb->buffer_desc.size - rb->wptr)) {
		adreno_regwrite(rb->device, REG_CP_RB_RPTR, 0);
		rb->rptr = 0;
		BUG_ON(num_rb_contents > rb->buffer_desc.size);
	}
	ringcmds = (unsigned int *)rb->buffer_desc.hostptr + rb->wptr;
	rcmd_gpu = rb->buffer_desc.gpuaddr + sizeof(unsigned int) * rb->wptr;
	for (i = 0; i < num_rb_contents; i++)
		GSL_RB_WRITE(ringcmds, rcmd_gpu, rb_buff[i]);
	rb->wptr += num_rb_contents;
	adreno_ringbuffer_submit(rb);
}
