// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"

static bool is_concurrent_binning(struct adreno_context *drawctxt)
{
	if (!drawctxt)
		return false;

	return !(drawctxt->base.flags & KGSL_CONTEXT_SECURE);
}

static int gen7_rb_pagetable_switch(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		struct kgsl_pagetable *pagetable, u32 *cmds)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u64 ttbr0 = kgsl_mmu_pagetable_get_ttbr0(pagetable);
	int count = 0;
	u32 id = drawctxt ? drawctxt->base.id : 0;

	if (pagetable == device->mmu.defaultpagetable)
		return 0;

	/* CP switches the pagetable and flushes the Caches */
	cmds[count++] = cp_type7_packet(CP_SMMU_TABLE_UPDATE, 3);
	cmds[count++] = lower_32_bits(ttbr0);
	cmds[count++] = upper_32_bits(ttbr0);
	cmds[count++] = id;

	cmds[count++] = cp_type7_packet(CP_MEM_WRITE, 5);
	cmds[count++] = lower_32_bits(SCRATCH_RB_GPU_ADDR(device,
				rb->id, ttbr0));
	cmds[count++] = upper_32_bits(SCRATCH_RB_GPU_ADDR(device,
				rb->id, ttbr0));
	cmds[count++] = lower_32_bits(ttbr0);
	cmds[count++] = upper_32_bits(ttbr0);
	cmds[count++] = id;

	/*
	 * Sync both threads after switching pagetables and enable BR only
	 * to make sure BV doesn't race ahead while BR is still switching
	 * pagetables.
	 */
	cmds[count++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[count++] = CP_SYNC_THREADS | CP_SET_THREAD_BR;

	return count;
}

