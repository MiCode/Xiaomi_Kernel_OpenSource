// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>

#include "a3xx_reg.h"
#include "adreno.h"
#include "adreno_iommu.h"
#include "adreno_pm4types.h"

/*
 * a3xx_wait_reg() - make CP poll on a register
 * @cmds:	Pointer to memory where commands are to be added
 * @addr:	Register address to poll for
 * @val:	Value to poll for
 * @mask:	The value against which register value is masked
 * @interval:	wait interval
 */
static unsigned int a3xx_wait_reg(struct adreno_device *adreno_dev,
			unsigned int *cmds, unsigned int addr,
			unsigned int val, unsigned int mask,
			unsigned int interval)
{
	unsigned int *start = cmds;

	*cmds++ = cp_packet(adreno_dev, CP_WAIT_REG_EQ, 4);
	*cmds++ = addr;
	*cmds++ = val;
	*cmds++ = mask;
	*cmds++ = interval;

	return cmds - start;
}

static unsigned int a3xx_vbif_lock(struct adreno_device *adreno_dev,
			unsigned int *cmds)
{
	unsigned int *start = cmds;
	/*
	 * glue commands together until next
	 * WAIT_FOR_ME
	 */
	cmds += a3xx_wait_reg(adreno_dev, cmds, A3XX_CP_WFI_PEND_CTR,
			1, 0xFFFFFFFF, 0xF);

	/* MMU-500 VBIF stall */
	*cmds++ = cp_packet(adreno_dev, CP_REG_RMW, 3);
	*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
	/* AND to unmask the HALT bit */
	*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
	/* OR to set the HALT bit */
	*cmds++ = 0x1;

	/* Wait for acknowledgment */
	cmds += a3xx_wait_reg(adreno_dev, cmds,
			A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL1,
			1, 0xFFFFFFFF, 0xF);

	return cmds - start;
}

static unsigned int a3xx_vbif_unlock(struct adreno_device *adreno_dev,
				unsigned int *cmds)
{
	unsigned int *start = cmds;

	/* MMU-500 VBIF unstall */
	*cmds++ = cp_packet(adreno_dev, CP_REG_RMW, 3);
	*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
	/* AND to unmask the HALT bit */
	*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
	/* OR to reset the HALT bit */
	*cmds++ = 0;

	/* release all commands since _vbif_lock() with wait_for_me */
	cmds += cp_wait_for_me(adreno_dev, cmds);
	return cmds - start;
}

#define A3XX_GPU_OFFSET 0xa000

/* This function is only needed for A3xx targets */
static unsigned int a3xx_cp_smmu_reg(struct adreno_device *adreno_dev,
				unsigned int *cmds,
				enum kgsl_iommu_reg_map reg,
				unsigned int num)
{
	unsigned int *start = cmds;
	unsigned int offset = (A3XX_GPU_OFFSET + kgsl_iommu_reg_list[reg]) >> 2;

	*cmds++ = cp_packet(adreno_dev, CP_REG_WR_NO_CTXT, num + 1);
	*cmds++ = offset;

	return cmds - start;
}

/* This function is only needed for A3xx targets */
static unsigned int a3xx_tlbiall(struct adreno_device *adreno_dev,
				unsigned int *cmds)
{
	unsigned int *start = cmds;
	unsigned int tlbstatus = (A3XX_GPU_OFFSET +
		kgsl_iommu_reg_list[KGSL_IOMMU_CTX_TLBSTATUS]) >> 2;

	cmds += a3xx_cp_smmu_reg(adreno_dev, cmds, KGSL_IOMMU_CTX_TLBIALL, 1);
	*cmds++ = 1;

	cmds += a3xx_cp_smmu_reg(adreno_dev, cmds, KGSL_IOMMU_CTX_TLBSYNC, 1);
	*cmds++ = 0;

	cmds += a3xx_wait_reg(adreno_dev, cmds, tlbstatus, 0,
			KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);

	return cmds - start;
}


/**
 * _adreno_iommu_add_idle_cmds - Add pm4 packets for GPU idle
 * @adreno_dev - Pointer to device structure
 * @cmds - Pointer to memory where idle commands need to be added
 */
