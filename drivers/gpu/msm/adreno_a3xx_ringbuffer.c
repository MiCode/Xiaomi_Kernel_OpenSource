// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"

static int a3xx_wait_reg(unsigned int *cmds, unsigned int addr,
			unsigned int val, unsigned int mask,
			unsigned int interval)
{
	cmds[0] = cp_type3_packet(CP_WAIT_REG_EQ, 4);
	cmds[1] = addr;
	cmds[2] = val;
	cmds[3] = mask;
	cmds[4] = interval;

	return 5;
}

static int a3xx_vbif_lock(unsigned int *cmds)
{
	int count;

	/*
	 * glue commands together until next
	 * WAIT_FOR_ME
	 */
	count = a3xx_wait_reg(cmds, A3XX_CP_WFI_PEND_CTR,
			1, 0xFFFFFFFF, 0xF);

	/* MMU-500 VBIF stall */
	cmds[count++] = cp_type3_packet(CP_REG_RMW, 3);
	cmds[count++] = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
	/* AND to unmask the HALT bit */
	cmds[count++] = ~(VBIF_RECOVERABLE_HALT_CTRL);
	/* OR to set the HALT bit */
	cmds[count++] = 0x1;

	/* Wait for acknowledgment */
	count += a3xx_wait_reg(&cmds[count],
			A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL1,
			1, 0xFFFFFFFF, 0xF);

	return count;
}

static int a3xx_vbif_unlock(unsigned int *cmds)
{
	/* MMU-500 VBIF unstall */
	cmds[0] = cp_type3_packet(CP_REG_RMW, 3);
	cmds[1] = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
	/* AND to unmask the HALT bit */
	cmds[2] = ~(VBIF_RECOVERABLE_HALT_CTRL);
	/* OR to reset the HALT bit */
	cmds[3] = 0;

	/* release all commands since _vbif_lock() with wait_for_me */
	cmds[4] = cp_type3_packet(CP_WAIT_FOR_ME, 1);
	cmds[5] = 0;

	return 6;
}

#define A3XX_GPU_OFFSET 0xa000

static int a3xx_cp_smmu_reg(unsigned int *cmds,
				u32 reg,
				unsigned int num)
{
	cmds[0] = cp_type3_packet(CP_REG_WR_NO_CTXT, num + 1);
	cmds[1] = (A3XX_GPU_OFFSET + reg) >> 2;

	return 2;
}

/* This function is only needed for A3xx targets */
static int a3xx_tlbiall(unsigned int *cmds)
{
	unsigned int tlbstatus = (A3XX_GPU_OFFSET +
		KGSL_IOMMU_CTX_TLBSTATUS) >> 2;
	int count;

	count = a3xx_cp_smmu_reg(cmds, KGSL_IOMMU_CTX_TLBIALL, 1);
	cmds[count++] = 1;

	count += a3xx_cp_smmu_reg(&cmds[count], KGSL_IOMMU_CTX_TLBSYNC, 1);
	cmds[count++] = 0;

	count += a3xx_wait_reg(&cmds[count], tlbstatus, 0,
			KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);

	return count;
}

/* offset at which a nop command is placed in setstate */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

static int a3xx_rb_pagetable_switch(struct adreno_device *adreno_dev,
		struct kgsl_pagetable *pagetable, u32 *cmds)
{
	u64 ttbr0 = kgsl_mmu_pagetable_get_ttbr0(pagetable);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU(device);
	int count = 0;

	/*
	 * Adding an indirect buffer ensures that the prefetch stalls until
	 * the commands in indirect buffer have completed. We need to stall
	 * prefetch with a nop indirect buffer when updating pagetables
	 * because it provides stabler synchronization.
	 */
	cmds[count++] = cp_type3_packet(CP_WAIT_FOR_ME, 1);
	cmds[count++] = 0;

	cmds[count++] = cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
	cmds[count++] = lower_32_bits(iommu->setstate->gpuaddr);
	cmds[count++] = 2;

	cmds[count++] = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	cmds[count++] = 0;

	cmds[count++] = cp_type3_packet(CP_WAIT_FOR_ME, 1);
	cmds[count++] = 0;

	count += a3xx_vbif_lock(&cmds[count]);

	count += a3xx_cp_smmu_reg(&cmds[count], KGSL_IOMMU_CTX_TTBR0, 2);
	cmds[count++] = lower_32_bits(ttbr0);
	cmds[count++] = upper_32_bits(ttbr0);