static int gen7_rb_context_switch(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_context *drawctxt)
{
	struct kgsl_pagetable *pagetable =
		adreno_drawctxt_get_pagetable(drawctxt);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int count = 0;
	u32 cmds[55];

	/* Sync both threads */
	cmds[count++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[count++] = CP_SYNC_THREADS | CP_SET_THREAD_BOTH;
	/* Reset context state */
	cmds[count++] = cp_type7_packet(CP_RESET_CONTEXT_STATE, 1);
	cmds[count++] = CP_RESET_GLOBAL_LOCAL_TS | CP_CLEAR_BV_BR_COUNTER |
			CP_CLEAR_RESOURCE_TABLE | CP_CLEAR_ON_CHIP_TS;
	/*
	 * Enable/disable concurrent binning for pagetable switch and
	 * set the thread to BR since only BR can execute the pagetable
	 * switch packets.
	 */
	/* Sync both threads and enable BR only */
	cmds[count++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[count++] = CP_SYNC_THREADS | CP_SET_THREAD_BR;

	if (adreno_drawctxt_get_pagetable(rb->drawctxt_active) != pagetable) {

		/* Clear performance counters during context switches */
		if (!adreno_dev->perfcounter) {
			cmds[count++] = cp_type4_packet(GEN7_RBBM_PERFCTR_SRAM_INIT_CMD, 1);
			cmds[count++] = 0x1;
		}

		count += gen7_rb_pagetable_switch(adreno_dev, rb,
			drawctxt, pagetable, &cmds[count]);

		/* Wait for performance counter clear to finish */
		if (!adreno_dev->perfcounter) {
			cmds[count++] = cp_type7_packet(CP_WAIT_REG_MEM, 6);
			cmds[count++] = 0x3;
			cmds[count++] = GEN7_RBBM_PERFCTR_SRAM_INIT_STATUS;
			cmds[count++] = 0x0;
			cmds[count++] = 0x1;
			cmds[count++] = 0x1;
			cmds[count++] = 0x0;
		}
	} else {
		struct kgsl_iommu *iommu = KGSL_IOMMU(device);

		u32 offset = GEN7_SMMU_BASE + (iommu->cb0_offset >> 2) + 0x0d;

		/*
		 * Set the CONTEXTIDR register to the current context id so we
		 * can use it in pagefault debugging. Unlike TTBR0 we don't
		 * need any special sequence or locking to change it
		 */
		cmds[count++] = cp_type4_packet(offset, 1);
		cmds[count++] = drawctxt->base.id;
	}

	cmds[count++] = cp_type7_packet(CP_NOP, 1);
	cmds[count++] = CONTEXT_TO_MEM_IDENTIFIER;

	cmds[count++] = cp_type7_packet(CP_MEM_WRITE, 3);
	cmds[count++] = lower_32_bits(MEMSTORE_RB_GPU_ADDR(device, rb,
				current_context));
	cmds[count++] = upper_32_bits(MEMSTORE_RB_GPU_ADDR(device, rb,
				current_context));
	cmds[count++] = drawctxt->base.id;

	cmds[count++] = cp_type7_packet(CP_MEM_WRITE, 3);
	cmds[count++] = lower_32_bits(MEMSTORE_ID_GPU_ADDR(device,
		KGSL_MEMSTORE_GLOBAL, current_context));
	cmds[count++] = upper_32_bits(MEMSTORE_ID_GPU_ADDR(device,
		KGSL_MEMSTORE_GLOBAL, current_context));
	cmds[count++] = drawctxt->base.id;

	cmds[count++] = cp_type7_packet(CP_EVENT_WRITE, 1);
	cmds[count++] = 0x31;

	if (adreno_is_preemption_enabled(adreno_dev)) {
		u64 gpuaddr = drawctxt->base.user_ctxt_record->memdesc.gpuaddr;

		cmds[count++] = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 3);
		cmds[count++] = SET_PSEUDO_NON_PRIV_SAVE_ADDR;
		cmds[count++] = lower_32_bits(gpuaddr);
		cmds[count++] = upper_32_bits(gpuaddr);
	}

	return gen7_ringbuffer_addcmds(adreno_dev, rb, NULL, F_NOTPROTECTED,
			cmds, count, 0, NULL);
}

#define RB_SOPTIMESTAMP(device, rb) \
	MEMSTORE_RB_GPU_ADDR(device, rb, soptimestamp)
#define CTXT_SOPTIMESTAMP(device, drawctxt) \
	MEMSTORE_ID_GPU_ADDR(device, (drawctxt)->base.id, soptimestamp)

#define RB_EOPTIMESTAMP(device, rb) \
	MEMSTORE_RB_GPU_ADDR(device, rb, eoptimestamp)
#define CTXT_EOPTIMESTAMP(device, drawctxt) \
	MEMSTORE_ID_GPU_ADDR(device, (drawctxt)->base.id, eoptimestamp)

int gen7_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;
	unsigned long flags;

	adreno_get_submit_time(adreno_dev, rb, time);
	adreno_profile_submit_time(time);

	spin_lock_irqsave(&rb->preempt_lock, flags);
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {
		if (adreno_dev->cur_rb == rb) {
			kgsl_pwrscale_busy(device);
			ret = gen7_fenced_write(adreno_dev,
				GEN7_CP_RB_WPTR, rb->_wptr,
				FENCE_STATUS_WRITEDROPPED0_MASK);
			rb->skip_inline_wptr = false;
		}
	} else {
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
		gmu_core_fault_snapshot(device);
		adreno_dispatcher_fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
	}

	return ret;
}

int gen7_ringbuffer_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int i, ret;

	ret = adreno_allocate_global(device, &device->scratch, PAGE_SIZE,
			0, 0, KGSL_MEMDESC_RANDOM | KGSL_MEMDESC_PRIVILEGED,
			"scratch");
	if (ret)
		return ret;

	adreno_dev->cur_rb = &(adreno_dev->ringbuffers[0]);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION)) {
		adreno_dev->num_ringbuffers = 1;
		return adreno_ringbuffer_setup(adreno_dev,
				&adreno_dev->ringbuffers[0], 0);
	}

	adreno_dev->num_ringbuffers = ARRAY_SIZE(adreno_dev->ringbuffers);

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		int ret;

		ret = adreno_ringbuffer_setup(adreno_dev,
				&adreno_dev->ringbuffers[i], i);
		if (ret)
			return ret;
	}

	timer_setup(&adreno_dev->preempt.timer, adreno_preemption_timer, 0);
	gen7_preemption_init(adreno_dev);
	return 0;
}

#define GEN7_SUBMIT_MAX 104