static inline int _adreno_iommu_add_idle_cmds(struct adreno_device *adreno_dev,
							unsigned int *cmds)
{
	unsigned int *start = cmds;

	cmds += cp_wait_for_idle(adreno_dev, cmds);

	if (adreno_is_a3xx(adreno_dev))
		cmds += cp_wait_for_me(adreno_dev, cmds);

	return cmds - start;
}

/**
 * adreno_iommu_set_apriv() - Generate commands to set/reset the APRIV
 * @adreno_dev: Device on which the commands will execute
 * @cmds: The memory pointer where commands are generated
 * @set: If set then APRIV is set else reset
 *
 * Returns the number of commands generated
 */
static unsigned int adreno_iommu_set_apriv(struct adreno_device *adreno_dev,
				unsigned int *cmds, int set)
{
	unsigned int *cmds_orig = cmds;

	/* adreno 3xx doesn't have the CP_CNTL.APRIV field */
	if (adreno_is_a3xx(adreno_dev))
		return 0;

	/* Targets with apriv control do not need to explicitly set the bit */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		return 0;

	cmds += cp_wait_for_idle(adreno_dev, cmds);
	cmds += cp_wait_for_me(adreno_dev, cmds);
	*cmds++ = cp_register(adreno_dev, adreno_getreg(adreno_dev,
				ADRENO_REG_CP_CNTL), 1);
	if (set)
		*cmds++ = 1;
	else
		*cmds++ = 0;

	return cmds - cmds_orig;
}

static inline int _adreno_iommu_add_idle_indirect_cmds(
			struct adreno_device *adreno_dev,
			unsigned int *cmds, uint64_t nop_gpuaddr)
{
	unsigned int *start = cmds;
	/*
	 * Adding an indirect buffer ensures that the prefetch stalls until
	 * the commands in indirect buffer have completed. We need to stall
	 * prefetch with a nop indirect buffer when updating pagetables
	 * because it provides stabler synchronization.
	 */
	cmds += cp_wait_for_me(adreno_dev, cmds);
	*cmds++ = cp_mem_packet(adreno_dev, CP_INDIRECT_BUFFER_PFE, 2, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds, nop_gpuaddr);
	*cmds++ = 2;
	cmds += cp_wait_for_idle(adreno_dev, cmds);
	return cmds - start;
}

static unsigned int _adreno_iommu_set_pt_v2_a3xx(struct kgsl_device *device,
					unsigned int *cmds_orig,
					u64 ttbr0, u32 contextidr)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int *cmds = cmds_orig;

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);

	cmds += a3xx_vbif_lock(adreno_dev, cmds);

	cmds += a3xx_cp_smmu_reg(adreno_dev, cmds, KGSL_IOMMU_CTX_TTBR0, 2);
	*cmds++ = lower_32_bits(ttbr0);
	*cmds++ = upper_32_bits(ttbr0);
	cmds += a3xx_cp_smmu_reg(adreno_dev, cmds, KGSL_IOMMU_CTX_CONTEXTIDR,
		1);
	*cmds++ = contextidr;

	cmds += a3xx_vbif_unlock(adreno_dev, cmds);

	cmds += a3xx_tlbiall(adreno_dev, cmds);

	/* wait for me to finish the TLBI */
	cmds += cp_wait_for_me(adreno_dev, cmds);

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

static unsigned int _adreno_iommu_set_pt_v2_a5xx(struct kgsl_device *device,
					unsigned int *cmds_orig,
					u64 ttbr0, u32 contextidr,
					struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int *cmds = cmds_orig;

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);
	cmds += cp_wait_for_me(adreno_dev, cmds);

	/* CP switches the pagetable and flushes the Caches */
	*cmds++ = cp_packet(adreno_dev, CP_SMMU_TABLE_UPDATE, 3);
	*cmds++ = lower_32_bits(ttbr0);
	*cmds++ = upper_32_bits(ttbr0);
	*cmds++ = contextidr;

	*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 4, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds, (rb->pagetable_desc.gpuaddr +
		PT_INFO_OFFSET(ttbr0)));
	*cmds++ = lower_32_bits(ttbr0);
	*cmds++ = upper_32_bits(ttbr0);
	*cmds++ = contextidr;

	/* release all commands with wait_for_me */
	cmds += cp_wait_for_me(adreno_dev, cmds);

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