	count += a3xx_vbif_unlock(&cmds[count]);

	count += a3xx_tlbiall(&cmds[count]);

	/* wait for me to finish the TLBI */
	cmds[count++] = cp_type3_packet(CP_WAIT_FOR_ME, 1);
	cmds[count++] = 0;
	cmds[count++] = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	cmds[count++] = 0;

	/* Invalidate the state */
	cmds[count++] = cp_type3_packet(CP_INVALIDATE_STATE, 1);
	cmds[count++] = 0x7ffff;

	return count;
}

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

static int a3xx_ringbuffer_addcmds(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 flags, u32 *in, u32 dwords, u32 timestamp,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 size = A3XX_SUBMIT_MAX + dwords;
	u32 *cmds, index = 0;
	u64 profile_gpuaddr;
	u32 profile_dwords;

	if (adreno_drawctxt_detached(drawctxt))
		return -ENOENT;

	if (adreno_gpu_fault(adreno_dev) != 0)
		return -EPROTO;

	rb->timestamp++;

	if (drawctxt)
		drawctxt->internal_timestamp = rb->timestamp;

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
		drawctxt, &profile_dwords);

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

static int a3xx_rb_context_switch(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_context *drawctxt)
{
	struct kgsl_pagetable *pagetable =
		adreno_drawctxt_get_pagetable(drawctxt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int count = 0;
	u32 cmds[64];

	if (adreno_drawctxt_get_pagetable(rb->drawctxt_active) != pagetable)
		count += a3xx_rb_pagetable_switch(adreno_dev, pagetable, cmds);

	cmds[count++] = cp_type3_packet(CP_NOP, 1);
	cmds[count++] = CONTEXT_TO_MEM_IDENTIFIER;

	cmds[count++] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[count++] = lower_32_bits(MEMSTORE_RB_GPU_ADDR(device, rb,
				current_context));
	cmds[count++] = drawctxt->base.id;

	cmds[count++] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[count++] = lower_32_bits(MEMSTORE_ID_GPU_ADDR(device,
		KGSL_MEMSTORE_GLOBAL, current_context));
	cmds[count++] = drawctxt->base.id;

	cmds[count++] = cp_type0_packet(A3XX_UCHE_CACHE_INVALIDATE0_REG, 2);
	cmds[count++] = 0;
	cmds[count++] = 0x90000000;

	return a3xx_ringbuffer_addcmds(adreno_dev, rb, NULL, F_NOTPROTECTED,
			cmds, count, 0, NULL);
}

static int a3xx_drawctxt_switch(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_context *drawctxt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (rb->drawctxt_active == drawctxt)
		return 0;

	if (kgsl_context_detached(&drawctxt->base))
		return -ENOENT;

	if (!_kgsl_context_get(&drawctxt->base))
		return -ENOENT;

	trace_adreno_drawctxt_switch(rb, drawctxt);

	a3xx_rb_context_switch(adreno_dev, rb, drawctxt);

	/* Release the current drawctxt as soon as the new one is switched */
	adreno_put_drawctxt_on_timestamp(device, rb->drawctxt_active,
		rb, rb->timestamp);

	rb->drawctxt_active = drawctxt;
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
			    (ib->flags & KGSL_CMDLIST_CTXTSWITCH_PREAMBLE
			     && !IS_PREAMBLE(flags)))
				cmds[index++] = cp_type3_packet(CP_NOP, 3);

			cmds[index++] =
				cp_type3_packet(CP_INDIRECT_BUFFER_PFE, 2);
			cmds[index++] = lower_32_bits(ib->gpuaddr);
			cmds[index++] = ib->size >> 2;
		}
	}

	cmds[index++] = cp_type3_packet(CP_NOP, 1);
	cmds[index++] = END_IB_IDENTIFIER;

	ret = a3xx_drawctxt_switch(adreno_dev, rb, drawctxt);

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

	adreno_drawobj_set_constraint(device, drawobj);

	ret = a3xx_ringbuffer_addcmds(adreno_dev, drawctxt->rb, drawctxt,
		flags, cmds, index, drawobj->timestamp, NULL);

done:
	trace_kgsl_issueibcmds(device, drawctxt->base.id, numibs,
		drawobj->timestamp, drawobj->flags, ret, drawctxt->type);

	kfree(cmds);
	return ret;
}
