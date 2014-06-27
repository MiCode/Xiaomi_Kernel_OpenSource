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
#include "adreno.h"

static unsigned int _adreno_iommu_set_pt_v0(struct kgsl_device *device,
					unsigned int *cmds_orig,
					phys_addr_t pt_val,
					int num_iommu_units)
{
	phys_addr_t reg_pt_val;
	unsigned int *cmds = cmds_orig;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	cmds += adreno_add_bank_change_cmds(cmds,
				KGSL_IOMMU_CONTEXT_USER,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	/* Acquire GPU-CPU sync Lock here */
	cmds += kgsl_mmu_sync_lock(&device->mmu, cmds);

	/*
	 * We need to perfrom the following operations for all
	 * IOMMU units
	 */
	for (i = 0; i < num_iommu_units; i++) {
		reg_pt_val = kgsl_mmu_get_default_ttbr0(&device->mmu,
					i, KGSL_IOMMU_CONTEXT_USER);
		reg_pt_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
		reg_pt_val |= (pt_val & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK);
		/*
		 * Set address of the new pagetable by writng to IOMMU
		 * TTBR0 register
		 */
		*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
		*cmds++ = kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
			KGSL_IOMMU_CONTEXT_USER, KGSL_IOMMU_CTX_TTBR0);
		*cmds++ = reg_pt_val;
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
		*cmds++ = 0x00000000;

		/*
		 * Read back the ttbr0 register as a barrier to ensure
		 * above writes have completed
		 */
		cmds += adreno_add_read_cmds(cmds,
			kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER, KGSL_IOMMU_CTX_TTBR0),
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	}
	/*
	 * tlb flush
	 */
	for (i = 0; i < num_iommu_units; i++) {
		reg_pt_val = (pt_val + kgsl_mmu_get_default_ttbr0(
					&device->mmu,
					i, KGSL_IOMMU_CONTEXT_USER));
		reg_pt_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
		reg_pt_val |= (pt_val & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK);
		*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
		*cmds++ = kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
			KGSL_IOMMU_CONTEXT_USER,
			KGSL_IOMMU_CTX_TLBIALL);
		*cmds++ = 1;

		cmds += __adreno_add_idle_indirect_cmds(cmds,
			device->mmu.setstate_memory.gpuaddr +
			KGSL_IOMMU_SETSTATE_NOP_OFFSET);

		cmds += adreno_add_read_cmds(cmds,
			kgsl_mmu_get_reg_gpuaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER,
				KGSL_IOMMU_CTX_TTBR0),
				reg_pt_val,
				device->mmu.setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET);
	}

	/* Release GPU-CPU sync Lock here */
	cmds += kgsl_mmu_sync_unlock(&device->mmu, cmds);

	cmds += adreno_add_bank_change_cmds(cmds,
		KGSL_IOMMU_CONTEXT_PRIV,
		device->mmu.setstate_memory.gpuaddr +
		KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

static unsigned int _adreno_iommu_set_pt_v1(struct kgsl_device *device,
					unsigned int *cmds_orig,
					phys_addr_t pt_val,
					int num_iommu_units)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t ttbr0_val;
	unsigned int reg_pt_val;
	unsigned int *cmds = cmds_orig;
	int i;
	unsigned int ttbr0, tlbiall, tlbstatus, tlbsync, mmu_ctrl;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	for (i = 0; i < num_iommu_units; i++) {
		ttbr0_val = kgsl_mmu_get_default_ttbr0(&device->mmu,
				i, KGSL_IOMMU_CONTEXT_USER);
		ttbr0_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
		ttbr0_val |= (pt_val & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK);
		mmu_ctrl = kgsl_mmu_get_reg_ahbaddr(
			&device->mmu, i,
			KGSL_IOMMU_CONTEXT_USER,
			KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL) >> 2;

		ttbr0 = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TTBR0) >> 2;

		if (kgsl_mmu_hw_halt_supported(&device->mmu, i)) {
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
			*cmds++ = 0;
			/*
			 * glue commands together until next
			 * WAIT_FOR_ME
			 */
			if (adreno_is_a4xx(adreno_dev))
				cmds += adreno_wait_reg_mem(cmds,
				adreno_getreg(adreno_dev,
					ADRENO_REG_CP_WFI_PEND_CTR),
					1, 0xFFFFFFFF, 0xF);
			else
				cmds += adreno_wait_reg_eq(cmds,
				adreno_getreg(adreno_dev,
					ADRENO_REG_CP_WFI_PEND_CTR),
					1, 0xFFFFFFFF, 0xF);

			/* set the iommu lock bit */
			*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
			*cmds++ = mmu_ctrl;
			/* AND to unmask the lock bit */
			*cmds++ =
				 ~(KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT);
				/* OR to set the IOMMU lock bit */
			*cmds++ =
				   KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT;
			/* wait for smmu to lock */
			if (adreno_is_a4xx(adreno_dev))
				cmds += adreno_wait_reg_mem(cmds,
					mmu_ctrl,
					KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE,
					KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE,
					0xF);
			else
				cmds += adreno_wait_reg_eq(cmds,
					mmu_ctrl,
					KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE,
					KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE,
					0xF);
		}
		/*
		 * set ttbr0, only need to set the higer bits if the
		 * address bits lie in the higher bits
		 */
		if (KGSL_IOMMU_CTX_TTBR0_ADDR_MASK &
			0xFFFFFFFF00000000ULL) {
			reg_pt_val = (unsigned int)ttbr0_val &
					0xFFFFFFFF;
			*cmds++ = cp_type0_packet(ttbr0, 1);
			*cmds++ = reg_pt_val;
			reg_pt_val = (unsigned int)
				((ttbr0_val & 0xFFFFFFFF00000000ULL) >> 32);
			*cmds++ = cp_type0_packet(ttbr0 + 1, 1);
			*cmds++ = reg_pt_val;
		} else {
			reg_pt_val = ttbr0_val;
			*cmds++ = cp_type0_packet(ttbr0, 1);
			*cmds++ = reg_pt_val;
		}
		if (kgsl_mmu_hw_halt_supported(&device->mmu, i)) {
			/* unlock the IOMMU lock */
			*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
			*cmds++ = mmu_ctrl;
			/* AND to unmask the lock bit */
			*cmds++ =
			   ~(KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT);
			/* OR with 0 so lock bit is unset */
			*cmds++ = 0;
			/* release all commands with wait_for_me */
			*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
			*cmds++ = 0;
		}
		tlbiall = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBIALL) >> 2;
		*cmds++ = cp_type0_packet(tlbiall, 1);
		*cmds++ = 1;

		tlbsync = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBSYNC) >> 2;
		*cmds++ = cp_type0_packet(tlbsync, 1);
		*cmds++ = 0;

		tlbstatus = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER,
				KGSL_IOMMU_CTX_TLBSTATUS) >> 2;
		if (adreno_is_a4xx(adreno_dev))
			cmds += adreno_wait_reg_mem(cmds, tlbstatus, 0,
				KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);
		else
			cmds += adreno_wait_reg_eq(cmds, tlbstatus, 0,
				KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);
		/* release all commands with wait_for_me */
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;
	}

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}