static unsigned int _adreno_iommu_set_pt_v2_a6xx(struct kgsl_device *device,
					unsigned int *cmds_orig,
					u64 ttbr0, u32 contextidr,
					struct adreno_ringbuffer *rb,
					unsigned int cb_num)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int *cmds = cmds_orig;

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);
	cmds += cp_wait_for_me(adreno_dev, cmds);

	/* CP switches the pagetable and flushes the Caches */
	*cmds++ = cp_packet(adreno_dev, CP_SMMU_TABLE_UPDATE, 4);
	*cmds++ = lower_32_bits(ttbr0);
	*cmds++ = upper_32_bits(ttbr0);
	*cmds++ = contextidr;
	*cmds++ = cb_num;

	*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 4, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds, (rb->pagetable_desc.gpuaddr +
		PT_INFO_OFFSET(ttbr0)));
	*cmds++ = lower_32_bits(ttbr0);
	*cmds++ = upper_32_bits(ttbr0);
	*cmds++ = contextidr;

	/* release all commands with wait_for_me */
	cmds += cp_wait_for_me(adreno_dev, cmds);

	cmds += _adreno_iommu_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

/**
 * adreno_iommu_set_pt_generate_cmds() - Generate commands to change pagetable
 * @rb: The RB pointer in which these commaands are to be submitted
 * @cmds: The pointer where the commands are placed
 * @pt: The pagetable to switch to
 */
unsigned int adreno_iommu_set_pt_generate_cmds(
					struct adreno_ringbuffer *rb,
					unsigned int *cmds,
					struct kgsl_pagetable *pt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);
	struct kgsl_iommu_context *ctx = &iommu->ctx[KGSL_IOMMU_CONTEXT_USER];
	u64 ttbr0;
	u32 contextidr;
	unsigned int *cmds_orig = cmds;

	ttbr0 = kgsl_mmu_pagetable_get_ttbr0(pt);
	contextidr = kgsl_mmu_pagetable_get_contextidr(pt);

	cmds += adreno_iommu_set_apriv(adreno_dev, cmds, 1);

	cmds += _adreno_iommu_add_idle_indirect_cmds(adreno_dev, cmds,
		iommu->setstate.gpuaddr + KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	if (adreno_is_a6xx(adreno_dev))
		cmds += _adreno_iommu_set_pt_v2_a6xx(device, cmds,
					ttbr0, contextidr, rb,
					ctx->cb_num);
	else if (adreno_is_a5xx(adreno_dev))
		cmds += _adreno_iommu_set_pt_v2_a5xx(device, cmds,
					ttbr0, contextidr, rb);
	else if (adreno_is_a3xx(adreno_dev))
		cmds += _adreno_iommu_set_pt_v2_a3xx(device, cmds,
					ttbr0, contextidr);

	/* invalidate all base pointers */
	cmds += cp_invalidate_state(adreno_dev, cmds);

	cmds += adreno_iommu_set_apriv(adreno_dev, cmds, 0);

	return cmds - cmds_orig;
}

/**
 * __add_curr_ctxt_cmds() - Add commands to set a context id in memstore
 * @rb: The RB in which the commands will be added for execution
 * @cmds: Pointer to memory where commands are added
 * @drawctxt: The context whose id is being set in memstore
 *
 * Returns the number of dwords
 */
static unsigned int __add_curr_ctxt_cmds(struct adreno_ringbuffer *rb,
			unsigned int *cmds,
			struct adreno_context *drawctxt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds_orig = cmds;

	/* write the context identifier to memstore memory */

	cmds += cp_identifier(adreno_dev, cmds, CONTEXT_TO_MEM_IDENTIFIER);

	*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds,
			MEMSTORE_RB_GPU_ADDR(device, rb, current_context));
	*cmds++ = (drawctxt ? drawctxt->base.id : 0);

	*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds,
			MEMSTORE_ID_GPU_ADDR(device,
				KGSL_MEMSTORE_GLOBAL, current_context));
	*cmds++ = (drawctxt ? drawctxt->base.id : 0);

	/* Invalidate UCHE for new context */
	if (adreno_is_a6xx(adreno_dev)) {
		*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
		*cmds++ = 0x31; /* CACHE_INVALIDATE */
	} else if (adreno_is_a5xx(adreno_dev)) {
		*cmds++ = cp_register(adreno_dev,
			adreno_getreg(adreno_dev,
		ADRENO_REG_UCHE_INVALIDATE0), 1);
		*cmds++ = 0x12;
	} else if (adreno_is_a3xx(adreno_dev)) {
		*cmds++ = cp_register(adreno_dev,
			adreno_getreg(adreno_dev,
			ADRENO_REG_UCHE_INVALIDATE0), 2);
		*cmds++ = 0;
		*cmds++ = 0x90000000;
	} else
		WARN_ONCE(1, "GPU UCHE invalidate sequence not defined\n");

	return cmds - cmds_orig;
}