int gen7_ringbuffer_addcmds(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 flags, u32 *in, u32 dwords, u32 timestamp,
		struct adreno_submit_time *time)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 size = GEN7_SUBMIT_MAX + dwords;
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

	/* All submissions are run with protected mode off due to APRIV */
	flags &= ~F_NOTPROTECTED;

	cmds = adreno_ringbuffer_allocspace(rb, size);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	/* Identify the start of a command */
	cmds[index++] = cp_type7_packet(CP_NOP, 1);
	cmds[index++] = drawctxt ? CMD_IDENTIFIER : CMD_INTERNAL_IDENTIFIER;

	/* This is 25 dwords when drawctxt is not NULL and perfcounter needs to be zapped*/
	index += gen7_preemption_pre_ibsubmit(adreno_dev, rb, drawctxt,
		&cmds[index]);

	cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[index++] = CP_SET_THREAD_BOTH;

	cmds[index++] = cp_type7_packet(CP_SET_MARKER, 1);
	cmds[index++] = 0x101; /* IFPC disable */

	cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[index++] = CP_SET_THREAD_BR;

	profile_gpuaddr = adreno_profile_preib_processing(adreno_dev,
		drawctxt, &profile_dwords);

	if (profile_gpuaddr) {
		cmds[index++] = cp_type7_packet(CP_INDIRECT_BUFFER_PFE, 3);
		cmds[index++] = lower_32_bits(profile_gpuaddr);
		cmds[index++] = upper_32_bits(profile_gpuaddr);
		cmds[index++] = profile_dwords;
	}

	if (drawctxt) {
		cmds[index++] = cp_type7_packet(CP_MEM_WRITE, 3);
		cmds[index++] = lower_32_bits(CTXT_SOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = upper_32_bits(CTXT_SOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = timestamp;
	}

	cmds[index++] = cp_type7_packet(CP_MEM_WRITE, 3);
	cmds[index++] = lower_32_bits(RB_SOPTIMESTAMP(device, rb));
	cmds[index++] = upper_32_bits(RB_SOPTIMESTAMP(device, rb));
	cmds[index++] = rb->timestamp;

	if (IS_SECURE(flags)) {
		/* Sync BV and BR if entering secure mode */
		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SYNC_THREADS | CP_CONCURRENT_BIN_DISABLE;
		cmds[index++] = cp_type7_packet(CP_SET_SECURE_MODE, 1);
		cmds[index++] = 1;
	}

	memcpy(&cmds[index], in, dwords << 2);
	index += dwords;

	profile_gpuaddr = adreno_profile_postib_processing(adreno_dev,
		drawctxt, &dwords);

	if (profile_gpuaddr) {
		cmds[index++] = cp_type7_packet(CP_INDIRECT_BUFFER_PFE, 3);
		cmds[index++] = lower_32_bits(profile_gpuaddr);
		cmds[index++] = upper_32_bits(profile_gpuaddr);
		cmds[index++] = profile_dwords;
	}

	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &device->mmu.pfpolicy))
		cmds[index++] = cp_type7_packet(CP_WAIT_MEM_WRITES, 0);

	if (is_concurrent_binning(drawctxt)) {
		u64 addr = SCRATCH_RB_GPU_ADDR(device, rb->id, bv_ts);

		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SET_THREAD_BV;

		/*
		 * Make sure the timestamp is committed once BV pipe is
		 * completely done with this submission.
		 */
		cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 4);
		cmds[index++] = CACHE_CLEAN | BIT(27);
		cmds[index++] = lower_32_bits(addr);
		cmds[index++] = upper_32_bits(addr);
		cmds[index++] = rb->timestamp;

		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SET_THREAD_BR;

		/*
		 * This makes sure that BR doesn't race ahead and commit
		 * timestamp to memstore while BV is still processing
		 * this submission.
		 */
		cmds[index++] = cp_type7_packet(CP_WAIT_TIMESTAMP, 4);
		cmds[index++] = 0;
		cmds[index++] = lower_32_bits(addr);
		cmds[index++] = upper_32_bits(addr);
		cmds[index++] = rb->timestamp;
	}

	/*
	 * If this is an internal command, just write the ringbuffer timestamp,
	 * otherwise, write both
	 */
	if (!drawctxt) {
		cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 4);
		cmds[index++] = CACHE_CLEAN | BIT(31) | BIT(27);
		cmds[index++] = lower_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = upper_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = rb->timestamp;
	} else {
		cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 4);
		cmds[index++] = CACHE_CLEAN | BIT(31) | BIT(27);
		cmds[index++] = lower_32_bits(CTXT_EOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = upper_32_bits(CTXT_EOPTIMESTAMP(device,
					drawctxt));
		cmds[index++] = timestamp;

		cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 4);
		cmds[index++] = CACHE_CLEAN | BIT(27);
		cmds[index++] = lower_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = upper_32_bits(RB_EOPTIMESTAMP(device, rb));
		cmds[index++] = rb->timestamp;
	}

	if (IS_WFI(flags))
		cmds[index++] = cp_type7_packet(CP_WAIT_FOR_IDLE, 0);

	if (IS_SECURE(flags)) {
		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_CONCURRENT_BIN_DISABLE;
		cmds[index++] = cp_type7_packet(CP_SET_SECURE_MODE, 1);
		cmds[index++] = 0;
		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SYNC_THREADS;
	}

	cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[index++] = CP_SET_THREAD_BOTH;

	cmds[index++] = cp_type7_packet(CP_SET_MARKER, 1);
	cmds[index++] = 0x100; /* IFPC enable */

	cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[index++] = CP_SET_THREAD_BR;

	/* 10 dwords */
	index += gen7_preemption_post_ibsubmit(adreno_dev, &cmds[index]);

	/* Adjust the thing for the number of bytes we actually wrote */
	rb->_wptr -= (size - index);

	return gen7_ringbuffer_submit(rb, time);
}

