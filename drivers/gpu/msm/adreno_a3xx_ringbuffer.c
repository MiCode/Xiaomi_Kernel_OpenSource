// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "kgsl_trace.h"

#define RB_SOPTIMESTAMP(device, rb) \
	       MEMSTORE_RB_GPU_ADDR(device, rb, soptimestamp)
#define CTXT_SOPTIMESTAMP(device, drawctxt) \
	       MEMSTORE_ID_GPU_ADDR(device, (drawctxt)->base.id, soptimestamp)

#define RB_EOPTIMESTAMP(device, rb) \
	       MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp)
#define CTXT_EOPTIMESTAMP(device, drawctxt) \
	       MEMSTORE_ID_GPU_ADDR(device, (drawctxt)->base.id, eoptimestamp)

int a3xx_ringbuffer_init(struct adreno_device *adreno_dev)
{
	adreno_dev->num_ringbuffers = 1;

	adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);

	return adreno_ringbuffer_setup(adreno_dev,
		&adreno_dev->ringbuffers[0], 0);
}

#define A3XX_SUBMIT_MAX 55

int a3xx_ringbuffer_addcmds(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 flags, u32 *in, u32 dwords, u32 timestamp,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 size = A3XX_SUBMIT_MAX + dwords;
	u32 *cmds, index = 0;
	u64 profile_gpuaddr;
	u32 profile_dwords;

	cmds = adreno_ringbuffer_allocspace(rb, size);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	/* Identify the start of a command */
	cmds[index++] = cp_type3_packet(CP_NOP, 1);
	cmds[index++] = drawctxt ? CMD_IDENTIFIER : CMD_INTERNAL_IDENTIFIER;

	if (IS_PWRON_FIXUP(flags)) {
		cmds[index++] = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		cmds[index++] = 0;

		cmds[index++] = cp_type3_packet(CP_NOP, 1);
		cmds[index++] = PWRON_FIXUP_IDENTIFIER;

		cmds[index++] = cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
		cmds[index++] = lower_32_bits(adreno_dev->pwron_fixup->gpuaddr);
		cmds[index++] = adreno_dev->pwron_fixup_dwords;

		cmds[index++] = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		cmds[index++] = 0;
	}

	profile_gpuaddr = adreno_profile_preib_processing(adreno_dev,
		drawctxt, &profile_dwords);

	if (profile_gpuaddr) {
		cmds[index++] = cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
		cmds[index++] = lower_32_bits(profile_gpuaddr);
		cmds[index++] = profile_dwords;
	}

	if (drawctxt) {
		cmds[index++] = cp_type3_packet(CP_MEM_WRITE, 2);
		cmds[index++] = lower_32_bits(CTXT_SOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = timestamp;
	}

	cmds[index++] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[index++] = lower_32_bits(RB_SOPTIMESTAMP(device, rb));
	cmds[index++] = rb->timestamp;

	if (IS_NOTPROTECTED(flags)) {
		cmds[index++] = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		cmds[index++] = 0;
	}

	memcpy(&cmds[index], in, dwords << 2);
	index += dwords;

	if (IS_NOTPROTECTED(flags)) {
		cmds[index++] = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
		cmds[index++] = 1;
	}

	/*
	 * Flush HLSQ lazy updates to make sure there are no resourses pending
	 * for indirect loads after the timestamp
	 */

	cmds[index++] = cp_type3_packet(CP_EVENT_WRITE, 1);
	cmds[index++] = 0x07; /* HLSQ FLUSH */
	cmds[index++] = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	cmds[index++] = 0;

	profile_gpuaddr = adreno_profile_postib_processing(adreno_dev,
		&profile_dwords);

	if (profile_gpuaddr) {
		cmds[index++] = cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
		cmds[index++] = lower_32_bits(profile_gpuaddr);
		cmds[index++] = profile_dwords;
	}

	/*
	 * If this is an internal command, just write the ringbuffer timestamp,
	 * otherwise, write both
	 */
	if (!drawctxt) {
		cmds[index++] = cp_type3_packet(CP_EVENT_WRITE, 3);
		cmds[index++] = CACHE_FLUSH_TS | (1 << 31);
		cmds[index++] = lower_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = rb->timestamp;
	} else {
		cmds[index++] = cp_type3_packet(CP_EVENT_WRITE, 3);
		cmds[index++] = CACHE_FLUSH_TS | (1 << 31);
		cmds[index++] = lower_32_bits(CTXT_EOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = timestamp;

		cmds[index++] = cp_type3_packet(CP_EVENT_WRITE, 3);
		cmds[index++] = CACHE_FLUSH_TS;
		cmds[index++] = lower_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = rb->timestamp;
	}

	/* Trigger a context rollover */
	cmds[index++] = cp_type3_packet(CP_SET_CONSTANT, 2);
	cmds[index++] = (4 << 16) | (A3XX_HLSQ_CL_KERNEL_GROUP_X_REG - 0x2000);
	cmds[index++] = 0;

	if (IS_WFI(flags)) {
		cmds[index++] = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
		cmds[index++] = 0;
	}

	/* Adjust the thing for the number of bytes we actually wrote */
	rb->_wptr -= (size - index);

	kgsl_pwrscale_busy(device);
	kgsl_regwrite(device, A3XX_CP_RB_WPTR, rb->_wptr);
	rb->wptr = rb->_wptr;

	return 0;
}

#define A3XX_COMMAND_DWORDS 4

int a3xx_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj, u32 flags,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	struct adreno_ringbuffer *rb = drawctxt->rb;
	int ret = 0, numibs = 0, index = 0;
	u32 *cmds;

	/* Count the number of IBs (if we are not skipping) */
	if (!IS_SKIP(flags)) {
		struct list_head *tmp;

		list_for_each(tmp, &cmdobj->cmdlist)
			numibs++;
	}

	cmds = kmalloc((A3XX_COMMAND_DWORDS + (numibs * 4)) << 2, GFP_KERNEL);
	if (!cmds) {
		ret = -ENOMEM;
		goto done;
	}

	cmds[index++] = cp_type3_packet(CP_NOP, 1);
	cmds[index++] = START_IB_IDENTIFIER;

	if (numibs) {
		struct kgsl_memobj_node *ib;

		list_for_each_entry(ib, &cmdobj->cmdlist, node) {
			if (ib->priv & MEMOBJ_SKIP ||
			    (ib->priv & MEMOBJ_PREAMBLE && !IS_PREAMBLE(flags)))
				cmds[index++] = cp_type3_packet(CP_NOP, 3);

			cmds[index++] =
				cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
			cmds[index++] = lower_32_bits(ib->gpuaddr);
			cmds[index++] = ib->size >> 2;
		}
	}

	cmds[index++] = cp_type3_packet(CP_NOP, 1);
	cmds[index++] = END_IB_IDENTIFIER;

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

	adreno_ringbuffer_set_constraint(device, drawobj);

	ret = adreno_ringbuffer_addcmds(adreno_dev, drawctxt->rb, drawctxt,
		flags, cmds, index, drawobj->timestamp, time);

done:
	trace_kgsl_issueibcmds(device, drawctxt->base.id, numibs,
		drawobj->timestamp, drawobj->flags, ret, drawctxt->type);

	kfree(cmds);
	return ret;
}