/**
 * _set_ctxt_gpu() - Add commands to set the current context in memstore
 * @rb: The ringbuffer in which commands to set memstore are added
 * @drawctxt: The context whose id is being set in memstore
 */
static int _set_ctxt_gpu(struct adreno_ringbuffer *rb,
			struct adreno_context *drawctxt)
{
	unsigned int link[15], *cmds;
	int result;

	cmds = &link[0];
	cmds += __add_curr_ctxt_cmds(rb, cmds, drawctxt);
	result = adreno_ringbuffer_issue_internal_cmds(rb, 0, link,
			(unsigned int)(cmds - link));
	return result;
}

/**
 * _set_pagetable_gpu() - Use GPU to switch the pagetable
 * @rb: The rb in which commands to switch pagetable are to be
 *    submitted
 * @new_pt: The pagetable to switch to
 */
static int _set_pagetable_gpu(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int *link = NULL, count;
	int result;

	link = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (link == NULL)
		return -ENOMEM;

	/* If we are in a fault the MMU will be reset soon */
	if (test_bit(ADRENO_DEVICE_FAULT, &adreno_dev->priv)) {
		kfree(link);
		return 0;
	}

	count = adreno_iommu_set_pt_generate_cmds(rb, link, new_pt);

	WARN(count > (PAGE_SIZE / sizeof(unsigned int)),
		"Temp command buffer overflow\n");

	/*
	 * This returns the per context timestamp but we need to
	 * use the global timestamp for iommu clock disablement
	 */
	result = adreno_ringbuffer_issue_internal_cmds(rb,
			KGSL_CMD_FLAGS_PMODE, link, count);

	kfree(link);
	return result;
}

/**
 * adreno_iommu_init() - Adreno iommu init
 * @adreno_dev: Adreno device
 */
void adreno_iommu_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_iommu *iommu = KGSL_IOMMU_PRIV(device);

	if (kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE)
		return;

	/*
	 * A nop is required in an indirect buffer when switching
	 * pagetables in-stream
	 */

	kgsl_sharedmem_writel(device, &iommu->setstate,
				KGSL_IOMMU_SETSTATE_NOP_OFFSET,
				cp_packet(adreno_dev, CP_NOP, 1));

	/* Enable guard page MMU feature for A3xx and A4xx targets only */
	if (adreno_is_a3xx(adreno_dev))
		device->mmu.features |= KGSL_MMU_NEED_GUARD_PAGE;
}

/**
 * adreno_iommu_set_pt_ctx() - Change the pagetable of the current RB
 * @device: Pointer to device to which the rb belongs
 * @rb: The RB pointer on which pagetable is to be changed
 * @new_pt: The new pt the device will change to
 * @drawctxt: The context whose pagetable the ringbuffer is switching to,
 * NULL means KGSL_CONTEXT_GLOBAL
 *
 * Returns 0 on success else error code.
 */
int adreno_iommu_set_pt_ctx(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt,
			struct adreno_context *drawctxt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pagetable *cur_pt = device->mmu.defaultpagetable;
	int result = 0;

	/* Switch the page table if a MMU is attached */
	if (kgsl_mmu_get_mmutype(device) != KGSL_MMU_TYPE_NONE) {
		if (rb->drawctxt_active)
			cur_pt = rb->drawctxt_active->base.proc_priv->pagetable;

		/* Pagetable switch */
		if (new_pt != cur_pt)
			result = _set_pagetable_gpu(rb, new_pt);

		if (result)
			return result;
	}

	/* Context switch */
	return _set_ctxt_gpu(rb, drawctxt);
}