static unsigned int _adreno_iommu_set_pt_v2_a3xx(struct kgsl_device *device,
					unsigned int *cmds_orig,
					phys_addr_t pt_val,
					int num_iommu_units)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t ttbr0_val;
	unsigned int reg_pt_val;
	unsigned int *cmds = cmds_orig;
	int i;
	unsigned int ttbr0, tlbiall, tlbstatus, tlbsync;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	for (i = 0; i < num_iommu_units; i++) {
		ttbr0_val = kgsl_mmu_get_default_ttbr0(&device->mmu,
				i, KGSL_IOMMU_CONTEXT_USER);
		ttbr0_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
		ttbr0_val |= (pt_val & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK);
		ttbr0 = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TTBR0) >> 2;

		/*
		 * glue commands together until next
		 * WAIT_FOR_ME
		 */
		cmds += adreno_wait_reg_eq(cmds,
			adreno_getreg(adreno_dev,
				ADRENO_REG_CP_WFI_PEND_CTR),
				1, 0xFFFFFFFF, 0xF);

		/* MMU-500 VBIF stall */
		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
		/* AND to unmask the HALT bit */
		*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
		/* OR to set the HALT bit */
		*cmds++ = 0x1;

		/* Wait for acknowledgement */
		cmds += adreno_wait_reg_eq(cmds,
			A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL1,
				1, 0xFFFFFFFF, 0xF);

		/* set ttbr0 */
		if (KGSL_IOMMU_CTX_TTBR0_ADDR_MASK &
			0xFFFFFFFF00000000ULL) {
			reg_pt_val = (unsigned int)ttbr0_val &
					0xFFFFFFFF;
			*cmds++ = cp_type3_packet(CP_REG_WR_NO_CTXT, 2);
			*cmds++ = ttbr0;
			*cmds++ = reg_pt_val;
			reg_pt_val = (unsigned int)
			((ttbr0_val & 0xFFFFFFFF00000000ULL) >> 32);
			*cmds++ = cp_type3_packet(CP_REG_WR_NO_CTXT, 2);
			*cmds++ = ttbr0 + 1;
			*cmds++ = reg_pt_val;
		} else {
			reg_pt_val = ttbr0_val;
			*cmds++ = cp_type3_packet(CP_REG_WR_NO_CTXT, 2);
			*cmds++ = ttbr0;
			*cmds++ = reg_pt_val;
		}

		/* MMU-500 VBIF unstall */
		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
		/* AND to unmask the HALT bit */
		*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
		/* OR to reset the HALT bit */
		*cmds++ = 0;

		/* release all commands with wait_for_me */
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;

		tlbiall = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBIALL) >> 2;
		*cmds++ = cp_type3_packet(CP_REG_WR_NO_CTXT, 2);
		*cmds++ = tlbiall;
		*cmds++ = 1;

		tlbsync = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBSYNC) >> 2;
		*cmds++ = cp_type3_packet(CP_REG_WR_NO_CTXT, 2);
		*cmds++ = tlbsync;
		*cmds++ = 0;

		tlbstatus = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBSTATUS) >> 2;
		cmds += adreno_wait_reg_eq(cmds, tlbstatus, 0,
				KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);
			/* release all commands with wait_for_me */
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;
	}

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