static u32 gen7_get_alwayson_counter(u32 *cmds, u64 gpuaddr)
{
	cmds[0] = cp_type7_packet(CP_REG_TO_MEM, 3);
	cmds[1] = GEN7_CP_ALWAYS_ON_COUNTER_LO | (1 << 30) | (2 << 18);
	cmds[2] = lower_32_bits(gpuaddr);
	cmds[3] = upper_32_bits(gpuaddr);

	return 4;
}

static u32 gen7_get_alwayson_context(u32 *cmds, u64 gpuaddr)
{
	cmds[0] = cp_type7_packet(CP_REG_TO_MEM, 3);
	cmds[1] = GEN7_CP_ALWAYS_ON_CONTEXT_LO | (1 << 30) | (2 << 18);
	cmds[2] = lower_32_bits(gpuaddr);
	cmds[3] = upper_32_bits(gpuaddr);

	return 4;
}

#define PROFILE_IB_DWORDS 4
#define PROFILE_IB_SLOTS (PAGE_SIZE / (PROFILE_IB_DWORDS << 2))

static u64 gen7_get_user_profiling_ib(struct adreno_ringbuffer *rb,
		struct kgsl_drawobj_cmd *cmdobj, u32 target_offset, u32 *cmds)
{
	u32 offset, *ib, dwords;

	if (IS_ERR(rb->profile_desc))
		return 0;

	offset = rb->profile_index * (PROFILE_IB_DWORDS << 2);
	ib = rb->profile_desc->hostptr + offset;
	dwords = gen7_get_alwayson_counter(ib,
		cmdobj->profiling_buffer_gpuaddr + target_offset);

	cmds[0] = cp_type7_packet(CP_INDIRECT_BUFFER_PFE, 3);
	cmds[1] = lower_32_bits(rb->profile_desc->gpuaddr + offset);
	cmds[2] = upper_32_bits(rb->profile_desc->gpuaddr + offset);
	cmds[3] = dwords;

	rb->profile_index = (rb->profile_index + 1) % PROFILE_IB_SLOTS;

	return 4;
}

static int gen7_drawctxt_switch(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_context *drawctxt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (rb->drawctxt_active == drawctxt)
		return 0;

	if (kgsl_context_detached(&drawctxt->base))
		return -ENOENT;

	if (!_kgsl_context_get(&drawctxt->base))
		return -ENOENT;

	ret = gen7_rb_context_switch(adreno_dev, rb, drawctxt);
	if (ret) {
		kgsl_context_put(&drawctxt->base);
		return ret;
	}

	trace_adreno_drawctxt_switch(rb, drawctxt);

	/* Release the current drawctxt as soon as the new one is switched */
	adreno_put_drawctxt_on_timestamp(device, rb->drawctxt_active,
		rb, rb->timestamp);

	rb->drawctxt_active = drawctxt;
	return 0;
}


#define GEN7_USER_PROFILE_IB(rb, cmdobj, cmds, field) \
	gen7_get_user_profiling_ib((rb), (cmdobj), \
		offsetof(struct kgsl_drawobj_profiling_buffer, field), \
		(cmds))

#define GEN7_KERNEL_PROFILE(dev, cmdobj, cmds, field) \
	gen7_get_alwayson_counter((cmds), \
		(dev)->profile_buffer->gpuaddr + \
			ADRENO_DRAWOBJ_PROFILE_OFFSET((cmdobj)->profile_index, \
				field))