static unsigned int _adreno_iommu_set_pt_v2_a4xx(struct kgsl_device *device,
					unsigned int *cmds_orig,
					phys_addr_t pt_val,
					int num_iommu_units)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	uint64_t ttbr0_val;
	unsigned int reg_pt_val;
	unsigned int *cmds = cmds_orig;
	int i;
	unsigned int ttbr0, tlbiall, tlbstatus, tlbsync;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	for (i = 0; i < num_iommu_units; i++) {
		ttbr0_val = kgsl_mmu_get_default_ttbr0(&device->mmu,
				i, KGSL_IOMMU_CONTEXT_USER);
		ttbr0_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
		ttbr0_val |= (pt_val & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK);
		ttbr0 = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TTBR0) >> 2;

		/*
		 * glue commands together until next
		 * WAIT_FOR_ME
		 */
		cmds += adreno_wait_reg_mem(cmds,
				adreno_getreg(adreno_dev,
					ADRENO_REG_CP_WFI_PEND_CTR),
					1, 0xFFFFFFFF, 0xF);

		/* MMU-500 VBIF stall */
		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
		/* AND to unmask the HALT bit */
		*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
		/* OR to set the HALT bit */
		*cmds++ = 0x1;

		/* Wait for acknowledgement */
		cmds += adreno_wait_reg_mem(cmds,
			A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL1,
				1, 0xFFFFFFFF, 0xF);

		/* set ttbr0 */
		if (sizeof(phys_addr_t) > sizeof(unsigned int)) {

			reg_pt_val = ttbr0_val & 0xFFFFFFFF;
			*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
			*cmds++ = ttbr0;
			*cmds++ = reg_pt_val;

			reg_pt_val = (unsigned int)((ttbr0_val &
				0xFFFFFFFF00000000ULL) >> 32);
			*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
			*cmds++ = ttbr0+1;
			*cmds++ = reg_pt_val;
		} else {
			reg_pt_val = ttbr0_val;
			*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
			*cmds++ = ttbr0;
			*cmds++ = reg_pt_val;
		}

		/* MMU-500 VBIF unstall */
		*cmds++ = cp_type3_packet(CP_REG_RMW, 3);
		*cmds++ = A3XX_VBIF_DDR_OUTPUT_RECOVERABLE_HALT_CTRL0;
		/* AND to unmask the HALT bit */
		*cmds++ = ~(VBIF_RECOVERABLE_HALT_CTRL);
		/* OR to reset the HALT bit */
		*cmds++ = 0;

		/* release all commands with wait_for_me */
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;

		tlbiall = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBIALL) >> 2;

		*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
		*cmds++ = tlbiall;
		*cmds++ = 1;

		tlbsync = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
					KGSL_IOMMU_CONTEXT_USER,
					KGSL_IOMMU_CTX_TLBSYNC) >> 2;

		*cmds++ = cp_type3_packet(CP_WIDE_REG_WRITE, 2);
		*cmds++ = tlbsync;
		*cmds++ = 0;

		tlbstatus = kgsl_mmu_get_reg_ahbaddr(&device->mmu, i,
				KGSL_IOMMU_CONTEXT_USER,
				KGSL_IOMMU_CTX_TLBSTATUS) >> 2;
		cmds += adreno_wait_reg_mem(cmds, tlbstatus, 0,
				KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE, 0xF);
		/* release all commands with wait_for_me */
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;
	}

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - cmds_orig;
}

/**
 * _set_pagetable_gpu() - Use GPU to switch the pagetable
 * @rb: The ringbuffer in which commands to switch pagetable are to be submitted
 * @new_pt: The pagetable to switch to
 */
int _set_pagetable_gpu(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt)
{
	phys_addr_t pt_val;
	unsigned int *link = NULL, *cmds;
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int num_iommu_units;
	int result;

	num_iommu_units = kgsl_mmu_get_num_iommu_units(&device->mmu);

	link = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (link == NULL) {
		result = -ENOMEM;
		goto done;
	}

	cmds = link;

	kgsl_mmu_enable_clk(&device->mmu, KGSL_IOMMU_MAX_UNITS);

	pt_val = kgsl_mmu_get_pt_base_addr(&device->mmu, new_pt);

	cmds += __adreno_add_idle_indirect_cmds(cmds,
		device->mmu.setstate_memory.gpuaddr +
		KGSL_IOMMU_SETSTATE_NOP_OFFSET);

	if (kgsl_msm_supports_iommu_v2())
		if (adreno_is_a4xx(adreno_dev))
			cmds += _adreno_iommu_set_pt_v2_a4xx(device, cmds,
					pt_val, num_iommu_units);
		else
			cmds += _adreno_iommu_set_pt_v2_a3xx(device, cmds,
					pt_val, num_iommu_units);
	else if (msm_soc_version_supports_iommu_v0())
		cmds += _adreno_iommu_set_pt_v0(device, cmds, pt_val,
						num_iommu_units);
	else
		cmds += _adreno_iommu_set_pt_v1(device, cmds, pt_val,
						num_iommu_units);

	/* invalidate all base pointers */
	*cmds++ = cp_type3_packet(CP_INVALIDATE_STATE, 1);
	*cmds++ = 0x7fff;

	if ((unsigned int) (cmds - link) > (PAGE_SIZE / sizeof(unsigned int))) {
		KGSL_DRV_ERR(device, "Temp command buffer overflow\n");
		BUG();
	}
	/*
	 * This returns the per context timestamp but we need to
	 * use the global timestamp for iommu clock disablement
	 */
	result = adreno_ringbuffer_issuecmds(rb,
			KGSL_CMD_FLAGS_PMODE, link,
			(unsigned int)(cmds - link));

	/*
	 * On error disable the IOMMU clock right away otherwise turn it off
	 * after the command has been retired
	 */
	if (result)
		kgsl_mmu_disable_clk(&device->mmu, KGSL_IOMMU_MAX_UNITS);
	else
		adreno_ringbuffer_mmu_disable_clk_on_ts(device, rb,
						KGSL_IOMMU_MAX_UNITS);

done:
	kfree(link);
	return result;
}

/**
 * adreno_mmu_set_pt() - Change the pagetable of the current RB
 * @device: Pointer to device to which the rb belongs
 * @rb: The RB pointer on which pagetable is to be changed
 * @new_pt: The new pt the device will change to
 * @adreno_ctx: The context whose pagetable the ringbuffer should switch to,
 * NULL means default
 *
 * Returns 0 on success else error code.
 */
int adreno_iommu_set_pt(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt)
{
	struct kgsl_device *device = rb->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pagetable *cur_pt = device->mmu.defaultpagetable;

	if (rb->drawctxt_active)
		cur_pt = rb->drawctxt_active->base.proc_priv->pagetable;

	if (new_pt == cur_pt)
		return 0;

	/* For current rb check if CPU path can be used */
	if ((rb == adreno_dev->cur_rb) &&
		adreno_use_cpu_path(adreno_dev)) {
		return kgsl_mmu_set_pt(&device->mmu, new_pt);
	}
	return _set_pagetable_gpu(rb, new_pt);
}