#define GEN7_KERNEL_PROFILE_CONTEXT(dev, cmdobj, cmds, field) \
	gen7_get_alwayson_context((cmds), \
		(dev)->profile_buffer->gpuaddr + \
			ADRENO_DRAWOBJ_PROFILE_OFFSET((cmdobj)->profile_index, \
				field))

#define GEN7_COMMAND_DWORDS 60

int gen7_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
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

	cmds = kvmalloc((GEN7_COMMAND_DWORDS + (numibs * 5)) << 2, GFP_KERNEL);
	if (!cmds) {
		ret = -ENOMEM;
		goto done;
	}

	cmds[index++] = cp_type7_packet(CP_NOP, 1);
	cmds[index++] = START_IB_IDENTIFIER;

	/* Kernel profiling: 8 dwords */
	if (IS_KERNEL_PROFILE(flags)) {
		index += GEN7_KERNEL_PROFILE(adreno_dev, cmdobj, &cmds[index],
			started);
		index += GEN7_KERNEL_PROFILE_CONTEXT(adreno_dev, cmdobj, &cmds[index],
			ctx_start);
	}

	/* User profiling: 4 dwords */
	if (IS_USER_PROFILE(flags))
		index += GEN7_USER_PROFILE_IB(rb, cmdobj, &cmds[index],
			gpu_ticks_submitted);

	if (is_concurrent_binning(drawctxt)) {
		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SET_THREAD_BOTH;
	}
	if (numibs) {
		struct kgsl_memobj_node *ib;

		cmds[index++] = cp_type7_packet(CP_SET_MARKER, 1);
		cmds[index++] = 0x00d; /* IB1LIST start */

		list_for_each_entry(ib, &cmdobj->cmdlist, node) {
			if (ib->priv & MEMOBJ_SKIP ||
				(ib->flags & KGSL_CMDLIST_CTXTSWITCH_PREAMBLE &&
				 !IS_PREAMBLE(flags)))
				cmds[index++] = cp_type7_packet(CP_NOP, 4);

			cmds[index++] =
				cp_type7_packet(CP_INDIRECT_BUFFER_PFE, 3);
			cmds[index++] = lower_32_bits(ib->gpuaddr);
			cmds[index++] = upper_32_bits(ib->gpuaddr);

			/* Double check that IB_PRIV is never set */
			cmds[index++] = (ib->size >> 2) & 0xfffff;
		}

		cmds[index++] = cp_type7_packet(CP_SET_MARKER, 1);
		cmds[index++] = 0x00e; /* IB1LIST end */
	}

	if (is_concurrent_binning(drawctxt)) {
		cmds[index++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
		cmds[index++] = CP_SET_THREAD_BR;
	}
	/* CCU invalidate depth */
	cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 1);
	cmds[index++] = 24;

	/* CCU invalidate color */
	cmds[index++] = cp_type7_packet(CP_EVENT_WRITE, 1);
	cmds[index++] = 25;

	/* 8 dwords */
	if (IS_KERNEL_PROFILE(flags)) {
		index += GEN7_KERNEL_PROFILE(adreno_dev, cmdobj, &cmds[index],
			retired);
		index += GEN7_KERNEL_PROFILE_CONTEXT(adreno_dev, cmdobj, &cmds[index],
			ctx_end);
	}

	/* 4 dwords */
	if (IS_USER_PROFILE(flags))
		index += GEN7_USER_PROFILE_IB(rb, cmdobj, &cmds[index],
			gpu_ticks_retired);

	cmds[index++] = cp_type7_packet(CP_NOP, 1);
	cmds[index++] = END_IB_IDENTIFIER;

	ret = gen7_drawctxt_switch(adreno_dev, rb, drawctxt);

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
				"Unable to switch draw context: %d\n", ret);
		goto done;
	}

	adreno_drawobj_set_constraint(device, drawobj);

	ret = gen7_ringbuffer_addcmds(adreno_dev, drawctxt->rb, drawctxt,
		flags, cmds, index, drawobj->timestamp, time);

done:
	trace_kgsl_issueibcmds(device, drawctxt->base.id, numibs,
		drawobj->timestamp, drawobj->flags, ret, drawctxt->type);

	kvfree(cmds);
	return ret;
}
